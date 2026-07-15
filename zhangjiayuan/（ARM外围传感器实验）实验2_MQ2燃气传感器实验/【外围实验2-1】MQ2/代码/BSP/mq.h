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
  * @brief  读取MQ2对应的ADC转换值
  * @param  无
  * @retval 12位ADC转换结果，范围为0～4095
  */
uint16_t MQ_ReadValue(void);

#endif /* __MQ_H__ */
