#include "buzzer.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "buzzer";

/* 接线（与 PINMAP.md 保持一致）：无源蜂鸣器信号脚 */
#define PIN_BUZZER GPIO_NUM_13

#define MAX_NOTES 16   /* 单次最多音符数 */
#define BUZZER_DUTY 512 /* 50% 占空比（10bit，音量按比例缩放） */

static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task = NULL;
static buzzer_note_t s_notes[MAX_NOTES];
static int s_note_count = 0;

static bool s_enabled = true;
static uint8_t s_volume = 70; /* 0-100 */

/* 预设声音事件表 */
static const buzzer_note_t SND_KEY_SEQ[] = {
    { 1000, 40 },                                        /* 按键：短嘀 */
};
static const buzzer_note_t SND_RISE_SEQ[] = {
    { 880, 120 }, { 1320, 160 },                         /* 涨：低→高 */
};
static const buzzer_note_t SND_FALL_SEQ[] = {
    { 660, 150 }, { 440, 120 },                          /* 跌：高→低 */
};
static const buzzer_note_t SND_ALERT_SEQ[] = {
    { 880, 100 }, { 0, 60 }, { 880, 100 }, { 0, 60 }, { 880, 100 }, /* 告警三连响 */
};
static const buzzer_note_t SND_DISCONN_SEQ[] = {
    { 880, 90 }, { 0, 60 }, { 880, 90 }, { 0, 120 }, { 440, 220 },  /* 断网两短一长 */
};

static const buzzer_note_t *const s_events[SND_COUNT] = {
    [SND_KEY]     = SND_KEY_SEQ,
    [SND_RISE]    = SND_RISE_SEQ,
    [SND_FALL]    = SND_FALL_SEQ,
    [SND_ALERT]   = SND_ALERT_SEQ,
    [SND_DISCONN] = SND_DISCONN_SEQ,
};
static const uint8_t s_event_len[SND_COUNT] = {
    [SND_KEY]     = 1,
    [SND_RISE]    = 2,
    [SND_FALL]    = 2,
    [SND_ALERT]   = 5,
    [SND_DISCONN] = 5,
};

static void buzzer_tone(uint16_t freq_hz)
{
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        /* 音量 0-100 → 占空比 0..BUZZER_DUTY */
        uint32_t duty = (uint32_t)BUZZER_DUTY * s_volume / 100;
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

static void buzzer_task(void *arg)
{
    (void)arg;
    buzzer_note_t notes[MAX_NOTES];
    int n;

    for (;;) {
        /* 等待新任务；空任务（n==0）用于退出本段并关掉蜂鸣 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        for (;;) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            n = s_note_count;
            if (n > 0) {
                memcpy(notes, s_notes, sizeof(buzzer_note_t) * (size_t)n);
            }
            s_note_count = 0;
            xSemaphoreGive(s_lock);

            if (n == 0) {
                break;
            }
            for (int i = 0; i < n; i++) {
                buzzer_tone(notes[i].freq_hz);
                vTaskDelay(pdMS_TO_TICKS(notes[i].dur_ms));
            }
        }
        buzzer_tone(0);
    }
}

void buzzer_init(void)
{
    s_lock = xSemaphoreCreateMutex();

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch);

    xTaskCreate(buzzer_task, "buzzer", 2048, NULL, 6, &s_task);
    ESP_LOGI(TAG, "init ok");
}

int buzzer_play(const buzzer_note_t *notes, int count)
{
    if (notes == NULL || count <= 0 || count > MAX_NOTES) {
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_notes, notes, sizeof(buzzer_note_t) * (size_t)count);
    s_note_count = count;
    xSemaphoreGive(s_lock);

    xTaskNotifyGive(s_task);
    return 0;
}

void buzzer_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_note_count = 0;
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_task);
}

int buzzer_play_event(sound_event_t ev)
{
    if (!s_enabled || ev >= SND_COUNT) {
        return -1;
    }
    return buzzer_play(s_events[ev], s_event_len[ev]);
}

void buzzer_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (!enabled) {
        buzzer_stop();
    }
}

void buzzer_set_volume(uint8_t volume)
{
    s_volume = volume > 100 ? 100 : volume;
}
