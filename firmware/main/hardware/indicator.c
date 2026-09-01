#include "indicator.h"

#include "esp_timer.h"
#include "led.h"

#define ALERT_MS 3000 /* 告警橙持续时间 */
#define FLASH_MS 150  /* 刷新蓝闪光时长 */
#define BLINK_MS 500  /* 断网闪烁周期 */

static uint64_t s_alert_until = 0;
static uint64_t s_flash_until = 0;
static int s_trend = 0; /* 1 涨 / -1 跌 / 0 平 */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void indicator_init(void)
{
    led_off();
}

void indicator_set_alert(bool on)
{
    s_alert_until = on ? (uint64_t)now_ms() + ALERT_MS : 0;
}

void indicator_on_refresh(void)
{
    s_flash_until = (uint64_t)now_ms() + FLASH_MS;
}

void indicator_set_trend(int trend)
{
    s_trend = trend;
}

void indicator_update(bool wifi_ok)
{
    uint32_t now = now_ms();

    if (now < s_alert_until) {
        /* 告警：橙色闪烁（300ms 周期） */
        if ((now / 300) & 1) {
            led_set_color(255, 140, 0);
        } else {
            led_off();
        }
    } else if (now < s_flash_until) {
        led_set_color(0, 140, 255); /* 刷新蓝 */
    } else if (s_trend > 0) {
        led_set_color(255, 40, 40); /* 涨红 */
    } else if (s_trend < 0) {
        led_set_color(40, 230, 80); /* 跌绿 */
    } else if (wifi_ok) {
        led_set_color(0, 200, 80); /* 联网绿 */
    } else {
        /* 断网：红 / 灭 交替闪烁 */
        if ((now / BLINK_MS) & 1) {
            led_set_color(255, 40, 40);
        } else {
            led_off();
        }
    }
}
