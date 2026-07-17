#ifndef __APP_LOG_H__
#define __APP_LOG_H__

#include "app_types.h"
#include "w25q64.h"
#include <stdint.h>

void App_Log_Init(void);
void App_Log_Add(LogType type, const char *text);
void App_Log_Export(void);
uint16_t App_Log_Count(void);
uint8_t App_Log_Read(uint16_t index, FlashLogRecord *out);
void App_Log_Clear(void);
const char *App_Log_TypeName(LogType type);

#endif
