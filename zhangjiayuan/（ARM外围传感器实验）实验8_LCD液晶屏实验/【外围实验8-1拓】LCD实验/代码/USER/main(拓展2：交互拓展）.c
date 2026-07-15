#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "lcd_ili9341.h"
#include "delay.h"

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    UART2_Init(115200);   //调试串口，方便打印

    printf("LCD TEST\r\n");

    LCD_Init();

    LCD_SetFontEN(&ASCII_8x16);

    LCD_SetColors(RED, BLACK);

    LCD_Clear(
        0,
        0,
        LCD_GetLenX(),
        LCD_GetLenY()
    );

    LCD_DispStringEN(
        0,
        LINE_EN(0),
        0,
        "3.2 inch LCD:"
    );

    LCD_DispStringEN(
        0,
        LINE_EN(1),
        0,
        "Image resolution:240x320 px"
    );

    while (1)
    {
    }
}
