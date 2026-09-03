#ifndef _BSP_LED_H_
#define _BSP_LED_H_

#include "driver/gpio.h"


//设置LED引脚
#define  LED_PIN  48


/**
 * @函数说明        LED初始化
 * @传入参数        无
 * @函数返回        无
 */
void LedGpioConfig(void);

/**
 * @函数说明        设置LED亮
 * @传入参数        无
 * @函数返回        无
 */
void LedOn(void);

/**
 * @函数说明        设置LED灭
 * @传入参数        无
 * @函数返回        无
 */
void LedOff(void);
#endif
