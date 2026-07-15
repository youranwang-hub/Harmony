#ifndef __ADAPTER_DHT11_H__
#define __ADAPTER_DHT11_H__

#include <stdint.h>

void Adapter_DHT11_Init(void);
uint8_t Adapter_DHT11_Read(int16_t *temperature_x10, uint8_t *humidity);
void Adapter_DHT11_SetOffset(int16_t temp_offset_x10, int8_t hum_offset);

#endif
