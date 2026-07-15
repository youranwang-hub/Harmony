#ifndef _BSP_IO_H_
#define _BSP_IO_H_

#include "stm32f4xx.h"

/* °´¼ü·µ»ØÖµ */
#define KEY_NONE    0
#define KEY1_VALUE  1
#define KEY2_VALUE  2
#define KEY3_VALUE  3
#define KEY4_VALUE  4

void BSP_IO_Init(void);
u8 BSP_KeyScan(void);
u8 BSP_AnyKeyPressed(void);

void LED1_On(void);
void LED1_Off(void);
void LED1_Toggle(void);

void BEEP_On(void);
void BEEP_Off(void);

#endif
