#ifndef __ADAPTER_TOUCH_H__
#define __ADAPTER_TOUCH_H__

#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t pressed;
} TouchEvent;

void Adapter_Touch_Init(void);
uint8_t Adapter_Touch_GetEvent(TouchEvent *event);
void Adapter_Touch_Inject(uint16_t x, uint16_t y);

#endif
