#include "gps_replay.h"
#include "gps.h"
static const GPS_Point g_route[] = {
    {39.9084f, 116.3970f},
    {39.9086f, 116.3990f},
    {39.9091f, 116.4020f},
    {39.9098f, 116.4070f},
};
void GPS_Replay_Start(void)
{
    (void)g_route;
    App_GPS_SetReplay(1U);
    App_GPS_SetLost(0U);
}
void GPS_Replay_Stop(void)
{
    App_GPS_SetReplay(0U);
}
