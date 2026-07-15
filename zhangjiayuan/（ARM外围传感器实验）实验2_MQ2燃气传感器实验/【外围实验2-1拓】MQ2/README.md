# MQ2 燃气传感器拓展实验说明

本实验对应 `【外围实验2-1拓】MQ2`，代码位于 `代码` 文件夹中。拓展版在基础 ADC 采集上加入去极值平均滤波、预热、基线校准、相对变化计算和三级状态判断。

## 实验目的

- 在 MQ2 基础 ADC 采集上提高数据稳定性。
- 掌握去极值平均滤波和环境基线校准方法。
- 理解相对基线偏差百分比的含义。
- 使用状态机实现 `NORMAL`、`WARNING`、`DANGER` 三级状态判断。

## 硬件连接

| 模块 | 引脚/资源 | 说明 |
| --- | --- | --- |
| MQ2 AO | PC2 / ADC1_CH12 | 模拟输出接 ADC 输入 |
| MQ2 VCC/GND | 开发板电源/GND | 保证供电稳定并共地 |
| USART2_TX/RX | PA2 / PA3 | 串口显示预热、基线、状态和 ADC 数据 |

## 代码结构

```text
代码
├─BSP
│  ├─mq.c / mq.h             # ADC 读取、平均、基线校准、电压换算
│  └─usart.c / usart.h       # USART2 和 printf 重定向
├─SYSTEM
│  └─delay.c / delay.h       # 延时函数
└─USER
   └─main.c                  # 预热、状态判断、串口输出
```

## 核心流程

1. 初始化 USART2 和 MQ2 ADC。
2. 上电后进行 10 s 预热倒计时。
3. 在正常环境中采集 30 组滤波数据，每组间隔 50 ms。
4. 将采集结果平均为环境基线 `baselineValue`。
5. 正式检测时每次采样 10 次，去掉最大值和最小值后求平均。
6. 计算当前值相对基线的偏差百分比。
7. 根据 20% 和 50% 阈值判断 `NORMAL`、`WARNING`、`DANGER`。
8. 新状态连续出现 3 次后才更新稳定状态。

## 运行现象

串口先输出预热倒计时，再输出基线校准结果和阈值信息。正常状态下显示 `State=NORMAL`；施加气体扰动后 Change 百分比增大，状态可能切换为 `WARNING` 或 `DANGER`；停止扰动并通风后状态逐步恢复。

## 串口示例

```text
MQ2 Extended Test Start
Filter + Baseline + State Judge
MQ2 warming up...
Baseline calibration completed.
Baseline AD=0277, V=0.223V
WARNING threshold: 20%
DANGER threshold : 50%
State confirm count: 3
AD=0642, V=0.517V, Change=131%, State=DANGER
```

## 关键参数

| 参数 | 值 |
| --- | --- |
| 滤波采样次数 | 10 次 |
| 基线采集组数 | 30 组 |
| 基线采样间隔 | 50 ms |
| 预热时间 | 10 s |
| WARNING 阈值 | 20% |
| DANGER 阈值 | 50% |
| 状态确认次数 | 连续 3 次 |

## 注意事项

- Change 表示相对基线变化百分比，不是气体浓度百分比。
- 10 s 预热只适合课堂演示，实际 MQ2 应有更充分的预热和标定。
- 基线校准时应保持空气相对清洁，否则后续状态判断会失真。
- 当前算法使用绝对偏差，ADC 明显降低也可能触发状态变化。
