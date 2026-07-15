#ifndef __APP_GAS_H__
#define __APP_GAS_H__

#include "app_types.h"
#include <stdint.h>

void App_Gas_Init(GasDataSource source);
void App_Gas_Update(void);
const GasDataManager *App_Gas_Get(void);
void App_Gas_SetSource(GasDataSource source);
GasDataSource App_Gas_GetSource(void);
const char *App_Gas_SourceName(GasDataSource source);
void App_Gas_SetUartValue(uint16_t value);
void App_Gas_StartUartRamp(uint16_t start, uint16_t end, uint16_t seconds);
void App_Gas_Task1000ms(void);
void App_Gas_SetSceneValue(uint16_t value);

#endif
