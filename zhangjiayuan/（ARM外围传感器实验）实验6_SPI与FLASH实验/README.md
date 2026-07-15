# 实验6 SPI 与 FLASH 实验

本实验使用 STM32F407 的 SPI1 接口访问 W25Q64 串行 FLASH，完成 JEDEC ID 读取、扇区擦除、256 字节数据写入、读回和校验。

## 代码结构

```text
代码/
├── BSP/
│   ├── flash_w25qxx.c / flash_w25qxx.h    # W25Q64 SPI FLASH 驱动
│   └── usart.c / usart.h                  # USART2 调试串口
├── USER/
│   └── main.c                             # 主程序和读写校验逻辑
└── UTILS/
    ├── delay.c / delay.h
    └── utils.c / utils.h
```

## 硬件与外设

| 功能 | 外设/引脚 | 说明 |
| --- | --- | --- |
| SPI SCK | PA5 / SPI1_SCK | SPI 时钟 |
| SPI MISO | PA6 / SPI1_MISO | 主机输入 |
| SPI MOSI | PA7 / SPI1_MOSI | 主机输出 |
| FLASH CS | PC0 | 软件片选，低电平选中 |
| FLASH | W25Q64 | JEDEC ID 期望值 `0xEF4017` |
| 调试串口 | USART2，PA2/PA3 | 115200，8N1 |

## 主要实现

### SPI 初始化

`W25QXX_Init()` 完成 SPI FLASH 初始化：

1. 使能 GPIOA、GPIOC 和 SPI1 时钟。
2. 将 PA5、PA6、PA7 配置为 SPI1 复用功能。
3. 将 PC0 配置为普通 GPIO 输出，用作片选 CS。
4. 默认拉高 CS，结束通信状态。
5. 配置 SPI1 为双线全双工、主机模式、8 位数据、MSB 先行。
6. 使用 CPOL 高、CPHA 第二边沿的 SPI 模式 3。
7. 软件管理 NSS，波特率预分频为 2。
8. 使能 SPI1。

### 底层收发

`W25QXX_SendByte()` 是 SPI 读写基础函数：

- 等待 TXE 置位后发送 1 字节。
- 等待 RXNE 置位后读取 1 字节。
- SPI 为全双工，因此发送命令或 dummy byte 的同时也会收到数据。

`W25QXX_ReadByte()` 通过发送 `0xFF` dummy byte 读取一个字节。

### FLASH 命令

驱动实现了以下关键操作：

| 函数 | 功能 |
| --- | --- |
| `W25QXX_ReadID()` | 发送 `0x9F` 读取 JEDEC ID |
| `W25QXX_WriteEnable()` | 发送 `0x06` 写使能 |
| `W25QXX_WaitForWriteEnd()` | 读取状态寄存器，等待 WIP 位清零 |
| `W25QXX_SectorErase()` | 发送 `0x20` 擦除指定扇区 |
| `W25QXX_PageWrite()` | 发送 `0x02` 页编程 |
| `W25QXX_BufferWrite()` | 处理页边界，批量写入 |
| `W25QXX_BufferRead()` | 发送 `0x03` 连续读取数据 |

## 主程序流程

`USER/main.c` 的流程：

1. 初始化 USART2，波特率 115200。
2. 调用 `W25QXX_Init()` 初始化 SPI FLASH。
3. 调用 `W25QXX_ReadID()` 读取 JEDEC ID。
4. 擦除地址 0 所在扇区。
5. 生成 0x00 到 0xFF 的 256 字节测试数据。
6. 调用 `W25QXX_BufferWrite(BufWrite, 0, 256)` 写入地址 0。
7. 调用 `W25QXX_BufferRead(BufRead, 0, 256)` 读回数据。
8. 逐字节比较读写缓冲区。
9. 全部一致时输出 `TEST OK`。

## 运行现象

串口输出流程大致为：

```text
W25QXX Init OK
ReadID:[EF4017]
SectorErase OK
DATA Writing:
0x00 0x01 ... 0xFF
DATA Write OK:
DATA Reading:
0x00 0x01 ... 0xFF
TEST OK
DATA Read OK:
```

如果 `ReadID` 为 0 或不是预期值，应优先检查 SPI 引脚、CS 片选、供电和 SPI 模式。

## 使用方法

1. 将 `代码/BSP`、`代码/USER`、`代码/UTILS` 加入 STM32F407 标准外设库工程。
2. 确认 PA5、PA6、PA7、PC0 与 W25Q64 连接正确。
3. 编译下载到开发板。
4. 打开串口调试助手，设置 115200、8N1。
5. 观察 ID、擦除、写入、读取和校验输出。

## 注意事项

- W25Q64 写入或擦除前必须先执行写使能命令。
- 擦除和写入后必须等待 WIP 位清零，否则后续读取可能得到旧数据或不稳定数据。
- 页编程最大 256 字节，跨页写入需要拆分，`W25QXX_BufferWrite()` 已处理页边界。
- CS 需要在一条完整命令开始前拉低，在命令结束后拉高。

