# 实验12 — NFC 读写卡实验

## 实验目的

- 理解 NFC 工作原理与应用场景
- 掌握 PN532 模块功能命令格式
- 掌握 PN532 模块的操作使用方法
- 掌握 PN532 驱动编程方法
- 实现 PN532 模块读写卡功能

## 实验概述

NFC 是当前手机等各种智能终端常用技术，在打卡、支付等各方面都有广泛应用。本实验通过 AU100（青软遨游 100）开发板自带的 **PN532 模块**实现了 NFC 对 **Mifare S50 标签卡**的数据读写。

通过本实验可深入理解 NFC 协议的交互流程、加强 STM32 的 UART 的编程，实验内容主要如下：

1. NFC 工程创建
2. NFC 串口通讯实现
3. NFC 基础命令实现
4. NFC 读写流程实现
5. NFC 测试示例实现

PN532 是 NXP 推出的**多协议 NFC 前端芯片**，支持 ISO/IEC 14443A/B、FeliCa 和 ISO 18092（NFC-IP1）协议，工作在 **13.56 MHz** 频段，可实现 NFC 读写器、卡模拟、点对点三种工作模式。本实验使用其**读写器模式**对 Mifare Classic S50 卡进行数据读写。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| PN532 TX | PC11 | UART4 RX | MCU 接收 PN532 应答 |
| PN532 RX | PC10 | UART4 TX | MCU 发送命令给 PN532 |
| PN532 ANT | - | PCB 天线 | 13.56MHz 线圈，读卡距离 ≤ 5cm |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

> **UART4 复用提醒**：本实验 PN532 也使用 **UART4（PC10/PC11）**——与实验10 GPS、实验11 ZN632 指纹**共用同一组引脚**。同一开发板需**分时复用**或外接扩展电路切换。

## 文件结构

```
实验12_NFC读写卡实验/
├── USER/
│   └── main.c                       # 主程序：每 35s 写+读+校验一次
├── BSP/
│   ├── nfc_pn532.c    / .h          # PN532 驱动（UART + HSU 协议 + Mifare 读写）
│   └── usart.c        / .h          # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    └── delay.c                      # 软件循环延时
```

## NFC 技术与原理

### 1. NFC 技术定位

```
┌──────────────────────────────────────────────────────────────┐
│                   短距离无线通讯技术                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Bluetooth (2.4GHz)  距离: 1-100m   速率: 1-3 Mbps           │
│           ▲                                                  │
│           │ 距离/速率递减                                     │
│           ▼                                                  │
│  Wi-Fi/ZigBee (2.4GHz) 距离: 10-100m  速率: 11-300 Mbps      │
│                                                              │
│  RFID (125KHz/13.56MHz/900MHz) 距离: 1cm-10m                 │
│           ▲                                                  │
│           │                                                   │
│           ▼                                                  │
│  NFC (13.56MHz)  距离: <10cm   速率: 106/212/424 kbps        │
│  ├─ Reader/Writer: 读写 NFC 标签                              │
│  ├─ Card Emulation: 模拟 NFC 卡片                             │
│  └─ Peer-to-Peer: 点对点通讯                                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 2. NFC 与 RFID 区别

| 特性 | NFC | RFID |
|------|-----|------|
| 工作频率 | 13.56 MHz | 125 KHz / 13.56 MHz / 900 MHz |
| 通讯距离 | < 10 cm | 1 cm ~ 10 m |
| 通讯方向 | 双向 | 通常单向 |
| 协议标准 | ISO/IEC 18092, 14443 | ISO/IEC 14443, 15693, EPC |
| 典型应用 | 移动支付、门禁、公交 | 物流、零售、资产管理 |
| 安全机制 | 完善（加密、签名） | 弱 |

### 3. NFC 协议栈

```
┌──────────────────────────────────────────────────────────────┐
│                       NFC 协议栈                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  应用层 (Application Layer)                                  │
│  ├─ Mifare Classic (S50/S70)                                │
│  ├─ Mifare Ultralight / DESFire / NTAG                      │
│  ├─ NDEF (NFC Data Exchange Format)                         │
│  └─ ISO 7816 (智能卡)                                        │
│                                                              │
│  协议层 (Protocol Layer) - ISO/IEC 14443                     │
│  ├─ Type A (NXP Mifare, 常见)                                │
│  ├─ Type B (身份证、银行卡)                                  │
│  └─ FeliCa (日本索尼)                                        │
│                                                              │
│  物理层 (Physical Layer)                                     │
│  └─ 13.56 MHz RF 电磁场，电感耦合                            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 4. Mifare S50 卡片结构

Mifare Classic S50 是 NXP 推出的 1KB 容量 NFC 标签，结构如下：

```
┌──────────────────────────────────────────────────────────────┐
│                Mifare S50 存储结构 (1KB)                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  扇区 0  │ 块 0 (厂商块)        │ UID 4B + 厂商数据 12B      │
│         │ 块 1 (数据块)        │ 用户数据 16B                │
│         │ 块 2 (数据块)        │ 用户数据 16B                │
│         │ 块 3 (控制块)        │ KeyA 6B + 控制 4B + KeyB 6B │
│  ────────────────────────────────────────────────────────    │
│  扇区 1  │ 块 4 (数据块)        │ 用户数据 16B                │
│         │ 块 5 (数据块)        │ 用户数据 16B                │
│         │ 块 6 (数据块)        │ 用户数据 16B                │
│         │ 块 7 (控制块)        │ KeyA + 控制 + KeyB          │
│  ────────────────────────────────────────────────────────    │
│  ... (扇区 2~15 同上)                                        │
│                                                              │
│  总计：16 扇区 × 4 块 = 64 块 × 16 字节 = 1024 字节          │
│  用户数据：48 数据块 × 16B = 768 字节                         │
│  16 个控制块 (KeyA + 控制位 + KeyB)                          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**控制块结构**（16 字节）：

```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│  KeyA    │  Access  │  Access  │  Access  │  KeyB    │
│  6 字节  │  Byte 0  │  Byte 1  │  Byte 2  │  6 字节  │
│          │          │          │          │          │
│  FF×6    │  FF 07 80 69        │          │  FF×6    │
│  默认    │  默认值  │  默认值  │  默认值  │  默认    │
└──────────┴──────────┴──────────┴──────────┴──────────┘
```

**Access 字节**（4 bit × 3 = 12 bit）控制每块的访问权限（C1/C2/C3）。

### 5. PN532 模块特性

| 参数 | 值 |
|------|-----|
| 工作频率 | 13.56 MHz |
| 支持协议 | ISO/IEC 14443A/B, FeliCa, ISO 18092 (NFC-IP1) |
| 接口 | UART (HSU) / SPI / I²C |
| UART 速率 | 9600 ~ 115200 bps（默认 115200） |
| 工作电压 | 2.7 ~ 5.5V |
| 读卡距离 | ≤ 5cm（取决于天线） |
| 内置处理器 | 80C52 8-bit MCU |
| 工作温度 | -30 ~ +85°C |
| 封装 | HVQFN40 (6×6mm) |

## PN532 通讯协议

### 1. HSU 帧格式（UART 模式）

PN532 在 UART 模式下使用一种**类 HDLC 协议**：

```
┌─────────┬─────────┬─────────┬─────────┬─────────┬──────┬─────────┬──────┐
│ Preamble│ Start   │ LEN     │ LCS     │ TFI     │ DATA │ DCS     │ Post │
│ 1 字节  │ 2 字节  │ 1 字节  │ 1 字节  │ 1 字节  │ N 字节│ 1 字节  │ 1 字节│
│ 0x00    │ 0x00 0xFF│ n     │ 0x100-n │ 0xD4/0xD5│ ... │ 0x100-Σ │ 0x00 │
└─────────┴─────────┴─────────┴─────────┴─────────┴──────┴─────────┴──────┘
```

| 字段 | 长度 | 说明 |
|------|------|------|
| Preamble | 1 | 前导码，固定 `0x00` |
| Start | 2 | 起始码，固定 `0x00 0xFF` |
| LEN | 1 | 包长度 = TFI + DATA 的字节数（不含 LCS、DCS） |
| LCS | 1 | 长度校验和 = `0x100 - LEN`（取低 8 位） |
| TFI | 1 | 帧方向码：MCU→PN532 用 `0xD4`，PN532→MCU 用 `0xD5` |
| DATA | N | 命令/响应数据 |
| DCS | 1 | 数据校验和 = `0x100 - (TFI+DATA 累加和)` |
| Post | 1 | 后序码，固定 `0x00` |

### 2. ACK 帧（6 字节）

PN532 收到命令后会**先返回 ACK 帧**作为握手，再返回完整应答：

```
0x00 0x00 0xFF 0x00 0xFF 0x00
  │   │    │    │    │    │
  │   │    │    │    │    └─ Postamble
  │   │    │    │    └─ LCS = 0x100 - LEN
  │   │    │    └─ LEN = 0
  │   │    └─ Start Code
  │   └─ Start Code
  └─ Preamble
```

### 3. 关键命令码

#### 主机 → PN532 (TFI = 0xD4)

| 命令码 | 名称 | 参数 | 含义 |
|:-----:|------|------|------|
| `0x14` | SAMConfiguration | mode | SAM 安全访问模块配置（普通/虚拟卡/双卡） |
| `0x4A` | InListPassiveTarget | MaxTg, BrTy | 寻卡，返回 UID 和 ATS |
| `0x40` | InDataExchange | Tg, MifareCmd | 与已激活卡片数据交换 |

#### Mifare 命令（嵌入 InDataExchange）

| 命令码 | 名称 | 参数 | 含义 |
|:-----:|------|------|------|
| `0x60` | Auth_KeyA | 块号 + KeyA(6B) + UID(4B) | 验证 KeyA 密码 |
| `0x61` | Auth_KeyB | 块号 + KeyB(6B) + UID(4B) | 验证 KeyB 密码 |
| `0x30` | Read | 块号 | 读取 16 字节数据 |
| `0xA0` | Write | 块号 + 16 字节 | 写入 16 字节数据 |
| `0x70` | Mifare_HALT | (无) | 暂停卡片 |

### 4. 应答包结构

完整应答在 ACK 之后发送：

```
┌─────────┬─────────┬─────────┬─────────┬─────────┬──────┬─────────┬──────┐
│ Preamble│ Start   │ LEN     │ LCS     │ TFI=0xD5│ DATA │ DCS     │ Post │
│ 0x00    │ 0x00 0xFF│ ...   │ ...     │ 0xD5    │ ...  │ ...     │ 0x00 │
└─────────┴─────────┴─────────┴─────────┴─────────┴──────┴─────────┴──────┘
```

**InDataExchange 应答 DATA 段**：
- `DATA[0]` = 命令回显（如 `0x41`）
- `DATA[1]` = status（0 = 成功）
- `DATA[2..]` = 响应数据（如 Mifare 读取的 16 字节）

**InListPassiveTarget 应答 DATA 段**：
- `DATA[0]` = 命令回显 `0x4B`
- `DATA[1]` = Tg 数量（找到的卡数）
- `DATA[2]` = 第一个目标的状态
- `DATA[3..6]` = UID（4 字节）
- `DATA[7..]` = ATS / SAK 等

## 核心模块说明

### 1. UART4 初始化 (`NFC_Init`)

```c
USART_InitStructure.USART_BaudRate = 115200;   // PN532 默认速率
USART_InitStructure.USART_WordLength = USART_WordLength_8b;
USART_InitStructure.USART_StopBits = USART_StopBits_1;
USART_InitStructure.USART_Parity = USART_Parity_No;
USART_ITConfig(PN532_USART, USART_IT_RXNE, ENABLE);
```

### 2. 唤醒 PN532 (`NFC_WakeUp`)

```c
int8_t NFC_WakeUp(void) {
    // 1. 发送 14 字节 0x55 唤醒序列
    uint8_t data[14] = {0x55, 0x55, 0,0,0,0,0,0,0,0,0,0,0,0};
    PN532_SendData(data, sizeof(data));
    
    // 2. 配置 SAM 为普通模式
    if (PN532_SAMConfiguration(1) == 0) {
        printf("NFC WakeUP OK\r\n");
        return 0;
    }
    return -1;
}
```

**0x55 唤醒序列的作用**：PN532 上电后默认处于低功耗状态，发送 14 字节 0x55 后模块**切换到正常命令模式**（从默认 I²C 切换到 UART 模式）。

**SAMConfiguration 模式**：

| mode | 模式 | 用途 |
|:----:|------|------|
| 1 | 普通模式 | 默认，PN532 自身作为主控 |
| 2 | 虚拟卡模式 | 模拟 NFC 卡片 |
| 3 | 有线卡模式 | 通过外部主机控制 |
| 4 | 双卡模式 | 配合 SAM 芯片 |

### 3. HSU 帧构造（以 SAMConfiguration 为例）

```c
int8_t PN532_SAMConfiguration(uint8_t mode) {
    uint8_t data[10] = {0};
    
    data[0] = 0x00;                              // Preamble
    data[1] = 0x00; data[2] = 0xFF;              // Start Code
    data[3] = 0x03;                              // LEN = 3 (TFI + CMD + 1 byte)
    data[4] = 0x100 - data[3];                   // LCS = 0x100 - LEN
    data[5] = 0xD4;                              // TFI (Host → PN532)
    data[6] = CMD_SAMConfiguration;              // 命令码 0x14
    data[7] = mode;                              // 参数: 模式
    data[8] = PN532_CheckSum(data);              // DCS 校验
    data[9] = 0x00;                              // Postamble
    
    PN532_SendData(data, sizeof(data));
    if (PN532_CheckRxBuf() < 0) return -1;
    return 0;
}
```

**帧构造要点**：

```c
uint8_t PN532_CheckSum(uint8_t *data) {
    // 从 TFI 开始累加 data[INDEX_TFI..] 共 LEN 字节
    uint8_t temp = 0;
    for (i = 0; i < data[INDEX_LEN]; i++) {
        temp += data[INDEX_TFI + i];
    }
    return 0x100 - temp;  // 取低 8 位
}
```

### 4. 寻卡 (`PN532_InListPassiveTarget`)

```c
int8_t PN532_InListPassiveTarget(void) {
    // 构造: InListPassiveTarget, 找 1 张, TypeA 106kbps
    data[6] = CMD_InListPassiveTarget;   // 0x4A
    data[7] = 0x01;                       // MaxTg = 1 (最多 1 张)
    data[8] = 0x00;                       // BrTy = 0x00 (106 kbps TypeA)
    
    // ... 发送校验 ...
    
    // 解析应答: 卡数量必须 = 1
    if (PN532_RxBuf[LEN_ACK + INDEX_TFI + 2] != 1) return 0;  // 未找到
    
    // 提取 UID (4 字节)
    UID[0] = RxBuf[LEN_ACK + INDEX_TFI + 8];
    UID[1] = RxBuf[LEN_ACK + INDEX_TFI + 9];
    UID[2] = RxBuf[LEN_ACK + INDEX_TFI + 10];
    UID[3] = RxBuf[LEN_ACK + INDEX_TFI + 11];
    return 1;  // 找到
}
```

**关键参数**：

| BrTy | 含义 |
|:----:|------|
| 0x00 | 106 kbps Type A（本实验用） |
| 0x01 | 212 kbps FeliCa |
| 0x02 | 424 kbps FeliCa |
| 0x03 | 106 kbps Type B |
| 0x04 | 106 kbps Innovision Jewel |

### 5. 验证 KeyA (`PN532_PsdVerifyKeyA`)

```c
int8_t PN532_PsdVerifyKeyA(uint8_t *pKeyA) {
    data[6] = CMD_InDataExchange;   // 0x40
    data[7] = 0x01;                  // Tg = 1 号卡
    data[8] = 0x60;                  // Mifare Auth_KeyA 命令
    data[9] = 0x03;                  // 待验证的块号
    data[10..15] = pKeyA[0..5];      // KeyA (6 字节，默认 FF FF FF FF FF FF)
    data[16..19] = UID[0..3];        // 卡 UID (4 字节)
    // ... 发送校验 ...
    
    // 验证 status = 0
    if (RxBuf[LEN_ACK + INDEX_TFI + 2] != 0) return -1;
    return 0;
}
```

**为何需要 UID**：Mifare 卡密码验证需要"块号 + KeyA + UID"三要素，UID 用于**防重放攻击**（不同卡密码不能通用）。

### 6. 读取 16 字节 (`PN532_Read`)

```c
int8_t PN532_Read(uint8_t block, uint8_t *buf) {
    data[6] = CMD_InDataExchange;   // 0x40
    data[7] = 0x01;                  // 1 号卡
    data[8] = 0x30;                  // Mifare Read 命令
    data[9] = block;                 // 块号
    // ... 发送校验 ...
    
    // 拷贝 16 字节数据
    memcpy(buf, RxBuf + LEN_ACK + INDEX_TFI + 3, 16);
    return 0;
}
```

**应答结构**：`[CmdEcho=0x41][Status=0][Data(16B)]`

### 7. 写入 16 字节 (`PN532_Write`)

```c
int8_t PN532_Write(uint8_t block, uint8_t *buf) {
    data[6] = CMD_InDataExchange;   // 0x40
    data[7] = 0x01;                  // 1 号卡
    data[8] = 0xA0;                  // Mifare Write 命令
    data[9] = block;                 // 块号
    memcpy(data + 10, buf, 16);     // 16 字节数据
    // ... 发送校验 ...
    
    // 验证 status
    if (RxBuf[LEN_ACK + INDEX_TFI + 2] != 0) return -1;
    return 0;
}
```

**Mifare 写入特性**：
- 写入前**必须先验证 KeyA 或 KeyB**（取决于访问权限）
- 写入只能将 1 改成 0；**修改已写入数据需先"擦除"**（实际是通过读+改+写完成的）
- 写入需要 ~5ms 时间

### 8. 完整读写流程 (`NFC_Write` / `NFC_Read`)

```c
int8_t NFC_Write(uint8_t block, uint8_t *buf) {
    while (1) {
        delay_ms(100);
        
        // 1. 寻卡
        if (PN532_InListPassiveTarget() <= 0) continue;
        printf("card found, verify key...\r\n");
        
        // 2. 验证 KeyA
        if (PN532_PsdVerifyKeyA(KEY_A) < 0) {
            printf("key verify fail\r\n");
            continue;
        }
        printf("key OK, writing...\r\n");
        
        // 3. 写入
        if (PN532_Write(block, buf) < 0) {
            printf("write fail\r\n");
            continue;
        }
        break;  // 成功
    }
    return 0;
}
```

**完整交互流程**：

```
MCU                 PN532                  Mifare 卡
 │                    │                        │
 ├─ 0x55×14 ────────>│                        │   唤醒
 │                    │                        │
 ├─ SAMConfig ──────>│                        │   配置模式
 │<──── ACK ─────────┤                        │
 │<──── 应答 ─────────┤                        │
 │                    │                        │
 ├─ InListPassive ──>│                        │   寻卡
 │<──── ACK ─────────┤                        │
 │                    ├── RF ON ──────────────>│
 │                    │<─ ATQA ────────────────┤
 │                    ├── Anticollision ──────>│
 │                    │<─ UID ─────────────────┤
 │<──── 应答 (UID) ───┤                        │
 │                    │                        │
 ├─ InDataExchange ─>│                        │   验证 KeyA
 │                    ├── Auth_KeyA ──────────>│
 │                    │<─ CRYPTO1 OK ─────────┤
 │<──── ACK ─────────┤                        │
 │<──── 应答 (OK) ────┤                        │
 │                    │                        │
 ├─ InDataExchange ─>│                        │   读/写
 │                    ├── Read/Write ─────────>│
 │                    │<─ Data/ACK ────────────┤
 │<──── ACK ─────────┤                        │
 │<──── 应答 (Data) ──┤                        │
```

### 9. 应答验证 (`PN532_CheckRxBuf`)

```c
int8_t PN532_CheckRxBuf(void) {
    // 1. 轮询接收 200ms
    for (t = 0; t < 200; t++) {
        if (RXNE) RxBuf[RxBufLen++] = USART_ReceiveData();
        delay_ms(1);
    }
    
    // 2. 长度检查
    if (RxBufLen < LEN_ACK + INDEX_TFI + 4) return -1;
    
    // 3. LCS 校验（包长度校验和）
    len = RxBuf[LEN_ACK + INDEX_LEN];
    if ((0x100 - len) != RxBuf[LEN_ACK + INDEX_LEN + 1]) return -1;
    
    // 4. DCS 校验（数据校验和）
    temp = 0;
    for (i = 0; i < len; i++) temp += RxBuf[LEN_ACK + INDEX_TFI + i];
    if ((0x100 - temp) != RxBuf[LEN_ACK + INDEX_TFI + len]) return -1;
    
    return 0;
}
```

**双重校验**：
- **LCS**：包长度本身的校验
- **DCS**：TFI + DATA 累加校验

### 10. 主程序测试逻辑 (`main.c`)

```c
int main(void) {
    UART2_Init(115200);
    NFC_Init(115200);
    printf("NFC Init OK\r\n");
    
    NFC_WakeUp();                        // 唤醒 PN532
    make_test_data();                    // 生成测试数据 0x00~0x0F
    
    while (1) {
        if (NFC_Write(2, BufWrite) < 0) continue;   // 写扇区 0 块 2
        printf("Write OK\r\n");
        delay_ms(5000);
        
        if (NFC_Read(2, BufRead) < 0) continue;     // 读扇区 0 块 2
        printf("Read OK\r\n");
        
        check_test_data();                // 比对数据
        delay_ms(30000);
    }
}
```

**测试数据**：

```c
uint8_t BufWrite[16] = {0, 1, 2, ..., 15};   // 写入: 0x00~0x0F
uint8_t BufRead[16] = {0};                   // 读取: 比对
```

**块号选择**：块 2 是**用户数据块**（非厂商块 0，非控制块 3），便于测试。

## 运行效果

### 1. 启动初始化

```
NFC Init OK
NFC WakeUP OK
0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F
```

### 2. 放卡后读写测试

```
NFC Write: waiting card...
NFC Write: card found, verify key...
NFC Write: key OK, writing...
Write OK

（延时 5 秒）

NFC Read: waiting card...
NFC Read: card found, verify key...
NFC Read: key OK, reading...
Read OK
0x00 0x01 0x02 ... 0x0F

TEST OK
```

### 3. 错误处理

```
NFC Write: waiting card...
NFC Write: card found, verify key...
NFC Write: key verify fail   ← KeyA 验证失败
NFC Write: waiting card...   ← 重新寻卡
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| NFC | 13.56MHz 短距离双向无线通讯，< 10cm |
| RFID 区别 | 距离更近、双向、协议更完善 |
| 协议栈 | ISO/IEC 14443A/B / FeliCa / ISO 18092 |
| PN532 | NXP 多协议 NFC 前端，UART/SPI/I²C 接口 |
| Mifare S50 | 1KB 容量，16 扇区 × 4 块 × 16B |
| 存储结构 | 厂商块 + 数据块 + 控制块（KeyA+控制+KeyB） |
| HSU 协议 | 类 HDLC 帧：Preamble+Start+LEN+LCS+TFI+DATA+DCS+Post |
| ACK 帧 | 6 字节 0x00 0x00 0xFF 0x00 0xFF 0x00，命令握手 |
| TFI 标识 | 0xD4=Host→PN532, 0xD5=PN532→Host |
| LCS 校验 | `0x100 - LEN` |
| DCS 校验 | `0x100 - TFI+DATA 累加和` |
| 0x55 唤醒 | 14 字节 0x55 切换到 UART 命令模式 |
| SAM 配置 | 安全访问模块，普通/虚拟卡/双卡模式 |
| 寻卡命令 | InListPassiveTarget，返回 UID |
| Key 验证 | Mifare Auth_KeyA(0x60) + 块号 + KeyA + UID |
| UID 防重放 | 不同卡片密码不能通用 |
| 数据交换 | InDataExchange(0x40) 嵌入 Mifare 命令 |
| Mifare 命令 | 0x30 读 / 0xA0 写 / 0x60/0x61 验证 |
| 三段式交互 | ACK 握手 → 命令发送 → 应答接收 |
| 错误码 status | InDataExchange 应答中 status=0 表示成功 |
| printf 重定向 | `fputc` 映射到 USART2，方便调试 |

## 代码注意事项

1. **UART4 引脚冲突 GPS/ZN632 模块**：本实验 PN532 与实验10 GPS、实验11 ZN632 **共用 UART4 (PC10/PC11)**。实际同一开发板上**三个 NFC/指纹/GPS 模块不能同时使用**，需通过电源切换或单独调试。

2. **0x55 唤醒序列必需**：PN532 上电默认在**低功耗 HSU 待机模式**，必须先发送 14 字节 0x55 才能进入**正常命令模式**。如果跳过此步骤直接发命令，会**收不到任何应答**。

3. **ACK 帧不等同于完整应答**：PN532 的通信流程是：
   ```
   主机发送 → PN532 返回 ACK 帧 → PN532 返回完整应答
   ```
   **ACK 仅表示 PN532 收到命令**，不代表命令执行成功。**生产环境**应同时检查 ACK 帧和完整应答的 status 字段。

4. **HSU 协议的两次校验**：每帧有 LCS（长度校验）和 DCS（数据校验）**两重校验**，必须**都通过**：
   - LCS 失败 → 长度字段错误
   - DCS 失败 → 数据字段错误
   - **任一失败都必须丢弃应答并重试**

5. **`MAX_TRY = 0` 永久阻塞**：默认 `MAX_TRY = 0` 表示 `NFC_Write/Read` 在没有卡片时**永久循环扫描**。生产环境**必须设置有限重试次数**（如 100 次 × 100ms = 10 秒），否则用户拿走卡片后程序会永远卡住。

6. **KeyA 默认密码 `FF FF FF FF FF FF`**：Mifare S50 出厂默认 KeyA 是 6 字节 0xFF。如果卡片被**改过密码**，本实验的 `KEY_A` 必须同步更新。**生产环境**建议从 Flash 读取用户配置密钥，避免硬编码。

7. **Mifare 写操作的两步过程**：
   - 1. 写 16 字节到卡内 Buffer（约 1ms）
   - 2. PN532 自动发送 Commit 命令写入 EEPROM（约 4ms）
   
   **完整写入耗时约 5-6ms**。如果 `PN532_Write` 后立即断电，**数据可能丢失**。生产环境写入后**调用 HALT 或 Delay** 确保数据落盘。

8. **Mifare 写入只能 1→0**：与 Flash 类似，**写入只能将 1 改成 0**，不能 0→1。**修改已写入数据**需要先读出 → 修改 → 写回（**底层靠 EEPROM 实现**，与 Flash 不同，不需要显式擦除）。

9. **UID 不可修改**：块 0（厂商块）的 UID 是卡片**唯一标识**，普通写入命令**无法修改**（写保护）。某些"UID 可修改卡"（俗称"UID 卡"）是特殊工艺卡，**生产环境应禁用**以防伪造。

10. **块 3（控制块）写入需谨慎**：控制块包含 KeyA、Access 位、KeyB，**写错会导致卡片永久锁死**。生产环境写入控制块前应**强制确认**。

11. **防冲突机制**：如果天线范围内有**多张卡**，本实验 `MaxTg=1` 只会选中冲突解决后的第一张。**生产环境**如需处理多卡，应设置 `MaxTg=2` 或更高，并解析 `Tg` 字段遍历所有选中目标。

12. **NFC 与手机兼容性**：手机 NFC 模拟的卡片**不能被本实验读取**（本实验是 Reader，手机模拟的是 Card Emulation）。要测试手机 NFC，需要**反向开发**手机端 App，或使用支持 P2P 模式的 NFC 模块。

13. **应用场景适配**：
    - **门禁**：读 UID 比对白名单
    - **支付**：读特定扇区数据 + 加密验证
    - **资产标签**：写入资产编号 + 读出统计
    - **公交卡**：依赖 SAM 安全模块和后台结算
    - 本实验仅演示**最简读写**，未实现加密和认证

14. **测试间隔 30s 偏长**：主循环 `delay_ms(30000)` 用于给用户取走/放回卡片留时间，但**生产环境**应改用**中断唤醒 + 用户按键触发**，避免持续轮询浪费 CPU。

15. **测试块 2 注意事项**：本实验选 Mifare S50 **块 2**（扇区 0 的第 2 块），是**普通数据块**。**生产环境**应使用自定义扇区（如块 4~6）避免污染出厂数据。

16. **`PN532_PsdVerifyKeyA` 中的块号 0x03 写死**：当前代码**始终验证块 3（控制块）的 KeyA**——这是**扇区级验证**，验证后扇区 0 的所有块都可读可写。如果要操作其他扇区，**应验证对应扇区的控制块**（如扇区 1 = 块 7，扇区 2 = 块 11）。

17. **PN532 应答包 TFI = 0xD5**：本实验校验函数**未显式检查 TFI 字段**，仅验证 LCS 和 DCS。生产环境应增加 `if (RxBuf[LEN_ACK + INDEX_TFI] != 0xD5) return -1;` 防止误识别其他来源的数据。

18. **`PN532_CheckSum` 函数无输入长度参数**：当前实现从 `data[INDEX_LEN]`（第 4 字节）读取长度，**这意味着 data 数组必须已经构造好 LEN 字段**。调用前应确保 `data[3] = 长度` 已被正确赋值。生产环境可改为 `PN532_CheckSum(uint8_t *data, uint8_t len)` 接收长度参数。
