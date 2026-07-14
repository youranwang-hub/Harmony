#include "stm32f4xx.h"

void BSP_RTC_Init(void)
{
	RTC_InitTypeDef  RTC_InitStruct;
	RTC_TimeTypeDef  RTC_TimeStruct;
	RTC_DateTypeDef  RTC_DateStruct;

	// 1. 使能 PWR 时钟, 解除备份域写保护
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// 2. 使能 LSE 32.768kHz 晶振
	RCC_LSEConfig(RCC_LSE_ON);
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET){}

	// 3. 选择 LSE 为 RTC 时钟源
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	RCC_RTCCLKCmd(ENABLE);
	RTC_WaitForSynchro();

	// 4. 配置预分频: 异步127 + 同步255 → 32768/(128*256)=1Hz
	RTC_InitStruct.RTC_HourFormat   = RTC_HourFormat_24;
	RTC_InitStruct.RTC_AsynchPrediv = 127;
	RTC_InitStruct.RTC_SynchPrediv  = 255;
	RTC_Init(&RTC_InitStruct);

	// 5. 首次上电(备份域数据丢失)时设置默认时间 2024-01-01 00:00:00
	if(RTC_ReadBackupRegister(RTC_BKP_DR0) != 0xA5A5)
	{
		RTC_TimeStruct.RTC_Hours   = 0;
		RTC_TimeStruct.RTC_Minutes = 0;
		RTC_TimeStruct.RTC_Seconds = 0;
		RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);

		RTC_DateStruct.RTC_Year  = 24;   // 2024
		RTC_DateStruct.RTC_Month = 1;
		RTC_DateStruct.RTC_Date  = 1;
		RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);

		RTC_WriteBackupRegister(RTC_BKP_DR0, 0xA5A5);
	}
}
