#include "stm32f4xx.h"

/* ADC采样值 */
u16 Adc = 0;

/* LED亮灭间隔时间，单位为ms */
u16 DelayTime = 100;

/* 函数声明 */
void LED_Init(void);
void Adc_Init(void);
uint16_t Get_Adc(uint8_t ch);
uint16_t Get_Adc_Average(uint8_t ch, uint8_t times);
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

int main(void)
{
    /*
     * 初始化LED1
     * LED1连接PB5，本拓展不使用PWM，
     * 将PB5配置为普通GPIO输出
     */
    LED_Init();

    /*
     * 初始化ADC1
     * ADC1通道11对应PC1电位器
     */
    Adc_Init();

    while (1)
    {
        /*
         * 连续采集10次ADC数据并取平均值，
         * 减小ADC数据波动
         */
        Adc = Get_Adc_Average(ADC_Channel_11, 10);

        /*
         * 将ADC值0～4095映射为100～2000ms
         *
         * ADC最小值0：
         * DelayTime = 100ms
         *
         * ADC最大值4095：
         * DelayTime = 2000ms
         */
        DelayTime = 100 + (uint32_t)Adc * 1900 / 4095;

        /*
         * 翻转PB5输出电平
         * LED原来亮则熄灭，原来灭则点亮
         */
        GPIO_ToggleBits(GPIOB, GPIO_Pin_5);

        /*
         * 延时时间由电位器ADC值决定
         */
        delay_ms(DelayTime);
    }
}

/*
 * LED1初始化函数
 * LED1连接到PB5
 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /*
     * 使能GPIOB时钟
     */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    /*
     * 配置PB5
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /*
     * PB5先输出高电平
     * 遨游100开发板上的LED通常为低电平点亮，
     * 因此高电平时LED初始熄灭
     */
    GPIO_SetBits(GPIOB, GPIO_Pin_5);
}

/*
 * ADC1初始化函数
 * ADC1通道11对应PC1
 */
void Adc_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /*
     * 使能GPIOC时钟
     */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /*
     * 使能ADC1时钟
     */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /*
     * PC1配置为模拟输入模式
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /*
     * 复位ADC1
     */
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE);

    /*
     * ADC公共参数配置
     */
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;

    ADC_CommonInitStructure.ADC_TwoSamplingDelay =
        ADC_TwoSamplingDelay_5Cycles;

    ADC_CommonInitStructure.ADC_DMAAccessMode =
        ADC_DMAAccessMode_Disabled;

    ADC_CommonInitStructure.ADC_Prescaler =
        ADC_Prescaler_Div6;

    ADC_CommonInit(&ADC_CommonInitStructure);

    /*
     * ADC1参数配置
     */
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;

    ADC_InitStructure.ADC_ScanConvMode = DISABLE;

    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;

    ADC_InitStructure.ADC_ExternalTrigConvEdge =
        ADC_ExternalTrigConvEdge_None;

    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;

    ADC_InitStructure.ADC_NbrOfConversion = 1;

    ADC_Init(ADC1, &ADC_InitStructure);

    /*
     * 使能ADC1
     */
    ADC_Cmd(ADC1, ENABLE);
}

/*
 * 进行一次ADC转换
 */
uint16_t Get_Adc(uint8_t ch)
{
    /*
     * 配置ADC1规则通道
     *
     * ch：需要转换的ADC通道
     * 1：规则转换序列中的第1个位置
     * 480Cycles：采样时间为480个ADC时钟周期
     */
    ADC_RegularChannelConfig(
        ADC1,
        ch,
        1,
        ADC_SampleTime_480Cycles
    );

    /*
     * 软件启动ADC转换
     */
    ADC_SoftwareStartConv(ADC1);

    /*
     * 等待转换结束标志EOC置位
     */
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
    {
    }

    /*
     * 返回ADC转换结果
     */
    return ADC_GetConversionValue(ADC1);
}

/*
 * ADC连续转换多次并取平均值
 */
uint16_t Get_Adc_Average(uint8_t ch, uint8_t times)
{
    uint32_t temp_val = 0;
    uint8_t i;

    for (i = 0; i < times; i++)
    {
        /*
         * 累加每次ADC转换结果
         */
        temp_val += Get_Adc(ch);

        /*
         * 两次采样之间延时5ms
         */
        delay_ms(5);
    }

    /*
     * 返回ADC平均值
     */
    return temp_val / times;
}

/*
 * 微秒延时函数
 */
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

/*
 * 毫秒延时函数
 */
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