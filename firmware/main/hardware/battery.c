#include "battery.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "batt";

/* ------------------------------------------------------------------
 * 电池电量采样（ADC1，仅可输入引脚）
 *
 * 接线（与 PINMAP.md 保持一致）：
 *   电池+  ──┬── R1(100K) ──┬── GPIO1 (ADC1_CH0)
 *           │               │
 *           └── 电池- ── R2(100K) ──┘  （分压电阻把 3.0~4.2V 折半到 1.5~2.1V）
 *
 * 说明：
 *   - ESP32-S3 ADC1 通道为 GPIO1~GPIO10（ADC1_CH0=GPIO1）；量程随衰减档而定，
 *     ADC_ATTEN_DB_12 下约为 0~3.1V，所以必须用分压电阻把电池电压折半再采样。
 *   - 若你的分压比不同，改 BATTERY_DIVIDER；电量 0%/100% 对应电压改 MIN/MAX。
 * ------------------------------------------------------------------ */
#define PIN_BATT        ADC_CHANNEL_0 /* GPIO1 */

#define BATTERY_DIVIDER 2.0f   /* 电池电压 = 采样电压 × 分压比 */
#define BATT_MIN_V      3.0f   /* 0% 对应电压 */
#define BATT_MAX_V      4.2f   /* 100% 对应电压（满电） */
#define SAMPLE_CNT      16     /* 每次采样取平均的 ADC 读数个数 */

static adc_oneshot_unit_handle_t s_adc = NULL;
static int32_t s_filtered = -1; /* -1 表示尚未初始化平滑值 */

void battery_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, PIN_BATT, &chan_cfg));
    ESP_LOGI(TAG, "init ok (ADC1_CH%d)", PIN_BATT);
}

/* 读取平滑滤波后的电池电压（V）。未接电池时 ADC 读数为 0，电压约 0V */
static float battery_voltage(void)
{
    uint32_t sum = 0;
    int raw;
    for (int i = 0; i < SAMPLE_CNT; i++) {
        if (adc_oneshot_read(s_adc, PIN_BATT, &raw) == ESP_OK) {
            sum += (uint32_t)raw;
        }
    }
    int32_t avg = (int32_t)(sum / SAMPLE_CNT);

    /* 指数平滑，避免状态栏电量每秒抖动 */
    if (s_filtered < 0) {
        s_filtered = avg;
    } else {
        s_filtered = (s_filtered * 7 + avg) / 8;
    }

    /* ADC(12bit, 0~3.1V@DB12) → 采样电压(mV) → 电池电压(V) */
    float mv = (float)s_filtered / 4095.0f * 3100.0f;
    return mv / 1000.0f * BATTERY_DIVIDER;
}

bool battery_present(void)
{
    /* 真实电池最低工作电压约 3.0V；明显低于该值视为未接电池（纯 USB 供电） */
    return battery_voltage() >= 2.5f;
}

uint8_t battery_get_percent(void)
{
    float batt = battery_voltage();

    float pct = (batt - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V) * 100.0f;
    if (pct < 0.0f) {
        pct = 0.0f;
    } else if (pct > 100.0f) {
        pct = 100.0f;
    }
    return (uint8_t)pct;
}
