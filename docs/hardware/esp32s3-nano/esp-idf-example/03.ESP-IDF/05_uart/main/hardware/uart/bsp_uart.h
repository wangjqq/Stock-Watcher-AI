#ifndef _BSP_UART_H_
#define _BSP_UART_H_

//导入串口设备库
#include "driver/uart.h"
//导入GPIO设备库
#include "driver/gpio.h"
//导入 strlen 函数支持
#include <string.h>

/**
 * @函数说明        uart_init_config
 * @传入参数        uart_port = 串口号 
 *                      可选参数：UART_NUM_0、UART_NUM_1、UART_NUM_2
 *                 baud_rate = 串口波特率
 *                 tx_pin = 发送引脚
 *                 rx_pin = 接收引脚     
 * @函数返回        无
 */
void uart_init_config(uart_port_t uart_port, uint32_t baud_rate, int tx_pin, int rx_pin);

/**
 * @函数说明        串口2接收任务
 * @传入参数        无
 * @函数返回        无
 * @函数说明        请通过xTaskCreate创建该任务
 */
void uart2_rx_task(void);
#endif
