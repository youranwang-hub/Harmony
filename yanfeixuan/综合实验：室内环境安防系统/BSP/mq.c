#include "stm32f4xx.h"
#include "mq.h"
#include "delay.h"

#define MQ_ADC ADC1
void MQ_GPIO_Config(void){
GPIO_InitTypeDef GPIO_InitX;
RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);//使能GPIO 时钟

//配置为模拟输入
GPIO_InitX.GPIO_Pin = GPIO_Pin_2;
GPIO_InitX.GPIO_Mode = GPIO_Mode_AIN;
GPIO_InitX.GPIO_PuPd = GPIO_PuPd_NOPULL ;
GPIO_Init(GPIOC, &GPIO_InitX);
}
void MQ_ADC_Config(void){
ADC_InitTypeDef ADC_InitX;
ADC_CommonInitTypeDef ADC_CommonInitX;

RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 , ENABLE); // 开启ADC时钟

ADC_CommonInitX.ADC_Mode = ADC_Mode_Independent; // 独立ADC模式
ADC_CommonInitX.ADC_Prescaler = ADC_Prescaler_Div4; // 时钟为fpclk x分频
ADC_CommonInitX.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; // 禁止DMA直接访问模式
ADC_CommonInitX.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles; //两通道间隔延时
ADC_CommonInit(&ADC_CommonInitX);

ADC_StructInit(&ADC_InitX);
ADC_InitX.ADC_Resolution = ADC_Resolution_12b; // ADC 分辨率
ADC_InitX.ADC_ScanConvMode = DISABLE; // 禁止扫描模式，多通道采集才需要
ADC_InitX.ADC_ContinuousConvMode = DISABLE; //连续转换
ADC_InitX.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; //禁止外部边沿触发
ADC_InitX.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;//禁止时可为任意值，无效
ADC_InitX.ADC_DataAlign = ADC_DataAlign_Right; //数据右对齐
ADC_InitX.ADC_NbrOfConversion = 1; //转换通道 1个
ADC_Init(MQ_ADC, &ADC_InitX);

//配置 ADC 通道转换顺序为1，第一个转换
ADC_RegularChannelConfig(MQ_ADC, ADC_Channel_12, 1, ADC_SampleTime_28Cycles);

ADC_Cmd(MQ_ADC, ENABLE); // 使能ADC

}

void MQ_Init(void){
MQ_GPIO_Config();
MQ_ADC_Config();
}
uint16_t MQ_ReadValue(void){
uint32_t uTimeout = 100;
ADC_SoftwareStartConv(MQ_ADC);//开始ADC转换

//判断是否采样转换完毕
while(ADC_GetFlagStatus(MQ_ADC, ADC_FLAG_EOC) != SET){
if(uTimeout--){ return 0; }
delay_ms(1);
}
ADC_ClearFlag(MQ_ADC, ADC_FLAG_EOC);//清除标志位

return ADC_GetConversionValue(MQ_ADC);
}
