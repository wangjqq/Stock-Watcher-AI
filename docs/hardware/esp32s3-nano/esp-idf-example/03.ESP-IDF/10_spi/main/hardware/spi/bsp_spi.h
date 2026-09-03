#ifndef _BSP_SPI_H__
#define _BSP_SPI_H__

#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "hal/gpio_types.h"
#include <string.h>   

//定义Flash实验所需要的引脚
#define Flash_SPI								SPI2_HOST
#define Flash_SPI_MISO							GPIO_NUM_12  	
#define Flash_SPI_MOSI							GPIO_NUM_13	
#define Flash_SPI_SCLK							GPIO_NUM_14	
#define Flash_SPI_CS							GPIO_NUM_15		
#define Flash_SPI_WP							-1
#define Flash_SPI_HD							-1
#define Flash_SPI_DMA							SPI_DMA_CH1

//定义设备参数
#define Flash_CLK_SPEED							6 * 1000 * 1000  //6M的时钟
#define Flash_Address_Bits						3*8				//地址位长度
#define Flash_Command_Bits						1*8				//命令位长度
#define Flash_Dummy_Bits						0*8				//dummy位长度
#define SPI_Flash_PageSize						256				//页写入最大值


//定义命令指令
#define W25X_JedecDeviceID						0x9F		//获取flashID的指令
#define W25X_WriteEnable						0x06		//写入使能
#define W25X_WriteDisable						0x04		//禁止写入
#define W25X_ReadStatusReg						0x05		//读取状态寄存器
#define W25X_SectorErase						0x20		//扇区擦除
#define W25X_BlockErase							0xD8		//块擦除
#define W25X_ChipErase							0xC7		//芯片擦除
#define W25X_PageProgram						0x02		//页写入
#define W25X_ReadData							0x03		//数据读取

#define Dummy_Byte								0xFF		//空指令，用于填充发送缓冲区
#define WIP_Flag								0x01		//flash忙碌标志位
#define WIP_SET									1 


esp_err_t w25q64_init_config(spi_device_handle_t* handle);
uint32_t bsp_spi_flash_ReadID(spi_device_handle_t handle);

void bsp_spi_flash_SectorErase(spi_device_handle_t handle,uint32_t SectorAddr);
void bsp_spi_flash_PageWrite(spi_device_handle_t handle, uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite);
void bsp_spi_flash_BufferWrite(spi_device_handle_t handle, uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void bsp_spi_flash_BufferRead(spi_device_handle_t handle, uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
#endif
