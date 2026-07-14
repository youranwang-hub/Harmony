# 实验11 — FPR 指纹识别实验

## 实验目的

- 理解指纹识别的技术原理与应用
- 掌握 ZN632 模块功能与交互协议
- 掌握 ZN632 指纹识别概念与流程
- 掌握 STM32 串口编程方法
- 掌握 ZN632 驱动编程方法
- 实现 ZN632 指纹录入与匹配功能

## 实验概述

指纹识别是常用的功能部件，在门禁、支付等多领域都有广泛应用。AU100（青软遨游 100）开发板带有 **ZN632 指纹识别模块**，本实验通过此模块实现了指纹录入与指纹匹配的功能。

通过本实验可深入学习 STM32 串口编程方法，掌握指纹模块工作原理及流程，掌握 ZN632 模块的命令交互和控制方法。实验内容主要如下：

1. FPR 工程创建
2. FPR 串口通讯实现
3. FPR 基础命令实现
4. FPR 功能流程实现
5. FPR 测试示例实现

ZN632 是一款**光学指纹识别模块**，内置高性能 DSP 处理器和 FLASH 存储器，支持指纹图像采集、特征提取、模板生成、1:1 比对、1:N 搜索等完整流程。MCU 通过 **UART（默认 57600 bps）** 与其通讯，使用**自定义命令协议**进行交互。模块内部可存储 **240 枚**指纹模板，掉电不丢失。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| 指纹模块 TX | PC11 | UART4 RX | MCU 接收 ZN632 响应数据 |
| 指纹模块 RX | PC10 | UART4 TX | MCU 发送命令给 ZN632 |
| FPR_PWR | PC9 | GPIO 推挽输出 | **电源控制**：高电平断电，低电平通电 |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

> **电源控制设计**：PC9 串联 P-MOSFET 或直接控制模块电源，**低电平导通**给 ZN632 上电。**高电平 100ms 后再拉低**可实现一次硬复位（上电启动流程）。

## 文件结构

```
实验11_FPR指纹识别实验/
├── USER/
│   └── main.c                       # 主程序：菜单交互（1录入 2匹配）
├── BSP/
│   ├── fpr_zn632.c    / .h          # ZN632 驱动（UART + 命令协议 + 录入匹配流程）
│   └── usart.c        / .h          # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    └── delay.c                      # 软件循环延时
```

## 指纹识别原理

### 1. 指纹识别系统组成

```
┌──────────────────────────────────────────────────────────────┐
│                  指纹识别系统组成                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  光学传感器（如 ZN632）                                        │
│  ├─ LED 光源 → 照射指尖                                        │
│  ├─ 棱镜 → 反射光线                                           │
│  ├─ CMOS 图像传感器 → 采集指纹图像                              │
│  └─ 图像缓冲区（ImageBuffer）                                  │
│       │                                                      │
│       ▼                                                      │
│  DSP 处理器（模块内置）                                        │
│  ├─ 图像预处理（增强、滤波、二值化）                             │
│  ├─ 特征点提取（minutiae）：端点、分叉点                        │
│  ├─ 特征文件生成（GenChar）                                    │
│  └─ 特征比对（Match/Search）                                   │
│       │                                                      │
│       ▼                                                      │
│  FLASH 指纹库                                                 │
│  ├─ 存储 240 枚指纹模板                                        │
│  └─ 索引表（IndexTable）记录已使用编号                          │
│                                                              │
│  通讯接口（UART 57600 bps）                                    │
│  └─ MCU ↔ ZN632 命令交互                                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 2. 指纹特征（Minutiae）

指纹的"特征"是指纹纹路中具有**唯一性和稳定性**的局部细节：

```
        ╱  ╲
       ╱    ╲
      ╱  ⊙   ╲     ⊙ 端点（ridge ending）
     ╱   ⊕    ╲    ⊕ 分叉点（bifurcation）
    ╱          ╲   △ 交叉点（crossover）
   ╱            ╲
```

**常见特征类型**：
- **端点（Ending）**：纹路终止点
- **分叉点（Bifurcation）**：纹路分叉处
- **分歧点（Ridge Divergence）**：纹路分叉
- **孤立点（Dot）**：极短的纹路
- **环形（Enclosure）**：纹路分叉后重新合并

每个人的指纹约有 60-100 个特征点，**比较 12-15 个特征点即可唯一确定身份**。

### 3. 指纹识别流程

```
                      录入流程                            匹配流程
                ┌──────────────┐                  ┌──────────────┐
                │  按下手指 #1  │                  │   按下手指    │
                └──────┬───────┘                  └──────┬───────┘
                       ▼                                  ▼
                ┌──────────────┐                  ┌──────────────┐
                │  GetImage    │                  │  GetImage    │
                │  (采集图像)    │                  │  (采集图像)    │
                └──────┬───────┘                  └──────┬───────┘
                       ▼                                  ▼
                ┌──────────────┐                  ┌──────────────┐
                │  GenChar(1)  │                  │  GenChar(1)  │
                │  (生成特征1)   │                  │  (生成特征)    │
                └──────┬───────┘                  └──────┬───────┘
                       ▼                                  ▼
                ┌──────────────┐                  ┌──────────────┐
                │  按下手指 #2  │                  │ HighSpeedSearch│
                │  (同指再按一次) │                  │ (1:N 搜索)    │
                └──────┬───────┘                  └──────┬───────┘
                       ▼                                  ▼
                ┌──────────────┐                  ┌──────────────┐
                │  GenChar(2)  │                  │  返回匹配ID    │
                │  (生成特征2)   │                  │  + 匹配分数    │
                └──────┬───────┘                  └──────────────┘
                       ▼
                ┌──────────────┐
                │  RegModel    │
                │  (合并特征)    │
                └──────┬───────┘
                       ▼
                ┌──────────────┐
                │  StoreChar   │
                │  (存储模板)    │
                └──────┬───────┘
                       ▼
                ┌──────────────┐
                │  SetIndex    │
                │  (标记已用)    │
                └──────────────┘
```

### 4. 光学指纹传感器特性

| 参数 | 值 |
|------|-----|
| 图像分辨率 | 256 × 288 像素（部分型号 192×192） |
| 图像灰度 | 8 bit（256 级灰度） |
| 特征文件大小 | 256 字节（生成后存于 CharBuffer1/2） |
| 模板大小 | 512 字节（合并后存于 Flash） |
| 误识率 (FAR) | < 0.001% |
| 拒识率 (FRR) | < 0.1% |
| 比对时间 | < 0.3 秒 |
| 容量 | 240 枚（部分型号 1000+） |

## ZN632 通讯协议

### 1. 命令包格式（MCU → ZN632）

```
┌────────┬────────────┬────────────┬─────────┬──────────┬──────────┐
│ 包头   │ 地址        │ 包标识     │ 包长度   │ 命令码+参数│ 校验和   │
│ 2字节  │ 4字节       │ 1字节      │ 2字节   │ N字节     │ 2字节    │
│ EF 01  │ XX XX XX XX │ 0x01        │ LEN_H LEN_L│ ...     │ SUM_H SUM_L│
└────────┴────────────┴────────────┴─────────┴──────────┴──────────┘
```

| 字段 | 长度 | 说明 |
|------|------|------|
| 包头 | 2 | 固定 `0xEF 0x01` |
| 地址 | 4 | 模块地址，默认 `0xFFFFFFFF`（广播） |
| 包标识 | 1 | `0x01` = 命令包 |
| 包长度 | 2 | 包标识+包长度+命令码+参数 的总字节数（不含包头、地址、校验和） |
| 命令码 | 1 | 见命令码表 |
| 参数 | N | 命令相关参数 |
| 校验和 | 2 | 包标识+包长度+命令码+参数 的**累加和低 16 位** |

### 2. 应答包格式（ZN632 → MCU）

```
┌────────┬────────────┬────────────┬─────────┬──────────┬──────────┐
│ 包头   │ 地址        │ 包标识     │ 包长度   │ 确认码+参数│ 校验和   │
│ 2字节  │ 4字节       │ 1字节      │ 2字节   │ N字节     │ 2字节    │
│ EF 01  │ XX XX XX XX │ 0x07        │ LEN_H LEN_L│ ...     │ SUM_H SUM_L│
└────────┴────────────┴────────────┴─────────┴──────────┴──────────┘
```

| 字段 | 长度 | 说明 |
|------|------|------|
| 包头 | 2 | 固定 `0xEF 0x01` |
| 地址 | 4 | 应答的模块地址 |
| 包标识 | 1 | `0x07` = 应答包 |
| 包长度 | 2 | 包含包标识+包长度+确认码+参数 |
| 确认码 | 1 | `0x00`=成功，其他见错误码表 |
| 应答参数 | N | 命令相关的返回数据 |
| 校验和 | 2 | 累加和低 16 位 |

### 3. 关键命令码

| 命令码 | 名称 | 参数 | 应答 | 含义 |
|:-----:|------|------|------|------|
| `0x01` | GetImage | 无 | 确认码 | 探测手指并采集图像到 ImageBuffer |
| `0x02` | GenChar | BufferID | 确认码 | 从 ImageBuffer 生成特征到 CharBuffer1/2 |
| `0x03` | Match | 无 | 确认码+分数 | CharBuffer1 与 CharBuffer2 精确比对 |
| `0x04` | Search | BufferID, Start, Num | 确认码+PageID+分数 | 1:N 搜索指纹库 |
| `0x05` | RegModel | 无 | 确认码 | 合并 CharBuffer1+2 → 模板 |
| `0x06` | StoreChar | BufferID, PageID | 确认码 | 存储模板到 Flash |
| `0x07` | LoadChar | BufferID, PageID | 确认码 | 从 Flash 加载模板到 CharBuffer |
| `0x0C` | DeletChar | PageID, N | 确认码 | 删除 N 个模板（从 PageID 起） |
| `0x0D` | Empty | 无 | 确认码 | 清空指纹库 |
| `0x0F` | ReadSysPara | 无 | 确认码+参数 | 读取模块系统参数 |
| `0x13` | VryPwd | Password(4B) | 确认码 | 验证模块密码 |
| `0x1B` | HighSpeedSearch | BufferID, Start, Num | 确认码+PageID+分数 | 高速 1:N 搜索 |
| `0x1F` | ReadIndexTable | Page(0-3) | 确认码+32B 位图 | 读取已注册指纹索引表 |

### 4. 关键确认码

| 确认码 | 含义 |
|:-----:|------|
| `0x00` | 成功 |
| `0x01` | 数据包接收错误 |
| `0x02` | 传感器无手指 |
| `0x03` | 录入指纹图像失败 |
| `0x04` | 图像太干太淡，无法生成特征 |
| `0x05` | 图像太湿太糊，无法生成特征 |
| `0x06` | 图像太乱，无法生成特征 |
| `0x07` | 特征点太少，无法生成特征 |
| `0x08` | 指纹不匹配 |
| `0x09` | 指纹库中无此指纹 |
| `0x0A` | 特征合并失败（两枚指纹不属于同一手指） |
| `0x0B` | PageID 超出指纹库范围 |
| `0x18` | 写 FLASH 出错 |
| `0x1F` | 指纹库已满 |

## 核心模块说明

### 1. ZN632 上电启动 (`ZN632_PowerOn`)

```c
int16_t ZN632_PowerOn(void) {
    GPIO_SetBits(GPIOC, GPIO_Pin_9);    // 1. 先拉高（断电）
    delay_ms(100);
    GPIO_ResetBits(GPIOC, GPIO_Pin_9);  // 2. 拉低（上电）
    
    // 3. 轮询等待模块发送 0x55 启动应答
    while (1) {
        if (RXNE) {
            if (ch == 0x55) return 0;   // 启动成功
        }
        if (i++ > 1000) return -1;     // 1 秒超时
        delay_ms(1);
    }
}
```

**上电时序**：

```
PC9  ──────┐     ┌───────────────────────
           └─────┘
           ↑ 100ms ↑        ↑
          断电→通电        等待 0x55 应答
```

### 2. 串口初始化 (`ZN632_UART_Init`)

```c
USART_InitStructure.USART_BaudRate = 57600;   // ZN632 默认速率
USART_InitStructure.USART_WordLength = USART_WordLength_8b;
USART_InitStructure.USART_StopBits = USART_StopBits_1;
USART_InitStructure.USART_Parity = USART_Parity_No;

USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);  // 接收中断
```

**与 GPS 共享 UART4**：本实验的 ZN632 也使用 **UART4（PC10/PC11）**——与实验10 GPS 占用同一组引脚！**实际使用同一开发板时不可同时上电**，需通过 PC9 控制 GPS / 指纹模块的电源切换。

### 3. 命令发送（通用模式）

所有命令都遵循相同的发送结构：

```c
int16_t ZN632_VryPwd(void) {
    uint8_t cmd[10] = {0};
    cmd[0] = 0x01;                   // 包标识
    cmd[1] = 0x00; cmd[2] = 0x07;    // 包长度 = 7
    cmd[3] = CMD_VryPwd;             // 命令码
    cmd[4-7] = 0x00000000;           // 密码（默认 0）
    
    // 校验和 = cmd[0..7] 累加
    uint16_t temp = cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6] + cmd[7];
    cmd[8] = temp >> 8;   // 高字节
    cmd[9] = temp & 0xFF; // 低字节
    
    ZN632_SendHead();              // 发送 0xEF 0x01 + 地址 (6 字节)
    ZN632_SendData(cmd, 10);       // 发送命令体 (10 字节)
    delay_ms(300);                  // 等待模块处理
    return ZN632_CheckAck();        // 接收并验证应答
}
```

**协议封装流程**：

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ ZN632_SendHead│ → │ ZN632_SendData│ → │ ZN632_CheckAck│
│  EF 01 + ADDR │    │   CMD + PARM  │    │  RECV + VERIFY│
│   (6 字节)    │    │   (N 字节)     │    │   (验证应答)   │
└─────────────┘    └─────────────┘    └─────────────┘
     包头封装          命令体封装          应答验证
```

### 4. 应答验证 (`ZN632_CheckAck`)

```c
int16_t ZN632_CheckAck(void) {
    // 1. 轮询接收 500ms
    for (i = 0; i < 500; i++) {
        if (RXNE) ZN632_RxBuf[ZN632_RxBufLen++] = USART_ReceiveData();
        delay_ms(1);
    }
    
    // 2. 包头检查
    if (RxBuf[0] != 0xEF || RxBuf[1] != 0x01) return -1;
    
    // 3. 地址检查（4 字节）
    if (RxBuf[2..5] != ZN632_ADDR[0..3]) return -1;
    
    // 4. 校验和检查
    len = RxBuf[7] << 8 | RxBuf[8];
    sum_calc = 0;
    for (i = 0; i <= len; i++) sum_calc += RxBuf[6 + i];  // 包标识到参数末尾
    sum_recv = RxBuf[9+len-2] << 8 | RxBuf[9+len-1];       // 最后 2 字节
    if (sum_calc != sum_recv) return -1;
    
    return RxBuf[9];   // 返回确认码
}
```

**关键技巧**：
- 500ms 轮询等待既保证响应时间又避免永久阻塞
- **校验和算法 = 包标识+包长度+命令码+参数** 的累加和（不含包头、地址、校验和本身）
- 返回 0x00 表示成功

### 5. 索引表管理

模块内 240 枚指纹库对应一个 **240 bit** 的索引表，用 32 字节表示：

```c
uint8_t ZN632_INDEX[32] = {0};   // 240 bit，每 bit 代表一个编号是否已用

// 设置索引：标记第 uIndex 个位置已用
void ZN632_SetIndex(uint16_t uIndex) {
    uint8_t uByte = uIndex / 8;       // 字节号
    uint8_t uBit = uIndex % 8;        // 位号
    ZN632_INDEX[uByte] |= (0x1 << uBit);
}

// 清除索引：标记第 uIndex 个位置为空
void ZN632_UnsetIndex(uint16_t uIndex) {
    ZN632_INDEX[uByte] &= ~(0x1 << uBit);
}

// 获取第一个空索引
uint16_t ZN632_GetIndexEmpty(void) {
    for (i = 0; i < 32; i++) {         // 32 字节
        for (j = 0; j < 8; j++) {       // 8 位
            if ((ZN632_INDEX[i] & (0x1 << j)) == 0)
                return i * 8 + j;       // 找到空位，返回全局编号
        }
    }
    return ZN632_INDEX_MAX;             // 全部已用，返回 240
}
```

**位图技巧**：用位图（bitmap）记录哪些位置已用，**32 字节即可表示 240 状态**，节省 95% 内存。

### 6. 录入流程 (`FPR_AddFinger`)

```c
void FPR_AddFinger(void) {
    uint8_t uStep = 1;
    
    // 第 1 阶段：采集 2 次手指图像
    while (uStep <= 2) {
        printf("Put Finger On Sensor:%d\r\n", uStep);
        
        ret = FPR_GetImage();                  // 1. 采图（阻塞等待手指）
        if (ret != 0) return;
        
        ret = ZN632_GenChar(uStep);            // 2. 生成特征到 Buffer1/2
        if (ret != 0) continue;
        
        ret = ZN632_HighSpeedSearch(uStep, &uID, &uScore);  // 3. 检查是否已注册
        if (ret == 0) {
            printf("Finger is Already registered\r\n");
            continue;                          // 已注册则重新采集
        }
        
        uStep++;
    }
    
    // 第 2 阶段：合并特征 + 存储
    ret = ZN632_RegModel();                    // 4. 合并 Buffer1+2 → 模板
    if (ret != 0) return;
    
    uID = ZN632_GetIndexEmpty();               // 5. 找空位置
    ret = ZN632_StoreChar(2, uID);              // 6. 存储模板到 Flash
    if (ret != 0) return;
    
    ZN632_SetIndex(uID);                       // 7. 标记本地索引
    printf("Add Finger OK, ID:%d\r\n", uID);
}
```

**两次采集的意义**：
- 防止单次图像**质量差**（手指干燥、出汗、按压力度不均）导致特征提取失败
- 两次特征合并后**模板更鲁棒**，误识率显著降低
- 合并时会校验是否同一手指（`0x0A` = 特征合并失败）

### 7. 匹配流程 (`FPR_MatchFinger`)

```c
void FPR_MatchFinger(void) {
    uint16_t uID = 0, uScore = 0;
    
    printf("Put Finger On Sensor:");
    if (FPR_GetImage() != 0) return;          // 1. 采图
    
    if (ZN632_GenChar(1) != 0) return;        // 2. 生成特征到 Buffer1
    
    if (ZN632_HighSpeedSearch(1, &uID, &uScore) != 0) {  // 3. 1:N 高速搜索
        printf("Finger NOT Match\r\n");
        return;
    }
    
    printf("Finger Match, ID:%d\r\n", uID);
}
```

**高速搜索 vs 普通搜索**：
- `Search (0x04)`：内部完整比对，较慢
- `HighSpeedSearch (0x1B)`：模块内部优化算法，**速度快 3-5 倍**

### 8. 主程序菜单交互 (`main.c`)

```c
int main(void) {
    char ch = 0;
    
    UART2_Init(115200);
    FPR_Init(57600);          // 1. 初始化 ZN632（自动上电）
    
    if (ZN632_VryPwd() == 0) {        // 2. 验证密码
        printf("Password OK\r\n");
    }
    if (ZN632_ReadIndexTable() == 0) { // 3. 读取索引表
        printf("ReadIndexTable OK\r\n");
    }
    
    while (1) {
        FPR_Usage();             // 打印菜单
        ch = getchar();          // 等待用户输入
        switch (ch) {
            case '1': FPR_AddFinger();  break;   // 录入
            case '2': FPR_MatchFinger(); break;   // 匹配
            default:  printf("No this number\r\n");
        }
    }
}
```

**菜单**：

```
Select one
1 Add New Finger
2 Match Finger
```

### 9. 错误信息显示 (`ZN632_ShowError`)

```c
void ZN632_ShowError(uint16_t code) {
    switch (code) {
        case 0x00: printf("OK\r\n"); break;
        case 0x02: printf("指纹模块没有检测到指纹！\r\n"); break;
        case 0x09: printf("对比指纹失败，指纹库不存在此指纹！\r\n"); break;
        case 0x0A: printf("特征合并失败\r\n"); break;
        case 0x1F: printf("指纹库满\r\n"); break;
        // ... 共 20+ 种错误码
    }
}
```

## 运行效果

### 1. 录入流程

```
Password OK
ReadIndexTable OK
Select one
1 Add New Finger
2 Match Finger
1
Put Finger On Sensor:1
GetImage ret=0
Get One Finger
Put Finger On Sensor:2
GetImage ret=0
Get One Finger
Add Finger OK, ID:0
```

### 2. 匹配流程

```
Select one
1 Add New Finger
2 Match Finger
2
Put Finger On Sensor: GetImage ret=0
Finger Match, ID:0
```

### 3. 错误处理示例

```
Select one
2
Put Finger On Sensor: GetImage ret=2
指纹模块没有检测到指纹！   ← 错误码 0x02
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| 指纹识别原理 | 特征点（端点+分叉点）提取 + 1:N 比对 |
| 光学传感器 | LED + 棱镜 + CMOS 采集灰度图像 |
| 特征文件 | 256 字节，存于模块 RAM（Buffer1/2） |
| 模板文件 | 512 字节，存于模块 FLASH（掉电不丢失） |
| ZN632 协议 | 自定义包格式：包头+地址+标识+长度+参数+校验 |
| 包头 | `0xEF 0x01` 固定魔数 |
| 校验和 | 包标识+包长度+命令码+参数的累加和低 16 位 |
| 包标识 | `0x01` 命令包 / `0x07` 应答包 |
| 1:1 比对 | Match (0x03) 两次指纹精确比对 |
| 1:N 搜索 | HighSpeedSearch (0x1B) 高速搜索整个指纹库 |
| 索引表 | 240 bit 位图，32 字节表示已用编号 |
| 两次采集 | 提高模板鲁棒性，校验是否同一手指 |
| 模板合并 | RegModel (0x05) 合并两次特征为最终模板 |
| 容量管理 | 240 枚，0x1F 错误码表示指纹库满 |
| 电源控制 | PC9 拉低上电 + 等待 0x55 启动应答 |
| UART 中断 | RXNE 中断接收 + 500ms 轮询等待响应 |
| 应答验证 | 检查包头 + 地址 + 校验和 + 返回确认码 |
| 错误码表 | 20+ 种错误码对应不同失败原因 |
| printf 重定向 | `fputc` 映射到 USART2，方便调试输出 |

## 代码注意事项

1. **UART4 引脚冲突 GPS 模块**：ZN632 与实验10 的 GPS 模块**共用 UART4 (PC10/PC11)**。同一开发板上**两个模块不能同时上电**——本实验通过 PC9 控制 ZN632 电源，实际生产中需增加多路电源切换或为不同模块分配不同 UART。

2. **校验和计算遗漏包头与地址**：协议规定的校验和是 `包标识+包长度+命令码+参数` 的累加和，**不包含包头 `0xEF 0x01` 和 4 字节地址**。代码中：

   ```c
   // cmd[0]=0x01(包标识) cmd[1..2]=长度 cmd[3]=命令码 cmd[4..N]=参数
   temp = cmd[0] + cmd[1] + cmd[2] + cmd[3] + ...;   // 正确
   ```

   如果错把包头 `0xEF 0x01` 也加入校验和，**会与模块端算法不一致**，导致应答验证失败 `sum != temp`。

3. **校验和的字节顺序**：校验和 2 字节以**大端（高字节在前）**发送：

   ```c
   cmd[8] = temp >> 8;          // 高字节在前
   cmd[9] = temp & 0xFF;        // 低字节在后
   ```

   与应答包的解析一致：

   ```c
   sum = ZN632_RxBuf[9+len-2] << 8 | ZN632_RxBuf[9+len-1];
   ```

4. **响应延迟时间**：模块处理不同命令的耗时不同：
   - `GetImage` / `GenChar` / `Match` ≈ 100-300ms
   - `RegModel` ≈ 500ms
   - `StoreChar` / `Search` ≈ 300ms
   - `Empty`（清空） ≈ 1-2s

   代码中统一用 `delay_ms(300)` 等待，**对慢操作可能不够**，会出现"命令发送成功但应答还未到达"。**生产环境**应改用**模块就绪引脚（ACK 输出）中断**或**自适应超时**。

5. **`ZN632_CheckAck` 的 500ms 阻塞轮询**：每次命令后都阻塞 500ms 接收响应，**MCU 在此期间无法处理其他任务**。生产环境应改用：
   - **双缓冲环形队列**暂存接收数据
   - **IDLE 中断**判断一帧结束（与实验10 GPS 相同）
   - **状态机**在主循环中处理

6. **`FPR_GetImage` 阻塞特性**：`FPR_GetImage` 内部是 `while(1)` 循环，**没有手指时会一直循环发送 GetImage 命令**，占用 CPU。可通过 `g_uExitToChange` 全局标志位在外部中断（如按键）触发时**强制退出**——本实验已预留接口但**未实现触发机制**。

7. **两次采集中途已注册检测**：`FPR_AddFinger` 在每次 `GenChar` 后都调用 `HighSpeedSearch` 检查"是否已注册"——防止用户重复录入同一手指浪费指纹库空间。但**生产环境**可能希望允许**覆盖式更新**指纹模板（用同一手指的新模板替换旧模板），需要根据需求调整。

8. **索引表同步问题**：`ZN632_INDEX[]` 是**本地内存缓存**，**实际指纹库的更新不会自动反映到此数组**。当前代码仅在 `ZN632_ReadIndexTable()` 时同步——如果模块内部被其他途径修改（如外部工具烧录），本地索引会不一致。**生产环境**应在每次录入/删除后立即重新读取索引表。

9. **指纹库容量边界**：`ZN632_INDEX_MAX = 240` 是模块实际容量，**但 `ZN632_INDEX[32]` 位图能表示 256 位**（多 16 位未用）。`ZN632_GetIndexEmpty` 找空位时如果遍历到第 240 位已用，会返回 240（已满），**但 240~255 位的索引仍可能被错误使用**——实际上 `ZN632_StoreChar(2, uID)` 时模块端会返回 `0x0B` 错误码。

10. **指纹库满错误码 `0x1F`**：当模块内 240 枚全部用完后，**任何 StoreChar 命令都会返回 `0x1F`**。代码中虽有错误码显示，但**没有自动删除最旧模板**的回退策略。生产环境应：
    - 录入前先检查索引表是否已满
    - 已满时询问用户是否替换最旧模板
    - 或自动循环覆盖（先进先出）

11. **指纹合并失败 (`0x0A`)**：两次按压若被识别为不同手指，会返回 `0x0A`。常见原因：
    - 同一手指按压力度/角度差异过大
    - 用户两次按的是不同手指
    - 第二次按压时手指滑动严重
    - 提示用户重新录入

12. **密码保护默认关闭**：`ZN632_VryPwd` 传入的密码为 `0x00000000`（4 字节 0），这是模块**默认密码**。如果用户通过其他工具设置了密码，**所有命令会返回 `0x13` 错误码**。生产环境如启用密码保护，应先保存密码到 Flash。

13. **`Z N632_Match` 和 `ZN632_DeletChar` 未实现**：头文件声明了这两个函数，但 `.c` 文件中**未提供实现**：
    - `ZN632_Match` (0x03)：1:1 精确比对（CharBuffer1 vs CharBuffer2）
    - `ZN632_DeletChar` (0x0C)：删除 N 个模板
    - `ZN632_Empty` (0x0D)：清空指纹库

    如需使用（删除单个指纹、清空指纹库），需自行实现。**特别注意 `ZN632_Empty` 会清空所有指纹，操作前应二次确认**。

14. **特征合并 vs 搜索误用**：`ZN632_RegModel` 合并两个 CharBuffer，**不会自动更新索引表**——需要在 `StoreChar` 后手动调用 `ZN632_SetIndex`。代码中已有此逻辑，但**操作顺序错误会导致索引表与实际指纹库不同步**。

15. **`UART4_IRQHandler` 仅做数据缓存**：与 GPS 不同，ZN632 的接收中断**不做任何协议解析**，仅把数据写入 `ZN632_RxBuf[]`。所有解析逻辑在 `ZN632_CheckAck` 中**主动轮询完成**。这种设计**简单但效率低**——`ZN632_CheckAck` 在已有数据可读时会重复读 RXNE 标志位，浪费 CPU。

16. **`printf` 阻塞**：主循环中 `getchar()` 是阻塞的，意味着**录入过程中无法响应匹配请求**。生产环境应改用**RTOS 多任务**或**状态机轮询**，将录入和匹配作为独立任务。
