#ifndef __ADAPTER_LCD_H__
#define __ADAPTER_LCD_H__

#include <stdint.h>

#define LCD_BTN_BLACK   0x0000U
#define LCD_BTN_WHITE   0xFFFFU
#define LCD_BTN_BLUE    0x051FU
#define LCD_BTN_RED     0xF800U
#define LCD_BTN_GREEN   0x07E0U
#define LCD_BTN_CYAN    0x7FFFU
#define LCD_BTN_YELLOW  0xFFE0U

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    const char *label;
    uint16_t color;
} LcdButton;

void Adapter_LCD_Init(void);
void Adapter_LCD_Clear(void);
void Adapter_LCD_DrawText(uint16_t x, uint16_t y, const char *text);
void Adapter_LCD_DrawStatus(const char *title, const char *body);
void Adapter_LCD_DrawButtonPage(const char *title,
                                const char *body,
                                const LcdButton *buttons,
                                uint8_t button_count);

#endif
