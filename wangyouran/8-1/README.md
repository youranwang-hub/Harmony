# 实验8-1 — RTC 实时时钟 + OLED 显示

## 实验概述

基于 STM32F407 的片上 RTC（实时时钟）外设，配合 128×32 OLED 显示屏，实现一个完整的电子时钟功能。显示屏同时显示**时、分、秒**（第一行）和**年、月、日**（第二行），每 400ms 刷新一次。开发板需安装 **CR1220 纽扣电池**，用于在主电源断开后维持 VBAT 域供电，保存 RTC 时间和备份寄存器（BKP）数据。

## 硬件连接

| 功能 | 引脚 / 接口 | 说明 |
|------|------------|------|
| OLED SCL | I2C1（或硬件 I2C） | 128×32 OLED 时钟线 |
| OLED SDA | I2C1（或硬件 I2C） | 128×32 OLED 数据线 |
| RTC 晶振 | PC14 / PC15 | 外部 32.768kHz 低速晶振（LSE） |
| VBAT 电池 | CR1220 纽扣电池座 | 主电断开后维持 RTC + BKP 供电 |
| 调试串口 | PA2 / PA3（USART2） | 预留（本实验未使用串口输出） |

> 注：OLED 和 RTC 底层驱动（`oled.c/h`、`rtc.c/h`）由课程 SDK 提供，本实验目录下仅包含应用层 `main.c`。

## 文件结构

```
8-1/
└── basic/
    └── main.c          # 主程序：OLED 初始化 → RTC 初始化 → 循环刷新显示
```

依赖的外部共享模块：

| 文件 | 来源 | 说明 |
|------|------|------|
| `delay.c/h` | `7-1/basic/` | 软件循环毫秒/微秒延时 |
| `oled.c/h` | 课程 SDK | OLED 128×32 驱动（含中文显示） |
| `rtc.c/h` | 课程 SDK | RTC 初始化、时间/日期读取 |
| `stm32f4xx.h` | ST 标准外设库 | STM32F4 寄存器定义 |

## 核心流程

### 主程序 (`main.c`)

```
上电
 │
 ├─ delay_ms(100)                等待 OLED 上电稳定
 ├─ OLED_Init()                  初始化 OLED 外设
 ├─ OLED_Clear()                 清屏
 │
 ├─ 逐字显示开机标题：
 │   OLED_ShowCHinese(28,0,0)    "青"
 │   OLED_ShowCHinese(46,0,1)    "软"
 │   OLED_ShowCHinese(64,0,2)    "集"
 │   OLED_ShowCHinese(82,0,3)    "团"
 │
 ├─ BSP_RTC_Init()              初始化 RTC（配置 LSE、日历寄存器）
 ├─ delay_ms(1000)              等待 RTC 稳定
 │
 └─ while(1) 主循环（400ms 间隔）：
       │
       ├─ RTC_GetTime(BIN, &RTC_TimeStruct)
       │   获取当前时间：时、分、秒
       │
       ├─ sprintf → " Time:HH:MM:SS  "
       │   OLED_ShowString(0, 0, ...)  → OLED 第 1 行
       │
       ├─ RTC_GetDate(BIN, &RTC_DateStruct)
       │   获取当前日期：年、月、日
       │
       ├─ sprintf → "Date:20YY-MM-DD"
       │   OLED_ShowString(0, 2, ...)  → OLED 第 3 行
       │
       └─ delay_ms(400)
```

### OLED 显示布局

128×32 OLED 共 4 行（每行 16 字符，8×16 字体）：

```
第0行:  Time:HH:MM:SS         ← 时间（覆盖开机时的"青软集团"标题）
第1行:  (空)
第2行:  Date:20YY-MM-DD       ← 日期
第3行:  (空)
```

> 注意：第 0 行先显示"青软集团"开机画面约 1 秒，随后被时间字符串覆盖。

## 核心模块说明

### 1. RTC 实时时钟 (`rtc.c/h` — 课程 SDK)

STM32F407 内置 RTC 是一套独立的 32 位计数器系统，可提供日历时钟功能：

#### RTC 初始化 (`BSP_RTC_Init`)

通常完成以下配置（由 SDK 封装）：

1. **使能 PWR 时钟** → 解除备份域写保护
2. **使能 LSE**（外部 32.768kHz 晶振）→ 配置为 RTC 时钟源
3. **配置 RTC 预分频器** → 异步 127 + 同步 255，获得 1Hz 计数频率
4. **设置初始时间/日期**（首次配置时，后续由备份寄存器判断是否已配置）
5. **使能 RTC**

#### 时间读取 (`RTC_GetTime`)

```c
RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
// 返回结构体成员：
//   RTC_Hours     → 小时 (0-23)
//   RTC_Minutes   → 分钟 (0-59)
//   RTC_Seconds   → 秒   (0-59)
```

`RTC_Format_BIN` 表示返回二进制格式（非 BCD）。

#### 日期读取 (`RTC_GetDate`)

```c
RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
// 返回结构体成员：
//   RTC_Year      → 年 (0-99，对应 2000-2099)
//   RTC_Month     → 月 (1-12)
//   RTC_Date      → 日 (1-31)
```

本实验中年的前缀 `20` 在 `sprintf` 格式化时硬编码添加。

### 2. OLED 显示屏 (`oled.c/h` — 课程 SDK)

128×32 单色 OLED，通过 I2C 总线驱动：

| 函数 | 说明 |
|------|------|
| `OLED_Init()` | 初始化 OLED，配置分辨率、对比度等 |
| `OLED_Clear()` | 清空整个屏幕 |
| `OLED_ShowCHinese(x, y, index)` | 在 (x,y) 显示库中第 index 个中文字符 |
| `OLED_ShowString(x, y, str, size)` | 在 (x,y) 显示 ASCII 字符串，size=16 为 8×16 字体 |

坐标说明：
- `x` 像素横坐标（0~127）
- `y` 行号，每行占 16 像素（0~3，共 4 行）

### 3. 软件延时 (`delay.c/h` — 共享模块)

使用空循环方式实现粗略延时：

```c
void delay_ms(uint16_t nms) {
    while(nms--) {
        i = 33800;       // 约 1ms 对应的循环次数（168MHz 下粗校）
        while(i--);
    }
}
```

延时精度受编译器优化级别影响，但本实验中仅用于 OLED 上电稳定等待、开机动画间隔和主循环刷新间隔，对精度要求不高。

### 4. CR1220 纽扣电池的作用

STM32F407 的 VBAT 引脚连接 CR1220 纽扣电池。当主电源（3.3V）断开时：

- **RTC 继续走时** — 32.768kHz LSE 晶振和 RTC 计数器由 VBAT 供电
- **备份寄存器（BKP）保留** — 共 20 个 32 位备份寄存器，可存储配置标志等
- **首次配置检测** — 通常用 BKP 寄存器标记"RTC 已配置"，避免每次上电重置时间

## 运行效果

上电后 OLED 依次显示：

```
┌──────────────────────┐
│      青软集团        │  ← 逐字出现，约 1 秒后消失
│                      │
│                      │
│                      │
└──────────────────────┘

        ↓ 1 秒后

┌──────────────────────┐
│  Time:14:35:22       │  ← 每秒刷新
│                      │
│ Date:2026-07-13      │  ← 同时刷新
│                      │
└──────────────────────┘
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| STM32 RTC 外设 | 独立电源域（VBAT），32.768kHz LSE 时钟源，日历功能 |
| 备份域供电 | CR1220 纽扣电池维持 RTC 走时和 BKP 数据 |
| I2C OLED 显示 | 128×32 单色屏，支持 ASCII 和中文字库 |
| `RTC_GetTime` / `RTC_GetDate` | 标准外设库 RTC API，返回 BIN/BCD 格式时间日期 |
| `sprintf` 格式化 | 将 RTC 结构体数值格式化为显示字符串 |
| 中文显示 | `OLED_ShowCHinese` + 字库索引，逐字延时展示开机标题 |
| 定时刷新 | 主循环 400ms 间隔，高于 1 秒刷新率保证肉眼连续感 |

## 代码注意事项

1. **OLED 和 RTC 驱动未在仓库中**：`oled.c/h` 和 `rtc.c/h` 由课程 SDK 提供，需在编译时将对应 `.c` 文件加入工程。本仓库仅包含应用层 `main.c`。

2. **`delay.h` 路径**：本实验依赖 `7-1/basic/delay.h` 和 `delay.c`，需在 Keil/IAR 工程中正确设置 include path。

3. **RTC 首次配置**：若 CR1220 电池未安装或电量耗尽，每次复位后 `BSP_RTC_Init` 会重新设置默认时间，需在初始化后通过串口或其他方式校时。

4. **OLED 第 0 行覆盖**：开机标题"青软集团"显示在第 0 行，1 秒后进入主循环即被时间字符串覆盖，属于预期的视觉过渡效果。

5. **延时采用软件循环**：`delay_ms` 使用空循环而非 SysTick，实测值会受编译器优化影响。400ms 刷新间隔的抖动对显示效果无明显影响。
