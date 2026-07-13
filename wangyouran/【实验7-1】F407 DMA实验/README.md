# 实验 7-1：DMA 串口发送数据

## 实验目的

掌握 STM32F407 的 DMA（直接存储器访问）工作原理，学会配置 DMA 与 USART 外设配合使用，实现无需 CPU 干预的数据传输。

## 硬件连接

| 外设 | 引脚 | 说明 |
|------|------|------|
| USART2 TX | PA2 | 串口发送，复用推挽输出 |
| USART2 RX | PA3 | 串口接收，复用推挽输出 |
| DMA | DMA1 Stream6 Channel4 | 对应 USART2_TX 通道 |

> **注意**：大多数开发板的板载 USB 转串口（CH340）默认接 USART1（PA9/PA10），而非 USART2（PA2/PA3）。若使用板载 USB 口直接连接电脑收不到数据，请用 USB 转 TTL 模块外接到 PA2/PA3，或修改代码为 USART1。

## USART2 配置参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 波特率 | 115200 | |
| 数据位 | 8bit | `USART_WordLength_8b` |
| 停止位 | 1bit | |
| 校验位 | 无 | |
| 硬件流控 | 无 | |
| 模式 | 收发 | `USART_Mode_Rx \| USART_Mode_Tx` |

## DMA 配置参数（DMA1 Stream6, Channel4）

| 参数 | 值 | 说明 |
|------|-----|------|
| 通道 | Channel 4 | USART2_TX |
| 方向 | 存储器 → 外设 | `DMA_DIR_MemoryToPeripheral` |
| 外设地址 | `&USART2->DR` | 串口数据寄存器，不递增 |
| 存储器地址 | `SendBuf` | 发送缓冲区首地址，递增 |
| 传输量 | 17 字节 | `"Hello Everyone!\r\n"` |
| 数据宽度 | 8bit × 8bit | 双方均为 Byte |
| 模式 | 普通模式 | 传完即停 |
| 优先级 | 中等 | |
| FIFO | 关闭 | |
| 突发传输 | 单次 | |

## 信号链路

```
SendBuf[] = "Hello Everyone!\r\n"
     │ (17 bytes, 存储器地址递增)
     ▼
DMA1_Stream6_Ch4  ──→  USART2_DR (外设地址不变)
                              │
                              ▼
                        PA2 (TX) ──→ USB转TTL ──→ 电脑串口助手
```

## 运行流程

```
上电/复位
  ↓
初始化 USART2（PA2/PA3，115200 8N1）
  ↓
开启 DMA1 时钟
  ↓
使能 USART2 DMA 发送
  ↓
配置 DMA1 Stream6（Mem→Periph, 17字节, Normal模式）
  ↓
启动 DMA → 硬件自动搬运 17 字节到 USART2 发送
  ↓
等待 DMA 传输完成标志（TCIF6）
  ↓
while(1) 死循环
```

## 预期现象

1. 使用 USB 转 TTL 模块连接 PA2（TX）和 GND 到电脑
2. 打开串口助手，波特率 115200，8N1，打开对应 COM 口
3. 开发板上电或按 RESET 键，串口助手收到：

```
Hello Everyone!
```

（程序仅发送一次，不会循环发送）

## 关键技术点

### DMA vs 普通串口发送

| | 普通方式 | DMA 方式 |
|------|---------|---------|
| 数据搬运 | CPU 逐字节写入 DR | DMA 硬件自动搬运 |
| CPU 占用 | 发送期间 CPU 被占用 | CPU 完全解放 |
| 适用场景 | 少量数据 | 大量/高速数据传输 |

### DMA 通道映射（STM32F407）

每个 DMA Stream 可映射到不同的外设请求通道：

| 外设 | DMA | Stream | Channel |
|------|-----|--------|---------|
| USART2_TX | DMA1 | Stream6 | Channel 4 |
| USART2_RX | DMA1 | Stream5 | Channel 4 |

### Normal 模式 vs Circular 模式

| 模式 | 说明 |
|------|------|
| Normal | 传输指定数量后自动停止，需手动重新配置 |
| Circular | 传输完成后自动重载，循环不停 |

本实验使用 Normal 模式，发送一次即停止。

## 文件结构

```
7-1/
  basic/
    main.c       # 主程序：DMA 串口发送
    delay.c      # 简单延时函数（US 级 + MS 级）
    delay.h      # 延时函数声明
  README.md      # 本文档
```

## 代码修复记录

| # | 问题 | 修复 |
|---|------|------|
| 1 | `delay_ms` 参数类型 `u16` 与头文件 `uint16_t` 不一致 | 统一为 `uint16_t` |
| 2 | 第 40 行全角空格 `　DMA_Cmd` | 改为半角空格 |
| 3 | DMA 发送后无传输完成等待 | 添加 `while(DMA_GetFlagStatus(...))` 等待 TCIF6 |
| 4 | 开启了 DMA Rx 但无对应配置 | 移除 `USART_DMACmd(..., Rx, ENABLE)` |
| 5 | 函数外多出孤立的大括号 `}` | 删除 |
| 6 | `USART2_Init` 函数内部缩进不一致 | 统一缩进 |
