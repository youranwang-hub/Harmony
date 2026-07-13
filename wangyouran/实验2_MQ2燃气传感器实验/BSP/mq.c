#include "stm32f4xx.h"
#include "mq.h"
#include "delay.h"

/* MQ2ʹ��ADC1 */
#define MQ_ADC    ADC1

/**
  * @brief  ����MQ2ģ����������PC2
  * @param  ��
  * @retval ��
  */
static void MQ_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* ʹ��GPIOCʱ�� */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /* PC2����Ϊģ������ģʽ */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/**
  * @brief  ����ADC1ͨ��12
  * @param  ��
  * @retval ��
  */
static void MQ_ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;

    /* ʹ��ADC1ʱ�� */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /*
     * ADC�����������ã�
     * ����ģʽ
     * ADCʱ��4��Ƶ
     * ��ʹ��DMA
     * ���β������20������
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

    /* ����ADC�ṹ��Ĭ��ֵ */
    ADC_StructInit(&ADC_InitStructure);

    /*
     * ADC1�������ã�
     * 12λ�ֱ���
     * ��ͨ��ģʽ
     * ����ת��ģʽ
     * ��������
     * �����Ҷ���
     * һ��ת��ͨ��
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
     * PC2��ӦADC1ͨ��12
     * ת�����Ϊ1
     * ����ʱ��Ϊ28��ADC����
     */
    ADC_RegularChannelConfig(
        MQ_ADC,
        ADC_Channel_12,
        1,
        ADC_SampleTime_28Cycles
    );

    /* ʹ��ADC1 */
    ADC_Cmd(MQ_ADC, ENABLE);
}

/**
  * @brief  ��ʼ��MQ2
  * @param  ��
  * @retval ��
  */
void MQ_Init(void)
{
    MQ_GPIO_Config();
    MQ_ADC_Config();
}

/**
  * @brief  ��ȡMQ2��Ӧ��ADCת��ֵ
  * @param  ��
  * @retval ADCת��ֵ����Χ0��4095
  */
uint16_t MQ_ReadValue(void)
{
    uint32_t timeout = 100;

    /* �����һ��ADCת����ɱ�־ */
    ADC_ClearFlag(MQ_ADC, ADC_FLAG_EOC);

    /* ��������ADCת�� */
    ADC_SoftwareStartConv(MQ_ADC);

    /* �ȴ�ADCת����� */
    while (ADC_GetFlagStatus(MQ_ADC, ADC_FLAG_EOC) == RESET)
    {
        /*
         * ��ֹADC�쳣ʱ��������������
         * ÿ�εȴ�1ms�����ȴ�Լ100ms��
         */
        if (timeout == 0)
        {
            return 0;
        }

        timeout--;
        delay_ms(1);
    }

    /* ����ADCת����� */
    return ADC_GetConversionValue(MQ_ADC);
}
