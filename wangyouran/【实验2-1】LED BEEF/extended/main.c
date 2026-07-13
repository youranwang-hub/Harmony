/* 2-1 拓展版 main.c - LED与蜂鸣器多种亮灭组合控制 */
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
    /* 开启 GPIOB 的时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 配置三个LED所在的GPIO（PB5、PB0、PB1）*/
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    /* 预先把输出置高--LED熄灭 */
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 开启 GPIOA 的时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    /* 配置蜂鸣器所在的GPIO（PA8）*/
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    /* 预先把输出置高--蜂鸣器不响 */
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    while (1)
    {
        /* 组合1: 三个LED全亮 + 蜂鸣器响 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(500);

        /* 组合2: LED1亮, LED2灭, LED3亮 + 蜂鸣器灭 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(500);

        /* 组合3: LED1灭, LED2亮, LED3灭 + 蜂鸣器响 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(500);

        /* 组合4: 流水灯 + 蜂鸣器交替响 */
        /* LED1亮 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);

        /* LED2亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);

        /* LED3亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(300);

        /* 组合5: 三个LED全灭 + 蜂鸣器长响 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        Delay_ms(800);
    }
}
