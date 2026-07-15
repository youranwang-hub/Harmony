# 实验6-1 ADC

本实验使用 ADC1 采集电位器模拟量，并将采样结果用于控制 LED1 的 PWM 亮度或闪烁间隔。

## 文件说明

| 文件 | 功能 |
| --- | --- |
| `main.c` | 采集 ADC1 通道 11，换算为 TIM3_CH2 的 PWM 比较值，控制 LED1 亮度 |
| `6mainplus1.c` | 拓展实验，采集 ADC1 通道 11，换算为 LED1 闪烁间隔 |

## 硬件连接

| 外设 | 引脚 | 说明 |
| --- | --- | --- |
| ADC1_CH11 | PC1 | 电位器模拟输入 |
| LED1 / TIM3_CH2 | PB5 | `main.c` 中复用为 PWM 输出 |
| LED1 | PB5 | `6mainplus1.c` 中作为普通 GPIO 输出 |

## ADC 配置

两个源码都使用 `Adc_Init()` 初始化 ADC1：

1. 使能 GPIOC 时钟。
2. 使能 ADC1 时钟。
3. 将 PC1 配置为模拟输入模式。
4. 复位 ADC1。
5. 配置 ADC 为独立模式。
6. 设置 ADC 分频为 `ADC_Prescaler_Div6`。
7. 设置 12 位分辨率。
8. 设置非扫描、非连续转换。
9. 软件触发转换。
10. 数据右对齐。

采样函数：

- `Get_Adc(ch)`：对指定通道进行一次 ADC 转换。
- `Get_Adc_Average(ch, times)`：连续采样多次并取平均值，代码中通常采样 10 次。

## 基础程序逻辑

`main.c` 同时初始化 TIM3_CH2 PWM：

1. PB5 复用为 TIM3_CH2。
2. TIM3 预分频值为 84。
3. 自动重装载值为 499。
4. PWM 模式为 `TIM_OCMode_PWM1`。
5. 输出极性为低电平有效。

主循环中：

```c
Adc = Get_Adc_Average(ADC_Channel_11, 10);
PWM_Value = 455 - Adc / 9;
TIM_SetCompare2(TIM3, PWM_Value);
```

也就是说，电位器输入变化会改变 ADC 采样值，进而改变 PWM 比较值和 LED1 亮度。

## 拓展程序逻辑

`6mainplus1.c` 不使用 PWM，而是将 PB5 配置为普通 GPIO 输出。主循环中：

```c
Adc = Get_Adc_Average(ADC_Channel_11, 10);
DelayTime = 100 + (uint32_t)Adc * 1900 / 4095;
GPIO_ToggleBits(GPIOB, GPIO_Pin_5);
delay_ms(DelayTime);
```

ADC 值 0 到 4095 被映射为 100 ms 到 2000 ms，电位器改变时，LED1 闪烁速度随之改变。

## 运行现象

运行 `main.c` 时，旋转电位器会改变 LED1 的 PWM 亮度。

运行 `6mainplus1.c` 时，旋转电位器会改变 LED1 的闪烁快慢。

## 使用方法

将需要运行的源码作为工程入口 `main.c`，编译下载后调节 PC1 对应电位器，观察 LED1 亮度或闪烁间隔变化。

