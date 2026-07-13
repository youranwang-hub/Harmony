# 实验5 — I2C 与 E2PROM 实验

## 实验概述

基于 STM32F4 的硬件 I2C1 外设驱动 AT24C02 EEPROM，实现单字节到多页批量数据的写入与读取，并通过串口输出自校验结果。AT24C02 是 256 字节（32页 × 8字节/页）的 I2C 接口 EEPROM，常用于掉电不丢失的小容量参数存储。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| I2C SCL | PB6 | I2C1 | 时钟线，开漏输出 |
| I2C SDA | PB7 | I2C1 | 数据线，开漏输出 |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

## 文件结构

```
实验5_I2C与E2PROM实验/
├── UESR/
│   └── main.c                  # 主程序：写入测试 → 读出校验 → 结果输出
├── BSP/
│   ├── e2prom_at24c02.c / .h   # AT24C02 驱动（I2C 配置 + 读写接口）
│   └── usart.c       / .h      # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    ├── delay.c                 # 软件循环延时（供 WaitE2promOK 使用）
    └── utils.c                 # 工具函数（预留）
```

## AT24C02 器件特性

| 参数 | 值 |
|------|-----|
| 总容量 | 2048 bit = 256 字节 |
| 页大小 | 8 字节/页，共 32 页 |
| I2C 地址 | 7 位：`0x50`（A2=A1=A0=0） |
| 写周期 | ≤ 5ms（页写入后需等待内部编程完成） |
| 通信速率 | 支持 100kHz（标准）和 400kHz（快速） |
| 耐久性 | 约 100 万次擦写 |

**器件地址格式**（8 位，含 R/W 位）：

```
  1  0  1  0  A2 A1 A0 R/W
  └─固定──┘ └─引脚─┘ └─┘
               硬件地址   0=写, 1=读
```

本实验 A2/A1/A0 均接地，写地址 = `0xA0`，读地址 = `0xA1`。

## 核心模块说明

### 1. I2C 初始化 (`e2prom_at24c02.c`)

#### GPIO 配置
```c
PB6 (SCL), PB7 (SDA)
├── Mode:    AF（复用功能）
├── OType:   OD（开漏输出，I2C 必要）
├── Speed:   50MHz
├── PuPd:    NOPULL（外接上拉电阻）
└── AF:      GPIO_AF_I2C1
```

**关键点**：I2C 总线必须配置为**开漏输出**，SCL 和 SDA 均需外部上拉电阻（通常 4.7kΩ）拉至 VCC。开漏特性允许多个设备共享总线并通过线与逻辑仲裁。

#### I2C 外设配置
- 模式：`I2C_Mode_I2C`（主模式）
- 速率：**100kHz** 标准模式（代码中 `I2C_Speed = 100000`）
- 占空比：`I2C_DutyCycle_2`（快速模式下标准占空比 2:1）
- 寻址：7 位寻址模式
- 应答：ACK 使能

### 2. 超时保护机制

所有 I2C 等待函数均内建超时退避，防止总线异常时死锁：

```c
int8_t AT24C02_WaitI2cEvent(uint32_t I2C_EVENT) {
    __IO uint32_t I2CTimeout = 0x10000;
    while(!I2C_CheckEvent(AT24C02_I2C, I2C_EVENT)) {
        if((I2CTimeout--) == 0) return -1;  // 超时退出
    }
    return 0;
}
```

返回值约定：**0 = 成功，-1 = 超时/失败**，调用方可据此判断通信是否正常。

### 3. E2PROM 就绪等待 (`AT24C02_WaitE2promOK`)

AT24C02 在写入数据后需一定时间完成内部编程（最大 5ms），此期间**不应答**任何 I2C 请求：

```
轮询流程（最多 100ms）：
  loop:
    发送 START
    发送器件地址 + 写方向(0)
    if (收到 ACK) → 器件就绪，退出
    if (收到 NACK) → 器件忙，继续轮询
  → 发送 STOP
```

**原理**：AT24C02 内部编程期间将所有 I2C 请求 NACK；一旦返回 ACK，表示可以接受新操作。此方法比固定 `delay_ms(5)` 更精确高效。

### 4. 页写入 (`AT24C02_PageWrite`)

AT24C02 每页 8 字节，同一页内的连续写入地址会自动回卷（地址 `0x07` 之后回卷到 `0x00`，不会跨页）。

**I2C 时序**：

```
S | 设备地址+W | ACK | 内存地址 | ACK | 数据1 | ACK | ... | 数据N | ACK | P
```

单次页写入流程：
1. 检测总线空闲（`I2C_FLAG_BUSY`）
2. 发送 **START + 器件地址(写)**
3. 发送 **目标内存地址**
4. 逐个发送数据字节
5. 发送 **STOP**
6. 调用 `AT24C02_WaitE2promOK()` 等待编程完成

### 5. 跨页批量写入 (`AT24C02_BufferWrite`)

当需要写入的数据跨越页边界时，需拆分为多次页写入：

```
算法流程（假设从地址 Addr 写入 Number 字节）：

第一步（对齐处理）：
  PageIndex = Addr % 8           // 起始地址在页内偏移
  if (PageIndex != 0):
    FirstCount = 8 - PageIndex   // 首页剩余空间
    写入 FirstCount 字节到当前页

第二步（整页写入）：
  PageNum = Number / 8           // 整页数
  逐页写入 PageNum × 8 字节

第三步（末尾处理）：
  ByteNum = Number % 8           // 尾端零散字节
  if (ByteNum > 0):
    写入最后一行 ByteNum 字节
```

该算法确保每次调用 `AT24C02_PageWrite` 的数据量不超过 8 字节且不跨页边界。

### 6. 批量读取 (`AT24C02_BufferRead`)

使用 I2C **复合读写**格式——先写地址再读数据，中间不产生 STOP：

```
S | 设备地址+W | ACK | 内存地址 | ACK | Sr | 设备地址+R | ACK | 数据1 | ACK | ... | 数据N-1 | ACK | 数据N | NACK | P
│              └── Dummy Write 设置内部地址指针 ──││              └── Repeated START 后读取 ───────────│
```

**关键细节**：
- 倒数第二个字节后设置 **NACK**（`I2C_AcknowledgeConfig(DISABLE)`），通知从机这是最后字节
- 最后一个字节前发送 **STOP**，STM32 硬件将在字节传输完成后自动产生停止条件
- 函数末尾重新使能 ACK，避免影响后续操作

### 7. 主程序测试逻辑 (`main.c`)

完整的写入-读出-校验测试流程：

```
1. 制造测试数据：BufWrite[0..255] = {0x00, 0x01, ..., 0xFF}
2. 调用 AT24C02_BufferWrite(BufWrite, 0, 256) → 写入全部 256 字节
3. 调用 AT24C02_BufferRead(BufRead, 0, 256)  → 读回全部 256 字节
4. 逐字节比较 BufRead[i] == BufWrite[i]
   └── 全部匹配 → 输出 "TEST OK"
   └── 发现差异 → 输出错误数据并终止
```

串口输出格式：每 16 字节换行，十六进制显示，形如：

```
DATA Writing:
0x00 0x01 0x02 ... 0x0F
0x10 0x11 0x12 ... 0x1F
...
DATA Write OK:
DATA Reading:
0x00 0x01 0x02 ... 0x0F
...
TEST OK
DATA Read OK:
```

### 8. USART2 调试输出 (`usart.c`)

- `fputc()` 重定向 → printf 输出到 USART2
- `fgetc()` 重定向 → scanf/getchar 从 USART2 读取
- 串口中断回显：收到任意字节后原样发回，便于调试

## 运行效果

连接串口助手（115200, 8N1），上电后自动执行测试：

```
AT24C02 Init OK
DATA Writing:
0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F
0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1A 0x1B 0x1C 0x1D 0x1E 0x1F
...(共 16 行)...
0xF0 0xF1 0xF2 0xF3 0xF4 0xF5 0xF6 0xF7 0xF8 0xF9 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF

DATA Write OK:
DATA Reading:
0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F
...(共 16 行)...
0xF0 0xF1 0xF2 0xF3 0xF4 0xF5 0xF6 0xF7 0xF8 0xF9 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF

TEST OK
DATA Read OK:
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| I2C 总线 | 两线制串行总线，开漏+上拉，7 位寻址 |
| 硬件 I2C1 | STM32F4 片上 I2C 外设，100kHz 标准模式 |
| 开漏输出 | SCL/SDA 必须设为 OD，实现多主线与仲裁 |
| 页写入限制 | AT24C02 每页 8 字节，跨页需拆分为多次传输 |
| 跨页算法 | 首页对齐 + 整页循环 + 尾页补齐，自动处理任意偏移和长度 |
| 复合读写 | Repeated START 先写地址指针再读数据，无需 STOP 间隔 |
| NACK 终止 | 读最后一个字节前关闭 ACK，通知从机结束传输 |
| 轮询就绪 | 发地址并检测 ACK/NACK 替代固定延时，精确判断 EEPROM 内部编程完成 |
| 超时保护 | 所有等待循环内建超时计数，防止总线死锁 |
| printf 重定向 | `fputc` 映射到 USART2，实现标准输出到串口 |

## 代码注意事项

1. **`delay_ms` 使用软件循环**：`UTILS/delay.c` 中的 `delay_ms` 基于空循环实现，实测延时值受编译器优化级别和 CPU 频率影响，不精确。`AT24C02_WaitE2promOK` 中轮询间隔使用的是此函数。在优化级别稳定的情况下可用，但建议改用 SysTick 定时器实现精确毫秒延时。

2. **`AT24C02_ByteWrite` 未实现**：头文件 `e2prom_at24c02.h` 声明了 `AT24C02_ByteWrite` 函数，但 `.c` 文件中未提供定义。如需单字节写入，可直接调用 `AT24C02_PageWrite(pBuffer, WriteAddr, 1)`。

3. **通信速率**：代码注释中标注"STM32 I2C 快速模式"，但实际 `I2C_Speed = 100000`（100kHz）属于**标准模式**。快速模式应为 400kHz。AT24C02 两种速率均支持，当前配置正确。

4. **`utils.c` 为空**：UTILS 目录下的 `utils.c` 仅包含 `#include "utils.h"`，暂无具体实现，预留给后续工具函数扩展。
