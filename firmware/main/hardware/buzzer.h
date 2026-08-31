#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 一个音符：频率(Hz)，0 = 停顿；时长(ms) */
typedef struct {
    uint16_t freq_hz;
    uint16_t dur_ms;
} buzzer_note_t;

/* 预设声音事件 */
typedef enum {
    SND_KEY = 0,    /* 按键 */
    SND_RISE,       /* 涨 */
    SND_FALL,       /* 跌 */
    SND_ALERT,      /* 告警 */
    SND_DISCONN,    /* 断网 */
    SND_COUNT
} sound_event_t;

/* 初始化：配置 LEDC PWM（GPIO 见 buzzer.c 顶部 PIN_BUZZER） */
void buzzer_init(void);

/* 异步播放音符序列，不阻塞调用方。返回 0 成功；序列过长/参数非法返回 -1 */
int buzzer_play(const buzzer_note_t *notes, int count);

/* 播放预设声音事件（受开关与音量控制） */
int buzzer_play_event(sound_event_t ev);

/* 开关与音量（0-100），网页配置写入后调用 */
void buzzer_set_enabled(bool enabled);
void buzzer_set_volume(uint8_t volume);

/* 停止当前播放并关闭蜂鸣 */
void buzzer_stop(void);
