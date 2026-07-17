#ifndef __APP_ALARM_H__
#define __APP_ALARM_H__

#include "app_types.h"
#include <stdint.h>

void App_Alarm_Init(void);
AlarmLevel App_Alarm_Evaluate(uint16_t gas_value,
                              int16_t temp_x10,
                              uint8_t gps_valid,
                              uint8_t can_online,
                              uint32_t uptime_ms);
AlarmLevel App_Alarm_GetLevel(void);
void App_Alarm_ForceEmergency(void);
void App_Alarm_MuteByKey(void);
uint8_t App_Alarm_IsMuted(void);
void App_Alarm_ResetByAdmin(void);
const char *App_Alarm_Name(AlarmLevel level);
void App_Alarm_SetThresholds(uint16_t warn, uint16_t danger, uint16_t critical);
void App_Alarm_GetThresholds(uint16_t *warn, uint16_t *danger, uint16_t *critical);
void App_Alarm_ResetThresholds(void);

#endif
