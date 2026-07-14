# 实验10 — GPS 定位实验

## 实验目的

- 理解 GPS 技术组成与原理
- 掌握 GPS 模块的配置使用方法
- 掌握 NMEA 0183 数据格式
- 掌握 STM32 串口编程方式
- 掌握 GPS 数据解析编程实现

## 实验概述

GPS 是当前手机和物联网常用的功能模块，是实现定位与导航的基础。AU100（青软遨游 100）开发板带有 **LC86L** 型号的 GPS 模块，本实验通过此模块实现了 GPS 定位数据的获取和数据解析。

通过本实验可深入掌握 GPS 数据标准格式、加强 STM32 的串口编程的应用，实验内容主要如下：

1. GPS 工程创建
2. GPS 数据解析
3. GPS 模块控制与测试

LC86L 是 **Quectel（移远通信）** 推出的多系统 GNSS 定位模块，支持 GPS / BeiDou / GLONASS / Galileo / QZSS 等多系统联合定位，默认输出 **NMEA 0183** 协议（标准 GPS 报文格式），通过 **UART 串口** 与 MCU 通讯。

## 硬件连接

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| GPS RX | PC11 | UART4 RX | MCU 接收 GPS 数据（GPS 模块 TX → MCU RX） |
| GPS TX | PC10 | UART4 TX | MCU 发送 GPS 命令（GPS 模块 RX ← MCU TX，本实验未使用） |
| GPS 电源 | - | 3.3V | LC86L 独立供电（板载已集成） |
| 调试串口 | PA2 / PA3 | USART2 | 115200 波特率，printf 输出 |

> **连接注意**：GPS 模块的 TX 接 MCU 的 RX，MCU 的 TX 接 GPS 的 RX，**收发交叉**。LC86L 默认 UART 速率为 **9600 bps**（与 PC 串口调试常见的 115200 不同），8N1 格式。

## 文件结构

```
实验10_GPS定位实验/
├── USER/
│   └── main.c                       # 主程序：测试数据 + 实际数据双路解析
├── BSP/
│   ├── gps.c           / .h         # GPS 串口驱动（UART4 + 接收缓冲 + IDLE 中断）
│   ├── gps_parser.c    / .h         # NMEA 0183 语句解析（GGA/RMC/GLL/GSA/GSV/VTG）
│   └── usart.c         / .h         # USART2 初始化 + printf/scanf 重定向
└── UTILS/
    └── delay.c                      # 软件循环延时（备用）
```

## GPS 技术与原理

### 1. GPS 系统组成

```
┌──────────────────────────────────────────────────────────────┐
│                       GPS 系统三大段                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  空间段 (Space Segment)                                      │
│  ├─ GPS: 31 颗 MEO 卫星，高度 20,200 km，周期 11h58m         │
│  ├─ BeiDou: 35 颗（5 GEO + 3 IGSO + 27 MEO）                │
│  ├─ GLONASS: 24 颗 MEO 卫星                                  │
│  └─ Galileo: 30 颗 MEO 卫星                                  │
│                                                              │
│  控制段 (Control Segment)                                    │
│  ├─ 主控站 (MCS)：美国科罗拉多                               │
│  ├─ 监测站：全球分布                                          │
│  └─ 注入站：向卫星发送星历数据                                │
│                                                              │
│  用户段 (User Segment)                                       │
│  └─ GPS 接收机（本实验 LC86L）：接收卫星信号并解算位置         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 2. 定位原理

**伪距测量法**（GPS 核心定位方法）：

```
1. 每颗 GPS 卫星持续广播自身星历（位置）和时间戳
2. 接收机在本地时间记录信号到达时间
3. 信号传播时间 × 光速 = 伪距（含时钟误差）
4. 4 颗以上卫星的伪距方程联立求解 → 经度/纬度/高度/时钟偏差
5. 多于 4 颗时，采用最小二乘法提高精度
```

**精度指标**：

| 精度因子 | 含义 | 良好值 |
|----------|------|--------|
| PDOP | 综合位置精度因子（含 3D） | < 2 |
| HDOP | 水平精度因子 | < 1 |
| VDOP | 垂直精度因子 | < 2 |
| SNR | 信噪比（dBHz） | > 40 |
| TTFF | 首次定位时间 | 冷启动 < 30s，热启动 < 1s |

### 3. LC86L 模块特性

| 参数 | 值 |
|------|-----|
| 定位系统 | GPS L1 + BeiDou B1 + GLONASS L1 + Galileo E1 + QZSS L1 |
| 定位精度 | < 2.5 m CEP（开阔天空） |
| 灵敏度 | 跟踪 -166 dBm / 捕获 -148 dBm / 冷启动 -148 dBm |
| 首次定位 | 冷启动 ≤ 35s / 热启动 ≤ 1s / 重新捕获 ≤ 1s |
| 更新速率 | 1Hz（默认）/ 最高 5Hz |
| 通讯接口 | UART 9600 bps（默认），8N1 |
| 协议 | NMEA 0183（默认）/ 自定义 |
| 工作电压 | 2.8 ~ 4.3V |
| 工作温度 | -40 ~ +85°C |
| 封装 | LCC 12.2×16.0×2.4 mm |

## NMEA 0183 数据格式

### 1. 语句结构

NMEA 0183 是 GPS 设备通用的**纯文本**通讯协议，每条语句结构如下：

```
$XXYYY,field1,field2,...,fieldN*hh<CR><LF>
│  │  │  │                        │  │    │
│  │  │  │                        │  │    └─ 结束符 (\r\n)
│  │  │  │                        │  └─ 校验和 (2字节16进制)
│  │  │  └─ 字段分隔符 (逗号)
│  │  └── 语句类型 (GGA, RMC, GLL, ...)
│  └───── 系统标识 (GP=GPS, GN=多系统, BD=BeiDou, GA=Galileo, GL=GLONASS)
└─────── 起始符 (美元符号)
```

**示例**：
```
$GNGGA,080237.000,3149.333190,N,11706.911552,E,2,15,0.74,53.489,M,-0.337,M,,*5F\r\n
```

### 2. 校验和算法

`*` 与 `\r\n` 之间的 2 字符是**校验和**，计算方法：

```c
uint8_t checksum = 0;
for (p = start; p < asterisk; p++) {
    checksum ^= *p;    // $ 与 * 之间所有字节的 XOR
}
// 输出为 2 位大写十六进制
```

本实验**未实现校验和验证**，应用层可根据需要补全——非校验语句说明数据传输异常，可丢弃。

### 3. 主要语句类型

| 语句 | 含义 | 主要字段 |
|------|------|----------|
| `$GNGGA` | GPS 定位数据 | 时间/经纬度/质量/卫星数/HDOP/海拔 |
| `$GNRMC` | 推荐最小定位信息 | 时间/状态/经纬度/速度/方位角/日期 |
| `$GNGLL` | 地理经纬度 | 经纬度/时间/状态 |
| `$GNGSA` | 当前卫星信息 | 模式/PRN 卫星列表/PDOP/HDOP/VDOP |
| `$GNGSV` | 可见卫星信息 | 卫星数/每颗卫星的 PRN/仰角/方位角/SNR |
| `$GNVTG` | 地面速度信息 | 航向/速度（节和 km/h） |

### 4. $GNGGA 详细字段

```
$GNGGA,080237.000,3149.333190,N,11706.911552,E,2,15,0.74,53.489,M,-0.337,M,,*5F
      │         │           │  │              │  │ │   │    │     │  │         │
      │         │           │  │              │  │ │   │    │     │  │         └─ 校验和 5F
      │         │           │  │              │  │ │   │    │     │  └─ 差分时间
      │         │           │  │              │  │ │   │    │     └─ M（米）
      │         │           │  │              │  │ │   │    └─ 大地水准面起伏 -0.337 m
      │         │           │  │              │  │ │   └─ 海拔 53.489 m
      │         │           │  │              │  │ └─ HDOP 0.74
      │         │           │  │              │  └─ 卫星数 15
      │         │           │  │              └─ 质量 2=差分GPS
      │         │           │  └─ 经度半球 E (东经)
      │         │           └─ 经度 117°06.911552'
      │         └─ 纬度半球 N (北纬)
      │           └─ 纬度 31°49.333190'
      └─ UTC 时间 08:02:37.000
```

**质量码含义**：

| 质量值 | 含义 |
|:------:|------|
| 0 | 定位无效 |
| 1 | GPS 单点定位 |
| 2 | 差分 GPS 定位 |
| 4 | RTK 固定解 |
| 5 | RTK 浮点解 |
| 6 | 估计（航位推算） |

### 5. $GNRMC 详细字段

```
$GNRMC,080237.000,A,3149.333190,N,11706.911552,E,0.00,5.78,221121,,,D,V*06
      │         │ │           │  │              │  │   │    │  │    │ │  │  │
      │         │ │           │  │              │  │   │    │  │    │ │  │  └─ 校验和 06
      │         │ │           │  │              │  │   │    │  │    │ │  └─ 导航状态 V
      │         │ │           │  │              │  │   │    │  │    │ └─ 模式 D
      │         │ │           │  │              │  │   │    │  │    └─ 磁偏角方向
      │         │ │           │  │              │  │   │    │  └─ 磁偏角 (空)
      │         │ │           │  │              │  │   │    └─ 日期 22-11-21
      │         │ │           │  │              │  │   └─ 方位角 5.78 度
      │         │ │           │  │              │  └─ 速度 0.00 节
      │         │ │           │  │              └─ 经度半球 E
      │         │ │           │  └─ 经度 117°06.911552'
      │         │ │           └─ 纬度半球 N
      │         │ └─ 纬度 31°49.333190'
      │         └─ 状态 A=有效 / V=无效
      └─ UTC 时间 08:02:37.000
```

**坐标转换**（NMEA ddmm.mmmm → 十进制度）：

```c
double lat_deg = (int)(lat / 100);              // 度
double lat_min = lat - lat_deg * 100;           // 分
double lat_decimal = lat_deg + lat_min / 60.0;  // 十进制度
if (lat_dir == 'S') lat_decimal = -lat_decimal; // 南纬取负
```

## 核心模块说明

### 1. UART4 初始化 (`GPS_Init`)

```c
void GPS_Init(uint32_t baud) {
    // GPIO: PC10 (TX), PC11 (RX) 复用为 UART4
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_UART4);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4);

    // UART: 9600 bps（LC86L 默认）, 8N1
    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    // 中断
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);   // 接收中断
    USART_ITConfig(UART4, USART_IT_IDLE, ENABLE);   // 空闲中断（关键！）

    // NVIC: UART4 中断, 抢占 1, 子 1
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
}
```

**两个关键中断**：
- `RXNE`（接收缓冲区非空）：每收到 1 字节触发一次
- `IDLE`（总线空闲）：一帧数据结束（线路空闲 ≥ 1 字符时间）触发

### 2. 接收中断 (`UART4_IRQHandler`)

```c
void UART4_IRQHandler(void) {
    if (USART_GetITStatus(UART4, USART_IT_RXNE) != RESET) {
        if (g_GPS_RxDataOK == 1) return;          // 上批未处理，丢弃新数据
        if (g_GPS_RxBufLen >= 500) return;        // 缓冲区满保护

        g_GPS_RxBuf[g_GPS_RxBufLen++] = USART_ReceiveData(UART4);
    }

    if (USART_GetITStatus(UART4, USART_IT_IDLE) == SET) {
        // 关键：清除 IDLE 中断的固定方法（先读 SR 再读 DR）
        UART4->SR;
        UART4->DR;
        g_GPS_RxDataOK = 1;     // 标记一帧接收完成
    }
}
```

**IDLE 中断清除技巧**：STM32F4 的 IDLE 中断**无法通过软件清零标志位**直接清除，必须按顺序读 `SR` 再读 `DR` 才能清除，否则会反复进入中断。

**缓冲区设计**：

```c
uint8_t g_GPS_RxBuf[500];         // 500 字节接收缓冲
volatile uint16_t g_GPS_RxBufLen;  // 当前接收长度
volatile uint8_t g_GPS_RxDataOK;   // 帧接收完成标志
```

**双缓冲设计**：
- 接收缓冲区与解析缓冲区共用，由 `g_GPS_RxDataOK` 互斥
- 接收中：填入缓冲区
- 解析中：标志位置 1，接收中断**拒绝新数据**（避免覆盖）
- 解析完：清空缓冲区，重置长度和标志

### 3. 解析流程 (`GPS_ReadAndParse`)

```c
void GPS_ReadAndParse(void) {
    if (g_GPS_RxDataOK != 1) return;     // 帧未就绪，退出

    // 1. 打印原始 NMEA 报文
    printf("================GPS DATA:\r\n");
    printf("%s", g_GPS_RxBuf);

    // 2. 依次解析每条 NMEA 语句（每条以 $ 开头，\n 结尾）
    pGpsStart = (char*)g_GPS_RxBuf;
    while (1) {
        pGpsStart = strpbrk(pGpsStart, "$");        // 找 $ 起始符
        if (pGpsStart == NULL) break;

        pGpsEnd = strpbrk(pGpsStart, "\n");        // 找换行结束符
        if (pGpsEnd == NULL) break;

        gps_parse(pGpsStart);                       // 解析单条语句
        pGpsStart = pGpsEnd + 1;                    // 移动到下一条
    }

    // 3. 解析完清空缓冲
    memset(g_GPS_RxBuf, 0, sizeof(g_GPS_RxBuf));
    g_GPS_RxBufLen = 0;
    g_GPS_RxDataOK = 0;
}
```

**一次接收可能含多条语句**（LC86L 1Hz 输出 6+ 条），循环解析所有。

### 4. NMEA 语句分发 (`gps_parse`)

```c
void gps_parse(char *gps_data) {
    if (strncmp(gps_data, ID_GGA, 5) == 0) gga_parse(&gga, gps_data),  gga_show(&gga);
    else if (strncmp(gps_data, ID_RMC, 5) == 0) rmc_parse(&rmc, gps_data), rmc_show(&rmc);
    else if (strncmp(gps_data, ID_GLL, 5) == 0) gll_parse(&gll, gps_data), gll_show(&gll);
    else if (strncmp(gps_data, ID_GSA, 5) == 0) gsa_parse(&gsa, gps_data), gsa_show(&gsa);
    else if (strncmp(gps_data, ID_GSV, 5) == 0) gsv_parse(&gsv, gps_data), gsv_show(&gsv);
    else if (strncmp(gps_data, ID_VTG, 5) == 0) vtg_parse(&vtg, gps_data), vtg_show(&vtg);
}
```

每种语句有独立的 `parse` + `show` 函数，支持 `ENABLE_XXX` 宏**按需启用**。

### 5. 字段分割算法（以 GGA 为例）

```c
void gga_parse(GGA *gga, char *gga_data) {
    char *p = gga_data;
    char *next;
    unsigned char times = 0;          // 字段序号

    memset(gga, 0, sizeof(GGA));

    while (p) {
        next = strpbrk(p, ",");       // 找下一个逗号
        if (next == p) {              // 字段为空（,,）跳过
            p = next + 1;
            times++;
            continue;
        }
        if (next == NULL) {           // 末尾：找 * 分隔校验和
            next = strpbrk(p, "*");
            if (next == NULL) break;
        }

        switch (times) {
            case 1: memcpy(gga->utc, p, next-p); break;     // 字符串字段用 memcpy
            case 2: gga->lat = strtod(p, NULL); break;      // 数字字段用 strtod
            case 3: gga->lat_dir = p[0]; break;             // 单字符直接取
            ...
        }

        p = next + 1;
        times++;
    }
}
```

**字段处理三大工具**：

| 函数 | 用途 | 适用字段 |
|------|------|----------|
| `memcpy(dst, p, len)` | 字符串复制 | UTC 时间、日期 |
| `strtod(p, NULL)` | 字符串转 double | 坐标、速度、高度 |
| `strtol(p, NULL, 10)` | 字符串转 long | 卫星数、PRN、SNR |
| `p[0]` | 取单个字符 | N/S、状态 A/V |

### 6. 数据结构（5 种语句 + 1 联合）

```c
typedef struct {
    char utc[11];
    double lat;
    char lat_dir;
    double lon;
    char lon_dir;
    unsigned char quality;
    unsigned char sats;
    double hdop;
    double alt;
    double undulation;
    unsigned char age;
    unsigned short stn_ID;
} GGA;

typedef struct { /* 13 字段：UTC, status, lat, dir, lon, dir, speed, track, date, mag_var, mag_dir, mode, nav_status */ } RMC;

typedef struct { /* 7 字段：lat, dir, lon, dir, utc, status */ } GLL;

typedef struct { /* 模式, 定位类型, prn[12], PDOP, HDOP, VDOP, sysid */ } GSA;

#pragma pack(1)
typedef struct { unsigned char prn, elev; unsigned short azimuth; unsigned char SNR; } SAT_INFO;
#pragma pack()

typedef struct { unsigned char msgs, msg, sats, sysid; SAT_INFO sat_info[36]; } GSV;

typedef struct { double track_true, track_mag, speed_Kn, speed_Km; char mode; } VTG;
```

**5 个全局实例**：

```c
GGA gga;
RMC rmc;
GLL gll;
GSA gsa;
GSV gsv;
VTG vtg;
```

### 7. 卫星信息特殊处理（GSV）

GSV 语句包含**多组卫星信息**（每条 GSV 最多 4 颗），且多系统下需**多条 GSV 联合表示**：

```c
gsv->sat_info[sat_index].prn      = strtol(p, NULL, 10);   // 卫星编号
gsv->sat_info[sat_index].elev     = strtol(p, NULL, 10);   // 仰角 0~90°
gsv->sat_info[sat_index].azimuth  = strtol(p, NULL, 10);   // 方位角 0~359°
gsv->sat_info[sat_index].SNR      = strtol(p, NULL, 10);   // 信噪比 dBHz

// 索引计算
sat_index = (gsv->msg - 1) * 4 + sat_count;  // 第 N 条 GSV 的第 M 颗卫星
```

**`#pragma pack(1)` 的作用**：紧凑对齐，避免 `unsigned short azimuth` 前后产生填充字节（4 字节 vs 6 字节），节省 RAM。

### 8. 主程序测试逻辑 (`main.c`)

```c
int main(void) {
    UART2_Init(115200);
    printf("=== GPS Experiment Start ===\r\n");
    GPS_Init(9600);
    printf("GPS Init Done, waiting data...\r\n");

    while (1) {
        GPS_ReadAndParse();
    }
}
```

**测试数据内置**（默认开启 `#if 1`）：

```c
char *gps_str = "$GNGGA,080237.000,3149.333190,N,11706.911552,E,2,15,0.74,53.489,M,-0.337,M,,*5F\r\n";
gps_parse(gps_str);
```

**应用场景**：
- **无 GPS 信号环境**（室内、屏蔽实验室）可启用测试段验证解析链路
- **有 GPS 信号**时改为 `#if 0` 关闭测试段，启用实际 UART 接收

**测试语句覆盖范围**（注释中预留 5 个其他语句）：

```c
//  char *gps_str = "$GNVTG,5.78,T,,M,0.00,N,0.00,K,D*2C\r\n";    // VTG
//  char *gps_str = "$GPGSV,4,1,13,194,72,074,43,26,61,222,45,31,61,352,43,32,60,116,47,1*5A\r\n";  // GSV
//  char *gps_str = "$GNGSA,A,3,22,29,26,25,03,32,31,16,194,193,,,1.35,0.74,1.13,1*0E\r\n";  // GSA
//  char *gps_str = "$GNGLL,3149.333190,N,11706.911552,E,080237.000,A,D*42\r\n";  // GLL
//  char *gps_str = "$GNRMC,080237.000,A,3149.333190,N,11706.911552,E,0.00,5.78,221121,,,D,V*06\r\n";  // RMC
```

## 运行效果

### 1. 实际 GPS 信号接收（户外测试）

```
=== GPS Experiment Start ===
GPS Init Done, waiting data...
================GPS DATA:
$GNGGA,080237.000,3149.333190,N,11706.911552,E,2,15,0.74,53.489,M,-0.337,M,,*5F
$GNRMC,080237.000,A,3149.333190,N,11706.911552,E,0.00,5.78,221121,,,D,V*06
$GNGLL,3149.333190,N,11706.911552,E,080237.000,A,D*42
...

================GGA DATA:
utc:080237.000
lat:3149.333190
lat_dir:N
lon:11706.911552
lon_dir:E
quality:2
sats:15
hdop:0.740000
alt:53.489000
undulation:-0.337000
age:0
stn_ID:0

================RMC DATA:
utc:080237.000
status:65        (A = 'A' = 0x41, 这里 status 输出为 ASCII 数值)
lat:3149.333190
lat_dir:N
lon:11706.911552
lon_dir:E
speed_Kn:0.000000
track_true:5.780000
date:221121
mag_dir:0.000000
mag_var_dir:0
mode:D
nav_status:V
...
```

### 2. 室内无信号（仅测试数据）

```
=== GPS Experiment Start ===
GPS Init Done, waiting data...
================GGA DATA:
utc:080237.000
lat:3149.333190
lat_dir:N
lon:11706.911552
lon_dir:E
quality:2
sats:15
...
```

## 关键技术点

| 技术 | 说明 |
|------|------|
| GPS 系统 | 空间段（卫星）+ 控制段（地面）+ 用户段（接收机） |
| 伪距定位 | 4 颗以上卫星伪距方程联立求解经纬度高度 |
| 多系统联合 | GPS + BeiDou + GLONASS + Galileo + QZSS，精度与卫星数都更高 |
| LC86L 模块 | Quectel 多系统 GNSS 接收机，NMEA 输出 |
| NMEA 0183 | `$XXYYY,fields*hh<CR><LF>` 文本协议 |
| 6 种语句 | GGA / RMC / GLL / GSA / GSV / VTG |
| 校验和 | `$` 与 `*` 之间所有字节的 XOR |
| 坐标格式 | ddmm.mmmm 需转换为十进制度（除 60 加度） |
| UART 通讯 | 9600 bps 8N1，与 MCU 串口交叉连接（TX→RX） |
| 双中断 | RXNE（接收）+ IDLE（空闲）联合判断一帧 |
| IDLE 清零 | 读 SR + 读 DR 固定序列 |
| 双缓冲 | `g_GPS_RxDataOK` 互斥保护，避免数据竞争 |
| 字符串解析 | `strpbrk` 按 `,` / `*` 分割 + `strtod` / `strtol` / `memcpy` |
| 字段类型处理 | 字符串用 memcpy，数字用 strtod/strtol，状态用 p[0] |
| 空字段处理 | `,,` 时 next==p，跳过 + 计数 |
| `#pragma pack(1)` | 紧凑对齐 SAT_INFO 结构体 |
| 测试数据 | 静态字符串模拟 GPS 输出，无信号环境也能验证解析 |
| printf 重定向 | `fputc` 映射到 USART2，方便调试坐标输出 |

## 代码注意事项

1. **GPS 串口速率 9600 不是 115200**：LC86L 默认 UART 速率 9600 bps。常见错误：误用 115200 初始化，接收数据全为乱码。**生产环境如需修改速率**，需向 GPS 模块发送 **PMTK 命令**（如 `$PMTK251,115200*1F\r\n`）。

2. **IDLE 中断清除必须读 SR+DR**：STM32F4 的 IDLE 标志位**不能通过 `USART_ClearITPendingBit` 清除**，必须**先读 `SR` 再读 `DR`**。这是 ST 硬件设计，应用层必须遵守。

3. **缓冲区溢出保护**：`g_GPS_RxBuf[500]` 是 500 字节，**单帧数据** 不会超过（实测 6 条语句约 250 字节），但**若 GPS 异常持续输出且未及时处理**可能溢出。代码中有 `if (g_GPS_RxBufLen >= 500) return;` 保护，溢出后**会丢弃剩余字节**直到下次 IDLE。

4. **`g_GPS_RxDataOK` 互斥**：接收中断中如果 `g_GPS_RxDataOK==1` 直接 `return`（不接收），**避免覆盖未解析的帧**。这是**单缓冲区**设计，**应用层必须保证解析速度 ≥ 接收速度**。生产环境应改用**双缓冲区乒乓**避免数据丢失。

5. **测试段默认开启**：本实验 main.c 中 `#if 1` 段每次上电都会执行一次**测试数据解析**，验证链路正常。在**真实 GPS 场景**下应改为 `#if 0` 关闭，否则会一直打印测试数据，掩盖真实 GPS 输出。

6. **校验和未验证**：代码**未实现 NMEA 校验和验证**，传输异常时仍会解析错误数据。**生产环境必须补全**：
   ```c
   uint8_t calc_checksum(char *str) {
       uint8_t chk = 0;
       for (char *p = str + 1; *p != '*'; p++) chk ^= *p;
       return chk;
   }
   ```

7. **坐标系 NMEA 与十进制转换**：NMEA 输出 `ddmm.mmmm` 格式（如 3149.333190 = 31°49.333190'），**代码解析后未转换为十进制度**。应用层如需在地图显示（如百度地图、高德地图），必须转换：
   ```c
   double lat_decimal = (int)(lat / 100) + (lat - (int)(lat/100)*100) / 60.0;
   if (lat_dir == 'S') lat_decimal = -lat_decimal;
   ```

8. **`status` 字段输出为 ASCII 数值**：`rmc_show` 中 `printf("status:%u\r\n", rmc->status);` 直接输出字符的整数值（如 'A' = 65, 'V' = 86）。**应改为 `%c`** 输出可读字符：
   ```c
   printf("status:%c\r\n", rmc->status);   // 修正后
   ```

9. **GSV 索引计算假设**：`sat_index = (gsv->msg-1) * 4 + sat_count` 假设每条 GSV 包含完整 4 颗卫星（满帧），**最后一条不满帧时也按 4 颗计算**。当 `sat_index >= gsv->sats` 时截断到 `gsv->sats - 1`，可能导致**多颗卫星写入同一索引**。**生产环境**应使用 `sat_count` 动态计算并对边界进行校验。

10. **室内无 GPS 信号**：LC86L 在室内、金属屏蔽或卫星数 < 4 时**无法定位**，quality=0。常见做法：
    - 检测 `quality > 0` 才使用坐标
    - 输出 `quality=0` 时保持上次有效坐标（抗漂移）
    - 或在 LCD 上提示"等待 GPS 信号"

11. **`ENABLE_XXX` 宏未做依赖检查**：头文件 `gps_parser.h` 中先定义宏（line 17~22），后定义依赖宏的结构体（line 30+），**结构体定义在宏之前**不会出错，但若把宏定义移到结构体定义**之后**会因 `gga_parse` 等函数前置声明而编译失败。**保持当前的"宏在前、结构体在后"顺序**。

12. **LC86L 模块供电**：LC86L 工作电压 2.8~4.3V，**不可用 5V**。AU100 板载已集成 3.3V 稳压器，但**外接独立电源**（如 USB 转接板）时务必确认电压。

13. **冷启动 TTFF 可能达 35 秒**：首次上电或长时间断电后，LC86L 需要下载星历数据（**冷启动**），TTFF 可能达 35 秒。**热启动**（关机 < 2 小时）通常 < 1 秒。开发板运行时建议**保持供电**避免冷启动。

14. **UART4 接收的双缓冲替代方案**：当前设计解析期间会丢失数据。**生产环境**推荐：
    - 接收 + IDLE 触发 DMA 双缓冲
    - 或使用**环形缓冲区**（ring buffer）暂存数据
    - 解析任务在另一线程/状态机中按需处理
