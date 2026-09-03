#ifndef _BSP_PWM_H_
#define _BSP_PWM_H_

#include "driver/ledc.h"


#define LEDC_TIMER              LEDC_TIMER_0        //定时器0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE //低速模式
#define LEDC_OUTPUT_IO          (48)                // 定义输出GPIO为GPIO48
#define LEDC_CHANNEL            LEDC_CHANNEL_0      // 使用LEDC的通道0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT   // LEDC分辨率设置为13位
#define LEDC_DUTY               (4095)              // 设置占空比为50%。 ((2的13次方) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (100)              // 频率单位是Hz。设置频率为5000 Hz




/**
 * @函数说明        LEDC功能初始化
 * @传入参数        无
 * @函数返回        无
 * @备    注        PWM频率越高，可用的占空比分辨率越低
 */
void LedcInitConfig(void);


#endif