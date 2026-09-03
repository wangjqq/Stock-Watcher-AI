#include <stdio.h>
//导入ESP32的日志输出功能
#include "esp_log.h"

//导入FREERTOS的任务调度
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//导入串口设备库
#include "driver/uart.h"

//导入GPIO设备库
#include "driver/gpio.h"

#include "bsp_uart.h"

void app_main(void)
{
    //初始化串口2 TX=GPIO10 RX=GPIO9
    uart_init_config(UART_NUM_2, 115200, 10, 9);

    //创建串口2接收任务
    xTaskCreate(uart2_rx_task, "uart2_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);

    //通过串口1发送字符串 start uart demo
    uart_write_bytes(UART_NUM_2, (const char*)"start uart demo", strlen("start uart demo"));

    while(1)
    {
        //串口2发送数据
        uart_write_bytes(UART_NUM_2, (const char*)"Task running : main", strlen("Task running : main"));    
        //ESP32S3的日志输出数据
        ESP_LOGI("main", "Task running : main\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);       
    }
}
