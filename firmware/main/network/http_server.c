#include "http_server.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "data_fetcher.h"
#include "field_parser.h"
#include "web_assets.h"
#include "wifi_manager.h"

static const char *TAG = "http";

#define BODY_MAX 4096
#define RAW_SHOW_MAX 500

/* 最近一次接口测试的解析结果（供字段选择页使用） */
static field_list_t s_last_fields;
static char s_last_raw[RAW_SHOW_MAX + 1];

/* ---------------- 工具 ---------------- */

static char *read_body(httpd_req_t *req)
{
    int len = req->content_len;
    if (len <= 0 || len > BODY_MAX) {
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
    app_config_t cfg;
    config_load(&cfg);
    char *json = config_to_json(&cfg);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json ? json : "{}");
    free(json);
    return ESP_OK;
}

static esp_err_t api_post_config(httpd_req_t *req)
{
    char *body = read_body(req);
    if (!body) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    app_config_t cfg;
    config_load(&cfg);
    esp_err_t err = config_from_json(body, &cfg);
    free(body);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    }
    err = config_save(&cfg);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    ESP_LOGI(TAG, "config saved");
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

/* POST /api/interface/test  body: {"url": "..."} */
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

    char raw[BODY_MAX];
    esp_err_t err = data_fetch(u->valuestring, raw, sizeof(raw), 5000);

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
    app_config_t cfg;
    config_load(&cfg);
    char ip[32];
    wifi_manager_ip_str(ip, sizeof(ip));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", cfg.device_name);
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_manager_is_connected());
    cJSON_AddStringToObject(root, "ip", ip);
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out ? out : "{}");
    free(out);
    return ESP_OK;
}

/* ---------------- 服务入口 ---------------- */

esp_err_t http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return ESP_FAIL;
    }

    httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_root },
        { .uri = "/api/config", .method = HTTP_GET, .handler = api_get_config },
        { .uri = "/api/config", .method = HTTP_POST, .handler = api_post_config },
        { .uri = "/api/interface/test", .method = HTTP_POST, .handler = api_test_interface },
        { .uri = "/api/fields", .method = HTTP_GET, .handler = api_get_fields },
        { .uri = "/api/status", .method = HTTP_GET, .handler = api_get_status },
        { .uri = "/*", .method = HTTP_GET, .handler = handle_404 }, /* 静态资源兜底 */
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "http server started");
    return ESP_OK;
}
