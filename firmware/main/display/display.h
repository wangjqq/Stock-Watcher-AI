#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"

/* ------------------------------------------------------------------
 * 屏幕与画布参数（与前端 web/src/constants.ts 保持一致）
 *
 *  屏幕 240x320 (ILI9341, 2.4")，顶部 16px 为状态栏，
 *  状态栏以下 240x304 为可配置显示区域（canvas）。
 * ------------------------------------------------------------------ */
#define DISPLAY_WIDTH       240
#define DISPLAY_HEIGHT      320
#define STATUS_BAR_HEIGHT   16
#define CANVAS_WIDTH        240
#define CANVAS_HEIGHT       (DISPLAY_HEIGHT - STATUS_BAR_HEIGHT)  /* 304 */

/* 初始化屏幕，brightness 为初始背光亮度 0-100 */
esp_err_t display_init(uint8_t brightness);

/* 运行期调整背光亮度 0-100（LEDC PWM） */
void display_set_brightness(uint8_t pct);

/* 在 LCD 所在的共享 SPI 总线上注册另一个设备（触摸屏 XPT2046 用）。
 * 需在 display_init() 之后调用；返回 spi_bus_add_device 的结果。 */
esp_err_t display_spi_add_device(const spi_device_interface_config_t *dev, spi_device_handle_t *out);

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
