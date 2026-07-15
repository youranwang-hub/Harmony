#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "app_types.h"
#include <stdint.h>

void App_UI_Init(void);
void App_UI_SetPage(UiPage page);
UiPage App_UI_GetPage(void);
void App_UI_Render(const AppSnapshot *snapshot);
void App_UI_HandleTouch(uint16_t x, uint16_t y);

#endif
