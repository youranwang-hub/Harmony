#include <stdio.h>
#include "stm32f4xx.h"
#include "delay.h" // 简单延时

#define IR_RECV_TIM TIM9

// 遥控器接收状态
// Bit[7]: 收到了引导码标志
// Bit[6]: 得到了一个按键的所有信息
// Bit[5]: 保留
// Bit[4]: 标记是否收到第一个上升沿
uint8_t IRState = 0;
#define IR_STATE_LEAD_OK 0x80 // 标记是否已接收到前导码
#define IR_STATE_DATA_OK 0x40 // 标记此键值是否已接收完
#define IR_STATE_UP_OK   0x10 // 标记是否收到第一个上升沿

uint32_t CapValue   = 0;  // 捕获信号时的时长
uint32_t IRCount    = 0;  // 按键按下的次数
uint32_t IRData     = 0;  // 红外接收到的完整数据
uint8_t  IRBitCount = 0;  // 已接收的数据位数（0~32），用于防stop bit误移位

void IR_GPIO_Init(void);
void IR_TIM_Init(void);


void IR_Recv_Init(void)
{
	IR_GPIO_Init();
	IR_TIM_Init();
}

void IR_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE); // 使能时钟

	// PE5
	GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOE, GPIO_PinSource5, GPIO_AF_TIM9);
}

void IR_TIM_Init(void)
{
	NVIC_InitTypeDef NVIC_InitStruct;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
	TIM_ICInitTypeDef TIM_ICInitStruct;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE); // TIM时钟使能

	TIM_TimeBaseStruct.TIM_Period        = 10000; // 自动装载值, 周期10ms
	TIM_TimeBaseStruct.TIM_Prescaler     = 167;   // 预分频器, 1M的计数频率, 1us加1
	TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1; // 设置时钟分割
	TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up; // TIM向上计数模式
	TIM_TimeBaseInit(IR_RECV_TIM, &TIM_TimeBaseStruct);

	TIM_ICInitStruct.TIM_Channel     = TIM_Channel_1;
	TIM_ICInitStruct.TIM_ICPolarity  = TIM_ICPolarity_Rising;
	TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1; // 配置输入分频, 不分频
	TIM_ICInitStruct.TIM_ICFilter    = 0x03;           // IC1F=0003 8个定时器时钟周期滤波
	TIM_ICInit(IR_RECV_TIM, &TIM_ICInitStruct); // 初始化定时器输入捕获通道

	TIM_ITConfig(TIM9, TIM_IT_Update | TIM_IT_CC1, ENABLE); // 允许更新中断
	TIM_Cmd(IR_RECV_TIM, ENABLE); // 使能定时器

	NVIC_InitStruct.NVIC_IRQChannel                   = TIM1_BRK_TIM9_IRQn; // TIM中断
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1; // 先占优先级1级
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 3; // 从优先级3级
	NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE; // IRQ通道被使能
	NVIC_Init(&NVIC_InitStruct);
}


// 定时器输入捕获中断回调函数
void TIM1_BRK_TIM9_IRQHandler(void)
{
	uint32_t cap; // 捕获值临时变量，防止减法下溢

	// 一个IR信号位由560us的脉冲低电平+间隔高电平组成，
	// 只需捕获前后两个上升沿，之间即为完整一个IR信号位，减去560us脉冲时长，即为间隔高电平时长
	if(TIM_GetITStatus(IR_RECV_TIM, TIM_IT_CC1) != RESET)
	{
		TIM_ClearITPendingBit(IR_RECV_TIM, TIM_IT_CC1);
		TIM_SetCounter(IR_RECV_TIM, 0); // 清空定时器值

		if(!(IRState & IR_STATE_UP_OK)) // 第一次上升沿只表示一组新信号开始，后续开始计时
		{
			IRState |= IR_STATE_UP_OK; // 标记
			return;
		}

		// 两次上升沿的间隔时间-560us脉冲即为高电平时长
		cap = TIM_GetCapture1(IR_RECV_TIM);
		if(cap < 560) // 防止毛刺导致减法下溢
		{
			return;
		}
		CapValue = cap - 560;

		// 根据高电平时长做不同判断
		if(CapValue > 300 && CapValue < 800) // 数据位0，标准值560us
		{
			if(!(IRState & IR_STATE_LEAD_OK)) return;  // 无引导码，忽略
			if(IRBitCount >= 32) return;                // 已收满32位，忽略stop bit

			IRData >>= 1; // 右移一位, NEC为LSB先发
			IRBitCount++;
		}
		else if(CapValue > 1400 && CapValue < 1800) // 数据位1，标准值为1680us
		{
			if(!(IRState & IR_STATE_LEAD_OK)) return;  // 无引导码，忽略
			if(IRBitCount >= 32) return;                // 已收满32位，忽略stop bit

			IRData >>= 1; // 右移一位, NEC为LSB先发
			IRData |= 0x80000000; // 接收到1
			IRBitCount++;
		}
		else if(CapValue > 2200 && CapValue < 2600) // 重复码间隔，标准值2.25ms
		{
			IRCount++; // 按键次数增加1次
		}
		else if(CapValue > 4200 && CapValue < 5000) // 4500为标准值4.5ms
		{
			IRData = 0;
			IRBitCount = 0;  // 重置位计数器
			IRState |= IR_STATE_LEAD_OK; // 标记成功接收到了引导码
			IRCount = 0; // 清除按键次数计数器
		}

		// 收满32位数据后立即标记完成，无需等待溢出超时
		if(IRBitCount >= 32 && (IRState & IR_STATE_LEAD_OK))
		{
			IRState &= ~IR_STATE_LEAD_OK; // 清除引导码标记，防止溢出中断误触发
			IRState |= IR_STATE_DATA_OK;  // 标记已经完成一次按键采集
		}
	}


	// Timer溢出中断，用于：1)帧接收超时/丢包恢复  2)初始化时等待第一个信号
	if(TIM_GetITStatus(IR_RECV_TIM, TIM_IT_Update) != RESET) // 超时溢出中断
	{
		TIM_ClearITPendingBit(IR_RECV_TIM, TIM_IT_Update);

		if(IRState & IR_STATE_LEAD_OK) // 有引导码但未收满32位，帧异常超时
		{
			IRState &= ~IR_STATE_UP_OK;   // 清除上升沿标记
			IRState &= ~IR_STATE_LEAD_OK; // 清除引导码标记
			IRBitCount = 0;               // 重置位计数器
		}
	}
}



// 实际键码处理函数，由应用程序可重写替换
__attribute__((weak)) void IR_Rece_Proc(uint16_t addr, uint8_t code)
{
	printf("IRRecv:addr:%d, code:%d\r\n", addr, code);
}

// 处理红外任务
void IR_Recv(void)
{
	uint16_t addr = 0; // 地址码
	uint8_t byte1, byte2, byte3, byte4 = 0;

	if(!(IRState & IR_STATE_DATA_OK)) // 数据接收OK
	{
		return;
	}

	IRState &= ~IR_STATE_DATA_OK; // 清除接收到有效按键标识

	byte1 = IRData;       // 地址码
	byte2 = IRData >> 8;  // 地址反码
	byte3 = IRData >> 16; // 键码
	byte4 = IRData >> 24; // 键码反码
	IRData = 0;

	// 检验遥控键码
	if(byte3 != (uint8_t)~byte4)
	{
		return;
	}

	// 两地址码若不是反码则为扩展协议，两字节组合为地址码
	if(byte1 != (uint8_t)~byte2)
	{
		addr = byte1 | ((uint16_t)byte2 << 8); // 取两字节组合为16位地址码
	}
	else
	{
		addr = byte1;
	}

	IR_Rece_Proc(addr, byte3);
}
