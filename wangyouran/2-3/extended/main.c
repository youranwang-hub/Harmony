/* 2-3 拓展版 main.c - 按键抬起检测：按下后等待抬起才响应 */
#include "stm32f4xx.h"

u8 KeyValue_1, KeyValue_2, KeyValue_3, KeyValue_4;
GPIO_InitTypeDef GPIO_InitStructure;

/* 函数声明 */
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

/***** 简单延时函数 *****/
void delay_us(uint16_t nus)
{
    uint16_t i;
    while (nus--)
    {
        i = 31;
        while (i--);
    }
}

void delay_ms(u16 nms)
{
    uint16_t i;
    while (nms--)
    {
        i = 33800;
        while (i--);
    }
}

/* 等待按键抬起函数 */
void Wait_KeyRelease(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
    while (GPIO_ReadInputDataBit(GPIOx, GPIO_Pin) == 0)
    {
        /* 原地等待，直到按键松开（引脚恢复高电平）*/
    }
}

int main(void)
{
    /*** 配置四个按键 ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);

    /* KEY1 — PA0 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* KEY2, KEY3, KEY4 — PC13, PC6, PC7 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_6 | GPIO_Pin_7;
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
        /* 读取四个按键的值 */
        KeyValue_1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
        KeyValue_2 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
        KeyValue_3 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6);
        KeyValue_4 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7);

        /* 检测是否有按键按下 */
        if ((0 == KeyValue_1) || (0 == KeyValue_2) || (0 == KeyValue_3) || (0 == KeyValue_4))
        {
            delay_ms(20);  /* 消抖 */
            KeyValue_1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
            KeyValue_2 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
            KeyValue_3 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6);
            KeyValue_4 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7);

            if (0 == KeyValue_1)  /* KEY1 按下 */
            {
                Wait_KeyRelease(GPIOA, GPIO_Pin_0);  /* 等待抬起 */
                /* 抬起后才翻转 LED1 */
                GPIO_ToggleBits(GPIOB, GPIO_Pin_5);
            }
            else if (0 == KeyValue_2)  /* KEY2 按下 */
            {
                Wait_KeyRelease(GPIOC, GPIO_Pin_13);  /* 等待抬起 */
                /* 抬起后才翻转 LED2 */
                GPIO_ToggleBits(GPIOB, GPIO_Pin_0);
            }
            else if (0 == KeyValue_3)  /* KEY3 按下 */
            {
                Wait_KeyRelease(GPIOC, GPIO_Pin_6);  /* 等待抬起 */
                /* 抬起后才翻转 LED3 */
                GPIO_ToggleBits(GPIOB, GPIO_Pin_1);
            }
            else if (0 == KeyValue_4)  /* KEY4 按下 */
            {
                Wait_KeyRelease(GPIOC, GPIO_Pin_7);  /* 等待抬起 */
                /* 抬起后熄灭全部 LED */
                GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
            }
        }
    }
}
