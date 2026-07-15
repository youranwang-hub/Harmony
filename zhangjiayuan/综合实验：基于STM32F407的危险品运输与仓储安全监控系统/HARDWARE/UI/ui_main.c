#include "ui_main.h"
#include "system_config.h"
#include "lcd.h"
#include "data_manager.h"
#include "alarm.h"
#include "gas_scene.h"
#include "log_manager.h"
#include "cargo_manager.h"
#include "gps.h"
#include "can_protocol.h"
#include "fingerprint.h"
#include "gpio_control.h"
#include "pwm_control.h"
#include "dac_output.h"
#include "eeprom.h"
#include "usart_debug.h"
#include "delay.h"
#include <stdio.h>

#define UI_TOUCH_TOP_BAR_Y      40U
#define UI_TOUCH_BOTTOM_NAV_Y   280U

#define UI_BTN_LEFT_X           4U
#define UI_BTN_RIGHT_X          124U
#define UI_BTN_W                112U
#define UI_BTN_H                42U
#define UI_BTN_ROW0_Y           48U
#define UI_BTN_ROW1_Y           96U
#define UI_BTN_ROW2_Y           144U
#define UI_BTN_ROW3_Y           192U
#define UI_MAX_BUTTONS          8U
#define UI_THRESHOLD_STEP       10U

typedef enum {
    UI_ACT_NONE = 0,
    UI_ACT_HOME,
    UI_ACT_PAGE_MONITOR,
    UI_ACT_PAGE_SOURCE,
    UI_ACT_PAGE_SCENE,
    UI_ACT_PAGE_LOG,
    UI_ACT_PAGE_SETTING,
    UI_ACT_PAGE_AUTH,
    UI_ACT_PAGE_CARGO,
    UI_ACT_PAGE_DEVICE,

    UI_ACT_AUTH_REG_NFC,
    UI_ACT_AUTH_REG_FPR,
    UI_ACT_AUTH_VER_NFC,
    UI_ACT_AUTH_VER_FPR,
    UI_ACT_AUTH_REG_BOTH,
    UI_ACT_AUTH_CLEAR,

    UI_ACT_SOURCE_POT,
    UI_ACT_SOURCE_DAC,
    UI_ACT_SOURCE_SCENE,
    UI_ACT_SOURCE_UART,
    UI_ACT_SOURCE_REAL,

    UI_ACT_SCENE_NORMAL,
    UI_ACT_SCENE_GAS_SLOW,
    UI_ACT_SCENE_GAS_FAST,
    UI_ACT_SCENE_GPS_LOST,
    UI_ACT_SCENE_ROUTE_DEV,
    UI_ACT_SCENE_CAN_OFF,
    UI_ACT_SCENE_ILLEGAL,
    UI_ACT_SCENE_RESET,

    UI_ACT_LOG_PREV,
    UI_ACT_LOG_NEXT,
    UI_ACT_LOG_EXPORT,
    UI_ACT_LOG_CLEAR,

    UI_ACT_SET_L1_INC,
    UI_ACT_SET_L1_DEC,
    UI_ACT_SET_L2_INC,
    UI_ACT_SET_L2_DEC,
    UI_ACT_SET_L3_INC,
    UI_ACT_SET_L3_DEC,
    UI_ACT_SET_SAVE,
    UI_ACT_SET_DEFAULT,

    UI_ACT_CARGO_ADD,
    UI_ACT_CARGO_OUT,
    UI_ACT_CARGO_LIST,
    UI_ACT_CARGO_NFC_OK,
    UI_ACT_CARGO_FPR_OK,
    UI_ACT_CARGO_CANCEL,

    UI_ACT_DEV_BEEP,
    UI_ACT_DEV_LED_G,
    UI_ACT_DEV_LED_R,
    UI_ACT_DEV_FAN25,
    UI_ACT_DEV_FAN75,
    UI_ACT_DEV_DAC300,
    UI_ACT_DEV_DAC850,
    UI_ACT_DEV_ALARM_RST
} UiAction;

typedef struct {
    LcdButton lcd;
    UiAction action;
} UiActionButton;

static UiPage s_page = UI_SELF_CHECK;
static uint16_t s_log_start = 0U;

#define BTN_L(row,label,color,act) {{UI_BTN_LEFT_X,  (uint16_t)(UI_BTN_ROW0_Y + (row) * 48U), UI_BTN_W, UI_BTN_H, label, color}, act}
#define BTN_R(row,label,color,act) {{UI_BTN_RIGHT_X, (uint16_t)(UI_BTN_ROW0_Y + (row) * 48U), UI_BTN_W, UI_BTN_H, label, color}, act}

static const UiActionButton s_home_buttons[] = {
    BTN_L(0U, "1 MONITOR", LCD_BTN_CYAN,   UI_ACT_PAGE_MONITOR),
    BTN_R(0U, "2 SOURCE",  LCD_BTN_CYAN,   UI_ACT_PAGE_SOURCE),
    BTN_L(1U, "3 SCENE",   LCD_BTN_GREEN,  UI_ACT_PAGE_SCENE),
    BTN_R(1U, "4 LOG",     LCD_BTN_GREEN,  UI_ACT_PAGE_LOG),
    BTN_L(2U, "5 SETTING", LCD_BTN_YELLOW, UI_ACT_PAGE_SETTING),
    BTN_R(2U, "6 AUTH",    LCD_BTN_YELLOW, UI_ACT_PAGE_AUTH),
    BTN_L(3U, "7 CARGO",   LCD_BTN_BLUE,   UI_ACT_PAGE_CARGO),
    BTN_R(3U, "8 DEVICE",  LCD_BTN_BLUE,   UI_ACT_PAGE_DEVICE)
};

static const UiActionButton s_auth_buttons[] = {
    BTN_L(0U, "REG NFC",   LCD_BTN_CYAN,   UI_ACT_AUTH_REG_NFC),
    BTN_R(0U, "REG FPR",   LCD_BTN_CYAN,   UI_ACT_AUTH_REG_FPR),
    BTN_L(1U, "VER NFC",   LCD_BTN_GREEN,  UI_ACT_AUTH_VER_NFC),
    BTN_R(1U, "VER FPR",   LCD_BTN_GREEN,  UI_ACT_AUTH_VER_FPR),
    BTN_L(2U, "REG BOTH",  LCD_BTN_YELLOW, UI_ACT_AUTH_REG_BOTH),
    BTN_R(2U, "CLEAR",     LCD_BTN_RED,    UI_ACT_AUTH_CLEAR)
};

static const UiActionButton s_monitor_buttons[] = {
    BTN_L(0U, "SOURCE",  LCD_BTN_CYAN,   UI_ACT_PAGE_SOURCE),
    BTN_R(0U, "SCENE",   LCD_BTN_GREEN,  UI_ACT_PAGE_SCENE),
    BTN_L(1U, "DEVICE",  LCD_BTN_BLUE,   UI_ACT_PAGE_DEVICE),
    BTN_R(1U, "LOG",     LCD_BTN_GREEN,  UI_ACT_PAGE_LOG),
    BTN_L(2U, "SETTING", LCD_BTN_YELLOW, UI_ACT_PAGE_SETTING),
    BTN_R(2U, "CARGO",   LCD_BTN_BLUE,   UI_ACT_PAGE_CARGO),
    BTN_L(3U, "AUTH",    LCD_BTN_YELLOW, UI_ACT_PAGE_AUTH),
    BTN_R(3U, "HOME",    LCD_BTN_CYAN,   UI_ACT_HOME)
};

static const UiActionButton s_source_buttons[] = {
    BTN_L(0U, "POT",    LCD_BTN_CYAN,   UI_ACT_SOURCE_POT),
    BTN_R(0U, "DAC",    LCD_BTN_CYAN,   UI_ACT_SOURCE_DAC),
    BTN_L(1U, "SCENE",  LCD_BTN_GREEN,  UI_ACT_SOURCE_SCENE),
    BTN_R(1U, "UART",   LCD_BTN_GREEN,  UI_ACT_SOURCE_UART),
    BTN_L(2U, "REAL",   LCD_BTN_YELLOW, UI_ACT_SOURCE_REAL),
    BTN_R(2U, "HOME",   LCD_BTN_BLUE,   UI_ACT_HOME)
};

static const UiActionButton s_scene_buttons[] = {
    BTN_L(0U, "NORMAL",    LCD_BTN_CYAN,   UI_ACT_SCENE_NORMAL),
    BTN_R(0U, "GAS SLOW",  LCD_BTN_GREEN,  UI_ACT_SCENE_GAS_SLOW),
    BTN_L(1U, "GAS FAST",  LCD_BTN_GREEN,  UI_ACT_SCENE_GAS_FAST),
    BTN_R(1U, "GPS LOST",  LCD_BTN_YELLOW, UI_ACT_SCENE_GPS_LOST),
    BTN_L(2U, "ROUTE DEV", LCD_BTN_YELLOW, UI_ACT_SCENE_ROUTE_DEV),
    BTN_R(2U, "CAN OFF",   LCD_BTN_RED,    UI_ACT_SCENE_CAN_OFF),
    BTN_L(3U, "ILLEGAL",   LCD_BTN_RED,    UI_ACT_SCENE_ILLEGAL),
    BTN_R(3U, "RESET",     LCD_BTN_BLUE,   UI_ACT_SCENE_RESET)
};

static const UiActionButton s_log_buttons[] = {
    BTN_L(0U, "PREV",   LCD_BTN_CYAN,   UI_ACT_LOG_PREV),
    BTN_R(0U, "NEXT",   LCD_BTN_CYAN,   UI_ACT_LOG_NEXT),
    BTN_L(1U, "EXPORT", LCD_BTN_GREEN,  UI_ACT_LOG_EXPORT),
    BTN_R(1U, "CLEAR",  LCD_BTN_RED,    UI_ACT_LOG_CLEAR),
    BTN_L(2U, "HOME",   LCD_BTN_BLUE,   UI_ACT_HOME)
};

static const UiActionButton s_setting_buttons[] = {
    BTN_L(0U, "L1+",     LCD_BTN_CYAN,   UI_ACT_SET_L1_INC),
    BTN_R(0U, "L1-",     LCD_BTN_CYAN,   UI_ACT_SET_L1_DEC),
    BTN_L(1U, "L2+",     LCD_BTN_GREEN,  UI_ACT_SET_L2_INC),
    BTN_R(1U, "L2-",     LCD_BTN_GREEN,  UI_ACT_SET_L2_DEC),
    BTN_L(2U, "L3+",     LCD_BTN_YELLOW, UI_ACT_SET_L3_INC),
    BTN_R(2U, "L3-",     LCD_BTN_YELLOW, UI_ACT_SET_L3_DEC),
    BTN_L(3U, "SAVE",    LCD_BTN_BLUE,   UI_ACT_SET_SAVE),
    BTN_R(3U, "DEFAULT", LCD_BTN_RED,    UI_ACT_SET_DEFAULT)
};

static const UiActionButton s_cargo_buttons[] = {
    BTN_L(0U, "ADD",    LCD_BTN_CYAN,   UI_ACT_CARGO_ADD),
    BTN_R(0U, "OUT",    LCD_BTN_YELLOW, UI_ACT_CARGO_OUT),
    BTN_L(1U, "LIST",   LCD_BTN_GREEN,  UI_ACT_CARGO_LIST),
    BTN_R(1U, "NFC OK", LCD_BTN_GREEN,  UI_ACT_CARGO_NFC_OK),
    BTN_L(2U, "FPR OK", LCD_BTN_GREEN,  UI_ACT_CARGO_FPR_OK),
    BTN_R(2U, "CANCEL", LCD_BTN_RED,    UI_ACT_CARGO_CANCEL),
    BTN_L(3U, "HOME",   LCD_BTN_BLUE,   UI_ACT_HOME)
};

static const UiActionButton s_device_buttons[] = {
    BTN_L(0U, "BEEP",      LCD_BTN_CYAN,   UI_ACT_DEV_BEEP),
    BTN_R(0U, "LED G",     LCD_BTN_GREEN,  UI_ACT_DEV_LED_G),
    BTN_L(1U, "LED R",     LCD_BTN_RED,    UI_ACT_DEV_LED_R),
    BTN_R(1U, "FAN 25",    LCD_BTN_GREEN,  UI_ACT_DEV_FAN25),
    BTN_L(2U, "FAN 75",    LCD_BTN_YELLOW, UI_ACT_DEV_FAN75),
    BTN_R(2U, "DAC 300",   LCD_BTN_CYAN,   UI_ACT_DEV_DAC300),
    BTN_L(3U, "DAC 850",   LCD_BTN_RED,    UI_ACT_DEV_DAC850),
    BTN_R(3U, "ALARM RST", LCD_BTN_BLUE,   UI_ACT_DEV_ALARM_RST)
};

static const UiActionButton s_alarm_buttons[] = {
    BTN_L(0U, "ALARM RST", LCD_BTN_BLUE, UI_ACT_DEV_ALARM_RST),
    BTN_R(0U, "LOG",       LCD_BTN_GREEN, UI_ACT_PAGE_LOG),
    BTN_L(1U, "DEVICE",    LCD_BTN_CYAN, UI_ACT_PAGE_DEVICE),
    BTN_R(1U, "HOME",      LCD_BTN_CYAN, UI_ACT_HOME)
};

static const char *page_name(UiPage page)
{
    switch (page) {
    case UI_SELF_CHECK: return "SELF CHECK";
    case UI_AUTH: return "AUTH";
    case UI_HOME: return "HOME";
    case UI_CARGO: return "CARGO";
    case UI_MONITOR: return "MONITOR";
    case UI_DEVICE: return "DEVICE";
    case UI_ALARM: return "ALARM";
    case UI_LOG: return "LOG";
    case UI_SETTINGS: return "SETTINGS";
    case UI_SOURCE: return "SOURCE";
    case UI_SCENE: return "SCENE";
    default: return "UNKNOWN";
    }
}

static const char *action_name(UiAction action)
{
    switch (action) {
    case UI_ACT_HOME: return "HOME";
    case UI_ACT_PAGE_MONITOR: return "PAGE MONITOR";
    case UI_ACT_PAGE_SOURCE: return "PAGE SOURCE";
    case UI_ACT_PAGE_SCENE: return "PAGE SCENE";
    case UI_ACT_PAGE_LOG: return "PAGE LOG";
    case UI_ACT_PAGE_SETTING: return "PAGE SETTING";
    case UI_ACT_PAGE_AUTH: return "PAGE AUTH";
    case UI_ACT_PAGE_CARGO: return "PAGE CARGO";
    case UI_ACT_PAGE_DEVICE: return "PAGE DEVICE";
    case UI_ACT_AUTH_REG_NFC: return "REG NFC";
    case UI_ACT_AUTH_REG_FPR: return "REG FPR";
    case UI_ACT_AUTH_VER_NFC: return "VER NFC";
    case UI_ACT_AUTH_VER_FPR: return "VER FPR";
    case UI_ACT_AUTH_REG_BOTH: return "REG BOTH";
    case UI_ACT_AUTH_CLEAR: return "CLEAR AUTH";
    case UI_ACT_SOURCE_POT: return "SOURCE POT";
    case UI_ACT_SOURCE_DAC: return "SOURCE DAC";
    case UI_ACT_SOURCE_SCENE: return "SOURCE SCENE";
    case UI_ACT_SOURCE_UART: return "SOURCE UART";
    case UI_ACT_SOURCE_REAL: return "SOURCE REAL";
    case UI_ACT_SCENE_NORMAL: return "SCENE NORMAL";
    case UI_ACT_SCENE_GAS_SLOW: return "SCENE GAS SLOW";
    case UI_ACT_SCENE_GAS_FAST: return "SCENE GAS FAST";
    case UI_ACT_SCENE_GPS_LOST: return "SCENE GPS LOST";
    case UI_ACT_SCENE_ROUTE_DEV: return "SCENE ROUTE DEV";
    case UI_ACT_SCENE_CAN_OFF: return "SCENE CAN OFF";
    case UI_ACT_SCENE_ILLEGAL: return "SCENE ILLEGAL";
    case UI_ACT_SCENE_RESET: return "SCENE RESET";
    case UI_ACT_LOG_PREV: return "LOG PREV";
    case UI_ACT_LOG_NEXT: return "LOG NEXT";
    case UI_ACT_LOG_EXPORT: return "LOG EXPORT";
    case UI_ACT_LOG_CLEAR: return "LOG CLEAR";
    case UI_ACT_SET_L1_INC: return "L1+";
    case UI_ACT_SET_L1_DEC: return "L1-";
    case UI_ACT_SET_L2_INC: return "L2+";
    case UI_ACT_SET_L2_DEC: return "L2-";
    case UI_ACT_SET_L3_INC: return "L3+";
    case UI_ACT_SET_L3_DEC: return "L3-";
    case UI_ACT_SET_SAVE: return "SETTING SAVE";
    case UI_ACT_SET_DEFAULT: return "SETTING DEFAULT";
    case UI_ACT_CARGO_ADD: return "CARGO ADD MODE";
    case UI_ACT_CARGO_OUT: return "CARGO OUT MODE";
    case UI_ACT_CARGO_LIST: return "CARGO LIST";
    case UI_ACT_CARGO_NFC_OK: return "CARGO NFC OK";
    case UI_ACT_CARGO_FPR_OK: return "CARGO FPR OK";
    case UI_ACT_CARGO_CANCEL: return "CARGO CANCEL";
    case UI_ACT_DEV_BEEP: return "DEVICE BEEP";
    case UI_ACT_DEV_LED_G: return "DEVICE LED G";
    case UI_ACT_DEV_LED_R: return "DEVICE LED R";
    case UI_ACT_DEV_FAN25: return "DEVICE FAN25";
    case UI_ACT_DEV_FAN75: return "DEVICE FAN75";
    case UI_ACT_DEV_DAC300: return "DEVICE DAC300";
    case UI_ACT_DEV_DAC850: return "DEVICE DAC850";
    case UI_ACT_DEV_ALARM_RST: return "DEVICE ALARM RESET";
    default: return "NONE";
    }
}

static uint8_t array_count_u8(uint16_t bytes, uint16_t item_size)
{
    return (uint8_t)(bytes / item_size);
}

static const UiActionButton *page_buttons(UiPage page, uint8_t *count)
{
    switch (page) {
    case UI_AUTH:
    case UI_SELF_CHECK:
        *count = array_count_u8(sizeof(s_auth_buttons), sizeof(s_auth_buttons[0]));
        return s_auth_buttons;
    case UI_HOME:
        *count = array_count_u8(sizeof(s_home_buttons), sizeof(s_home_buttons[0]));
        return s_home_buttons;
    case UI_MONITOR:
        *count = array_count_u8(sizeof(s_monitor_buttons), sizeof(s_monitor_buttons[0]));
        return s_monitor_buttons;
    case UI_SOURCE:
        *count = array_count_u8(sizeof(s_source_buttons), sizeof(s_source_buttons[0]));
        return s_source_buttons;
    case UI_SCENE:
        *count = array_count_u8(sizeof(s_scene_buttons), sizeof(s_scene_buttons[0]));
        return s_scene_buttons;
    case UI_LOG:
        *count = array_count_u8(sizeof(s_log_buttons), sizeof(s_log_buttons[0]));
        return s_log_buttons;
    case UI_SETTINGS:
        *count = array_count_u8(sizeof(s_setting_buttons), sizeof(s_setting_buttons[0]));
        return s_setting_buttons;
    case UI_CARGO:
        *count = array_count_u8(sizeof(s_cargo_buttons), sizeof(s_cargo_buttons[0]));
        return s_cargo_buttons;
    case UI_DEVICE:
        *count = array_count_u8(sizeof(s_device_buttons), sizeof(s_device_buttons[0]));
        return s_device_buttons;
    case UI_ALARM:
        *count = array_count_u8(sizeof(s_alarm_buttons), sizeof(s_alarm_buttons[0]));
        return s_alarm_buttons;
    default:
        *count = 0U;
        return 0;
    }
}

static uint8_t copy_lcd_buttons(const UiActionButton *src, uint8_t count, LcdButton *dst)
{
    uint8_t i;

    if (src == 0 || dst == 0) {
        return 0U;
    }

    for (i = 0U; i < count && i < UI_MAX_BUTTONS; i++) {
        dst[i] = src[i].lcd;
    }

    return i;
}

static UiAction hit_action(uint16_t x, uint16_t y)
{
    uint8_t i;
    uint8_t count;
    const UiActionButton *buttons = page_buttons(s_page, &count);

    for (i = 0U; i < count; i++) {
        const LcdButton *b = &buttons[i].lcd;
        if (x >= b->x &&
            x < (uint16_t)(b->x + b->w) &&
            y >= b->y &&
            y < (uint16_t)(b->y + b->h)) {
            return buttons[i].action;
        }
    }

    return UI_ACT_NONE;
}

static void ui_log_touch(uint16_t x, uint16_t y)
{
    char text[64];

    snprintf(text, sizeof(text), "[UI TOUCH] page=%s x=%u y=%u\r\n",
             page_name(s_page), x, y);
    UART_SendString(USART2, text);
}

static void ui_log_action(UiAction action, uint8_t ok)
{
    UART_SendString(USART2, "[UI] ");
    UART_SendString(USART2, action_name(action));
    UART_SendString(USART2, ok ? " OK\r\n" : " FAIL\r\n");
}

static void ui_short_beep(void)
{
    Board_Buzzer_Set(1U);
    delay_ms(80U);
    Board_Buzzer_Set(0U);
}

static void clamp_log_start(void)
{
    uint16_t count = App_Log_Count();

    if (count <= 4U) {
        s_log_start = 0U;
    } else if (s_log_start > (uint16_t)(count - 4U)) {
        s_log_start = (uint16_t)(count - 4U);
    }
}

static void append_latest_logs(char *body, uint16_t body_len)
{
    uint16_t count = App_Log_Count();
    uint16_t i;
    uint16_t used = 0U;
    FlashLogRecord rec;

    clamp_log_start();
    used += (uint16_t)snprintf(body + used, body_len - used,
                               "Records:%u start:%u\r\n",
                               count, s_log_start);
    for (i = 0U; i < 4U && (uint16_t)(s_log_start + i) < count && used < body_len; i++) {
        if (App_Log_Read((uint16_t)(s_log_start + i), &rec) != 0U) {
            used += (uint16_t)snprintf(body + used, body_len - used,
                                       "%03u %s %s\r\n",
                                       (uint16_t)(s_log_start + i),
                                       App_Log_TypeName(rec.type),
                                       rec.text);
        }
    }
}

static void save_thresholds(void)
{
    AppParameters params;
    uint16_t warn;
    uint16_t danger;
    uint16_t critical;

    if (Adapter_EEPROM_Load(&params) == 0U) {
        params.default_source = GAS_SOURCE_POT;
    }

    App_Alarm_GetThresholds(&warn, &danger, &critical);
    params.gas_warn = warn;
    params.gas_danger = danger;
    params.gas_critical = critical;
    params.crc = Adapter_EEPROM_Crc(&params);
    Adapter_EEPROM_Save(&params);
    App_Log_Add(LOG_PARAM, "alarm threshold saved");
}

static void adjust_threshold(uint8_t index, int16_t delta)
{
    uint16_t warn;
    uint16_t danger;
    uint16_t critical;
    int32_t value;

    App_Alarm_GetThresholds(&warn, &danger, &critical);
    if (index == 1U) {
        value = (int32_t)warn + delta;
        if (value < 100) value = 100;
        warn = (uint16_t)value;
    } else if (index == 2U) {
        value = (int32_t)danger + delta;
        if (value < 101) value = 101;
        danger = (uint16_t)value;
    } else {
        value = (int32_t)critical + delta;
        if (value < 102) value = 102;
        critical = (uint16_t)value;
    }
    App_Alarm_SetThresholds(warn, danger, critical);
}

static uint8_t confirm_cargo_with(AuthMethod method, const char *name)
{
    if (Adapter_Auth_Verify(method, 0U) == 0U) {
        return 0U;
    }
    return App_Cargo_Confirm(name);
}

static void ui_print_cargo_list(void)
{
    char list[128];

    App_Cargo_FormatList(list, sizeof(list));
    UART_SendString(USART2, "CARGO LIST ");
    UART_SendString(USART2, list);
    UART_SendString(USART2, "\r\n");
}

void App_UI_Init(void)
{
    Adapter_LCD_Init();
    s_page = UI_SELF_CHECK;
}

void App_UI_SetPage(UiPage page)
{
    if (page != UI_AUTH &&
        page != UI_SELF_CHECK &&
        page != UI_ALARM &&
        Adapter_Auth_IsLoggedIn() == 0U) {
        s_page = UI_AUTH;
        return;
    }

    s_page = page;
}

UiPage App_UI_GetPage(void)
{
    return s_page;
}

static void build_page_body(const AppSnapshot *snapshot, char *body, uint16_t body_len)
{
    uint16_t warn;
    uint16_t danger;
    uint16_t critical;
    char cargo_list[96];

    switch (s_page) {
    case UI_SELF_CHECK:
        snprintf(body, body_len,
                 "System ready. Register first.\r\nTouch every visible block.");
        break;
    case UI_AUTH:
        snprintf(body, body_len,
                 "Status:%s\r\nCard:%s %c/%c Finger:%s %c/%c",
                 Adapter_Auth_StatusText(),
                 Adapter_Auth_CardId(),
                 Adapter_Auth_IsNfcRegistered() ? 'R' : '-',
                 Adapter_Auth_IsNfcVerified() ? 'V' : '-',
                 Adapter_Auth_FingerId(),
                 Adapter_Auth_IsFingerRegistered() ? 'R' : '-',
                 Adapter_Auth_IsFingerVerified() ? 'V' : '-');
        break;
    case UI_HOME:
        snprintf(body, body_len,
                 "Gas:%u %s Alarm:%s\r\nAll blocks are touch actions.",
                 snapshot->gas_value,
                 App_Gas_SourceName(snapshot->gas_source),
                 App_Alarm_Name(snapshot->alarm));
        break;
    case UI_MONITOR:
        snprintf(body, body_len,
                 "T:%d.%dC H:%u%% Gas:%u %s\r\nGPS:%s CAN:%s Alarm:%s",
                 snapshot->temperature_x10 / 10,
                 snapshot->temperature_x10 % 10,
                 snapshot->humidity,
                 snapshot->gas_value,
                 App_Gas_SourceName(snapshot->gas_source),
                 App_GPS_StatusName(),
                 Board_CAN_StatusName(),
                 App_Alarm_Name(snapshot->alarm));
        break;
    case UI_SOURCE:
        snprintf(body, body_len,
                 "Current:%s Gas:%u\r\nTap source block to switch.",
                 App_Gas_SourceName(snapshot->gas_source),
                 snapshot->gas_value);
        break;
    case UI_SCENE:
        snprintf(body, body_len,
                 "Current:%s Gas:%u\r\nTap scene block to simulate.",
                 App_Scene_Name(snapshot->scene),
                 snapshot->gas_value);
        break;
    case UI_LOG:
        append_latest_logs(body, body_len);
        break;
    case UI_SETTINGS:
        App_Alarm_GetThresholds(&warn, &danger, &critical);
        snprintf(body, body_len,
                 "L1:%u L2:%u L3:%u\r\nStartup protect:%us",
                 warn, danger, critical,
                 (uint16_t)(APP_GAS_STARTUP_PROTECT_MS / 1000U));
        break;
    case UI_CARGO:
        App_Cargo_FormatList(cargo_list, sizeof(cargo_list));
        snprintf(body, body_len,
                 "%s pending:%s %s x%u\r\n%s\r\nPASS ok: 25531460",
                 App_Cargo_StatusText(),
                 App_Cargo_ModeName(App_Cargo_Mode()),
                 App_Cargo_PendingName(),
                 App_Cargo_PendingQuantity(),
                 cargo_list);
        break;
    case UI_DEVICE:
        snprintf(body, body_len,
                 "Gas:%u Alarm:%s\r\nLED/BEEP/FAN/DAC direct control",
                 snapshot->gas_value,
                 App_Alarm_Name(snapshot->alarm));
        break;
    case UI_ALARM:
        snprintf(body, body_len,
                 "ALARM:%s %s Gas:%u %s\r\nKEY/TOUCH MUTE. GPS:%s",
                 App_Alarm_Name(snapshot->alarm),
                 App_Alarm_IsMuted() ? "MUTED" : "RING",
                 snapshot->gas_value,
                 App_Gas_SourceName(snapshot->gas_source),
                 App_GPS_StatusName());
        break;
    default:
        snprintf(body, body_len, "Unknown page");
        break;
    }
}

void App_UI_Render(const AppSnapshot *snapshot)
{
    char title[48];
    char body[512];
    uint8_t count;
    uint8_t lcd_count;
    LcdButton lcd_buttons[UI_MAX_BUTTONS];
    const UiActionButton *buttons;

    if (snapshot->alarm >= ALARM_LEVEL2) {
        s_page = UI_ALARM;
    }

    snprintf(title, sizeof(title), "%s | t=%lus",
             page_name(s_page),
             (unsigned long)(snapshot->uptime_ms / 1000U));
    build_page_body(snapshot, body, sizeof(body));

    buttons = page_buttons(s_page, &count);
    lcd_count = copy_lcd_buttons(buttons, count, lcd_buttons);
    Adapter_LCD_DrawButtonPage(title, body, lcd_buttons, lcd_count);
}

static uint8_t handle_page_action(UiAction action)
{
    switch (action) {
    case UI_ACT_HOME: App_UI_SetPage(UI_HOME); return 1U;
    case UI_ACT_PAGE_MONITOR: App_UI_SetPage(UI_MONITOR); return 1U;
    case UI_ACT_PAGE_SOURCE: App_UI_SetPage(UI_SOURCE); return 1U;
    case UI_ACT_PAGE_SCENE: App_UI_SetPage(UI_SCENE); return 1U;
    case UI_ACT_PAGE_LOG: App_UI_SetPage(UI_LOG); return 1U;
    case UI_ACT_PAGE_SETTING: App_UI_SetPage(UI_SETTINGS); return 1U;
    case UI_ACT_PAGE_AUTH: App_UI_SetPage(UI_AUTH); return 1U;
    case UI_ACT_PAGE_CARGO: App_UI_SetPage(UI_CARGO); return 1U;
    case UI_ACT_PAGE_DEVICE: App_UI_SetPage(UI_DEVICE); return 1U;
    default: return 0U;
    }
}

static uint8_t handle_action(UiAction action)
{
    if (handle_page_action(action) != 0U) {
        return 1U;
    }

    switch (action) {
    case UI_ACT_AUTH_REG_NFC: return Adapter_Auth_Register(AUTH_METHOD_NFC, 0U);
    case UI_ACT_AUTH_REG_FPR: return Adapter_Auth_Register(AUTH_METHOD_FINGER, 0U);
    case UI_ACT_AUTH_VER_NFC: return Adapter_Auth_Verify(AUTH_METHOD_NFC, 0U);
    case UI_ACT_AUTH_VER_FPR: return Adapter_Auth_Verify(AUTH_METHOD_FINGER, 0U);
    case UI_ACT_AUTH_REG_BOTH: return Adapter_Auth_Register(AUTH_METHOD_BOTH, 0U);
    case UI_ACT_AUTH_CLEAR: Adapter_Auth_ClearRegistration(); return 1U;

    case UI_ACT_SOURCE_POT: App_Gas_SetSource(GAS_SOURCE_POT); App_Log_Add(LOG_PARAM, "source pot"); return 1U;
    case UI_ACT_SOURCE_DAC: App_Gas_SetSource(GAS_SOURCE_DAC_LOOP); App_Log_Add(LOG_PARAM, "source dac"); return 1U;
    case UI_ACT_SOURCE_SCENE: App_Gas_SetSource(GAS_SOURCE_SCENE); App_Log_Add(LOG_PARAM, "source scene"); return 1U;
    case UI_ACT_SOURCE_UART: App_Gas_SetSource(GAS_SOURCE_UART); App_Log_Add(LOG_PARAM, "source uart"); return 1U;
    case UI_ACT_SOURCE_REAL: App_Gas_SetSource(GAS_SOURCE_REAL_MQ2); App_Log_Add(LOG_PARAM, "source real"); return 1U;

    case UI_ACT_SCENE_NORMAL: App_Scene_Start(SCENE_NORMAL); return 1U;
    case UI_ACT_SCENE_GAS_SLOW: App_Scene_Start(SCENE_GAS_SLOW); return 1U;
    case UI_ACT_SCENE_GAS_FAST: App_Scene_Start(SCENE_GAS_FAST); return 1U;
    case UI_ACT_SCENE_GPS_LOST: App_Scene_Start(SCENE_GPS_LOST); return 1U;
    case UI_ACT_SCENE_ROUTE_DEV: App_Scene_Start(SCENE_ROUTE_DEVIATE); return 1U;
    case UI_ACT_SCENE_CAN_OFF: App_Scene_Start(SCENE_CAN_OFFLINE); return 1U;
    case UI_ACT_SCENE_ILLEGAL: App_Scene_Start(SCENE_ILLEGAL_OPEN); return 1U;
    case UI_ACT_SCENE_RESET: App_Scene_Stop(); return 1U;

    case UI_ACT_LOG_PREV:
        if (s_log_start >= 4U) s_log_start = (uint16_t)(s_log_start - 4U);
        else s_log_start = 0U;
        return 1U;
    case UI_ACT_LOG_NEXT:
        s_log_start = (uint16_t)(s_log_start + 4U);
        clamp_log_start();
        return 1U;
    case UI_ACT_LOG_EXPORT: App_Log_Export(); return 1U;
    case UI_ACT_LOG_CLEAR: App_Log_Clear(); s_log_start = 0U; return 1U;

    case UI_ACT_SET_L1_INC: adjust_threshold(1U, UI_THRESHOLD_STEP); return 1U;
    case UI_ACT_SET_L1_DEC: adjust_threshold(1U, -((int16_t)UI_THRESHOLD_STEP)); return 1U;
    case UI_ACT_SET_L2_INC: adjust_threshold(2U, UI_THRESHOLD_STEP); return 1U;
    case UI_ACT_SET_L2_DEC: adjust_threshold(2U, -((int16_t)UI_THRESHOLD_STEP)); return 1U;
    case UI_ACT_SET_L3_INC: adjust_threshold(3U, UI_THRESHOLD_STEP); return 1U;
    case UI_ACT_SET_L3_DEC: adjust_threshold(3U, -((int16_t)UI_THRESHOLD_STEP)); return 1U;
    case UI_ACT_SET_SAVE: save_thresholds(); return 1U;
    case UI_ACT_SET_DEFAULT: App_Alarm_ResetThresholds(); save_thresholds(); return 1U;

    case UI_ACT_CARGO_ADD: App_Cargo_SelectMode(CARGO_MODE_ADD); return 1U;
    case UI_ACT_CARGO_OUT: App_Cargo_SelectMode(CARGO_MODE_OUT); return 1U;
    case UI_ACT_CARGO_LIST: ui_print_cargo_list(); return 1U;
    case UI_ACT_CARGO_NFC_OK: return confirm_cargo_with(AUTH_METHOD_NFC, "NFC");
    case UI_ACT_CARGO_FPR_OK: return confirm_cargo_with(AUTH_METHOD_FINGER, "FPR");
    case UI_ACT_CARGO_CANCEL: App_Cargo_SelectMode(CARGO_MODE_NONE); return 1U;

    case UI_ACT_DEV_BEEP: ui_short_beep(); return 1U;
    case UI_ACT_DEV_LED_G: Board_LED_Toggle(BOARD_LED_GREEN); return 1U;
    case UI_ACT_DEV_LED_R: Board_LED_Toggle(BOARD_LED_RED); return 1U;
    case UI_ACT_DEV_FAN25: Board_PWM_SetFanPercent(25U); return 1U;
    case UI_ACT_DEV_FAN75: Board_PWM_SetFanPercent(75U); return 1U;
    case UI_ACT_DEV_DAC300: Board_DAC_SetGasValue(300U); return 1U;
    case UI_ACT_DEV_DAC850: Board_DAC_SetGasValue(850U); return 1U;
    case UI_ACT_DEV_ALARM_RST:
        if (Adapter_Auth_IsAdmin() == 0U) return 0U;
        App_Alarm_ResetByAdmin();
        return 1U;
    default:
        return 0U;
    }
}

void App_UI_HandleTouch(uint16_t x, uint16_t y)
{
    UiAction action;
    uint8_t ok;

    ui_log_touch(x, y);

    if (App_Alarm_GetLevel() >= ALARM_LEVEL3) {
        App_Alarm_MuteByKey();
    }

    if (y < UI_TOUCH_TOP_BAR_Y) {
        App_UI_SetPage(Adapter_Auth_IsLoggedIn() ? UI_HOME : UI_AUTH);
        ui_log_action(UI_ACT_HOME, 1U);
        return;
    }

    if (s_page != UI_HOME && s_page != UI_AUTH && s_page != UI_SELF_CHECK && y >= UI_TOUCH_BOTTOM_NAV_Y) {
        if (x < 80U) {
            action = UI_ACT_PAGE_MONITOR;
        } else if (x < 160U) {
            action = UI_ACT_PAGE_SCENE;
        } else {
            action = UI_ACT_PAGE_LOG;
        }
        ok = handle_action(action);
        ui_log_action(action, ok);
        return;
    }

    action = hit_action(x, y);
    ok = handle_action(action);
    ui_log_action(action, ok);

    if ((s_page == UI_AUTH || s_page == UI_SELF_CHECK) &&
        Adapter_Auth_IsLoggedIn() != 0U) {
        App_UI_SetPage(UI_HOME);
    }
}
