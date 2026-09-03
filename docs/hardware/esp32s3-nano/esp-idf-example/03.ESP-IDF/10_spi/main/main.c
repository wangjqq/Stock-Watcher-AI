
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "lcd.h"
#include "bsp_spi.h"



spi_device_handle_t spi2_handle;


void app_main(void)
{
   uint8_t pBuffer[600] = { 0x10,0x20,0x30,0x40,0x50,0x90,0x11,0x00,0x00,0x00 };
    int id=0;
    w25q64_init_config(&spi2_handle);
    id=bsp_spi_flash_ReadID(spi2_handle);
    printf("id = %X\r\n",id);
    //LCD_Init();

    //LCD_Fill(0,0,LCD_W,LCD_H,0xFFFF);//清全屏为白色
    // bsp_spi_flash_SectorErase(spi2_handle, 0);
	// bsp_spi_flash_BufferWrite(spi2_handle, pBuffer, 1, 600);

	uint8_t rBuffer[600];
	bsp_spi_flash_BufferRead(spi2_handle, rBuffer, 1, 600);
	for (int i = 0; i < 600; i++)
	{
		if (i % 10 == 0)
		{
			printf("\n");
		}
		printf("0x%x ", rBuffer[i]);
		
	}

    while(1)
    {
        // LCD_DrawPoint(i++, 10, 0x0000);
        // if( i > 200 ) {i = 0;LCD_Fill(0,0,LCD_W,LCD_H,0xFFFF);}
         vTaskDelay(100/portTICK_PERIOD_MS);
    }
}
