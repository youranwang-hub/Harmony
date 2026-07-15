#ifndef __SYSTEM_CONFIG_H__
#define __SYSTEM_CONFIG_H__

#include "stm32f4xx.h"

#define APP_NAME                    "Hazmat Transport Monitor"
#define APP_VERSION                 "2026.07.15"
#define APP_UART_BAUD               115200U
#define APP_UI_REFRESH_MS           500U
#define APP_LOG_PERIOD_MS           5000U

#define APP_ENABLE_REAL_LCD         1
#define APP_ENABLE_REAL_TOUCH       1
#define APP_ENABLE_REAL_DHT11       0
#define APP_ENABLE_REAL_FINGERPRINT 1
#define APP_ENABLE_REAL_NFC         1
#define APP_ENABLE_REAL_EEPROM      0
#define APP_ENABLE_REAL_SPI_FLASH   0

#define APP_DAC_LOOP_FALLBACK_TO_SETPOINT 1

#define APP_ADC_GAS_MAX             1000U
#define APP_GAS_NORMAL_MAX          399U
#define APP_GAS_ATTENTION_ENTER     400U
#define APP_GAS_ATTENTION_EXIT      350U
#define APP_GAS_ATTENTION_MAX       549U
#define APP_GAS_WARN_ENTER          550U
#define APP_GAS_WARN_EXIT           500U
#define APP_GAS_DANGER_ENTER        680U
#define APP_GAS_DANGER_EXIT         600U
#define APP_GAS_CRITICAL_ENTER      780U
#define APP_GAS_CRITICAL_EXIT       700U

#define APP_TEMP_WARN_X10           320
#define APP_TEMP_DANGER_X10         350
#define APP_HUM_WARN                70U
#define APP_HUM_DANGER              80U

#define APP_ADC_CH_MQ2_REAL         ADC_Channel_12  /* PC2 */
#define APP_ADC_CH_POT              ADC_Channel_11  /* PC1 */
#define APP_ADC_CH_DAC_LOOP         ADC_Channel_1   /* PA1, connect from PA4 DAC via 1k-4.7k */
#define APP_ADC_CH_BATTERY          ADC_Channel_13  /* PC3 */

#define APP_LOG_MAX_RECORDS         64U
#define APP_UART_LINE_MAX           128U
#define APP_EXPORT_BUFFER_SIZE      3072U

#define APP_GAS_STARTUP_PROTECT_MS  30000U
#define APP_EMERGENCY_IGNORE_MS     3000U
#define APP_EMERGENCY_DEBOUNCE_MS   800U

#endif
