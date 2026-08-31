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

#define FETCH_BUF_SIZE 4096
#define FETCH_TIMEOUT_MS 5000

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

    uint32_t last_fetch_ms = 0;
    char buf[FETCH_BUF_SIZE];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* 每次循环重新读取配置，网页保存后即时生效（无需重启） */
        config_load(&cfg);

        if (wifi_manager_is_connected()) {
            status_bar_start_sntp();
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (wifi_manager_is_connected() && strlen(cfg.api_url) > 0
                && now_ms - last_fetch_ms >= cfg.refresh_interval_ms) {
            last_fetch_ms = now_ms;

            if (data_fetch(cfg.api_url, buf, sizeof(buf), FETCH_TIMEOUT_MS) == ESP_OK
                    && strlen(buf) > 0) {
                layout_render(&cfg, buf);
            }
        }

        /* 每秒刷新一次状态栏（时间/信号/电量），连同画布一起刷屏 */
        status_bar_draw();
        display_update();
    }
}
