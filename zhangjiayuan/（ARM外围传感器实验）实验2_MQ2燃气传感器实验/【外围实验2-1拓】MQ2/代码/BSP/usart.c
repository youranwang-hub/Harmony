#include <stdio.h>
#include "stm32f4xx.h"
#include "usart.h"

/**
  * @brief  初始化USART2调试串口
  * @param  bound：波特率
  * @retval 无
  */
void UART2_Init(uint32_t bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /*
     * USART2使用：
     * PA2：USART2_TX
     * PA3：USART2_RX
     */

     /* 使能GPIOA和USART2时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_USART2,
        ENABLE
    );

    /* 配置PA2和PA3的复用功能 */
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

    /* 配置USART2对应GPIO */
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

    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 配置USART2参数 */
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

    USART_Init(USART2, &USART_InitStructure);

    /* 配置USART2中断 */
    NVIC_InitStructure.NVIC_IRQChannel =
        USART2_IRQn;

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        3;

    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        3;

    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    /* 使能USART2接收数据寄存器非空中断 */
    USART_ITConfig(
        USART2,
        USART_IT_RXNE,
        ENABLE
    );

    /* 使能USART2 */
    USART_Cmd(USART2, ENABLE);
}

/**
  * @brief  发送一个字节
  * @param  pUSARTx：串口外设
  * @param  ch：待发送字节
  * @retval 无
  */
void UART_SendByte(
    USART_TypeDef* pUSARTx,
    uint8_t ch
)
{
    /* 将一个字节写入发送数据寄存器 */
    USART_SendData(pUSARTx, ch);

    /* 等待发送数据寄存器为空 */
    while (
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
        )
    {
    }
}

/**
  * @brief  发送字符串
  * @param  pUSARTx：串口外设
  * @param  str：待发送字符串
  * @retval 无
  */
void UART_SendString(
    USART_TypeDef* pUSARTx,
    char* str
)
{
    while (*str != '\0')
    {
        UART_SendByte(
            pUSARTx,
            (uint8_t)(*str)
        );

        str++;
    }

    /* 等待整个字符串发送完成 */
    while (
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TC
        ) == RESET
        )
    {
    }
}

/**
  * @brief  发送一个16位数据
  * @param  pUSARTx：串口外设
  * @param  ch：待发送数据
  * @retval 无
  */
void UART_SendHalfWord(
    USART_TypeDef* pUSARTx,
    uint16_t ch
)
{
    uint8_t temp_h;
    uint8_t temp_l;

    /* 取出高8位和低8位 */
    temp_h = (uint8_t)((ch & 0xFF00U) >> 8);
    temp_l = (uint8_t)(ch & 0x00FFU);

    /* 发送高8位 */
    USART_SendData(pUSARTx, temp_h);

    while (
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
        )
    {
    }

    /* 发送低8位 */
    USART_SendData(pUSARTx, temp_l);

    while (
        USART_GetFlagStatus(
            pUSARTx,
            USART_FLAG_TXE
        ) == RESET
        )
    {
    }
}

/**
  * @brief  重定向printf到USART2
  * @param  ch：待发送字符
  * @param  f：文件指针
  * @retval 发送的字符
  */
int fputc(int ch, FILE* f)
{
    (void)f;

    USART_SendData(
        USART2,
        (uint8_t)ch
    );

    while (
        USART_GetFlagStatus(
            USART2,
            USART_FLAG_TXE
        ) == RESET
        )
    {
    }

    return ch;
}

/**
  * @brief  重定向scanf和getchar到USART2
  * @param  f：文件指针
  * @retval 接收到的字符
  */
int fgetc(FILE* f)
{
    (void)f;

    /* 等待接收到串口数据 */
    while (
        USART_GetFlagStatus(
            USART2,
            USART_FLAG_RXNE
        ) == RESET
        )
    {
    }

    return (int)USART_ReceiveData(USART2);
}

/**
  * @brief  USART2中断服务函数
  * @param  无
  * @retval 无
  */
void USART2_IRQHandler(void)
{
    uint8_t receiveData;

    /* 判断是否产生接收数据寄存器非空中断 */
    if (
        USART_GetITStatus(
            USART2,
            USART_IT_RXNE
        ) != RESET
        )
    {
        /* 读取接收到的数据 */
        receiveData =
            (uint8_t)USART_ReceiveData(USART2);

        /* 将接收到的数据原样回显 */
        USART_SendData(
            USART2,
            receiveData
        );

        while (
            USART_GetFlagStatus(
                USART2,
                USART_FLAG_TXE
            ) == RESET
            )
        {
        }
    }
}
