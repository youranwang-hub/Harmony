#include "can_protocol.h"

static volatile uint8_t s_can_seen = 0;
static volatile uint8_t s_virtual_online = 1;

void Board_CAN_Init(void)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_CAN1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_CAN1);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &gpio);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can);
    can.CAN_TTCM = DISABLE;
    can.CAN_ABOM = DISABLE;
    can.CAN_AWUM = DISABLE;
    can.CAN_NART = ENABLE;
    can.CAN_RFLM = DISABLE;
    can.CAN_TXFP = DISABLE;
    can.CAN_Mode = CAN_Mode_LoopBack;
    can.CAN_SJW = CAN_SJW_1tq;
    can.CAN_BS1 = CAN_BS1_6tq;
    can.CAN_BS2 = CAN_BS2_7tq;
    can.CAN_Prescaler = 6;
    CAN_Init(CAN1, &can);

    filter.CAN_FilterNumber = 0;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh = 0;
    filter.CAN_FilterIdLow = 0;
    filter.CAN_FilterMaskIdHigh = 0;
    filter.CAN_FilterMaskIdLow = 0;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&filter);

    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
    nvic.NVIC_IRQChannel = CAN1_RX0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 2;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void send_frame(uint16_t id, uint8_t b0, uint8_t b1, uint16_t value)
{
    CanTxMsg tx;
    tx.StdId = id;
    tx.ExtId = 0;
    tx.IDE = CAN_Id_Standard;
    tx.RTR = CAN_RTR_Data;
    tx.DLC = 4;
    tx.Data[0] = b0;
    tx.Data[1] = b1;
    tx.Data[2] = (uint8_t)(value >> 8);
    tx.Data[3] = (uint8_t)value;
    CAN_Transmit(CAN1, &tx);
}

void Board_CAN_SendHeartbeat(uint8_t state, uint16_t gas_value)
{
    if (s_virtual_online) send_frame(0x103, state, 0xA5, gas_value);
}

void Board_CAN_SendAlarm(uint8_t alarm_level, uint16_t gas_value)
{
    send_frame(0x3FF, alarm_level, 0xE1, gas_value);
}

void Board_CAN_IRQHandler(void)
{
    CanRxMsg rx;
    while (CAN_MessagePending(CAN1, CAN_FIFO0) > 0U) {
        CAN_Receive(CAN1, CAN_FIFO0, &rx);
        if (rx.StdId == 0x103 || rx.StdId == 0x3FF) s_can_seen = 1U;
    }
}

uint8_t Board_CAN_IsOnline(void)
{
    return (uint8_t)(s_virtual_online && s_can_seen);
}

void Board_CAN_SetVirtualOnline(uint8_t online)
{
    s_virtual_online = online ? 1U : 0U;
    if (!online) s_can_seen = 0U;
}

const char *Board_CAN_StatusName(void)
{
    if (!s_virtual_online) {
        return "SIM_OFF";
    }
    return s_can_seen ? "LOOPBACK" : "LOOPBACK_WAIT";
}
