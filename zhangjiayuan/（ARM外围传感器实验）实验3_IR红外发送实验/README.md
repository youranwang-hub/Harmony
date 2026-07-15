# 实验3 IR 红外发送实验

本实验在红外接收模块基础上增加红外发送功能，实现 STM32F407 通过 TIM10 输出 38 kHz 载波，并按照 NEC 协议发送地址码和键码。工程同时保留红外接收模块，用于验证开发板自发自收结果。

## 代码结构

```text
代码/
├── BSP/
│   ├── ir_send.c / ir_send.h    # 红外发送驱动
│   ├── ir_recv.c / ir_recv.h    # 红外接收与 NEC 解码
│   └── usart.c / usart.h        # USART2 调试串口
├── USER/
│   └── main.c                   # 主程序
└── UTILS/
    ├── delay.c / delay.h        # SysTick 微秒/毫秒延时
    └── utils.c / utils.h
```

## 硬件与外设

| 功能 | 外设/引脚 | 说明 |
| --- | --- | --- |
| 红外发送 | PB8 / TIM10_CH1 | 输出 38 kHz PWM 载波 |
| 红外接收 | PE5 / TIM9_CH1 | 输入捕获下降沿，解析 NEC 帧 |
| 调试串口 | USART2，PA2/PA3 | 115200，8N1，用于 `printf` 输出 |

## 主要实现

### 红外发送

`ir_send.c` 中 `IR_Send_Init()` 完成红外发送初始化：

- `IR_Send_GPIO_Init()` 将 PB8 配置为 TIM10 通道 1 复用输出。
- `IR_Send_PWM_Init()` 配置 TIM10 PWM，周期 `4421`、比较值 `2210`，用于生成约 38 kHz 载波。
- `PwmEnable()` 开启载波输出。
- `PwmDisable()` 关闭载波输出。

`IR_Send(addr, code)` 按 NEC 协议发送完整数据帧：

1. 9 ms 载波引导脉冲。
2. 4.5 ms 空闲间隔。
3. 发送地址码 `addr`。
4. 发送地址反码 `~addr`。
5. 发送键码 `code`。
6. 发送键码反码 `~code`。
7. 发送 560 us 停止位。

`IR_Sent_Byte()` 按低位先发方式发送每个字节：

- 数据 0：560 us 载波 + 560 us 间隔。
- 数据 1：560 us 载波 + 1680 us 间隔。

### 红外接收

工程复用了 `ir_recv.c`：

- PE5 配置为 TIM9_CH1 复用输入。
- TIM9 计数频率为 1 MHz，即 1 个计数约 1 us。
- 捕获下降沿间隔来判断 NEC 引导码、重复码、数据 0 和数据 1。
- 接收满 32 位后设置 `IR_FrameReady`，由主循环调用 `IR_Recv()` 完成解析。
- 解析时校验键码和键码反码，支持标准 NEC 地址与扩展 NEC 地址。

## 主程序流程

`USER/main.c` 的流程：

1. 配置 SysTick。
2. 初始化 USART2，波特率 115200。
3. 调用 `IR_Recv_Init()` 初始化红外接收。
4. 调用 `IR_Send_Init()` 初始化红外发送。
5. 主循环中持续调用 `IR_Recv()` 解析接收到的红外帧。
6. 约每隔 10 秒执行一次 `IR_Send(1, 123)`，发送地址 `1`、键码 `123`。

## 运行现象

串口会先输出初始化信息：

```text
IR Recv Init OK
IR Send Init OK
```

周期发送时输出：

```text
IRSend:addr:1,code:123
```

如果板载红外接收头成功接收到本板发送的 NEC 帧，会打印类似：

```text
IRRecv:[847BFE01]
IRRecv:addr:1, code:123
```

## 使用方法

1. 使用 Keil MDK 打开或创建 STM32F407 标准外设库工程。
2. 将 `代码` 目录下的 `BSP`、`USER`、`UTILS` 加入工程。
3. 确认包含路径中加入对应头文件目录。
4. 编译并下载到 AU100/STM32F407 开发板。
5. 打开串口调试助手，设置 115200、8N1，观察发送和接收打印。

## 注意事项

- 发送端和接收端在本工程中同时启用，因此可以做自发自收验证。
- NEC 协议对微秒级时序较敏感，`systick_delay_us()` 的准确性会影响发送识别率。
- 如果串口只看到发送信息而没有接收结果，需要检查 PB8 红外发射、PE5 红外接收头、TIM10/TIM9 复用配置以及红外器件方向。

