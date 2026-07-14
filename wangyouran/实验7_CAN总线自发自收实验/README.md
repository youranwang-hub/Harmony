# 实验7 — CAN 总线自发自收实验

## 实验目的

- 理解 CAN 总线特点与应用场景
- 掌握 CAN 总线控制协议
- 掌握 CAN 控制器功能与配置方法
- 掌握 CAN 总线 STM32 编程方法
- 掌握 CAN 总线通讯编程方法

## 实验概述

CAN 总线是工业环境应用最广泛的总线之一，具有良好的性能与高可靠性。AU100（青软遨游 100）开发板上带有 CAN 总线节点，本实验使用 CAN 总线的 **LoopBack 自发自收**功能，实现 CAN 节点的数据收发功能实验。

通过本实验可深入掌握 CAN 通讯协议的收发原理，掌握 CAN 总线的 STM32 编程方法。实验内容主要如下：

1. CAN 工程创建
2. CAN 收发示例实现

由于开发板上 CAN 收发器（SN65VD230 / TJA1050 等）需外挂，且实验仅验证 MCU 内置 bxCAN 控制器的协议层功能，因此采用 **LoopBack 模式**：发送的报文不经过总线收发器与外部节点，在 CAN 控制器内部直接回环至接收 FIFO，便于自测协议层逻辑。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| CAN1 TX | PA12 | CAN1 | CAN 发送引脚（LoopBack 模式不实际输出） |
| CAN1 RX | PA11 | CAN1 | CAN 接收引脚（LoopBack 模式不实际输入） |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

> **注意**：本实验使用 LoopBack 模式，PA11/PA12 在物理上**不连接到**外部 CAN 收发器。如需正常总线通讯，应在 PA11/PA12 外接 CAN 收发器（如 SN65VD230 / TJA1050）并将收发器 CANH/CANL 接入总线（带 120Ω 终端电阻）。

## 文件结构

```
实验7_CAN总线自发自收实验/
├── USER/
│   └── main.c                # 主程序：循环发送 CAN 消息（5s 周期）
├── BSP/
│   ├── can.c      / .h       # CAN1 初始化 + 过滤器配置 + 收发接口
│   └── usart.c    / .h       # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    └── delay.c               # 软件循环延时（main 中使用）
```

## CAN 协议简述

### 1. CAN 总线主要特点

| 特点 | 说明 |
|------|------|
| 多主控制 | 总线上**所有节点都可主动发送**报文，无主从之分 |
| 仲裁方式 | 非破坏性**逐位仲裁**，ID 越小优先级越高 |
| 标识符 | 11 位（标准帧）/ 29 位（扩展帧），同时充当报文优先级 |
| 总线速率 | 最高 1Mbps（40m 内），低速可至 5kbps（10km 远距离） |
| 错误检测 | CRC 校验、位填充、ACK 应答、格式检查等 5 种错误检测 |
| 节点数 | 理论无上限，受总线电容限制（通常 ≤ 110 个节点） |
| 报文长度 | 数据帧最多 **8 字节** |

### 2. CAN 典型应用场景

- **汽车电子**：动力总成、车身电子、底盘控制（CAN/CAN FD 已成为车载网络标准）
- **工业自动化**：PLC、伺服驱动、传感器网络（CANopen、DeviceNet 协议）
- **医疗设备**：监护仪、影像设备间数据交互
- **轨道交通 / 船舶**：车辆 / 船舶内部网络
- **机器人**：分布式电机驱动、关节控制

### 3. CAN 数据帧结构（标准帧）

```
┌──────┬─────────────┬──────────────┬─────────┬────────┬─────┬──────┬──────┐
│ SOF  │ Arbitration │   Control    │  Data   │  CRC   │ ACK │  EOF │  IFS │
│ 1bit │ 11+1+1=13bit │ 6bit         │ 0~64bit │ 16bit  │ 2bit│ 7bit │ 3bit │
└──────┴─────────────┴──────────────┴─────────┴────────┴─────┴──────┴──────┘
       └─ 标识符+SRR+IDE+RTR ─┘   └─ 数据负载 ┘
       决定优先级 + 帧类型         0~8 字节
```

> 本实验发送的是**扩展帧**（29 位 ID），格式为：SOF + Arbitration(29+SRR+IDE+RTR=32bit) + Control(6bit) + Data(0~64bit) + CRC(16bit) + ACK(2bit) + EOF(7bit) + IFS(3bit)。

### 4. CAN 帧类型

| 类型 | 用途 |
|------|------|
| 数据帧 | 携带 0~8 字节数据 |
| 远程帧 | 请求具有相同 ID 的节点发送数据 |
| 错误帧 | 任一节点检测到总线错误时发送 |
| 过载帧 | 接收节点尚未准备好时发送，用于延迟下一帧 |
| 帧间隔 | 用于分隔前后两个数据帧 / 远程帧 |

## 核心模块说明

### 1. CAN 初始化 (`CAN1_Init`)

#### GPIO 配置

```
PA11 (CAN1_RX) ──→ AF9  (CAN1), 上拉输入（实际由外设驱动）
PA12 (CAN1_TX) ──→ AF9  (CAN1), 推挽输出
```

```c
GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_11 | GPIO_Pin_12;
GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;        // 复用功能
GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;       // 推挽
GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;    // 外部收发器上下拉
GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
```

#### CAN 工作模式

```c
CAN_InitStruct.CAN_Mode = CAN_Mode_LoopBack;   // 自发自收
```

| 模式 | 说明 |
|------|------|
| `CAN_Mode_Normal` | 正常模式，连接外部收发器参与总线通讯 |
| `CAN_Mode_LoopBack` | **本实验采用**：发送帧内部回环到 RX，不发到总线 |
| `CAN_Mode_Silent` | 静默模式，只接收不发送 |
| `CAN_Mode_Silent_LoopBack` | 静默回环，自发自收但完全不影响总线 |

#### CAN 波特率计算

STM32F4 的 CAN 控制器挂在 **APB1** 总线，时钟频率 = 42MHz。

```
波特率 = fAPB1 / (Prescaler × (1 + BS1 + BS2))
        = 42MHz / (6 × (1 + 6 + 7))
        = 42MHz / (6 × 14)
        = 500 kbps
```

| 参数 | 设定值 | 含义 |
|------|--------|------|
| `CAN_Prescaler` | 6 | 分频系数（实际分频 = 6+1 = 7 × 由硬件实现） |
| `CAN_SJW` | 1 tq | 重新同步跳跃宽度，用于补偿节点间时钟偏差 |
| `CAN_BS1` | 6 tq | 位段 1（同步段 + 传播段 + 相位段 1） |
| `CAN_BS2` | 7 tq | 位段 2（相位段 2） |
| 采样点 | (1+6)/(1+6+7) ≈ **56.25%** | 位于位时间中后段 |

> **位时间分配**：1 + BS1 + BS2 = 14 tq，总线时间配额 1:6:7，采样点靠近 75%/50% 区间中部，是 CAN 推荐的稳健配置。

#### CAN 控制器行为配置

```c
CAN_InitStruct.CAN_TTCM = DISABLE;   // 非时间触发通信
CAN_InitStruct.CAN_ABOM = DISABLE;   // 软件自动离线管理（手动退出 Bus-Off）
CAN_InitStruct.CAN_AWUM = DISABLE;   // 软件唤醒（手动清除 SLEEP 位）
CAN_InitStruct.CAN_NART = ENABLE;    // 禁止报文自动重传
CAN_InitStruct.CAN_RFLM = DISABLE;   // 报文不锁定，新报文覆盖旧报文
CAN_InitStruct.CAN_TXFP = DISABLE;   // 发送优先级由报文标识符决定
```

| 参数 | 含义 | 选用值影响 |
|------|------|----------|
| `NART` | 发送失败时是否自动重传 | `ENABLE` 失败即丢弃，便于调试（LoopBack 模式下不会失败） |
| `RFLM` | FIFO 满时是否锁定 / 覆盖 | `DISABLE` 覆盖式接收 |
| `TXFP` | 发送优先级由 ID 决定还是 FIFO 顺序 | `DISABLE` 严格按 ID 仲裁（ID 越小越优先） |

#### FIFO0 接收中断

```c
CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);   // FIFO0 消息挂号中断
NVIC: CAN1_RX0_IRQn, 主优先级=1, 子优先级=0
```

STM32F4 的 bxCAN 提供 **3 个发送邮箱**和 **2 个接收 FIFO**（FIFO0、FIFO1）。本实验使用 FIFO0 接收，配置 FMP0（FIFO0 Message Pending）中断：每当 FIFO0 中有 1 个以上待处理报文时触发中断。

### 2. 验收过滤器配置 (`CAN1_FilterConfig`)

STM32F4 的 bxCAN 提供 **28 个过滤器**（0~13 在 CAN1，14~27 在 CAN2），用于自动过滤无关报文，减轻 CPU 负担。

```c
CAN_Filter.CAN_FilterNumber          = 0;     // 过滤器 0
CAN_Filter.CAN_FilterMode            = CAN_FilterMode_IdMask;   // 标识符 + 屏蔽模式
CAN_Filter.CAN_FilterScale           = CAN_FilterScale_32bit;   // 32 位宽
CAN_Filter.CAN_FilterIdHigh          = 0x0000; // 期望 ID 高 16 位
CAN_Filter.CAN_FilterIdLow           = 0x0000; // 期望 ID 低 16 位
CAN_Filter.CAN_FilterMaskIdHigh      = 0x0000; // 屏蔽位高 16 位
CAN_Filter.CAN_FilterMaskIdLow       = 0x0000; // 屏蔽位低 16 位
CAN_Filter.CAN_FilterFIFOAssignment  = CAN_Filter_FIFO0;
CAN_Filter.CAN_FilterActivation      = ENABLE;
```

| 模式 | 含义 | 本实验配置 |
|------|------|----------|
| 标识符屏蔽模式（IdMask） | ID + MASK 决定接收哪些报文 | ✅ 采用 |
| 标识符列表模式（IdList） | 精确匹配 2（16位）或 4（32位）个 ID | 未用 |

**过滤规则**：当 `ID & MASK == 期望ID & MASK` 时接收。期望 ID 和 MASK 都设为 `0x0000`，则任意报文 `X & 0 = 0` 都能通过——**即接收所有报文**。这是 LoopBack 自测模式下最方便的设置，确保自发的报文能够被自身接收。

### 3. 发送报文 (`CAN1_SendMsg`)

```c
void CAN1_SendMsg(uint32_t uID, uint8_t *pData, uint32_t uLen) {
    CanTxMsg TxMessage;

    TxMessage.StdId = uID;                    // 标准标识符（11位）
    TxMessage.ExtId = uID;                    // 扩展标识符（29位）
    TxMessage.IDE   = CAN_Id_Extended;        // 使用扩展帧
    TxMessage.RTR   = CAN_RTR_Data;           // 数据帧（非远程帧）
    TxMessage.DLC   = uLen;                   // 数据长度（0~8）

    for (i = 0; i < uLen; i++) {
        TxMessage.Data[i] = pData[i];
    }

    uMailBox = CAN_Transmit(CAN1, &TxMessage);                // 申请空邮箱发送
    while (CAN_TransmitStatus(CAN1, uMailBox) == CAN_TxStatus_Failed) {
        if (++i >= 0xFFF) { printf("Send Failed!\r\n"); return; }
    }
}
```

**bxCAN 发送机制**：
- bxCAN 提供 **3 个发送邮箱**（Mailbox 0/1/2），硬件自动调度空邮箱
- `CAN_Transmit()` 选择一个空邮箱装入报文并启动发送，返回邮箱号
- `CAN_TransmitStatus()` 查询发送结果：成功、失败、待定
- 由于本实验 `CAN_NART=ENABLE`，发送失败时**不会自动重传**，便于超时检测

**发送标识符与帧类型说明**：
- `IDE = CAN_Id_Extended` → 报文以**扩展帧**格式发送，ID 长度 29 位
- `RTR = CAN_RTR_Data` → 数据帧（远程帧 RTR=1 用于请求其他节点发送数据）
- `DLC` 范围 0~8，超过 8 时 bxCAN 硬件会**自动截断为 8 字节**

### 4. 接收中断 (`CAN1_RX0_IRQHandler`)

```c
void CAN1_RX0_IRQHandler(void) {
    CanRxMsg RxMessage;
    CAN_Receive(CAN1, 0, &RxMessage);          // 从 FIFO0 读取最早入队的报文

    if (RxMessage.IDE == CAN_Id_Standard)
        uCanID = RxMessage.StdId;
    else
        uCanID = RxMessage.ExtId;

    printf("Recv CANID:0x%08X Data:[", uCanID);
    for (i = 0; i < 8; i++) {
        printf("%02X ", RxMessage.Data[i]);
    }
    printf("]\r\n");
}
```

**关键点**：
- `CAN_Receive()` 会自动从 FIFO0 出队最旧报文，并释放对应邮箱（重要：必须调用，否则 FIFO 满后将不再接收新报文）
- 在 LoopBack 模式下，发送的报文会被自身立即回环接收，并触发 FIFO0 中断
- 中断处理中只做**打印**，避免在 ISR 中执行复杂操作

### 5. 主程序测试逻辑 (`main.c`)

```c
int main(void) {
    uint8_t pTxData[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

    UART2_Init(115200);
    CAN1_Init();

    while (1) {
        CAN1_SendMsg(0x1234, pTxData, 8);   // 发送扩展帧 ID=0x1234
        delay_ms(5000);                     // 5 秒后再次发送
    }
}
```

**测试设计要点**：
1. **固定测试数据**：8 字节降序数据 `0x88..0x11` 便于串口观察
2. **扩展帧 ID `0x1234`**：29 位 ID，验证扩展帧发送接收链路
3. **5 秒周期**：在串口助手可清晰观察发送与回环接收的成对输出
4. **LoopBack 自发自收**：无需外部节点，单板即可验证完整发送→接收链路

### 6. USART2 调试输出 (`usart.c`)

- `fputc()` 重定向 → `printf` 输出到 USART2
- `fgetc()` 重定向 → `scanf/getchar` 从 USART2 读取
- 串口接收中断回显：收到任意字节后原样发回，便于调试

## 运行效果

连接串口助手（115200, 8N1），上电后程序开始循环发送 CAN 报文（LoopBack 自发自收），输出形如：

```
Send CAN Data:[0x1234: 88 77 66 55 44 33 22 11 ]

Recv CANID:0x00001234 Data:[88 77 66 55 44 33 22 11 ]

Send CAN Data:[0x1234: 88 77 66 55 44 33 22 11 ]

Recv CANID:0x00001234 Data:[88 77 66 55 44 33 22 11 ]
... （每 5 秒重复一次）
```

**现象解读**：
- 发送的 `0x1234` 扩展帧 ID 与回环接收到的 `0x00001234` 一致
- 8 字节数据与发送时完全相同
- 串口输出 `Send...` 与 `Recv...` 成对出现，且时间间隔极短（回环延迟 < 1ms）
- 在外部接上 CAN 收发器时（切换为 `CAN_Mode_Normal`），两块 AU100 开发板可通过 CANH/CANL 双绞线互联，实现跨板收发

## 关键技术点

| 技术 | 说明 |
|------|------|
| CAN 协议 | 多主、ID 仲裁、CRC 校验、5 种错误检测机制 |
| bxCAN 外设 | STM32F4 内置 CAN 控制器，3 个发送邮箱 + 2 个接收 FIFO |
| LoopBack 模式 | 内部回环，绕过物理总线收发器，便于协议层自测 |
| 波特率计算 | 42MHz / (6 × 14) = **500 kbps**，采样点 ~56% |
| 验收过滤器 | 28 个过滤器（CAN1 独享 0~13），IdMask 32 位模式 |
| 扩展帧 | 29 位 ID，本实验使用 `0x1234` |
| 中断接收 | FIFO0 消息挂号中断 + 自动出队，CPU 零轮询 |
| NART 配置 | 禁用自动重传，超时主动报错便于调试 |
| 发送邮箱 | 3 个发送邮箱硬件调度，返回邮箱号查询状态 |
| 标识符编码 | 同一变量 `uID` 同时填入 StdId 和 ExtId，由 IDE 位决定使用哪一段 |
| 过滤器 ID/MASK | 期望 ID = 0, MASK = 0 → 接收所有报文（最宽松过滤） |
| printf 重定向 | `fputc` 映射到 USART2，方便日志输出 |

## 代码注意事项

1. **LoopBack 模式与外部收发器互斥**：本实验使用 `CAN_Mode_LoopBack`，报文在 bxCAN 内部回环，不经过 PA11/PA12 引脚，也不参与外部总线仲裁。如需多板通讯或外接 CAN 设备，必须将模式改为 `CAN_Mode_Normal`，并在 PA11/PA12 接 CAN 收发器（如 TJA1050），收发器 CANH/CANL 接入带 120Ω 终端电阻的双绞线。

2. **过滤器 ID/MASK 全 0 的含义**：`CAN_FilterId=0x0000, CAN_FilterMaskId=0x0000` 表示**接收任何报文**。在 LoopBack 模式下这是最简便的设置，但实际多节点总线中应**严格配置过滤范围**，避免无关报文占用 FIFO。

3. **必须调用 `CAN_Receive` 出队**：在接收中断中**必须**调用 `CAN_Receive(CAN1, 0, &RxMessage)`，否则 FIFO0 不会释放邮箱空间。当 FIFO0 装满（3 个）后，新到达的报文若 `RFLM=DISABLE` 将覆盖最旧的报文，`RFLM=ENABLE` 则直接丢弃——两种情况均会导致数据丢失或异常。

4. **`delay_ms` 使用软件循环**：`UTILS/delay.c` 中的 `delay_ms` 基于空循环实现，受编译优化级别和 CPU 频率影响，5000ms 实际可能略大。生产环境建议改用 SysTick 定时器或硬件定时器实现精确毫秒延时。

5. **发送 ID 双赋值冗余**：代码中 `TxMessage.StdId = uID; TxMessage.ExtId = uID;` 看似冗余，实为**兼容性写法**——`CAN_Transmit` 内部根据 `IDE` 位决定使用 StdId 还是 ExtId，因此两者都需正确赋值。

6. **`CAN_NART=ENABLE` 便于调试**：bxCAN 默认 `NART=DISABLE`，失败时会自动重传，导致问题难以复现。本实验设为 `ENABLE`，发送失败立即返回 `Failed`，便于排查总线配置错误。生产环境通常设为 `DISABLE`，保证数据可靠送达。

7. **`CAN_ABOM=DISABLE` 需手动恢复**：进入 Bus-Off 状态后（通常 11 位隐性错误计数器达到 255 时），如果 `ABOM=DISABLE` 则需要软件复位 CAN 外设或重新初始化。生产环境建议 `ABOM=ENABLE` 实现自动恢复。

8. **CAN 物理层特性**：CAN 总线两端必须接 **120Ω 终端电阻**（用于阻抗匹配，防止信号反射），双绞线长度与速率成反比——1Mbps 时建议 ≤ 40m，500kbps 时 ≤ 100m，10kbps 时最远可达 10km。
