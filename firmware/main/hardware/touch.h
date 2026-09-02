#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

/* ------------------------------------------------------------------
 * 触摸屏 XPT2046（SPI 电阻式，2.4" ILI9341 模块自带）
 *
 *  T_CLK / T_DIN 与 LCD 共用同一条 SPI 总线（SCLK / MOSI，见 display.c），
 *  T_CS 独立片选，T_DO 接共享 MISO（display.c 的 PIN_MISO = GPIO11），
 *  T_IRQ 中断输入（按下为低，内部上拉）。
 * 接线与 PINMAP.md 保持一致。
 * ------------------------------------------------------------------ */
#define TOUCH_PIN_CS   GPIO_NUM_9
#define TOUCH_PIN_IRQ  GPIO_NUM_15

/* 屏幕分辨率（标定映射目标，与 display.h 一致） */
#define TOUCH_SCREEN_W 240
#define TOUCH_SCREEN_H 320

/* 触摸事件回调：一次点按（tap），坐标为屏幕像素（含顶部状态栏区域 0..W-1 / 0..H-1） */
typedef void (*touch_handler_t)(int x, int y);

/* 初始化：注册到 LCD 共享 SPI 总线 + 配置 IRQ + 读取 NVS 标定 + 启动轮询任务 */
esp_err_t touch_init(void);

/* 注册 tap 回调（只支持一个，传入 NULL 清除） */
void touch_set_handler(touch_handler_t cb);

/* 立即读取一次触摸：按标定映射为屏幕像素坐标；有按下返回 true。
 * 未标定时恒返回 false（坐标不可用）。 */
bool touch_read(int *x, int *y);

/* 读取原始 ADC 值（未映射），有按下返回 true；标定用 */
bool touch_read_raw(int *rx, int *ry);

/* 是否已标定（NVS 中有有效标定） */
bool touch_is_calibrated(void);

/* ---- 两角标定（左上 + 右下） ---- */

/* 进入标定：清掉旧标定，等待第一次点按记录左上角 */
void touch_cal_begin(void);

/* 当前标定阶段：0=空闲 1=等左上 2=等右下 */
int touch_cal_state(void);

/* 每帧喂入原始点，完成标定并保存到 NVS 时返回 true，否则 false。
 * 由上层在标定界面轮询 touch_read_raw 后调用。 */
bool touch_cal_feed(int raw_x, int raw_y, bool pressed);
