#ifndef __APP_TYPES_H__
#define __APP_TYPES_H__

#include <stdint.h>

typedef enum {
    APP_MODE_REAL = 0,
    APP_MODE_MIXED,
    APP_MODE_SIM
} AppMode;

typedef enum {
    SYS_SELF_CHECK = 0,
    SYS_WAIT_LOGIN,
    SYS_IDLE,
    SYS_LOADING,
    SYS_READY,
    SYS_TRANSPORTING,
    SYS_PAUSED,
    SYS_WARNING,
    SYS_DANGER,
    SYS_HANDOVER,
    SYS_MAINTENANCE
} SystemState;

typedef enum {
    GAS_SOURCE_REAL_MQ2 = 0,
    GAS_SOURCE_POT,
    GAS_SOURCE_DAC_LOOP,
    GAS_SOURCE_SCENE,
    GAS_SOURCE_UART
} GasDataSource;

typedef enum {
    SCENE_NONE = 0,
    SCENE_NORMAL,
    SCENE_GAS_SLOW,
    SCENE_GAS_FAST,
    SCENE_GPS_LOST,
    SCENE_ROUTE_DEVIATE,
    SCENE_ILLEGAL_OPEN,
    SCENE_LOW_BATTERY,
    SCENE_CAN_OFFLINE
} SceneType;

typedef enum {
    ALARM_NONE = 0,
    ALARM_ATTENTION,
    ALARM_LEVEL1,
    ALARM_LEVEL2,
    ALARM_LEVEL3
} AlarmLevel;

typedef enum {
    UI_SELF_CHECK = 0,
    UI_AUTH,
    UI_HOME,
    UI_CARGO,
    UI_MONITOR,
    UI_DEVICE,
    UI_ALARM,
    UI_LOG,
    UI_SETTINGS,
    UI_SOURCE,
    UI_SCENE
} UiPage;

typedef enum {
    LOG_INFO = 0,
    LOG_AUTH,
    LOG_TRANSPORT,
    LOG_ALARM,
    LOG_FAULT,
    LOG_PARAM
} LogType;

typedef struct {
    int32_t latitude_e7;
    int32_t longitude_e7;
    uint16_t speed_x10;
    uint8_t valid;
    uint8_t replay;
    uint16_t lost_seconds;
} GpsFix;

typedef struct {
    uint16_t mq2_real;
    uint16_t potentiometer;
    uint16_t dac_loop;
    uint16_t scene_value;
    uint16_t uart_value;
    uint16_t effective_value;
    GasDataSource source;
} GasDataManager;

typedef struct {
    int16_t temperature_x10;
    uint8_t humidity;
    uint16_t gas_value;
    GasDataSource gas_source;
    uint16_t battery_mv;
    GpsFix gps;
    AlarmLevel alarm;
    SystemState state;
    AppMode mode;
    SceneType scene;
    uint8_t can_online;
    uint8_t authenticated;
    uint8_t admin;
    uint32_t uptime_ms;
} AppSnapshot;

typedef struct {
    uint16_t gas_warn;
    uint16_t gas_danger;
    uint16_t gas_critical;
    uint8_t default_source;
    uint16_t crc;
} AppParameters;

#endif
