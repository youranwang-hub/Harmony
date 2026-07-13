/* 5-1 拓展版 main.c - TIM3定时中断 + 多LED多频率闪烁 */
/* 通过不同分频系数演示时钟频率改变对定时器的影响 */
#include "stm32f4xx.h"

/* ================================================================
 * STM32F407 时钟树说明
 * ================================================================
 *
 *   HSE(8MHz)
 *     │
 *     ↓ ÷ PLL_M(8)
 *   1MHz
 *     │
 *     ↓ × PLL_N(336)
 *  336MHz (VCO)
 *     │
 *     ↓ ÷ PLL_P(2)
 *  168MHz ──────→ SYSCLK (系统时钟)
 *     │
 *     ├──→ AHB ÷ 1  → 168MHz
 *     │     ├──→ APB1 ÷ 4 → 42MHz
 *     │     │    └──→ TIM3 × 2 = 84MHz  ← 定时器时钟
 *     │     └──→ APB2 ÷ 2 → 84MHz
 *     │
 *     └──→ 其他外设
 *
 * TIM3 当前时序:
 *   84MHz ÷ 8400(预分频) = 10kHz
 *   10kHz ÷ 5000(Period=4999) = 2Hz 中断
 *   LED1 每中断翻转一次 → 1Hz 闪烁
 *
 * ★ 修改 system_stm32f4xx.c 中的 PLL_N 会改变 SYSCLK:
 *   PLL_N=336 → SYSCLK=168MHz → LED1 1Hz
 *   PLL_N=168 → SYSCLK= 84MHz → LED1 0.5Hz (变慢一半)
 *   PLL_N=672 → SYSCLK=336MHz → 可能超频, 不建议
 *
 * ★ PLL 参数位置: system_stm32f4xx.c 文件开头
 *   #define PLL_M  8
 *   #define PLL_N  336   ← 改这里
 *   #define PLL_P  2
 * ================================================================ */

GPIO_InitTypeDef GPIO_InitStructure;
TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
NVIC_InitTypeDef NVIC_InitStructure;

/* 定时器3中断服务函数 */
void TIM3_IRQHandler(void)
{
    static u16 tick = 0;  /* 软件计数器，每次中断 +1 */
    
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET)
    {
        tick++;
        
        /* LED1(PB5): 每 1 次中断翻转 → 1Hz */
        if (tick % 1 == 0)
            GPIO_ToggleBits(GPIOB, GPIO_Pin_5);
        
        /* LED2(PB0): 每 2 次中断翻转 → 0.5Hz (慢一倍) */
        if (tick % 2 == 0)
            GPIO_ToggleBits(GPIOB, GPIO_Pin_0);
        
        /* LED3(PB1): 每 4 次中断翻转 → 0.25Hz (慢四倍) */
        if (tick % 4 == 0)
            GPIO_ToggleBits(GPIOB, GPIO_Pin_1);
        
        /* 蜂鸣器(PA8): tick=0 时短鸣一声，用于标记周期起点 */
        if (tick == 0)
        {
            GPIO_ResetBits(GPIOA, GPIO_Pin_8);   /* 响 */
        }
        else if (tick == 1)
        {
            GPIO_SetBits(GPIOA, GPIO_Pin_8);     /* 静音 */
        }
        
        /* tick 每 30 次循环归零 (约15秒一个周期，蜂鸣器短鸣标记) */
        if (tick >= 30)
            tick = 0;
    }
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
}


int main(void)
{
    /*** 配置三个LED (PB5, PB0, PB1) ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1); /* 初始全灭 */
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /*** 配置蜂鸣器 (PA8) ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_SetBits(GPIOA, GPIO_Pin_8); /* 初始静音 */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*** 配置定时器TIM3 ***/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* 84MHz ÷ 8400 ÷ 5000 = 2Hz 中断 */
    TIM_TimeBaseInitStructure.TIM_Period    = 4999;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 8400;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    /*** 配置NVIC ***/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0x03;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM3, ENABLE); /* 启动定时器 */

    while (1)
    {
        /* 一切由 TIM3 中断驱动，主循环空闲 */
    }
}
