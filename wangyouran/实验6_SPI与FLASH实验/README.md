# 实验6 — SPI 与 FLASH 实验

## 实验概述

基于 STM32F4 的硬件 SPI1 外设驱动 W25Q64 NOR Flash，实现器件识别、扇区擦除、页写入和批量读写功能，并通过串口输出写入-读出-校验结果。W25Q64 是一款 8MB（64Mbit）SPI 接口 NOR Flash，支持 4KB 扇区擦除、256 字节页编程，常用于固件存储、字库、日志等应用。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| SPI SCK | PA5 | SPI1 | 时钟线 |
| SPI MISO | PA6 | SPI1 | 主入从出 |
| SPI MOSI | PA7 | SPI1 | 主出从入 |
| SPI CS | PC0 | GPIO 推挽输出 | 片选，低电平有效，软件控制 |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

## 文件结构

```
实验6_SPI与FLASH实验/
├── USER/
│   └── main.c                  # 主程序：读ID → 擦除 → 写测试 → 读校验
├── BSP/
│   ├── flash_w25qxx.c / .h     # W25QXX Flash 驱动（SPI 配置 + 全部读写擦除接口）
│   └── usart.c       / .h      # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    ├── delay.c                 # 软件循环延时（备用）
    └── utils.c                 # 工具函数（预留）
```

## W25Q64 器件特性

| 参数 | 值 |
|------|-----|
| 总容量 | 8MB（64Mbit）= 8,388,608 字节 |
| 组织结构 | 128 块（Block）× 64KB = 8192 扇区（Sector）× 4KB |
| 页大小 | 256 字节/页 |
| 扇区大小 | 4KB（最小擦除单位） |
| 块大小 | 64KB |
| SPI 模式 | 支持 Mode 0 (CPOL=0, CPHA=0) 和 Mode 3 (CPOL=1, CPHA=1) |
| 最高时钟 | 80MHz（标准读）/ 104MHz（快速读，部分型号） |
| JEDEC ID | `0xEF4017`（W25Q64 制造商+型号） |
| 写入前要求 | **必须先擦除**（Flash 物理特性：擦除置 1，写入只能将 1 变为 0） |

**存储层次结构**：

```
W25Q64 (8MB)
├── Block 0 (64KB)
│   ├── Sector 0  (4KB)  ← 本实验操作此扇区
│   ├── Sector 1  (4KB)
│   ├── ...
│   └── Sector 15 (4KB)
├── Block 1 (64KB)
├── ...
└── Block 127 (64KB)
```

## 核心模块说明

### 1. SPI 初始化 (`W25QXX_Init`)

#### 引脚配置

```
PA5 (SCK)  ──→ AF: GPIO_AF_SPI1, PP
PA6 (MISO) ──→ AF: GPIO_AF_SPI1, PP
PA7 (MOSI) ──→ AF: GPIO_AF_SPI1, PP
PC0 (CS)   ──→ GPIO Output, PP  (软件片选)
```

**CS 片选**采用 GPIO 软件控制而非硬件 NSS，通过宏封装：

```c
#define W25QXX_CS_LOW()   GPIO_ResetBits(GPIOC, GPIO_Pin_0)
#define W25QXX_CS_HIGH()  GPIO_SetBits(GPIOC, GPIO_Pin_0)
```

每次 SPI 事务前后必须拉低/拉高 CS 以帧定通信边界。

#### SPI 外设配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 模式 | `SPI_Mode_Master` | 全双工主模式 |
| 数据位 | `SPI_DataSize_8b` | 8 位帧 |
| CPOL | `SPI_CPOL_High` | 时钟空闲高电平 |
| CPHA | `SPI_CPHA_2Edge` | 第 2 个边沿采样 |
| SPI 模式 | Mode 3 | CPOL=1, CPHA=1 |
| NSS | `SPI_NSS_Soft` | 软件片选 |
| 预分频 | `SPI_BaudRatePrescaler_2` | fPCLK/2 = **42MHz** |
| 位序 | `SPI_FirstBit_MSB` | 高位先发 |

SPI1 挂 APB2（84MHz），2 分频后 SCLK = **42MHz**，在 W25Q64 支持范围内。

### 2. 数据传输基础

#### SPI 字节发送 (`W25QXX_SendByte`)

SPI 的本质是**全双工移位寄存器**——每发送一个字节的同时也接收一个字节：

```c
uint8_t W25QXX_SendByte(uint8_t byte) {
    W25QXX_WaitSpiFlag(SPI_I2S_FLAG_TXE, SET);   // 等待发送寄存器空
    SPI_I2S_SendData(W25QXX_SPI, byte);            // 写入 DR → 启动传输
    W25QXX_WaitSpiFlag(SPI_I2S_FLAG_RXNE, SET);   // 等待接收寄存器满
    return SPI_I2S_ReceiveData(W25QXX_SPI);        // 读回接收到的数据
}
```

**超时保护**：`W25QXX_WaitSpiFlag` 内建超时计数（0x100000 次），超时返回 -1，防止 SPI 异常时死锁。

#### SPI 字节接收 (`W25QXX_ReadByte`)

读取的本质是**发送哑字节（Dummy Byte）并接收返回数据**：

```c
uint8_t W25QXX_ReadByte(void) {
    return W25QXX_SendByte(DUMMY_BYTE);  // 发送 0xFF，收到 Flash 返回数据
}
```

`DUMMY_BYTE = 0xFF` 可使 MOSI 保持高电平，对从机无副作用。

### 3. 器件识别 (`W25QXX_ReadID`)

读取 JEDEC 制造商/器件 ID 以确认 Flash 型号：

```
时序：
  CS ↓ → 发送 0x9F → 接收 Manufacturer ID (0xEF) → 接收 Memory Type (0x40) → 接收 Capacity (0x17) → CS ↑

返回值：
  Temp = (0xEF << 16) | (0x40 << 8) | 0x17 = 0xEF4017 → W25Q64
```

如果 `ReadID` 返回 0，说明 SPI 通信异常或 Flash 未正确连接。

### 4. 写使能 (`W25QXX_WriteEnable`)

W25QXX 在上电、写操作完成、写禁用后会自动清除写使能锁存器（WEL）。每次**擦除/写入操作前**必须重新发送写使能命令：

```
CS ↓ → 发送 0x06 (Write Enable) → CS ↑
```

### 5. 忙等待 (`W25QXX_WaitForWriteEnd`)

擦除和写入需要时间（扇区擦除典型 45ms，页编程典型 0.4ms），通过轮询**状态寄存器**的 WIP（Write In Progress）位判断操作是否完成：

```
CS ↓ → 发送 0x05 (Read Status Reg) → 循环读取状态字节 → 直到 WIP=0 → CS ↑
                              ┌── 超时保护 (0x100000 次) ──┐
```

状态寄存器说明：

| 位 | 名称 | 说明 |
|----|------|------|
| 0 (LSB) | BUSY / WIP | 1 = 正在写入/擦除，0 = 空闲 |

### 6. 扇区擦除 (`W25QXX_SectorErase`)

擦除操作将扇区（4KB）内的所有位重置为 `0xFF`——写入前必须先擦除：

```
写使能 → CS↓ → 发送 0x20 (Sector Erase) → 发送 24-bit 地址 → CS↑ → 等待完成
```

24 位地址分为 3 字节发送（高位先发），W25Q64 有效地址范围 `0x000000` ~ `0x7FFFFF`。

**注意**：Flash 的写入只能将 1 变为 0，要写新数据先需擦除全 1，这正是 NOR Flash 与 EEPROM 的关键区别。

### 7. 页写入 (`W25QXX_PageWrite`)

W25Q64 页大小 256 字节，单次页写入内地址自增，但**不可跨页**——地址到达页末（如 `0x00FF`）后回卷到页首，而非跨到下一页：

```
写使能 → CS↓ → 发送 0x02 (Page Program) → 发送 24-bit 地址 → 循环发送数据 → CS↑ → 等待完成
```

**安全防护**：若传入数据量超过 `W25QXX_PER_WRITE_PAGE_SIZE`（256 字节），自动截断并打印警告。

### 8. 跨页批量写入 (`W25QXX_BufferWrite`)

与 E2PROM 实验相同的对齐分页算法，只是页大小放大为 256 字节：

```
算法流程：

第一步（对齐处理）：
  PageIndex = WriteAddr % 256
  若非页对齐，先写首页尾部补齐

第二步（整页循环）：
  逐页写入 256 字节

第三步（尾页补齐）：
  写入最后不足一页的零散数据
```

### 9. 批量读取 (`W25QXX_BufferRead`)

读取操作不需要擦除、不需要写使能，可以**连续读取任意长度**：

```
CS↓ → 发送 0x03 (Read Data) → 发送 24-bit 地址 → 循环读取 N 字节 → CS↑
```

Flash 内部地址指针自动递增，支持跨页连续读取，直到 CS 拉高结束。

### 10. 主程序测试流程 (`main.c`)

```
1. 初始化 USART2 + W25QXX
2. 读取 JEDEC ID 验证器件
   ├── ID = 0xEF4017 → W25Q64 正常
   └── ID = 0 → 通信失败，报错
3. 擦除扇区 0（4KB，为写入做准备）
4. 制造测试数据 BufWrite[0..255] = {0x00, 0x01, ..., 0xFF}
5. W25QXX_BufferWrite → 写入 256 字节到扇区 0
6. W25QXX_BufferRead  → 读出 256 字节
7. 逐字节比较校验
   ├── 全部匹配 → 输出 "TEST OK"
   └── 发现差异 → 输出错误位置及数据
```

串口输出示例：

```
W25QXX Init OK
ReadID:[EF4017]
SectorErase OK
DATA Writing:
0x00 0x01 0x02 ... 0x0F
0x10 0x11 0x12 ... 0x1F
...(共 16 行)...
0xF0 0xF1 0xF2 ... 0xFF

DATA Write OK:
DATA Reading:
0x00 0x01 0x02 ... 0x0F
0x10 0x11 0x12 ... 0x1F
...(共 16 行)...
0xF0 0xF1 0xF2 ... 0xFF

TEST OK
DATA Read OK:
```

## 命令集速查

| 命令 | 操作码 | 说明 |
|------|--------|------|
| Write Enable | `0x06` | 置位 WEL，擦写前必须发送 |
| Write Disable | `0x04` | 清零 WEL |
| Read Status Reg | `0x05` | 读状态寄存器（含 WIP 位） |
| Write Status Reg | `0x01` | 写状态寄存器 |
| Read Data | `0x03` | 标准读 |
| Fast Read | `0x0B` | 快速读（含 dummy byte） |
| Page Program | `0x02` | 页编程（最多 256 字节） |
| Sector Erase | `0x20` | 扇区擦除（4KB） |
| Block Erase | `0xD8` | 块擦除（64KB） |
| Chip Erase | `0xC7` | 全片擦除 |
| JEDEC ID | `0x9F` | 读取制造商+型号 ID |
| Power Down | `0xB9` | 进入低功耗 |
| Release PD | `0xAB` | 退出低功耗 |

## 关键技术点

| 技术 | 说明 |
|------|------|
| SPI 主模式 | 全双工 4 线制（SCK/MOSI/MISO/CS），42MHz |
| SPI Mode 3 | CPOL=1（空闲高）, CPHA=1（第2边沿采样） |
| 软件片选 | GPIO 推挽控制 CS，取代硬件 NSS |
| 写使能锁存 | 每次擦写前必须 `0x06` 置位 WEL |
| 擦后写 | NOR Flash 物理特性：擦除归 1，写入只能 1→0 |
| WIP 轮询 | 通过状态寄存器 BUSY 位判断操作完成 |
| 跨页写入 | 256 字节页对齐拆分算法，防地址回卷 |
| JEDEC 识别 | `0x9F` 读取 3 字节 ID，确认 Flash 型号 |
| 超时保护 | WaitSpiFlag 和 WaitForWriteEnd 均带超时退避 |

## 代码注意事项

1. **头文件声明未全部实现**：`flash_w25qxx.h` 声明了 `W25QXX_ReadDeviceID`、`W25QXX_BulkErase`、`W25QXX_StartReadSequence`、`W25QXX_PowerDown`、`W25QXX_WakeUp` 等函数，但在 `.c` 文件中未提供实现，属于预留接口，当前仅调用了已实现的函数。

2. **`delay_ms` 使用软件循环**：`UTILS/delay.c` 中的延时基于空循环，受编译优化和 CPU 频率影响。所幸本实验中 W25QXX 忙等待使用 `W25QXX_WaitForWriteEnd`（硬件轮询），不依赖软件延时。

3. **页写入超量截断**：`W25QXX_PageWrite` 对超过 256 字节的数据仅截断并打印警告，不会返回错误码。调用者需自行确保每次页写入数据量不超过 256 字节——`W25QXX_BufferWrite` 已自动保证此约束。

4. **`utils.c` 为空**：预留工具函数扩展文件，当前仅包含 `#include "utils.h"`。
