#include "w25q64.h"
#include "system_config.h"
#include <string.h>

static FlashLogRecord s_records[APP_LOG_MAX_RECORDS];
static uint16_t s_head = 0;
static uint16_t s_count = 0;

void Adapter_Flash_Init(void)
{
    memset(s_records, 0, sizeof(s_records));
    s_head = 0;
    s_count = 0;
}

void Adapter_Flash_Append(LogType type, uint32_t timestamp, const char *text)
{
    FlashLogRecord *r = &s_records[s_head];
    r->timestamp = timestamp;
    r->type = type;
    strncpy(r->text, text, sizeof(r->text) - 1U);
    r->text[sizeof(r->text) - 1U] = '\0';
    s_head = (uint16_t)((s_head + 1U) % APP_LOG_MAX_RECORDS);
    if (s_count < APP_LOG_MAX_RECORDS) s_count++;
}

uint16_t Adapter_Flash_Count(void)
{
    return s_count;
}

uint8_t Adapter_Flash_Read(uint16_t index, FlashLogRecord *out)
{
    uint16_t pos;
    if (index >= s_count) return 0U;
    pos = (uint16_t)((s_head + APP_LOG_MAX_RECORDS - s_count + index) % APP_LOG_MAX_RECORDS);
    *out = s_records[pos];
    return 1U;
}

void Adapter_Flash_Clear(void)
{
    memset(s_records, 0, sizeof(s_records));
    s_head = 0;
    s_count = 0;
}
