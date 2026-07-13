#include "stm32f4xx.h"

/* 定义四个变量，用于存储四个按键的读取值 */
u8 KeyValue_1, KeyValue_2, KeyValue_3, KeyValue_4;

/* 定义一个GPIO初始化用的结构体 */
GPIO_InitTypeDef GPIO_InitStructure;

/* 函数声明 */
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

int main(void)
{
    /** 配置四个按键KEY1—KEY4所在的GPIO **/

    /* 开启GPIOA、GPIOC的时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC,
        ENABLE
    );

    /* 配置KEY1所在的GPIO：PA0 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    // 要初始化的引脚号PA0

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    // GPIO设置为输入模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    // 速度设置为2MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    // 设置为上拉输入

    /* 初始化GPIOA */
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    /* 配置KEY2、KEY3、KEY4所在的GPIO：PC13、PC6、PC7 */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_13 | GPIO_Pin_6 | GPIO_Pin_7;
    // 要初始化的引脚号

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    // GPIO设置为输入模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    // 速度设置为2MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    // 设置为上拉输入

    /* 初始化GPIOC */
    GPIO_Init(GPIOC, &GPIO_InitStructure);


    /** 配置三个LED所在的GPIO **/

    /* 开启GPIOB的时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    /* 配置LED1、LED2、LED3所在的GPIO */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    // LED1：PB5，LED2：PB0，LED3：PB1

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    // GPIO设置为输出模式

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    // GPIO设置为推挽输出模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    // 速度设置为25MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    // 不使用上拉、下拉电阻

    /* 预先输出高电平，确保三个LED初始状态为熄灭 */
    GPIO_SetBits(
        GPIOB,
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
    );

    /* 初始化GPIOB */
    GPIO_Init(GPIOB, &GPIO_InitStructure);


    while (1)
    {
        /* 第一次读取四个按键的GPIO电平 */

        KeyValue_1 =
            GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
        // 读取KEY1

        KeyValue_2 =
            GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
        // 读取KEY2

        KeyValue_3 =
            GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6);
        // 读取KEY3

        KeyValue_4 =
            GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7);
        // 读取KEY4


        /* 判断是否有按键按下，低电平表示按下 */
        if ((0 == KeyValue_1) ||
            (0 == KeyValue_2) ||
            (0 == KeyValue_3) ||
            (0 == KeyValue_4))
        {
            /* 延时20毫秒，消除按键按下时的抖动 */
            delay_ms(20);

            /* 消抖后再次读取四个按键的电平 */
            KeyValue_1 =
                GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);

            KeyValue_2 =
                GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);

            KeyValue_3 =
                GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6);

            KeyValue_4 =
                GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7);


            /* 判断真正按下的是哪个按键 */
            if (0 == KeyValue_1)
            {
                /*
                 * KEY1已经确认按下。
                 * 按键没有抬起时，PA0一直为低电平，
                 * 程序停留在循环中等待KEY1抬起。
                 */
                while (
                    0 == GPIO_ReadInputDataBit(
                        GPIOA,
                        GPIO_Pin_0
                    )
                    )
                {
                    /* 等待KEY1抬起 */
                }

                /* 延时20毫秒，消除按键抬起时的抖动 */
                delay_ms(20);

                /* KEY1抬起后点亮LED1 */
                GPIO_ResetBits(GPIOB, GPIO_Pin_5);
            }
            else if (0 == KeyValue_2)
            {
                /*
                 * 等待KEY2抬起。
                 * KEY2按下时PC13为低电平。
                 */
                while (
                    0 == GPIO_ReadInputDataBit(
                        GPIOC,
                        GPIO_Pin_13
                    )
                    )
                {
                    /* 等待KEY2抬起 */
                }

                /* 消除按键抬起时的抖动 */
                delay_ms(20);

                /* KEY2抬起后点亮LED2 */
                GPIO_ResetBits(GPIOB, GPIO_Pin_0);
            }
            else if (0 == KeyValue_3)
            {
                /*
                 * 等待KEY3抬起。
                 * KEY3按下时PC6为低电平。
                 */
                while (
                    0 == GPIO_ReadInputDataBit(
                        GPIOC,
                        GPIO_Pin_6
                    )
                    )
                {
                    /* 等待KEY3抬起 */
                }

                /* 消除按键抬起时的抖动 */
                delay_ms(20);

                /* KEY3抬起后点亮LED3 */
                GPIO_ResetBits(GPIOB, GPIO_Pin_1);
            }
            else if (0 == KeyValue_4)
            {
                /*
                 * 等待KEY4抬起。
                 * KEY4按下时PC7为低电平。
                 */
                while (
                    0 == GPIO_ReadInputDataBit(
                        GPIOC,
                        GPIO_Pin_7
                    )
                    )
                {
                    /* 等待KEY4抬起 */
                }

                /* 消除按键抬起时的抖动 */
                delay_ms(20);

                /* KEY4抬起后熄灭三个LED */
                GPIO_SetBits(GPIOB, GPIO_Pin_5);
                GPIO_SetBits(GPIOB, GPIO_Pin_0);
                GPIO_SetBits(GPIOB, GPIO_Pin_1);
            }
        }
    }
}


/***** 简单延时函数，ms、us *****/

void delay_us(uint16_t nus)
{
    uint16_t i;

    while (nus--)
    {
        i = 31;

        while (i--)
        {
        }
    }
}

void delay_ms(u16 nms)
{
    uint16_t i;

    while (nms--)
    {
        i = 33800;

        while (i--)
        {
        }
    }
}
