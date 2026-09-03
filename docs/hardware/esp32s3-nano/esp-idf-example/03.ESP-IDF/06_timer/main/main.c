#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_timer.h"


static const char *TAG = "example";


void app_main(void)
{
    int number = 0;
    QueueHandle_t queue = 0;

    // 初始化定时器 1秒进入回调函数一次
    queue = timerInitConfig(1000000,1000000);
   
    
    while(1)
    {
        //从队列中接收一个数据，不能在中断服务函数使用
        if (xQueueReceive(queue, &number, pdMS_TO_TICKS(2000))) 
        {
            ESP_LOGI(TAG, "Timer stopped, count=%d", number);
        } else {
            ESP_LOGW(TAG, "Missed one count event");
        }
    }
}