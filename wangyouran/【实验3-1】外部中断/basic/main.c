/* 3-1 基础版 main.c - 基础流水灯 */
#include "stm32f4xx.h"

GPIO_InitTypeDef GPIO_InitStructure;

void delay_ms(u16 nms)
{
    uint16_t i;
    while (nms--)
    {
        i = 33800;
        while (i--);
    }
}

int main(void)
{
    /* 配置三个LED */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    while (1)
    {
        /* LED1亮 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
        delay_ms(300);

        /* LED2亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        delay_ms(300);

        /* LED3亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        delay_ms(300);
    }
}
