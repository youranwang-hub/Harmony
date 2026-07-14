# 实验8 — LCD 液晶屏实验

## 实验目的

- 理解 LCD 工作原理与基本组成
- 掌握 ILI9341 芯片原理与交互方法
- 掌握 STM32 的 FSMC 概念与控制方法
- 掌握 ILI9341 的驱动编写方法
- 实现 LCD 字符显示

## 实验概述

LCD 是常用的显示设备，几乎所有物联网项目都会使用。AU100（青软遨游 100）开发板带有 320×240 的液晶屏，本实验通过此液晶屏实现图形数据显示，并通过字模转换的方法实现英文字符的显示。

通过本实验可深入理解 LCD 的控制交互方法，掌握 FSMC 与 8080 的接口使用方法，掌握文字显示原理与提取方法等。实验内容主要如下：

1. LCD 工程创建
2. LCD 初始化实现
3. LCD 基础控制实现
4. LCD 字符显示实现
5. LCD 测试示例实现

本实验采用的是 **ILI9341 + 3.2 寸 TFT 液晶屏**（分辨率 240×320），通过 STM32F4 的 **FSMC（Flexible Static Memory Controller）** 模拟 8080 并行接口与 ILI9341 通讯，达到高速刷屏的效果。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| FSMC D[0:1] | PD14, PD15 | FSMC | 数据线低 2 位 |
| FSMC D[2:3] | PD0, PD1 | FSMC | 数据线 2-3 位 |
| FSMC D[4:7] | PE7~PE10 | FSMC | 数据线 4-7 位 |
| FSMC D[8:12] | PE11~PE15 | FSMC | 数据线 8-12 位 |
| FSMC D[13:15] | PD8, PD9, PD10 | FSMC | 数据线 13-15 位 |
| FSMC NOE | PD4 | FSMC | 输出使能（读控制） |
| FSMC NWE | PD5 | FSMC | 写使能 |
| FSMC NE1 | PD7 | FSMC | Bank1 片选 |
| FSMC A16 | PD11 | FSMC | D/C 数据/命令选择 |
| LCD RST | PE1 | GPIO 推挽输出 | 硬复位 |
| LCD BL | PD12 | GPIO 推挽输出 | 背光控制（低电平点亮） |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

> **数据线映射关系**：FSMC 数据线编号与 GPIO 引脚号不对应（如 `D0 → PD14`、`D2 → PD0`），这是 PCB 走线优化结果，软件层面只需按 FSMC 总线写入即可。

## 文件结构

```
实验8_LCD液晶屏实验/
├── USER/
│   └── main.c                       # 主程序：初始化 + 设置字体颜色 + 显示字符串
├── BSP/
│   ├── lcd_ili9341.c   / .h         # ILI9341 驱动（FSMC + 寄存器 + 绘图 + 字符显示）
│   ├── lcd_fonts.c     / .h         # ASCII 8×16 字模与字体管理
│   └── usart.c         / .h         # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    └── delay.c                      # 软件循环延时
```

## LCD 与 ILI9341 工作原理

### 1. LCD 基本组成

TFT-LCD 显示屏的核心结构：

```
┌──────────────────────────────────────────────┐
│  背光板 (Backlight)                           │  ← 提供均匀光源
│  ↓                                           │
│  偏光片 (Polarizer)                          │
│  ↓                                           │
│  TFT 基板 (玻璃)         ← 行/列驱动          │  ← 薄膜晶体管阵列
│  ↓                                           │
│  液晶层 (Liquid Crystal)  ← 控制偏转         │  ← 控制每个像素透光量
│  ↓                                           │
│  彩色滤光片 (Color Filter) RGB 三色          │  ← 还原色彩
│  ↓                                           │
│  偏光片 (Polarizer)                          │
│  ↓                                           │
│  ITO 公共电极                                │
└──────────────────────────────────────────────┘
```

**关键点**：每个像素由 R / G / B 三个子像素组成，通过控制液晶分子偏转角度改变透光率，配合彩色滤光片得到 16M 色显示（RGB888）或 65K 色（RGB565）。

### 2. ILI9341 控制器

ILI9341 是 ILI Technology 推出的 **240×320 像素 TFT LCD 单芯片驱动器**，内置 172800 字节（320×240×18bit/8 = 172800）GRAM 显存。

| 参数 | 值 |
|------|-----|
| 分辨率 | 240×320（横屏）/ 320×240（竖屏） |
| 色彩深度 | 18 bit/pixel（RGB666） |
| 通讯接口 | 8080 8/9/16/18-bit 并口 / SPI / RGB |
| 内部 GRAM | 172800 字节 |
| 工作电压 | 模拟 2.5~3.3V，I/O 1.65~3.3V |
| 背光方式 | LED，恒流驱动 |

**驱动原理**：MCU 通过 8080 接口向 ILI9341 的 GRAM 写入像素颜色数据，ILI9341 自动按扫描时序刷新到 LCD 玻璃上，**MCU 不需关心 LCD 玻璃的复杂时序控制**，大大降低软件负担。

### 3. 8080 并行接口

ILI9341 使用 8080 风格的并行接口，关键信号线如下：

| 信号 | 名称 | 方向 | 说明 |
|------|------|------|------|
| CS | Chip Select | 输入 | 片选，低有效 |
| WR | Write | 输入 | 写使能，**上升沿**锁存数据 |
| RD | Read | 输入 | 读使能 |
| RS (D/C) | Data/Command | 输入 | **0 = 命令，1 = 数据** |
| RESET | Reset | 输入 | 硬复位 |
| DB[15:0] | Data Bus | 双向 | 16 位数据总线 |

**关键时序**：

```
写命令/数据：
              ┌─────┐
CS ───────────┘     └────────────────────
              ┌─────────────┐
WR ───────────┘             └─────────────  (低电平期间锁存)
                    ┌──────┐
RS ────────────────┘      └───────────────  (决定命令/数据)
              ┌────────────────────┐
DB[15:0] ─────┤  CMD/DATA          ├─────   (在 WR 上升沿锁存)
              └────────────────────┘
```

## FSMC 概念

### 1. FSMC 简介

**FSMC（Flexible Static Memory Controller）** 是 STM32F4 内置的灵活静态存储控制器，可驱动：

- SRAM / PSRAM
- NOR Flash
- NAND Flash
- **8080/6800 并口 LCD**（模拟 SRAM 行为）

FSMC 的核心价值是把对外部存储器的**总线读写操作**转化为对内部地址空间的**普通内存读写**——CPU 写一个 `*(__IO uint16_t*)0x60000000 = 0x002A`，FSMC 硬件自动产生相应的 8080 时序。

### 2. FSMC_Bank1 寻址结构

FSMC_Bank1 包含 4 个 NORSRAM 子区，每个 64MB：

```
FSMC_Bank1 总地址空间：0x60000000 ~ 0x6FFFFFFF (256MB)
  ├── NORSRAM1：0x60000000 ~ 0x63FFFFFF (64MB)  ← 本实验使用
  ├── NORSRAM2：0x64000000 ~ 0x67FFFFFF
  ├── NORSRAM3：0x68000000 ~ 0x6BFFFFFF
  └── NORSRAM4：0x6C000000 ~ 0x6FFFFFFF
```

每个 64MB 子区分为 4 个 16MB 区域，由 **HADDR[27:26]** 选择；**HADDR[25:0]** 寻址 16MB 内部空间。

### 3. 8080 接口与 FSMC 地址映射

ILI9341 仅有 2 类操作：**写命令**和**写数据**。FSMC 用 **A16** 区分这两种操作：

```
HADDR[25:0] 内地址空间，16MB
  ├── A[25:0] = 0x000000 (命令地址)   ← HADDR[16] = 0
  │   → 0x60000000
  └── A[25:0] = 0x020000 (数据地址)   ← HADDR[16] = 1
      → 0x60000000 + 0x20000 = 0x60020000
      (即 A16=1, 偏移 2^16 × sizeof(uint16_t) = 0x20000)
```

代码中定义：

```c
#define FSMC_Addr_ILI9341_CMD   ((uint32_t)0x60000000)  // RS=0  写命令
#define FSMC_Addr_ILI9341_DATA  ((uint32_t)0x60020000)  // RS=1  写数据

#define ILI9341_WriteCmd(usCmd)   *(__IO uint16_t*)(FSMC_Addr_ILI9341_CMD)  = usCmd;
#define ILI9341_WriteData(usData) *(__IO uint16_t*)(FSMC_Addr_ILI9341_DATA) = usData;
```

> 写入 `*0x60000000` 时，FSMC 自动拉低 A16（RS=0），写入 `*0x60020000` 时 A16 拉高（RS=1）。CPU 无感操作，硬件自动完成 8080 时序。

## 核心模块说明

### 1. GPIO 配置 (`ILI9341_GPIO_Config`)

LCD 占用 21 个 GPIO 引脚（20 个 FSMC 数据/控制 + 1 个复位/背光）：

| 引脚 | 模式 | 复用 |
|------|------|------|
| PD0,1,4,5,7,8,9,10,11,14,15 | AF | GPIO_AF_FSMC |
| PE1 | OUT 推挽 | 复位控制 |
| PE7~PE15 | AF | GPIO_AF_FSMC |
| PD12 | OUT 推挽 | 背光控制 |

```c
GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;     // 复用为 FSMC
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // FSMC 高速通讯
GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    // 推挽
GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL; // FSMC 总线有外部上下拉
```

### 2. FSMC 配置 (`ILI9341_FSMC_Config`)

```c
FSMC_NORSRAMInitStructure.FSMC_Bank             = FSMC_Bank1_NORSRAM1;
FSMC_NORSRAMInitStructure.FSMC_DataAddressMux   = FSMC_DataAddressMux_Disable;
FSMC_NORSRAMInitStructure.FSMC_MemoryType       = FSMC_MemoryType_SRAM;        // 模拟 SRAM
FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth  = FSMC_MemoryDataWidth_16b;    // 16位数据
FSMC_NORSRAMInitStructure.FSMC_AccessMode       = FSMC_AccessMode_A;           // 模式 A
FSMC_NORSRAMInitStructure.FSMC_WriteOperation   = FSMC_WriteOperation_Enable;

p.FSMC_AddressSetupTime = 0x6;  // 地址建立时间 6 HCLK
p.FSMC_AddressHoldTime  = 0;
p.FSMC_DataSetupTime    = 0x6;  // 数据建立时间 6 HCLK
```

**关键时序说明**：

| 参数 | 含义 | 关系 |
|------|------|------|
| `AddressSetupTime` | CS 拉低到 WR 拉低的时间 | 必须满足 LCD 数据建立时间 |
| `DataSetupTime` | WR 拉低到 WR 拉高的时间 | 必须满足 LCD 数据保持时间 |
| `AccessMode` | 模式 A：读/写共用一套时序 | 最简洁，ILI9341 完全够用 |

STM32F4 HCLK = 168MHz，6 个 HCLK ≈ 35.7ns，远小于 ILI9341 的最小建立时间（约 100ns），因此通讯稳定可靠。

### 3. 软件复位 (`ILI9341_Rst`)

```c
GPIO_ResetBits(ILI9341_RST_PORT, ILI9341_RST_PIN);  // 拉低复位
delay_ms(5);
GPIO_SetBits(ILI9341_RST_PORT, ILI9341_RST_PIN);    // 释放
delay_ms(5);
```

通过 GPIO 拉低 RST 引脚 5ms 触发 ILI9341 硬件复位（等同 `CMD_RESET 0x0001` 软件复位），确保 LCD 控制器进入已知初始状态。

### 4. ILI9341 寄存器配置 (`ILI9341_REG_Config`)

```c
ILI9341_WriteCmd(CMD_RESET);          // 软件复位（备选）
delay_ms(120);

ILI9341_WriteCmd(CMD_PIXEL_FORMAT);
ILI9341_WriteData(0x55);              // 16bit/pixel RGB565

ILI9341_WriteCmd(CMD_SLEEP_OUT);      // 退出睡眠模式
delay_ms(100);

ILI9341_WriteCmd(CMD_DISPLAY_ON);     // 开启显示
delay_ms(100);
```

**精简初始化流程**（生产环境可省略大部分参数寄存器，使用 ILI9341 默认值）：

| 命令 | 编号 | 作用 |
|------|------|------|
| `CMD_RESET` | 0x0001 | 软件复位，等价于拉低 RST 引脚 |
| `CMD_PIXEL_FORMAT` | 0x003A | 设置像素格式，`0x55` = 16bit RGB565 |
| `CMD_SLEEP_OUT` | 0x0011 | 退出睡眠（唤醒后需 ≥5ms 稳定） |
| `CMD_DISPLAY_ON` | 0x0029 | 打开显示输出 |

### 5. GRAM 扫描方向 (`ILI9341_GramScan`)

ILI9341 通过 `CMD_MAC (0x36)` 寄存器的高 3 位控制扫描方向：

```c
ILI9341_WriteData(0x08 | (ucOption << 5));  // 0x08 固定位 | 扫描方向
```

`ucOption` 取值 0~7，对应 8 种显示方向（含 90° 旋转）：

| ucOption | X 长度 | Y 长度 | 说明 |
|:--------:|:------:|:------:|------|
| 0 | 240 | 320 | 竖屏，左上原点 |
| 1 | 320 | 240 | 横屏，左上原点 |
| 2 | 240 | 320 | 竖屏，右上原点 |
| 3 | 320 | 240 | 横屏，左下原点 |
| 4 | 240 | 320 | 竖屏，右下原点 |
| 5 | 320 | 240 | 横屏，右上原点 |
| **6** | **240** | **320** | **本实验默认：竖屏，左下原点（标准显示方向）** |
| 7 | 320 | 240 | 横屏，右下原点 |

**偶数模式（0,2,4,6）**：X=240（短边）、Y=320（长边）  
**奇数模式（1,3,5,7）**：X=320（长边）、Y=240（短边）

### 6. 窗口设置 (`ILI9341_OpenWindow`)

ILI9341 支持在指定矩形窗口内连续写像素：

```c
ILI9341_WriteCmd(CMD_SetCoordinateX);                    // 0x2A
ILI9341_WriteData(usX >> 8);                             // 起始 X 高 8 位
ILI9341_WriteData(usX & 0xFF);                           // 起始 X 低 8 位
ILI9341_WriteData((usX + usWidth - 1) >> 8);             // 结束 X 高 8 位
ILI9341_WriteData((usX + usWidth - 1) & 0xFF);           // 结束 X 低 8 位

ILI9341_WriteCmd(CMD_SetCoordinateY);                    // 0x2B
ILI9341_WriteData(usY >> 8);
ILI9341_WriteData(usY & 0xFF);
ILI9341_WriteData((usY + usHeight - 1) >> 8);
ILI9341_WriteData((usY + usHeight - 1) & 0xFF);

ILI9341_WriteCmd(CMD_SetPixel);                          // 0x2C, 之后连续 WriteData 即填充窗口
```

**设计思想**：先开窗（一次设置范围），然后 `CMD_SetPixel` + 多次 `WriteData` 即可顺序填充窗口内所有像素，**自动按 X→Y 顺序递增**——这是后续字符显示、画线、画矩形等所有绘图操作的基础。

### 7. 颜色配置 (RGB565)

ILI9341 配置为 **RGB565** 格式（16bit/pixel）：

```
  15    11 10    5 4     0
┌────┐  ┌────┐  ┌────┐
│ RRRRR│  │ GGGGGG│  │ BBBBB│
└────┘  └────┘  └────┘
  5位      6位      5位     共 16 位
```

| 颜色 | RGB565 | 二进制 |
|------|--------|-------|
| WHITE | 0xFFFF | 11111 111111 11111 |
| BLACK | 0x0000 | 00000 000000 00000 |
| RED | 0xF800 | 11111 000000 00000 |
| GREEN | 0x07E0 | 00000 111111 00000 |
| BLUE | 0x001F | 00000 000000 11111 |
| YELLOW | 0xFFE0 | 11111 111111 00000 |
| CYAN | 0x7FFF | 00000 111111 11111 |
| MAGENTA | 0xF81F | 11111 000000 11111 |
| GREY | 0xF7DE | 11110 111111 01110 |

### 8. 基础绘图 API

```c
void LCD_Clear(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight);
// 以背景色清屏指定矩形窗口

void LCD_SetPixel(uint16_t usX, uint16_t usY);
// 绘制单个像素（前景色）

// 以下接口在头文件中声明但 .c 文件未实现，预留给后续扩展：
void LCD_DrawLine(...);         // 画线
void LCD_DrawRectangle(...);    // 画矩形（可空心/实心）
void LCD_DrawCircle(...);       // 画圆
```

**清屏原理**（`LCD_Clear`）：

```c
ILI9341_OpenWindow(usX, usY, usWidth, usHeight);   // 1. 设置窗口
ILI9341_FillColor(usWidth * usHeight, g_LCD_BackColor);  // 2. 填入背景色
```

### 9. 字模显示

#### 字体结构

```c
typedef struct _tFont {
    const uint8_t *table;   // 字模数据指针
    uint16_t Width;         // 字符宽度（像素）
    uint16_t Height;        // 字符高度（像素）
} FONT;
```

#### ASCII 8×16 字模

`lcd_fonts.c` 中提供 **ASCII_8x16** 字体（95 个可打印字符）：

```c
const uint8_t ASCII8x16_Table[] = {
    0x00,0x00,...,0x00,  // 0x20 ' ' 空格 (16字节)
    0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x08,0x00,0x08,0x18,0x00,0x00,0x00,  // 0x21 '!'
    ...
    0x00,0x00,0x00,0x00,0x3c,0x66,0x42,0x47,0x5b,0x73,0x42,0x66,0x3c,0x00,0x00,0x00,  // 0x30 '0'
};
FONT ASCII_8x16 = { ASCII8x16_Table, 8, 16 };
```

**字模数据格式**：

```
8×16 点阵 = 128 位 = 16 字节 / 字符
每个字节 8 位对应一行（X 方向 8 个像素）
字节内 MSB → LSB 对应 字符左 → 右
字节间顺序 对应 字符上 → 下
```

#### 字模提取

```c
uint8_t *LCD_GetMaskEN(char cChar) {
    // 起点为 ' ' (ASCII 0x20)，字符 cChar 的字模在表中偏移：
    return (uint8_t*)(g_LCD_FontEN->table + (cChar - ' ') * (Width*Height)/8);
}
```

例如提取 `'A'`（ASCII 0x41）：

```
偏移 = (0x41 - 0x20) × 8×16/8 = 33 × 16 = 528 字节
即 ASCII8x16_Table[528..543] 这 16 字节
```

#### 单字符显示 (`LCD_DispMask`)

```c
void LCD_DispMask(uint16_t uX, uint16_t uY, uint16_t uWidth, uint16_t uHeight, const uint8_t *pMask) {
    uint16_t uFontLenght = uWidth * uHeight / 8;  // 8×16 = 16 字节

    ILI9341_OpenWindow(uX, uY, uWidth, uHeight);
    ILI9341_WriteCmd(CMD_SetPixel);

    for (i = 0; i < uFontLenght; i++) {         // 逐字节
        for (j = 0; j < 8; j++) {               // 8 位
            if (pMask[i] & (0x80 >> j))         // MSB 在前
                ILI9341_WriteData(g_LCD_TextColor);   // 1 = 前景色
            else
                ILI9341_WriteData(g_LCD_BackColor);   // 0 = 背景色
        }
    }
}
```

**绘制流程**：开窗 → 命令 0x2C（开始 GRAM 写入）→ 按 MSB→LSB 顺序写入每点颜色 → 由于已开窗，数据按 X→Y 自动排列。

#### 字符串显示 (`LCD_DispStringEN`)

```c
void LCD_DispStringEN(uint16_t uX, uint16_t uY, uint8_t uDir, char *pStr) {
    uint16_t uCharWidth  = LCD_GetFontEN()->Width;   // 8
    uint16_t uCharHeight = LCD_GetFontEN()->Height;  // 16

    while (*pStr != '\0') {
        uint8_t *pMask = LCD_GetMaskEN(*pStr);   // 查表
        pStr++;

        // 越界处理：X 不足则换行，Y 不足则回到顶部
        if (g_LCD_LenX - uX < uCharWidth)  { uX = 0; uY += uCharHeight; }
        if (g_LCD_LenY - uY < uCharHeight) { uY = 0; }

        LCD_DispMask(uX, uY, uCharWidth, uCharHeight, pMask);

        if (uDir == 0)  uX += uCharWidth;        // 水平方向（uDir=0）
        else            uX += uCharHeight;       // 垂直方向（未用）
    }
}
```

**字模的提取方法**（软件工具）：

常用工具：**PCtoLCD2002**、**字模提取 V2.2** 等，设置如下参数后生成 `const uint8_t table[]`：
- 字体：宋体 12 / Times New Roman 8×16
- 取模方式：横向取模（X 方向）
- 字节顺序：MSB 在前（即高位对应左侧）
- 输出格式：C51 格式（A `0xXX,0xXX,...`）

### 10. 主程序测试逻辑 (`main.c`)

```c
int main(void) {
    uint8_t pTxData[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

    UART2_Init(115200);
    printf("LCD TEST\r\n");

    LCD_Init();                                              // 初始化 LCD

    LCD_SetFontEN(&ASCII_8x16);                              // 选择字体
    LCD_SetColors(RED, BLACK);                               // 前景红色 / 背景黑色
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());           // 全屏清屏

    LCD_DispStringEN(0, LINE_EN(0), 0, "3.2 inch LCD:");                       // 第 1 行
    LCD_DispStringEN(0, LINE_EN(1), 0, "Image resolution:240x320 px");         // 第 2 行

    while (1) { }
}
```

**核心宏 `LINE_EN(x)`**：

```c
#define LINE_EN(x) ((x) * (((FONT *)LCD_GetFontEN())->Height))
// LINE_EN(0) = 0    （首行 Y 起点）
// LINE_EN(1) = 16   （第二行 Y 起点，= 字体高）
// LINE_EN(2) = 32   ...
```

**显示效果**（开发板 LCD 上）：

```
+--------------------------------+
| 3.2 inch LCD:                  |   ← 红色字体，黑色背景
| Image resolution:240x320 px    |
|                                |
|                                |
|                                |
+--------------------------------+
```

### 11. USART2 调试输出 (`usart.c`)

- `fputc()` 重定向 → `printf` 输出到 USART2
- `fgetc()` 重定向 → `scanf/getchar` 从 USART2 读取
- 串口接收中断回显

## 运行效果

连接串口助手（115200, 8N1），上电后：

```
LCD TEST
```

同时 3.2 寸 LCD 屏幕显示（黑底红字）：

```
┌──────────────────────────┐
│ 3.2 inch LCD:            │   ← 8×16 ASCII 字体
│ Image resolution:240x320 px │
│                          │
│         (黑色背景)         │
│                          │
└──────────────────────────┘
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| TFT-LCD 原理 | 液晶分子偏转 + 彩色滤光片实现彩色显示 |
| ILI9341 控制器 | 240×320 分辨率，集成 GRAM 与时序驱动 |
| 8080 并口 | CS / RS / WR / RD + 16-bit 数据总线 |
| FSMC | STM32 灵活存储控制器，把 LCD 模拟为 SRAM |
| 模拟 SRAM | CPU 直接对地址空间 `*0x60000000` 写入即产生 8080 时序 |
| 地址映射技巧 | A16=0 → 命令地址 0x60000000；A16=1 → 数据地址 0x60020000 |
| 显存 GRAM | ILI9341 内置 172800 字节显存，CPU 不需维护 |
| RGB565 色彩 | 16 位色，5R/6G/5B，节省存储与带宽 |
| 扫描方向 | CMD_MAC (0x36) 寄存器高 3 位控制 8 种方向 |
| 窗口写像素 | CMD_SetCoordinateX/Y + 连续 WriteData 自动 X→Y 顺序 |
| 字模点阵 | 8×16 字符用 16 字节位图表示，1 bit = 1 像素 |
| 字符位置计算 | `(cChar - ' ') * 字符字节数` 定位字模表项 |
| MSB 优先 | 字模字节内 MSB → LSB 对应字符左 → 右 |
| 字体管理 | FONT 结构体封装字模数据 + 尺寸，支持动态切换字体 |
| 越界处理 | 字符串显示自动换行 + 顶部回卷 |
| printf 重定向 | `fputc` 映射到 USART2，方便调试信息输出 |

## 代码注意事项

1. **FSMC 与 GPIO 复用关系**：FSMC 的 16 根数据线与 21 根控制/地址线映射到 STM32F407 固定的 GPIO（见硬件连接表），不可更改。如果 PCB 走线改动，必须同步修改 `GPIO_PinAFConfig` 与 `FSMC_*` 时序。

2. **A16 偏移量计算**：命令地址 0x60000000 与数据地址 0x60020000 相差 0x20000 = 2^16 × 2。这是因为 16 位数据宽度下，每个地址单元对应 2 字节，A16 拉高 1 对应地址增加 `1 × 2 = 2 字节** N 倍？** 不对，正确的理解是：
   - HADDR 寻址单位是 **字节**（1 字节）
   - 16 位数据宽度下，每次读写 2 字节，因此 `A16=1` 对应 HADDR 增加 0x10000（64K）= 64K×2=128K 字节
   - 但 ILI9341 寄存器是 16 位的，所以 HADDR 跳变 0x20000 后才能改变 A16 状态
   - 代码中使用 `0x60000000 + 0x20000 = 0x60020000` 是**对齐后的 16 位访问基址**，写入时 FSMC 自动产生对应的 8080 写时序

3. **精简初始化可能显示异常**：本实验仅配置了 4 个关键寄存器（RESET、PIXEL_FORMAT、SLEEP_OUT、DISPLAY_ON），**省略了伽马校正、电源时序、帧率控制等寄存器**。在部分 LCD 模块上可能显示偏色或闪烁，**生产环境** 应补全完整寄存器序列（参考 ILI9341 数据手册第 8 章）。

4. **预留接口未实现**：`lcd_ili9341.h` 中声明了 `LCD_DrawLine`、`LCD_DrawRectangle`、`LCD_DrawCircle` 等绘图函数，但 `.c` 文件中**未提供实现**。如需画线/画矩形等高级图形，需自行实现：
   - 画水平线：`for(x=0; x<w; x++) SetPixel(x, y);`
   - 画垂直线：`for(y=0; y<h; y++) SetPixel(x, y);`
   - 画矩形：组合 4 条线（空心）或循环填充（实心）
   - 画圆：使用 Bresenham 圆算法或中点圆算法

5. **字模数组固定 `0x20` 为起点**：`LCD_GetMaskEN` 中 `(cChar - ' ')` 假设字模表从空格字符（ASCII 0x20）开始。如果替换为不含空格的字模表（如自定义图标），需修改此偏移量。

6. **`delay_ms` 使用软件循环**：`UTILS/delay.c` 中的 `delay_ms` 基于空循环实现，受编译优化级别和 CPU 频率影响，**LCD 初始化中的 5ms/100ms/120ms 延时如果过短会导致 LCD 初始化失败**。生产环境建议改用 SysTick 定时器实现精确毫秒延时。

7. **越界处理粗暴**：`LCD_DispStringEN` 在 X/Y 越界时分别将 `uX=0` / `uY=0`，**不会清屏**，会直接覆盖屏幕顶部已有内容。生产环境应增加滚动或裁剪逻辑。

8. **横屏切换后 X/Y 长度变化**：扫描方向变化时 `g_LCD_LenX`、`g_LCD_LenY` 会自动更新（如方向 1 变为 X=320, Y=240），用户调用 `LCD_GetLenX/Y` 获取的是当前方向的尺寸。**应用层不要硬编码 240/320**。

9. **FSMC 时序裕量**：本实验配置 `AddressSetupTime=6`、`DataSetupTime=6`（HCLK 周期），约 35.7ns。ILI9341 最小建立时间 10ns，保持时间 10ns，**裕量充足**。如使用更慢的 LCD（如 ST7789V），可能需要加大时序配置。

10. **FSMC 写操作自动同步**：通过 `*(__IO uint16_t*)addr = data` 写入时，编译器会插入内存屏障（Memory Barrier），CPU 不会乱序优化，**数据安全**。但建议关键写操作前后加 `__DSB()` 指令防止极端情况下的指令重排。
