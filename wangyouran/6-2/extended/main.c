#include "stm32f4xx.h"

u16 DacValue = 4095;//定义DAC的输出值，从最大值开始

/* 定义所需结构体*/
GPIO_InitTypeDef GPIO_InitStructure;//定义 GPIO 初始化结构体
DAC_InitTypeDef DAC_InitStructure; //定义 DAC 初始化结构体

/*函数声明*/
void delay_us(uint16_t nus);
void delay_ms(u16 nms);

/***** 简单延时函数,ms,us *****/
void delay_us(uint16_t nus)
{
	uint16_t i;
	while(nus--)
	{
		i = 31;
		while(i--);
	}
}

void delay_ms(u16 nms)
{
	uint16_t i;
	while(nms--)
	{
		i = 33800;
		while(i--);
	}
}

int main(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);//开启GPIOA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);//开启DAC时钟

	/* 配置PA4为DAC的输出端 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; //选择 GPIOA 的 Pin4
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN; //模拟模式（输出）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; //速度100MHz
	GPIO_Init(GPIOA, &GPIO_InitStructure); //按照上述参数初始化GPIOA

	/*配置DAC的输出参数*/
	DAC_InitStructure.DAC_Trigger = DAC_Trigger_None; //不使用触发功能
	DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None; //不用波形发生器
	DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;//屏蔽幅值
	DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable; //关闭DAC1输出缓存
	DAC_Init(DAC_Channel_1, &DAC_InitStructure); //初始化DAC通道1

	DAC_Cmd(DAC_Channel_1, ENABLE); //使能DAC通道1

	DAC_SetChannel1Data(DAC_Align_12b_R, 4095); //12位右对齐数据格式，初始输出最大电压

	while(1)
	{
		DAC_SetChannel1Data(DAC_Align_12b_R, DacValue);//设置DAC输出电压（PA4）

		/*
		 * 递减：每次减248，输出电压约降低0.2V
		 * 先判断是否够减，避免u16下溢出
		 * 范围始终在 0 ~ 4095 之间
		 */
		if(DacValue >= 248)
			DacValue -= 248;            //够减则正常递减
		else
			DacValue = 4095;            //不够减则回到最大值，重新开始

		delay_ms(1000);//每次延时1秒
	}
}
