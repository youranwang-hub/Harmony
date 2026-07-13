#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "ir_recv.h"
#include "ir_send.h"

volatile uint32_t systick_ms = 0; //SysTick毫秒计数器


int main( void )
{
	uint32_t last_send_ms = 0;
	uint8_t  ir_sending   = 0;   //发送中标志，防止自干扰

	SysTick_Config(SystemCoreClock / 1000); //初始化Systick，1ms中断

	UART2_Init(115200);
	IR_Recv_Init();
	printf("IR Recv Init OK\r\n");
	IR_Send_Init();
	printf("IR Send Init OK\r\n");

	while(1)
	{
		//检测是否接收到数据（发送期间跳过，防止收到自己发出的信号）
		if(!ir_sending)
		{
			IR_Recv();
		}

		//每隔10秒发送一次
		if(systick_ms - last_send_ms >= 10000)
		{
			printf("IRSend:addr:1,code:123\r\n");
			ir_sending = 1;
			IR_Send(1, 123);
			ir_sending = 0;
			last_send_ms = systick_ms;
		}
	}
}