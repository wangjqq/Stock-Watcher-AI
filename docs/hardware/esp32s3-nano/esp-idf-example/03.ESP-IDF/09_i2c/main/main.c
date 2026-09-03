
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "oled.h"

void app_main(void)
{
     int x = 0, y = 0;

     OLED_Init();
     OLED_Clear();

     while (1)
     {
          OLED_DrawPoint(x++,y,1);//画点

          if( x >= 127 ) 
          {
               x = 0;
               if( y++ >= 32 ) 
               {
                    y = 0;
                    OLED_Clear();
               }
          }

          OLED_Refresh();
          printf("again drwa line\n");
          vTaskDelay(1/portTICK_PERIOD_MS);
     }
   
}