#ifndef __MQ_H__
#define __MQ_H__

#include "stm32f4xx.h"

void MQ_Init(void);
uint16_t MQ_ReadValue(void);

#endif /* __MQ_H__ */