#pragma once

#include <stdint.h>

/* 崩溃日志：开机时根据上次复位原因统计「异常复位」次数，并记录最后崩溃原因。
 * 存储于独立 NVS 命名空间 "crash"，与配置（"app"）相互独立：
 * 一键重置配置不会清空崩溃统计，便于持续追踪设备稳定性。 */

/* 初始化：读取上次复位原因，若为异常复位（panic / 看门狗 / 电压跌落等）则
 * 崩溃次数 +1 并更新最后崩溃原因。需在 nvs_flash_init 之后调用一次。 */
void crashlog_init(void);

/* 累计异常复位次数 */
uint32_t crashlog_get_count(void);

/* 最后崩溃原因码（esp_reset_reason_t），0 = 从未异常复位 */
int32_t crashlog_get_last_code(void);

/* 原因码 → 可读字符串（ASCII，8px 点阵可直接显示；web 端按码映射中文） */
const char *crashlog_reason_str(int32_t code);
