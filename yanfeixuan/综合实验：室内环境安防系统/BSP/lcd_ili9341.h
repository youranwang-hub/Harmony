#ifndef __LCD_ILI9341_H
#define __LCD_ILI9341_H

#include <stddef.h>
#include "stm32f4xx.h"

// RGB565颜色定义
#define WHITE  0xFFFF
#define BLACK  0x0000
#define RED    0xF800
#define YELLOW 0xFFE0
#define BLUE   0x001F
#define GREEN  0x07E0

/**************** FSMC/FSMC 屏幕硬件定义 ****************/
#define FSMC_LCD_REG    (*(volatile unsigned short *)0x60000000)
#define FSMC_LCD_DATA   (*(volatile unsigned short *)0x6002000)

/**************** ILI9341 屏幕参数 ****************/
#define ILI9341_LESS_PIX  240
#define ILI9341_LONG_PIX  320

// 指令宏
#define CMD_RESET     0x01
#define CMD_DISP_ON   0x29
#define CMD_MADCTL    0x36
#define CMD_PIXFMT    0x3A
#define CMD_CASET     0x2A
#define CMD_RASET     0x2B
#define CMD_WR_RAM    0x2C

/**************** 全局外部变量 ****************/
extern uint8_t  g_LCD_ScanMode;
extern uint16_t g_TextColor;
extern uint16_t g_BgColor;
extern uint16_t g_LCD_Width;
extern uint16_t g_LCD_Height;

/**************** 底层原生函数声明（4参数LCD_Clear保留） ****************/
void LCD_Init(void);
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Clear(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void LCD_SetTextColor(uint16_t color);
void LCD_SetBackColor(uint16_t color);
void LCD_GetColors(uint16_t *txt, uint16_t *bg);
void LCD_DrawPoint(uint16_t x, uint16_t y);
void LCD_DispMask(uint16_t x, uint16_t y, uint16_t w, int16_t h, const uint8_t *buf);
void LCD_DispStringEN(uint16_t x, uint16_t y, uint8_t dir, char *str);

/**************** 封装函数（无同名冲突，适配main调用） ****************/
void LCD_ClearFull(uint16_t color);  // 全屏清屏，替代冲突的单参数LCD_Clear
void LCD_ShowString(uint16_t x, uint16_t y, char *str, uint16_t color);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t color);

uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);
void LCD_SetScanDir(uint8_t mode);
void LCD_BackLight(uint8_t sta);

#endif
