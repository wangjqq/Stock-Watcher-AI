#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"


#define DEFAULT_VREF    1100        //默认参考电压，单位mV

static esp_adc_cal_characteristics_t *adc_chars;

#define channel     ADC_CHANNEL_0               // ADC测量通道
#define width       ADC_WIDTH_BIT_12            // ADC分辨率
#define atten       ADC_ATTEN_DB_11             // ADC衰减
#define unit        ADC_UNIT_1                  // ADC1

void app_main(void)
{
    int read_raw_1=0, read_raw_2=0, read_raw_3=0;
    uint32_t voltage =0;
    float voltage_f = 0;
    adc1_config_width(width);// 12位分辨率
    
    //ADC_ATTEN_DB_0:表示参考电压为1.1V
    //ADC_ATTEN_DB_2_5:表示参考电压为1.5V
    //ADC_ATTEN_DB_6:表示参考电压为2.2V
    //ADC_ATTEN_DB_11:表示参考电压为3.3V
    //adc1_config_channel_atten( channel,atten);// 设置通道0和3.3V参考电压

    // 分配内存
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    // 对 ADC 特性进行初始化，使其能够正确地计算转换结果和补偿因素
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);

    while(1)
    {
        //采集三个通道的ADC值
        read_raw_1 = adc1_get_raw(ADC1_CHANNEL_0);  //GPIO1
        read_raw_2 = adc1_get_raw(ADC1_CHANNEL_1);  //gpio2
        read_raw_3 = adc1_get_raw(ADC1_CHANNEL_2);  //gpio3
        //将ADC1的通道0（gpio1）的结果转换成电压,单位mV
        voltage = esp_adc_cal_raw_to_voltage(read_raw_1, adc_chars);

        //输出ADC值 与 实际电压值
        printf("read_raw_1 = %d\tread_raw_2 = %d\tread_raw_3 = %d\tvoltage: %f\n",read_raw_1,read_raw_2,read_raw_3,voltage/1000.0);
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
