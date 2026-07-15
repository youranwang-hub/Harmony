#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "lcd_ili9341.h"
#include "delay.h"

/* 方块参数 */
#define BLOCK_SIZE             20
#define BLOCK_Y                100
#define BLOCK_STEP             2

/* 刷新时间参数，单位为ms */
#define FRAME_DELAY_MS         30
#define COUNT_UPDATE_MS        1000
#define COLOR_CHANGE_MS        3000

#define COLOR_NUMBER           5

/* 背景颜色 */
static const uint16_t g_BackColors[COLOR_NUMBER] =
{
    RED,
    GREEN,
    BLUE,
    WHITE,
    BLACK
};

/* 不同背景对应的文字颜色 */
static const uint16_t g_TextColors[COLOR_NUMBER] =
{
    WHITE,      /* 红色背景 */
    BLACK,      /* 绿色背景 */
    WHITE,      /* 蓝色背景 */
    BLACK,      /* 白色背景 */
    WHITE       /* 黑色背景 */
};

/* 不同背景对应的方块颜色 */
static const uint16_t g_BlockColors[COLOR_NUMBER] =
{
    WHITE,      /* 红色背景使用白色方块 */
    BLACK,      /* 绿色背景使用黑色方块 */
    YELLOW,     /* 蓝色背景使用黄色方块 */
    RED,        /* 白色背景使用红色方块 */
    GREEN       /* 黑色背景使用绿色方块 */
};

/* 背景颜色名称 */
static char *g_ColorNames[COLOR_NUMBER] =
{
    "RED",
    "GREEN",
    "BLUE",
    "WHITE",
    "BLACK"
};

/**
  * @brief  使用指定颜色填充一个实心方块
  * @param  usX：方块左上角X坐标
  * @param  usY：方块左上角Y坐标
  * @param  usSize：方块边长
  * @param  usFillColor：方块填充颜色
  * @param  usBackColor：当前屏幕背景颜色
  * @retval 无
  */
static void LCD_FillBlock(uint16_t usX,
                          uint16_t usY,
                          uint16_t usSize,
                          uint16_t usFillColor,
                          uint16_t usBackColor)
{
    /*
     * LCD_Clear使用当前背景色填充指定区域。
     * 先临时将背景色设置为方块颜色，完成方块填充，
     * 再恢复原来的页面背景色。
     */
    LCD_SetBackColor(usFillColor);

    LCD_Clear(usX,
              usY,
              usSize,
              usSize);

    LCD_SetBackColor(usBackColor);
}

/**
  * @brief  显示当前背景颜色名称
  * @param  pColorName：颜色名称
  * @retval 无
  */
static void LCD_ShowColorName(char *pColorName)
{
    char str[32];

    sprintf(str,
            "Color: %s",
            pColorName);

    LCD_DispStringEN(0,
                     LINE_EN(1),
                     0,
                     str);
}

/**
  * @brief  显示当前计数值
  * @param  count：当前计数值
  * @retval 无
  */
static void LCD_ShowCount(uint32_t count)
{
    char str[32];

    /*
     * 先清除原来的计数显示区域，
     * 防止数字位数变化后出现字符残留。
     */
    LCD_Clear(0,
              LINE_EN(2),
              LCD_GetLenX(),
              LCD_GetFontEN()->Height);

    sprintf(str,
            "Count: %lu",
            (unsigned long)count);

    LCD_DispStringEN(0,
                     LINE_EN(2),
                     0,
                     str);
}

/**
  * @brief  显示完整综合测试界面
  * @param  colorIndex：当前颜色序号
  * @param  count：当前计数值
  * @retval 无
  */
static void LCD_ShowInterface(uint8_t colorIndex,
                              uint32_t count)
{
    uint16_t backColor;
    uint16_t textColor;

    backColor = g_BackColors[colorIndex];
    textColor = g_TextColors[colorIndex];

    /*
     * 设置当前页面文字颜色和背景颜色。
     */
    LCD_SetColors(textColor,
                  backColor);

    /*
     * 使用当前背景颜色清除整个LCD。
     */
    LCD_Clear(0,
              0,
              LCD_GetLenX(),
              LCD_GetLenY());

    LCD_DispStringEN(0,
                     LINE_EN(0),
                     0,
                     "LCD COMBINED TEST");

    LCD_ShowColorName(
        g_ColorNames[colorIndex]
    );

    LCD_ShowCount(count);

    LCD_DispStringEN(0,
                     LINE_EN(4),
                     0,
                     "MOVING BLOCK");
}

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    uint32_t count;
    uint16_t countTime;
    uint16_t colorTime;

    uint8_t colorIndex;

    int16_t blockX;
    int16_t oldBlockX;
    int8_t direction;

    uint16_t backColor;
    uint16_t blockColor;

    count = 0;
    countTime = 0;
    colorTime = 0;

    colorIndex = 0;

    blockX = 0;
    oldBlockX = 0;
    direction = 1;

    /*
     * 初始化UART2调试串口。
     */
    UART2_Init(115200);

    printf("LCD COMBINED TEST\r\n");

    /*
     * 初始化LCD。
     */
    LCD_Init();

    /*
     * 使用8×16 ASCII英文字体。
     */
    LCD_SetFontEN(&ASCII_8x16);

    /*
     * 获取初始页面颜色。
     */
    backColor = g_BackColors[colorIndex];
    blockColor = g_BlockColors[colorIndex];

    /*
     * 显示初始界面。
     */
    LCD_ShowInterface(colorIndex,
                      count);

    /*
     * 显示初始方块。
     */
    LCD_FillBlock((uint16_t)blockX,
                  BLOCK_Y,
                  BLOCK_SIZE,
                  blockColor,
                  backColor);

    printf("Background: %s\r\n",
           g_ColorNames[colorIndex]);

    printf("Count: %lu\r\n",
           (unsigned long)count);

    while (1)
    {
        /*
         * 擦除方块原来的位置。
         */
        oldBlockX = blockX;

        LCD_FillBlock((uint16_t)oldBlockX,
                      BLOCK_Y,
                      BLOCK_SIZE,
                      backColor,
                      backColor);

        /*
         * 更新方块X坐标。
         */
        blockX += direction * BLOCK_STEP;

        /*
         * 到达右边界后反向。
         */
        if (blockX >=
            (int16_t)(LCD_GetLenX() - BLOCK_SIZE))
        {
            blockX =
                (int16_t)(LCD_GetLenX() - BLOCK_SIZE);

            direction = -1;
        }
        /*
         * 到达左边界后反向。
         */
        else if (blockX <= 0)
        {
            blockX = 0;
            direction = 1;
        }

        /*
         * 累计运行时间。
         */
        countTime += FRAME_DELAY_MS;
        colorTime += FRAME_DELAY_MS;

        /*
         * 每隔1秒更新一次计数值。
         */
        if (countTime >= COUNT_UPDATE_MS)
        {
            countTime = 0;

            count++;

            if (count > 999999)
            {
                count = 0;
            }

            LCD_ShowCount(count);

            printf("Count: %lu\r\n",
                   (unsigned long)count);
        }

        /*
         * 每隔3秒切换一次背景颜色。
         */
        if (colorTime >= COLOR_CHANGE_MS)
        {
            colorTime = 0;

            colorIndex++;

            if (colorIndex >= COLOR_NUMBER)
            {
                colorIndex = 0;
            }

            /*
             * 获取新的背景颜色和方块颜色。
             */
            backColor =
                g_BackColors[colorIndex];

            blockColor =
                g_BlockColors[colorIndex];

            /*
             * 切换背景颜色后重新绘制整个界面。
             */
            LCD_ShowInterface(colorIndex,
                              count);

            printf("Background: %s\r\n",
                   g_ColorNames[colorIndex]);
        }

        /*
         * 在新的坐标位置绘制方块。
         */
        LCD_FillBlock((uint16_t)blockX,
                      BLOCK_Y,
                      BLOCK_SIZE,
                      blockColor,
                      backColor);

        delay_ms(FRAME_DELAY_MS);
    }
}
