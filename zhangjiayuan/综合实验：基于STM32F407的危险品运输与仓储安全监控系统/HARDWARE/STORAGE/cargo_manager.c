#include "cargo_manager.h"
#include "log_manager.h"
#include <stdio.h>
#include <string.h>

static CargoItem s_items[APP_CARGO_MAX_ITEMS];
static CargoMode s_mode = CARGO_MODE_NONE;
static char s_pending_name[APP_CARGO_NAME_LEN];
static uint16_t s_pending_quantity = 0U;
static char s_status[64] = "cargo ready";

static void set_item(uint8_t index, const char *name, uint16_t quantity)
{
    if (index >= APP_CARGO_MAX_ITEMS) {
        return;
    }

    strncpy(s_items[index].name, name, APP_CARGO_NAME_LEN - 1U);
    s_items[index].name[APP_CARGO_NAME_LEN - 1U] = '\0';
    s_items[index].quantity = quantity;
    s_items[index].used = 1U;
}

static int8_t find_item(const char *name)
{
    uint8_t i;

    for (i = 0U; i < APP_CARGO_MAX_ITEMS; i++) {
        if (s_items[i].used != 0U && strcmp(s_items[i].name, name) == 0) {
            return (int8_t)i;
        }
    }

    return -1;
}

static int8_t find_free_item(void)
{
    uint8_t i;

    for (i = 0U; i < APP_CARGO_MAX_ITEMS; i++) {
        if (s_items[i].used == 0U) {
            return (int8_t)i;
        }
    }

    return -1;
}

static void clear_pending(void)
{
    s_mode = CARGO_MODE_NONE;
    s_pending_name[0] = '\0';
    s_pending_quantity = 0U;
}

void App_Cargo_Init(void)
{
    App_Cargo_Reset();
}

void App_Cargo_Reset(void)
{
    memset(s_items, 0, sizeof(s_items));
    set_item(0U, "A", 10U);
    set_item(1U, "B", 8U);
    set_item(2U, "C", 6U);
    set_item(3U, "D", 12U);
    set_item(4U, "E", 5U);
    set_item(5U, "F", 3U);
    clear_pending();
    strncpy(s_status, "cargo reset A-F", sizeof(s_status) - 1U);
    s_status[sizeof(s_status) - 1U] = '\0';
}

void App_Cargo_SelectMode(CargoMode mode)
{
    s_mode = mode;
    s_pending_name[0] = '\0';
    s_pending_quantity = 0U;
    if (mode == CARGO_MODE_NONE) {
        snprintf(s_status, sizeof(s_status), "cargo pending cancelled");
    } else {
        snprintf(s_status, sizeof(s_status), "%s mode, send CARGO cmd", App_Cargo_ModeName(mode));
    }
}

uint8_t App_Cargo_SetPending(CargoMode mode, const char *name, uint16_t quantity)
{
    if (name == 0 || name[0] == '\0' || quantity == 0U || mode == CARGO_MODE_NONE) {
        strncpy(s_status, "cargo pending invalid", sizeof(s_status) - 1U);
        s_status[sizeof(s_status) - 1U] = '\0';
        return 0U;
    }

    s_mode = mode;
    strncpy(s_pending_name, name, sizeof(s_pending_name) - 1U);
    s_pending_name[sizeof(s_pending_name) - 1U] = '\0';
    s_pending_quantity = quantity;
    snprintf(s_status, sizeof(s_status), "pending %s %s x%u",
             App_Cargo_ModeName(mode), s_pending_name, s_pending_quantity);
    return 1U;
}

uint8_t App_Cargo_Confirm(const char *auth_method)
{
    int8_t index;
    char log_text[80];

    if (s_mode == CARGO_MODE_NONE || s_pending_name[0] == '\0' || s_pending_quantity == 0U) {
        strncpy(s_status, "no cargo pending", sizeof(s_status) - 1U);
        s_status[sizeof(s_status) - 1U] = '\0';
        App_Log_Add(LOG_FAULT, "cargo confirm without pending");
        return 0U;
    }

    index = find_item(s_pending_name);

    if (s_mode == CARGO_MODE_ADD) {
        if (index < 0) {
            index = find_free_item();
            if (index < 0) {
                strncpy(s_status, "cargo list full", sizeof(s_status) - 1U);
                s_status[sizeof(s_status) - 1U] = '\0';
                App_Log_Add(LOG_FAULT, "cargo add failed full");
                return 0U;
            }
            set_item((uint8_t)index, s_pending_name, 0U);
        }
        s_items[(uint8_t)index].quantity =
            (uint16_t)(s_items[(uint8_t)index].quantity + s_pending_quantity);
        snprintf(log_text, sizeof(log_text), "cargo add %s x%u by %s",
                 s_pending_name, s_pending_quantity, auth_method);
        snprintf(s_status, sizeof(s_status), "added %s x%u",
                 s_pending_name, s_pending_quantity);
        App_Log_Add(LOG_TRANSPORT, log_text);
        clear_pending();
        return 1U;
    }

    if (index < 0 || s_items[(uint8_t)index].quantity < s_pending_quantity) {
        snprintf(log_text, sizeof(log_text), "cargo out failed %s x%u",
                 s_pending_name, s_pending_quantity);
        snprintf(s_status, sizeof(s_status), "out fail %s x%u",
                 s_pending_name, s_pending_quantity);
        App_Log_Add(LOG_FAULT, log_text);
        return 0U;
    }

    s_items[(uint8_t)index].quantity =
        (uint16_t)(s_items[(uint8_t)index].quantity - s_pending_quantity);
    snprintf(log_text, sizeof(log_text), "cargo out %s x%u by %s",
             s_pending_name, s_pending_quantity, auth_method);
    snprintf(s_status, sizeof(s_status), "out %s x%u",
             s_pending_name, s_pending_quantity);
    App_Log_Add(LOG_TRANSPORT, log_text);
    clear_pending();
    return 1U;
}

uint8_t App_Cargo_Read(uint8_t index, CargoItem *out)
{
    if (out == 0 || index >= APP_CARGO_MAX_ITEMS || s_items[index].used == 0U) {
        return 0U;
    }

    *out = s_items[index];
    return 1U;
}

uint8_t App_Cargo_Count(void)
{
    uint8_t i;
    uint8_t count = 0U;

    for (i = 0U; i < APP_CARGO_MAX_ITEMS; i++) {
        if (s_items[i].used != 0U) {
            count++;
        }
    }

    return count;
}

CargoMode App_Cargo_Mode(void)
{
    return s_mode;
}

const char *App_Cargo_ModeName(CargoMode mode)
{
    switch (mode) {
    case CARGO_MODE_ADD: return "ADD";
    case CARGO_MODE_OUT: return "OUT";
    default: return "NONE";
    }
}

const char *App_Cargo_StatusText(void)
{
    return s_status;
}

const char *App_Cargo_PendingName(void)
{
    return s_pending_name;
}

uint16_t App_Cargo_PendingQuantity(void)
{
    return s_pending_quantity;
}

void App_Cargo_FormatList(char *out, uint16_t out_len)
{
    uint8_t i;
    uint16_t used = 0U;

    if (out == 0 || out_len == 0U) {
        return;
    }

    out[0] = '\0';
    for (i = 0U; i < APP_CARGO_MAX_ITEMS && used < out_len; i++) {
        if (s_items[i].used != 0U) {
            used += (uint16_t)snprintf(out + used,
                                       out_len - used,
                                       "%s:%u ",
                                       s_items[i].name,
                                       s_items[i].quantity);
        }
    }
}
