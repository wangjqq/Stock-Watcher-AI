#include <stdbool.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "app_ui.h"
#include "buzzer.h"
#include "data_fetcher.h"
#include "display.h"
#include "http_server.h"
#include "indicator.h"
#include "knob.h"
#include "layout_renderer.h"
#include "selftest.h"
#include "status_bar.h"
#include "version.h"
#include "wifi_manager.h"

#define FETCH_BUF_SIZE    4096
#define FETCH_TIMEOUT_MS  5000

/* 每个接口的最近一次数据缓存（按下标与 cfg->interfaces[i] 对应） */
static char    s_bodies[CONFIG_INTERFACE_MAX][FETCH_BUF_SIZE];
static char    s_last_url[CONFIG_INTERFACE_MAX][CONFIG_API_URL_MAX];
static uint32_t s_last_ms[CONFIG_INTERFACE_MAX];
static bool     s_has_body[CONFIG_INTERFACE_MAX];

/* 当前打开的用户应用（开机默认第一个应用） */
static uint32_t s_current_app = 0;
/* 当前配置中的用户应用数量（主循环每轮从 cfg 同步） */
static uint32_t s_app_count = 1;
/* 有输入或状态变化后置位，主循环据此立即重绘，避免等下一次拉取 */
static bool s_force_redraw = false;

/* ---------------- 界面状态机：应用列表 / 用户应用 / 系统应用 ---------------- */
typedef enum {
    UI_MENU = 0,   /* 应用列表菜单 */
    UI_APP,        /* 用户应用内 */
    UI_SYSTEM,     /* 内置「系统」应用内 */
} ui_state_t;

static ui_state_t  s_ui = UI_MENU;
static uint32_t    s_cursor = 0;     /* 应用列表光标：0..s_menu_count-1，末尾为「系统」 */
static uint32_t    s_menu_count = 1; /* = 用户应用数 + 1（含「系统」） */
static int         s_sys = SYS_VIEW_MENU;   /* 系统应用内部视图 */
static uint32_t    s_sys_cursor = 0;        /* 系统菜单光标 0..SYS_ITEM_COUNT-1 */
static uint8_t     s_brightness = 80;       /* 当前亮度（会话值，主循环回写 NVS） */
static bool        s_brightness_dirty = false;
static bool        s_manual_refresh = false;        /* OK「手动刷新」置位，主循环立即拉取 */
static uint32_t    s_refresh_feedback_ms = 0;       /* 手动刷新提示的截止时间 */

/* 按键音 + 请求重绘 */
static void key_feedback(void)
{
    buzzer_play_event(SND_KEY);
    s_force_redraw = true;
}

static void apply_brightness(void)
{
    display_set_brightness(s_brightness);
    s_brightness_dirty = true;
}

/* --- 应用列表菜单：旋转移动光标，OK 打开，BACK 无操作 --- */
static void on_menu_input(knob_event_t ev)
{
    switch (ev) {
    case KNOB_EV_LEFT:
        s_cursor = (s_cursor + s_menu_count - 1) % s_menu_count;
        key_feedback();
        break;
    case KNOB_EV_RIGHT:
        s_cursor = (s_cursor + 1) % s_menu_count;
        key_feedback();
        break;
    case KNOB_EV_OK:
        if (s_cursor == s_menu_count - 1) {
            s_ui = UI_SYSTEM; /* 末尾固定为「系统」应用 */
            s_sys = SYS_VIEW_MENU;
            s_sys_cursor = 0;
        } else {
            s_current_app = s_cursor;
            s_ui = UI_APP;
        }
        key_feedback();
        break;
    case KNOB_EV_BACK:
    default:
        break; /* 列表页返回无操作 */
    }
}

/* --- 应用内：旋转交给应用自身功能（股票应用暂无旋钮能力），BACK 回列表 --- */
static void on_app_input(knob_event_t ev)
{
    switch (ev) {
    case KNOB_EV_BACK:
        s_ui = UI_MENU;
        s_cursor = s_current_app; /* 回到列表并停在刚打开的应用上 */
        key_feedback();
        break;
    default:
        break;
    }
}

/* --- 系统应用：亮度 / 手动刷新 / 状态 --- */
static void on_system_input(knob_event_t ev)
{
    switch (s_sys) {
    case SYS_VIEW_MENU:
        switch (ev) {
        case KNOB_EV_LEFT:
            s_sys_cursor = (s_sys_cursor + SYS_ITEM_COUNT - 1) % SYS_ITEM_COUNT;
            key_feedback();
            break;
        case KNOB_EV_RIGHT:
            s_sys_cursor = (s_sys_cursor + 1) % SYS_ITEM_COUNT;
            key_feedback();
            break;
        case KNOB_EV_OK:
            if (s_sys_cursor == 0) {
                s_sys = SYS_VIEW_BRIGHT;
            } else if (s_sys_cursor == 1) {
                s_manual_refresh = true; /* 主循环立即重新拉取全部接口 */
            } else {
                s_sys = SYS_VIEW_STATUS;
            }
            key_feedback();
            break;
        case KNOB_EV_BACK:
            s_ui = UI_MENU;
            s_cursor = s_menu_count - 1; /* 回列表并停在「系统」 */
            key_feedback();
            break;
        default:
            break;
        }
        break;

    case SYS_VIEW_BRIGHT:
        switch (ev) {
        case KNOB_EV_LEFT:
            s_brightness = (s_brightness >= 5) ? (uint8_t)(s_brightness - 5) : 0;
            apply_brightness();
            key_feedback();
            break;
        case KNOB_EV_RIGHT:
            s_brightness = (s_brightness > 95) ? 100 : (uint8_t)(s_brightness + 5);
            apply_brightness();
            key_feedback();
            break;
        case KNOB_EV_OK:
        case KNOB_EV_BACK:
            s_sys = SYS_VIEW_MENU;
            key_feedback();
            break;
        default:
            break;
        }
        break;

    case SYS_VIEW_STATUS:
        switch (ev) {
        case KNOB_EV_OK:
        case KNOB_EV_BACK:
            s_sys = SYS_VIEW_MENU;
            key_feedback();
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}

/* 统一输入入口：按当前界面状态分发 */
static void on_input(knob_event_t ev)
{
    switch (s_ui) {
    case UI_MENU:   on_menu_input(ev);   break;
    case UI_APP:    on_app_input(ev);    break;
    case UI_SYSTEM: on_system_input(ev); break;
    default:                            break;
    }
}

/* 按当前界面状态绘制画布区（状态栏不动，由 status_bar 单独负责） */
static void render_canvas(const app_config_t *cfg)
{
    if (s_ui == UI_MENU) {
        static char names[CONFIG_APP_MAX + 1][CONFIG_NAME_MAX];
        uint32_t n = cfg->app_count > 0 ? cfg->app_count : 1;
        for (uint32_t i = 0; i < n; i++) {
            strlcpy(names[i], cfg->apps[i].name, CONFIG_NAME_MAX);
        }
        strlcpy(names[n], "System", CONFIG_NAME_MAX); /* 末尾固定系统应用 */
        app_ui_draw_menu(names, (int)n + 1, (int)s_cursor);
        return;
    }

    if (s_ui == UI_APP) {
        const char *bodies[CONFIG_INTERFACE_MAX];
        for (uint32_t i = 0; i < CONFIG_INTERFACE_MAX; i++) {
            bodies[i] = s_bodies[i];
        }
        int trend = 0;
        layout_render(cfg, s_current_app, bodies, s_has_body, &trend);
        indicator_set_trend(trend); /* 状态灯随涨跌字段变色 */
        return;
    }

    if (s_ui == UI_SYSTEM) {
        char ip[32];
        wifi_manager_ip_str(ip, sizeof(ip));
        bool refreshing = (uint32_t)(esp_timer_get_time() / 1000) < s_refresh_feedback_ms;
        app_ui_draw_system(s_sys, (int)s_sys_cursor, s_brightness,
                           wifi_manager_is_connected(), wifi_manager_get_rssi(),
                           ip, FW_VERSION, refreshing);
        return;
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

    /* 界面会话初始状态：亮度取配置，开机默认进入第一个应用 */
    s_brightness = cfg.brightness;
    s_ui = UI_APP;
    s_current_app = 0;
    s_cursor = 0;
    s_force_redraw = true; /* 首次循环立即绘制画布 */

    display_init(cfg.brightness);
    knob_init();
    buzzer_init();
    led_init();
    indicator_init();
    wifi_manager_init(&cfg);
    wifi_manager_start_mdns("stockwatcher");
    http_server_start();

    /* 上电硬件自检：LED 变色 + 蜂鸣（可经 selftest.h 关闭） */
    hw_selftest_run();

    /* 输入统一走这里：按当前界面状态（菜单 / 应用 / 系统）分发 */
    knob_set_handler(on_input);

    uint32_t last_status_ms = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        /* 每次循环重新读取配置，网页保存后即时生效（无需重启） */
        config_load(&cfg);

        /* 应用数量随配置同步；配置变更后越界时回退 */
        s_app_count = cfg.app_count > 0 ? cfg.app_count : 1;
        s_menu_count = s_app_count + 1; /* 末尾固定为「系统」应用 */
        if (s_current_app >= s_app_count) {
            s_current_app = 0;
        }
        if (s_cursor >= s_menu_count) {
            s_cursor = 0;
        }

        /* 旋钮调节的亮度回写配置（去抖：主循环每 100ms 检查一次） */
        if (s_brightness_dirty) {
            s_brightness_dirty = false;
            cfg.brightness = s_brightness;
            config_save(&cfg);
        }

        /* 蜂鸣器开关/音量随配置即时生效 */
        buzzer_set_enabled(cfg.buzzer_enabled);
        buzzer_set_volume(cfg.buzzer_volume);

        if (wifi_manager_is_connected()) {
            status_bar_start_sntp();
        }

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        /* 「系统」应用手动刷新：下一轮立即重新拉取全部接口 */
        if (s_manual_refresh) {
            s_manual_refresh = false;
            for (uint32_t i = 0; i < CONFIG_INTERFACE_MAX; i++) {
                s_last_ms[i] = 0;
            }
            s_refresh_feedback_ms = now_ms + 1500;
            s_force_redraw = true;
        }
        /* 手动刷新提示超时后自动清除并重绘 */
        if (s_refresh_feedback_ms != 0 && now_ms >= s_refresh_feedback_ms) {
            s_refresh_feedback_ms = 0;
            s_force_redraw = true;
        }

        /* 状态灯：按网络/涨跌/刷新/告警输出颜色 */
        indicator_update(wifi_manager_is_connected());

        /* 状态栏每秒刷新一次并整屏刷 LCD */
        if (now_ms - last_status_ms >= 1000) {
            last_status_ms = now_ms;
            status_bar_draw();
            display_update();
        }

        /* 各接口按自己的刷新时间独立拉取（仅在联网时） */
        bool any_fetched = false;
        if (wifi_manager_is_connected()) {
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
        }

        /* 界面渲染：应用内数据更新即重绘；菜单/系统页仅在有输入时重绘。
         * 渲染不依赖网络，离线也能操作菜单与系统应用。 */
        bool need_render = s_force_redraw;
        if (s_ui == UI_APP) {
            need_render = need_render || any_fetched;
        }
        if (need_render) {
            s_force_redraw = false;
            render_canvas(&cfg);
        }
    }
}
