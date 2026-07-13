#include "stm32f4xx.h"

u16 Adc = 0;     //ADC值
u16 Delay_Ms = 0; //延时时间(ms)

/* 函数声明 */
void delay_us(uint16_t nus);
void delay_ms(u16 nms);
void LED_Init(void);
void Adc_Init(void);
uint16_t Get_Adc(uint8_t ch);
uint16_t Get_Adc_Average(uint8_t ch, uint8_t times);


/***** 简单延时函数 *****/
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


/***** LED初始化 PB5 推挽输出 *****/
void LED_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE); //使能GPIOB时钟

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;              //PB5
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;           //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;       //速度100MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;           //推挽
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;            //上拉
	GPIO_Init(GPIOB, &GPIO_InitStructure);                   //初始化PB5

	GPIO_SetBits(GPIOB, GPIO_Pin_5); //初始熄灭（上拉时高电平灭）
}


/***** ADC单次转换 *****/
uint16_t Get_Adc(u8 ch)
{
	ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_480Cycles);
	ADC_SoftwareStartConv(ADC1);
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	return ADC_GetConversionValue(ADC1);
}


/***** ADC多次转换取平均 *****/
uint16_t Get_Adc_Average(uint8_t ch, uint8_t times)
{
	uint32_t temp_val = 0;
	uint8_t i;
	for(i = 0; i < times; i++)
	{
		temp_val += Get_Adc(ch);
		delay_ms(5);
	}
	return temp_val / times;
}


/***** ADC初始化 PC1 电位器输入 *****/
void Adc_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);    //使能GPIOC时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);     //使能ADC1时钟

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;              //PC1 电位器
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AN;            //模拟输入
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;        //无上下拉
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE);

	ADC_CommonInitStructure.ADC_Mode             = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInitStructure.ADC_DMAAccessMode    = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_Prescaler        = ADC_Prescaler_Div6; //84/6=14MHz
	ADC_CommonInit(&ADC_CommonInitStructure);

	ADC_InitStructure.ADC_Resolution          = ADC_Resolution_12b;       //12位
	ADC_InitStructure.ADC_ScanConvMode        = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode  = DISABLE;                  //单次转换
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_DataAlign           = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion     = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_Cmd(ADC1, ENABLE);
}


/*******************************
 * 主函数
 * ADC电位器控制LED1闪烁频率
 * 拧动电位器 → ADC值0~4095
 * → 映射到延时 100ms~2000ms
 * → LED亮/灭间隔随之变化
 *******************************/
int main(void)
{
	LED_Init();   //初始化LED(PB5)
	Adc_Init();   //初始化ADC(PC1)

	while(1)
	{
		/* 读取ADC平均值(通道11=PC1, 10次平均) */
		Adc = Get_Adc_Average(ADC_Channel_11, 10);

		/*
		 * ADC值(0~4095) → 延时(100~2000ms)
		 * Delay_Ms = 100 + Adc * 1900 / 4095
		 *
		 * 电位器最左(0V)   Adc≈0   → 延时≈100ms   闪烁最快(≈5Hz)
		 * 电位器中间       Adc≈2048→ 延时≈1050ms  适中
		 * 电位器最右(3.3V) Adc≈4095→ 延时≈2000ms  闪烁最慢(≈0.25Hz)
		 */
		Delay_Ms = 100 + (Adc * 1900) / 4095;

		/* 翻转LED: 亮→灭 或 灭→亮 */
		GPIO_ToggleBits(GPIOB, GPIO_Pin_5);

		/* ADC值决定的延时，即LED保持当前状态的时间 */
		delay_ms(Delay_Ms);
	}
}
