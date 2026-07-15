#include "nfc.h"
#include "system_config.h"
#include "fingerprint.h"
#include "delay.h"
#include "usart_debug.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#if APP_ENABLE_REAL_NFC

#define PN532_USART                  USART1
#define PN532_ACK_LEN                6U
#define PN532_INDEX_LEN              3U
#define PN532_INDEX_TFI              5U
#define PN532_CMD_SAM_CONFIG         0x14U
#define PN532_CMD_IN_LIST_TARGET     0x4AU
#define PN532_RX_BUF_LEN             500U
#define PN532_WAKE_TRIES             3U
#define PN532_CARD_TRIES             20U

static uint8_t s_uid[4];
static uint8_t s_rx_buf[PN532_RX_BUF_LEN];
static volatile uint16_t s_rx_len;
static uint8_t s_wake_ok;

static void pn532_usart1_init(uint32_t baud)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(PN532_USART);
    usart.USART_BaudRate = baud;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(PN532_USART, &usart);
    USART_ITConfig(PN532_USART, USART_IT_RXNE, ENABLE);
    USART_Cmd(PN532_USART, ENABLE);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 1U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void pn532_send_data(const uint8_t *data, uint8_t length)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        USART_SendData(PN532_USART, data[i]);
        while (USART_GetFlagStatus(PN532_USART, USART_FLAG_TXE) == RESET) {}
    }
}

void NFC_USART1_RxByte(uint8_t byte)
{
    if (s_rx_len < PN532_RX_BUF_LEN) {
        s_rx_buf[s_rx_len++] = byte;
    }
}

static void pn532_clear_rx(void)
{
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
    s_rx_len = 0U;
}

static int8_t pn532_check_rx(void)
{
    uint8_t i;
    uint8_t len;
    uint8_t temp;

    if (s_rx_len < (PN532_ACK_LEN + PN532_INDEX_TFI + 4U)) {
        return -1;
    }

    len = s_rx_buf[PN532_ACK_LEN + PN532_INDEX_LEN];
    temp = (uint8_t)(0x100U - len);
    if (temp != s_rx_buf[PN532_ACK_LEN + PN532_INDEX_LEN + 1U]) {
        return -1;
    }

    temp = 0U;
    for (i = 0U; i < len; i++) {
        temp = (uint8_t)(temp + s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + i]);
    }
    temp = (uint8_t)(0x100U - temp);
    if (temp != s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + len]) {
        return -1;
    }

    return 0;
}

static uint8_t pn532_checksum(const uint8_t *data)
{
    uint8_t i;
    uint8_t temp = 0U;

    for (i = 0U; i < data[PN532_INDEX_LEN]; i++) {
        temp = (uint8_t)(temp + data[PN532_INDEX_TFI + i]);
    }

    return (uint8_t)(0x100U - temp);
}

static int8_t pn532_sam_config(uint8_t mode)
{
    uint8_t data[10] = {0};
    int8_t ret = 0;

    data[0] = 0x00U;
    data[1] = 0x00U;
    data[2] = 0xFFU;
    data[3] = 0x03U;
    data[4] = (uint8_t)(0x100U - data[3]);
    data[5] = 0xD4U;
    data[6] = PN532_CMD_SAM_CONFIG;
    data[7] = mode;
    data[8] = pn532_checksum(data);
    data[9] = 0x00U;

    pn532_clear_rx();
    pn532_send_data(data, sizeof(data));
    delay_ms(200U);

    if (pn532_check_rx() < 0) {
        ret = -1;
    }

    pn532_clear_rx();
    return ret;
}

static int8_t pn532_wakeup(void)
{
    uint8_t data[14] = {0};
    uint8_t i;

    data[0] = 0x55U;
    data[1] = 0x55U;

    for (i = 0U; i < PN532_WAKE_TRIES; i++) {
        pn532_send_data(data, sizeof(data));
        if (pn532_sam_config(1U) == 0) {
            return 0;
        }
        delay_ms(100U);
    }

    return -1;
}

static int8_t pn532_find_card_once(void)
{
    uint8_t data[11] = {0};
    int8_t ret = 0;

    data[0] = 0x00U;
    data[1] = 0x00U;
    data[2] = 0xFFU;
    data[3] = 0x04U;
    data[4] = (uint8_t)(0x100U - data[3]);
    data[5] = 0xD4U;
    data[6] = PN532_CMD_IN_LIST_TARGET;
    data[7] = 0x01U;
    data[8] = 0x00U;
    data[9] = pn532_checksum(data);
    data[10] = 0x00U;

    pn532_clear_rx();
    pn532_send_data(data, sizeof(data));
    delay_ms(200U);

    if (pn532_check_rx() < 0) {
        ret = -1;
        goto done;
    }

    if (s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + 2U] != 1U) {
        ret = 0;
        goto done;
    }

    s_uid[0] = s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + 8U];
    s_uid[1] = s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + 9U];
    s_uid[2] = s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + 10U];
    s_uid[3] = s_rx_buf[PN532_ACK_LEN + PN532_INDEX_TFI + 11U];
    ret = 1;

done:
    pn532_clear_rx();
    return ret;
}

#endif

#if !APP_ENABLE_REAL_NFC
void NFC_USART1_RxByte(uint8_t byte)
{
    (void)byte;
}
#endif

void NFC_Init(void)
{
#if APP_ENABLE_REAL_NFC
    Adapter_Auth_SetUsart1Target(AUTH_USART1_TARGET_NFC);
    pn532_usart1_init(115200U);
    s_wake_ok = (pn532_wakeup() == 0) ? 1U : 0U;
    UART_SendString(USART2, s_wake_ok ? "[NFC] PN532 wake OK\r\n" : "[NFC] PN532 wake FAIL\r\n");
#endif
}

uint8_t NFC_ReadCard(char *card_id, uint16_t len)
{
#if APP_ENABLE_REAL_NFC
    uint8_t i;
    int8_t ret;

    if (card_id == 0 || len == 0U) {
        return 0U;
    }

    Adapter_Auth_SetUsart1Target(AUTH_USART1_TARGET_NFC);
    NFC_Init();
    if (s_wake_ok == 0U) {
        return 0U;
    }

    UART_SendString(USART2, "[NFC] Place card near PN532...\r\n");
    for (i = 0U; i < PN532_CARD_TRIES; i++) {
        ret = pn532_find_card_once();
        if (ret == 1) {
            snprintf(card_id, len, "NFC-%02X%02X%02X%02X",
                     s_uid[0], s_uid[1], s_uid[2], s_uid[3]);
            UART_SendString(USART2, "[NFC] Card UID ");
            UART_SendString(USART2, card_id);
            UART_SendString(USART2, "\r\n");
            return 1U;
        }
        delay_ms(100U);
    }

    UART_SendString(USART2, "[NFC] Card read timeout\r\n");
    return 0U;
#else
    const char *demo = "NFC-DEMO-001";
    if (card_id == 0 || len == 0U) return 0U;
    strncpy(card_id, demo, len - 1U);
    card_id[len - 1U] = '\0';
    return 1U;
#endif
}
