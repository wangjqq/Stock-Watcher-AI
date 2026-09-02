#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

/* 系统应用内部视图 */
#define SYS_VIEW_MENU   0 /* 亮度 / 手动刷新 / 状态 / 触摸标定 */
#define SYS_VIEW_BRIGHT 1 /* 调节亮度 */
#define SYS_VIEW_STATUS 2 /* 显示状态 */
#define SYS_VIEW_CAL    3 /* 触摸标定 */
/* 系统应用菜单项数 */
#define SYS_ITEM_COUNT  4

/* 在画布区（状态栏以下 240x304）绘制应用列表菜单。
 * names[i] 为第 i 个应用名；count = 用户应用数 + 1（末尾为「系统」应用）；
 * cursor 为当前选中项（0..count-1）。 */
void app_ui_draw_menu(const char names[][CONFIG_NAME_MAX], int count, int cursor);

/* 在画布区绘制系统应用页。
 * view 为 SYS_VIEW_*；cursor 为系统菜单光标（0..SYS_ITEM_COUNT-1）；
 * brightness 0-100；auto_brightness 为真表示光敏自动亮度已开启；
 * wifi_ok / rssi / ip / version 为设备状态；refreshing 为真时显示「刷新中」提示。 */
void app_ui_draw_system(int view, int cursor, uint8_t brightness, bool auto_brightness,
                        bool wifi_ok, int rssi, const char *ip,
                        const char *version, bool refreshing);

/* 在画布区绘制触摸标定页。stage：1=等左上角 2=等右下角 0=标定完成 */
void app_ui_draw_touch_cal(int stage);
