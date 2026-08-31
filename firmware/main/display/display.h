#pragma once

#include <stdint.h>
#include "esp_err.h"

/*
 * 屏幕抽象层。
 * 第一阶段用日志模拟屏幕，保证闭环可运行；
 * 后续接入真实 LCD/TFT（esp_lcd / LVGL 等）时，只需替换本文件实现。
 */

esp_err_t display_init(void);

void display_clear(uint32_t color); /* RGB888 */

void display_draw_text(int x, int y, const char *text, int font_size, uint32_t color);

void display_update(void);
