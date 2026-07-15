#include "eeprom.h"
#include "system_config.h"
#include <string.h>

static AppParameters s_params;
static uint8_t s_has_params = 0;

void Adapter_EEPROM_Init(void)
{
    memset(&s_params, 0, sizeof(s_params));
    s_params.gas_warn = APP_GAS_WARN_ENTER;
    s_params.gas_danger = APP_GAS_DANGER_ENTER;
    s_params.gas_critical = APP_GAS_CRITICAL_ENTER;
    s_params.default_source = GAS_SOURCE_POT;
    s_params.crc = Adapter_EEPROM_Crc(&s_params);
    s_has_params = 1U;
}

uint16_t Adapter_EEPROM_Crc(const AppParameters *params)
{
    const uint8_t *p = (const uint8_t *)params;
    uint16_t crc = 0xA55A;
    uint16_t i;
    for (i = 0; i < (uint16_t)(sizeof(AppParameters) - sizeof(uint16_t)); ++i) {
        crc = (uint16_t)((crc << 1) ^ p[i] ^ (crc >> 15));
    }
    return crc;
}

uint8_t Adapter_EEPROM_Load(AppParameters *params)
{
    if (!s_has_params) return 0U;
    *params = s_params;
    return (params->crc == Adapter_EEPROM_Crc(params)) ? 1U : 0U;
}

uint8_t Adapter_EEPROM_Save(const AppParameters *params)
{
    s_params = *params;
    s_params.crc = Adapter_EEPROM_Crc(&s_params);
    s_has_params = 1U;
    return 1U;
}
