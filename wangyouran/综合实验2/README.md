# 综合实验 — 智能物流配送终端

## 模拟场景

生鲜冷链运输车辆（教室演示版），由司机驾驶配送包裹至目的地，收货人通过遥控器确认签收。系统全程监控货箱温湿度，记录 GPS 定位信息，并在异常时发出声光报警。由于实验在室内进行，部分场景通过以下方式模拟：

| 真实场景 | 教室模拟方式 |
|---------|-------------|
| 司机指纹登录 | ZN632 指纹模块录入/识别 |
| 冷链箱温度超标 | 手指捏 DHT11，阈值故意设低（25°C）触发告警 |
| 车辆火灾预警 | 酒精棉片靠近 MQ2，ADC 飙升触发报警 |
| 车辆 GPS 轨迹 | 实验箱放窗边，接收真实 GPS 坐标 |
| 收货人签字确认 | 红外遥控器按键模拟签收 |
| 签收记录存档 | 写入 W25Q64 FLASH，串口导出查看 |

---

## 实验流程

```
上电 → 开机画面
  │
  ├── LCD 显示 "SmartLogi V1.0" + 司机名(从E2PROM读取) + 当前时间(RTC)
  │
  └── QCOM 主菜单(串口 115200)
        │
        ├── [1] 指纹登录 ──→ 录入新指纹 / 匹配验证身份
        ├── [2] GPS 定位  ──→ 打印实时经纬度，q 退出
        ├── [3] 环境监控 ──→ DHT11 温湿度 + MQ2 浓度循环上报
        │                    超温/高气 → 蜂鸣器响 + LED 闪烁
        │                    同时 LCD 实时刷新数值
        ├── [4] LCD 刷新 ──→ 全屏更新标题+司机+时间+日期
        ├── [5] IR 签收  ──→ 遥控器按键 → 蜂鸣器嘀 + FLASH 存记录
        ├── [6] FLASH 导出──→ 串口打印历史签收记录
        ├── [7] CAN 自测 ──→ LoopBack 收发一帧
        └── [0] 退出    ──→ LCD 显示 "System Off"
```

---

## 实验用模块

### 硬件资源

| 模块 | 芯片/外设 | 接口 | 实验来源 |
|------|----------|------|:---:|
| DHT11 温湿度 | DHT11 | 单总线 PE6 | 实验1 |
| MQ2 燃气 | MQ2 | ADC1 CH12 (PC2) | 实验2 |
| FLASH 存储 | W25Q64 | SPI1 (PA5/6/7, PC0) | 实验6 |
| E2PROM 存储 | AT24C02 | I2C1 (PB6/7) | 实验5 |
| GPS 定位 | LC86L | UART4 (PC10/11) @9600 | 实验10 |
| 指纹识别 | ZN632 | UART4 (PC10/11) @57600 | 实验11 |
| LCD 显示 | ILI9341 | FSMC (PD/PE 共21脚) | 实验8 |
| 触摸屏 | XPT2046 | 软件 SPI (PE0/2/3, PD13) | 实验9 |
| IR 红外接收 | VS1838 | TIM9 CH1 (PE5) | 实验3&4 |
| CAN 总线 | bxCAN | LoopBack (PA11/12) | 实验7 |
| RTC 时钟 | LSE 32.768kHz | PC14/15 | 实验8-1 |
| LED/蜂鸣器 | GPIO | PB0/1/5, PA8 | 实验2-1 |

> **注意**：GPS 与指纹共用 UART4，不可同时使用。主程序在切换功能时会重新初始化 UART4。

### 软件文件结构

```
综合实验/
├── USER/
│   └── main.c                    # 主程序 (菜单调度 + 开机画面)
├── BSP/
│   ├── dht11.c/h                 # DHT11 单总线驱动
│   ├── mq.c/h                    # MQ2 ADC 读取
│   ├── e2prom_at24c02.c/h        # AT24C02 I²C 驱动
│   ├── flash_w25qxx.c/h          # W25Q64 SPI 驱动
│   ├── can.c/h                   # bxCAN LoopBack 驱动
│   ├── gps.c/h + gps_parser.c/h  # GPS NMEA 解析
│   ├── fpr_zn632.c/h             # ZN632 指纹驱动
│   ├── lcd_ili9341.c/h           # ILI9341 FSMC 驱动
│   ├── lcd_fonts.c/h             # ASCII 8×16 字模
│   ├── xpt2046.c/h               # XPT2046 触摸驱动
│   ├── ir_recv.c/h + ir_send.c/h # IR NEC 收发
│   ├── rtc.c/h                   # RTC 日历初始化
│   ├── led_buzz.c/h              # LED/蜂鸣器 GPIO 封装
│   └── usart.c/h                 # USART2 printf/scanf 重定向
├── UTILS/
│   ├── delay.c/h                 # SysTick 精确延时
│   └── utils.c/h                 # 工具函数
└── README.md
```

---

## 运行方式

### 硬件准备

1. 青软遨游100 实验箱（STM32F407）
2. USB 转 TTL 模块接 USART2 (PA2/PA3) → PC QCOM
3. GPS 模块放窗边（冷启动约 35 秒）
4. Mifare 卡 / 遥控器 / 酒精棉片（可选，按需准备）

### Keil 配置

| 项目 | 设置 |
|------|------|
| Include Paths | 添加 `..\BSP` 和 `..\UTILS` |
| 源文件 | BSP 下 16 个 `.c` + UTILS 下 2 个 `.c` + USER 下 `main.c` |
| `stm32f4xx_conf.h` | 确保 CAN、FSMC、ADC、I2C、SPI 等宏未注释 |

### 串口工具

- QCOM / SSCOM：波特率 115200、8N1、无校验
- 输入菜单数字键交互

---

## 演示步骤（约 3 分钟）

```
步骤 1: 上电 → LCD 显示开机画面 + QCOM 打印菜单
步骤 2: 菜单输入 1 → 指纹登录识别
步骤 3: 菜单输入 2 → GPS 打印坐标（需放到窗边）
步骤 4: 菜单输入 3 → 手指捏 DHT11 看温湿度飙升 → 蜂鸣器报警
步骤 5: 酒精棉片晃 MQ2 → QCOM 打印 ALARM → 蜂鸣器急促
步骤 6: 菜单输入 5 → 遥控器按键 → QCOM 打印签收成功 → 蜂鸣器嘀
步骤 7: 菜单输入 6 → FLASH 导出签收记录
步骤 8: 菜单输入 7 → CAN 自测收发
```

---

## 关键设计

1. **UART4 分时复用**：GPS (9600) 和指纹 (57600) 共享 PC10/PC11。`menu_gps()` 和 `menu_fingerprint()` 各自调用 `GPS_Init` / `FPR_Init` 重新配置波特率
2. **IDLE 中断**：仅 GPS 使用，用于判定 NMEA 帧边界；FPR 使用主动轮询 `ZN632_CheckAck`，不依赖中断
3. **FLASH 追加写入**：IR 签收回调中用 `static flash_offset` 累加写入偏移，避免覆盖历史记录
4. **E2PROM 存储司机名**：首次上电写入默认名 "Wang"，后续从 E2PROM 读出，断电不丢失
5. **弱函数 `IR_Rece_Proc`**：由 `ir_recv.c` 以 `__weak` 声明，`main.c` 中重写，实现签收记录写入 FLASH
