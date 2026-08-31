#include "battery.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "batt";

/* ------------------------------------------------------------------
 * 电池电量采样（ADC1，仅可输入引脚）
 *
 * 接线（与 PINMAP.md 保持一致）：
 *   电池+  ──┬── R1(100K) ──┬── GPIO36 (SENSOR_VP / ADC1_CH0)
 *           │               │
 *           └── 电池- ── R2(100K) ──┘  （分压电阻把 3.0~4.2V 折半到 1.5~2.1V）
 *
 * 说明：
 *   - ESP32 ADC 量程 0~3.3V，所以必须用分压电阻把电池电压折半再采样。
 *   - 若你的分压比不同，改 BATTERY_DIVIDER；电量 0%/100% 对应电压改 MIN/MAX。
 * ------------------------------------------------------------------ */
#define PIN_BATT        ADC1_CHANNEL_0 /* GPIO36 */

#define BATTERY_DIVIDER 2.0f   /* 电池电压 = 采样电压 × 分压比 */
#define BATT_MIN_V      3.0f   /* 0% 对应电压 */
#define BATT_MAX_V      4.2f   /* 100% 对应电压（满电） */
#define SAMPLE_CNT      16     /* 每次采样取平均的 ADC 读数个数 */

static int32_t s_filtered = -1; /* -1 表示尚未初始化平滑值 */

void battery_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PIN_BATT, ADC_ATTEN_DB_11);
    ESP_LOGI(TAG, "init ok (ADC1_CH%d)", PIN_BATT);
}

uint8_t battery_get_percent(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLE_CNT; i++) {
        sum += adc1_get_raw(PIN_BATT);
    }
    int32_t raw = (int32_t)(sum / SAMPLE_CNT);

    /* 指数平滑，避免状态栏电量每秒抖动 */
    if (s_filtered < 0) {
        s_filtered = raw;
    } else {
        s_filtered = (s_filtered * 7 + raw) / 8;
    }

    /* ADC(12bit, 0~3.3V) → 采样电压(mV) → 电池电压(mV) → 电量百分比 */
    float mv = (float)s_filtered / 4095.0f * 3300.0f;
    float batt = mv / 1000.0f * BATTERY_DIVIDER;

    float pct = (batt - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V) * 100.0f;
    if (pct < 0.0f) {
        pct = 0.0f;
    } else if (pct > 100.0f) {
        pct = 100.0f;
    }
    return (uint8_t)pct;
}
