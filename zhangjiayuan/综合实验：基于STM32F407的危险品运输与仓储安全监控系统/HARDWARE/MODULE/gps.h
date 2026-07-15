#ifndef __APP_GPS_H__
#define __APP_GPS_H__

#include "app_types.h"
#include <stdint.h>

void App_GPS_Init(void);
void App_GPS_Task1000ms(void);
const GpsFix *App_GPS_GetFix(void);
void App_GPS_SetReplay(uint8_t enable);
void App_GPS_SetLost(uint8_t lost);
void App_GPS_SetRouteDeviation(uint8_t enable);
uint8_t App_GPS_FeedNmea(const char *line);
void App_GPS_Format(char *out, uint16_t out_len);
const char *App_GPS_StatusName(void);
const char *App_GPS_SourceName(void);

#endif
