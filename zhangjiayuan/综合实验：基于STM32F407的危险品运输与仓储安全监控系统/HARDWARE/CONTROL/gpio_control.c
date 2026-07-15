#include "gpio_control.h"

static GPIO_TypeDef *led_port(BoardLed led)
{
    (void)led;
    return GPIOB;
}

static uint16_t led_pin(BoardLed led)
{
    if (led == BOARD_LED_GREEN) return GPIO_Pin_0;
    if (led == BOARD_LED_RED) return GPIO_Pin_1;
    return GPIO_Pin_5;
}

void Board_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_8;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOC, &gpio);

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
    EXTI_StructInit(&exti);
    exti.EXTI_Line = EXTI_Line0;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 1;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    Board_LED_Set(BOARD_LED_GREEN, 0);
    Board_LED_Set(BOARD_LED_RED, 0);
    Board_Buzzer_Set(0);
}

void Board_LED_Set(BoardLed led, uint8_t on)
{
    if (on) GPIO_ResetBits(led_port(led), led_pin(led));
    else GPIO_SetBits(led_port(led), led_pin(led));
}

void Board_LED_Toggle(BoardLed led)
{
    led_port(led)->ODR ^= led_pin(led);
}

void Board_Buzzer_Set(uint8_t on)
{
    if (on) GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    else GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

uint8_t Board_Key_Read(uint8_t key_id)
{
    if (key_id == 1) return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) ? 0U : 1U;
    if (key_id == 2) return GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) ? 0U : 1U;
    if (key_id == 3) return GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6) ? 0U : 1U;
    if (key_id == 4) return GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7) ? 0U : 1U;
    return 0U;
}
