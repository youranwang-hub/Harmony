#include <stdio.h>
#include "stm32f4xx.h"
#include "mq.h"
#include "usart.h"
#include "delay.h"

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    uint16_t adcValue;
    float voltageValue;

    /*
     * 初始化USART2：
     * 波特率115200
     * 数据位8位
     * 停止位1位
     * 无校验
     */
    UART2_Init(115200);

    /* 初始化MQ2使用的PC2和ADC1 */
    MQ_Init();

    printf("\r\n");
    printf("MQ Init OK\r\n");
    printf("MQ2 ADC Test Start\r\n");

    while (1)
    {
        /* 读取MQ2对应的12位ADC数据 */
        adcValue = MQ_ReadValue();

        /*
         * 将ADC数据转换为电压值：
         * ADC分辨率为12位
         * 数字量范围为0～4095
         * 参考电压为3.3V
         */
        voltageValue =
            ((float)adcValue / 4096.0f) * 3.3f;

        /* 通过USART2输出ADC数据和电压 */
        printf(
            "AD=%04u, %.3fV\r\n",
            (unsigned int)adcValue,
            voltageValue
        );

        /* 每隔1秒采集一次 */
        delay_ms(1000);
    }
}

