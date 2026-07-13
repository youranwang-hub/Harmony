#include "stm32f4xx.h"

/* 定义一个GPIO初始化用的结构体 */
GPIO_InitTypeDef GPIO_InitStructure;

/* 定义一个定时器初始化结构体 */
TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;

/* 定义一个NVIC初始化结构体 */
NVIC_InitTypeDef NVIC_InitStructure;

int main(void)
{
    /* 初始化LED1的GPIO，用于闪烁，LED1连接PB5 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    /* 使能LED1的GPIOB时钟 */

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    /* 选择LED1对应的PB5引脚 */

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    /* 配置PB5为普通输出模式 */

    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    /* 配置PB5为推挽输出模式 */

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    /* 配置GPIO输出速度为2MHz */

    GPIO_SetBits(GPIOB, GPIO_Pin_5);
    /* 预先设置PB5输出高电平，熄灭LED1 */

    GPIO_Init(GPIOB, &GPIO_InitStructure);
    /* 根据GPIO_InitStructure中的参数初始化PB5 */


    /* 初始化定时器3 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    /* 使能定时器TIM3的外设时钟 */

    TIM_TimeBaseInitStructure.TIM_Period = 4999;
    /* 设置定时器自动重装载值 */

    TIM_TimeBaseInitStructure.TIM_Prescaler = 8400;
    /* 设置定时器预分频值 */

    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    /* 设置TIM3为向上计数模式 */

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
    /* 根据设置的参数初始化TIM3 */


    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    /* 清除一次TIM3的更新标志，避免刚启动时立即进入中断 */

    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    /* 开启TIM3的更新中断，也就是溢出中断 */


    /* 配置TIM3的NVIC中断 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    /* 设置中断通道为TIM3中断 */

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    /* 设置抢占优先级为1 */

    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;
    /* 设置子优先级为3 */

    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    /* 使能TIM3中断通道 */

    NVIC_Init(&NVIC_InitStructure);
    /* 根据NVIC_InitStructure中的参数初始化NVIC */


    TIM_Cmd(TIM3, ENABLE);
    /* 使能TIM3，定时器开始计数 */


    while (1)
    {
        /* 主循环中没有内容 */
    }
}


/* 定时器3中断服务函数 */
void TIM3_IRQHandler(void)
{
    static u8 i;
    /* 定义静态变量i，每次退出中断函数后不会被清零 */

    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET)
    {
        /* 判断是否产生了TIM3更新中断 */

        if (i)
        {
            i = 0;

            GPIO_ResetBits(GPIOB, GPIO_Pin_5);
            /* PB5输出低电平，点亮LED1 */
        }
        else
        {
            i = 1;

            GPIO_SetBits(GPIOB, GPIO_Pin_5);
            /* PB5输出高电平，熄灭LED1 */
        }
    }

    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    /* 清除TIM3更新中断标志位 */
}
