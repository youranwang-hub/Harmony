#ifndef __BOARD_RTC_H__
#define __BOARD_RTC_H__

#include "stm32f4xx.h"
#include <stdint.h>

void Board_RTC_Init(void);
uint32_t Board_RTC_GetUnixLike(void);
void Board_RTC_Format(char *out, uint16_t out_len);

#endif
