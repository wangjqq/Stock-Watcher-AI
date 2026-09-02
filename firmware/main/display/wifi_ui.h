#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "knob.h"
#include "touch.h"

/* ------------------------------------------------------------------
 * 设备端 WiFi 连接页（「系统」应用内，替代原 AP 配网）
 *
 *  列表视图：扫描附近网络 / 手动输入 SSID / 重新扫描
 *  密码视图：触摸软键盘输入密码（字母/数字符号两页）
 *  连接视图：连接中 / 连接结果（成功保存配置，失败提示原因）
 * ------------------------------------------------------------------ */

/* WiFi 页内部视图 */
#define WIFI_VIEW_LIST 0 /* 扫描到的网络列表 + 连接状态 */
#define WIFI_VIEW_PASS 1 /* 密码输入（触摸软键盘） */
#define WIFI_VIEW_CONN 2 /* 连接中 / 连接结果 */

/* 进入 WiFi 页：快照当前已保存配置 + 启动扫描 */
void wifi_ui_enter(void);

/* 退出 WiFi 页（回到系统菜单）：停止扫描、复位内部状态 */
void wifi_ui_exit(void);

/* 主循环每 ~100ms 调用：轮询扫描完成与连接结果 */
void wifi_ui_tick(void);

/* 旋钮输入：LEFT/RIGHT 移动光标、OK 确认、BACK 返回。
 * 密码页内：LEFT=退格、RIGHT=字母/符号页切换、OK=连接、BACK=返回列表 */
void wifi_ui_knob(knob_event_t ev);

/* 触摸输入（屏幕坐标，含状态栏区；本页自行过滤状态栏区） */
void wifi_ui_touch(const touch_event_t *ev);

/* 绘制当前视图到画布区（状态栏以下），绘制后清除待重绘标志 */
void wifi_ui_draw(void);

/* 是否有待重绘（扫描完成 / 连接状态变化 / 输入操作时置位） */
bool wifi_ui_need_redraw(void);

/* 用户请求退出本页（knob BACK / 边缘返回 / 页面内返回键）。
 * 主循环检测到后调用 wifi_ui_exit() 并切回系统菜单。 */
bool wifi_ui_exit_requested(void);
