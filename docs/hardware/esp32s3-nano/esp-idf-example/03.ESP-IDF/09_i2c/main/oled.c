
#include "oled.h"
#include "stdlib.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/

static u8 OLED_GRAM[144][8];

#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */


/******************************************************************
 * 函 数 名 称：OLED_WR_Byte
 * 函 数 说 明：发送一个字节
 * 函 数 形 参：dat:要发送的数据    mode:数据/命令标志 0,表示命令;1,表示数据;
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void OLED_WR_Byte(__uint8_t dat,__uint8_t mode)
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    int err=0;
    i2c_master_start(cmd);
    err = i2c_master_write_byte(cmd, 0x78, ACK_CHECK_EN);
    
    if(mode)    
    {
      i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);
    }
    else        
    {
      i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN);
    }
    
    i2c_master_write_byte(cmd, dat, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);  
}

//更新显存到OLED 
void OLED_Refresh(void)
{
  u8 i,n;
  for(i=0;i<8;i++)
  {
    OLED_WR_Byte(0xb0+i,OLED_CMD); //设置行起始地址
    OLED_WR_Byte(0x00,OLED_CMD);   //设置低列起始地址
    OLED_WR_Byte(0x10,OLED_CMD);   //设置高列起始地址
        

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x78, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN);

    for(n=0;n<128;n++)
    {
      i2c_master_write_byte(cmd, OLED_GRAM[n][i], ACK_CHECK_EN);
      // I2C_WaitAck();
    }
    i2c_master_stop(cmd);        
    i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_PERIOD_MS);                       
    i2c_cmd_link_delete(cmd);
  }
}

//清屏函数
void OLED_Clear(void)
{
  u8 i,n;
  for(i=0;i<8;i++)
  {
     for(n=0;n<128;n++)
      {
       OLED_GRAM[n][i]=0;//清除所有数据
      }
  }
  OLED_Refresh();//更新显示
}

//画点 
//x:0~127
//y:0~63
//t:1 填充 0,清空 
void OLED_DrawPoint(u8 x,u8 y,u8 t)
{
  u8 i,m,n;
  i=y/8;
  m=y%8;
  n=1<<m;
  if(t){OLED_GRAM[x][i]|=n;}
  else
  {
    OLED_GRAM[x][i]=~OLED_GRAM[x][i];
    OLED_GRAM[x][i]|=n;
    OLED_GRAM[x][i]=~OLED_GRAM[x][i];
  }
}

//OLED的初始化
void OLED_Init(void)
{
    int i2c_master_port = I2C_NUM_1;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_7,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = GPIO_NUM_8,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    i2c_param_config(i2c_master_port, &conf);

    //注册I2C服务即使能
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);    

  OLED_WR_Byte(0xAE,OLED_CMD); /*display off*/
  OLED_WR_Byte(0x00,OLED_CMD); /*set lower column address*/ 
  OLED_WR_Byte(0x10,OLED_CMD); /*set higher column address*/
  OLED_WR_Byte(0x00,OLED_CMD); /*set display start line*/ 
  OLED_WR_Byte(0xB0,OLED_CMD); /*set page address*/ 
  OLED_WR_Byte(0x81,OLED_CMD); /*contract control*/ 
  OLED_WR_Byte(0xff,OLED_CMD); /*128*/ 
  OLED_WR_Byte(0xA1,OLED_CMD); /*set segment remap*/ 
  OLED_WR_Byte(0xA6,OLED_CMD); /*normal / reverse*/ 
  OLED_WR_Byte(0xA8,OLED_CMD); /*multiplex ratio*/ 
  OLED_WR_Byte(0x1F,OLED_CMD); /*duty = 1/32*/ 
  OLED_WR_Byte(0xC8,OLED_CMD); /*Com scan direction*/ 
  OLED_WR_Byte(0xD3,OLED_CMD); /*set display offset*/ 
  OLED_WR_Byte(0x00,OLED_CMD); 
  OLED_WR_Byte(0xD5,OLED_CMD); /*set osc division*/ 
  OLED_WR_Byte(0x80,OLED_CMD); 
  OLED_WR_Byte(0xD9,OLED_CMD); /*set pre-charge period*/ 
  OLED_WR_Byte(0x1f,OLED_CMD); 
  OLED_WR_Byte(0xDA,OLED_CMD); /*set COM pins*/ 
  OLED_WR_Byte(0x00,OLED_CMD); 
  OLED_WR_Byte(0xdb,OLED_CMD); /*set vcomh*/ 
  OLED_WR_Byte(0x40,OLED_CMD); 
  OLED_WR_Byte(0x8d,OLED_CMD); /*set charge pump enable*/ 
  OLED_WR_Byte(0x14,OLED_CMD);
  OLED_Clear();
  OLED_WR_Byte(0xAF,OLED_CMD); /*display ON*/

}