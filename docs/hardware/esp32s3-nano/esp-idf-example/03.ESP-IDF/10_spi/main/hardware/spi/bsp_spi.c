#include "bsp_spi.h"

/**********************************************************
 * 函 数 名 称：w25q64_init_config
 * 函 数 功 能：w25q64初始化
 * 传 入 参 数：无
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
esp_err_t w25q64_init_config(spi_device_handle_t* handle)
{	
	//00 定义错误标志
	esp_err_t e;

	//01 配置总线初始化结构体
	static spi_bus_config_t bus_cfg;    //总线配置结构体

	bus_cfg.miso_io_num		= Flash_SPI_MISO;				 //miso
	bus_cfg.mosi_io_num		= Flash_SPI_MOSI;				 //mosi
	bus_cfg.sclk_io_num		= Flash_SPI_SCLK;				 //sclk
	bus_cfg.quadhd_io_num	= Flash_SPI_HD;					// HD
	bus_cfg.quadwp_io_num	= Flash_SPI_WP;					// WP
	bus_cfg.max_transfer_sz = 4092;							//非DMA最大64bytes,DMA最大4092bytes
	//bus_cfg.intr_flags = 0;								//这个用于设置中断优先级的，0是默认
	bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;
	//这个用于设置初始化的时候要检测哪些选项。比如这里设置的是spi初始化为主机模式是否成功。检测结果通过spi_bus_initialize函数的
	//返回值进行返回。如果初始化为主机模式成功，就会返回esp_ok

	//02 初始化总线配置结构体
	e = spi_bus_initialize(Flash_SPI, &bus_cfg, SPI_DMA_CH_AUTO);

	if (e != ESP_OK)
	{
		printf("bus initialize failed!\n");
		return e;
	}

	//03 配置设备结构体
	static spi_device_interface_config_t interface_cfg; //设备配置结构体


   interface_cfg.address_bits = Flash_Address_Bits;   //配置地址位长度
   //(1)如果设置为0，在通讯的时候就不会发送地址位。
   //(2)如果设置了非零值，就会在spi通讯的地址发送阶段发送指定长度的address数据。
   //如果设置了非零值并且在后面数据发送结构体中没有定义addr的值，会默认发送指定长度0值
   //(3)我们后面发送数据会使用到spi_transaction_t结构体，这个结构体会使用spi_device_interface_config_t中定义好address、command和dummy的长度
   //如果想使用非固定长度，就要使用spi_transaction_ext_t结构体了。这个结构体包括了四个部分，包含了一个spi_transaction_t和address、command、dummy的长度。
   //我们要做的就是在spi_transaction_ext_t.base.flags中设置SPI_TRANS_VARIABLE_ADDR/CMD/DUMMY
   //然后定义好这三部分数据的长度，然后用spi_transaction_ext_t.base的指针代替spi_transaction_t的指针即可


	 
	interface_cfg.command_bits = Flash_Command_Bits;  //配置命令位长度
	//与address_bits是一样的


	interface_cfg.dummy_bits = Flash_Dummy_Bits;  //配置dummy长度
	//这里的配置方法与address_bits是一样的。但是要着重说一下这个配置的意义，后面会再说一遍
	//(1)dummy_bits是用来用来补偿输入延迟。
	//(2)在read phase开始阶段之前被插入进去。在dummy_bits的时钟下，并不进行数据读取的工作
	//相当于这段时间发送的clock都是虚拟的时钟，并没有功能。在输入延迟最大允许时间不够的时候，可以通过这种方法进行配置，从而
	//能够使得系统工作在更高的时钟频率下。
	//(3)如果主机设备只进行write操作，可以在flags中设置SPI_DEVICE_NO_DUMMY，关闭dummy bits的发送。只有写操作的话，即使使用了gpio交换矩阵,时钟周期也可以工作在80MHZ


	//interface_cfg.input_delay_ns = 0;  //配置输入延时的允许范围
	//时钟发出信号到miso进行输入直接会有延迟，这个参数就是配置这个允许的最大延迟时间。
	//如果主机接收到从机时钟，但是超过这个时间没有收到miso发来的输入信号，就会返回通讯失败。
	//这个时间即使设置为0，也能正常工作，但是最好通过手册或逻辑分析仪进行估算。能够实现更好的通讯。
	//超过8M的通讯都应该认真设置这个数字


	interface_cfg.clock_speed_hz = Flash_CLK_SPEED;  //配置时钟频率
	//配置通讯的时钟频率。
	//这个频率受到io_mux和input_delay_ns限制。
	//如果是io直连的，时钟上限是80MHZ，如果是gpio交换矩阵连接进来的,时钟上限是40MHZ。
	//如果是全双工，时钟上限是26MHZ。并且还要考虑输入延时。在相同输入延时的条件下，使用gpio交换矩阵会比使用io mux最大允许的时钟频率小。可以通过
	//spi_get_freq_limit()来计算能够允许的最大时钟频率是多少
	//有关SPI通讯时钟极限和配置的问题，后面会详细说一下。




	interface_cfg.mode = 0; //设置SPI通讯的相位特性和采样边沿。包括了mode0-3四种。要看从设备能够使用哪种模式

	interface_cfg.spics_io_num = Flash_SPI_CS; //配置片选线


	interface_cfg.duty_cycle_pos = 0;  //配置占空比
	//设置时钟的占空比，比例是 pos*1/256,默认为0，也就是50%占空比


	//interface_cfg.cs_ena_pretrans; //在传输之前，片选线应该保持激活状态多少个时钟，只有全双工的时候才需要配置
	//interface_cfg.cs_ena_posttrans; //在传输之后，片选线应该保持激活状态多少个时钟，只有全双工的时候才需要配置


	interface_cfg.queue_size = 6; //传输队列的长度，表示可以在通讯的时候挂起多少个spi通讯。在中断通讯模式的时候会把当前spi通讯进程挂起到队列中


	//interface_cfg.flags; //配置与从机有关的一些参数，比如MSB还是LSB，使不使用三线SPI

	//interface_cfg.pre_cb; 
	//配置通讯前中断。比如不在这里配置cs片选线，把片选线作为自行控制的线，把片选线拉低放在通讯前中断中


	//interface_cfg.post_cb;
	//配置通讯后中断。比如不在这里配置cs片选线，把片选线作为自行控制的线，把片选线拉高放在通讯前中断中


	//04 设备初始化
	e = spi_bus_add_device(Flash_SPI, &interface_cfg, handle);
	if (e != ESP_OK)
	{
		printf("device config error\n");
		return e;
	}
	

	return ESP_OK;
}

uint32_t bsp_spi_flash_ReadID(spi_device_handle_t handle)
{
	//00 定义错误标志
	esp_err_t e;

	//01 接收数据的时候发送的空指令
	uint8_t data[3];
	data[0] = Dummy_Byte;
	data[1] = Dummy_Byte;
	data[2] = Dummy_Byte;

	//02 定义用于返回的数据
	uint32_t Temp;

	//03 定义数据发送接收结构体
	spi_transaction_ext_t ext;					//因为读取设备ID的指令结构与前面定义的默认的不一样，所以需要更改位长
	memset(&ext, 0, sizeof(ext));				//初始化结构体
	ext.command_bits =	8;						//指令位长度为8
	ext.address_bits =	0;						//地址位长度为0
	ext.base.cmd =		W25X_JedecDeviceID;		//设备ID
	ext.base.length =	3 * 8;					//要发送数据的长度
	ext.base.tx_buffer = data;					//要发送数据的内容
	ext.base.rx_buffer = NULL;					//接收数据buffer使用结构体内部带的
	ext.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA;  

	//04 数据收发
	e=spi_device_polling_transmit(handle, &ext.base);

	if (e != ESP_OK)
	{
		printf("get ID error!\n");
		return 0;
	}

	//05 返回获取的数据ID
	uint8_t temp0 = ext.base.rx_data[0];
	uint8_t temp1 = ext.base.rx_data[1];
	uint8_t temp2 = ext.base.rx_data[2];

	Temp = (temp0 << 16) | (temp1 << 8) | temp2;

	return Temp;
}

/**
* @breif flash写使能。在执行页写入和擦除命令之前，都必须执行一次页写入
* @param[in]  handle: 提供SPI的操作句柄
* @retval 无
**/

void bsp_spi_flash_WriteEnable(spi_device_handle_t handle)
{
	esp_err_t e; //错误标志位

	// 定义数据发送接收结构体
	spi_transaction_ext_t ext;					//写使能的长度与默认的不同，需要修改
	memset(&ext, 0, sizeof(ext));				//初始化结构体
	ext.command_bits = 8;						//指令位长度为8
	ext.address_bits = 0;						//地址位长度为0
	ext.base.cmd = W25X_WriteEnable;			//写使能
	ext.base.length = 0;						//要发送数据的长度，这里不需要发送数据
	ext.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;

	//发送指令
	e = spi_device_polling_transmit(handle, &ext.base);

	if (e != ESP_OK)
	{
		printf("write enable failed!\n");
		
	}

}

/**
* @breif 等待flash完成当前操作
* @param[in]  handle: 提供SPI的操作句柄
* @retval 无
**/

void bsp_spi_flash_WaitForWriteEnd(spi_device_handle_t handle)
{
	
	// 定义数据发送接收结构体
	spi_transaction_ext_t ext;					//写使能的长度与默认的不同，需要修改
	memset(&ext, 0, sizeof(ext));				//初始化结构体
	ext.command_bits = 8;						//指令位长度为8
	ext.address_bits = 0;						//地址位长度为0
	ext.base.cmd = W25X_ReadStatusReg;			//读取状态寄存器
	ext.base.length = 1 * 8;					//要发送数据的长度，这里不需要发送数据
	ext.base.rx_buffer = NULL;					//不使用外部数据
	ext.base.tx_buffer = NULL;					//不使用外部数据
	ext.base.tx_data[0] = Dummy_Byte;			//发送数据

	ext.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;

	do
	{
		//发送指令
		spi_device_polling_transmit(handle, &ext.base);
	}

	while ( (ext.base.rx_data[0] & WIP_Flag )== WIP_SET);
}

/**
* @breif 扇区擦除
* @param[in]  handle: 提供SPI的操作句柄
* @param[in] SectorAddr: 要擦除的起始扇区地址
* @retval 无
**/

void bsp_spi_flash_SectorErase(spi_device_handle_t handle,uint32_t SectorAddr)
{
	bsp_spi_flash_WriteEnable(handle);
	bsp_spi_flash_WaitForWriteEnd(handle);


	// 定义数据发送接收结构体
	spi_transaction_t t;						//配置位与默认一致，不需要修改
	memset(&t, 0, sizeof(t));					//初始化结构体
	t.cmd = W25X_SectorErase;					//擦除指令
	t.addr = SectorAddr;						//擦除地址
	t.length = 0;								//不需要额外数据了

	//发送指令
	spi_device_polling_transmit(handle, &t);


	//等待擦除完毕
	bsp_spi_flash_WaitForWriteEnd(handle);
}


/**
* @breif 页写入
* @param[in]  handle: 提供SPI的操作句柄
* @param[in]  pBuffer:要写入的数据地址
* @param[in]  WriteAddr:要写入的地址
* @param[in]  NumByteToWrite: 要写入的长度
* @retval 无
**/

void bsp_spi_flash_PageWrite(spi_device_handle_t handle, uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	bsp_spi_flash_WriteEnable(handle);

	// 定义数据发送接收结构体
	spi_transaction_t t;						//配置位与默认一致，不需要修改
	memset(&t, 0, sizeof(t));					//初始化结构体
	t.cmd = W25X_PageProgram;					//页写入
	t.addr = WriteAddr;							//擦除地址
	t.length = 8*NumByteToWrite;				//写入长度
	t.tx_buffer = pBuffer;						//写入的数据
	t.rx_buffer = NULL;							//不需要读取数据

	if (NumByteToWrite > SPI_Flash_PageSize)
	{
		printf("length is too long!\n");
		return ;
	}

	//发送指令
	spi_device_polling_transmit(handle, &t);


	//等待擦除完毕
	bsp_spi_flash_WaitForWriteEnd(handle);



}

/**
* @breif 不定量数据写入
* @param[in]  handle: 提供SPI的操作句柄
* @param[in]  pBuffer:要写入的数据地址
* @param[in]  WriteAddr:要写入的地址
* @param[in]  NumByteToWrite: 要写入的长度
* @retval 无
**/

void bsp_spi_flash_BufferWrite(spi_device_handle_t handle, uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
	uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;

	//进行取余运算，查看是否进行了页对齐
	Addr = WriteAddr % SPI_Flash_PageSize;
	
	//差count个数据值可以进行页对齐
	count = SPI_Flash_PageSize - Addr;

	//计算要写多少个完整的页
	NumOfPage = NumByteToWrite / SPI_Flash_PageSize;

	//计算剩余多少字节不满1页
	NumOfSingle = NumByteToWrite % SPI_Flash_PageSize;

	//如果Addr=0，也就是进行了页对齐
	if (Addr == 0)
	{
		//如果写不满1页
		if (NumOfPage == 0)
		{
			bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, NumByteToWrite);
		}
		
		else
		{
			//如果超过1页，先把满的写了
			while (NumOfPage--)
			{
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, SPI_Flash_PageSize);
				WriteAddr += SPI_Flash_PageSize;
				pBuffer += SPI_Flash_PageSize;
			}
			//不满的1页再写
			bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, NumOfSingle);
		}

	}
	//如果没有进行页对齐
	else
	{

		if (NumOfPage == 0)
		{
			//如果当前页剩下的count个位置比NumOfSingle小，1页写不完
			if (NumOfSingle > count)
			{
				//先把这页剩下的写了
				temp = NumOfSingle - count;
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, count);

				WriteAddr += count;
				pBuffer += count;

				//再把多了的写了
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, temp);
			}
			else
			{
				//如果剩下的空间足够大，就直接写
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, NumByteToWrite);
			}

		}
		//如果不止1页
		else
		{
			//先把对不齐的字节写了
			NumByteToWrite -= count;
			NumOfPage = NumByteToWrite / SPI_Flash_PageSize;
			NumOfSingle = NumByteToWrite % SPI_Flash_PageSize;
			bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr,count);

			//重复地址对齐的情况
			WriteAddr += count;
			pBuffer += count;

			while (NumOfPage--)
			{
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, SPI_Flash_PageSize);
				pBuffer += SPI_Flash_PageSize;
				WriteAddr += SPI_Flash_PageSize;

			}

			if (NumOfSingle != 0)
			{
				bsp_spi_flash_PageWrite(handle, pBuffer, WriteAddr, NumOfSingle);

			}

		}

	}

}



/**
* @breif 数据读取
* @param[in]  handle: 提供SPI的操作句柄
* @param[out]  pBuffer:要读取的数据buffer地址
* @param[in]  WriteAddr:要写入的地址
* @param[in]  NumByteToWrite: 要写入的长度
* @retval 无
**/

void bsp_spi_flash_BufferRead(spi_device_handle_t handle, uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
	bsp_spi_flash_WriteEnable(handle);

	// 定义数据发送接收结构体
	spi_transaction_t t;						//配置位与默认一致，不需要修改
	memset(&t, 0, sizeof(t));					//初始化结构体
	t.cmd = W25X_ReadData;						//读取数据
	t.addr = WriteAddr;							//擦除地址
	t.length = 8 * NumByteToWrite;				//读取长度
	t.tx_buffer = NULL;							//不需要写入数据
	t.rx_buffer = pBuffer;						//读取数据

	

	//发送指令
	spi_device_polling_transmit(handle, &t);


	//等待擦除完毕
	bsp_spi_flash_WaitForWriteEnd(handle);

}

// /******************************************************************
//  * 函 数 名 称：spi_read_write_byte
//  * 函 数 说 明：硬件SPI的读写
//  * 函 数 形 参：dat=发送的数据
//  * 函 数 返 回：读取到的数据
//  * 作       者：LCKFB
//  * 备       注：无
// ******************************************************************/
// uint8_t spi_read_write_byte(uint8_t dat)
// {
// 	//等待发送缓冲区为空
// 	while(RESET == spi_i2s_flag_get(SPI4,  SPI_FLAG_TBE) );
//          spi_i2s_data_transmit(SPI4, dat);
// 	//等待接收缓冲区为空
// 	while(RESET == spi_i2s_flag_get(SPI4,  SPI_FLAG_RBNE) );
//          return spi_i2s_data_receive(SPI4);
// }

// /******************************************************************
//  * 函 数 名 称：W25Q64_readID
//  * 函 数 说 明：读取W25Q64的厂商ID和设备ID
//  * 函 数 形 参：无
//  * 函 数 返 回：设备正常返回EF16
//  * 作       者：LCKFB
//  * 备       注：无
// ******************************************************************/
// uint16_t W25Q64_readID(void)
// {
// 	uint16_t  temp = 0;	  	
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);
		
// 	spi_read_write_byte(0x90);//发送读取ID命令	    
// 	spi_read_write_byte(0x00); 	    
// 	spi_read_write_byte(0x00); 	    
// 	spi_read_write_byte(0x00); 		
// 	//接收数据
// 	temp |= spi_read_write_byte(0xFF)<<8;  
// 	temp |= spi_read_write_byte(0xFF);	
		 
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, SET);	
// 	return temp;
// }



// /**********************************************************
//  * 函 数 名 称：W25Q64_wait_busy
//  * 函 数 功 能：判断W25Q64是否忙
//  * 传 入 参 数：无
//  * 函 数 返 回：无
//  * 作       者：LCKFB
//  * 备       注：无
// **********************************************************/
// void W25Q64_wait_busy(void)   
// {   
//     unsigned char byte = 0;
//     do
//      { 
//         gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);                              
//         spi_read_write_byte(0x05);                 
//         byte = spi_read_write_byte(0Xff);       
//         gpio_bit_write(GPIOF, GPIO_PIN_6, SET);         
//      }while( ( byte & 0x01 ) == 1 );  
// }  

// /**********************************************************
//  * 函 数 名 称：W25Q64_write_enable
//  * 函 数 功 能：发送写使能
//  * 传 入 参 数：无
//  * 函 数 返 回：无
//  * 作       者：LCKFB
//  * 备       注：无
// **********************************************************/
// void W25Q64_write_enable(void)   
// {
//     gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);                           
//     spi_read_write_byte(0x06);                  
//     gpio_bit_write(GPIOF, GPIO_PIN_6, SET); 
// }                            	      

// /**********************************************************
//  * 函 数 名 称：W25Q64_erase_sector
//  * 函 数 功 能：擦除一个扇区
//  * 传 入 参 数：addr=擦除的扇区号
//  * 函 数 返 回：无
//  * 作       者：LCKFB
//  * 备       注：addr=擦除的扇区号，范围=0~15
// **********************************************************/
// void W25Q64_erase_sector(uint32_t addr)   
// {
// 	addr *= 4096;
// 	W25Q64_write_enable();  //写使能   
// 	W25Q64_wait_busy();     //判断忙 
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);                                        
// 	spi_read_write_byte(0x20);        	
// 	spi_read_write_byte((uint8_t)((addr)>>16));      
// 	spi_read_write_byte((uint8_t)((addr)>>8));   
// 	spi_read_write_byte((uint8_t)addr);  
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, SET);                  
// 	//等待擦除完成                           	      	 
// 	W25Q64_wait_busy();   
// }         				                      	      


// /**********************************************************
//  * 函 数 名 称：W25Q64_write
//  * 函 数 功 能：写数据到W25Q64进行保存
//  * 传 入 参 数：buffer=写入的数据内容	addr=写入地址	numbyte=写入数据的长度
//  * 函 数 返 回：无
//  * 作       者：LCKFB
//  * 备       注：无
// **********************************************************/
// void W25Q64_write(uint8_t* buffer, uint32_t addr, uint16_t numbyte)
// {    //0x02e21
//     unsigned int i = 0;
//     W25Q64_erase_sector(addr/4096);//擦除扇区数据
//     W25Q64_write_enable();//写使能    
//     W25Q64_wait_busy(); //忙检测    
//     //写入数据
//     gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);                                        
//     spi_read_write_byte(0x02);                    
//     spi_read_write_byte((uint8_t)((addr)>>16));     
//     spi_read_write_byte((uint8_t)((addr)>>8));   
//     spi_read_write_byte((uint8_t)addr);   
//     for(i=0;i<numbyte;i++)
//     {
//         spi_read_write_byte(buffer[i]);  
//     }
//     gpio_bit_write(GPIOF, GPIO_PIN_6, SET);
//     W25Q64_wait_busy(); //忙检测      
// }

// /**********************************************************
//  * 函 数 名 称：W25Q64_read
//  * 函 数 功 能：读取W25Q64的数据
//  * 传 入 参 数：buffer=读出数据的保存地址  read_addr=读取地址		read_length=读去长度
//  * 函 数 返 回：无
//  * 作       者：LCKFB
//  * 备       注：无
// **********************************************************/
// void W25Q64_read(uint8_t* buffer,uint32_t read_addr,uint16_t read_length)   
// { 
// 	uint16_t i;   		
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, RESET);            
// 	spi_read_write_byte(0x03);                           
// 	spi_read_write_byte((uint8_t)((read_addr)>>16));           
// 	spi_read_write_byte((uint8_t)((read_addr)>>8));   
// 	spi_read_write_byte((uint8_t)read_addr);   
// 	for(i=0;i<read_length;i++)
// 	{ 
// 		buffer[i]= spi_read_write_byte(0XFF);  
// 	}
// 	gpio_bit_write(GPIOF, GPIO_PIN_6, SET);  		    	      
// } 

