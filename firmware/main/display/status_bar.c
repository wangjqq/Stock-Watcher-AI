#include "status_bar.h"

#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"

#include "display.h"
#include "wifi_manager.h"

static const char *TAG = "status";

/* 设备没有电池采样硬件，先固定显示满电量；
 * 接入电池/ADC 后把 s_battery 换成真实采样值即可。 */
#define BATTERY_DEFAULT 100

static bool s_sntp_started = false;
static int s_battery = BATTERY_DEFAULT;

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
        snprintf(time_str, sizeof(time_str), "U%02d:%02d", h, m);
    }

    display_draw_status_bar(time_str, wifi_manager_get_rssi(), s_battery);
}
