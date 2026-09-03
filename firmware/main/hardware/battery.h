#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 初始化电池电量 ADC 采样（分压引脚见 battery.c 顶部 PIN_BATT） */
void battery_init(void);

/* 读取当前电量百分比 0-100（带平滑滤波，未接电池时约为 0） */
uint8_t battery_get_percent(void);

/* 是否检测到电池接入（分压采样电压达到真实电池下限）。
 * 未接电池（如纯 USB 供电）时 ADC 读数为 0，用于低电量保护等场景避免误触发。 */
bool battery_present(void);
