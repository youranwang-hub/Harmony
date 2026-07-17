#include "log_manager.h"
#include "w25q64.h"
#include "dma_uart.h"
#include "rtc_manager.h"
#include "usart_debug.h"
#include "system_config.h"
#include <stdio.h>
#include <string.h>

const char *App_Log_TypeName(LogType type)
{
    switch (type) {
    case LOG_AUTH: return "AUTH";
    case LOG_TRANSPORT: return "TRANSPORT";
    case LOG_ALARM: return "ALARM";
    case LOG_FAULT: return "FAULT";
    case LOG_PARAM: return "PARAM";
    default: return "INFO";
    }
}

void App_Log_Init(void)
{
    Adapter_Flash_Init();
    App_Log_Add(LOG_INFO, "system log initialized");
}

void App_Log_Add(LogType type, const char *text)
{
    Adapter_Flash_Append(type, Board_RTC_GetUnixLike(), text);
}

uint16_t App_Log_Count(void)
{
    return Adapter_Flash_Count();
}

uint8_t App_Log_Read(uint16_t index, FlashLogRecord *out)
{
    return Adapter_Flash_Read(index, out);
}

void App_Log_Clear(void)
{
    Adapter_Flash_Clear();
    App_Log_Add(LOG_INFO, "log cleared");
}

void App_Log_Export(void)
{
    static char export_buf[APP_EXPORT_BUFFER_SIZE];
    uint16_t i;
    uint16_t count = Adapter_Flash_Count();
    uint16_t used = 0;
    FlashLogRecord rec;

    used += (uint16_t)snprintf(export_buf + used, sizeof(export_buf) - used,
                               "\r\n# Hazmat log export, count=%u\r\n", count);
    for (i = 0; i < count && used < sizeof(export_buf) - 96U; ++i) {
        if (Adapter_Flash_Read(i, &rec)) {
            used += (uint16_t)snprintf(export_buf + used, sizeof(export_buf) - used,
                                       "%03u,%lu,%s,%s\r\n",
                                       i, (unsigned long)rec.timestamp, App_Log_TypeName(rec.type), rec.text);
        }
    }
    if (!Board_DMA_USART2_Send((const uint8_t *)export_buf, used)) {
        UART_SendString(USART2, export_buf);
    }
}
