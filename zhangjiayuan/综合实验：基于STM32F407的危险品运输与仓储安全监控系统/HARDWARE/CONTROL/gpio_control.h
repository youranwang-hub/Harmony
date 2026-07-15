#ifndef __BOARD_GPIO_H__
#define __BOARD_GPIO_H__

#include "stm32f4xx.h"
#include <stdint.h>

typedef enum {
    BOARD_LED_GREEN = 0,
    BOARD_LED_RED,
    BOARD_LED_STATUS
} BoardLed;

void Board_GPIO_Init(void);
void Board_LED_Set(BoardLed led, uint8_t on);
void Board_LED_Toggle(BoardLed led);
void Board_Buzzer_Set(uint8_t on);
uint8_t Board_Key_Read(uint8_t key_id);

#endif
