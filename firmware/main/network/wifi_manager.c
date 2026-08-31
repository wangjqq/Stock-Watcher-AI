#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

static const char *TAG = "wifi";

static bool s_connected = false;
static char s_ip[32] = "";

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start, connecting...");
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            ESP_LOGW(TAG, "disconnected, retrying...");
            esp_wifi_connect(); /* 简单自动重连 */
        } else if (id == WIFI_EVENT_AP_STACONNECTED) {
            ESP_LOGI(TAG, "station joined AP");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&evt->ip_info.ip));
        s_connected = true;
        ESP_LOGI(TAG, "got ip: %s", s_ip);
    }
}

esp_err_t wifi_manager_init(const app_config_t *cfg)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    if (strlen(cfg->ssid) > 0) {
        /* STA 模式：连接用户配置的网络 */
        wifi_config_t sta = { 0 };
        strlcpy((char *)sta.sta.ssid, cfg->ssid, sizeof(sta.sta.ssid));
        strlcpy((char *)sta.sta.password, cfg->password, sizeof(sta.sta.password));
        sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    } else {
        /* AP 配网模式：热点名 StockWatcher-xxxx */
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        char ssid[32];
        snprintf(ssid, sizeof(ssid), "StockWatcher-%02X%02X", mac[4], mac[5]);
        wifi_config_t ap = { 0 };
        strlcpy((char *)ap.ap.ssid, ssid, sizeof(ap.ap.ssid));
        strlcpy((char *)ap.ap.password, "12345678", sizeof(ap.ap.password));
        ap.ap.max_connection = 4;
        ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        ESP_LOGI(TAG, "AP mode, ssid=%s", ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
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

void wifi_manager_ip_str(char *out, size_t out_size)
{
    strlcpy(out, s_ip, out_size);
}
