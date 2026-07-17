#include "dac_output.h"
#include "system_config.h"

static uint16_t s_last_gas_value = 0;

void Board_DAC_Init(void)
{
    GPIO_InitTypeDef gpio;
    DAC_InitTypeDef dac;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &gpio);

    DAC_StructInit(&dac);
    dac.DAC_Trigger = DAC_Trigger_None;
    dac.DAC_WaveGeneration = DAC_WaveGeneration_None;
    dac.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_1, &dac);
    DAC_Cmd(DAC_Channel_1, ENABLE);
    Board_DAC_SetGasValue(0);
}

void Board_DAC_SetMilliVolt(uint16_t mv)
{
    uint32_t raw;
    if (mv > 3300U) mv = 3300U;
    raw = ((uint32_t)mv * 4095U) / 3300U;
    DAC_SetChannel1Data(DAC_Align_12b_R, (uint16_t)raw);
}

void Board_DAC_SetGasValue(uint16_t gas_value)
{
    uint32_t raw;
    if (gas_value > APP_ADC_GAS_MAX) gas_value = APP_ADC_GAS_MAX;
    s_last_gas_value = gas_value;
    raw = ((uint32_t)gas_value * 4095U) / APP_ADC_GAS_MAX;
    DAC_SetChannel1Data(DAC_Align_12b_R, (uint16_t)raw);
}

uint16_t Board_DAC_GetLastGasValue(void)
{
    return s_last_gas_value;
}
