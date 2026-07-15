#include "alarm.h"
#include "system_config.h"
#include "gpio_control.h"
#include "pwm_control.h"
#include "can_protocol.h"
#include "log_manager.h"

static AlarmLevel s_level = ALARM_NONE;
static uint8_t s_gas_critical_count = 0;
static uint8_t s_forced = 0;
static uint8_t s_muted = 0;
static uint16_t s_warn_enter = APP_GAS_WARN_ENTER;
static uint16_t s_danger_enter = APP_GAS_DANGER_ENTER;
static uint16_t s_critical_enter = APP_GAS_CRITICAL_ENTER;

void App_Alarm_Init(void)
{
    s_level = ALARM_NONE;
    s_gas_critical_count = 0;
    s_forced = 0;
    s_muted = 0;
    App_Alarm_ResetThresholds();
}

const char *App_Alarm_Name(AlarmLevel level)
{
    switch (level) {
    case ALARM_ATTENTION: return "ATTENTION";
    case ALARM_LEVEL1: return "LEVEL1";
    case ALARM_LEVEL2: return "LEVEL2";
    case ALARM_LEVEL3: return "LEVEL3";
    default: return "NONE";
    }
}

static AlarmLevel evaluate_gas_level(uint16_t gas_value, int16_t temp_x10)
{
    AlarmLevel next = s_level;

    if (gas_value >= s_critical_enter) {
        if (s_gas_critical_count < 10U) {
            s_gas_critical_count++;
        }
    } else if (gas_value <= APP_GAS_CRITICAL_EXIT) {
        s_gas_critical_count = 0U;
    }

    if (s_gas_critical_count >= 3U || (s_level == ALARM_LEVEL3 && gas_value > APP_GAS_CRITICAL_EXIT)) {
        return ALARM_LEVEL3;
    }

    if (gas_value >= s_danger_enter || temp_x10 >= APP_TEMP_DANGER_X10) {
        return ALARM_LEVEL2;
    }
    if (s_level == ALARM_LEVEL2 && (gas_value > APP_GAS_DANGER_EXIT || temp_x10 >= APP_TEMP_WARN_X10)) {
        return ALARM_LEVEL2;
    }

    if (gas_value >= s_warn_enter || temp_x10 >= APP_TEMP_WARN_X10) {
        return ALARM_LEVEL1;
    }
    if (s_level == ALARM_LEVEL1 && gas_value > APP_GAS_WARN_EXIT) {
        return ALARM_LEVEL1;
    }

    if (gas_value >= APP_GAS_ATTENTION_ENTER) {
        return ALARM_ATTENTION;
    }
    if (s_level == ALARM_ATTENTION && gas_value > APP_GAS_ATTENTION_EXIT) {
        return ALARM_ATTENTION;
    }

    next = ALARM_NONE;
    return next;
}

AlarmLevel App_Alarm_Evaluate(uint16_t gas_value,
                              int16_t temp_x10,
                              uint8_t gps_valid,
                              uint8_t can_online,
                              uint32_t uptime_ms)
{
    AlarmLevel next = ALARM_NONE;
    (void)gps_valid;
    (void)can_online;

    if (s_forced) {
        next = ALARM_LEVEL3;
    } else if (uptime_ms < APP_GAS_STARTUP_PROTECT_MS) {
        next = ALARM_NONE;
        s_gas_critical_count = 0U;
    } else {
        next = evaluate_gas_level(gas_value, temp_x10);
    }

    if (next != s_level) {
        if (next >= ALARM_LEVEL3) {
            s_muted = 0U;
        } else if (next < ALARM_LEVEL3) {
            s_muted = 0U;
        }
        s_level = next;
        if (s_level >= ALARM_LEVEL2) {
            Board_CAN_SendAlarm((uint8_t)s_level, gas_value);
            App_Log_Add(LOG_ALARM, App_Alarm_Name(s_level));
        }
    }

    Board_LED_Set(BOARD_LED_GREEN, s_level == ALARM_NONE);
    Board_LED_Set(BOARD_LED_RED, s_level >= ALARM_LEVEL2);
    Board_Buzzer_Set((s_level >= ALARM_LEVEL3) && (s_muted == 0U));
    if (s_level == ALARM_NONE) Board_PWM_SetFanPercent(20);
    else if (s_level == ALARM_ATTENTION) Board_PWM_SetFanPercent(40);
    else if (s_level == ALARM_LEVEL1) Board_PWM_SetFanPercent(60);
    else if (s_level == ALARM_LEVEL2) Board_PWM_SetFanPercent(80);
    else Board_PWM_SetFanPercent(100);
    return s_level;
}

AlarmLevel App_Alarm_GetLevel(void)
{
    return s_level;
}

void App_Alarm_ForceEmergency(void)
{
    s_forced = 1U;
    s_muted = 0U;
    s_level = ALARM_LEVEL3;
    Board_Buzzer_Set(1U);
    Board_LED_Set(BOARD_LED_RED, 1U);
    Board_CAN_SendAlarm((uint8_t)s_level, 999U);
    App_Log_Add(LOG_ALARM, "manual emergency exti");
}

void App_Alarm_MuteByKey(void)
{
    if (s_level >= ALARM_LEVEL3) {
        s_muted = 1U;
        Board_Buzzer_Set(0U);
        App_Log_Add(LOG_ALARM, "alarm muted by key");
    }
}

uint8_t App_Alarm_IsMuted(void)
{
    return s_muted;
}

void App_Alarm_ResetByAdmin(void)
{
    s_forced = 0U;
    s_muted = 0U;
    s_level = ALARM_NONE;
    s_gas_critical_count = 0U;
    Board_Buzzer_Set(0U);
    Board_LED_Set(BOARD_LED_RED, 0U);
    App_Log_Add(LOG_ALARM, "alarm reset by admin");
}

void App_Alarm_SetThresholds(uint16_t warn, uint16_t danger, uint16_t critical)
{
    if (warn < 100U) warn = 100U;
    if (warn > 900U) warn = 900U;
    if (danger <= warn) danger = (uint16_t)(warn + 1U);
    if (danger > 950U) danger = 950U;
    if (critical <= danger) critical = (uint16_t)(danger + 1U);
    if (critical > APP_ADC_GAS_MAX) critical = APP_ADC_GAS_MAX;

    s_warn_enter = warn;
    s_danger_enter = danger;
    s_critical_enter = critical;
}

void App_Alarm_GetThresholds(uint16_t *warn, uint16_t *danger, uint16_t *critical)
{
    if (warn != 0) *warn = s_warn_enter;
    if (danger != 0) *danger = s_danger_enter;
    if (critical != 0) *critical = s_critical_enter;
}

void App_Alarm_ResetThresholds(void)
{
    s_warn_enter = APP_GAS_WARN_ENTER;
    s_danger_enter = APP_GAS_DANGER_ENTER;
    s_critical_enter = APP_GAS_CRITICAL_ENTER;
}
