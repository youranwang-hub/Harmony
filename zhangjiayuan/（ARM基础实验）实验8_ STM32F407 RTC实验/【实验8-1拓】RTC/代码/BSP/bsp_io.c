#include "bsp_io.h"
#include "delay.h"

/*
 * KEY1：PA0
 * KEY2：PC13
 * KEY3：PC6
 * KEY4：PC7
 *
 * LED1：PB5，低电平点亮
 * BEEP：PA8，低电平响
 */

void BSP_IO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 开启GPIOA、GPIOB、GPIOC时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA |
        RCC_AHB1Periph_GPIOB |
        RCC_AHB1Periph_GPIOC,
        ENABLE
    );

    /* PA8配置为蜂鸣器输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB5配置为LED1输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 默认关闭蜂鸣器和LED */
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    GPIO_SetBits(GPIOB, GPIO_Pin_5);

    /* PA0配置为按键输入，上拉 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PC13、PC6、PC7配置为按键输入，上拉 */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_13 |
        GPIO_Pin_6 |
        GPIO_Pin_7;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void LED1_On(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_5);
}

void LED1_Off(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
}

void LED1_Toggle(void)
{
    GPIO_ToggleBits(GPIOB, GPIO_Pin_5);
}

void BEEP_On(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
}

void BEEP_Off(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

/* 判断是否有任意按键按下 */
u8 BSP_AnyKeyPressed(void)
{
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET)
    {
        return 1;
    }

    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET)
    {
        return 1;
    }

    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6) == Bit_RESET)
    {
        return 1;
    }

    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7) == Bit_RESET)
    {
        return 1;
    }

    return 0;
}

/* 带消抖和松手检测的按键扫描 */
u8 BSP_KeyScan(void)
{
    u8 key = KEY_NONE;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET)
    {
        delay_ms(20);

        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_RESET)
        {
            key = KEY1_VALUE;
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET)
    {
        delay_ms(20);

        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET)
        {
            key = KEY2_VALUE;
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6) == Bit_RESET)
    {
        delay_ms(20);

        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6) == Bit_RESET)
        {
            key = KEY3_VALUE;
        }
    }
    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7) == Bit_RESET)
    {
        delay_ms(20);

        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7) == Bit_RESET)
        {
            key = KEY4_VALUE;
        }
    }

    if (key != KEY_NONE)
    {
        while (BSP_AnyKeyPressed())
        {
            /* 等待按键松开 */
        }

        delay_ms(20);
    }

    return key;
}
