# 实验3-1 外部中断

本实验使用 EXTI 外部中断响应按键下降沿，通过中断服务函数修改 LED 流水灯速度。

## 文件说明

| 文件 | 功能 |
| --- | --- |
| `main.c` | KEY1 触发 EXTI0 中断，将流水灯间隔从 500 ms 改为 100 ms |
| `3mainplus1.c` | 拓展实验，KEY1 加快流水灯，KEY2 减慢流水灯 |

## 硬件连接

| 外设 | 引脚 | 中断线 | 功能 |
| --- | --- | --- | --- |
| KEY1 | PA0 | EXTI_Line0 | 下降沿触发，修改流水速度 |
| KEY2 | PC13 | EXTI_Line13 | 仅 `3mainplus1.c` 使用，下降沿触发 |
| LED1 | PB5 | - | 低电平点亮 |
| LED2 | PB0 | - | 低电平点亮 |
| LED3 | PB1 | - | 低电平点亮 |

## 程序逻辑

`main.c` 的主要流程：

1. 初始化 PB5、PB0、PB1 为 LED 输出。
2. 初始化 PA0 为上拉输入。
3. 使能 SYSCFG 时钟。
4. 使用 `SYSCFG_EXTILineConfig()` 将 PA0 映射到 EXTI_Line0。
5. 配置 EXTI_Line0 为下降沿中断。
6. 配置 NVIC，使能 `EXTI0_IRQn`。
7. 主循环按 LED1、LED2、LED3 的顺序执行流水灯。
8. 通过全局变量 `LedTime` 控制每个 LED 状态的保持时间。
9. 在 `EXTI0_IRQHandler()` 中清除中断标志，并把 `LedTime` 改为 100。

`3mainplus1.c` 在基础版上增加 KEY2：

- KEY1：PA0 -> EXTI_Line0 -> `EXTI0_IRQHandler()`，将 `LedTime` 改为 100 ms。
- KEY2：PC13 -> EXTI_Line13 -> `EXTI15_10_IRQHandler()`，将 `LedTime` 改为 1000 ms。

## 运行现象

基础程序运行后，三盏 LED 初始按 500 ms 间隔流水。按下 KEY1 后，中断服务函数修改 `LedTime`，流水速度明显变快。

拓展程序运行后，按 KEY1 可以加快流水灯，按 KEY2 可以减慢流水灯，LED 点亮顺序不变，只改变切换间隔。

## 使用方法

将 `main.c` 或 `3mainplus1.c` 作为工程入口文件编译下载。运行时通过按键触发外部中断，观察 LED 流水速度变化。

