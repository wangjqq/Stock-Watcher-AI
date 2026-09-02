#include "http_server.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "data_fetcher.h"
#include "field_parser.h"
#include "ota.h"
#include "version.h"
#include "web_assets.h"
#include "wifi_manager.h"

static const char *TAG = "http";

/* 接口测试原始数据缓存（栈上数组，保持较小） */
#define BODY_MAX 4096
/* 配置 JSON body 上限（保存 / 导入导出完整配置可能较大，read_body 用 malloc 读入） */
#define CFG_JSON_BODY_MAX 16384
#define RAW_SHOW_MAX 500

/* 最近一次接口测试的解析结果（供字段选择页使用） */
static field_list_t s_last_fields;
static char s_last_raw[RAW_SHOW_MAX + 1];

/* ---------------- 工具 ---------------- */

static char *read_body(httpd_req_t *req)
{
    int len = req->content_len;
    if (len <= 0 || len > CFG_JSON_BODY_MAX) {
        return NULL;
    }
    char *buf = malloc(len + 1);
    if (!buf) {
        return NULL;
    }
    int r = httpd_req_recv(req, buf, len);
    if (r <= 0) {
        free(buf);
        return NULL;
    }
    buf[r] = '\0';
    return buf;
}

/* 在内存中查找内嵌资源（按请求 URI 精确匹配） */
static const web_asset_t *find_asset(const char *uri)
{
    for (uint32_t i = 0; i < web_assets_count; i++) {
        if (strcmp(web_assets[i].path, uri) == 0) {
            return &web_assets[i];
        }
    }
    return NULL;
}

/* 发送内嵌 web 资源；gzip 资源附带 Content-Encoding 头 */
static esp_err_t serve_static(httpd_req_t *req, const char *uri)
{
    const web_asset_t *a = find_asset(uri);
    if (!a) {
        /* 无扩展名的路径视为前端路由，回退根页面；有扩展名则真正 404 */
        if (strchr(uri + 1, '.')) {
            return httpd_resp_send_404(req);
        }
        a = find_asset("/");
        if (!a) {
            return httpd_resp_send_404(req);
        }
    }

    httpd_resp_set_type(req, a->mime);
    if (a->gzip) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    httpd_resp_send_chunk(req, (const char *)a->data, a->size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* ---------------- 静态资源 ---------------- */

static esp_err_t handle_root(httpd_req_t *req)
{
    return serve_static(req, "/");
}

static esp_err_t handle_404(httpd_req_t *req)
{
    return serve_static(req, req->uri);
}

/* ---------------- REST API ---------------- */

static esp_err_t api_get_config(httpd_req_t *req)
{
    /* cfg 约 60KB，httpd 任务栈只有几 KB，必须堆分配 */
    app_config_t *cfg = malloc(sizeof(app_config_t));
    if (!cfg) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    config_load(cfg);
    char *json = config_to_json(cfg);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json ? json : "{}");
    free(json);
    free(cfg);
    return ESP_OK;
}

static esp_err_t api_post_config(httpd_req_t *req)
{
    char *body = read_body(req);
    if (!body) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    /* cfg 约 60KB，httpd 任务栈只有几 KB，必须堆分配 */
    app_config_t *cfg = malloc(sizeof(app_config_t));
    if (!cfg) {
        free(body);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    config_load(cfg);
    esp_err_t err = config_from_json(body, cfg);
    free(body);
    if (err != ESP_OK) {
        free(cfg);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    }
    err = config_save(cfg);
    if (err != ESP_OK) {
        free(cfg);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    wifi_manager_apply_config(cfg); /* WiFi 配置变化时自动重连，无需重启 */
    free(cfg);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    ESP_LOGI(TAG, "config saved");
    return ESP_OK;
}

/* POST /api/reset  一键清空配置并重启（重启后需在设备端「系统 → WiFi」页重连 Wi-Fi） */
static esp_err_t api_reset(httpd_req_t *req)
{
    if (config_reset() != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "reset failed");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    ESP_LOGI(TAG, "config reset, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(200)); /* 让响应先发出去 */
    esp_restart();
    return ESP_OK;
}

static void fields_to_json(cJSON *arr, const field_list_t *list)
{
    for (uint32_t i = 0; i < list->count; i++) {
        cJSON *f = cJSON_CreateObject();
        cJSON_AddStringToObject(f, "path", list->items[i].path);
        cJSON_AddStringToObject(f, "type", list->items[i].type);
        cJSON_AddStringToObject(f, "sample", list->items[i].sample);
        cJSON_AddItemToArray(arr, f);
    }
}

/* POST /api/interface/test
 * body: {"url": "...", "method": 0|1, "headers": ["Key: Value", ...], "post_body": "..."} */
static esp_err_t api_test_interface(httpd_req_t *req)
{
    char *body = read_body(req);
    if (!body) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    }
    cJSON *u = cJSON_GetObjectItem(root, "url");
    if (!cJSON_IsString(u) || strlen(u->valuestring) == 0) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no url");
    }

    /* 按 body 构造测试请求：支持 method / headers / post_body
     * it 约 1KB、raw 4KB，httpd 任务栈只有几 KB，须堆分配（否则栈溢出导致连接被重置） */
    interface_t *it = malloc(sizeof(interface_t));
    char *raw = malloc(BODY_MAX);
    if (!it || !raw) {
        free(raw);
        free(it);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    memset(it, 0, sizeof(*it));
    it->method = REQ_GET;
    strlcpy(it->url, u->valuestring, sizeof(it->url));
    cJSON *m = cJSON_GetObjectItem(root, "method");
    if (cJSON_IsNumber(m)) {
        it->method = (http_method_t)m->valueint;
    }
    cJSON *hs = cJSON_GetObjectItem(root, "headers");
    if (cJSON_IsArray(hs)) {
        int hi = 0;
        cJSON *h;
        cJSON_ArrayForEach(h, hs) {
            if (hi >= CONFIG_HEADER_MAX) {
                break;
            }
            if (cJSON_IsString(h) && h->valuestring[0] != '\0') {
                strlcpy(it->headers[hi], h->valuestring, CONFIG_HEADER_LEN);
                hi++;
            }
        }
    }
    cJSON *pb = cJSON_GetObjectItem(root, "post_body");
    if (cJSON_IsString(pb)) {
        strlcpy(it->post_body, pb->valuestring, sizeof(it->post_body));
    }

    esp_err_t err = data_fetch_iface(it, raw, BODY_MAX, 5000);
    free(it);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", err == ESP_OK);
    if (err == ESP_OK) {
        s_last_raw[0] = '\0';
        strlcpy(s_last_raw, raw, sizeof(s_last_raw));
        if (field_parse(raw, &s_last_fields) != ESP_OK) {
            s_last_fields.count = 0;
        }
        cJSON_AddNumberToObject(resp, "field_count", s_last_fields.count);
        cJSON *arr = cJSON_AddArrayToObject(resp, "fields");
        fields_to_json(arr, &s_last_fields);
    } else {
        cJSON_AddStringToObject(resp, "error", esp_err_to_name(err));
    }
    cJSON_Delete(root);
    free(raw);

    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out ? out : "{}");
    free(out);
    return ESP_OK;
}

/* GET /api/fields  返回最近一次测试解析出的字段 */
static esp_err_t api_get_fields(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "raw", s_last_raw);
    cJSON *arr = cJSON_AddArrayToObject(root, "fields");
    fields_to_json(arr, &s_last_fields);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out ? out : "{}");
    free(out);
    return ESP_OK;
}

static esp_err_t api_get_status(httpd_req_t *req)
{
    /* cfg 约 60KB，httpd 任务栈只有几 KB，必须堆分配 */
    app_config_t *cfg = malloc(sizeof(app_config_t));
    if (!cfg) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
    }
    config_load(cfg);
    char ip[32];
    wifi_manager_ip_str(ip, sizeof(ip));
    char ap_ip[32];
    wifi_manager_ap_ip_str(ap_ip, sizeof(ap_ip));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", cfg->device_name);
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_manager_is_connected());
    cJSON_AddStringToObject(root, "ip", ip);
    cJSON_AddNumberToObject(root, "rssi", wifi_manager_get_rssi());
    cJSON_AddStringToObject(root, "ap_ssid", wifi_manager_ap_ssid());
    cJSON_AddStringToObject(root, "ap_ip", ap_ip);
    cJSON_AddStringToObject(root, "firmware_version", FW_VERSION);
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out ? out : "{}");
    free(out);
    free(cfg);
    return ESP_OK;
}

/* ---------------- 服务入口 ---------------- */

esp_err_t http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    /* OTA 上传大固件需要更长的接收等待；handler 上限需容纳新增的 /api/ota */
    cfg.recv_wait_timeout = 30;
    cfg.max_uri_handlers = 16;
    /* 默认栈 4KB 太小：接口测试等 handler 会调用 JSON 解析/HTTP 客户端，栈上稍大就溢出，
     * 导致连接被重置，需扩大到 8KB */
    cfg.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return ESP_FAIL;
    }

    httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_root },
        { .uri = "/api/config", .method = HTTP_GET, .handler = api_get_config },
        { .uri = "/api/config", .method = HTTP_POST, .handler = api_post_config },
        { .uri = "/api/reset", .method = HTTP_POST, .handler = api_reset },
        { .uri = "/api/interface/test", .method = HTTP_POST, .handler = api_test_interface },
        { .uri = "/api/fields", .method = HTTP_GET, .handler = api_get_fields },
        { .uri = "/api/status", .method = HTTP_GET, .handler = api_get_status },
        { .uri = "/*", .method = HTTP_GET, .handler = handle_404 }, /* 静态资源兜底 */
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
    ota_register(server); /* POST /api/ota 固件升级 */

    ESP_LOGI(TAG, "http server started");
    return ESP_OK;
}
