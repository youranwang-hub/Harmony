#include "stm32f4xx.h"

/* 定义GPIO、EXTI、NVIC初始化用到的结构体变量 */
GPIO_InitTypeDef GPIO_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;
EXTI_InitTypeDef EXTI_InitStructure;

/*
 * 定义时间变量，用于控制LED流水灯速度。
 * volatile表示该变量可能在中断服务函数中被修改。
 */
volatile u16 LedTime = 500;

/* 函数声明 */
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

int main(void)
{
    /*******************************************************
     * 一、初始化三个LED所在的GPIO
     * LED1：PB5
     * LED2：PB0
     * LED3：PB1
     *******************************************************/

    /* 开启GPIOB的时钟，才能使用GPIOB */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    /* 配置三个LED所在的GPIO：PB5、PB0、PB1 */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    // 选择PB5、PB0、PB1

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    // 设置为输出模式

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    // 设置为推挽输出模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    // GPIO速度设置为25MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    // 不使用上拉、下拉电阻

    /* 使用上述配置初始化GPIOB */
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 预先输出高电平，使三个LED初始状态为熄灭 */
    GPIO_SetBits(
        GPIOB,
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
    );


    /*******************************************************
     * 二、初始化KEY1所在的GPIO
     * KEY1：PA0
     *******************************************************/

    /* 开启GPIOA的时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    /* 开启SYSCFG的时钟 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_SYSCFG,
        ENABLE
    );

    /* 配置KEY1所在的GPIO：PA0 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    // 选择PA0

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    // 设置为输入模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    // GPIO速度设置为25MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    // 设置为上拉输入

    /* 使用上述配置初始化GPIOA */
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    /*******************************************************
     * 三、配置外部中断EXTI
     *******************************************************/

    /*
     * 将GPIOA的PA0连接到外部中断线EXTI_Line0
     */
    SYSCFG_EXTILineConfig(
        EXTI_PortSourceGPIOA,
        EXTI_PinSource0
    );

    /* 配置EXTI_Line0 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    // 选择外部中断线0

    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    // 设置为中断模式

    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    // 设置为下降沿触发

    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    // 使能外部中断线0

    /* 使用上述配置初始化EXTI */
    EXTI_Init(&EXTI_InitStructure);


    /*******************************************************
     * 四、配置嵌套向量中断控制器NVIC
     *******************************************************/

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    // 选择外部中断线0对应的中断向量

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
    // 抢占优先级设置为0

    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;
    // 响应优先级设置为2

    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    // 使能EXTI0中断通道

    /* 使用上述配置初始化NVIC */
    NVIC_Init(&NVIC_InitStructure);


    /*******************************************************
     * 五、主循环：LED流水灯
     * 初始LedTime为500 ms。
     * KEY1触发中断后，LedTime变为100 ms。
     *******************************************************/

    while (1)
    {
        /* 点亮LED1，熄灭LED2和LED3 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_0 | GPIO_Pin_1
        );

        /* 根据LedTime变量进行可变延时 */
        delay_ms(LedTime);


        /* 点亮LED2，熄灭LED1和LED3 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_5 | GPIO_Pin_1
        );

        /* 根据LedTime变量进行可变延时 */
        delay_ms(LedTime);


        /* 点亮LED3，熄灭LED1和LED2 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_5 | GPIO_Pin_0
        );

        /* 根据LedTime变量进行可变延时 */
        delay_ms(LedTime);
    }
}


/**********************************************************
 * 外部中断0服务函数
 * KEY1按下时，PA0出现下降沿并触发该函数
 **********************************************************/
void EXTI0_IRQHandler(void)
{
    /* 判断EXTI_Line0是否产生了中断 */
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        /* 清除EXTI_Line0上的中断标志位 */
        EXTI_ClearITPendingBit(EXTI_Line0);

        /* 将流水灯延时时间修改为100 ms */
        LedTime = 100;
    }
}


/***** 简单微秒延时函数 *****/
void delay_us(uint16_t nus)
{
    uint16_t i;

    while (nus--)
    {
        i = 31;

        while (i--);
    }
}


/***** 简单毫秒延时函数 *****/
void delay_ms(u16 nms)
{
    uint16_t i;

    while (nms--)
    {
        i = 33800;

        while (i--);
    }
}

