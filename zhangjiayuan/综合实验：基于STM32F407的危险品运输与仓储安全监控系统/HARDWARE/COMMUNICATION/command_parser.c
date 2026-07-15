#include "command_parser.h"
#include "app_types.h"
#include "data_manager.h"
#include "gas_scene.h"
#include "gps.h"
#include "ui_main.h"
#include "log_manager.h"
#include "cargo_manager.h"
#include "alarm.h"
#include "fingerprint.h"
#include "xpt2046.h"
#include "usart_debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BACKUP_PASSWORD "25531460"

static void reply(const char *s)
{
    UART_SendString(USART2, s);
    UART_SendString(USART2, "\r\n");
}

static GasDataSource parse_source(const char *s)
{
    if (strcmp(s, "REAL") == 0 || strcmp(s, "REAL_MQ2") == 0) return GAS_SOURCE_REAL_MQ2;
    if (strcmp(s, "POT") == 0) return GAS_SOURCE_POT;
    if (strcmp(s, "DAC") == 0 || strcmp(s, "DAC_LOOP") == 0) return GAS_SOURCE_DAC_LOOP;
    if (strcmp(s, "SCENE") == 0) return GAS_SOURCE_SCENE;
    if (strcmp(s, "UART") == 0) return GAS_SOURCE_UART;
    return App_Gas_GetSource();
}

static SceneType parse_scene(const char *s)
{
    if (strcmp(s, "NORMAL") == 0) return SCENE_NORMAL;
    if (strcmp(s, "GAS_SLOW") == 0) return SCENE_GAS_SLOW;
    if (strcmp(s, "GAS_FAST") == 0) return SCENE_GAS_FAST;
    if (strcmp(s, "GPS_LOST") == 0) return SCENE_GPS_LOST;
    if (strcmp(s, "ROUTE_DEVIATE") == 0) return SCENE_ROUTE_DEVIATE;
    if (strcmp(s, "CAN_OFFLINE") == 0) return SCENE_CAN_OFFLINE;
    if (strcmp(s, "ILLEGAL_OPEN") == 0) return SCENE_ILLEGAL_OPEN;
    return SCENE_NONE;
}

static uint8_t cargo_has_pending(void)
{
    return (App_Cargo_Mode() != CARGO_MODE_NONE &&
            App_Cargo_PendingName()[0] != '\0' &&
            App_Cargo_PendingQuantity() != 0U) ? 1U : 0U;
}

static uint8_t process_backup_password(const char *password)
{
    uint8_t ok;

    if (password == 0 || strcmp(password, BACKUP_PASSWORD) != 0) {
        return 0U;
    }

    if (cargo_has_pending() != 0U) {
        if (Adapter_Auth_BackupVerify(password, 0U) == 0U) {
            reply("ERR PASS");
            return 1U;
        }
        ok = App_Cargo_Confirm("PASS");
        App_Log_Add(ok ? LOG_TRANSPORT : LOG_FAULT,
                    ok ? "cargo backup password confirm" : "cargo backup password failed");
        if (ok != 0U) {
            App_UI_SetPage(UI_HOME);
        } else {
            App_UI_SetPage(UI_CARGO);
        }
        reply(ok ? "OK PASS CARGO CONFIRM" : "ERR PASS CARGO CONFIRM");
        return 1U;
    }

    if (Adapter_Auth_BackupVerify(password, 1U) != 0U) {
        App_Log_Add(LOG_AUTH, "backup password verified");
        App_UI_SetPage(UI_HOME);
        reply("OK PASS HOME");
    } else {
        reply("ERR PASS");
    }

    return 1U;
}

void App_Cmd_ProcessLine(const char *line)
{
    char a[32] = {0};
    char b[32] = {0};
    char c[32] = {0};
    uint16_t v1, v2, v3;

    if (line == 0 || line[0] == '\0') return;

    if (strcmp(line, "HELP") == 0) {
        reply("Commands: HELP | REG NFC|FINGER|USER|ADMIN|RESET | AUTH NFC|FINGER|USER|ADMIN|RESET");
        reply("PASS 25531460 | PASSWORD 25531460 | direct backup auth and HOME");
        reply("PAGE HOME|MONITOR|SOURCE|SCENE|LOG|SETTING|AUTH|CARGO|DEVICE");
        reply("MODE REAL|MIXED|SIM | GAS SOURCE REAL_MQ2|POT|DAC_LOOP|SCENE|UART");
        reply("GAS SET <0-1000> | GAS RAMP <start> <end> <seconds>");
        reply("SCENE NORMAL|GAS_SLOW|GAS_FAST|GPS_LOST|ROUTE_DEVIATE|CAN_OFFLINE|RESET");
        reply("CARGO ADD <NAME> <QTY> | CARGO OUT <NAME> <QTY> | CARGO LIST | CARGO RESET");
        reply("GPS REPLAY|LOST|RECOVER | LOG EXPORT | ALARM RESET | TOUCH x y");
        return;
    }

    if (process_backup_password(line) != 0U) {
        return;
    }

    if (sscanf(line, "%31s %31s %31s", a, b, c) >= 1) {
        if (strcmp(a, "PASS") == 0 || strcmp(a, "PASSWORD") == 0) {
            if (process_backup_password(b) == 0U) {
                reply("ERR PASS");
            }
        } else if (strcmp(a, "REG") == 0) {
            uint8_t ok = 0U;
            if (strcmp(b, "NFC") == 0) ok = Adapter_Auth_Register(AUTH_METHOD_NFC, 0U);
            else if (strcmp(b, "FINGER") == 0 || strcmp(b, "FPR") == 0) ok = Adapter_Auth_Register(AUTH_METHOD_FINGER, 0U);
            else if (strcmp(b, "ADMIN") == 0) ok = Adapter_Auth_Register(AUTH_METHOD_BOTH, 1U);
            else if (strcmp(b, "RESET") == 0 || strcmp(b, "CLEAR") == 0) {
                Adapter_Auth_ClearRegistration();
                ok = 1U;
            } else ok = Adapter_Auth_Register(AUTH_METHOD_BOTH, 0U);

            if (ok != 0U) {
                App_Log_Add(LOG_AUTH, "identity registered");
                App_UI_SetPage(UI_AUTH);
                reply("OK REG");
            } else {
                reply("ERR REG FAILED");
            }
        } else if (strcmp(a, "AUTH") == 0) {
            uint8_t ok = 0U;
            if (strcmp(b, "NFC") == 0) ok = Adapter_Auth_Verify(AUTH_METHOD_NFC, 0U);
            else if (strcmp(b, "FINGER") == 0 || strcmp(b, "FPR") == 0) ok = Adapter_Auth_Verify(AUTH_METHOD_FINGER, 0U);
            else if (strcmp(b, "ADMIN") == 0) ok = Adapter_Auth_Login(AUTH_METHOD_BOTH, 1U);
            else if (strcmp(b, "RESET") == 0 || strcmp(b, "LOGOUT") == 0) {
                Adapter_Auth_Logout();
                App_UI_SetPage(UI_AUTH);
                reply("OK AUTH RESET");
                return;
            } else ok = Adapter_Auth_Login(AUTH_METHOD_BOTH, 0U);

            if (ok != 0U) {
                App_Log_Add(LOG_AUTH, Adapter_Auth_IsLoggedIn() ? "identity login complete" : "identity factor verified");
                if (Adapter_Auth_IsLoggedIn()) {
                    App_UI_SetPage(UI_HOME);
                    reply("OK AUTH LOGIN");
                } else {
                    App_UI_SetPage(UI_AUTH);
                    reply("OK AUTH FACTOR");
                }
            } else {
                App_UI_SetPage(UI_AUTH);
                reply("ERR AUTH NEED REG OR OTHER FACTOR");
            }
        } else if (strcmp(a, "PAGE") == 0) {
            if (strcmp(b, "MONITOR") == 0) App_UI_SetPage(UI_MONITOR);
            else if (strcmp(b, "SOURCE") == 0) App_UI_SetPage(UI_SOURCE);
            else if (strcmp(b, "SCENE") == 0) App_UI_SetPage(UI_SCENE);
            else if (strcmp(b, "LOG") == 0) App_UI_SetPage(UI_LOG);
            else if (strcmp(b, "SETTING") == 0 || strcmp(b, "SETTINGS") == 0) App_UI_SetPage(UI_SETTINGS);
            else if (strcmp(b, "AUTH") == 0) App_UI_SetPage(UI_AUTH);
            else if (strcmp(b, "CARGO") == 0) App_UI_SetPage(UI_CARGO);
            else if (strcmp(b, "DEVICE") == 0) App_UI_SetPage(UI_DEVICE);
            else App_UI_SetPage(UI_HOME);
            reply("OK PAGE");
        } else if (strcmp(a, "GAS") == 0 && strcmp(b, "SOURCE") == 0 && sscanf(line, "%31s %31s %31s", a, b, c) == 3) {
            App_Gas_SetSource(parse_source(c));
            App_Log_Add(LOG_PARAM, "gas source changed");
            reply("OK GAS SOURCE");
        } else if (strcmp(a, "GAS") == 0 && strcmp(b, "SET") == 0 && sscanf(line, "%31s %31s %hu", a, b, &v1) == 3) {
            App_Gas_SetUartValue(v1);
            reply("OK GAS SET");
        } else if (strcmp(a, "GAS") == 0 && strcmp(b, "RAMP") == 0 && sscanf(line, "%31s %31s %hu %hu %hu", a, b, &v1, &v2, &v3) == 5) {
            App_Gas_StartUartRamp(v1, v2, v3);
            reply("OK GAS RAMP");
        } else if (strcmp(a, "SCENE") == 0) {
            if (strcmp(b, "RESET") == 0) App_Scene_Stop();
            else App_Scene_Start(parse_scene(b));
            reply("OK SCENE");
        } else if (strcmp(a, "GPS") == 0) {
            if (strcmp(b, "LOST") == 0) App_GPS_SetLost(1);
            else if (strcmp(b, "RECOVER") == 0) App_GPS_SetLost(0);
            else if (strcmp(b, "REPLAY") == 0) App_GPS_SetReplay(1);
            reply("OK GPS");
        } else if (strcmp(a, "NMEA") == 0) {
            App_GPS_FeedNmea(line + 5);
            reply("OK NMEA");
        } else if (strcmp(a, "LOG") == 0 && strcmp(b, "EXPORT") == 0) {
            App_Log_Export();
        } else if (strcmp(a, "CARGO") == 0) {
            char list[128];
            if ((strcmp(b, "ADD") == 0 || strcmp(b, "OUT") == 0) &&
                sscanf(line, "%31s %31s %31s %hu", a, b, c, &v1) == 4) {
                if (strcmp(b, "ADD") == 0) {
                    if (App_Cargo_SetPending(CARGO_MODE_ADD, c, v1)) reply("OK CARGO ADD PENDING");
                    else reply("ERR CARGO ADD");
                } else {
                    if (App_Cargo_SetPending(CARGO_MODE_OUT, c, v1)) reply("OK CARGO OUT PENDING");
                    else reply("ERR CARGO OUT");
                }
                App_UI_SetPage(UI_CARGO);
            } else if (strcmp(b, "LIST") == 0) {
                App_Cargo_FormatList(list, sizeof(list));
                UART_SendString(USART2, "CARGO LIST ");
                UART_SendString(USART2, list);
                UART_SendString(USART2, "\r\n");
                App_UI_SetPage(UI_CARGO);
            } else if (strcmp(b, "RESET") == 0) {
                App_Cargo_Reset();
                App_Log_Add(LOG_PARAM, "cargo reset");
                App_UI_SetPage(UI_CARGO);
                reply("OK CARGO RESET");
            } else {
                reply("ERR CARGO CMD");
            }
        } else if (strcmp(a, "ALARM") == 0 && strcmp(b, "RESET") == 0) {
            if (Adapter_Auth_IsAdmin()) {
                App_Alarm_ResetByAdmin();
                reply("OK ALARM RESET");
            } else {
                reply("ERR ADMIN REQUIRED");
            }
        } else if (strcmp(a, "TOUCH") == 0 && sscanf(line, "%31s %hu %hu", a, &v1, &v2) == 3) {
            Adapter_Touch_Inject(v1, v2);
            reply("OK TOUCH");
        } else {
            reply("ERR UNKNOWN COMMAND, TYPE HELP");
        }
    }
}
