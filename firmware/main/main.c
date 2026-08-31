#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "buzzer.h"
#include "data_fetcher.h"
#include "display.h"
#include "http_server.h"
#include "indicator.h"
#include "knob.h"
#include "layout_renderer.h"
#include "selftest.h"
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

/* 当前显示的应用（开机默认第一个应用；旋钮旋转切换） */
static uint32_t s_current_app = 0;
/* 当前配置中的应用数量（主循环每轮从 cfg 同步，供旋钮切换应用使用） */
static uint32_t s_app_count = 1;
/* 应用切换后置位，主循环据此用缓存数据立即重绘，避免等下一次拉取 */
static bool s_force_redraw = false;

/* 输入事件：旋钮旋转切换应用，OK/BACK 目前做按键音 + 日志（后续分配动作） */
static void on_input(knob_event_t ev)
{
    switch (ev) {
    case KNOB_EV_LEFT:
        if (s_app_count > 0) {
            s_current_app = (s_current_app + s_app_count - 1) % s_app_count;
        }
        ESP_LOGI(TAG, "knob LEFT -> app %lu", (unsigned long)s_current_app);
        buzzer_play_event(SND_KEY);
        s_force_redraw = true;
        break;
    case KNOB_EV_RIGHT:
        if (s_app_count > 0) {
            s_current_app = (s_current_app + 1) % s_app_count;
        }
        ESP_LOGI(TAG, "knob RIGHT -> app %lu", (unsigned long)s_current_app);
        buzzer_play_event(SND_KEY);
        s_force_redraw = true;
        break;
    case KNOB_EV_OK:
        ESP_LOGI(TAG, "knob OK (确认)");
        buzzer_play_event(SND_KEY);
        break;
    case KNOB_EV_BACK:
        ESP_LOGI(TAG, "knob BACK (返回)");
        buzzer_play_event(SND_KEY);
        break;
    default:
        break;
    }
}

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
    knob_init();
    buzzer_init();
    led_init();
    indicator_init();
    wifi_manager_init(&cfg);
    wifi_manager_start_mdns("stockwatcher");
    http_server_start();

    /* 上电硬件自检：LED 变色 + 蜂鸣（可经 selftest.h 关闭） */
    hw_selftest_run();

    /* 输入统一走这里：旋钮旋转切换应用，OK/BACK 先做按键音 + 日志 */
    knob_set_handler(on_input);

    uint32_t last_status_ms = 0;
    const char *bodies[CONFIG_INTERFACE_MAX];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        /* 每次循环重新读取配置，网页保存后即时生效（无需重启） */
        config_load(&cfg);

        /* 应用数量随配置同步；配置变更后越界时回退到第一个应用 */
        s_app_count = cfg.app_count > 0 ? cfg.app_count : 1;
        if (s_current_app >= s_app_count) {
            s_current_app = 0;
        }

        /* 蜂鸣器开关/音量随配置即时生效 */
        buzzer_set_enabled(cfg.buzzer_enabled);
        buzzer_set_volume(cfg.buzzer_volume);

        if (wifi_manager_is_connected()) {
            status_bar_start_sntp();
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        /* 状态灯：按网络/涨跌/刷新/告警输出颜色 */
        indicator_update(wifi_manager_is_connected());

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
                indicator_on_refresh(); /* 刷新闪光 */
            }
        }

        if (any_fetched || s_force_redraw) {
            s_force_redraw = false;
            for (uint32_t i = 0; i < CONFIG_INTERFACE_MAX; i++) {
                bodies[i] = s_bodies[i];
            }
            int trend = 0;
            layout_render(&cfg, s_current_app, bodies, s_has_body, &trend);
            indicator_set_trend(trend); /* 随涨跌字段变色 */
        }
    }
}
