/* 3-1 拓展版 main.c - 流水灯 + KEY2减速控制 */
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
    u16 delay_time;

    /*** 配置按键 ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /* KEY2 — PC13，上拉输入 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /*** 配置三个LED ***/
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
        /* 每轮循环检测 KEY2，动态切换速度（非阻塞）*/
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0)
        {
            delay_ms(20);  /* 消抖 */
            if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0)
            {
                delay_time = 1000;  /* 按住 → 慢速 */
            }
        }
        else
        {
            delay_time = 300;  /* 松开 → 恢复快速 */
        }

        /* LED1亮 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
        delay_ms(delay_time);

        /* LED2亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5);
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
        delay_ms(delay_time);

        /* LED3亮 */
        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0);
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        delay_ms(delay_time);
    }
}
