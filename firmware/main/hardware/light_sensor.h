#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 初始化 I2C 总线并配置 BH1750（持续高分辨率模式）。
 * 传感器未接时读取会失败并返回 0，不影响系统运行。 */
void light_sensor_init(void);

/* 读取环境光照度（lux）。返回 0 表示读取失败 / 传感器未接 */
uint16_t light_sensor_read_lux(void);
