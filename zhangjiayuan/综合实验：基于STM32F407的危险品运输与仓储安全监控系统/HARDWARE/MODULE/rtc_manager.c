#include "rtc_manager.h"
#include <stdio.h>

void Board_RTC_Init(void)
{
    RTC_InitTypeDef rtc;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    uint32_t timeout = 0xFFFFF;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && timeout--) {}
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();

    RTC_StructInit(&rtc);
    rtc.RTC_HourFormat = RTC_HourFormat_24;
    rtc.RTC_AsynchPrediv = 0x7F;
    rtc.RTC_SynchPrediv = 0x00FF;
    RTC_Init(&rtc);

    RTC_TimeStructInit(&time);
    time.RTC_Hours = 8;
    time.RTC_Minutes = 0;
    time.RTC_Seconds = 0;
    RTC_SetTime(RTC_Format_BIN, &time);

    RTC_DateStructInit(&date);
    date.RTC_Year = 26;
    date.RTC_Month = 7;
    date.RTC_Date = 15;
    date.RTC_WeekDay = RTC_Weekday_Wednesday;
    RTC_SetDate(RTC_Format_BIN, &date);
}

uint32_t Board_RTC_GetUnixLike(void)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    RTC_GetTime(RTC_Format_BIN, &time);
    RTC_GetDate(RTC_Format_BIN, &date);
    return ((uint32_t)date.RTC_Date * 86400U) + ((uint32_t)time.RTC_Hours * 3600U) +
           ((uint32_t)time.RTC_Minutes * 60U) + time.RTC_Seconds;
}

void Board_RTC_Format(char *out, uint16_t out_len)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    RTC_GetTime(RTC_Format_BIN, &time);
    RTC_GetDate(RTC_Format_BIN, &date);
    snprintf(out, out_len, "20%02u-%02u-%02u %02u:%02u:%02u",
             date.RTC_Year, date.RTC_Month, date.RTC_Date,
             time.RTC_Hours, time.RTC_Minutes, time.RTC_Seconds);
}
