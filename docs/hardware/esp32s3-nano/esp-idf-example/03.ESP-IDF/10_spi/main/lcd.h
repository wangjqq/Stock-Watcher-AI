 /********************************************************************************
   * 测试硬件：立创·梁山派开发板GD32F470ZGT6    使用主频200Mhz    晶振25Mhz
   * 版 本 号: V1.0
   * 修改作者: LC
   * 修改日期: 2023年06月12日
   * 功能介绍:      
   ******************************************************************************
   * 梁山派软硬件资料与相关扩展板软硬件资料官网全部开源  
   * 开发板官网：www.lckfb.com   
   * 技术支持常驻论坛，任何技术问题欢迎随时交流学习  
   * 立创论坛：club.szlcsc.com   
   * 其余模块移植手册：【立创·梁山派开发板】模块移植手册
   * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
   * 不靠卖板赚钱，以培养中国工程师为己任
 *********************************************************************************/

#ifndef __LCDINIT_H__
#define __LCDINIT_H__

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define USE_SOFTWARE   1   //是否使用软件SPI 0使用硬件SPI  1使用软件SPI

#define USE_HORIZONTAL 0  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 240
#define LCD_H 280
#else
#define LCD_W 280
#define LCD_H 240
#endif

//GND - GND
//VCC - 3.3V
//SCL - PB13    SPI1_SCK
//SDA - PB15    SPI1_MOSI
//RES - PD0     (可以接入复位)
//DC  - PC6
//CS  - PB12    SPI1_NSS
//BLK - PC7     TIMER7_CH0

/**************     LCD端口移植    *******************/

#define PIN_NUM_DC GPIO_NUM_6
#define PIN_NUM_RES GPIO_NUM_7

/**************     LCD端口输出定义    *******************/
/*#define LCD_SCLK_Clr() gpio_bit_write(PORT_LCD_SCL, GPIO_LCD_SCL, RESET)//SCL=SCLK
#define LCD_SCLK_Set() gpio_bit_write(PORT_LCD_SCL, GPIO_LCD_SCL, SET)

#define LCD_MOSI_Clr() gpio_bit_write(PORT_LCD_SDA, GPIO_LCD_SDA, RESET)//SDA=MOSI
#define LCD_MOSI_Set() gpio_bit_write(PORT_LCD_SDA, GPIO_LCD_SDA, SET)
*/
#define LCD_RES_Clr()  gpio_set_level(PIN_NUM_RES, 0)//RES
#define LCD_RES_Set()  gpio_set_level(PIN_NUM_RES, 1)

#define LCD_DC_Clr()   gpio_set_level(PIN_NUM_DC, 1)//DC
#define LCD_DC_Set()   gpio_set_level(PIN_NUM_DC, 0)
  /*      
#define LCD_CS_Clr()   gpio_bit_write(PORT_LCD_CS, GPIO_LCD_CS, RESET)//CS
#define LCD_CS_Set()   gpio_bit_write(PORT_LCD_CS, GPIO_LCD_CS, SET)

#define LCD_BLK_Clr()  gpio_bit_write(PORT_LCD_BLK, GPIO_LCD_BLK, RESET)//BLK
#define LCD_BLK_Set()  gpio_bit_write(PORT_LCD_BLK, GPIO_LCD_BLK, SET)
*/  

void lcd_gpio_config(void);//初始化GPIO
void LCD_WR_DATA8(uint8_t dat);//写入一个字节
void LCD_WR_DATA(uint16_t dat);//写入两个字节
void LCD_WR_REG(uint8_t dat);//写入一个指令
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);//设置坐标函数
void LCD_Init(void);
void LCD_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color);
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color);
#endif