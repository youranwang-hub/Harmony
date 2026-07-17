#include "adc_sensor.h"
#include "system_config.h"

void Board_ADC_Init(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef adc;
    ADC_CommonInitTypeDef common;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOC, &gpio);

    ADC_CommonStructInit(&common);
    common.ADC_Mode = ADC_Mode_Independent;
    common.ADC_Prescaler = ADC_Prescaler_Div4;
    common.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    common.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&common);

    ADC_StructInit(&adc);
    adc.ADC_Resolution = ADC_Resolution_12b;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
}

uint16_t Board_ADC_ReadRaw(uint8_t channel)
{
    uint32_t timeout = 100000U;
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_144Cycles);
    ADC_SoftwareStartConv(ADC1);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET && timeout--) {}
    if (timeout == 0U) return 0U;
    return (uint16_t)ADC_GetConversionValue(ADC1);
}

uint16_t Board_ADC_RawToGas(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * APP_ADC_GAS_MAX) / 4095U);
}

uint16_t Board_ADC_ReadGasReal(void)
{
    return Board_ADC_RawToGas(Board_ADC_ReadRaw(APP_ADC_CH_MQ2_REAL));
}

uint16_t Board_ADC_ReadPot(void)
{
    return Board_ADC_RawToGas(Board_ADC_ReadRaw(APP_ADC_CH_POT));
}

uint16_t Board_ADC_ReadDacLoop(void)
{
    return Board_ADC_RawToGas(Board_ADC_ReadRaw(APP_ADC_CH_DAC_LOOP));
}

uint16_t Board_ADC_ReadBatteryMv(void)
{
    uint16_t raw = Board_ADC_ReadRaw(APP_ADC_CH_BATTERY);
    return (uint16_t)(((uint32_t)raw * 3300U * 4U) / 4095U);
}
