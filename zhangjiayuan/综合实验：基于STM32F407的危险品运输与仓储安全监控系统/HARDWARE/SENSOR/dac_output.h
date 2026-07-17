#ifndef __BOARD_DAC_H__
#define __BOARD_DAC_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_DAC_Init(void);
void Board_DAC_SetGasValue(uint16_t gas_value);
void Board_DAC_SetMilliVolt(uint16_t mv);
uint16_t Board_DAC_GetLastGasValue(void);

#endif
