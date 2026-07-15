#ifndef __APP_CARGO_H__
#define __APP_CARGO_H__

#include <stdint.h>

#define APP_CARGO_NAME_LEN 12U
#define APP_CARGO_MAX_ITEMS 8U

typedef enum {
    CARGO_MODE_NONE = 0,
    CARGO_MODE_ADD,
    CARGO_MODE_OUT
} CargoMode;

typedef struct {
    char name[APP_CARGO_NAME_LEN];
    uint16_t quantity;
    uint8_t used;
} CargoItem;

void App_Cargo_Init(void);
void App_Cargo_Reset(void);
uint8_t App_Cargo_SetPending(CargoMode mode, const char *name, uint16_t quantity);
void App_Cargo_SelectMode(CargoMode mode);
uint8_t App_Cargo_Confirm(const char *auth_method);
uint8_t App_Cargo_Read(uint8_t index, CargoItem *out);
uint8_t App_Cargo_Count(void);
CargoMode App_Cargo_Mode(void);
const char *App_Cargo_ModeName(CargoMode mode);
const char *App_Cargo_StatusText(void);
const char *App_Cargo_PendingName(void);
uint16_t App_Cargo_PendingQuantity(void);
void App_Cargo_FormatList(char *out, uint16_t out_len);

#endif
