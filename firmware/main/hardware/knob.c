#include "knob.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "knob";

/* ------------------------------------------------------------------
 * 接线（与 PINMAP.md 保持一致）：
 *   A / B    旋钮正交输出（内部上拉）
 *   SW       旋钮按下 = 确认（内部上拉，按下为低）
 *   BACK     返回键（内部上拉，按下为低）
 * EC11 与 E8H6 等旋钮原理相同（正交相位 + 内置按下开关），驱动通用。
 *
 * 注意：目标芯片 ESP32-S3 N16R8（Octal PSRAM）占用 GPIO26~37，
 *       旋钮 B/SW 避开该区间使用 GPIO6/7。
 * ------------------------------------------------------------------ */
#define KNOB_GPIO_A    GPIO_NUM_47   /* 原 GPIO25 在 S3 上不存在，已改至 GPIO47 */
#define KNOB_GPIO_B    GPIO_NUM_6
#define KNOB_GPIO_SW   GPIO_NUM_7
#define KNOB_GPIO_BACK GPIO_NUM_14

#define ACTIVE_LEVEL   0   /* 按下/闭合 = 低电平 */
#define DEBOUNCE_MS    30  /* 按键消抖窗口 */
#define SCAN_PERIOD_MS 10  /* 扫描周期 */

/* 累积多少个相位跳变触发一次旋转事件。
 * EC11/E8H6 一个定位齿 = 4 次相位变化；若一格要拧两下，可调成 2。 */
#define KNOB_STEP_THRESH 4
/* 实际旋转方向与预期相反时改为 1 */
#define KNOB_REVERSE     0

static knob_handler_t s_handler = NULL;

/* --- 旋钮正交解码 ------------------------------------------------ */
static int8_t s_enc_prev = 0; /* 上一次 (A<<1|B) 状态 */
static int8_t s_enc_acc  = 0; /* 累积方向 */

/* 状态转移方向表：索引 = (旧状态 << 2) | 新状态，输出 -1 / 0 / +1
 * 00 / 01 / 10 / 11 对应 (A,B) 的组合。 */
static const int8_t s_dir[16] = {
     0, -1, +1,  0,   /* 旧 00 */
    +1,  0,  0, -1,   /* 旧 01 */
    -1,  0,  0, +1,   /* 旧 10 */
     0, +1, -1,  0,   /* 旧 11 */
};

/* --- 两个按键（旋钮 SW + 返回键）的消抖状态 ---------------------- */
typedef struct {
    uint8_t stable;       /* 当前稳定电平（0 = 按下） */
    uint8_t pending;      /* 消抖中的新电平 */
    uint8_t debounce_cnt;
    uint8_t held;         /* 是否处于按住状态 */
} key_state_t;

static key_state_t s_sw;   /* 旋钮按下 */
static key_state_t s_back; /* 返回键 */

static void emit(knob_event_t ev)
{
    if (s_handler) {
        s_handler(ev);
    }
}

static void scan_encoder(void)
{
    int a = gpio_get_level(KNOB_GPIO_A);
    int b = gpio_get_level(KNOB_GPIO_B);
    int8_t state = (int8_t)((a << 1) | b);
    if (state == s_enc_prev) {
        return;
    }
    int8_t d = s_dir[(s_enc_prev << 2) | state];
    s_enc_prev = state;
    if (d == 0) {
        return; /* 非法跳变（抖动/跨齿），忽略 */
    }
    if (KNOB_REVERSE) {
        d = (int8_t)-d;
    }
    s_enc_acc += d;
    if (s_enc_acc >= KNOB_STEP_THRESH) {
        s_enc_acc = 0;
        emit(KNOB_EV_RIGHT);
    } else if (s_enc_acc <= -KNOB_STEP_THRESH) {
        s_enc_acc = 0;
        emit(KNOB_EV_LEFT);
    }
}

static void scan_key(key_state_t *st, gpio_num_t pin, knob_event_t ev)
{
    int level = gpio_get_level(pin);
    if (level != st->stable) {
        st->pending = (uint8_t)level;
        if (++st->debounce_cnt >= (DEBOUNCE_MS / SCAN_PERIOD_MS)) {
            st->stable = st->pending;
            st->debounce_cnt = 0;
            if (st->stable == ACTIVE_LEVEL) {
                st->held = 1;
            } else if (st->held) {
                st->held = 0;
                emit(ev); /* 松开时触发，避免按住抖动重复 */
            }
        }
    } else {
        st->debounce_cnt = 0;
    }
}

static void knob_task(void *arg)
{
    (void)arg;
    for (;;) {
        scan_encoder();
        scan_key(&s_sw, KNOB_GPIO_SW, KNOB_EV_OK);
        scan_key(&s_back, KNOB_GPIO_BACK, KNOB_EV_BACK);
        vTaskDelay(pdMS_TO_TICKS(SCAN_PERIOD_MS));
    }
}

void knob_init(void)
{
    gpio_config_t io = { 0 };
    io.pin_bit_mask = (1ULL << KNOB_GPIO_A) | (1ULL << KNOB_GPIO_B)
                    | (1ULL << KNOB_GPIO_SW) | (1ULL << KNOB_GPIO_BACK);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    /* 初始化相位基准，避免上电时误触发一次旋转 */
    s_enc_prev = (int8_t)((gpio_get_level(KNOB_GPIO_A) << 1) | gpio_get_level(KNOB_GPIO_B));

    xTaskCreate(knob_task, "knob", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "init ok (A=%d B=%d SW=%d BACK=%d)",
             KNOB_GPIO_A, KNOB_GPIO_B, KNOB_GPIO_SW, KNOB_GPIO_BACK);
}

void knob_set_handler(knob_handler_t cb)
{
    s_handler = cb;
}
