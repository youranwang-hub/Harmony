#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "delay.h"
#include "can.h"

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    uint8_t i = 0;
    uint8_t frame_type = 0;

    uint8_t pStdData[8] = {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88
    };

    uint8_t pExtData[8] = {
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11
    };

    UART2_Init(115200);
    CAN1_Init();

    printf("\r\nCAN LoopBack Test\r\n");
    printf("STD ID:0x123, EXT ID:0x1234\r\n");
    printf("Input 1/3/5 to set interval 1s/3s/5s\r\n");
    printf("Data will increase after each transmission\r\n\r\n");

    while(1)
    {
        if(frame_type == 0)
        {
            //发送标准帧
            CAN1_SendStdMsg(0x123, pStdData, 8);

            //标准帧数据全部加1
            for(i = 0; i < 8; i++)
            {
                pStdData[i]++;
            }

            frame_type = 1;
        }
        else
        {
            //发送扩展帧
            CAN1_SendMsg(0x1234, pExtData, 8);

            //扩展帧数据全部加1
            for(i = 0; i < 8; i++)
            {
                pExtData[i]++;
            }

            frame_type = 0;
        }

        delay_ms(g_can_send_interval);
    }
}
