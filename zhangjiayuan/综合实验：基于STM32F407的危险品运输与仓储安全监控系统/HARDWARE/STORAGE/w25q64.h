#ifndef __ADAPTER_FLASH_H__
#define __ADAPTER_FLASH_H__

#include "app_types.h"
#include <stdint.h>

typedef struct {
    uint32_t timestamp;
    LogType type;
    char text[80];
} FlashLogRecord;

void Adapter_Flash_Init(void);
void Adapter_Flash_Append(LogType type, uint32_t timestamp, const char *text);
uint16_t Adapter_Flash_Count(void);
uint8_t Adapter_Flash_Read(uint16_t index, FlashLogRecord *out);
void Adapter_Flash_Clear(void);

#endif
