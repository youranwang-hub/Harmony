#include <stdio.h>
#include "usart.h"


void UART2_Init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /*
    使能GPIOA和USART2时钟
    */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_USART2,
        ENABLE
    );

    /*
    USART2对应引脚复用映射：
    PA2：TX
    PA3：RX
    */
    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource2,
        GPIO_AF_USART2
    );

    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource3,
        GPIO_AF_USART2
    );

    /*
    USART2 IO配置
    */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_2 | GPIO_Pin_3;

    GPIO_InitStructure.GPIO_Mode =
        GPIO_Mode_AF;

    GPIO_InitStructure.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_OType =
        GPIO_OType_PP;

    GPIO_InitStructure.GPIO_PuPd =
        GPIO_PuPd_UP;

    GPIO_Init(
        GPIOA,
        &GPIO_InitStructure
    );

    /*
    USART2端口配置
    */
    USART_InitStructure.USART_BaudRate =
        bound;

    USART_InitStructure.USART_WordLength =
        USART_WordLength_8b;

    USART_InitStructure.USART_StopBits =
        USART_StopBits_1;

    USART_InitStructure.USART_Parity =
        USART_Parity_No;

    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;

    USART_InitStructure.USART_Mode =
        USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(
        USART2,
        &USART_InitStructure
    );

    /*
    配置USART2中断通道。
    当前实验不打开USART2接收中断，
    getchar通过轮询方式接收数据。
    */
    NVIC_InitStructure.NVIC_IRQChannel =
        USART2_IRQn;

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        3;

    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        3;

    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(
        &NVIC_InitStructure
    );

    /*
    本实验不要开启USART2接收中断
    */
    /*
    USART_ITConfig(
        USART2,
        USART_IT_RXNE,
        ENABLE
    );
    */

    USART_Cmd(
        USART2,
        ENABLE
    );
}


/*
发送一个字符
*/
void UART_SendByte(
    USART_TypeDef *pUSARTx,
    uint8_t ch
)
{
    USART_SendData(
        pUSARTx,
        ch
    );

    while(
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
    )
    {
    }
}


/*
发送字符串
*/
void UART_SendString(
    USART_TypeDef *pUSARTx,
    char *str
)
{
    unsigned int k;

    k = 0;

    do
    {
        UART_SendByte(
            pUSARTx,
            *(str + k)
        );

        k++;
    }
    while(*(str + k) != '\0');

    while(
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TC
        ) == RESET
    )
    {
    }
}


/*
发送一个16位数
*/
void UART_SendHalfWord(
    USART_TypeDef *pUSARTx,
    uint16_t ch
)
{
    uint8_t temp_h;
    uint8_t temp_l;

    temp_h =
        (ch & 0xFF00) >> 8;

    temp_l =
        ch & 0x00FF;

    USART_SendData(
        pUSARTx,
        temp_h
    );

    while(
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
    )
    {
    }

    USART_SendData(
        pUSARTx,
        temp_l
    );

    while(
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
    )
    {
    }
}


/*
重定向printf到USART2
*/
int fputc(int ch, FILE *f)
{
    USART_SendData(
        USART2,
        (uint8_t)ch
    );

    while(
        USART_GetFlagStatus(
            USART2,
            USART_FLAG_TXE
        ) == RESET
    )
    {
    }

    return ch;
}


/*
重定向scanf、getchar到USART2
*/
int fgetc(FILE *f)
{
    while(
        USART_GetFlagStatus(
            USART2,
            USART_FLAG_RXNE
        ) == RESET
    )
    {
    }

    return (int)USART_ReceiveData(
        USART2
    );
}


/*
USART2中断处理函数

本实验未开启USART2接收中断，
保留实验手册中的中断函数。
*/
void USART2_IRQHandler(void)
{
    u8 sbuf;

    sbuf = 0;

    if(
        USART_GetFlagStatus(
            USART2,
            USART_FLAG_RXNE
        ) != RESET
    )
    {
        sbuf = (u8)USART_ReceiveData(
            USART2
        );

        USART_SendData(
            USART2,
            sbuf
        );
    }
}
