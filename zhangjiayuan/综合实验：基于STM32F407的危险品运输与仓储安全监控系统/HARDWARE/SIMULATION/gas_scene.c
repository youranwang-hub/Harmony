#include "gas_scene.h"
#include "data_manager.h"
#include "gps.h"
#include "dac_output.h"
#include "can_protocol.h"
#include "dht11.h"
#include "log_manager.h"
#include "system_config.h"

static SceneType s_scene = SCENE_NONE;
static uint16_t s_elapsed = 0;

void App_Scene_Init(void)
{
    s_scene = SCENE_NONE;
    s_elapsed = 0;
}

const char *App_Scene_Name(SceneType scene)
{
    switch (scene) {
    case SCENE_NORMAL: return "NORMAL";
    case SCENE_GAS_SLOW: return "GAS_SLOW";
    case SCENE_GAS_FAST: return "GAS_FAST";
    case SCENE_GPS_LOST: return "GPS_LOST";
    case SCENE_ROUTE_DEVIATE: return "ROUTE_DEVIATE";
    case SCENE_ILLEGAL_OPEN: return "ILLEGAL_OPEN";
    case SCENE_LOW_BATTERY: return "LOW_BATTERY";
    case SCENE_CAN_OFFLINE: return "CAN_OFFLINE";
    default: return "NONE";
    }
}

void App_Scene_Start(SceneType scene)
{
    s_scene = scene;
    s_elapsed = 0U;
    App_Log_Add(LOG_INFO, App_Scene_Name(scene));
    if (scene == SCENE_NORMAL) {
        App_Gas_SetSource(GAS_SOURCE_POT);
        App_GPS_SetLost(0);
        App_GPS_SetRouteDeviation(0);
        Board_CAN_SetVirtualOnline(1);
        Adapter_DHT11_SetOffset(0, 0);
    } else if (scene == SCENE_GAS_SLOW || scene == SCENE_GAS_FAST) {
        App_Gas_SetSource(GAS_SOURCE_DAC_LOOP);
    } else if (scene == SCENE_GPS_LOST) {
        App_GPS_SetLost(1);
    } else if (scene == SCENE_ROUTE_DEVIATE) {
        App_GPS_SetRouteDeviation(1);
    } else if (scene == SCENE_CAN_OFFLINE) {
        Board_CAN_SetVirtualOnline(0);
    } else if (scene == SCENE_LOW_BATTERY) {
        Adapter_DHT11_SetOffset(60, 8);
    }
}

void App_Scene_Stop(void)
{
    s_scene = SCENE_NONE;
    s_elapsed = 0U;
    App_GPS_SetLost(0);
    App_GPS_SetRouteDeviation(0);
    Board_CAN_SetVirtualOnline(1);
    Adapter_DHT11_SetOffset(0, 0);
}

void App_Scene_Task1000ms(void)
{
    uint16_t value = 0;
    if (s_scene == SCENE_NONE) return;
    s_elapsed++;
    if (s_scene == SCENE_GAS_SLOW) {
        if (s_elapsed <= 10U) value = 300U;
        else if (s_elapsed <= 40U) value = (uint16_t)(300U + (s_elapsed - 10U) * 15U);
        else if (s_elapsed <= 55U) value = (uint16_t)(750U + (s_elapsed - 40U) * 6U);
        else value = 360U;
        if (value > 850U) value = 850U;
        Board_DAC_SetGasValue(value);
        App_Gas_SetSceneValue(value);
    } else if (s_scene == SCENE_GAS_FAST) {
        value = (s_elapsed < 20U) ? (uint16_t)(320U + s_elapsed * 28U) : 850U;
        if (value > 900U) value = 900U;
        Board_DAC_SetGasValue(value);
        App_Gas_SetSceneValue(value);
    } else if (s_scene == SCENE_ILLEGAL_OPEN) {
        if (s_elapsed == 1U) App_Log_Add(LOG_ALARM, "illegal box open simulated");
    }
}

SceneType App_Scene_Current(void)
{
    return s_scene;
}
