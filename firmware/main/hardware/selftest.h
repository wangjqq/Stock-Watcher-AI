#pragma once

/* 上电硬件自检：LED 三色切换 + 蜂鸣提示音 + 按键事件打日志。
 * 设为 0 可跳过（保留时仅约 1s，不影响正常使用）。 */
#define HW_SELFTEST_ENABLED 1

void hw_selftest_run(void);
