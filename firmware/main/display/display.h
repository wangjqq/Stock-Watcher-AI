#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ------------------------------------------------------------------
 * 屏幕与画布参数（与前端 web/src/constants.ts 保持一致）
 *
 *  屏幕 128x160 (ST7735, 1.8")，顶部 16px 为状态栏，
 *  状态栏以下 128x144 为可配置显示区域（canvas）。
 * ------------------------------------------------------------------ */
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      160
#define STATUS_BAR_HEIGHT   16
#define CANVAS_WIDTH        128
#define CANVAS_HEIGHT       (DISPLAY_HEIGHT - STATUS_BAR_HEIGHT)  /* 144 */

esp_err_t display_init(void);

void display_clear(uint32_t color); /* RGB888 */
void display_fill_rect(int x, int y, int w, int h, uint32_t color);
void display_draw_text(int x, int y, const char *text, int font_size, uint32_t color);

/* 状态栏（顶部 16px）：左边时间，右边信号(4格)+电池。由 status_bar 定时调用 */
void display_draw_status_bar(const char *time_str, int rssi_dbm, int battery_pct);

/* 把帧缓冲整屏刷到 LCD */
void display_update(void);
