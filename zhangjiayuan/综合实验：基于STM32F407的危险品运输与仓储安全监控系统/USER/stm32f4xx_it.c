/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt handlers for hazmat monitor project.
  ******************************************************************************
  */

#include "stm32f4xx_it.h"
#include "app_main.h"
#include "dma_uart.h"
#include "can_protocol.h"
#include "fingerprint.h"

void NMI_Handler(void) {}

void HardFault_Handler(void)
{
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1) {}
}

void BusFault_Handler(void)
{
  while (1) {}
}

void UsageFault_Handler(void)
{
  while (1) {}
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void SysTick_Handler(void)
{
  App_OnSysTick();
}

void EXTI0_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
    EXTI_ClearITPendingBit(EXTI_Line0);
    App_OnEmergencyButton();
  }
}

void DMA1_Stream6_IRQHandler(void)
{
  Board_DMA_USART2_IRQHandler();
}

void CAN1_RX0_IRQHandler(void)
{
  Board_CAN_IRQHandler();
}

void USART1_IRQHandler(void)
{
  Adapter_Auth_USART1_IRQHandler();
}
