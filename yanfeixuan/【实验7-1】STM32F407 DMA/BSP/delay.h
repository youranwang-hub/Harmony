#ifndef __DELAY_H
#define __DELAY_H
#include "stm32f4xx.h"

//函数声明，让其他c文件可以调用这两个延时
void delay_us(uint16_t nus);
void delay_ms(uint32_t nms);

#endif