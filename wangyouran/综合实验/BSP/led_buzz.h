#ifndef __LED_BUZZ_H__
#define __LED_BUZZ_H__

#include "stm32f4xx.h"

#define LED1_PORT  GPIOB
#define LED1_PIN   GPIO_Pin_5
#define LED2_PORT  GPIOB
#define LED2_PIN   GPIO_Pin_0
#define LED3_PORT  GPIOB
#define LED3_PIN   GPIO_Pin_1
#define BUZZ_PORT  GPIOA
#define BUZZ_PIN   GPIO_Pin_8

#define LED1_ON()   GPIO_ResetBits(LED1_PORT, LED1_PIN)
#define LED1_OFF()  GPIO_SetBits(LED1_PORT, LED1_PIN)
#define LED2_ON()   GPIO_ResetBits(LED2_PORT, LED2_PIN)
#define LED2_OFF()  GPIO_SetBits(LED2_PORT, LED2_PIN)
#define LED3_ON()   GPIO_ResetBits(LED3_PORT, LED3_PIN)
#define LED3_OFF()  GPIO_SetBits(LED3_PORT, LED3_PIN)
#define BUZZ_ON()   GPIO_ResetBits(BUZZ_PORT, BUZZ_PIN)
#define BUZZ_OFF()  GPIO_SetBits(BUZZ_PORT, BUZZ_PIN)

void LED_BUZZ_Init(void);

#endif
