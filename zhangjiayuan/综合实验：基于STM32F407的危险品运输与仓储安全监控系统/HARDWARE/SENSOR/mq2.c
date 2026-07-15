#include "mq2.h"
#include "adc_sensor.h"
void MQ2_Init(void) {}
uint16_t MQ2_ReadValue(void)
{
    return Board_ADC_ReadGasReal();
}
