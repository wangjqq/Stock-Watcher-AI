#include "selftest.h"

#include "buzzer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"

static const char *TAG = "selftest";

void hw_selftest_run(void)
{
#if HW_SELFTEST_ENABLED
    ESP_LOGW(TAG, "=== hardware selftest start ===");

    /* LED：红 → 绿 → 蓝 → 黄 */
    led_set_color(255, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_set_color(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_set_color(0, 0, 255);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_set_color(255, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    led_off();

    /* 蜂鸣器：上行音阶提示 */
    static const buzzer_note_t seq[] = {
        { 880, 120 }, { 660, 120 }, { 880, 120 }, { 440, 240 },
    };
    buzzer_play(seq, 4);
    ESP_LOGW(TAG, "selftest: LED/buzzer done");
#endif
}
