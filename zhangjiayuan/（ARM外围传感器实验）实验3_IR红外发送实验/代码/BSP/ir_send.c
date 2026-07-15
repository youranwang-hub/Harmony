#include "stm32f4xx.h"
#include "delay.h"

#define IR_SEND_TIM  TIM10

void IR_Send_GPIO_Init(void);
void IR_Send_PWM_Init(void);

#define IR_SEND_DELAY_US(x) systick_delay_us(x)


void IR_Send_Init(void)
{
    IR_Send_GPIO_Init();
    IR_Send_PWM_Init();
}


void IR_Send_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    //PB8，TIM10，Channel 1
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM10);
}


void IR_Send_PWM_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

    //TIM10的输入频率为APB2频率168M，最终需生成38K载波
    //168M/38K = 4421
    TIM_TimeBaseStruct.TIM_Period = 4421;
    TIM_TimeBaseStruct.TIM_Prescaler = 1;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(IR_SEND_TIM, &TIM_TimeBaseStruct);

    //PWM Mode：CH1
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = 2210;
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(IR_SEND_TIM, &TIM_OCInitStruct);

    TIM_CCxCmd(IR_SEND_TIM, TIM_Channel_1, TIM_CCx_Enable);
}


void PwmEnable(void)
{
    TIM_ForcedOC1Config(IR_SEND_TIM, TIM_OCMode_PWM1);
    TIM_Cmd(IR_SEND_TIM, ENABLE);
}


void PwmDisable(void)
{
    TIM_ForcedOC1Config(
        IR_SEND_TIM,
        TIM_ForcedAction_InActive
    );

    TIM_Cmd(IR_SEND_TIM, DISABLE);
}


void IR_Sent_Byte(uint8_t data)
{
    uint8_t i = 0;

    for (i = 0; i < 8; i++)
    {
        PwmEnable();
        IR_SEND_DELAY_US(560);
        PwmDisable();

        if (data & 0x01)
        {
            IR_SEND_DELAY_US(1680);
        }
        else
        {
            IR_SEND_DELAY_US(560);
        }

        data = data >> 1;
    }
}


void IR_Send(uint8_t addr, uint8_t code)
{
    //引导头
    PwmEnable();
    IR_SEND_DELAY_US(9000);

    PwmDisable();
    IR_SEND_DELAY_US(4500);

    IR_Sent_Byte(addr);
    IR_Sent_Byte(~addr);
    IR_Sent_Byte(code);
    IR_Sent_Byte(~code);

    //停止位，必须有
    PwmEnable();
    IR_SEND_DELAY_US(560);
    PwmDisable();
}
