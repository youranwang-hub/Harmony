#ifndef __BOARD_CAN_H__
#define __BOARD_CAN_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_CAN_Init(void);
void Board_CAN_SendHeartbeat(uint8_t state, uint16_t gas_value);
void Board_CAN_SendAlarm(uint8_t alarm_level, uint16_t gas_value);
void Board_CAN_IRQHandler(void);
uint8_t Board_CAN_IsOnline(void);
void Board_CAN_SetVirtualOnline(uint8_t online);
const char *Board_CAN_StatusName(void);

#endif
