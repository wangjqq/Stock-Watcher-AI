#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "bsp_led.h"


void app_main(void)
{
    //LED初始化
    LedGpioConfig();
    
    while(1) 
    {
        //点亮LED
        LedOn();
        //延时100ms，即亮100ms
        vTaskDelay(100 / portTICK_PERIOD_MS);
        //关闭LED
        LedOff();
        //延时100ms，即灭100ms
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
