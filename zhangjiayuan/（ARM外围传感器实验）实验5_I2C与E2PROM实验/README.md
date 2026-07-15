# 实验5 I2C 与 E2PROM 实验

本实验使用 STM32F407 的 I2C1 接口访问 AT24C02 E2PROM，完成 256 字节测试数据的写入、读出和逐字节校验，并通过 USART2 输出测试过程。

## 代码结构

```text
代码/
├── BSP/
│   ├── e2prom_at24c02.c / e2prom_at24c02.h    # AT24C02 I2C 驱动
│   ├── usart.c / usart.h                      # USART2 调试串口
│   └── delay.c / delay.h                      # 写周期等待延时
└── USER/
    └── main.c                                 # 主程序和读写校验逻辑
```

## 硬件与外设

| 功能 | 外设/引脚 | 说明 |
| --- | --- | --- |
| I2C SCL | PB6 / I2C1_SCL | 开漏复用输出 |
| I2C SDA | PB7 / I2C1_SDA | 开漏复用输出 |
| E2PROM | AT24C02 | 设备地址 `0xA0` |
| 调试串口 | USART2，PA2/PA3 | 115200，8N1 |

## 主要实现

### I2C 初始化

`AT24C02_Init()` 调用两个内部函数：

- `I2C_GPIO_Config()`：开启 GPIOB 和 I2C1 时钟，将 PB6/PB7 配置为 I2C1 复用开漏模式。
- `I2C_Mode_Config()`：配置 I2C 模式、7 位地址、ACK 使能和 100 kHz 通信速率。

### 等待函数

驱动中使用两个等待函数提高通信可靠性：

- `AT24C02_WaitI2cEvent()`：等待指定 I2C 事件，例如 EV5、EV6、EV8、EV7。
- `AT24C02_WaitI2cFlag()`：等待指定 I2C 标志位达到目标状态。

`AT24C02_WaitE2promOK()` 用于等待 AT24C02 内部写周期结束。因为 EEPROM 写入后需要时间完成内部存储，程序会循环发送起始条件和设备地址，直到器件重新应答。

### 写入与读取

`AT24C02_PageWrite()` 完成单页写入：

1. 等待 I2C 空闲。
2. 发送起始条件。
3. 发送设备地址和写方向。
4. 发送存储单元地址。
5. 连续发送数据。
6. 发送停止条件。
7. 等待 E2PROM 写周期结束。

AT24C02 每页 8 字节，`AT24C02_BufferWrite()` 会处理非页对齐地址、整页写入和最后不足一页的数据，避免跨页写入导致地址回卷。

`AT24C02_BufferRead()` 先以写方向发送待读地址，再重新发送起始条件并切换到读方向，连续读出指定长度的数据。

## 主程序流程

`USER/main.c` 的流程：

1. 初始化 USART2，波特率 115200。
2. 初始化 AT24C02。
3. 生成 0x00 到 0xFF 的 256 字节测试数据。
4. 调用 `AT24C02_BufferWrite(BufWrite, 0, 256)` 写入全部地址空间。
5. 调用 `AT24C02_BufferRead(BufRead, 0, 256)` 从地址 0 读回。
6. 逐字节比较 `BufRead` 和 `BufWrite`。
7. 数据完全一致时输出 `TEST OK`。

## 运行现象

串口输出流程大致为：

```text
AT24C02 Init OK
DATA Writing:
0x00 0x01 ... 0xFF
DATA Write OK:
DATA Reading:
0x00 0x01 ... 0xFF
TEST OK
DATA Read OK:
```

如果读回数据与写入数据不一致，程序会输出错误位置附近的读回值并打印 `not correct`。

## 使用方法

1. 将 `代码/BSP` 和 `代码/USER` 加入 STM32F407 标准外设库工程。
2. 确认 PB6、PB7 已连接 AT24C02 的 SCL、SDA。
3. 编译下载到开发板。
4. 打开串口调试助手，设置 115200、8N1。
5. 观察写入、读取和最终校验结果。

## 注意事项

- AT24C02 页大小为 8 字节，跨页写入必须拆分处理。
- EEPROM 写入不是立即完成，写操作后需要等待内部写周期结束。
- 如果一直无法得到 `TEST OK`，应检查 I2C 地址、SCL/SDA 连接、上拉电阻和工程头文件路径。

