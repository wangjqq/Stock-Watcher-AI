#pragma once

#include <stdint.h>

/* 初始化电池电量 ADC 采样（分压引脚见 battery.c 顶部 PIN_BATT） */
void battery_init(void);

/* 读取当前电量百分比 0-100（带平滑滤波，未接电池时约为 0） */
uint8_t battery_get_percent(void);
