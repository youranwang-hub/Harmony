#ifndef __BOARD_TIMER_H__
#define __BOARD_TIMER_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_Timer_Init(void);
void Board_PWM_Init(void);
void Board_PWM_SetFanPercent(uint8_t percent);

#endif
