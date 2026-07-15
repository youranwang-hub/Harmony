#include "rtc.h"
#include "delay.h"      //简单延时

/*
 * 函数名称：RTC_Set_Time
 * 功能说明：设置RTC时间
 */
ErrorStatus RTC_Set_Time(u8 hour, u8 min, u8 sec, u8 ampm)
{
    RTC_TimeTypeDef RTC_TimeTypeInitStructure;

    RTC_TimeTypeInitStructure.RTC_Hours = hour;
    RTC_TimeTypeInitStructure.RTC_Minutes = min;
    RTC_TimeTypeInitStructure.RTC_Seconds = sec;
    RTC_TimeTypeInitStructure.RTC_H12 = ampm;

    return RTC_SetTime(RTC_Format_BIN, &RTC_TimeTypeInitStructure);
}

/*
 * 函数名称：RTC_Set_Date
 * 功能说明：设置RTC日期
 */
ErrorStatus RTC_Set_Date(u8 year, u8 month, u8 date, u8 week)
{
    RTC_DateTypeDef RTC_DateTypeInitStructure;

    RTC_DateTypeInitStructure.RTC_Date = date;
    RTC_DateTypeInitStructure.RTC_Month = month;
    RTC_DateTypeInitStructure.RTC_WeekDay = week;
    RTC_DateTypeInitStructure.RTC_Year = year;

    return RTC_SetDate(RTC_Format_BIN, &RTC_DateTypeInitStructure);
}

/*
 * 函数名称：BSP_RTC_Init
 * 功能说明：初始化STM32F407的RTC
 */
u8 BSP_RTC_Init(void)
{
    RTC_InitTypeDef RTC_InitStructure;
    u16 retry = 0X1FFF;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);  //使能PWR时钟
    PWR_BackupAccessCmd(ENABLE);                         //使能后备寄存器访问

    /*
     * 根据BKP寄存器中的值，判断是否是第一次设置RTC
     * 或者是否没有安装RTC后备电池
     */
    if (RTC_ReadBackupRegister(RTC_BKP_DR0) != 0x5050)
    {
        RCC_LSEConfig(RCC_LSE_ON);       //开启LSE

        /*
         * 检查指定的RCC标志位设置与否，
         * 等待外部低速晶振就绪
         */
        while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
        {
            retry++;
            delay_ms(10);
        }

        if (retry == 0)
        {
            return 1;                   //LSE开启失败，返回1
        }

        /*
         * 设置RTC时钟RTCCLK，
         * 选择LSE作为RTC时钟源
         */
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        RCC_RTCCLKCmd(ENABLE);           //使能RTC时钟

        RTC_InitStructure.RTC_AsynchPrediv = 0x7F;       //RTC异步分频系数
        RTC_InitStructure.RTC_SynchPrediv = 0xFF;        //RTC同步分频系数
        RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;

        RTC_Init(&RTC_InitStructure);

        RTC_Set_Time(13, 00, 00, RTC_H12_AM);            //设置时间
        RTC_Set_Date(21, 12, 15, 3);                     //设置日期

        /*
         * RTC已经配置完成，
         * 将标志保存到RTC_BKP_DR0中
         */
        RTC_WriteBackupRegister(RTC_BKP_DR0, 0x5050);
    }

    return 0;
}