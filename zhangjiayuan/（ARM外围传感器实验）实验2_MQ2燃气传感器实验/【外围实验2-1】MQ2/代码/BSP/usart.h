#ifndef __USART_H__
#define __USART_H__

#include "stm32f4xx.h"

/**
  * @brief  初始化USART2
  * @param  bound：串口波特率
  * @retval 无
  */
void UART2_Init(uint32_t bound);

/**
  * @brief  发送一个字节
  * @param  pUSARTx：串口外设
  * @param  ch：待发送字节
  * @retval 无
  */
void UART_SendByte(
    USART_TypeDef* pUSARTx,
    uint8_t ch
);

/**
  * @brief  发送字符串
  * @param  pUSARTx：串口外设
  * @param  str：字符串首地址
  * @retval 无
  */
void UART_SendString(
    USART_TypeDef* pUSARTx,
    char* str
);

/**
  * @brief  发送一个16位数据
  * @param  pUSARTx：串口外设
  * @param  ch：16位数据
  * @retval 无
  */
void UART_SendHalfWord(
    USART_TypeDef* pUSARTx,
    uint16_t ch
);

#endif /* __USART_H__ */
