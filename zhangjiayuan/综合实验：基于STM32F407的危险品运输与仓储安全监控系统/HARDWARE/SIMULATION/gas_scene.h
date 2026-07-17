#ifndef __APP_SCENE_H__
#define __APP_SCENE_H__

#include "app_types.h"
#include <stdint.h>

void App_Scene_Init(void);
void App_Scene_Start(SceneType scene);
void App_Scene_Stop(void);
void App_Scene_Task1000ms(void);
SceneType App_Scene_Current(void);
const char *App_Scene_Name(SceneType scene);

#endif
