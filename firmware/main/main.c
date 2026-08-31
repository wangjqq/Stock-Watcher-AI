#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "data_fetcher.h"
#include "display.h"
#include "http_server.h"
#include "layout_renderer.h"
#include "status_bar.h"
#include "wifi_manager.h"

static const char *TAG = "main";

#define FETCH_BUF_SIZE    4096
#define FETCH_TIMEOUT_MS  5000

/* 每个接口的最近一次数据缓存（按下标与 cfg->interfaces[i] 对应） */
static char    s_bodies[CONFIG_INTERFACE_MAX][FETCH_BUF_SIZE];
static char    s_last_url[CONFIG_INTERFACE_MAX][CONFIG_API_URL_MAX];
static uint32_t s_last_ms[CONFIG_INTERFACE_MAX];
static bool     s_has_body[CONFIG_INTERFACE_MAX];

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    app_config_t cfg;
    config_load(&cfg);

    display_init();
    wifi_manager_init(&cfg);
    wifi_manager_start_mdns("stockwatcher");
    http_server_start();

    uint32_t last_status_ms = 0;
    const char *bodies[CONFIG_INTERFACE_MAX];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        /* 每次循环重新读取配置，网页保存后即时生效（无需重启） */
        config_load(&cfg);

        if (wifi_manager_is_connected()) {
            status_bar_start_sntp();
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        /* 状态栏每秒刷新一次并整屏刷 LCD */
        if (now_ms - last_status_ms >= 1000) {
            last_status_ms = now_ms;
            status_bar_draw();
            display_update();
        }

        if (!wifi_manager_is_connected()) {
            continue;
        }

        /* 各接口按自己的刷新时间独立拉取 */
        bool any_fetched = false;
        for (uint32_t i = 0; i < cfg.interface_count; i++) {
            const interface_t *it = &cfg.interfaces[i];
            if (strlen(it->url) == 0) {
                continue;
            }
            /* URL 变化时立即重新拉取 */
            if (strcmp(s_last_url[i], it->url) != 0) {
                strlcpy(s_last_url[i], it->url, sizeof(s_last_url[i]));
                s_last_ms[i] = 0;
                s_has_body[i] = false;
            }
            if (now_ms - s_last_ms[i] < it->refresh_interval_ms) {
                continue;
            }
            s_last_ms[i] = now_ms;
            if (data_fetch(it->url, s_bodies[i], sizeof(s_bodies[i]), FETCH_TIMEOUT_MS) == ESP_OK
                    && strlen(s_bodies[i]) > 0) {
                s_has_body[i] = true;
                any_fetched = true;
            }
        }

        if (any_fetched) {
            for (uint32_t i = 0; i < CONFIG_INTERFACE_MAX; i++) {
                bodies[i] = s_bodies[i];
            }
            layout_render(&cfg, bodies, s_has_body);
        }
    }
}
