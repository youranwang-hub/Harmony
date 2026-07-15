#include "stm32f4xx.h"
#include "delay.h"
#include "oled.h"
#include "rtc.h"
#include "bsp_io.h"
#include "stdio.h"

typedef enum
{
    MODE_NORMAL = 0,

    MODE_SET_HOUR,
    MODE_SET_MINUTE,
    MODE_SET_SECOND,

    MODE_SET_YEAR,
    MODE_SET_MONTH,
    MODE_SET_DATE,

    MODE_SET_ALARM_HOUR,
    MODE_SET_ALARM_MINUTE,
    MODE_SET_ALARM_ENABLE
} SystemModeTypeDef;

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

RTC_AlarmConfigTypeDef RTC_AlarmConfig;

u8 Rtc_Data[50];

u8 edit_hour;
u8 edit_minute;
u8 edit_second;

u8 edit_year;
u8 edit_month;
u8 edit_date;

u8 edit_alarm_hour;
u8 edit_alarm_minute;
u8 edit_alarm_enable;

u8 time_dirty = 0;
u8 alarm_dirty = 0;

SystemModeTypeDef system_mode = MODE_NORMAL;

u8 IsLeapYear(u16 year)
{
    if(
        (year % 400 == 0) ||
        ((year % 4 == 0) && (year % 100 != 0))
    )
    {
        return 1;
    }

    return 0;
}

u8 GetDaysInMonth(u16 year, u8 month)
{
    const u8 days_table[12] =
    {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if(month == 2 && IsLeapYear(year))
    {
        return 29;
    }

    return days_table[month - 1];
}

u8 CalculateWeekDay(
    u16 year,
    u8 month,
    u8 day
)
{
    u16 calculate_year;
    u8 calculate_month;
    u8 week;

    calculate_year = year;
    calculate_month = month;

    if(calculate_month < 3)
    {
        calculate_month += 12;
        calculate_year--;
    }

    week = (
        day +
        2 * calculate_month +
        3 * (calculate_month + 1) / 5 +
        calculate_year +
        calculate_year / 4 -
        calculate_year / 100 +
        calculate_year / 400
    ) % 7;

    return week + 1;
}

void LoadEditData(void)
{
    RTC_GetTime(
        RTC_Format_BIN,
        &RTC_TimeStruct
    );

    RTC_GetDate(
        RTC_Format_BIN,
        &RTC_DateStruct
    );

    edit_hour = RTC_TimeStruct.RTC_Hours;
    edit_minute = RTC_TimeStruct.RTC_Minutes;
    edit_second = RTC_TimeStruct.RTC_Seconds;

    edit_year = RTC_DateStruct.RTC_Year;
    edit_month = RTC_DateStruct.RTC_Month;
    edit_date = RTC_DateStruct.RTC_Date;

    edit_alarm_hour = RTC_AlarmConfig.hour;
    edit_alarm_minute = RTC_AlarmConfig.minute;
    edit_alarm_enable = RTC_AlarmConfig.enable;

    time_dirty = 0;
    alarm_dirty = 0;
}

void SaveEditData(void)
{
    u8 week;
    u8 max_day;

    max_day = GetDaysInMonth(
        2000 + edit_year,
        edit_month
    );

    if(edit_date > max_day)
    {
        edit_date = max_day;
    }

    if(time_dirty)
    {
        week = CalculateWeekDay(
            2000 + edit_year,
            edit_month,
            edit_date
        );

        RTC_Set_Time(
            edit_hour,
            edit_minute,
            edit_second,
            RTC_H12_AM
        );

        RTC_Set_Date(
            edit_year,
            edit_month,
            edit_date,
            week
        );
    }

    if(alarm_dirty)
    {
        RTC_AlarmConfig.hour =
            edit_alarm_hour;

        RTC_AlarmConfig.minute =
            edit_alarm_minute;

        RTC_AlarmConfig.enable =
            edit_alarm_enable;

        BSP_RTC_SaveAlarm(
            &RTC_AlarmConfig
        );

        BSP_RTC_SetDailyAlarm(
            RTC_AlarmConfig.hour,
            RTC_AlarmConfig.minute,
            RTC_AlarmConfig.enable
        );
    }

    system_mode = MODE_NORMAL;
    OLED_Clear();
}

void IncreaseCurrentValue(void)
{
    u8 max_day;

    switch(system_mode)
    {
        case MODE_SET_HOUR:

            edit_hour++;

            if(edit_hour > 23)
            {
                edit_hour = 0;
            }

            time_dirty = 1;
            break;

        case MODE_SET_MINUTE:

            edit_minute++;

            if(edit_minute > 59)
            {
                edit_minute = 0;
            }

            time_dirty = 1;
            break;

        case MODE_SET_SECOND:

            edit_second++;

            if(edit_second > 59)
            {
                edit_second = 0;
            }

            time_dirty = 1;
            break;

        case MODE_SET_YEAR:

            edit_year++;

            if(edit_year > 99)
            {
                edit_year = 0;
            }

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            if(edit_date > max_day)
            {
                edit_date = max_day;
            }

            time_dirty = 1;
            break;

        case MODE_SET_MONTH:

            edit_month++;

            if(edit_month > 12)
            {
                edit_month = 1;
            }

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            if(edit_date > max_day)
            {
                edit_date = max_day;
            }

            time_dirty = 1;
            break;

        case MODE_SET_DATE:

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            edit_date++;

            if(edit_date > max_day)
            {
                edit_date = 1;
            }

            time_dirty = 1;
            break;

        case MODE_SET_ALARM_HOUR:

            edit_alarm_hour++;

            if(edit_alarm_hour > 23)
            {
                edit_alarm_hour = 0;
            }

            alarm_dirty = 1;
            break;

        case MODE_SET_ALARM_MINUTE:

            edit_alarm_minute++;

            if(edit_alarm_minute > 59)
            {
                edit_alarm_minute = 0;
            }

            alarm_dirty = 1;
            break;

        case MODE_SET_ALARM_ENABLE:

            edit_alarm_enable =
                !edit_alarm_enable;

            alarm_dirty = 1;
            break;

        default:
            break;
    }
}

void DecreaseCurrentValue(void)
{
    u8 max_day;

    switch(system_mode)
    {
        case MODE_SET_HOUR:

            if(edit_hour == 0)
            {
                edit_hour = 23;
            }
            else
            {
                edit_hour--;
            }

            time_dirty = 1;
            break;

        case MODE_SET_MINUTE:

            if(edit_minute == 0)
            {
                edit_minute = 59;
            }
            else
            {
                edit_minute--;
            }

            time_dirty = 1;
            break;

        case MODE_SET_SECOND:

            if(edit_second == 0)
            {
                edit_second = 59;
            }
            else
            {
                edit_second--;
            }

            time_dirty = 1;
            break;

        case MODE_SET_YEAR:

            if(edit_year == 0)
            {
                edit_year = 99;
            }
            else
            {
                edit_year--;
            }

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            if(edit_date > max_day)
            {
                edit_date = max_day;
            }

            time_dirty = 1;
            break;

        case MODE_SET_MONTH:

            if(edit_month == 1)
            {
                edit_month = 12;
            }
            else
            {
                edit_month--;
            }

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            if(edit_date > max_day)
            {
                edit_date = max_day;
            }

            time_dirty = 1;
            break;

        case MODE_SET_DATE:

            max_day = GetDaysInMonth(
                2000 + edit_year,
                edit_month
            );

            if(edit_date == 1)
            {
                edit_date = max_day;
            }
            else
            {
                edit_date--;
            }

            time_dirty = 1;
            break;

        case MODE_SET_ALARM_HOUR:

            if(edit_alarm_hour == 0)
            {
                edit_alarm_hour = 23;
            }
            else
            {
                edit_alarm_hour--;
            }

            alarm_dirty = 1;
            break;

        case MODE_SET_ALARM_MINUTE:

            if(edit_alarm_minute == 0)
            {
                edit_alarm_minute = 59;
            }
            else
            {
                edit_alarm_minute--;
            }

            alarm_dirty = 1;
            break;

        case MODE_SET_ALARM_ENABLE:

            edit_alarm_enable =
                !edit_alarm_enable;

            alarm_dirty = 1;
            break;

        default:
            break;
    }
}

void NextSettingItem(void)
{
    switch(system_mode)
    {
        case MODE_NORMAL:
            system_mode = MODE_SET_HOUR;
            break;

        case MODE_SET_HOUR:
            system_mode = MODE_SET_MINUTE;
            break;

        case MODE_SET_MINUTE:
            system_mode = MODE_SET_SECOND;
            break;

        case MODE_SET_SECOND:
            system_mode = MODE_SET_YEAR;
            break;

        case MODE_SET_YEAR:
            system_mode = MODE_SET_MONTH;
            break;

        case MODE_SET_MONTH:
            system_mode = MODE_SET_DATE;
            break;

        case MODE_SET_DATE:
            system_mode = MODE_SET_ALARM_HOUR;
            break;

        case MODE_SET_ALARM_HOUR:
            system_mode = MODE_SET_ALARM_MINUTE;
            break;

        case MODE_SET_ALARM_MINUTE:
            system_mode = MODE_SET_ALARM_ENABLE;
            break;

        case MODE_SET_ALARM_ENABLE:
            system_mode = MODE_SET_HOUR;
            break;

        default:
            system_mode = MODE_NORMAL;
            break;
    }

    OLED_Clear();
}

void DisplayNormalPage(void)
{
    RTC_GetTime(
        RTC_Format_BIN,
        &RTC_TimeStruct
    );

    RTC_GetDate(
        RTC_Format_BIN,
        &RTC_DateStruct
    );

    sprintf(
        (char *)Rtc_Data,
        "Time:%02d:%02d:%02d ",
        RTC_TimeStruct.RTC_Hours,
        RTC_TimeStruct.RTC_Minutes,
        RTC_TimeStruct.RTC_Seconds
    );

    OLED_ShowString(
        0,
        0,
        Rtc_Data,
        16
    );

    sprintf(
        (char *)Rtc_Data,
        "Date:20%02d-%02d-%02d",
        RTC_DateStruct.RTC_Year,
        RTC_DateStruct.RTC_Month,
        RTC_DateStruct.RTC_Date
    );

    OLED_ShowString(
        0,
        2,
        Rtc_Data,
        16
    );
}

void DisplaySettingPage(void)
{
    switch(system_mode)
    {
        case MODE_SET_HOUR:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET TIME HOUR   ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_hour
            );
            break;

        case MODE_SET_MINUTE:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET TIME MIN    ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_minute
            );
            break;

        case MODE_SET_SECOND:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET TIME SEC    ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_second
            );
            break;

        case MODE_SET_YEAR:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET DATE YEAR   ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:20%02d      ",
                edit_year
            );
            break;

        case MODE_SET_MONTH:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET DATE MONTH  ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_month
            );
            break;

        case MODE_SET_DATE:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET DATE DAY    ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_date
            );
            break;

        case MODE_SET_ALARM_HOUR:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET ALARM HOUR  ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_alarm_hour
            );
            break;

        case MODE_SET_ALARM_MINUTE:

            OLED_ShowString(
                0,
                0,
                (u8 *)"SET ALARM MIN   ",
                16
            );

            sprintf(
                (char *)Rtc_Data,
                "VALUE:%02d        ",
                edit_alarm_minute
            );
            break;

        case MODE_SET_ALARM_ENABLE:

            OLED_ShowString(
                0,
                0,
                (u8 *)"ALARM SWITCH    ",
                16
            );

            if(edit_alarm_enable)
            {
                sprintf(
                    (char *)Rtc_Data,
                    "VALUE:ON        "
                );
            }
            else
            {
                sprintf(
                    (char *)Rtc_Data,
                    "VALUE:OFF       "
                );
            }
            break;

        default:
            return;
    }

    OLED_ShowString(
        0,
        2,
        Rtc_Data,
        16
    );
}

void Alarm_Process(void)
{
    u8 key;

    OLED_Clear();

    OLED_ShowString(
        0,
        0,
        (u8 *)"*** ALARM ***   ",
        16
    );

    OLED_ShowString(
        0,
        2,
        (u8 *)"PRESS ANY KEY   ",
        16
    );

    while(g_rtc_alarm_flag)
    {
        LED1_On();
        BEEP_On();

        delay_ms(200);

        key = BSP_KeyScan();

        if(key != KEY_NONE)
        {
            g_rtc_alarm_flag = 0;
            break;
        }

        LED1_Off();
        BEEP_Off();

        delay_ms(200);

        key = BSP_KeyScan();

        if(key != KEY_NONE)
        {
            g_rtc_alarm_flag = 0;
            break;
        }
    }

    LED1_Off();
    BEEP_Off();

    OLED_Clear();
}

int main(void)
{
    u8 key;
    u8 rtc_status;

    BSP_IO_Init();

    delay_ms(100);

    OLED_Init();
    OLED_Clear();

    OLED_ShowCHinese(28, 0, 0);
    delay_ms(200);

    OLED_ShowCHinese(46, 0, 1);
    delay_ms(200);

    OLED_ShowCHinese(64, 0, 2);
    delay_ms(200);

    OLED_ShowCHinese(82, 0, 3);
    delay_ms(500);

    OLED_Clear();

    rtc_status = BSP_RTC_Init();

    if(rtc_status != 0)
    {
        OLED_ShowString(
            0,
            0,
            (u8 *)"RTC INIT ERROR  ",
            16
        );

        OLED_ShowString(
            0,
            2,
            (u8 *)"CHECK LSE       ",
            16
        );

        while(1)
        {
        }
    }

    BSP_RTC_AlarmIRQ_Init();

    BSP_RTC_LoadAlarm(
        &RTC_AlarmConfig
    );

    BSP_RTC_SetDailyAlarm(
        RTC_AlarmConfig.hour,
        RTC_AlarmConfig.minute,
        RTC_AlarmConfig.enable
    );

    while(1)
    {
        if(g_rtc_alarm_flag)
        {
            Alarm_Process();
        }

        key = BSP_KeyScan();

        if(system_mode == MODE_NORMAL)
        {
            if(key == KEY1_VALUE)
            {
                LoadEditData();
                NextSettingItem();
            }
        }
        else
        {
            if(key == KEY1_VALUE)
            {
                NextSettingItem();
            }
            else if(key == KEY2_VALUE)
            {
                IncreaseCurrentValue();
            }
            else if(key == KEY3_VALUE)
            {
                DecreaseCurrentValue();
            }
            else if(key == KEY4_VALUE)
            {
                SaveEditData();
            }
        }

        if(system_mode == MODE_NORMAL)
        {
            DisplayNormalPage();
        }
        else
        {
            DisplaySettingPage();
        }

        delay_ms(100);
    }
}
