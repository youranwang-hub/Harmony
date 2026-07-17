#include <stdio.h>
#include <string.h>
#include "usart_debug.h"
#include "system_config.h"

static volatile char s_rx_line[APP_UART_LINE_MAX];
static volatile uint8_t s_rx_ready = 0;
static char s_rx_work[APP_UART_LINE_MAX];
static volatile uint16_t s_rx_index = 0;

void UART2_Init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

void UART_SendByte(USART_TypeDef* pUSARTx, uint8_t ch)
{
    USART_SendData(pUSARTx, ch);
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET) {}
}

void UART_SendString(USART_TypeDef* pUSARTx, const char* str)
{
    while (*str != '\0') {
        UART_SendByte(pUSARTx, (uint8_t)*str++);
    }
    while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TC) == RESET) {}
}

void UART_SendHalfWord(USART_TypeDef* pUSARTx, uint16_t ch)
{
    uint8_t temp_h, temp_l;
    temp_h = (ch & 0XFF00) >> 8;
    temp_l = ch & 0XFF;
    UART_SendByte(pUSARTx, temp_h);
    UART_SendByte(pUSARTx, temp_l);
}

int fputc(int ch, FILE* f)
{
    (void)f;
    UART_SendByte(USART2, (uint8_t)ch);
    return ch;
}

int fgetc(FILE* f)
{
    (void)f;
    while (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET) {}
    return (int)USART_ReceiveData(USART2);
}

uint8_t UART2_ReadLine(char *out, uint16_t out_len)
{
    uint16_t i;
    if (!s_rx_ready || out_len == 0U) return 0U;
    __disable_irq();
    for (i = 0; i < out_len - 1U && s_rx_line[i] != '\0'; ++i) {
        out[i] = (char)s_rx_line[i];
    }
    out[i] = '\0';
    s_rx_ready = 0U;
    __enable_irq();
    return 1U;
}

void USART2_IRQHandler(void)
{
    uint8_t ch;
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        ch = (uint8_t)USART_ReceiveData(USART2);
        UART_SendByte(USART2, ch);
        if (ch == '\r' || ch == '\n') {
            if (s_rx_index > 0U) {
                s_rx_work[s_rx_index] = '\0';
                strncpy((char *)s_rx_line, s_rx_work, APP_UART_LINE_MAX - 1U);
                s_rx_line[APP_UART_LINE_MAX - 1U] = '\0';
                s_rx_ready = 1U;
                s_rx_index = 0U;
            }
        } else {
            if (s_rx_index < APP_UART_LINE_MAX - 1U) {
                s_rx_work[s_rx_index++] = (char)ch;
            } else {
                s_rx_index = 0U;
            }
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
