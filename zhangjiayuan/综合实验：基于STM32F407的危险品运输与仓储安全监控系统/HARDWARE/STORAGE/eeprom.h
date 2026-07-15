#ifndef __ADAPTER_EEPROM_H__
#define __ADAPTER_EEPROM_H__

#include "app_types.h"
#include <stdint.h>

void Adapter_EEPROM_Init(void);
uint8_t Adapter_EEPROM_Load(AppParameters *params);
uint8_t Adapter_EEPROM_Save(const AppParameters *params);
uint16_t Adapter_EEPROM_Crc(const AppParameters *params);

#endif
