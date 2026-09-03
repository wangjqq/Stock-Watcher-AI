#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_pwm.h"



void app_main(void)
{
    int duty_value = 0;
    // 设置led外设配置
    LedcInitConfig();
    // 设置占空比为50
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    // 更新通道占空比
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    while(1)
    {
        // 实现渐亮效果
        for(duty_value=0;duty_value<8191;duty_value+=100) 
        {
            // 设置亮度模拟值，占空比不断加大
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_value);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            // 延时 10ms
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        // 实现渐灭效果
        for(duty_value=8191;duty_value>=0;duty_value-=100) 
        {
            // 设置亮度模拟值，占空比不断减少
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_value);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            // 延时 10ms
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

    }
}
