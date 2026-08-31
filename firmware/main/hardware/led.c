#include "led.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "led";

/* ------------------------------------------------------------------
 * 接线（与 PINMAP.md 保持一致）：三引脚 RGB LED
 *  默认共阴极（高电平点亮）。若为共阳极，把 LED_ACTIVE_HIGH 改为 0。
 * ------------------------------------------------------------------ */
#define LED_GPIO_R GPIO_NUM_33
#define LED_GPIO_G GPIO_NUM_21
#define LED_GPIO_B GPIO_NUM_22

#define LED_ACTIVE_HIGH 1

#define LED_CH_R LEDC_CHANNEL_1
#define LED_CH_G LEDC_CHANNEL_2
#define LED_CH_B LEDC_CHANNEL_3

static void channel_setup(ledc_channel_t ch, gpio_num_t pin)
{
    ledc_channel_config_t c = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = ch,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&c);
}

void led_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_8_BIT, /* 0-255 对应 0-100% */
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    channel_setup(LED_CH_R, LED_GPIO_R);
    channel_setup(LED_CH_G, LED_GPIO_G);
    channel_setup(LED_CH_B, LED_GPIO_B);
    ESP_LOGI(TAG, "init ok");
}

void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t cr = r, cg = g, cb = b;
#if !LED_ACTIVE_HIGH
    cr = 255 - cr;
    cg = 255 - cg;
    cb = 255 - cb;
#endif
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CH_R, cr);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CH_G, cg);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CH_B, cb);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CH_R);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CH_G);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CH_B);
}

void led_off(void)
{
    led_set_color(0, 0, 0);
}
