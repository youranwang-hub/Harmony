#include "stm32f4xx.h"

/* 定义GPIO、EXTI、NVIC初始化用到的结构体变量 */
GPIO_InitTypeDef GPIO_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;
EXTI_InitTypeDef EXTI_InitStructure;

/* 定义一个时间变量用于控制LED流水速度 */
u16 LedTime = 500;

/* 函数声明 */
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

int main(void)
{
    /********************************************************
     * 一、初始化三个LED
     * LED1：PB5
     * LED2：PB0
     * LED3：PB1
     ********************************************************/

     /* 开启GPIOB的时钟，才能使用GPIOB */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    /* 配置三个LED所在的GPIO：PB5、PB0、PB1 */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    // GPIO设置为输出模式

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    // GPIO设置为推挽输出模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    // GPIO速度设置为25MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    // 不使用上拉、下拉电阻

    /* 使用上面的配置初始化GPIOB */
    GPIO_Init(GPIOB, &GPIO_InitStructure);


    /********************************************************
     * 二、开启KEY1、KEY2和SYSCFG所需要的时钟
     *
     * KEY1：PA0
     * KEY2：PC13
     ********************************************************/

     /* 开启GPIOA和GPIOC的时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA |
        RCC_AHB1Periph_GPIOC,
        ENABLE
    );

    /* 开启SYSCFG的时钟 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_SYSCFG,
        ENABLE
    );


    /********************************************************
     * 三、初始化KEY1：PA0
     ********************************************************/

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


    /********************************************************
     * 四、初始化KEY2：PC13
     ********************************************************/

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    // 选择PC13

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    // 设置为输入模式

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    // GPIO速度设置为25MHz

    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    // 设置为上拉输入

    /* 使用上述配置初始化GPIOC */
    GPIO_Init(GPIOC, &GPIO_InitStructure);


    /********************************************************
     * 五、配置KEY1对应的外部中断线EXTI_Line0
     ********************************************************/

     /* 将PA0连接到外部中断线0 */
    SYSCFG_EXTILineConfig(
        EXTI_PortSourceGPIOA,
        EXTI_PinSource0
    );

    /* 配置EXTI_Line0 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    // 选择中断线0

    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    // 设置为中断模式

    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    // 设置为下降沿触发

    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    // 使能中断线0

    /* 初始化EXTI_Line0 */
    EXTI_Init(&EXTI_InitStructure);


    /********************************************************
     * 六、配置KEY2对应的外部中断线EXTI_Line13
     ********************************************************/

     /* 将PC13连接到外部中断线13 */
    SYSCFG_EXTILineConfig(
        EXTI_PortSourceGPIOC,
        EXTI_PinSource13
    );

    /* 配置EXTI_Line13 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line13;
    // 选择中断线13

    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    // 设置为中断模式

    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    // 设置为下降沿触发

    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    // 使能中断线13

    /* 初始化EXTI_Line13 */
    EXTI_Init(&EXTI_InitStructure);


    /********************************************************
     * 七、配置KEY1的NVIC
     * EXTI_Line0使用EXTI0_IRQn中断向量
     ********************************************************/

    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    // 设置KEY1对应的中断向量

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
    // 抢占优先级设置为0

    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;
    // 响应优先级设置为2

    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    // 使能中断通道

    /* 初始化KEY1的NVIC */
    NVIC_Init(&NVIC_InitStructure);


    /********************************************************
     * 八、配置KEY2的NVIC
     *
     * EXTI_Line10至EXTI_Line15共用
     * EXTI15_10_IRQn中断向量
     ********************************************************/

    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    // 设置KEY2对应的中断向量

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
    // 抢占优先级设置为0

    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;
    // 响应优先级设置为3

    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    // 使能中断通道

    /* 初始化KEY2的NVIC */
    NVIC_Init(&NVIC_InitStructure);


    /********************************************************
     * 九、主循环：三个LED流水灯
     *
     * 初始LedTime为500毫秒
     * 按下KEY1后变为100毫秒
     * 按下KEY2后变为1000毫秒
     ********************************************************/

    while (1)
    {
        /* 点亮LED1，熄灭LED2和LED3 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);

        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_0 | GPIO_Pin_1
        );

        /* 根据LedTime变量延时 */
        delay_ms(LedTime);


        /* 点亮LED2，熄灭LED1和LED3 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);

        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_5 | GPIO_Pin_1
        );

        /* 根据LedTime变量延时 */
        delay_ms(LedTime);


        /* 点亮LED3，熄灭LED1和LED2 */
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);

        GPIO_SetBits(
            GPIOB,
            GPIO_Pin_5 | GPIO_Pin_0
        );

        /* 根据LedTime变量延时 */
        delay_ms(LedTime);
    }
}


/**********************************************************
 * KEY1外部中断服务函数
 *
 * PA0对应EXTI_Line0。
 * 按下KEY1后，把流水灯间隔改为100毫秒。
 **********************************************************/
void EXTI0_IRQHandler(void)
{
    /* 清除EXTI_Line0上的中断标志位 */
    EXTI_ClearITPendingBit(EXTI_Line0);

    /* 加快流水速度 */
    LedTime = 100;
}


/**********************************************************
 * KEY2外部中断服务函数
 *
 * PC13对应EXTI_Line13。
 * EXTI_Line10至EXTI_Line15共用该中断服务函数。
 * 按下KEY2后，把流水灯间隔改为1000毫秒。
 **********************************************************/
void EXTI15_10_IRQHandler(void)
{
    /* 清除EXTI_Line13上的中断标志位 */
    EXTI_ClearITPendingBit(EXTI_Line13);

    /* 减慢流水速度 */
    LedTime = 1000;
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