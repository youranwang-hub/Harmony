#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "ir_recv.h"

int main(void)
{
    /* 初始化UART2，波特率115200 */
    UART2_Init(115200);

    /* 初始化红外接收模块 */
    IR_Recv_Init();

    printf("IRRecv Init OK\r\n");

    while(1)
    {
        /* 循环解析已经完整接收的红外数据 */
        IR_Recv();
    }
}
