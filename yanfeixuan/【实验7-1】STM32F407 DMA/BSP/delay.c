#include "delay.h"

void delay_us(uint16_t nus)
{
    volatile uint16_t i;
    while(nus--)
    {
        i = 31; 
        while(i--);
    }
}

void delay_ms(uint32_t nms)
{
    volatile uint16_t i;
    while(nms--)
    {
        i = 33800;
        while(i--);
    }
}