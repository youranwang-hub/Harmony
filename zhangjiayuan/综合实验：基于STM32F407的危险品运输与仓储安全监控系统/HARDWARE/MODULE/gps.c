#include "gps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GpsFix s_fix;
static uint8_t s_replay = 1;
static uint8_t s_lost = 0;
static uint8_t s_deviation = 0;
static uint8_t s_route_index = 0;

static const int32_t route_lat_e7[] = {399084000, 399086000, 399091000, 399098000};
static const int32_t route_lon_e7[] = {1163970000, 1163990000, 1164020000, 1164070000};

static int32_t parse_coord_e7(const char *text, char hemi)
{
    double raw = atof(text);
    int deg = (int)(raw / 100.0);
    double minutes = raw - (double)(deg * 100);
    double dec = (double)deg + minutes / 60.0;
    int32_t result = (int32_t)(dec * 10000000.0);
    if (hemi == 'S' || hemi == 'W') result = -result;
    return result;
}

void App_GPS_Init(void)
{
    memset(&s_fix, 0, sizeof(s_fix));
    s_replay = 1U;
    s_lost = 0U;
    s_deviation = 0U;
    s_route_index = 0U;
    s_fix.replay = 1U;
    s_fix.valid = 0U;
}

void App_GPS_SetReplay(uint8_t enable)
{
    s_replay = enable ? 1U : 0U;
    s_fix.replay = s_replay;
}

void App_GPS_SetLost(uint8_t lost)
{
    s_lost = lost ? 1U : 0U;
    if (!lost) s_fix.lost_seconds = 0U;
}

void App_GPS_SetRouteDeviation(uint8_t enable)
{
    s_deviation = enable ? 1U : 0U;
}

void App_GPS_Task1000ms(void)
{
    if (s_lost) {
        s_fix.valid = 0U;
        s_fix.lost_seconds++;
        return;
    }
    if (s_replay) {
        s_fix.replay = 1U;
        s_fix.valid = 1U;
        s_fix.lost_seconds = 0U;
        s_fix.latitude_e7 = route_lat_e7[s_route_index];
        s_fix.longitude_e7 = route_lon_e7[s_route_index];
        if (s_deviation) {
            s_fix.latitude_e7 += 90000;
            s_fix.longitude_e7 += 90000;
        }
        s_fix.speed_x10 = 325;
        s_route_index = (uint8_t)((s_route_index + 1U) % 4U);
    } else if (!s_fix.valid) {
        s_fix.lost_seconds++;
    }
}

const GpsFix *App_GPS_GetFix(void)
{
    return &s_fix;
}

const char *App_GPS_SourceName(void)
{
    return s_fix.replay ? "REPLAY" : "REAL_NMEA";
}

const char *App_GPS_StatusName(void)
{
    if (s_lost || s_fix.lost_seconds > 0U) {
        return s_fix.replay ? "REPLAY_LOST" : "NOFIX";
    }
    if (s_fix.replay) {
        return s_fix.valid ? "REPLAY" : "REPLAY_WAIT";
    }
    return s_fix.valid ? "REAL_FIX" : "NOFIX";
}

uint8_t App_GPS_FeedNmea(const char *line)
{
    char buf[128];
    char *field[14];
    char *tok;
    uint8_t n = 0;
    strncpy(buf, line, sizeof(buf) - 1U);
    buf[sizeof(buf) - 1U] = '\0';
    tok = strtok(buf, ",");
    while (tok && n < 14U) {
        field[n++] = tok;
        tok = strtok(NULL, ",");
    }
    if (n > 6U && (strstr(field[0], "RMC") != 0)) {
        if (field[2][0] == 'A') {
            s_fix.valid = 1U;
            s_fix.replay = 0U;
            s_fix.lost_seconds = 0U;
            s_fix.latitude_e7 = parse_coord_e7(field[3], field[4][0]);
            s_fix.longitude_e7 = parse_coord_e7(field[5], field[6][0]);
            return 1U;
        }
        s_fix.valid = 0U;
        return 0U;
    }
    return 0U;
}

void App_GPS_Format(char *out, uint16_t out_len)
{
    snprintf(out, out_len, "GPS:%s src:%s lat:%ld.%07ld lon:%ld.%07ld lost:%us",
             App_GPS_StatusName(),
             App_GPS_SourceName(),
             (long)(s_fix.latitude_e7 / 10000000L),
             (long)labs(s_fix.latitude_e7 % 10000000L),
             (long)(s_fix.longitude_e7 / 10000000L),
             (long)labs(s_fix.longitude_e7 % 10000000L),
             s_fix.lost_seconds);
}
