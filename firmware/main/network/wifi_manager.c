#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

static const char *TAG = "wifi";

static bool     s_connected = false;      /* 已获取 IP（接入网络） */
static bool     s_sta_configured = false; /* 已配置 ssid（会尝试连接）；空 ssid 时待机 */
static char     s_ip[32] = "";
static int      s_last_reason = 0;        /* 最近一次断开原因（0=无） */
static char     s_applied_ssid[CONFIG_SSID_MAX] = "";
static char     s_applied_pass[CONFIG_PASS_MAX] = "";
static bool     s_scan_done = false;      /* 最近一次扫描是否已完成 */

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start, connecting...");
            if (s_sta_configured) {
                esp_wifi_connect();
            }
        } else if (id == WIFI_EVENT_SCAN_DONE) {
            s_scan_done = true; /* 扫描结束（结果用 scan_get_ap_records 取） */
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t *)data;
            s_connected = false;
            if (s_sta_configured) {
                s_last_reason = evt ? (int)evt->reason : 0;
                ESP_LOGW(TAG, "disconnected (reason=%d), retrying...", s_last_reason);
                esp_wifi_connect(); /* 简单自动重连 */
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&evt->ip_info.ip));
        s_connected = true;
        s_last_reason = 0;
        ESP_LOGI(TAG, "got ip: %s", s_ip);
    }
}

/* 纯 STA：配置并启动 WiFi。ssid 为空则待机（不连接），由设备端 WiFi 页配置 */
static esp_err_t sta_setup(const char *ssid, const char *password)
{
    s_connected = false;
    s_last_reason = 0;

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        return err;
    }

    wifi_config_t sta = { 0 };
    if (ssid && ssid[0]) {
        strlcpy((char *)sta.sta.ssid, ssid, sizeof(sta.sta.ssid));
        if (password) {
            strlcpy((char *)sta.sta.password, password, sizeof(sta.sta.password));
        }
        sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    s_sta_configured = (sta.sta.ssid[0] != '\0');

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    err = esp_wifi_start();
    if (err == ESP_OK && s_sta_configured) {
        ESP_LOGI(TAG, "sta connect to %s", ssid);
    }
    return err;
}

esp_err_t wifi_manager_init(const app_config_t *cfg)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    esp_err_t err = sta_setup(cfg->ssid, cfg->password);
    strlcpy(s_applied_ssid, cfg->ssid, sizeof(s_applied_ssid));
    strlcpy(s_applied_pass, cfg->password, sizeof(s_applied_pass));
    return err;
}

/* 网页保存配置后调用：ssid 或 password 变化时重新配置并重连，无需重启 */
esp_err_t wifi_manager_apply_config(const app_config_t *cfg)
{
    if (strcmp(s_applied_ssid, cfg->ssid) == 0 &&
            strcmp(s_applied_pass, cfg->password) == 0) {
        return ESP_OK; /* WiFi 配置没变，保持当前连接 */
    }
    esp_err_t err = sta_setup(cfg->ssid, cfg->password);
    strlcpy(s_applied_ssid, cfg->ssid, sizeof(s_applied_ssid));
    strlcpy(s_applied_pass, cfg->password, sizeof(s_applied_pass));
    return err;
}

/* 设备端 WiFi 页手动连接：只切换连接，不改配置（配置由 main.c 负责写 NVS） */
esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    esp_err_t err = sta_setup(ssid, password);
    if (err == ESP_OK) {
        strlcpy(s_applied_ssid, ssid, sizeof(s_applied_ssid));
        strlcpy(s_applied_pass, password, sizeof(s_applied_pass));
    }
    return err;
}

void wifi_manager_disconnect(void)
{
    s_connected = false;
    s_sta_configured = false; /* 断开后不再自动重连 */
    esp_wifi_disconnect();
}

esp_err_t wifi_manager_scan_start(void)
{
    s_scan_done = false;
    return esp_wifi_scan_start(NULL, false); /* 异步，SCAN_DONE 事件收尾 */
}

bool wifi_manager_scan_done(void)
{
    return s_scan_done;
}

int wifi_manager_scan_get_results(wifi_ap_record_t *aps, int max)
{
    uint16_t n = (uint16_t)max;
    esp_err_t e = esp_wifi_scan_get_ap_records(&n, aps);
    esp_wifi_scan_stop(); /* 释放内核扫描结果内存 */
    if (e != ESP_OK) {
        return 0;
    }
    return (int)n;
}

void wifi_manager_scan_stop(void)
{
    esp_wifi_scan_stop(); /* 未在扫描时返回错误，忽略 */
}

esp_err_t wifi_manager_start_mdns(const char *hostname)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns init failed: %s", esp_err_to_name(err));
        return err;
    }
    mdns_hostname_set(hostname);
    mdns_instance_name_set("StockWatcher Device");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://%s.local", hostname);
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

int wifi_manager_get_rssi(void)
{
    if (!s_connected) {
        return 0;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

int wifi_manager_last_disconnect_reason(void)
{
    return s_last_reason;
}

const char *wifi_manager_connected_ssid(void)
{
    if (!s_connected) {
        return "";
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return (const char *)ap.ssid;
    }
    return "";
}

void wifi_manager_ip_str(char *out, size_t out_size)
{
    strlcpy(out, s_ip, out_size);
}
