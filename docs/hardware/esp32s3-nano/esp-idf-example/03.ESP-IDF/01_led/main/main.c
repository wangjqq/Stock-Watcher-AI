#include <stdio.h>
#include "hardware/led/bsp_led.h"

/**
 * @函数说明        
 * @传入参数
 * @函数返回
 */
void app_main(void)
{
    //LED初始化
    LedGpioConfig();

    //设置引脚输出低电平
    LedOn();
    
}