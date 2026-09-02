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

/* 初始化屏幕，brightness 为初始背光亮度 0-100 */
esp_err_t display_init(uint8_t brightness);

/* 运行期调整背光亮度 0-100（LEDC PWM） */
void display_set_brightness(uint8_t pct);

void display_clear(uint32_t color); /* RGB888 */
void display_fill_rect(int x, int y, int w, int h, uint32_t color);
void display_draw_text(int x, int y, const char *text, int font_size, uint32_t color);

/* 中英混排文本（统一 16px 高度基线）：ASCII 用 8x16 点阵，中文用 GB2312 一级字库 16x16。
 * scale 仅支持 1(16px) / 2(32px)；字库未就绪或缺字时中文以方块占位。
 * 返回整行像素宽（不含起始 x）。 */
int display_draw_text_zh(int x, int y, const char *text, int scale, uint32_t color);

/* 状态栏（顶部 16px）：左边时间，右边信号(4格)+电池。由 status_bar 定时调用 */
void display_draw_status_bar(const char *time_str, int rssi_dbm, int battery_pct);

/* 把帧缓冲整屏刷到 LCD */
void display_update(void);
