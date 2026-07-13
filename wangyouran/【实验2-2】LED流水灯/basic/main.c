/* 2-2 基础版 main.c - 向右流水灯 */
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
    /* 开启 GPIOB 时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /* 配置 LED（PB5、PB0、PB1）*/
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    while (1)
    {
        /* 向右流水灯: LED1→LED2→LED3 */

        /* LED1亮, 其余灭 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
        Delay_ms(300);

        /* LED2亮, 其余灭 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        Delay_ms(300);

        /* LED3亮, 其余灭 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        Delay_ms(300);
    }
}
