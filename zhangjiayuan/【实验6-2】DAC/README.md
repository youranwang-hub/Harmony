# 实验6-2 DAC

本实验使用 STM32F407 的 DAC 通道 1 在 PA4 输出模拟电压。

## 文件说明

| 文件 | 功能 |
| --- | --- |
| `main.c` | DAC 输出值从 0 开始递增，每秒增加约 0.2 V 对应的数字量，超过 4095 后清零 |
| `6mainplus2.c` | 拓展实验，DAC 输出从 4095 递减到 0，分 18 个电压点循环输出 |

## 硬件连接

| 外设 | 引脚 | 说明 |
| --- | --- | --- |
| DAC_CH1 | PA4 | DAC 通道 1 模拟电压输出 |

## DAC 配置

两个源码都使用 DAC 通道 1：

1. 使能 GPIOA 时钟。
2. 使能 DAC 外设时钟。
3. 将 PA4 配置为模拟模式。
4. 设置 DAC 不使用外部触发。
5. 关闭噪声波和三角波发生器。
6. 使用 12 位右对齐数据格式 `DAC_Align_12b_R`。
7. 调用 `DAC_Cmd(DAC_Channel_1, ENABLE)` 使能 DAC。

## 基础程序逻辑

`main.c` 中 `DacValue` 初始为 0。主循环每秒执行一次：

```c
DAC_SetChannel1Data(DAC_Align_12b_R, DacValue);
DacValue += 248;
if (0xFFF < DacValue)
{
    DacValue = 0;
}
```

`DacValue` 每次增加 248，约对应 0.2 V 的输出电压变化。当数值超过 12 位最大值 4095 后，从 0 重新开始。

## 拓展程序逻辑

`6mainplus2.c` 中 `DacValue` 初始为 4095，`StepIndex` 表示当前递减级数。主循环中根据级数计算输出值：

```c
DacValue = 4095 - ((uint32_t)4095 * StepIndex / 17);
```

`StepIndex` 从 0 增加到 17，共 18 个输出电压点。输出到 0 V 对应值后，重新从 4095 开始。

拓展程序开启了 DAC 输出缓存：

```c
DAC_InitType.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
```

这样可以提高 PA4 输出驱动能力，使万用表测量结果更稳定。

## 运行现象

运行 `main.c` 时，用万用表测量 PA4，可看到输出电压从低到高逐级上升，超过最大值后回到低电压重新循环。

运行 `6mainplus2.c` 时，PA4 输出电压从接近 3.3 V 开始逐级下降到 0 V，然后重新循环。

## 使用方法

将需要运行的源码作为工程入口 `main.c`，编译下载后使用万用表测量 PA4 与 GND 之间的电压变化。

