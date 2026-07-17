#include <stdio.h>
#include "stm32f4xx.h"
#include "mq.h"
#include "usart.h"
#include "delay.h"
/**
* @brief 主函数
* @param 无
* @retval 无
*/
int main(void){
uint16_t uValue = 0;
float fVolValue = 0.0;

UART2_Init( 115200 );
MQ_Init();
printf("MQ Init OK\r\n");

while(1){
uValue = MQ_ReadValue();//读取ADC值
fVolValue = (float)uValue/4096*(float)3.3;//转换为电压值
printf("AD=%04d, %fV\r\n", uValue, fVolValue);

delay_ms( 1000 );
}
}
