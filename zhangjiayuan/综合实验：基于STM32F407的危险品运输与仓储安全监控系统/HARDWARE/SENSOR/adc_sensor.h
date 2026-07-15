#ifndef __BOARD_ADC_H__
#define __BOARD_ADC_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_ADC_Init(void);
uint16_t Board_ADC_ReadRaw(uint8_t channel);
uint16_t Board_ADC_ReadGasReal(void);
uint16_t Board_ADC_ReadPot(void);
uint16_t Board_ADC_ReadDacLoop(void);
uint16_t Board_ADC_ReadBatteryMv(void);
uint16_t Board_ADC_RawToGas(uint16_t raw);

#endif
