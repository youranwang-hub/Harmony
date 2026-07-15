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

    /* 使能GPIOC外设时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOC,
        ENABLE
    );

    /*
     * PC2配置为模拟输入模式。
     * PC2对应ADC1通道12。
     */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_2;

    GPIO_InitStructure.GPIO_Mode =
        GPIO_Mode_AIN;

    GPIO_InitStructure.GPIO_PuPd =
        GPIO_PuPd_NOPULL;

    GPIO_Init(
        GPIOC,
        &GPIO_InitStructure
    );
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

    /* 使能ADC1外设时钟 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_ADC1,
        ENABLE
    );

    /*
     * ADC公共参数配置：
     * 独立ADC模式；
     * ADC时钟4分频；
     * 不使用DMA；
     * 两次采样间隔为20个ADC周期。
     */
    ADC_CommonInitStructure.ADC_Mode =
        ADC_Mode_Independent;

    ADC_CommonInitStructure.ADC_Prescaler =
        ADC_Prescaler_Div4;

    ADC_CommonInitStructure.ADC_DMAAccessMode =
        ADC_DMAAccessMode_Disabled;

    ADC_CommonInitStructure.ADC_TwoSamplingDelay =
        ADC_TwoSamplingDelay_20Cycles;

    ADC_CommonInit(
        &ADC_CommonInitStructure
    );

    /* 将ADC配置结构体设置为默认值 */
    ADC_StructInit(
        &ADC_InitStructure
    );

    /*
     * ADC1工作参数：
     * 12位分辨率；
     * 单通道采集；
     * 单次转换；
     * 软件触发；
     * 数据右对齐；
     * 一个规则转换通道。
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

    ADC_Init(
        MQ_ADC,
        &ADC_InitStructure
    );

    /*
     * 配置规则通道：
     * ADC1通道12；
     * 转换顺序为第1个；
     * 采样时间为28个ADC周期。
     */
    ADC_RegularChannelConfig(
        MQ_ADC,
        ADC_Channel_12,
        1,
        ADC_SampleTime_28Cycles
    );

    /* 使能ADC1 */
    ADC_Cmd(
        MQ_ADC,
        ENABLE
    );
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
  * @brief  读取一次MQ2的ADC值
  * @param  无
  * @retval ADC转换结果，范围0～4095
  */
uint16_t MQ_ReadValue(void)
{
    uint32_t timeout;

    timeout = 100;

    /* 清除ADC转换完成标志 */
    ADC_ClearFlag(
        MQ_ADC,
        ADC_FLAG_EOC
    );

    /* 软件启动ADC转换 */
    ADC_SoftwareStartConv(
        MQ_ADC
    );

    /* 等待ADC转换完成 */
    while (
        ADC_GetFlagStatus(
            MQ_ADC,
            ADC_FLAG_EOC
        ) == RESET
    )
    {
        if (timeout == 0)
        {
            /*
             * 等待超过100ms仍未转换完成，
             * 返回0，避免程序永久阻塞。
             */
            return 0;
        }

        timeout--;
        delay_ms(1);
    }

    /* 读取ADC转换结果 */
    return ADC_GetConversionValue(
        MQ_ADC
    );
}

/**
  * @brief  多次采样并进行去极值平均滤波
  * @param  sampleCount：采样次数
  * @retval 滤波后的ADC值
  */
uint16_t MQ_ReadAverage(uint8_t sampleCount)
{
    uint8_t i;
    uint16_t adcValue;
    uint16_t maximumValue;
    uint16_t minimumValue;
    uint32_t sum;

    /*
     * 去掉最大值和最小值后求平均，
     * 因此采样次数不能小于3。
     */
    if (sampleCount < 3)
    {
        sampleCount = 3;
    }

    /*
     * 防止采样次数过大，导致一次读取时间过长。
     */
    if (sampleCount > 50)
    {
        sampleCount = 50;
    }

    sum = 0;
    maximumValue = 0;
    minimumValue = 4095;

    for (i = 0; i < sampleCount; i++)
    {
        adcValue = MQ_ReadValue();

        sum += adcValue;

        if (adcValue > maximumValue)
        {
            maximumValue = adcValue;
        }

        if (adcValue < minimumValue)
        {
            minimumValue = adcValue;
        }

        /*
         * 相邻采样之间适当延时，
         * 避免连续读取过快。
         */
        delay_ms(5);
    }

    /*
     * 从总和中去掉最大值和最小值，
     * 再对剩余数据求平均。
     */
    sum = sum - maximumValue - minimumValue;

    return (uint16_t)(
        sum / (sampleCount - 2)
    );
}

/**
  * @brief  采集正常环境数据，计算基线值
  * @param  groupCount：采集组数
  * @param  intervalMs：相邻两组间隔时间
  * @retval 正常环境基线ADC值
  */
uint16_t MQ_CalibrateBaseline(
    uint16_t groupCount,
    uint16_t intervalMs
)
{
    uint16_t i;
    uint16_t averageValue;
    uint32_t sum;

    if (groupCount == 0)
    {
        groupCount = 1;
    }

    sum = 0;

    for (i = 0; i < groupCount; i++)
    {
        /*
         * 每组数据内部再进行10次采样滤波，
         * 提高基线值的稳定性。
         */
        averageValue = MQ_ReadAverage(10);

        sum += averageValue;

        delay_ms(intervalMs);
    }

    return (uint16_t)(
        sum / groupCount
    );
}

/**
  * @brief  将ADC值转换为毫伏
  * @param  adcValue：ADC转换结果
  * @retval 电压值，单位mV
  */
uint16_t MQ_ADCToMillivolt(uint16_t adcValue)
{
    uint32_t voltage;

    /*
     * ADC为12位，满量程为4096。
     * 参考电压为3300mV。
     *
     * 加2048用于进行四舍五入。
     */
    voltage =
        ((uint32_t)adcValue * 3300U + 2048U)
        / 4096U;

    return (uint16_t)voltage;
}
