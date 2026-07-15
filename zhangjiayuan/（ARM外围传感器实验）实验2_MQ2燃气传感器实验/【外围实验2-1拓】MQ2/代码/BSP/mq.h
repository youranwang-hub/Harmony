#ifndef __MQ_H__
#define __MQ_H__

#include "stm32f4xx.h"

/**
  * @brief  初始化MQ2使用的GPIO和ADC
  * @param  无
  * @retval 无
  */
void MQ_Init(void);

/**
  * @brief  读取一次MQ2的ADC值
  * @param  无
  * @retval ADC转换结果，范围0～4095
  */
uint16_t MQ_ReadValue(void);

/**
  * @brief  对MQ2进行多次采样滤波
  * @param  sampleCount：采样次数，建议不小于3
  * @retval 去掉最大值和最小值后的平均ADC值
  */
uint16_t MQ_ReadAverage(uint8_t sampleCount);

/**
  * @brief  采集正常环境数据，计算MQ2基线值
  * @param  groupCount：基线采集组数
  * @param  intervalMs：每组数据之间的时间间隔，单位ms
  * @retval MQ2正常环境基线ADC值
  */
uint16_t MQ_CalibrateBaseline(
    uint16_t groupCount,
    uint16_t intervalMs
);

/**
  * @brief  将ADC值转换为毫伏
  * @param  adcValue：12位ADC转换结果
  * @retval 电压值，单位mV
  */
uint16_t MQ_ADCToMillivolt(uint16_t adcValue);

#endif /* __MQ_H__ */
