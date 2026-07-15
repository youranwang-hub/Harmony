#include "lcd.h"
#include "system_config.h"
#include "usart_debug.h"
#include <stdio.h>
#include <string.h>

#if APP_ENABLE_REAL_LCD
#include "lcd_ili9341.h"
#endif

#define LCD_ADAPTER_MAX_LINE_CHARS 30U
#define LCD_BUTTON_BODY_LINES      2U
#define LCD_SERIAL_PAGE_MIRROR     0U

void Adapter_LCD_Init(void)
{
#if APP_ENABLE_REAL_LCD
    LCD_Init();
    LCD_SetFontEN(&ASCII_8x16);
    LCD_SetColors(WHITE, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_DispStringEN(0, LINE_EN(0), 0, "Hazmat Monitor Boot");
    UART_SendString(USART2, "[LCD] ILI9341 FSMC driver enabled.\r\n");
#else
    UART_SendString(USART2, "[LCD] adapter mode: serial-only LCD page mirror.\r\n");
#endif
}

void Adapter_LCD_Clear(void)
{
#if APP_ENABLE_REAL_LCD
    LCD_SetColors(WHITE, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
#endif
    UART_SendString(USART2, "\r\n[LCD] clear\r\n");
}

void Adapter_LCD_DrawText(uint16_t x, uint16_t y, const char *text)
{
    char line[120];
#if APP_ENABLE_REAL_LCD
    if (x < LCD_GetLenX() && y < LCD_GetLenY()) {
        LCD_DispStringEN(x, y, 0, (char *)text);
    }
#endif
    snprintf(line, sizeof(line), "[LCD %03u,%03u] %s\r\n", x, y, text);
    UART_SendString(USART2, line);
}

static void draw_body_lines_from(const char *body, uint16_t start_y, uint8_t max_lines)
{
#if APP_ENABLE_REAL_LCD
    char line[LCD_ADAPTER_MAX_LINE_CHARS + 1U];
    uint16_t y = start_y;
    uint8_t idx = 0U;
    uint8_t lines = 0U;

    while (*body != '\0' && y < LCD_GetLenY() && (max_lines == 0U || lines < max_lines)) {
        if (*body == '\r') {
            body++;
            continue;
        }
        if (*body == '\n' || idx >= LCD_ADAPTER_MAX_LINE_CHARS) {
            line[idx] = '\0';
            LCD_DispStringEN(0, y, 0, line);
            y = (uint16_t)(y + LINE_EN(1));
            idx = 0U;
            lines++;
            if (*body == '\n') {
                body++;
            }
            continue;
        }
        line[idx++] = *body++;
    }

    if (idx > 0U && y < LCD_GetLenY() && (max_lines == 0U || lines < max_lines)) {
        line[idx] = '\0';
        LCD_DispStringEN(0, y, 0, line);
    }
#else
    (void)body;
    (void)start_y;
    (void)max_lines;
#endif
}

#if APP_ENABLE_REAL_LCD
static void draw_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *label, uint16_t color)
{
    LCD_SetColors(WHITE, color);
    LCD_Clear(x, y, w, h);
    LCD_DispStringEN((uint16_t)(x + 6U), (uint16_t)(y + 14U), 0, (char *)label);
    LCD_SetColors(WHITE, BLACK);
}
#endif

#if LCD_SERIAL_PAGE_MIRROR
static void mirror_buttons(const LcdButton *buttons, uint8_t count)
{
    uint8_t i;
    char line[80];

    for (i = 0U; i < count; i++) {
        snprintf(line, sizeof(line), "[BTN %u] %s (%u,%u,%u,%u)\r\n",
                 i,
                 buttons[i].label,
                 buttons[i].x,
                 buttons[i].y,
                 buttons[i].w,
                 buttons[i].h);
        UART_SendString(USART2, line);
    }
}
#endif

void Adapter_LCD_DrawButtonPage(const char *title,
                                const char *body,
                                const LcdButton *buttons,
                                uint8_t button_count)
{
    uint8_t i;
#if APP_ENABLE_REAL_LCD
    uint16_t text = WHITE;
    if (strstr(title, "ALARM") != 0 || strstr(body, "ALARM LEVEL") != 0) {
        text = RED;
    }
    LCD_SetColors(text, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_DispStringEN(0, LINE_EN(0), 0, (char *)title);
    LCD_SetColors(WHITE, BLACK);

    if (buttons != 0 && button_count > 0U) {
        draw_body_lines_from(body, LINE_EN(1), LCD_BUTTON_BODY_LINES);
        for (i = 0U; i < button_count; i++) {
            draw_button(buttons[i].x,
                        buttons[i].y,
                        buttons[i].w,
                        buttons[i].h,
                        buttons[i].label,
                        buttons[i].color);
        }
    } else {
        draw_body_lines_from(body, LINE_EN(2), 0U);
    }
#endif
#if LCD_SERIAL_PAGE_MIRROR
    UART_SendString(USART2, "\r\n========== LCD PAGE ==========\r\n");
    UART_SendString(USART2, title);
    UART_SendString(USART2, "\r\n");
    UART_SendString(USART2, body);
    UART_SendString(USART2, "\r\n==============================\r\n");
    if (buttons != 0 && button_count > 0U) {
        mirror_buttons(buttons, button_count);
    }
#else
    (void)title;
    (void)body;
    (void)buttons;
    (void)button_count;
#endif
}

void Adapter_LCD_DrawStatus(const char *title, const char *body)
{
    Adapter_LCD_DrawButtonPage(title, body, 0, 0U);
}
