#ifndef __IR_RECV_H__
#define __IR_RECV_H__

#include "stm32f4xx.h"

void IR_Recv_Init( void );
void IR_Recv( void );
void IR_Rece_Proc(uint16_t addr, uint8_t code);

#endif