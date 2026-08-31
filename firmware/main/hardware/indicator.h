#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 状态指示灯（复用 led 模块，无额外接线）。
 * 颜色优先级：告警橙 > 刷新蓝 > 涨跌色 > 联网绿 / 断网红闪。
 * 由主循环周期调用 indicator_update()（建议 100ms）。 */

/* 初始化状态灯（熄灭） */
void indicator_init(void);

/* 告警（橙色，维持 3 秒）——由条件提醒模块触发 */
void indicator_set_alert(bool on);

/* 数据刷新闪光（蓝色，短暂）——每次成功拉取后调用 */
void indicator_on_refresh(void);

/* 设置涨跌趋势：1 涨 / -1 跌 / 0 平（随涨跌字段变色） */
void indicator_set_trend(int trend);

/* 周期调用：按优先级决定 LED 颜色。wifi_ok 决定联网绿 / 断网红闪底色 */
void indicator_update(bool wifi_ok);
