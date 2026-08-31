#pragma once

#include <stdint.h>

/* 连上网（STA 拿到 IP）后调用，开始 SNTP 校时（只初始化一次） */
void status_bar_start_sntp(void);

/* 把状态栏绘制进帧缓冲（不刷新屏幕），由主循环每秒调用。
 * 时间未同步时显示运行时长（U hh:mm）。 */
void status_bar_draw(void);
