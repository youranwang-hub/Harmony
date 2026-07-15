#include <stddef.h>
#include "lcd_ili9341.h"
#include "lcd_fonts.h"
#include "delay.h"     //简单延时

/***************************************************************************************
2^26 = 0X0400 0000 = 64MB，每个 BANK 有4*64MB = 256MB

选择BANK1-NORSRAM1连接TFT。
FSMC_A16接LCD的DC（寄存器/数据选择）脚。

当向0x60000000写数据时，LCD作为命令接收。
当向0x60020000写数据时，LCD作为数据接收。
****************************************************************************************/

/******************************* ILI9341 显示屏的 FSMC 参数定义 ***************************/

//FSMC_Bank1_NORSRAM用于LCD命令操作的地址
#define FSMC_Addr_ILI9341_CMD   ((uint32_t)0x60000000)

//FSMC_Bank1_NORSRAM用于LCD数据操作的地址
#define FSMC_Addr_ILI9341_DATA  ((uint32_t)0x60020000)

//复位引脚
#define ILI9341_RST_PORT        GPIOE
#define ILI9341_RST_PIN         GPIO_Pin_1

//背光引脚
#define ILI9341_BK_PORT         GPIOD
#define ILI9341_BK_PIN          GPIO_Pin_12

#define ILI9341_LESS_PIXEL      240    //液晶屏较短方向的像素宽度
#define ILI9341_MORE_PIXEL      320    //液晶屏较长方向的像素宽度

//Commands
#define CMD_RESET                   0x0001
#define CMD_SLEEP_OUT               0x0011
#define CMD_GAMMA                   0x0026
#define CMD_DISPLAY_OFF             0x0028
#define CMD_DISPLAY_ON              0x0029
#define CMD_COLUMN_ADDR             0x002A
#define CMD_PAGE_ADDR               0x002B
#define CMD_GRAM                    0x002C
#define CMD_TEARING_OFF             0x0034
#define CMD_TEARING_ON              0x0035
#define CMD_DISPLAY_INVERSION       0x00B4
#define CMD_MAC                     0x0036
#define CMD_PIXEL_FORMAT            0x003A
#define CMD_WDB                     0x0051
#define CMD_WCD                     0x0053
#define CMD_RGB_INTERFACE           0x00B0
#define CMD_FRC                     0x00B1
#define CMD_BPC                     0x00B5
#define CMD_DFC                     0x00B6
#define CMD_ETMOD                   0x00B7
#define CMD_POWER1                  0x00C0
#define CMD_POWER2                  0x00C1
#define CMD_VCOM1                   0x00C5
#define CMD_VCOM2                   0x00C7
#define CMD_POWERA                  0x00CB
#define CMD_POWERB                  0x00CF
#define CMD_PGAMMA                  0x00E0
#define CMD_NGAMMA                  0x00E1
#define CMD_DTCA                    0x00E8
#define CMD_DTCB                    0x00EA
#define CMD_POWER_SEQ               0x00ED
#define CMD_3GAMMA_EN               0x00F2
#define CMD_INTERFACE               0x00F6
#define CMD_PRC                     0x00F7
#define CMD_VERTICAL_SCROLL         0x0033

#define CMD_SetCoordinateX          0x2A
#define CMD_SetCoordinateY          0x2B
#define CMD_SetPixel                0x2C

//延时函数
#define ILI9341_DELAY_MS(x)         delay_ms(x)

//向ILI9341写入命令
#define ILI9341_WriteCmd(usCmd) \
    *(__IO uint16_t *)(FSMC_Addr_ILI9341_CMD) = (usCmd)

//向ILI9341写入数据
#define ILI9341_WriteData(usData) \
    *(__IO uint16_t *)(FSMC_Addr_ILI9341_DATA) = (usData)

//从ILI9341读取数据
#define ILI9341_ReadData() \
    (*(__IO uint16_t *)(FSMC_Addr_ILI9341_DATA))

//液晶屏扫描模式，取值为0-7
uint8_t g_LCD_ScanMode = 2;

//前景色
static uint16_t g_LCD_TextColor = BLACK;

//背景色
static uint16_t g_LCD_BackColor = WHITE;

//根据液晶扫描方向变化的XY像素宽度
uint16_t g_LCD_LenX = ILI9341_LESS_PIXEL;
uint16_t g_LCD_LenY = ILI9341_MORE_PIXEL;

/* 内部函数声明 */
static void ILI9341_GPIO_Config(void);
static void ILI9341_FSMC_Config(void);
static void ILI9341_Rst(void);
static void ILI9341_REG_Config(void);
static void ILI9341_FillColor(uint32_t ulAmout_Point,
                              uint16_t usColor);
static void ILI9341_BackLight(uint8_t uOnOff);
static void ILI9341_GramScan(uint8_t ucOption);
static void ILI9341_OpenWindow(uint16_t usX,
                               uint16_t usY,
                               uint16_t usWidth,
                               uint16_t usHeight);
static void LCD_DispMask(uint16_t uX,
                         uint16_t uY,
                         uint16_t uWidth,
                         int16_t uHeight,
                         const uint8_t *pMask);

/**
  * @brief  获取LCD的X方向长度
  * @param  无
  * @retval LCD的X方向像素数
  */
uint16_t LCD_GetLenX(void)
{
    return g_LCD_LenX;
}

/**
  * @brief  获取LCD的Y方向长度
  * @param  无
  * @retval LCD的Y方向像素数
  */
uint16_t LCD_GetLenY(void)
{
    return g_LCD_LenY;
}

/**
  * @brief  初始化ILI9341的IO引脚
  * @param  无
  * @retval 无
  */
static void ILI9341_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOD |
        RCC_AHB1Periph_GPIOE,
        ENABLE
    );

    // SRAM Data lines, NOE and NWE configuration
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_0 |
        GPIO_Pin_1 |
        GPIO_Pin_4 |
        GPIO_Pin_5 |
        GPIO_Pin_7 |
        GPIO_Pin_8 |
        GPIO_Pin_9 |
        GPIO_Pin_10 |
        GPIO_Pin_11 |
        GPIO_Pin_14 |
        GPIO_Pin_15;

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // GPIOE
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_7 |
        GPIO_Pin_8 |
        GPIO_Pin_9 |
        GPIO_Pin_10 |
        GPIO_Pin_11 |
        GPIO_Pin_12 |
        GPIO_Pin_13 |
        GPIO_Pin_14 |
        GPIO_Pin_15;

    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource4,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource7,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource11, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_FSMC);

    GPIO_PinAFConfig(GPIOE, GPIO_PinSource7,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, GPIO_AF_FSMC);

    //LED_BL
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOD, &GPIO_InitStructure);

    //RESET
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/**
 * @brief  ILI9341背光LED控制
 * @param  uOnOff：决定是否使能背光LED
 * @retval 无
 */
static void ILI9341_BackLight(uint8_t uOnOff)
{
    if (uOnOff)
    {
        GPIO_ResetBits(ILI9341_BK_PORT,
                       ILI9341_BK_PIN);
    }
    else
    {
        GPIO_SetBits(ILI9341_BK_PORT,
                     ILI9341_BK_PIN);
    }
}

/**
 * @brief  ILI9341 软件复位
 * @param  无
 * @retval 无
 */
static void ILI9341_Rst(void)
{
    GPIO_ResetBits(ILI9341_RST_PORT,
                   ILI9341_RST_PIN);

    ILI9341_DELAY_MS(5);

    GPIO_SetBits(ILI9341_RST_PORT,
                 ILI9341_RST_PIN);

    ILI9341_DELAY_MS(5);
}

/**
  * @brief  LCD FSMC模式配置
  * @param  无
  * @retval 无
  */
static void ILI9341_FSMC_Config(void)
{
    FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
    FSMC_NORSRAMTimingInitTypeDef p;

    RCC_AHB3PeriphClockCmd(
        RCC_AHB3Periph_FSMC,
        ENABLE
    );

    //SRAM Bank 1
    p.FSMC_AddressSetupTime = 0x6;
    p.FSMC_AddressHoldTime = 0;
    p.FSMC_DataSetupTime = 0x6;
    p.FSMC_BusTurnAroundDuration = 0;
    p.FSMC_CLKDivision = 0;
    p.FSMC_DataLatency = 0;
    p.FSMC_AccessMode = FSMC_AccessMode_A;

    //LCD configured as follow
    FSMC_NORSRAMInitStructure.FSMC_Bank =
        FSMC_Bank1_NORSRAM1;

    FSMC_NORSRAMInitStructure.FSMC_DataAddressMux =
        FSMC_DataAddressMux_Disable;

    FSMC_NORSRAMInitStructure.FSMC_MemoryType =
        FSMC_MemoryType_SRAM;

    FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth =
        FSMC_MemoryDataWidth_16b;

    FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode =
        FSMC_BurstAccessMode_Disable;

    FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait =
        FSMC_AsynchronousWait_Disable;

    FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity =
        FSMC_WaitSignalPolarity_Low;

    FSMC_NORSRAMInitStructure.FSMC_WrapMode =
        FSMC_WrapMode_Disable;

    FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive =
        FSMC_WaitSignalActive_BeforeWaitState;

    FSMC_NORSRAMInitStructure.FSMC_WriteOperation =
        FSMC_WriteOperation_Enable;

    FSMC_NORSRAMInitStructure.FSMC_WaitSignal =
        FSMC_WaitSignal_Disable;

    FSMC_NORSRAMInitStructure.FSMC_ExtendedMode =
        FSMC_ExtendedMode_Disable;

    FSMC_NORSRAMInitStructure.FSMC_WriteBurst =
        FSMC_WriteBurst_Disable;

    FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct =
        &p;

    FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct =
        &p;

    FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

    FSMC_NORSRAMCmd(
        FSMC_Bank1_NORSRAM1,
        ENABLE
    );
}

/**
 * @brief  初始化ILI9341寄存器
 * @param  无
 * @retval 无
 */
static void ILI9341_REG_Config(void)
{
    ILI9341_WriteCmd(CMD_RESET);
    ILI9341_DELAY_MS(120);

    ILI9341_WriteCmd(CMD_DISPLAY_OFF);

    ILI9341_WriteCmd(CMD_PIXEL_FORMAT);
    ILI9341_WriteData(0x55);

    ILI9341_WriteCmd(CMD_SLEEP_OUT);
    ILI9341_DELAY_MS(100);

    ILI9341_WriteCmd(CMD_DISPLAY_ON);
    ILI9341_DELAY_MS(100);

    ILI9341_WriteCmd(CMD_GRAM);
    ILI9341_DELAY_MS(5);
}

/**
 * @brief  LCD初始化函数，如果要用到lcd，要调用这个函数
 * @param  无
 * @retval 无
 */
void LCD_Init(void)
{
    ILI9341_GPIO_Config();
    ILI9341_FSMC_Config();

    ILI9341_Rst();
    ILI9341_BackLight(ENABLE);
    ILI9341_REG_Config();

    ILI9341_GramScan(g_LCD_ScanMode);
}

/**
 * @brief  在ILI9341显示器上开辟一个窗口
 * @param  usX：窗口起始X坐标
 * @param  usY：窗口起始Y坐标
 * @param  usWidth：窗口宽度
 * @param  usHeight：窗口高度
 * @retval 无
 */
static void ILI9341_OpenWindow(uint16_t usX,
                               uint16_t usY,
                               uint16_t usWidth,
                               uint16_t usHeight)
{
    ILI9341_WriteCmd(CMD_SetCoordinateX);

    ILI9341_WriteData(usX >> 8);
    ILI9341_WriteData(usX & 0xFF);

    ILI9341_WriteData(
        (usX + usWidth - 1) >> 8
    );

    ILI9341_WriteData(
        (usX + usWidth - 1) & 0xFF
    );

    ILI9341_WriteCmd(CMD_SetCoordinateY);

    ILI9341_WriteData(usY >> 8);
    ILI9341_WriteData(usY & 0xFF);

    ILI9341_WriteData(
        (usY + usHeight - 1) >> 8
    );

    ILI9341_WriteData(
        (usY + usHeight - 1) & 0xFF
    );
}

/**
 * @brief  设置ILI9341的GRAM扫描方向
 * @param  ucOption：选择GRAM扫描方向
 * @retval 无
 */
static void ILI9341_GramScan(uint8_t ucOption)
{
    //参数检查，只可输入0-7
    if (ucOption > 7)
    {
        return;
    }

    g_LCD_ScanMode = ucOption;

    if (ucOption % 2 == 0)
    {
        g_LCD_LenX = ILI9341_LESS_PIXEL;
        g_LCD_LenY = ILI9341_MORE_PIXEL;
    }
    else
    {
        g_LCD_LenX = ILI9341_MORE_PIXEL;
        g_LCD_LenY = ILI9341_LESS_PIXEL;
    }

    ILI9341_WriteCmd(CMD_MAC);

    ILI9341_WriteData(
        0x08 | (ucOption << 5)
    );

    ILI9341_OpenWindow(
        0,
        0,
        g_LCD_LenX,
        g_LCD_LenY
    );

    ILI9341_WriteCmd(CMD_SetPixel);
}

/**
 * @brief  在ILI9341显示器上以某一颜色填充像素点
 * @param  ulAmout_Point：要填充颜色的像素点总数
 * @param  usColor：颜色
 * @retval 无
 */
static void ILI9341_FillColor(uint32_t ulAmout_Point,
                              uint16_t usColor)
{
    uint32_t i = 0;

    ILI9341_WriteCmd(CMD_SetPixel);

    for (i = 0; i < ulAmout_Point; i++)
    {
        ILI9341_WriteData(usColor);
    }
}

/**
  * @brief  设置LCD的前景及背景颜色
  * @param  TextColor：前景颜色
  * @param  BackColor：背景颜色
  * @retval 无
  */
void LCD_SetColors(uint16_t TextColor,
                   uint16_t BackColor)
{
    g_LCD_TextColor = TextColor;
    g_LCD_BackColor = BackColor;
}

/**
  * @brief  获取LCD的前景及背景颜色
  * @param  TextColor：存储前景颜色
  * @param  BackColor：存储背景颜色
  * @retval 无
  */
void LCD_GetColors(uint16_t *TextColor,
                   uint16_t *BackColor)
{
    *TextColor = g_LCD_TextColor;
    *BackColor = g_LCD_BackColor;
}

/**
  * @brief  设置LCD前景颜色
  * @param  Color：前景颜色
  * @retval 无
  */
void LCD_SetTextColor(uint16_t Color)
{
    g_LCD_TextColor = Color;
}

/**
  * @brief  设置LCD背景颜色
  * @param  Color：背景颜色
  * @retval 无
  */
void LCD_SetBackColor(uint16_t Color)
{
    g_LCD_BackColor = Color;
}

/**
 * @brief  对LCD显示器的某一窗口进行清屏
 * @param  usX：窗口起始X坐标
 * @param  usY：窗口起始Y坐标
 * @param  usWidth：窗口宽度
 * @param  usHeight：窗口高度
 * @retval 无
 */
void LCD_Clear(uint16_t usX,
               uint16_t usY,
               uint16_t usWidth,
               uint16_t usHeight)
{
    ILI9341_OpenWindow(
        usX,
        usY,
        usWidth,
        usHeight
    );

    ILI9341_FillColor(
        usWidth * usHeight,
        g_LCD_BackColor
    );
}

/**
 * @brief  对ILI9341显示器的某一点进行填充
 * @param  uX：X坐标
 * @param  uY：Y坐标
 * @retval 无
 */
void LCD_SetPixel(uint16_t uX,
                  uint16_t uY)
{
    if ((uX < g_LCD_LenX) &&
        (uY < g_LCD_LenY))
    {
        ILI9341_OpenWindow(
            uX,
            uY,
            1,
            1
        );

        ILI9341_FillColor(
            1,
            g_LCD_TextColor
        );
    }
}

/**
 * @brief  在LCD显示器上显示一个字模
 * @param  uX：字符起始X坐标
 * @param  uY：字符起始Y坐标
 * @param  uWidth：字模宽度
 * @param  uHeight：字模高度
 * @param  pMask：字模地址
 * @retval 无
 */
static void LCD_DispMask(uint16_t uX,
                         uint16_t uY,
                         uint16_t uWidth,
                         int16_t uHeight,
                         const uint8_t *pMask)
{
    uint16_t i;
    uint16_t j = 0;
    uint16_t uFontLenght = 0;

    ILI9341_OpenWindow(
        uX,
        uY,
        uWidth,
        uHeight
    );

    ILI9341_WriteCmd(CMD_SetPixel);

    uFontLenght = uWidth * uHeight / 8;

    for (i = 0; i < uFontLenght; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (pMask[i] & (0x80 >> j))
            {
                ILI9341_WriteData(
                    g_LCD_TextColor
                );
            }
            else
            {
                ILI9341_WriteData(
                    g_LCD_BackColor
                );
            }
        }
    }
}

/**
 * @brief  在LCD显示器上显示字符串
 * @param  uX：显示起始X坐标
 * @param  uY：显示起始Y坐标
 * @param  uDir：字符串显示方向，0=X方向，非0=Y方向
 * @param  pStr：字符串首地址
 * @retval 无
 */
void LCD_DispStringEN(uint16_t uX,
                      uint16_t uY,
                      uint8_t uDir,
                      char *pStr)
{
    uint8_t *pMask = NULL;

    uint16_t uCharWidth =
        LCD_GetFontEN()->Width;

    uint16_t uCharHeight =
        LCD_GetFontEN()->Height;

    while (*pStr != '\0')
    {
        pMask = LCD_GetMaskEN(*pStr);
        pStr++;

        if (g_LCD_LenX - uX < uCharWidth)
        {
            uX = 0;
            uY += uCharHeight;
        }

        if (g_LCD_LenY - uY < uCharHeight)
        {
            uY = 0;
        }

        LCD_DispMask(
            uX,
            uY,
            uCharWidth,
            uCharHeight,
            pMask
        );

        if (uDir == 0)
        {
            uX += uCharWidth;
        }
        else
        {
            uX += uCharHeight;
        }
    }
}
