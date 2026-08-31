#pragma once

#include <stdint.h>

/* 初始化：配置 RGB LED 三路 PWM（GPIO 见 led.c 顶部） */
void led_init(void);

/* 设置颜色（0-255）。led_off() 等价 led_set_color(0,0,0) */
void led_set_color(uint8_t r, uint8_t g, uint8_t b);

void led_off(void);
