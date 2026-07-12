/* 4-1 拓展版 main.c - 串口指令独立控制每个LED的开关 */
#include "stm32f4xx.h"

/* 变量定义区 */
u8 OD_Flag = 0;
u8 Rx_Frame_Flag = 0;
u8 Rx_Buf[64];
u16 Rx_Con;
u16 Rx_Len;
u8 Error = 0;

/* 结构体变量定义 */
GPIO_InitTypeDef GPIO_InitStructure;
USART_InitTypeDef USART_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;

/* 函数声明 */
void USART2_Send_Frame(u8* data, u16 len);
void delay_ms(u16 nms);

/******** UART2 帧发送函数 ********/
void USART2_Send_Frame(u8* data, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, data[i]);
    }
}

/***** 毫秒延时函数 *****/
void delay_ms(u16 nms)
{
    uint16_t i;
    while (nms--)
    {
        i = 33800;
        while (i--);
    }
}

/*** 串口2中断处理函数 ***/
void USART2_IRQHandler(void)
{
    u8 res = 0;

    if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE))
    {
        res = USART_ReceiveData(USART2);

        if (OD_Flag)
        {
            if (res == 0x0A)
            {
                Rx_Frame_Flag = 1;
                Rx_Len = Rx_Con;
                Rx_Con = 0;
                OD_Flag = 0;
            }
        }
        else
        {
            if (res == 0x0D)
            {
                OD_Flag = 1;
            }
            else
            {
                if (Rx_Con < 63)
                {
                    Rx_Buf[Rx_Con] = res;
                    Rx_Con++;
                }
            }
        }
    }
}

int main(void)
{
    /*** 配置三个LED ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /*** 配置蜂鸣器 ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_SetBits(GPIOA, GPIO_Pin_8);
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*** 配置USART2 ***/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    USART_Cmd(USART2, ENABLE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART2_Send_Frame("Ready!\r\n指令: L1+/L1- L2+/L2- L3+/L3- AL+/AL- BEEP\r\n", 52);

    while (1)
    {
        if (Rx_Frame_Flag)
        {
            Rx_Frame_Flag = 0;

            if (4 == Rx_Len)  /* 4字节指令: L1+ / L1- / BEEP 等 */
            {
                if (('B' == Rx_Buf[0]) && ('E' == Rx_Buf[1]) &&
                    ('E' == Rx_Buf[2]) && ('P' == Rx_Buf[3]))
                {
                    /* BEEP: 蜂鸣器响一声 */
                    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
                    delay_ms(80);
                    GPIO_SetBits(GPIOA, GPIO_Pin_8);
                    USART2_Send_Frame("BEEP OK\r\n", 10);
                }
                else if ('L' == Rx_Buf[0])
                {
                    if ('+' == Rx_Buf[2])  /* Lx+ 点亮 */
                    {
                        if ('1' == Rx_Buf[1])
                        {
                            GPIO_ResetBits(GPIOB, GPIO_Pin_5);
                            USART2_Send_Frame("LED1 ON\r\n", 10);
                        }
                        else if ('2' == Rx_Buf[1])
                        {
                            GPIO_ResetBits(GPIOB, GPIO_Pin_0);
                            USART2_Send_Frame("LED2 ON\r\n", 10);
                        }
                        else if ('3' == Rx_Buf[1])
                        {
                            GPIO_ResetBits(GPIOB, GPIO_Pin_1);
                            USART2_Send_Frame("LED3 ON\r\n", 10);
                        }
                        else Error = 1;
                    }
                    else if ('-' == Rx_Buf[2])  /* Lx- 熄灭 */
                    {
                        if ('1' == Rx_Buf[1])
                        {
                            GPIO_SetBits(GPIOB, GPIO_Pin_5);
                            USART2_Send_Frame("LED1 OFF\r\n", 11);
                        }
                        else if ('2' == Rx_Buf[1])
                        {
                            GPIO_SetBits(GPIOB, GPIO_Pin_0);
                            USART2_Send_Frame("LED2 OFF\r\n", 11);
                        }
                        else if ('3' == Rx_Buf[1])
                        {
                            GPIO_SetBits(GPIOB, GPIO_Pin_1);
                            USART2_Send_Frame("LED3 OFF\r\n", 11);
                        }
                        else Error = 1;
                    }
                    else Error = 1;
                }
                else if (('A' == Rx_Buf[0]) && ('L' == Rx_Buf[1]))
                {
                    if ('+' == Rx_Buf[2] && '+' == Rx_Buf[3])  /* AL++ 全亮 */
                    {
                        GPIO_ResetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
                        USART2_Send_Frame("ALL ON\r\n", 9);
                    }
                    else if ('-' == Rx_Buf[2] && '-' == Rx_Buf[3])  /* AL-- 全灭 */
                    {
                        GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
                        USART2_Send_Frame("ALL OFF\r\n", 10);
                    }
                    else Error = 1;
                }
                else Error = 1;
            }
            else Error = 1;

            if (Error)
            {
                Error = 0;
                USART2_Send_Frame("指令错误！\r\n", 12);
            }
        }
    }
}
