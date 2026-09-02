#include "status_bar.h"

#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"

#include "display.h"
#include "battery.h"
#include "wifi_manager.h"

static const char *TAG = "status";

static bool s_sntp_started = false;
static int  s_left_w = 0; /* 状态栏最左侧预留宽度（应用内返回按钮用） */

void status_bar_set_left(int w)
{
    s_left_w = (w > 0) ? w : 0;
}

void status_bar_start_sntp(void)
{
    if (s_sntp_started) {
        return;
    }
    s_sntp_started = true;

    /* 中国时区 */
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    ESP_LOGI(TAG, "sntp started");
}

void status_bar_draw(void)
{
    char time_str[16];
    time_t now = 0;
    time(&now);

    struct tm tmv;
    if (now > 1700000000 && localtime_r(&now, &tmv)) {
        /* 时间已同步：显示 HH:MM */
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    } else {
        /* 未同步：显示运行时长 U hh:mm */
        uint32_t up_ms = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t h = up_ms / 3600000;
        uint32_t m = (up_ms / 60000) % 60;
        snprintf(time_str, sizeof(time_str), "U%02d:%02d", (int)h, (int)m);
    }

    display_draw_status_bar(time_str, wifi_manager_get_rssi(), battery_get_percent(), s_left_w);
}
