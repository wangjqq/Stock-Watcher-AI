#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_led.h"
#include "bsp_key.h"


void app_main(void)
{
    int cnt = 0; 
    //LED初始化
    LedGpioConfig();
    //按键初始化
    KeyGpioConfig();
    
    while(1) {
        //因ESP32采用的是RTOS方式运行，必须在死循环中加入延时让其能够正常运转
        vTaskDelay(20 / portTICK_PERIOD_MS);   

        //如果按键有按下
        if( GetKeyValue() == 0 )
        {
            //使LED状态取反。LED引脚为GPIO48
            gpio_set_level(LED_PIN, cnt = !cnt);
        }
    }
}
