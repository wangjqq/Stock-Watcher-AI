#include "touch.h"

#include <string.h>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "display.h"

static const char *TAG = "touch";

/* XPT2046 SPI 时钟：数据手册建议 2.5MHz 以内 */
#define TOUCH_SPI_FREQ_HZ (2 * 1000 * 1000)
/* 轮询周期：约 60Hz，配合 tap 判定足够 */
#define TOUCH_POLL_MS     15
/* 点按最长时间：超过视为长按，不触发 tap */
#define TOUCH_TAP_MAX_MS  400
/* 滑动最长时间：超过视为拖拽/长按，不触发滑动 */
#define TOUCH_SWIPE_MAX_MS 1000
/* 判定为滑动的横向位移阈值（屏幕像素） */
#define TOUCH_SWIPE_THRESH 40
/* 左右边缘宽度：从该区域内向内滑视为「返回」手势 */
#define TOUCH_EDGE_W       32
/* 单次读取的采样次数（取中值滤波） */
#define TOUCH_NSAMPLE     3

/* XPT2046 控制字节：S | A2A1A0 | MODE | SER | PD1PD0 */
#define CMD_X      0x90  /* X 位置（差分，PD=00） */
#define CMD_Y      0xD0  /* Y 位置（差分，PD=00） */
#define CMD_Z1     0xB0  /* Z1 压力（差分，PD=00） */

static spi_device_handle_t s_spi = NULL;
static touch_handler_t     s_handler = NULL;

/* 标定数据（NVS 持久化）：TL/BR 为左上/右下两角按下的原始 ADC 值。
 * 线性映射：屏幕坐标 = (raw - raw_tl) * (屏宽/高-1) / (raw_br - raw_tl)，
 * 自动兼容任一轴反向（分母为负时翻转）。 */
typedef struct {
    bool   valid;
    int32_t x_tl, x_br; /* 屏幕左/右边界对应的原始 X */
    int32_t y_tl, y_br; /* 屏幕上/下边界对应的原始 Y */
} touch_cal_t;

static touch_cal_t s_cal = { 0 };
static int         s_cal_state = 0; /* 0=空闲 1=等左上 2=等右下 */
static bool        s_cal_prev_pressed = false; /* 标定喂点的上次按下状态（边沿检测） */

#define CAL_NVS_NS   "touch"
#define CAL_NVS_KEY  "cal"

/* ---------------- XPT2046 SPI 读取 ---------------- */

/* 读一个 12 位通道值：先发 8 位控制字节，再读两个字节（结果左对齐，右移 3 位） */
static uint16_t touch_read12(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = { 0 };
    spi_transaction_t t = { 0 };
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    t.length = 24;
    if (spi_device_transmit(s_spi, &t) != ESP_OK) {
        return 0xFFFF;
    }
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
}

/* 采样 N 次取中值，去掉偶发尖峰 */
static uint16_t sample_channel(uint8_t cmd)
{
    uint16_t v[TOUCH_NSAMPLE];
    for (int i = 0; i < TOUCH_NSAMPLE; i++) {
        v[i] = touch_read12(cmd);
    }
    /* 冒泡排序取中值（N=3，开销可忽略） */
    for (int i = 0; i < TOUCH_NSAMPLE - 1; i++) {
        for (int j = i + 1; j < TOUCH_NSAMPLE; j++) {
            if (v[j] < v[i]) {
                uint16_t tmp = v[i];
                v[i] = v[j];
                v[j] = tmp;
            }
        }
    }
    return v[TOUCH_NSAMPLE / 2];
}

/* 原始坐标映射到屏幕像素（未标定时返回 false） */
static bool raw_to_screen(int rx, int ry, int *sx, int *sy)
{
    if (!s_cal.valid) {
        return false;
    }
    int dx = (int)(s_cal.x_br - s_cal.x_tl);
    int dy = (int)(s_cal.y_br - s_cal.y_tl);
    if (dx == 0 || dy == 0) {
        return false;
    }
    *sx = ((rx - (int)s_cal.x_tl) * (TOUCH_SCREEN_W - 1)) / dx;
    *sy = ((ry - (int)s_cal.y_tl) * (TOUCH_SCREEN_H - 1)) / dy;
    if (*sx < 0) *sx = 0;
    if (*sx >= TOUCH_SCREEN_W) *sx = TOUCH_SCREEN_W - 1;
    if (*sy < 0) *sy = 0;
    if (*sy >= TOUCH_SCREEN_H) *sy = TOUCH_SCREEN_H - 1;
    return true;
}

/* ---------------- NVS 标定持久化 ---------------- */

static void cal_save(void)
{
    nvs_handle_t h;
    if (nvs_open(CAL_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, CAL_NVS_KEY, &s_cal, sizeof(s_cal));
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG, "cal saved (x:%d..%d y:%d..%d)", s_cal.x_tl, s_cal.x_br, s_cal.y_tl, s_cal.y_br);
    }
}

static void cal_load(void)
{
    s_cal.valid = false;
    nvs_handle_t h;
    if (nvs_open(CAL_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_cal);
        if (nvs_get_blob(h, CAL_NVS_KEY, &s_cal, &len) == ESP_OK && len == sizeof(s_cal) && s_cal.valid) {
            ESP_LOGI(TAG, "cal loaded (x:%d..%d y:%d..%d)", s_cal.x_tl, s_cal.x_br, s_cal.y_tl, s_cal.y_br);
        } else {
            ESP_LOGW(TAG, "no valid calibration, touch disabled until calibrated");
        }
        nvs_close(h);
    }
}

/* ---------------- 对外接口 ---------------- */

bool touch_read_raw(int *rx, int *ry)
{
    if (!s_spi) {
        return false;
    }
    /* IRQ 按下为低；未按下直接返回，避免多余 SPI 传输 */
    if (gpio_get_level(TOUCH_PIN_IRQ) != 0) {
        return false;
    }
    *rx = sample_channel(CMD_X);
    *ry = sample_channel(CMD_Y);
    return true;
}

bool touch_read(int *x, int *y)
{
    int rx, ry;
    if (!touch_read_raw(&rx, &ry)) {
        return false;
    }
    return raw_to_screen(rx, ry, x, y);
}

bool touch_is_calibrated(void)
{
    return s_cal.valid;
}

/* 轮询任务：按「按下→抬起」判定一次手势。
 *   - 位移小且时长短 → TAP（用按压中心坐标上报）
 *   - 横向位移超过阈值 → 左右滑动（方向按起点→终点）
 *   - 从屏幕左/右边缘向内滑 → 返回手势（TOUCH_SWIPE_BACK）
 *   - 长按 / 斜向 / 竖直滑动暂不处理 */
static void touch_task(void *arg)
{
    (void)arg;
    bool     prev_pressed = false;
    uint32_t press_ms = 0;
    int      fx = 0, fy = 0;    /* 起点（首采样） */
    int      lx = 0, ly = 0;    /* 终点（最新采样） */
    int      acc_x = 0, acc_y = 0, n = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
        int rx, ry;
        bool pressed = touch_read_raw(&rx, &ry);

        if (pressed && !prev_pressed) {
            prev_pressed = true;
            press_ms = (uint32_t)(esp_timer_get_time() / 1000);
            fx = rx; fy = ry; lx = rx; ly = ry;
            acc_x = rx; acc_y = ry; n = 1;
        } else if (pressed) {
            lx = rx; ly = ry;
            acc_x += rx; acc_y += ry; n++;
        } else if (!pressed && prev_pressed) {
            prev_pressed = false;
            if (!s_handler) {
                continue;
            }
            int sx, sy, ex, ey;
            if (!raw_to_screen(fx, fy, &sx, &sy) || !raw_to_screen(lx, ly, &ex, &ey)) {
                continue; /* 未标定或坐标无效，不产生事件 */
            }
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            uint32_t dur = now_ms - press_ms;
            int dx = ex - sx;
            int dy = ey - sy;
            int adx = dx < 0 ? -dx : dx;
            int ady = dy < 0 ? -dy : dy;

            touch_event_t ev = { 0 };
            ev.x = sx; ev.y = sy; ev.ex = ex; ev.ey = ey;

            if (adx < TOUCH_SWIPE_THRESH && ady < TOUCH_SWIPE_THRESH && dur <= TOUCH_TAP_MAX_MS) {
                /* 短按：以按压中心作为坐标 */
                if (n > 0) {
                    raw_to_screen(acc_x / n, acc_y / n, &ev.x, &ev.y);
                }
                ev.ev = TOUCH_TAP;
            } else if (adx >= TOUCH_SWIPE_THRESH && adx > ady && dur <= TOUCH_SWIPE_MAX_MS) {
                /* 横向滑动：从左右边缘向内滑 = 返回 */
                if ((sx < TOUCH_EDGE_W && dx > 0) ||
                        (sx >= TOUCH_SCREEN_W - TOUCH_EDGE_W && dx < 0)) {
                    ev.ev = TOUCH_SWIPE_BACK;
                } else {
                    ev.ev = (dx > 0) ? TOUCH_SWIPE_RIGHT : TOUCH_SWIPE_LEFT;
                }
            } else {
                continue; /* 长按 / 斜向 / 竖直滑动：暂不处理 */
            }
            s_handler(&ev);
        }
    }
}

void touch_set_handler(touch_handler_t cb)
{
    s_handler = cb;
}

esp_err_t touch_init(void)
{
    /* IRQ：输入 + 内部上拉（XPT2046 /PENIRQ 为开漏，按下拉低） */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << TOUCH_PIN_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    /* 注册为 LCD 共享总线上的第二个设备 */
    spi_device_interface_config_t dev = {
        .mode = 0,
        .clock_speed_hz = TOUCH_SPI_FREQ_HZ,
        .spics_io_num = TOUCH_PIN_CS,
        .queue_size = 1,
    };
    esp_err_t err = display_spi_add_device(&dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi add device failed: %s", esp_err_to_name(err));
        return err;
    }

    cal_load();

    xTaskCreate(touch_task, "touch", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "init ok (CS=%d IRQ=%d calibrated=%d)",
             TOUCH_PIN_CS, TOUCH_PIN_IRQ, s_cal.valid);
    return ESP_OK;
}

/* ---------------- 两角标定 ---------------- */

void touch_cal_begin(void)
{
    s_cal.valid = false;
    s_cal_state = 1;
    s_cal_prev_pressed = false;
}

int touch_cal_state(void)
{
    return s_cal_state;
}

/* 按下沿触发式喂点：只处理"松开→按下"的一次跳变，避免按住时重复记录 */
bool touch_cal_feed(int raw_x, int raw_y, bool pressed)
{
    bool rising = pressed && !s_cal_prev_pressed;
    s_cal_prev_pressed = pressed;
    if (s_cal_state == 0 || !rising) {
        return false;
    }
    if (s_cal_state == 1) { /* 第一次点按：左上角 */
        s_cal.x_tl = raw_x;
        s_cal.y_tl = raw_y;
        s_cal_state = 2;
        return false;
    }
    /* 第二次点按：右下角，要求两次原始坐标不重合 */
    s_cal.x_br = raw_x;
    s_cal.y_br = raw_y;
    if (s_cal.x_br == s_cal.x_tl || s_cal.y_br == s_cal.y_tl) {
        s_cal_state = 1; /* 无效点，重新等左上 */
        return false;
    }
    s_cal.valid = true;
    s_cal_state = 0;
    cal_save();
    return true;
}
