/* 2-2 拓展版 main.c - 向左流水灯 + LED1亮时蜂鸣器发声 */
#include "stm32f4xx.h"

GPIO_InitTypeDef GPIO_InitStructure;

void Delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < 10000; j++);
    }
}

int main(void)
{
    /* 开启 GPIOB 时钟 — LED */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 开启 GPIOA 时钟 — 蜂鸣器 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    while (1)
    {
        /* 向左流水灯: LED3→LED2→LED1 */

        /* LED3亮, 其余灭 + 蜂鸣器不响（不是LED1）*/
        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);

        /* LED2亮, 其余灭 + 蜂鸣器不响（不是LED1）*/
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);

        /* LED1亮, 其余灭 + 蜂鸣器发声 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);
    }
}
