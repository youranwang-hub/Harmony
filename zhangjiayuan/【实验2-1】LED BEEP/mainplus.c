#include "stm32f4xx.h"

/* 定义GPIO初始化结构体变量 */
GPIO_InitTypeDef GPIO_InitStructure;

/*----------------------------------------------------------
  LED控制宏
  遨游100开发板的LED为低电平点亮，高电平熄灭
----------------------------------------------------------*/
#define LED1_ON()     GPIO_ResetBits(GPIOB, GPIO_Pin_5)
#define LED1_OFF()    GPIO_SetBits(GPIOB, GPIO_Pin_5)

#define LED2_ON()     GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define LED2_OFF()    GPIO_SetBits(GPIOB, GPIO_Pin_0)

#define LED3_ON()     GPIO_ResetBits(GPIOB, GPIO_Pin_1)
#define LED3_OFF()    GPIO_SetBits(GPIOB, GPIO_Pin_1)

/*----------------------------------------------------------
  蜂鸣器控制宏
  PA8输出低电平时蜂鸣器响，高电平时蜂鸣器关闭
----------------------------------------------------------*/
#define BEEP_ON()     GPIO_ResetBits(GPIOA, GPIO_Pin_8)
#define BEEP_OFF()    GPIO_SetBits(GPIOA, GPIO_Pin_8)

/* 延时函数声明 */
void delay_ms(uint32_t ms);

/*----------------------------------------------------------
  毫秒延时函数
  使用SysTick定时器实现延时
----------------------------------------------------------*/
void delay_ms(uint32_t ms)
{
    uint32_t i;

    /* 设置SysTick每1毫秒产生一次计数完成标志 */
    SysTick->LOAD = SystemCoreClock / 1000 - 1;

    /* 清空当前计数值 */
    SysTick->VAL = 0;

    /* 使用处理器时钟并启动SysTick */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
        SysTick_CTRL_ENABLE_Msk;

    for (i = 0; i < ms; i++)
    {
        /* 等待1毫秒计数完成 */
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    }

    /* 延时完成后关闭SysTick */
    SysTick->CTRL = 0;
}

int main(void)
{
    /* 更新系统时钟变量，保证延时函数计时正确 */
    SystemCoreClockUpdate();

    /*------------------------------------------------------
      1. 开启GPIOA和GPIOB的外设时钟
      GPIOB控制三个LED，GPIOA控制蜂鸣器
    ------------------------------------------------------*/
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    /*------------------------------------------------------
      2. 初始化LED对应的GPIO
      LED1：PB5
      LED2：PB0
      LED3：PB1
    ------------------------------------------------------*/

    /* 初始化前先输出高电平，防止LED意外点亮 */
    GPIO_SetBits(
        GPIOB,
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
    );

    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /*------------------------------------------------------
      3. 初始化蜂鸣器对应的GPIO
      蜂鸣器：PA8
    ------------------------------------------------------*/

    /* 初始化前输出高电平，使蜂鸣器保持关闭 */
    GPIO_SetBits(GPIOA, GPIO_Pin_8);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOA, &GPIO_InitStructure);

    while (1)
    {
        /*--------------------------------------------------
          组合1：
          LED1点亮，LED2和LED3熄灭，蜂鸣器关闭
        --------------------------------------------------*/
        LED1_ON();
        LED2_OFF();
        LED3_OFF();
        BEEP_OFF();

        delay_ms(1000);

        /*--------------------------------------------------
          组合2：
          LED2点亮，LED1和LED3熄灭，蜂鸣器响
        --------------------------------------------------*/
        LED1_OFF();
        LED2_ON();
        LED3_OFF();
        BEEP_ON();

        delay_ms(1000);

        /*--------------------------------------------------
          组合3：
          LED3点亮，LED1和LED2熄灭，蜂鸣器关闭
        --------------------------------------------------*/
        LED1_OFF();
        LED2_OFF();
        LED3_ON();
        BEEP_OFF();

        delay_ms(1000);

        /*--------------------------------------------------
          组合4：
          LED1和LED3点亮，LED2熄灭，蜂鸣器响
        --------------------------------------------------*/
        LED1_ON();
        LED2_OFF();
        LED3_ON();
        BEEP_ON();

        delay_ms(1000);

        /*--------------------------------------------------
          组合5：
          三个LED全部点亮，蜂鸣器关闭
        --------------------------------------------------*/
        LED1_ON();
        LED2_ON();
        LED3_ON();
        BEEP_OFF();

        delay_ms(1000);

        /*--------------------------------------------------
          组合6：
          三个LED全部熄灭，蜂鸣器响
        --------------------------------------------------*/
        LED1_OFF();
        LED2_OFF();
        LED3_OFF();
        BEEP_ON();

        delay_ms(1000);

        /*--------------------------------------------------
          组合7：
          三个LED全部熄灭，蜂鸣器关闭
        --------------------------------------------------*/
        LED1_OFF();
        LED2_OFF();
        LED3_OFF();
        BEEP_OFF();

        delay_ms(1000);
    }
}