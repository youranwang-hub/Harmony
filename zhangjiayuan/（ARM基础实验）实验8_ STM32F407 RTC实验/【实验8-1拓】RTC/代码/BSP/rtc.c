#include "rtc.h"
#include "delay.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_exti.h"
#include "misc.h"

/* RTC初始化完成标志 */
#define RTC_INIT_MARK          0x5050

/* 闹钟数据有效标志 */
#define RTC_ALARM_MARK         0xA55A

/*
 * 第一次运行时设置的初始时间：
 * 2026-07-13 13:30:00
 *
 * 第一次运行时设置的默认闹钟：
 * 13:32
 *
 * 初始时间和闹钟相差2分钟，方便验证。
 */
#define DEFAULT_TIME_HOUR      13
#define DEFAULT_TIME_MINUTE    30
#define DEFAULT_TIME_SECOND    0

#define DEFAULT_ALARM_HOUR     13
#define DEFAULT_ALARM_MINUTE   32
#define DEFAULT_ALARM_ENABLE   1

/* RTC闹钟触发标志 */
volatile u8 g_rtc_alarm_flag = 0;


/*
 * 函数名称：RTC_Set_Time
 * 功能说明：设置RTC时间
 */
ErrorStatus RTC_Set_Time(
    u8 hour,
    u8 min,
    u8 sec,
    u8 ampm
)
{
    RTC_TimeTypeDef RTC_TimeStructure;

    RTC_TimeStructure.RTC_Hours = hour;
    RTC_TimeStructure.RTC_Minutes = min;
    RTC_TimeStructure.RTC_Seconds = sec;
    RTC_TimeStructure.RTC_H12 = ampm;

    return RTC_SetTime(
        RTC_Format_BIN,
        &RTC_TimeStructure
    );
}


/*
 * 函数名称：RTC_Set_Date
 * 功能说明：设置RTC日期
 */
ErrorStatus RTC_Set_Date(
    u8 year,
    u8 month,
    u8 date,
    u8 week
)
{
    RTC_DateTypeDef RTC_DateStructure;

    RTC_DateStructure.RTC_Year = year;
    RTC_DateStructure.RTC_Month = month;
    RTC_DateStructure.RTC_Date = date;
    RTC_DateStructure.RTC_WeekDay = week;

    return RTC_SetDate(
        RTC_Format_BIN,
        &RTC_DateStructure
    );
}


/*
 * 函数名称：BSP_RTC_Init
 * 功能说明：初始化RTC
 *
 * 第一次运行时：
 * 1. 开启LSE
 * 2. 设置RTC时钟和分频
 * 3. 设置初始时间、日期
 * 4. 写入后备寄存器标志
 *
 * 后续运行时：
 * 不重新设置时间，只同步RTC寄存器
 */
u8 BSP_RTC_Init(void)
{
    RTC_InitTypeDef RTC_InitStructure;
    u16 retry = 0;

    /* 开启PWR外设时钟 */
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_PWR,
        ENABLE
    );

    /* 允许访问RTC和后备寄存器 */
    PWR_BackupAccessCmd(ENABLE);

    /*
     * RTC_BKP_DR0中没有0x5050：
     * 表示第一次运行，或者后备数据已经丢失
     */
    if(
        RTC_ReadBackupRegister(RTC_BKP_DR0)
        != RTC_INIT_MARK
    )
    {
        /* 开启外部32.768kHz低速晶振 */
        RCC_LSEConfig(RCC_LSE_ON);

        /* 等待LSE晶振就绪 */
        while(
            RCC_GetFlagStatus(RCC_FLAG_LSERDY)
            == RESET
            &&
            retry < 500
        )
        {
            retry++;
            delay_ms(10);
        }

        /* LSE在规定时间内没有启动 */
        if(
            RCC_GetFlagStatus(RCC_FLAG_LSERDY)
            == RESET
        )
        {
            return 1;
        }

        /* 选择LSE作为RTC时钟源 */
        RCC_RTCCLKConfig(
            RCC_RTCCLKSource_LSE
        );

        /* 开启RTC时钟 */
        RCC_RTCCLKCmd(ENABLE);

        /* 等待RTC影子寄存器同步 */
        if(RTC_WaitForSynchro() == ERROR)
        {
            return 1;
        }

        /*
         * RTC时钟分频：
         * 32768 / 128 / 256 = 1Hz
         */
        RTC_InitStructure.RTC_AsynchPrediv =
            0x7F;

        RTC_InitStructure.RTC_SynchPrediv =
            0xFF;

        RTC_InitStructure.RTC_HourFormat =
            RTC_HourFormat_24;

        /* 初始化RTC */
        if(RTC_Init(&RTC_InitStructure) == ERROR)
        {
            return 1;
        }

        /*
         * 设置初始时间：
         * 13:30:00
         */
        if(
            RTC_Set_Time(
                DEFAULT_TIME_HOUR,
                DEFAULT_TIME_MINUTE,
                DEFAULT_TIME_SECOND,
                RTC_H12_AM
            ) == ERROR
        )
        {
            return 1;
        }

        /*
         * 设置初始日期：
         * 2026-07-13，星期一
         *
         * RTC年份寄存器只保存后两位，
         * 因此写入26。
         */
        if(
            RTC_Set_Date(
                26,
                7,
                13,
                RTC_Weekday_Monday
            ) == ERROR
        )
        {
            return 1;
        }

        /* 写入RTC已经初始化的标志 */
        RTC_WriteBackupRegister(
            RTC_BKP_DR0,
            RTC_INIT_MARK
        );
    }
    else
    {
        /*
         * RTC已经初始化过，
         * 不再重新设置时间和日期
         */
        RCC_LSEConfig(RCC_LSE_ON);
        RCC_RTCCLKCmd(ENABLE);

        /* 等待RTC影子寄存器同步 */
        if(RTC_WaitForSynchro() == ERROR)
        {
            return 1;
        }
    }

    return 0;
}


/*
 * 函数名称：BSP_RTC_SaveAlarm
 * 功能说明：将闹钟参数保存到后备寄存器
 */
void BSP_RTC_SaveAlarm(
    RTC_AlarmConfigTypeDef *alarm
)
{
    /* 允许访问后备寄存器 */
    PWR_BackupAccessCmd(ENABLE);

    /* 保存闹钟小时 */
    RTC_WriteBackupRegister(
        RTC_BKP_DR1,
        alarm->hour
    );

    /* 保存闹钟分钟 */
    RTC_WriteBackupRegister(
        RTC_BKP_DR2,
        alarm->minute
    );

    /* 保存闹钟开关状态 */
    RTC_WriteBackupRegister(
        RTC_BKP_DR3,
        alarm->enable
    );

    /* 保存闹钟参数有效标志 */
    RTC_WriteBackupRegister(
        RTC_BKP_DR4,
        RTC_ALARM_MARK
    );
}


/*
 * 函数名称：BSP_RTC_LoadAlarm
 * 功能说明：从后备寄存器读取闹钟参数
 */
void BSP_RTC_LoadAlarm(
    RTC_AlarmConfigTypeDef *alarm
)
{
    u32 alarm_mark;

    alarm_mark = RTC_ReadBackupRegister(
        RTC_BKP_DR4
    );

    /*
     * 闹钟标志无效：
     * 使用默认闹钟并写入后备寄存器
     */
    if(alarm_mark != RTC_ALARM_MARK)
    {
        alarm->hour =
            DEFAULT_ALARM_HOUR;

        alarm->minute =
            DEFAULT_ALARM_MINUTE;

        alarm->enable =
            DEFAULT_ALARM_ENABLE;

        BSP_RTC_SaveAlarm(alarm);
    }
    else
    {
        /* 读取已保存的闹钟小时 */
        alarm->hour =
            (u8)RTC_ReadBackupRegister(
                RTC_BKP_DR1
            );

        /* 读取已保存的闹钟分钟 */
        alarm->minute =
            (u8)RTC_ReadBackupRegister(
                RTC_BKP_DR2
            );

        /* 读取已保存的闹钟开关 */
        alarm->enable =
            (u8)RTC_ReadBackupRegister(
                RTC_BKP_DR3
            );

        /* 检查小时数据是否合法 */
        if(alarm->hour > 23)
        {
            alarm->hour =
                DEFAULT_ALARM_HOUR;
        }

        /* 检查分钟数据是否合法 */
        if(alarm->minute > 59)
        {
            alarm->minute =
                DEFAULT_ALARM_MINUTE;
        }

        /* 检查闹钟开关是否合法 */
        if(alarm->enable > 1)
        {
            alarm->enable =
                DEFAULT_ALARM_ENABLE;
        }
    }
}


/*
 * 函数名称：BSP_RTC_AlarmIRQ_Init
 * 功能说明：配置RTC Alarm A中断
 *
 * RTC Alarm连接到EXTI Line17
 */
void BSP_RTC_AlarmIRQ_Init(void)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 清除EXTI Line17原有标志 */
    EXTI_ClearITPendingBit(
        EXTI_Line17
    );

    /* 配置RTC Alarm的EXTI中断线 */
    EXTI_InitStructure.EXTI_Line =
        EXTI_Line17;

    EXTI_InitStructure.EXTI_Mode =
        EXTI_Mode_Interrupt;

    EXTI_InitStructure.EXTI_Trigger =
        EXTI_Trigger_Rising;

    EXTI_InitStructure.EXTI_LineCmd =
        ENABLE;

    EXTI_Init(&EXTI_InitStructure);

    /* 配置RTC Alarm中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel =
        RTC_Alarm_IRQn;

    NVIC_InitStructure
        .NVIC_IRQChannelPreemptionPriority =
        1;

    NVIC_InitStructure
        .NVIC_IRQChannelSubPriority =
        1;

    NVIC_InitStructure
        .NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(&NVIC_InitStructure);
}


/*
 * 函数名称：BSP_RTC_SetDailyAlarm
 * 功能说明：设置每天固定时间触发的RTC Alarm A
 *
 * hour：闹钟小时，0～23
 * minute：闹钟分钟，0～59
 * enable：1开启，0关闭
 */
ErrorStatus BSP_RTC_SetDailyAlarm(
    u8 hour,
    u8 minute,
    u8 enable
)
{
    RTC_AlarmTypeDef RTC_AlarmStructure;
    ErrorStatus status;

    /* 清除软件闹钟触发标志 */
    g_rtc_alarm_flag = 0;

    /* 修改闹钟前先关闭Alarm A */
    RTC_AlarmCmd(
        RTC_Alarm_A,
        DISABLE
    );

    /* 关闭Alarm A中断 */
    RTC_ITConfig(
        RTC_IT_ALRA,
        DISABLE
    );

    /* 清除RTC Alarm A中断标志 */
    RTC_ClearITPendingBit(
        RTC_IT_ALRA
    );

    /* 清除EXTI Line17中断标志 */
    EXTI_ClearITPendingBit(
        EXTI_Line17
    );

    /* enable为0，只关闭闹钟，不再重新配置 */
    if(enable == 0)
    {
        return SUCCESS;
    }

    /* 检查闹钟时间是否合法 */
    if(hour > 23 || minute > 59)
    {
        return ERROR;
    }

    /* 初始化闹钟结构体 */
    RTC_AlarmStructInit(
        &RTC_AlarmStructure
    );

    /* 设置闹钟小时 */
    RTC_AlarmStructure
        .RTC_AlarmTime
        .RTC_Hours = hour;

    /* 设置闹钟分钟 */
    RTC_AlarmStructure
        .RTC_AlarmTime
        .RTC_Minutes = minute;

    /*
     * 闹钟在指定时间的00秒触发
     */
    RTC_AlarmStructure
        .RTC_AlarmTime
        .RTC_Seconds = 0;

    RTC_AlarmStructure
        .RTC_AlarmTime
        .RTC_H12 = RTC_H12_AM;

    /*
     * 屏蔽日期和星期比较。
     *
     * 只比较时、分、秒，
     * 因此闹钟每天同一时刻触发。
     */
    RTC_AlarmStructure.RTC_AlarmMask =
        RTC_AlarmMask_DateWeekDay;

    RTC_AlarmStructure
        .RTC_AlarmDateWeekDaySel =
        RTC_AlarmDateWeekDaySel_Date;

    /*
     * 由于日期已经被屏蔽，
     * 此处的日期值不会参与比较。
     */
    RTC_AlarmStructure
        .RTC_AlarmDateWeekDay = 1;

    /*
     * 注意：
     * RTC_SetAlarm()返回类型为void，
     * 不能赋值给ErrorStatus变量。
     */
    RTC_SetAlarm(
        RTC_Format_BIN,
        RTC_Alarm_A,
        &RTC_AlarmStructure
    );

    /* 再次清除可能残留的中断标志 */
    RTC_ClearITPendingBit(
        RTC_IT_ALRA
    );

    EXTI_ClearITPendingBit(
        EXTI_Line17
    );

    /* 开启Alarm A中断 */
    RTC_ITConfig(
        RTC_IT_ALRA,
        ENABLE
    );

    /*
     * RTC_AlarmCmd()返回ErrorStatus，
     * 使用它判断Alarm A是否成功启用。
     */
    status = RTC_AlarmCmd(
        RTC_Alarm_A,
        ENABLE
    );

    return status;
}


/*
 * RTC Alarm A中断服务函数
 *
 * 闹钟到达后只设置标志变量，
 * 蜂鸣器、LED和OLED处理放在main.c中执行。
 */
void RTC_Alarm_IRQHandler(void)
{
    if(
        RTC_GetITStatus(RTC_IT_ALRA)
        != RESET
    )
    {
        /* 通知主程序处理闹钟 */
        g_rtc_alarm_flag = 1;

        /* 清除RTC Alarm A中断标志 */
        RTC_ClearITPendingBit(
            RTC_IT_ALRA
        );

        /* 清除EXTI Line17中断标志 */
        EXTI_ClearITPendingBit(
            EXTI_Line17
        );
    }
}
