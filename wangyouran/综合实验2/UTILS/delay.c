#include "delay.h"

void delay_us(uint16_t nus)
{
	uint32_t temp;
	SysTick->LOAD = nus * 168;               // 168MHz: 1us = 168 cycles
	SysTick->VAL  = 0;
	SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
	do {
		temp = SysTick->CTRL;
	} while((temp & 0x01) && !(temp & (1 << 16)));
	SysTick->CTRL = 0;
}

void delay_ms(u16 nms)
{
	SysTick->LOAD = 168000;
	SysTick->VAL  = 0;
	SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
	while(nms--) {
		while(!(SysTick->CTRL & (1 << 16))){}
	}
	SysTick->CTRL = 0;
}
