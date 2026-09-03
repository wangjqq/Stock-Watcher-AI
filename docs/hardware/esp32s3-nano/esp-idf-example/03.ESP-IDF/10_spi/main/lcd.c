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

#include "lcd.h"
#include "stdio.h"
#include "stdlib.h"

spi_device_handle_t spi_port;

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
   
    gpio_set_level(PIN_NUM_DC, dc);
}
/******************************************************************
 * 函 数 名 称：lcd_gpio_config
 * 函 数 说 明：对LCD引脚初始化
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：注意是使用软件SPI还是硬件SPI
******************************************************************/
void lcd_gpio_config(void)
{
    esp_err_t ret;

    spi_bus_config_t buscfg={
        .miso_io_num=GPIO_NUM_4,
        .mosi_io_num=GPIO_NUM_8,
        .sclk_io_num=GPIO_NUM_2,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz= 16 *280*2+8 //最大传输大小
    };
    spi_device_interface_config_t devcfg={

        .clock_speed_hz=80*1000*1000,           //Clock out at 80 MHz
        .mode=3,                                //SPI mode 3   //设置SPI通讯的相位特性和采样边沿。包括了mode0-3四种。
        .spics_io_num=GPIO_NUM_5,               //CS pin  //配置片选线
        .queue_size=7,                          //事务队列尺寸 7个  //传输队列的长度，表示可以在通讯的时候挂起多少个spi通讯。在中断通讯模式的时候会把当前spi通讯进程挂起到队列中
        .pre_cb=lcd_spi_pre_transfer_callback,  // 数据传输前回调，用作D/C（数据命令）线分别处理
    };
  // 初始化SPI总线
    ret=spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    
  // 添加SPI总线驱动
    ret=spi_bus_add_device(SPI2_HOST, &devcfg, &spi_port);
    ESP_ERROR_CHECK(ret);

// 初始化其它控制引脚
  gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_NUM_RES, GPIO_MODE_OUTPUT);
  

}

// /******************************************************************
//  * 函 数 名 称：LCD_Writ_Bus
//  * 函 数 说 明：LCD串行数据写入函数
//  * 函 数 形 参：dat  要写入的串行数据
//  * 函 数 返 回：无
//  * 作       者： LC
//  * 备       注：注意是使用软件SPI还是硬件SPI
// ******************************************************************/
// void LCD_Writ_Bus(uint8_t dat) 
// {  

// #if USE_SOFTWARE                               
//  uint8_t i;
//  LCD_CS_Clr();
//  for(i=0;i<8;i++)
//  {       
//    LCD_SCLK_Clr();
//    if(dat&0x80)
//    {
//       LCD_MOSI_Set();
//    }
//    else
//    {
//       LCD_MOSI_Clr();
//    }
//    LCD_SCLK_Set();
//    dat<<=1;
//  } 
//   LCD_CS_Set();  

// #else

//  LCD_CS_Clr();

//  while(RESET == spi_i2s_flag_get(PORT_SPI, SPI_FLAG_TBE));
//      spi_i2s_data_transmit(PORT_SPI, dat);
//  while(RESET == spi_i2s_flag_get(PORT_SPI, SPI_FLAG_RBNE));
//      spi_i2s_data_receive(PORT_SPI);
      
//  LCD_CS_Set(); 

// #endif
// }
/******************************************************************
 * 函 数 名 称：LCD_WR_DATA8
 * 函 数 说 明：LCD写入8位数据
 * 函 数 形 参：dat 写入的数据
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_WR_DATA8(uint8_t dat)
{
  // LCD_Writ_Bus(dat);
    esp_err_t ret;
  spi_transaction_t t={0};
//  if (len==0) return;       // 长度为0 没有数据要传输
//  memset(&t, 0, sizeof(t));   // 清空结构体
  t.length=1*8;         // 要写入的数据长度 Len 是字节数，len, transaction length is in bits.
  t.tx_buffer=&dat;       // 数据指针
  t.user=(void*)1;        // 设置D/C 线，在SPI传输前回调中根据此值处理DC信号线
  ret=spi_device_polling_transmit(spi_port, &t);    // 开始传输
  assert(ret==ESP_OK);      // 一般不会有问题
  
}

/******************************************************************
 * 函 数 名 称：LCD_WR_DATA
 * 函 数 说 明：LCD写入16位数据
 * 函 数 形 参：dat 写入的数据
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_WR_DATA(uint16_t dat)
{
  // LCD_Writ_Bus(dat>>8);
  // LCD_Writ_Bus(dat);
  uint8_t dat_HL[2] ={dat>>8,dat};
  
  esp_err_t ret;
  spi_transaction_t t={0};
//  if (len==0) return;       // 长度为0 没有数据要传输
  //memset(&t, 0, sizeof(t));   // 清空结构体
  t.length=2*8;         // 要写入的数据长度 Len 是字节数，len, transaction length is in bits.
  t.tx_buffer=dat_HL;       // 数据指针
  t.user=(void*)1;        // 设置D/C 线，在SPI传输前回调中根据此值处理DC信号线
  ret=spi_device_polling_transmit(spi_port, &t);    // 开始传输
  assert(ret==ESP_OK);      // 一般不会有问题

}

/******************************************************************
 * 函 数 名 称：LCD_WR_REG
 * 函 数 说 明：LCD写入命令
 * 函 数 形 参：dat 写入的命令
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_WR_REG(uint8_t dat)
{
  // LCD_DC_Clr();//写命令
  // LCD_Writ_Bus(dat);
  // LCD_DC_Set();//写数据
  esp_err_t ret;
  spi_transaction_t t={0};
//  memset(&t, 0, sizeof(t));   // 清空结构体
  t.length=8;           // 要传输的位数 一个字节 8位
  t.tx_buffer=&dat;       // 将命令填充进去
  t.user=(void*)0;        // 设置D/C 线，在SPI传输前回调中根据此值处理DC信号线
  ret=spi_device_polling_transmit(spi_port, &t);    // 开始传输
  assert(ret==ESP_OK);      // 一般不会有问题
 
}

/******************************************************************
 * 函 数 名 称：LCD_Address_Set
 * 函 数 说 明：设置起始和结束地址
 * 函 数 形 参：x1,x2 设置列的起始和结束地址
                y1,y2 设置行的起始和结束地址
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
  if(USE_HORIZONTAL==0)
  {
    LCD_WR_REG(0x2a);//列地址设置
    LCD_WR_DATA(x1);
    LCD_WR_DATA(x2);
    LCD_WR_REG(0x2b);//行地址设置
    LCD_WR_DATA(y1+20);
    LCD_WR_DATA(y2+20);
    LCD_WR_REG(0x2c);//储存器写
  }
  else if(USE_HORIZONTAL==1)
  {
    LCD_WR_REG(0x2a);//列地址设置
    LCD_WR_DATA(x1);
    LCD_WR_DATA(x2);
    LCD_WR_REG(0x2b);//行地址设置
    LCD_WR_DATA(y1+20);
    LCD_WR_DATA(y2+20);
    LCD_WR_REG(0x2c);//储存器写
  }
  else if(USE_HORIZONTAL==2)
  {
    LCD_WR_REG(0x2a);//列地址设置
    LCD_WR_DATA(x1+20);
    LCD_WR_DATA(x2+20);
    LCD_WR_REG(0x2b);//行地址设置
    LCD_WR_DATA(y1);
    LCD_WR_DATA(y2);
    LCD_WR_REG(0x2c);//储存器写
  }
  else
  {
    LCD_WR_REG(0x2a);//列地址设置
    LCD_WR_DATA(x1+20);
    LCD_WR_DATA(x2+20);
    LCD_WR_REG(0x2b);//行地址设置
    LCD_WR_DATA(y1);
    LCD_WR_DATA(y2);
    LCD_WR_REG(0x2c);//储存器写
  }
}

/******************************************************************
 * 函 数 名 称：LCD_Init
 * 函 数 说 明：LCD初始化
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_Init(void)
{
  lcd_gpio_config();//初始化GPIO
  
  LCD_RES_Clr();//复位
  
  vTaskDelay(100 / portTICK_PERIOD_MS);
  LCD_RES_Set();//停止复位
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  // LCD_BLK_Set();//打开背光
  // vTaskDelay(100 / portTICK_PERIOD_MS);
  
  //************* Start Initial Sequence **********//
  LCD_WR_REG(0x11); //关闭睡眠模式命令
  vTaskDelay(100 / portTICK_PERIOD_MS);             //Delay 120ms 
  //************* Start Initial Sequence **********// 
  LCD_WR_REG(0x36);//设置扫描顺序命令
  

  if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x00);
  else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC0);
  else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x70);
  else LCD_WR_DATA8(0xA0);
        
  //设置数据颜色编码格式命令
  LCD_WR_REG(0x3A);   
  //8位数据总线，16位/像素(RGB 5-6-5位输入)，65K-Colors, 3Ah= " 05h "
  LCD_WR_DATA8(0x05);
  

  LCD_WR_REG(0xB2);     
  LCD_WR_DATA8(0x0C);
  LCD_WR_DATA8(0x0C); 
  LCD_WR_DATA8(0x00); 
  LCD_WR_DATA8(0x33); 
  LCD_WR_DATA8(0x33);       

  LCD_WR_REG(0xB7);     
  LCD_WR_DATA8(0x35);

  LCD_WR_REG(0xBB);     
  LCD_WR_DATA8(0x32); //Vcom=1.35V
          
  LCD_WR_REG(0xC2);
  LCD_WR_DATA8(0x01);

  LCD_WR_REG(0xC3);     
  LCD_WR_DATA8(0x15); //GVDD=4.8V  颜色深度
        
  LCD_WR_REG(0xC4);     
  LCD_WR_DATA8(0x20); //VDV, 0x20:0v

  LCD_WR_REG(0xC6);     
  LCD_WR_DATA8(0x0F); //0x0F:60Hz         

  LCD_WR_REG(0xD0);     
  LCD_WR_DATA8(0xA4);
  LCD_WR_DATA8(0xA1); 

  LCD_WR_REG(0xE0);
  LCD_WR_DATA8(0xD0);   
  LCD_WR_DATA8(0x08);   
  LCD_WR_DATA8(0x0E);   
  LCD_WR_DATA8(0x09);   
  LCD_WR_DATA8(0x09);   
  LCD_WR_DATA8(0x05);   
  LCD_WR_DATA8(0x31);   
  LCD_WR_DATA8(0x33);   
  LCD_WR_DATA8(0x48);   
  LCD_WR_DATA8(0x17);   
  LCD_WR_DATA8(0x14);   
  LCD_WR_DATA8(0x15);   
  LCD_WR_DATA8(0x31);   
  LCD_WR_DATA8(0x34);   

  LCD_WR_REG(0xE1);     
  LCD_WR_DATA8(0xD0);   
  LCD_WR_DATA8(0x08);   
  LCD_WR_DATA8(0x0E);   
  LCD_WR_DATA8(0x09);   
  LCD_WR_DATA8(0x09);   
  LCD_WR_DATA8(0x15);   
  LCD_WR_DATA8(0x31);   
  LCD_WR_DATA8(0x33);   
  LCD_WR_DATA8(0x48);   
  LCD_WR_DATA8(0x17);   
  LCD_WR_DATA8(0x14);   
  LCD_WR_DATA8(0x15);   
  LCD_WR_DATA8(0x31);   
  LCD_WR_DATA8(0x34);
  LCD_WR_REG(0x21); 

  LCD_WR_REG(0x29);
}
/******************************************************************
 * 函 数 名 称：LCD_Fill
 * 函 数 说 明：在指定区域填充颜色
 * 函 数 形 参：xsta,ysta   起始坐标
                xend,yend   终止坐标
        color       要填充的颜色
 * 函 数 返 回：无
 * 作       者： LC
 * 备       注：无
******************************************************************/
void LCD_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color)
{          
  uint16_t i,j; 
  LCD_Address_Set(xsta,ysta,xend-1,yend-1);//设置显示范围
  for(i=ysta;i<yend;i++)
  {                               
    for(j=xsta;j<xend;j++)
    {
      LCD_WR_DATA(color);
    }
  }                   
}

/******************************************************************
 * 函 数 名 称：LCD_DrawPoint
 * 函 数 说 明：在指定位置画点
 * 函 数 形 参：x,y 画点坐标
                color 点的颜色
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{
  LCD_Address_Set(x,y,x,y);//设置光标位置 
  LCD_WR_DATA(color);
} 