#ifndef __USART_H__
#define __USART_H__

#include "stm32f4xx.h"
#include <stdint.h>

void UART2_Init(u32 bound);
void UART_SendByte(USART_TypeDef* pUSARTx, uint8_t ch);
void UART_SendString(USART_TypeDef* pUSARTx, const char* str);
void UART_SendHalfWord(USART_TypeDef* pUSARTx, uint16_t ch);
uint8_t UART2_ReadLine(char *out, uint16_t out_len);

#endif
