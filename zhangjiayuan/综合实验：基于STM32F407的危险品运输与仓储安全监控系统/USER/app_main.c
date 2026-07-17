#include "app_main.h"
#include "system_config.h"
#include "data_manager.h"
#include "gps.h"
#include "alarm.h"
#include "gas_scene.h"
#include "ui_main.h"
#include "command_parser.h"
#include "log_manager.h"
#include "cargo_manager.h"
#include "fingerprint.h"
#include "dht11.h"
#include "eeprom.h"
#include "xpt2046.h"
#include "gpio_control.h"
#include "adc_sensor.h"
#include "dac_output.h"
#include "pwm_control.h"
#include "can_protocol.h"
#include "rtc_manager.h"
#include "dma_uart.h"
#include "usart_debug.h"
#include <stdio.h>
#include <string.h>

static volatile uint32_t s_ms = 0;
static AppSnapshot s_snapshot;
static AppParameters s_params;
static uint32_t s_last_sensor = 0;
static uint32_t s_last_ui = 0;
static uint32_t s_last_touch = 0;
static uint32_t s_last_1s = 0;
static uint32_t s_last_log = 0;
static uint32_t s_last_key = 0;
static uint8_t s_key_latch = 0;
static uint8_t s_ignore_next_key_mute = 0;
static uint8_t s_status_valid = 0;
static AppSnapshot s_last_reported_snapshot;
static uint8_t s_last_reported_muted = 0;

#define APP_STATUS_GAS_REPORT_STEP 20U

static void self_check(void)
{
    UART_SendString(USART2, "\r\n[SELF] GPIO USART ADC DAC RTC CAN DMA adapters OK\r\n");
    Board_LED_Set(BOARD_LED_GREEN, 1U);
}

void App_Init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    UART2_Init(APP_UART_BAUD);
    UART_SendString(USART2, "\r\n=== Hazmat Transport Monitor boot ===\r\n");
    Board_GPIO_Init();
    Board_ADC_Init();
    Board_DAC_Init();
    Board_PWM_Init();
    Board_RTC_Init();
    Board_CAN_Init();
    Board_DMA_USART2_Init();
    Adapter_EEPROM_Init();
    Adapter_Auth_Init();
    Adapter_DHT11_Init();
    Adapter_Touch_Init();
    App_Log_Init();
    App_Cargo_Init();
    if (!Adapter_EEPROM_Load(&s_params)) {
        s_params.gas_warn = APP_GAS_WARN_ENTER;
        s_params.gas_danger = APP_GAS_DANGER_ENTER;
        s_params.gas_critical = APP_GAS_CRITICAL_ENTER;
        s_params.default_source = GAS_SOURCE_POT;
        s_params.crc = Adapter_EEPROM_Crc(&s_params);
        Adapter_EEPROM_Save(&s_params);
    }
    App_Gas_Init((GasDataSource)s_params.default_source);
    App_GPS_Init();
    App_Alarm_Init();
    App_Alarm_SetThresholds(s_params.gas_warn,
                            s_params.gas_danger,
                            s_params.gas_critical);
    App_Scene_Init();
    App_UI_Init();
    s_snapshot.state = SYS_SELF_CHECK;
    s_snapshot.mode = APP_MODE_MIXED;
    self_check();
    App_UI_SetPage(UI_AUTH);
    s_snapshot.state = SYS_WAIT_LOGIN;
    Board_Timer_Init();
    UART_SendString(USART2, "Type HELP for commands.\r\n");
}

void App_OnSysTick(void)
{
    s_ms++;
}

void App_OnEmergencyButton(void)
{
    static uint32_t s_last_emergency_ms = 0;

    if (s_ms < APP_EMERGENCY_IGNORE_MS) {
        return;
    }
    if ((uint32_t)(s_ms - s_last_emergency_ms) < APP_EMERGENCY_DEBOUNCE_MS) {
        return;
    }
    s_last_emergency_ms = s_ms;
    if (App_Alarm_GetLevel() >= ALARM_LEVEL3) {
        App_Alarm_MuteByKey();
        return;
    }
    App_Alarm_ForceEmergency();
    s_ignore_next_key_mute = 1U;
}

const AppSnapshot *App_GetSnapshot(void)
{
    return &s_snapshot;
}

static void update_snapshot(void)
{
    const GasDataManager *gas = App_Gas_Get();
    const GpsFix *gps = App_GPS_GetFix();
    s_snapshot.uptime_ms = s_ms;
    s_snapshot.gas_value = gas->effective_value;
    s_snapshot.gas_source = gas->source;
    s_snapshot.battery_mv = Board_ADC_ReadBatteryMv();
    s_snapshot.gps = *gps;
    s_snapshot.can_online = Board_CAN_IsOnline();
    s_snapshot.authenticated = Adapter_Auth_IsLoggedIn();
    s_snapshot.admin = Adapter_Auth_IsAdmin();
    s_snapshot.alarm = App_Alarm_GetLevel();
    s_snapshot.scene = App_Scene_Current();
    if (s_snapshot.alarm >= ALARM_LEVEL2) s_snapshot.state = SYS_DANGER;
    else if (s_snapshot.authenticated) s_snapshot.state = SYS_TRANSPORTING;
    else s_snapshot.state = SYS_WAIT_LOGIN;
}

static void sensor_task(void)
{
    Adapter_DHT11_Read(&s_snapshot.temperature_x10, &s_snapshot.humidity);
    App_Gas_Update();
    update_snapshot();
    s_snapshot.alarm = App_Alarm_Evaluate(s_snapshot.gas_value,
                                          s_snapshot.temperature_x10,
                                          s_snapshot.gps.valid,
                                          s_snapshot.can_online,
                                          s_snapshot.uptime_ms);
}

static void task_1s(void)
{
    App_Gas_Task1000ms();
    App_GPS_Task1000ms();
    App_Scene_Task1000ms();
    Board_CAN_SendHeartbeat((uint8_t)s_snapshot.state, s_snapshot.gas_value);
}

static uint16_t diff_u16(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint8_t status_has_changed(void)
{
    if (s_status_valid == 0U) {
        return 1U;
    }

    if (s_snapshot.alarm != s_last_reported_snapshot.alarm ||
        s_snapshot.gas_source != s_last_reported_snapshot.gas_source ||
        s_snapshot.state != s_last_reported_snapshot.state ||
        s_snapshot.scene != s_last_reported_snapshot.scene ||
        s_snapshot.can_online != s_last_reported_snapshot.can_online ||
        s_snapshot.gps.valid != s_last_reported_snapshot.gps.valid ||
        s_snapshot.gps.replay != s_last_reported_snapshot.gps.replay ||
        s_snapshot.authenticated != s_last_reported_snapshot.authenticated ||
        s_snapshot.admin != s_last_reported_snapshot.admin ||
        App_Alarm_IsMuted() != s_last_reported_muted) {
        return 1U;
    }

    if (diff_u16(s_snapshot.gas_value,
                 s_last_reported_snapshot.gas_value) >= APP_STATUS_GAS_REPORT_STEP) {
        return 1U;
    }

    return 0U;
}

static void report_status_if_changed(uint8_t force)
{
    char text[128];

    if (force == 0U && status_has_changed() == 0U) {
        return;
    }

    snprintf(text, sizeof(text),
             "[STAT] gas=%u src=%s alarm=%s mute=%c scene=%s gps=%s can=%s auth=%u\r\n",
             s_snapshot.gas_value,
             App_Gas_SourceName(s_snapshot.gas_source),
             App_Alarm_Name(s_snapshot.alarm),
             App_Alarm_IsMuted() ? 'Y' : 'N',
             App_Scene_Name(s_snapshot.scene),
             App_GPS_StatusName(),
             Board_CAN_StatusName(),
             s_snapshot.authenticated);
    UART_SendString(USART2, text);

    s_last_reported_snapshot = s_snapshot;
    s_last_reported_muted = App_Alarm_IsMuted();
    s_status_valid = 1U;
}

static void key_task(void)
{
    uint8_t key_mask = 0U;
    char text[48];

    if (Board_Key_Read(1U) != 0U) key_mask |= 0x01U;
    if (Board_Key_Read(2U) != 0U) key_mask |= 0x02U;
    if (Board_Key_Read(3U) != 0U) key_mask |= 0x04U;
    if (Board_Key_Read(4U) != 0U) key_mask |= 0x08U;

    if (key_mask != 0U && s_key_latch == 0U) {
        snprintf(text, sizeof(text), "[KEY] mask=0x%02X\r\n", key_mask);
        UART_SendString(USART2, text);

        if (s_ignore_next_key_mute != 0U && (key_mask & 0x01U) != 0U) {
            s_ignore_next_key_mute = 0U;
        } else if (App_Alarm_GetLevel() >= ALARM_LEVEL3) {
            App_Alarm_MuteByKey();
        }
        report_status_if_changed(1U);
    }

    if (key_mask == 0U) {
        s_ignore_next_key_mute = 0U;
    }

    s_key_latch = key_mask;
}

void App_Run(void)
{
    char line[APP_UART_LINE_MAX];
    TouchEvent touch;

    if (UART2_ReadLine(line, sizeof(line))) {
        App_Cmd_ProcessLine(line);
    }

    if ((uint32_t)(s_ms - s_last_touch) >= 50U) {
        s_last_touch = s_ms;
        if (Adapter_Touch_GetEvent(&touch)) {
            App_UI_HandleTouch(touch.x, touch.y);
        }
    }

    if ((uint32_t)(s_ms - s_last_key) >= 50U) {
        s_last_key = s_ms;
        key_task();
    }

    if ((uint32_t)(s_ms - s_last_sensor) >= 500U) {
        s_last_sensor = s_ms;
        sensor_task();
        report_status_if_changed(0U);
    }

    if ((uint32_t)(s_ms - s_last_1s) >= 1000U) {
        s_last_1s = s_ms;
        task_1s();
    }

    if ((uint32_t)(s_ms - s_last_ui) >= APP_UI_REFRESH_MS) {
        s_last_ui = s_ms;
        App_UI_Render(&s_snapshot);
    }

    if ((uint32_t)(s_ms - s_last_log) >= APP_LOG_PERIOD_MS) {
        s_last_log = s_ms;
        App_Log_Add(LOG_TRANSPORT, "periodic snapshot");
    }
}
