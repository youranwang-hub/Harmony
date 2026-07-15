#ifndef __APP_MAIN_H__
#define __APP_MAIN_H__

#include "app_types.h"
#include <stdint.h>

void App_Init(void);
void App_Run(void);
void App_OnSysTick(void);
void App_OnEmergencyButton(void);
const AppSnapshot *App_GetSnapshot(void);

#endif
