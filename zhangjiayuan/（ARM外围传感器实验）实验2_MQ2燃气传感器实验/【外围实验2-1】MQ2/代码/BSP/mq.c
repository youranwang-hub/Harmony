#include "stm32f4xx.h"
#include "mq.h"
#include "delay.h"

/* MQ2使用ADC1 */
#define MQ_ADC    ADC1

/**
  * @brief  配置MQ2模拟输入引脚PC2
  * @param  无
  * @retval 无
  */
static void MQ_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOC时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /* PC2配置为模拟输入模式 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/**
  * @brief  配置ADC1通道12
  * @param  无
  * @retval 无
  */
static void MQ_ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;

    /* 使能ADC1时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /*
     * ADC公共参数配置：
     * 独立模式
     * ADC时钟4分频
     * 不使用DMA
     * 两次采样间隔20个周期
     */
    ADC_CommonInitStructure.ADC_Mode =
        ADC_Mode_Independent;

    ADC_CommonInitStructure.ADC_Prescaler =
        ADC_Prescaler_Div4;

    ADC_CommonInitStructure.ADC_DMAAccessMode =
        ADC_DMAAccessMode_Disabled;

    ADC_CommonInitStructure.ADC_TwoSamplingDelay =
        ADC_TwoSamplingDelay_20Cycles;

    ADC_CommonInit(&ADC_CommonInitStructure);

    /* 设置ADC结构体默认值 */
    ADC_StructInit(&ADC_InitStructure);

    /*
     * ADC1参数配置：
     * 12位分辨率
     * 单通道模式
     * 单次转换模式
     * 软件触发
     * 数据右对齐
     * 一个转换通道
     */
    ADC_InitStructure.ADC_Resolution =
        ADC_Resolution_12b;

    ADC_InitStructure.ADC_ScanConvMode =
        DISABLE;

    ADC_InitStructure.ADC_ContinuousConvMode =
        DISABLE;

    ADC_InitStructure.ADC_ExternalTrigConvEdge =
        ADC_ExternalTrigConvEdge_None;

    ADC_InitStructure.ADC_ExternalTrigConv =
        ADC_ExternalTrigConv_T1_CC1;

    ADC_InitStructure.ADC_DataAlign =
        ADC_DataAlign_Right;

    ADC_InitStructure.ADC_NbrOfConversion =
        1;

    ADC_Init(MQ_ADC, &ADC_InitStructure);

    /*
     * PC2对应ADC1通道12
     * 转换序号为1
     * 采样时间为28个ADC周期
     */
    ADC_RegularChannelConfig(
        MQ_ADC,
        ADC_Channel_12,
        1,
        ADC_SampleTime_28Cycles
    );

    /* 使能ADC1 */
    ADC_Cmd(MQ_ADC, ENABLE);
}

/**
  * @brief  初始化MQ2
  * @param  无
  * @retval 无
  */
void MQ_Init(void)
{
    MQ_GPIO_Config();
    MQ_ADC_Config();
}

/**
  * @brief  读取MQ2对应的ADC转换值
  * @param  无
  * @retval ADC转换值，范围0～4095
  */
uint16_t MQ_ReadValue(void)
{
    uint32_t timeout = 100;

    /* 清除上一次ADC转换完成标志 */
    ADC_ClearFlag(MQ_ADC, ADC_FLAG_EOC);

    /* 软件启动ADC转换 */
    ADC_SoftwareStartConv(MQ_ADC);

    /* 等待ADC转换完成 */
    while (ADC_GetFlagStatus(MQ_ADC, ADC_FLAG_EOC) == RESET)
    {
        /*
         * 防止ADC异常时程序永久阻塞。
         * 每次等待1ms，最多等待约100ms。
         */
        if (timeout == 0)
        {
            return 0;
        }

        timeout--;
        delay_ms(1);
    }

    /* 返回ADC转换结果 */
    return ADC_GetConversionValue(MQ_ADC);
}
