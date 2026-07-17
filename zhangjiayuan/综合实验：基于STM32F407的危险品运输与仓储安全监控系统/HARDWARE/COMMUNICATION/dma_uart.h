#ifndef __BOARD_DMA_H__
#define __BOARD_DMA_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_DMA_USART2_Init(void);
uint8_t Board_DMA_USART2_Send(const uint8_t *data, uint16_t len);
uint8_t Board_DMA_USART2_IsBusy(void);
void Board_DMA_USART2_IRQHandler(void);

#endif
