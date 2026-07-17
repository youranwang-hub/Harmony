#include "dht11.h"

static int16_t s_temp_offset = 0;
static int8_t s_hum_offset = 0;

void Adapter_DHT11_Init(void)
{
    s_temp_offset = 0;
    s_hum_offset = 0;
}

uint8_t Adapter_DHT11_Read(int16_t *temperature_x10, uint8_t *humidity)
{
    int16_t hum;
    *temperature_x10 = (int16_t)(264 + s_temp_offset);
    hum = (int16_t)(58 + s_hum_offset);
    if (hum < 0) hum = 0;
    if (hum > 100) hum = 100;
    *humidity = (uint8_t)hum;
    return 1U;
}

void Adapter_DHT11_SetOffset(int16_t temp_offset_x10, int8_t hum_offset)
{
    s_temp_offset = temp_offset_x10;
    s_hum_offset = hum_offset;
}
