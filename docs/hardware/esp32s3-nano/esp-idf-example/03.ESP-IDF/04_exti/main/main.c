
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_NUM_48) 

#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_NUM_0) 

#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

//中断服务函数
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
     xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

//按键检测任务
static void gpio_get_key_value_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        //读取最新的gpio_evt_queue消息
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void app_main(void)
{
    gpio_config_t io_conf = {};
   
   //配置LED
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    //配置按键
    //下降沿中断
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //设置GPIO0的输入寄存器
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //输入模式
    io_conf.mode = GPIO_MODE_INPUT;
    //使能上拉模式
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    //创建一个队列来处理来自isr的gpio事件
     gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
   
    //注册中断服务
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    
    //设置GPIO的中断服务函数
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void*) GPIO_NUM_0);
   
    //使能GPIO模块中断信号
    gpio_intr_enable(GPIO_NUM_0);
 
    //创建一个按键检测任务
    xTaskCreate(gpio_get_key_value_task,        //任务函数
                "gpio_get_key_value_task",      //任务名字
                2048,                           //任务堆栈
                NULL,                           //传递给任务函数的参数
                10,                             //任务优先级
                NULL                            //任务句柄
     );

    int cnt = 0;
    while(1) {
        printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_48, cnt % 2);
    }
}
