#ifndef __GPS_REPLAY_H__
#define __GPS_REPLAY_H__
#include <stdint.h>
typedef struct { float lat; float lon; } GPS_Point;
void GPS_Replay_Start(void);
void GPS_Replay_Stop(void);
#endif
