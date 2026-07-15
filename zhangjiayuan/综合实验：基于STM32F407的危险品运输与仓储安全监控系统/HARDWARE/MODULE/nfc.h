#ifndef __NFC_H__
#define __NFC_H__
#include <stdint.h>

void NFC_Init(void);
uint8_t NFC_ReadCard(char *card_id, uint16_t len);
void NFC_USART1_RxByte(uint8_t byte);

#endif
