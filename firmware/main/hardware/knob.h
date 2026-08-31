#pragma once

#include <stdint.h>

/* 旋钮 + 返回键 输入事件
 *  LEFT  / RIGHT：旋转一格（逆时针 / 顺时针）
 *  OK   ：旋钮按下（确认）
 *  BACK ：返回键
 */
typedef enum {
    KNOB_EV_LEFT = 0,
    KNOB_EV_RIGHT,
    KNOB_EV_OK,
    KNOB_EV_BACK,
    KNOB_EV_COUNT
} knob_event_t;

typedef void (*knob_handler_t)(knob_event_t ev);

/* 初始化：配置 GPIO（内部上拉）+ 启动 10ms 扫描任务 */
void knob_init(void);

/* 注册事件回调（只支持一个，传入 NULL 清除） */
void knob_set_handler(knob_handler_t cb);
