#ifndef _RTC_H_
#define _RTC_H_

#include "stm32f4xx.h"

typedef struct
{
    u8 hour;
    u8 minute;
    u8 enable;
} RTC_AlarmConfigTypeDef;

extern volatile u8 g_rtc_alarm_flag;

u8 BSP_RTC_Init(void);

ErrorStatus RTC_Set_Time(
    u8 hour,
    u8 min,
    u8 sec,
    u8 ampm
);

ErrorStatus RTC_Set_Date(
    u8 year,
    u8 month,
    u8 date,
    u8 week
);

void BSP_RTC_AlarmIRQ_Init(void);

ErrorStatus BSP_RTC_SetDailyAlarm(
    u8 hour,
    u8 minute,
    u8 enable
);

void BSP_RTC_SaveAlarm(
    RTC_AlarmConfigTypeDef *alarm
);

void BSP_RTC_LoadAlarm(
    RTC_AlarmConfigTypeDef *alarm
);

#endif
