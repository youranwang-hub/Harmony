# 基于 STM32F407 的危险品运输与仓储安全监控系统

本工程是基于 QST-AU100 综合实验开发板和 STM32F407VE 的综合实验项目，围绕“危险品运输与仓储安全监控”这一真实应用场景，将前期多个基础外设实验中已经验证过的模块重新组织成一个完整系统。系统不再只是单独演示某一个外设，而是围绕身份认证、物资加入/领出、环境监测、气体报警、场景模拟、日志记录、参数设置、设备控制和触摸交互形成闭环。

项目采用 STM32F4 标准外设库工程，核心目标不是引入新的框架，而是在原有实验模板和已完成实验驱动的基础上进行整合。当前工程保留了真实硬件路径，也考虑了课堂实验条件下 GPS 不具备真实定位、气体浓度变化不明显、外部 CAN 节点不一定接入等限制，因此采用“真实模块 + 演示/回放/适配器”的混合实现方式。LCD、触摸屏、NFC、指纹、按键、LED、蜂鸣器、ADC、DAC、PWM、USART、DMA、CAN 回环等功能均被纳入系统流程。

## 1. 项目定位

本系统模拟危险品从仓库登记、身份核验、物资加入或领出、运输监控、异常报警、日志追溯到设备控制的全过程。系统上电后首先进入身份认证页面，用户需要完成 NFC 和指纹双重认证，或者在模块无法识别时通过备用密码 `25531460` 完成验证。登录后进入 HOME 主界面，主界面提供 8 个触摸方块入口：

| 方块 | 页面 | 作用 |
|---|---|---|
| `1 MONITOR` | 实时监控页 | 查看温湿度、气体值、数据源、GPS 回放状态、CAN 回环状态和报警等级 |
| `2 SOURCE` | 气体数据源页 | 在 POT、DAC、SCENE、UART、REAL_MQ2 之间切换气体数据来源 |
| `3 SCENE` | 场景模拟页 | 触发正常、慢速泄漏、快速泄漏、GPS 丢失、路线偏离、CAN 离线、非法开箱等场景 |
| `4 LOG` | 日志页 | 查看、翻页、导出和清除系统日志 |
| `5 SETTING` | 参数设置页 | 调整 L1、L2、L3 报警阈值并保存或恢复默认值 |
| `6 AUTH` | 认证页 | 返回 NFC/指纹注册与验证页面 |
| `7 CARGO` | 物资页 | 进行物资加入、领出、清单查看和身份确认 |
| `8 DEVICE` | 设备控制页 | 控制蜂鸣器、LED、风扇 PWM、DAC 输出和报警复位 |

所有页面内部按钮均与触摸坐标绑定，触摸后串口会输出触摸点和操作结果，例如：

```text
[TOUCH] x=75 y=117
[UI TOUCH] page=DEVICE x=75 y=117
[UI] DEVICE LED R OK
```

串口命令仍然保留，主要用于调试、备选验证、快速切换页面、设置气体值、输入物资名称和数量、导出日志等。也就是说，本工程支持“LCD 触摸操作”和“串口命令操作”两条验证线路。

## 2. 开发环境

| 项目 | 内容 |
|---|---|
| 单片机 | STM32F407VE |
| 开发板 | 青软 QST-AU100 综合实验开发板 |
| IDE | Keil uVision5 / MDK 5.25 |
| 编译器 | ARMCC V5 |
| 固件库 | STM32F4xx_DSP_StdPeriph_Lib_V1.4.0 / STM32F4 标准外设库 |
| 下载调试 | CMSIS-DAP，SWD 模式 |
| 串口调试 | USART2，115200，8N1 |
| 工程入口 | `MDK/Project.uvprojx` |
| 主程序入口 | `USER/main.c` |

工程使用标准库宏定义：

```text
STM32F40_41xxx,USE_STDPERIPH_DRIVER
```

工程没有引入 HAL，也没有使用 CubeMX 生成代码。

## 3. 工程目录结构

当前工程根目录结构如下：

```text
STM32F407_Hazard_Monitor_System
├── CORE
│   ├── startup_stm32f40_41xxx.s
│   └── core_cm4*.h
├── FWLIB
│   ├── inc
│   └── src
├── HARDWARE
│   ├── LCD
│   ├── TOUCH
│   ├── SENSOR
│   ├── COMMUNICATION
│   ├── STORAGE
│   ├── MODULE
│   ├── CONTROL
│   ├── SIMULATION
│   └── UI
├── LISTINGS
├── MDK
│   └── Project.uvprojx
├── OBJ
├── REFERENCE_DRIVERS
├── USER
└── UTILS
```

各目录作用如下：

| 目录 | 作用 |
|---|---|
| `CORE` | Cortex-M4 内核头文件和启动文件 |
| `FWLIB` | STM32F4 标准外设库源码和头文件 |
| `USER` | 主入口、系统配置、数据类型、系统时钟和中断管理 |
| `UTILS` | 延时等通用工具 |
| `HARDWARE/LCD` | LCD 适配层，调用 ILI9341 参考驱动绘制页面 |
| `HARDWARE/TOUCH` | XPT2046 触摸采样、坐标校准、触摸事件上报 |
| `HARDWARE/SENSOR` | ADC、DAC、气体数据源管理、温湿度和 MQ-2 适配 |
| `HARDWARE/COMMUNICATION` | USART2 调试命令、DMA 串口导出、CAN 回环通信 |
| `HARDWARE/STORAGE` | 物资库存、日志、EEPROM 参数适配、Flash 日志适配 |
| `HARDWARE/MODULE` | NFC、指纹、GPS、RTC 相关模块 |
| `HARDWARE/CONTROL` | LED、蜂鸣器、按键、外部中断、报警、PWM 风扇和系统状态 |
| `HARDWARE/SIMULATION` | 气体泄漏、GPS 丢失、路线偏离、CAN 离线等场景模拟 |
| `HARDWARE/UI` | LCD 页面、按钮表、触摸命中和页面动作处理 |
| `REFERENCE_DRIVERS` | 从旧实验复制来的参考驱动，主要用于 LCD、XPT、DHT11、MQ2、GPS、FPR、NFC 的对照和移植 |

## 4. 当前实际使用的模块与功能对照

下表只列出本综合工程实际使用到的模块，不列出未接入的实验内容。

| 类别 | 模块或资源 | 在系统中的具体作用 |
|---|---|---|
| 主控与显示 | STM32F407VE、ILI9341 LCD、XPT2046 触摸屏 | 完成任务调度、页面显示、触摸操作和状态反馈 |
| 身份认证 | PN532 NFC、ZN632/FPR 指纹模块、备用密码 | 用于注册、验证、双重认证和关键操作确认 |
| 环境与气体 | ADC、MQ-2 接口、PC1 电位器、DAC、温湿度适配器 | 采集或模拟气体值、温湿度值，驱动分级报警 |
| 通信与日志 | USART2、DMA、CAN 回环、日志适配器 | 完成调试命令、日志导出、报警报文和状态回环 |
| 参数与记录 | EEPROM 适配器、Flash 日志适配器、RAM 库存 | 保存阈值参数、记录事件、维护物资清单 |
| 执行与报警 | LED、蜂鸣器、按键、EXTI0、TIM3 PWM、DAC 输出 | 实现状态指示、三级报警响铃、按键/外部中断消音、风扇联动和模拟控制电压 |
| 定位与场景 | GPS 回放/模拟状态 | 用于运输路线、GPS 丢失和路线偏离场景验证，不作为真实定位结果 |

## 5. 系统配置文件

全局配置集中在 `USER/system_config.h`。几个最重要的宏如下：

```c
#define APP_UART_BAUD               115200U
#define APP_UI_REFRESH_MS           500U
#define APP_LOG_PERIOD_MS           5000U

#define APP_ENABLE_REAL_LCD         1
#define APP_ENABLE_REAL_TOUCH       1
#define APP_ENABLE_REAL_DHT11       0
#define APP_ENABLE_REAL_FINGERPRINT 1
#define APP_ENABLE_REAL_NFC         1
#define APP_ENABLE_REAL_EEPROM      0
#define APP_ENABLE_REAL_SPI_FLASH   0

#define APP_ADC_GAS_MAX             1000U
#define APP_GAS_ATTENTION_ENTER     400U
#define APP_GAS_WARN_ENTER          550U
#define APP_GAS_DANGER_ENTER        680U
#define APP_GAS_CRITICAL_ENTER      780U

#define APP_ADC_CH_MQ2_REAL         ADC_Channel_12  /* PC2 */
#define APP_ADC_CH_POT              ADC_Channel_11  /* PC1 */
#define APP_ADC_CH_DAC_LOOP         ADC_Channel_1   /* PA1 */
#define APP_ADC_CH_BATTERY          ADC_Channel_13  /* PC3 */

#define APP_GAS_STARTUP_PROTECT_MS  30000U
```

说明：

- `APP_ENABLE_REAL_LCD=1`、`APP_ENABLE_REAL_TOUCH=1` 表示 LCD 和触摸屏按真实硬件运行。
- `APP_ENABLE_REAL_FINGERPRINT=1`、`APP_ENABLE_REAL_NFC=1` 表示指纹和 NFC 尝试真实模块通信。
- `APP_ENABLE_REAL_DHT11=0` 表示当前温湿度使用适配器值，默认 26.4 ℃、58%，场景可增加偏移。
- `APP_ENABLE_REAL_EEPROM=0`、`APP_ENABLE_REAL_SPI_FLASH=0` 表示 EEPROM 和 Flash 逻辑使用 RAM 适配器，适合课堂演示。
- 气体有效值统一映射为 `0~1000`，所有报警判断都基于这个统一量程。
- 电位器通道使用 `PC1 / ADC_Channel_11`，与基础实验中使用的硬件连接保持一致。

## 6. 程序入口与任务调度

主入口非常简洁，位于 `USER/main.c`：

```c
#include "stm32f4xx.h"
#include "app_main.h"

int main(void)
{
    App_Init();
    while (1) {
        App_Run();
    }
}
```

系统初始化和主循环位于 `USER/app_main.c`。初始化顺序如下：

```c
void App_Init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    UART2_Init(APP_UART_BAUD);
    UART_SendString(USART2, "\r\n=== Hazmat Transport Monitor boot ===\r\n");
    Board_GPIO_Init();
    Board_ADC_Init();
    Board_DAC_Init();
    Board_PWM_Init();
    Board_RTC_Init();
    Board_CAN_Init();
    Board_DMA_USART2_Init();
    Adapter_EEPROM_Init();
    Adapter_Auth_Init();
    Adapter_DHT11_Init();
    Adapter_Touch_Init();
    App_Log_Init();
    App_Cargo_Init();
    ...
    App_UI_Init();
    App_UI_SetPage(UI_AUTH);
    Board_Timer_Init();
}
```

初始化设计要点：

1. 串口优先初始化，便于从上电第一刻开始输出调试信息。
2. GPIO 初始化中会关闭蜂鸣器，防止复位后蜂鸣器误响。
3. LCD/触摸初始化不依赖 GPS、CAN 或物资流程，避免某个模块阻塞导致屏幕黑屏。
4. 系统默认进入 `UI_AUTH` 页面，未认证前无法进入 HOME 后的业务页面。
5. SysTick 在 `Board_Timer_Init()` 中配置为 1 ms 节拍，用于任务轮询和计时。

`App_Run()` 采用轻量轮询调度，不使用 RTOS：

```c
void App_Run(void)
{
    if (UART2_ReadLine(line, sizeof(line))) {
        App_Cmd_ProcessLine(line);
    }

    if (Adapter_Touch_GetEvent(&touch)) {
        App_UI_HandleTouch(touch.x, touch.y);
    }

    key_task();
    sensor_task();
    task_1s();
    App_UI_Render(&s_snapshot);
    App_Log_Add(LOG_TRANSPORT, "periodic snapshot");
}
```

实际代码中各任务有时间间隔控制：

| 任务 | 周期 | 作用 |
|---|---:|---|
| 串口行解析 | 有输入时 | 执行 HELP、PAGE、GAS、SCENE、CARGO 等命令 |
| 触摸扫描 | 50 ms | 获取 XPT2046 触摸事件并分发到 UI |
| 按键扫描 | 50 ms | 报警时按键消音，输出 `[KEY]` 状态 |
| 传感器采样 | 500 ms | 更新气体值、温湿度、电压、报警等级 |
| 场景/GPS/CAN | 1000 ms | 推进模拟场景、GPS 回放、CAN 心跳 |
| LCD 刷新 | 500 ms | 根据当前页面重绘按钮和状态 |
| 周期日志 | 5000 ms | 记录运输快照 |

串口状态输出不是固定 1 秒打印，而是由 `report_status_if_changed()` 控制，仅在状态改变、数据源改变、报警改变、登录状态改变或气体值变化超过步进时输出。

```c
#define APP_STATUS_GAS_REPORT_STEP 20U

static uint8_t status_has_changed(void)
{
    if (s_snapshot.alarm != s_last_reported_snapshot.alarm ||
        s_snapshot.gas_source != s_last_reported_snapshot.gas_source ||
        s_snapshot.scene != s_last_reported_snapshot.scene ||
        App_Alarm_IsMuted() != s_last_reported_muted) {
        return 1U;
    }

    if (diff_u16(s_snapshot.gas_value,
                 s_last_reported_snapshot.gas_value) >= APP_STATUS_GAS_REPORT_STEP) {
        return 1U;
    }

    return 0U;
}
```

## 7. LCD 与触摸屏页面系统

### 7.1 LCD 驱动适配层

LCD 适配层位于 `HARDWARE/LCD/lcd.c`，真实 LCD 模式下调用 ILI9341 FSMC 驱动：

```c
void Adapter_LCD_Init(void)
{
#if APP_ENABLE_REAL_LCD
    LCD_Init();
    LCD_SetFontEN(&ASCII_8x16);
    LCD_SetColors(WHITE, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_DispStringEN(0, LINE_EN(0), 0, "Hazmat Monitor Boot");
    UART_SendString(USART2, "[LCD] ILI9341 FSMC driver enabled.\r\n");
#endif
}
```

页面统一通过 `Adapter_LCD_DrawButtonPage()` 绘制。该函数会先清屏，再绘制标题、状态文本和按钮。按钮背景色来自按钮表，文字统一为白色。报警页标题或正文包含 `ALARM` 时，标题文字会切换为红色。

```c
void Adapter_LCD_DrawButtonPage(const char *title,
                                const char *body,
                                const LcdButton *buttons,
                                uint8_t button_count)
{
    LCD_SetColors(text, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_DispStringEN(0, LINE_EN(0), 0, (char *)title);
    draw_body_lines_from(body, LINE_EN(1), LCD_BUTTON_BODY_LINES);
    for (i = 0U; i < button_count; i++) {
        draw_button(buttons[i].x, buttons[i].y,
                    buttons[i].w, buttons[i].h,
                    buttons[i].label, buttons[i].color);
    }
}
```

### 7.2 触摸屏采样与坐标映射

触摸屏适配层位于 `HARDWARE/TOUCH/xpt2046.c`。当前使用 XPT2046 软件时序读取原始 ADC 值，再通过校准矩阵转换成 LCD 坐标：

```c
static const XptFactor s_factor_scan0 = {
    -0.006464L, -0.073259L, 280.358032L,
     0.074878L,  0.002052L,  -6.545977L
};
```

触摸事件上报格式：

```text
[TOUCH] x=84 y=62
[UI TOUCH] page=AUTH x=84 y=62
[UI] REG NFC OK
```

如果触摸不准，应优先看 `[TOUCH] x=... y=...` 的坐标是否落在按钮区域。系统还支持串口注入触摸：

```text
TOUCH 40 60
```

该命令会调用：

```c
void Adapter_Touch_Inject(uint16_t x, uint16_t y)
{
    s_event.x = x;
    s_event.y = y;
    s_event.pressed = 1U;
}
```

### 7.3 统一按钮表与页面命中

UI 主逻辑位于 `HARDWARE/UI/ui_main.c`。所有页面都使用统一按钮结构：

```c
typedef struct {
    LcdButton lcd;
    UiAction action;
} UiActionButton;
```

按钮位置使用固定坐标：

```c
#define UI_BTN_LEFT_X           4U
#define UI_BTN_RIGHT_X          124U
#define UI_BTN_W                112U
#define UI_BTN_H                42U
#define UI_BTN_ROW0_Y           48U
#define UI_BTN_ROW1_Y           96U
#define UI_BTN_ROW2_Y           144U
#define UI_BTN_ROW3_Y           192U
```

按钮绘制和触摸命中都使用同一套表，避免“看得见按钮但点不到”的问题：

```c
static UiAction hit_action(uint16_t x, uint16_t y)
{
    const UiActionButton *buttons = page_buttons(s_page, &count);
    for (i = 0U; i < count; i++) {
        const LcdButton *b = &buttons[i].lcd;
        if (x >= b->x &&
            x < (uint16_t)(b->x + b->w) &&
            y >= b->y &&
            y < (uint16_t)(b->y + b->h)) {
            return buttons[i].action;
        }
    }
    return UI_ACT_NONE;
}
```

页面顶部区域 `y < 40` 作为返回规则：已登录返回 HOME，未登录返回 AUTH。子页面底部 `y >= 280` 保留快捷导航：左侧 MONITOR，中间 SCENE，右侧 LOG。

## 8. 身份认证功能

认证模块主要由 `HARDWARE/MODULE/fingerprint.c` 和 `HARDWARE/MODULE/nfc.c` 实现。虽然文件名叫 `fingerprint.c`，但其中也维护了统一认证状态，包括 NFC 注册、指纹注册、验证标志、登录状态、管理员状态和备用密码状态。

### 8.1 AUTH 页面

开机后默认进入 AUTH 页面。页面按钮包括：

| 按钮 | 功能 |
|---|---|
| `REG NFC` | 注册 NFC 卡 |
| `REG FPR` | 注册指纹 |
| `VER NFC` | 验证 NFC 卡 |
| `VER FPR` | 验证指纹 |
| `REG BOTH` | 依次注册 NFC 和指纹 |
| `CLEAR` | 清除注册状态 |

AUTH 页面状态文本由 `Adapter_Auth_StatusText()` 返回，常见状态包括：

```text
REGISTER NFC+FPR
REGISTER NFC
REGISTER FPR
VERIFY NFC+FPR
VERIFY NFC
VERIFY FPR
LOGIN USER
LOGIN ADMIN
```

### 8.2 NFC 真实读卡

NFC 使用 PN532 串口模式，USART1 复用 PA9/PA10。`NFC_Init()` 会切换 USART1 接收目标到 NFC，然后唤醒 PN532：

```c
void NFC_Init(void)
{
    Adapter_Auth_SetUsart1Target(AUTH_USART1_TARGET_NFC);
    pn532_usart1_init(115200U);
    s_wake_ok = (pn532_wakeup() == 0) ? 1U : 0U;
}
```

读卡时发送寻卡命令，解析 UID，并格式化为字符串：

```c
snprintf(card_id, len, "NFC-%02X%02X%02X%02X",
         s_uid[0], s_uid[1], s_uid[2], s_uid[3]);
```

串口典型输出：

```text
[NFC] PN532 wake OK
[NFC] Place card near PN532...
[NFC] Card UID NFC-FD2A605E
[UI] REG NFC OK
```

### 8.3 指纹真实录入与验证

指纹使用 ZN632/FPR 模块，USART1 同样复用 PA9/PA10。模块与 PN532 共用 USART1，因此认证代码用 `Adapter_Auth_SetUsart1Target()` 在 NFC 和 FPR 之间切换接收目标。

指纹录入流程：

1. 指纹模块上电并验证密码；
2. 读取索引表；
3. 第一次放手指，采图；
4. `GenChar(1)` 生成特征；
5. 移开手指后再次放同一手指；
6. `GenChar(2)` 生成第二次特征；
7. `RegModel()` 合成模板；
8. 找空页并 `StoreChar()` 保存；
9. 保存页号为 `FINGER-xxx`。

关键函数：

```c
static uint8_t fpr_register_real(uint16_t *page_id)
{
    if (fpr_wait_image("[FPR] Put finger for enroll step 1\r\n") == 0U) return 0U;
    if (fpr_gen_char(1U) != 0) return 0U;
    ...
    if (fpr_wait_image("[FPR] Put finger for enroll step 2\r\n") == 0U) return 0U;
    if (fpr_gen_char(2U) != 0) return 0U;
    if (fpr_reg_model() != 0) return 0U;
    if (fpr_store_char(2U, empty_id) != 0) return 0U;
    *page_id = empty_id;
    return 1U;
}
```

验证流程是采图、生成特征、搜索指纹库，并与注册页号比较：

```c
static uint8_t fpr_verify_real(uint16_t expected_id,
                               uint16_t *matched_id,
                               uint16_t *score)
{
    if (fpr_wait_image("[FPR] Put finger for verify\r\n") == 0U) return 0U;
    if (fpr_gen_char(1U) != 0) return 0U;
    if (fpr_high_speed_search(1U, matched_id, score) != 0) return 0U;
    if (expected_id != 0xFFFFU && *matched_id != expected_id) return 0U;
    return 1U;
}
```

注册或验证成功后会短鸣一次：

```c
static void auth_beep_ok(void)
{
    Board_Buzzer_Set(1U);
    delay_ms(80U);
    Board_Buzzer_Set(0U);
}
```

### 8.4 双重验证和备用密码

系统要求 NFC 和指纹两个因子都验证成功后才算登录：

```c
static void update_login_if_ready(uint8_t admin)
{
    if (s_registered != 0U &&
        s_nfc_verified != 0U &&
        s_finger_verified != 0U) {
        s_login = 1U;
        s_admin = admin ? 1U : s_registered_admin;
        set_name(s_admin);
    }
}
```

如果 NFC 或指纹无法识别，可以在串口输入：

```text
25531460
```

或：

```text
PASS 25531460
PASSWORD 25531460
```

该密码不是单纯返回主页的命令，而是备用身份验证。无待处理物资操作时，密码会完成登录、进入 HOME 并复位到安全显示状态；若 CARGO 页面有待确认的加入或领出任务，密码会作为当前物资操作的确认身份，执行库存更新后再返回 HOME。

## 9. 气体数据源与 ADC/DAC 映射

气体数据管理模块位于 `HARDWARE/SENSOR/data_manager.c`。系统支持 5 种气体来源：

| 来源 | 枚举 | 数据含义 |
|---|---|---|
| `REAL_MQ2` | `GAS_SOURCE_REAL_MQ2` | MQ-2 AO 接入 PC2/ADC_Channel_12 后的真实采样 |
| `POT` | `GAS_SOURCE_POT` | 电位器接入 PC1/ADC_Channel_11 后的手动模拟气体值 |
| `DAC_LOOP` | `GAS_SOURCE_DAC_LOOP` | PA4 DAC 输出接 PA1 ADC 后形成闭环模拟 |
| `SCENE` | `GAS_SOURCE_SCENE` | 场景模块写入的模拟气体值 |
| `UART` | `GAS_SOURCE_UART` | 串口命令 `GAS SET` 或 `GAS RAMP` 设置的值 |

所有来源统一映射到 `0~1000`：

```c
uint16_t Board_ADC_RawToGas(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * APP_ADC_GAS_MAX) / 4095U);
}
```

其中电位器读取函数为：

```c
uint16_t Board_ADC_ReadPot(void)
{
    return Board_ADC_RawToGas(Board_ADC_ReadRaw(APP_ADC_CH_POT));
}
```

`APP_ADC_CH_POT` 当前配置为：

```c
#define APP_ADC_CH_POT ADC_Channel_11  /* PC1 */
```

因此如果使用开发板电位器验证 GAS 变化，需要确认硬件连接到 PC1 对应电位器通道。

数据源管理器每次更新时先读取所有候选值，再根据当前 source 选择唯一有效值：

```c
void App_Gas_Update(void)
{
    s_gas.mq2_real = clamp_gas(Board_ADC_ReadGasReal());
    s_gas.potentiometer = clamp_gas(Board_ADC_ReadPot());
    s_gas.dac_loop = clamp_gas(Board_ADC_ReadDacLoop());

    switch (s_gas.source) {
    case GAS_SOURCE_REAL_MQ2:
        s_gas.effective_value = s_gas.mq2_real;
        break;
    case GAS_SOURCE_POT:
        s_gas.effective_value = s_gas.potentiometer;
        break;
    case GAS_SOURCE_DAC_LOOP:
        s_gas.effective_value = s_gas.dac_loop;
        break;
    case GAS_SOURCE_SCENE:
        s_gas.effective_value = s_gas.scene_value;
        break;
    case GAS_SOURCE_UART:
        s_gas.effective_value = s_gas.uart_value;
        break;
    }
}
```

这种设计避免了电位器、DAC、串口和场景同时直接修改报警变量的问题。报警模块只看 `effective_value`。

DAC 输出位于 `HARDWARE/SENSOR/dac_output.c`。DAC 也使用 `0~1000` 量程：

```c
void Board_DAC_SetGasValue(uint16_t gas_value)
{
    if (gas_value > APP_ADC_GAS_MAX) gas_value = APP_ADC_GAS_MAX;
    s_last_gas_value = gas_value;
    raw = ((uint32_t)gas_value * 4095U) / APP_ADC_GAS_MAX;
    DAC_SetChannel1Data(DAC_Align_12b_R, (uint16_t)raw);
}
```

如果 PA4 DAC 没有实际接回 PA1 ADC，系统启用了回退机制：

```c
#define APP_DAC_LOOP_FALLBACK_TO_SETPOINT 1
```

当 DAC_LOOP ADC 值很低而 DAC 已设置目标值时，系统可以使用最后设置值，保证课堂演示中 `DAC 300`、`DAC 850` 或场景泄漏仍能稳定表现。

## 10. 分级报警逻辑

报警模块位于 `HARDWARE/CONTROL/alarm.c`。报警分为五种状态：

| 状态 | 气体值范围 | 进入条件 | 表现 |
|---|---:|---|---|
| `NONE` | `<400` | 气体值低于关注阈值 | 绿灯表示正常，蜂鸣器关闭，风扇约 20% |
| `ATTENTION` | `400~549` | 气体值达到 400 | 页面和串口显示关注状态，风扇约 40%，不响铃 |
| `LEVEL1` | `550~679` | 气体值达到 550 或温度达到预警值 | 风扇约 60%，串口显示 `LEVEL1` |
| `LEVEL2` | `680~779` | 气体值达到 680 或温度达到危险值 | 红灯亮，CAN 发送报警，风扇约 80%，进入 ALARM 页面 |
| `LEVEL3` | `>=780` | 气体值连续超过三级阈值，或外部中断强制报警 | 红灯亮，蜂鸣器响，风扇 100%，进入 ALARM 页面 |

默认阈值定义在 `system_config.h`：

```c
#define APP_GAS_ATTENTION_ENTER     400U
#define APP_GAS_WARN_ENTER          550U
#define APP_GAS_DANGER_ENTER        680U
#define APP_GAS_CRITICAL_ENTER      780U
```

为了避免 MQ-2 或 ADC 上电初始抖动导致误报警，系统有 30 秒启动保护：

```c
#define APP_GAS_STARTUP_PROTECT_MS  30000U
```

报警评估时，如果运行时间小于 30 秒，直接保持 `ALARM_NONE`：

```c
if (uptime_ms < APP_GAS_STARTUP_PROTECT_MS) {
    next = ALARM_NONE;
    s_gas_critical_count = 0U;
} else {
    next = evaluate_gas_level(gas_value, temp_x10);
}
```

报警还加入滞回机制，避免阈值附近来回跳变：

```c
#define APP_GAS_ATTENTION_EXIT      350U
#define APP_GAS_WARN_EXIT           500U
#define APP_GAS_DANGER_EXIT         600U
#define APP_GAS_CRITICAL_EXIT       700U
```

例如进入 `LEVEL2` 后，气体值必须降到 600 以下才会退出二级报警；进入 `LEVEL3` 后，气体值必须降到 700 以下并经过状态评估后才会解除。

三级报警会控制蜂鸣器：

```c
Board_Buzzer_Set((s_level >= ALARM_LEVEL3) && (s_muted == 0U));
```

如果报警响起，可以通过以下方式消音或复位：

| 操作 | 作用 |
|---|---|
| 按开发板按键 | 报警为 `LEVEL3` 时调用 `App_Alarm_MuteByKey()`，蜂鸣器停止，报警状态仍保留 |
| 触摸 ALARM 页面任意区域 | 三级报警时先执行消音，再处理触摸 |
| 触摸 `ALARM RST` | 管理员认证后调用 `App_Alarm_ResetByAdmin()` |
| 串口输入 `25531460` | 作为备用认证，清除报警并返回 HOME，同时把当前状态调整为安全显示 |
| 串口 `ALARM RESET` | 管理员已认证时可复位报警 |

外部中断使用 PA0/EXTI0。中断服务函数位于 `USER/stm32f4xx_it.c`：

```c
void EXTI0_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
    EXTI_ClearITPendingBit(EXTI_Line0);
    App_OnEmergencyButton();
  }
}
```

`App_OnEmergencyButton()` 在正常状态下可触发人工紧急报警，在三级报警状态下可作为快捷消音入口。

## 11. GPS 与 CAN 演示状态

### 11.1 GPS 回放

GPS 模块位于 `HARDWARE/MODULE/gps.c`。由于当前实验条件下 GPS 不具备稳定真实定位能力，系统默认使用回放模式：

```c
void App_GPS_Init(void)
{
    memset(&s_fix, 0, sizeof(s_fix));
    s_replay = 1U;
    s_lost = 0U;
    s_deviation = 0U;
    s_route_index = 0U;
    s_fix.replay = 1U;
    s_fix.valid = 0U;
}
```

每秒推进路线点：

```c
static const int32_t route_lat_e7[] = {399084000, 399086000, 399091000, 399098000};
static const int32_t route_lon_e7[] = {1163970000, 1163990000, 1164020000, 1164070000};
```

LCD 和串口显示为：

```text
GPS:REPLAY
GPS:REPLAY_LOST
GPS:REPLAY_WAIT
```

系统不会把 GPS 无定位直接当成危险报警。GPS 丢失只作为场景或故障状态显示，避免因硬件限制导致系统上电就报警。

### 11.2 CAN 回环

CAN 模块位于 `HARDWARE/COMMUNICATION/can_protocol.c`，当前采用 CAN1 LoopBack 模式：

```c
can.CAN_Mode = CAN_Mode_LoopBack;
```

系统每秒发送心跳：

```c
void Board_CAN_SendHeartbeat(uint8_t state, uint16_t gas_value)
{
    if (s_virtual_online) send_frame(0x103, state, 0xA5, gas_value);
}
```

报警时发送报警帧：

```c
void Board_CAN_SendAlarm(uint8_t alarm_level, uint16_t gas_value)
{
    send_frame(0x3FF, alarm_level, 0xE1, gas_value);
}
```

页面显示不会写成真实在线，而是显示：

```text
CAN:LOOPBACK
CAN:LOOPBACK_WAIT
CAN:SIM_OFF
```

这说明当前是单机回环或模拟离线状态，不误导为真实多节点 CAN 在线。

## 12. 物资库存与 CARGO 页面

物资模块位于 `HARDWARE/STORAGE/cargo_manager.c`。当前库存保存在 RAM 中，复位后恢复预设值：

```c
void App_Cargo_Reset(void)
{
    memset(s_items, 0, sizeof(s_items));
    set_item(0U, "A", 10U);
    set_item(1U, "B", 8U);
    set_item(2U, "C", 6U);
    set_item(3U, "D", 12U);
    set_item(4U, "E", 5U);
    set_item(5U, "F", 3U);
    clear_pending();
}
```

CARGO 页面按钮：

| 按钮 | 功能 |
|---|---|
| `ADD` | 进入加入模式，等待串口输入物品和数量 |
| `OUT` | 进入领出模式，等待串口输入物品和数量 |
| `LIST` | 在 LCD 显示清单，并在串口打印清单 |
| `NFC OK` | 使用 NFC 验证后确认当前待处理加入/领出 |
| `FPR OK` | 使用指纹验证后确认当前待处理加入/领出 |
| `CANCEL` | 取消当前待处理任务 |
| `HOME` | 返回主界面 |

物资操作流程：

1. 进入 CARGO 页面；
2. 点 `ADD` 或 `OUT`；
3. 串口输入 `CARGO ADD A 3` 或 `CARGO OUT A 2`；
4. LCD 显示待确认任务；
5. 点 `NFC OK` 或 `FPR OK`，或者串口输入 `25531460` 作为备用验证；
6. 成功后更新库存，并记录日志；
7. 点 `LIST` 或查看页面正文确认库存变化。

设置待处理任务：

```c
uint8_t App_Cargo_SetPending(CargoMode mode, const char *name, uint16_t quantity)
{
    if (name == 0 || name[0] == '\0' || quantity == 0U || mode == CARGO_MODE_NONE) {
        return 0U;
    }

    s_mode = mode;
    strncpy(s_pending_name, name, sizeof(s_pending_name) - 1U);
    s_pending_quantity = quantity;
    snprintf(s_status, sizeof(s_status), "pending %s %s x%u",
             App_Cargo_ModeName(mode), s_pending_name, s_pending_quantity);
    return 1U;
}
```

确认加入或领出：

```c
uint8_t App_Cargo_Confirm(const char *auth_method)
{
    index = find_item(s_pending_name);

    if (s_mode == CARGO_MODE_ADD) {
        if (index < 0) {
            index = find_free_item();
            set_item((uint8_t)index, s_pending_name, 0U);
        }
        s_items[(uint8_t)index].quantity += s_pending_quantity;
        App_Log_Add(LOG_TRANSPORT, log_text);
        clear_pending();
        return 1U;
    }

    if (index < 0 || s_items[(uint8_t)index].quantity < s_pending_quantity) {
        App_Log_Add(LOG_FAULT, log_text);
        return 0U;
    }

    s_items[(uint8_t)index].quantity -= s_pending_quantity;
    App_Log_Add(LOG_TRANSPORT, log_text);
    clear_pending();
    return 1U;
}
```

库存不足时不扣减，并记录失败日志。

## 13. 日志与导出

日志上层位于 `HARDWARE/STORAGE/log_manager.c`，底层 Flash 适配器位于 `HARDWARE/STORAGE/w25q64.c`。当前 `APP_ENABLE_REAL_SPI_FLASH=0`，日志使用 RAM 环形缓冲实现：

```c
static FlashLogRecord s_records[APP_LOG_MAX_RECORDS];
static uint16_t s_head = 0;
static uint16_t s_count = 0;
```

追加日志：

```c
void Adapter_Flash_Append(LogType type, uint32_t timestamp, const char *text)
{
    FlashLogRecord *r = &s_records[s_head];
    r->timestamp = timestamp;
    r->type = type;
    strncpy(r->text, text, sizeof(r->text) - 1U);
    s_head = (s_head + 1U) % APP_LOG_MAX_RECORDS;
    if (s_count < APP_LOG_MAX_RECORDS) s_count++;
}
```

日志类型包括：

```c
LOG_INFO
LOG_AUTH
LOG_TRANSPORT
LOG_ALARM
LOG_FAULT
LOG_PARAM
```

LOG 页面显示最新 4 条日志，支持 `PREV`、`NEXT`、`EXPORT`、`CLEAR`。

导出时优先使用 DMA USART2：

```c
void App_Log_Export(void)
{
    ...
    if (!Board_DMA_USART2_Send((const uint8_t *)export_buf, used)) {
        UART_SendString(USART2, export_buf);
    }
}
```

DMA 发送底层使用 `DMA1_Stream6 / Channel4`，对应 USART2 TX：

```c
dma.DMA_Channel = DMA_Channel_4;
dma.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
dma.DMA_DIR = DMA_DIR_MemoryToPeripheral;
```

串口典型导出：

```text
# Hazmat log export, count=64
000,1326190,TRANSPORT,periodic snapshot
001,1326196,TRANSPORT,periodic snapshot
004,1326211,PARAM,alarm threshold saved
...
```

## 14. SETTING 参数设置页

SETTING 页面用于调整报警阈值。按钮包括：

| 按钮 | 功能 |
|---|---|
| `L1+` / `L1-` | 一级报警阈值增减，每次 10 |
| `L2+` / `L2-` | 二级报警阈值增减，每次 10 |
| `L3+` / `L3-` | 三级报警阈值增减，每次 10 |
| `SAVE` | 保存当前阈值到 EEPROM 适配器 |
| `DEFAULT` | 恢复默认阈值并保存 |

阈值调整函数：

```c
static void adjust_threshold(uint8_t index, int16_t delta)
{
    App_Alarm_GetThresholds(&warn, &danger, &critical);
    ...
    App_Alarm_SetThresholds(warn, danger, critical);
}
```

保存函数：

```c
static void save_thresholds(void)
{
    App_Alarm_GetThresholds(&warn, &danger, &critical);
    params.gas_warn = warn;
    params.gas_danger = danger;
    params.gas_critical = critical;
    params.crc = Adapter_EEPROM_Crc(&params);
    Adapter_EEPROM_Save(&params);
    App_Log_Add(LOG_PARAM, "alarm threshold saved");
}
```

虽然当前 EEPROM 使用 RAM 适配器，但接口已经按真实 EEPROM 参数保存方式组织，后续可以把 `eeprom.c` 替换为 AT24C02 实际 I2C 驱动。

## 15. DEVICE 设备控制页

DEVICE 页面用于直接测试执行器和模拟输出：

| 按钮 | 调用函数 | 现象 |
|---|---|---|
| `BEEP` | `ui_short_beep()` | 蜂鸣器短鸣 |
| `LED G` | `Board_LED_Toggle(BOARD_LED_GREEN)` | 绿色 LED 翻转 |
| `LED R` | `Board_LED_Toggle(BOARD_LED_RED)` | 红色 LED 翻转 |
| `FAN 25` | `Board_PWM_SetFanPercent(25)` | TIM3 PWM 占空比约 25% |
| `FAN 75` | `Board_PWM_SetFanPercent(75)` | TIM3 PWM 占空比约 75% |
| `DAC 300` | `Board_DAC_SetGasValue(300)` | DAC 输出对应 300/1000 比例电压 |
| `DAC 850` | `Board_DAC_SetGasValue(850)` | DAC 输出对应 850/1000 比例电压，可触发高气体值验证 |
| `ALARM RST` | `App_Alarm_ResetByAdmin()` | 管理员认证后复位报警 |

蜂鸣器和 LED 均为低电平有效：

```c
void Board_Buzzer_Set(uint8_t on)
{
    if (on) GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    else GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

void Board_LED_Set(BoardLed led, uint8_t on)
{
    if (on) GPIO_ResetBits(led_port(led), led_pin(led));
    else GPIO_SetBits(led_port(led), led_pin(led));
}
```

PWM 风扇使用 TIM3_CH2：

```c
void Board_PWM_SetFanPercent(uint8_t percent)
{
    if (percent > 100U) percent = 100U;
    TIM_SetCompare2(TIM3, (uint16_t)(percent * 10U));
}
```

## 16. SCENE 场景模拟页

SCENE 页面用于稳定复现实验场景：

| 按钮 | 场景 | 实现 |
|---|---|---|
| `NORMAL` | 正常运输 | 气体源回到 POT，GPS 恢复，路线偏离清除，CAN 回环恢复 |
| `GAS SLOW` | 慢速泄漏 | 气体值随时间缓慢上升，DAC 输出同步变化 |
| `GAS FAST` | 快速泄漏 | 气体值快速升高，便于触发 LEVEL2/LEVEL3 |
| `GPS LOST` | GPS 丢失 | GPS 状态显示 `REPLAY_LOST` |
| `ROUTE DEV` | 路线偏离 | GPS 回放坐标加入偏移 |
| `CAN OFF` | CAN 离线 | CAN 状态显示 `SIM_OFF` |
| `ILLEGAL` | 非法开箱 | 记录非法开箱日志 |
| `RESET` | 场景复位 | 清除场景、GPS/CAN 恢复 |

场景启动函数：

```c
void App_Scene_Start(SceneType scene)
{
    s_scene = scene;
    s_elapsed = 0U;
    App_Log_Add(LOG_INFO, App_Scene_Name(scene));

    if (scene == SCENE_NORMAL) {
        App_Gas_SetSource(GAS_SOURCE_POT);
        App_GPS_SetLost(0);
        App_GPS_SetRouteDeviation(0);
        Board_CAN_SetVirtualOnline(1);
    } else if (scene == SCENE_GAS_SLOW || scene == SCENE_GAS_FAST) {
        App_Gas_SetSource(GAS_SOURCE_DAC_LOOP);
    } else if (scene == SCENE_GPS_LOST) {
        App_GPS_SetLost(1);
    } else if (scene == SCENE_ROUTE_DEVIATE) {
        App_GPS_SetRouteDeviation(1);
    } else if (scene == SCENE_CAN_OFFLINE) {
        Board_CAN_SetVirtualOnline(0);
    }
}
```

慢速泄漏曲线：

```c
if (s_scene == SCENE_GAS_SLOW) {
    if (s_elapsed <= 10U) value = 300U;
    else if (s_elapsed <= 40U) value = 300U + (s_elapsed - 10U) * 15U;
    else if (s_elapsed <= 55U) value = 750U + (s_elapsed - 40U) * 6U;
    else value = 360U;
    Board_DAC_SetGasValue(value);
}
```

快速泄漏曲线：

```c
else if (s_scene == SCENE_GAS_FAST) {
    value = (s_elapsed < 20U) ? (320U + s_elapsed * 28U) : 850U;
    Board_DAC_SetGasValue(value);
}
```

## 17. MONITOR 实时监控页

MONITOR 页面用于集中查看系统状态：

```text
MONITOR | t=...
T:26.4C H:58% Gas:320 UART
GPS:REPLAY CAN:LOOPBACK Alarm:NONE
```

该页面本身不直接修改数据，避免误触改变系统状态。页面内提供快速跳转按钮：

```text
SOURCE / SCENE / DEVICE / LOG / SETTING / CARGO / AUTH / HOME
```

监控页读取的是 `AppSnapshot`，由 `update_snapshot()` 汇总：

```c
static void update_snapshot(void)
{
    const GasDataManager *gas = App_Gas_Get();
    const GpsFix *gps = App_GPS_GetFix();
    s_snapshot.gas_value = gas->effective_value;
    s_snapshot.gas_source = gas->source;
    s_snapshot.battery_mv = Board_ADC_ReadBatteryMv();
    s_snapshot.gps = *gps;
    s_snapshot.can_online = Board_CAN_IsOnline();
    s_snapshot.authenticated = Adapter_Auth_IsLoggedIn();
    s_snapshot.admin = Adapter_Auth_IsAdmin();
    s_snapshot.alarm = App_Alarm_GetLevel();
    s_snapshot.scene = App_Scene_Current();
}
```

## 18. SOURCE 气体数据源页

SOURCE 页面用于切换气体值来源：

```text
SOURCE | t=...
Current:POT Gas:300
Tap source block to switch.
```

按钮：

```text
POT / DAC / SCENE / UART / REAL / HOME
```

触摸按钮后调用：

```c
case UI_ACT_SOURCE_POT:
    App_Gas_SetSource(GAS_SOURCE_POT);
    App_Log_Add(LOG_PARAM, "source pot");
    return 1U;
```

串口也可以切换：

```text
GAS SOURCE POT
GAS SOURCE DAC_LOOP
GAS SOURCE SCENE
GAS SOURCE UART
GAS SOURCE REAL_MQ2
```

## 19. 串口命令参考

串口命令解析位于 `HARDWARE/COMMUNICATION/command_parser.c`。输入 `HELP` 可打印完整命令列表。

### 19.1 登录与认证

```text
REG NFC
REG FINGER
REG ADMIN
REG RESET

AUTH NFC
AUTH FINGER
AUTH ADMIN
AUTH RESET

25531460
PASS 25531460
PASSWORD 25531460
```

说明：

- `REG NFC` 注册卡；
- `REG FINGER` 注册指纹；
- `AUTH NFC` 和 `AUTH FINGER` 分别验证两个身份因子；
- 双重验证完成后进入 HOME；
- `25531460` 是备用验证，验证成功后进入 HOME；
- 若物资页面存在待处理任务，`25531460` 优先确认该物资任务。

### 19.2 页面切换

```text
PAGE HOME
PAGE MONITOR
PAGE SOURCE
PAGE SCENE
PAGE LOG
PAGE SETTING
PAGE AUTH
PAGE CARGO
PAGE DEVICE
```

未登录时，业务页面会被拦截回 AUTH。

### 19.3 气体数据

```text
GAS SOURCE POT
GAS SOURCE DAC_LOOP
GAS SOURCE SCENE
GAS SOURCE UART
GAS SOURCE REAL_MQ2

GAS SET 300
GAS SET 850
GAS RAMP 300 850 30
```

`GAS SET` 会自动切换到 UART 数据源。

### 19.4 场景控制

```text
SCENE NORMAL
SCENE GAS_SLOW
SCENE GAS_FAST
SCENE GPS_LOST
SCENE ROUTE_DEVIATE
SCENE CAN_OFFLINE
SCENE RESET
```

### 19.5 GPS、日志、报警

```text
GPS REPLAY
GPS LOST
GPS RECOVER

LOG EXPORT
ALARM RESET
TOUCH 40 60
```

### 19.6 物资管理

```text
CARGO ADD A 3
CARGO OUT A 2
CARGO LIST
CARGO RESET
```

注意：`CARGO ADD/OUT` 只是设置待处理任务，不立即修改库存。必须通过 LCD 上的 `NFC OK`、`FPR OK` 或串口备用密码 `25531460` 完成身份确认后才会真正更新库存。

## 20. 登录后的完整验证流程

### 20.1 基础启动与认证验证

1. 连接 CMSIS-DAP，下载程序。
2. 打开串口助手，设置 `115200, 8N1`。
3. 复位开发板。
4. 串口应看到：

```text
=== Hazmat Transport Monitor boot ===
[NFC] PN532 wake OK
[LCD] ILI9341 FSMC driver enabled.
[SELF] GPIO USART ADC DAC RTC CAN DMA adapters OK
Type HELP for commands.
```

5. LCD 进入 AUTH 页面。
6. 点 `REG NFC`，将 NFC 卡靠近 PN532，成功后短鸣，串口显示 UID。
7. 点 `REG FPR`，按串口提示两次放同一手指，成功后短鸣。
8. 点 `VER NFC`，再次刷同一张卡。
9. 点 `VER FPR`，再次按手指。
10. 双重验证成功后自动进入 HOME。

如果 NFC 或指纹识别失败，串口输入：

```text
25531460
```

系统应输出：

```text
OK PASS HOME
```

并进入 HOME。

### 20.2 HOME 页面验证

HOME 页面有 8 个方块。依次触摸每个方块，串口应输出：

```text
[UI TOUCH] page=HOME x=...
[UI] PAGE MONITOR OK
```

并进入对应页面。顶部区域点击可回 HOME。

### 20.3 MONITOR 页面验证

1. 点 `1 MONITOR`。
2. LCD 显示温湿度、气体值、数据源、GPS、CAN、报警状态。
3. 点页面内 `SOURCE`、`SCENE`、`DEVICE`、`LOG` 可快速跳转。
4. 串口命令：

```text
PAGE MONITOR
```

也可进入该页。

### 20.4 SOURCE 页面验证

1. 点 `2 SOURCE`。
2. 点 `POT`，旋转 PC1 电位器，观察 Gas 从低到高变化。
3. 点 `DAC`，再到 DEVICE 页点 `DAC 300` 或 `DAC 850`，观察 Current 变为 `DAC_LOOP`。
4. 点 `UART`，串口输入：

```text
GAS SET 300
GAS SET 850
```

5. 点 `REAL`，系统读取 MQ-2 通道 PC2。
6. 点 `SCENE`，配合 SCENE 页面曲线。

### 20.5 SCENE 页面验证

1. 点 `3 SCENE`。
2. 点 `GAS SLOW`，气体值逐步升高，串口状态依次可能出现 `ATTENTION`、`LEVEL1`、`LEVEL2`。
3. 点 `GAS FAST`，气体值快速升高，进入 `LEVEL3` 后蜂鸣器响，红灯亮，风扇 100%，LCD 进入 ALARM 页面。
4. 报警响后按任意键或触摸可消音，状态中的 `mute` 变为 `Y`。
5. 输入 `25531460` 可解除报警、回 HOME，并恢复安全状态。
6. 点 `GPS LOST` 后，GPS 状态显示 `REPLAY_LOST`，不触发危险报警。
7. 点 `CAN OFF` 后，CAN 状态显示 `SIM_OFF`。
8. 点 `RESET` 清除场景。

### 20.6 LOG 页面验证

1. 点 `4 LOG`。
2. 页面显示记录数量和当前 4 条日志。
3. 点 `PREV` / `NEXT` 翻页。
4. 点 `EXPORT`，串口输出日志导出内容。
5. 点 `CLEAR`，日志清空并新增 `log cleared` 记录。
6. 串口命令：

```text
LOG EXPORT
```

可执行同样导出。

### 20.7 SETTING 页面验证

1. 点 `5 SETTING`。
2. 页面显示：

```text
L1:550 L2:680 L3:780
Startup protect:30s
```

3. 点 `L1+`、`L1-`、`L2+`、`L2-`、`L3+`、`L3-` 调整阈值。
4. 点 `SAVE` 保存参数并写日志。
5. 点 `DEFAULT` 恢复默认阈值。
6. 日志中应出现 `alarm threshold saved`。

### 20.8 CARGO 页面验证

1. 点 `7 CARGO`。
2. 点 `LIST`，LCD 显示清单，串口显示：

```text
CARGO LIST A:10 B:8 C:6 D:12 E:5 F:3
```

3. 点 `ADD`，串口输入：

```text
CARGO ADD A 3
```

4. LCD 显示待确认 `ADD A x3`。
5. 点 `NFC OK` 或 `FPR OK`，按要求刷卡或按指纹。
6. 也可以输入：

```text
25531460
```

作为备用确认。
7. 再点 `LIST`，A 应由 10 变为 13。
8. 点 `OUT`，串口输入：

```text
CARGO OUT A 2
```

9. 验证后 A 应由 13 变为 11。
10. 若领出数量大于库存，系统不扣减，日志记录失败。

### 20.9 DEVICE 页面验证

1. 点 `8 DEVICE`。
2. 点 `BEEP`，蜂鸣器短鸣。
3. 点 `LED G`，绿色 LED 状态翻转。
4. 点 `LED R`，红色 LED 状态翻转。
5. 点 `FAN 25` 和 `FAN 75`，风扇 PWM 输出改变。
6. 点 `DAC 300` 和 `DAC 850`，DAC 输出模拟气体控制电压。
7. 报警后以管理员身份点 `ALARM RST` 可复位。

## 21. 关键文件说明

| 文件 | 作用 |
|---|---|
| `USER/main.c` | 程序入口 |
| `USER/app_main.c` | 系统初始化、任务轮询、状态快照、按键处理、周期任务 |
| `USER/system_config.h` | 全局宏、阈值、硬件使能、ADC 通道和周期参数 |
| `USER/app_types.h` | 系统状态、气体源、场景、报警等级、UI 页面、日志类型等枚举 |
| `HARDWARE/UI/ui_main.c` | 页面按钮表、触摸命中、页面渲染、页面动作处理 |
| `HARDWARE/LCD/lcd.c` | LCD 适配层和统一页面绘制 |
| `HARDWARE/TOUCH/xpt2046.c` | 触摸屏读取、滤波、坐标转换、触摸注入 |
| `HARDWARE/SENSOR/adc_sensor.c` | ADC 初始化、PC1 电位器、PC2 MQ2、PA1 DAC 回读、PC3 电池采样 |
| `HARDWARE/SENSOR/data_manager.c` | 统一气体数据源管理 |
| `HARDWARE/SENSOR/dac_output.c` | PA4 DAC 输出和 0~1000 气体值映射 |
| `HARDWARE/CONTROL/alarm.c` | 分级报警、启动保护、滞回、蜂鸣器/LED/风扇联动 |
| `HARDWARE/CONTROL/gpio_control.c` | LED、蜂鸣器、按键、EXTI0 初始化 |
| `HARDWARE/CONTROL/pwm_control.c` | SysTick 和 TIM3 PWM 风扇控制 |
| `HARDWARE/COMMUNICATION/command_parser.c` | 串口命令解析 |
| `HARDWARE/COMMUNICATION/usart_debug.c` | USART2 初始化、收发和行缓冲 |
| `HARDWARE/COMMUNICATION/dma_uart.c` | DMA1_Stream6 USART2 日志导出 |
| `HARDWARE/COMMUNICATION/can_protocol.c` | CAN1 回环、心跳和报警帧 |
| `HARDWARE/MODULE/nfc.c` | PN532 唤醒、寻卡和 UID 读取 |
| `HARDWARE/MODULE/fingerprint.c` | ZN632 指纹录入/验证和统一认证状态管理 |
| `HARDWARE/MODULE/gps.c` | GPS 回放、丢失、路线偏离和 NMEA 解析入口 |
| `HARDWARE/STORAGE/cargo_manager.c` | 物资库存、加入、领出、清单格式化 |
| `HARDWARE/STORAGE/log_manager.c` | 日志添加、读取、清除和导出 |
| `HARDWARE/STORAGE/eeprom.c` | 参数保存适配器 |
| `HARDWARE/STORAGE/w25q64.c` | 日志存储适配器 |
| `HARDWARE/SIMULATION/gas_scene.c` | 场景模拟曲线和 GPS/CAN 状态注入 |
| `USER/stm32f4xx_it.c` | SysTick、EXTI0、DMA、CAN、USART1 中断入口 |

## 22. 编译与下载注意事项

1. 使用 `MDK/Project.uvprojx` 打开工程。
2. 设备型号应为 `STM32F407VE`。
3. C/C++ 宏定义应包含：

```text
STM32F40_41xxx,USE_STDPERIPH_DRIVER
```

4. 头文件路径应包含 `FWLIB/inc`、`USER`、`CORE`、`UTILS`、各个 `HARDWARE` 子目录以及 `REFERENCE_DRIVERS/LCD_ILI9341`。
5. LCD 工程文件引用了：

```text
REFERENCE_DRIVERS\LCD_ILI9341\lcd_fonts.c
REFERENCE_DRIVERS\LCD_ILI9341\lcd_fonts.h
REFERENCE_DRIVERS\LCD_ILI9341\lcd_ili9341.c
REFERENCE_DRIVERS\LCD_ILI9341\lcd_ili9341.h
```

如果编译时出现找不到 `lcd_ili9341.h` 或 `lcd_fonts.c`，先检查 `REFERENCE_DRIVERS/LCD_ILI9341` 是否完整，以及 Keil 工程中的相对路径是否仍指向该目录。当前目录中应当保留这些参考驱动文件。

6. 下载器选择 `CMSIS-DAP Debugger`，接口选择 `SWD`。
7. 下载后如果 LCD 不亮，优先检查：
   - 开发板电源；
   - LCD 背光和驱动文件是否加入工程；
   - `Adapter_LCD_Init()` 是否执行；
   - 是否被 NFC/FPR 等模块阻塞在初始化阶段；
   - 蜂鸣器 PA8 是否在 `Board_GPIO_Init()` 中默认关闭。

## 23. 常见问题

### 23.1 复位后为什么先进入 AUTH？

这是设计要求。未登录时不能直接进入主页面或物资、设备等业务页面。`App_UI_SetPage()` 中对未登录状态做了拦截：

```c
if (page != UI_AUTH &&
    page != UI_SELF_CHECK &&
    page != UI_ALARM &&
    Adapter_Auth_IsLoggedIn() == 0U) {
    s_page = UI_AUTH;
    return;
}
```

### 23.2 按按钮蜂鸣器响，但页面没变怎么办？

看串口是否有：

```text
[TOUCH] x=...
[UI TOUCH] page=...
[UI] ... OK
```

如果有 `[TOUCH]` 但没有对应 `[UI] ... OK`，可能触摸坐标没有落在按钮区域。可以用 `TOUCH x y` 命令测试按钮坐标。

### 23.3 气体值不变化怎么办？

优先进入 SOURCE 页面选择 `POT`，确认电位器连接到 PC1/ADC_Channel_11。串口应看到类似：

```text
[STAT] gas=26 src=POT alarm=NONE
[STAT] gas=300 src=POT alarm=NONE
[STAT] gas=649 src=POT alarm=LEVEL1
```

如果真实电位器变化不明显，可以改用：

```text
GAS SET 850
```

或 SOURCE 选择 DAC/SCENE 后使用 SCENE 页面。

### 23.4 GPS 为什么显示 REPLAY？

当前实验条件下不把 GPS 定位成功作为必要条件。`REPLAY` 表示使用路线回放验证运输位置、GPS 丢失和路线偏离逻辑；这比室内等待真实定位更稳定，也不会把无定位误判为危险报警。

### 23.5 CAN 为什么显示 LOOPBACK？

当前为单板演示，CAN 使用回环模式验证报文发送与接收，不代表真实外部节点在线。若进入 `CAN OFF` 场景，显示会变成 `SIM_OFF`。

### 23.6 报警后为什么回不去？

当报警等级达到 `LEVEL2` 或 `LEVEL3` 时，`App_UI_Render()` 会强制切到 `UI_ALARM`，防止危险状态被普通页面遮挡：

```c
if (snapshot->alarm >= ALARM_LEVEL2) {
    s_page = UI_ALARM;
}
```

三级报警时先按键或触摸消音，再输入 `25531460` 或管理员执行 `ALARM RST` 复位。复位后才能正常回到 HOME。

### 23.7 为什么日志是 RAM，不是真正 W25Q64？

当前 `APP_ENABLE_REAL_SPI_FLASH=0`，`w25q64.c` 作为 Flash 日志适配层使用 RAM 环形缓存。这样做可以先验证日志接口、导出、翻页、清除和事件记录逻辑。后续要做掉电保存时，只需把 `Adapter_Flash_*` 函数替换为真实 W25Q64 读写实现。

## 24. 后续可扩展方向

1. 将 `eeprom.c` 替换为真实 AT24C02 I2C 驱动，实现阈值掉电保存。
2. 将 `w25q64.c` 替换为真实 W25Q64 SPI Flash 驱动，实现日志掉电保存。
3. 将 DHT11 适配器切换为真实时序读取，并在 SETTING 页面增加温湿度阈值。
4. 增加箱门开关输入，把非法开箱从场景模拟改成真实外部中断。
5. 增加重量传感器，把物资加入/领出从人工数量输入扩展为重量核验。
6. 增加 Wi-Fi/LoRa/4G 上报，把日志和报警同步到上位机。
7. 为中文 LCD 界面增加中文字模，内部枚举和逻辑继续保持英文，显示层单独映射中文按钮文字。

## 25. 总结

该工程将 STM32F407 标准库工程、LCD 图形显示、XPT2046 触摸、USART 调试、DMA 导出、ADC/DAC 模拟量、CAN 回环、PWM 控制、LED/蜂鸣器/按键、NFC、指纹、GPS 回放、物资库存、日志记录、参数设置和分级报警组合成一个完整的危险品运输与仓储安全监控系统。

它的核心特点不是简单堆叠外设，而是通过统一数据源管理、统一页面按钮表、统一身份认证状态、统一报警状态机和统一日志接口，把多个基础实验组织成可操作、可验证、可演示的综合项目。对于无法稳定获得真实变化的模块，工程保留真实接口，同时采用模拟/回放/适配器方式完成闭环验证，并在界面上明确显示 `REPLAY`、`LOOPBACK`、`SIM_OFF` 等状态，保证实验结果既稳定又符合实际条件。

