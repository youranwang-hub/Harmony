
#include "delay.h"


/***** 精确延时函数, ms / us *****/
void delay_us(uint16_t nus)
{
	uint16_t i;
	while(nus--)
	{
		i = 31;         //实测延时1us时的循环次数设定值
		while(i--){};
	}
}

void delay_ms(uint16_t nms)
{
	uint16_t i;
	while(nms--)
	{
		i = 33800;      //实测延时1ms时的循环次数设定值
		while(i--){};
	}
}
