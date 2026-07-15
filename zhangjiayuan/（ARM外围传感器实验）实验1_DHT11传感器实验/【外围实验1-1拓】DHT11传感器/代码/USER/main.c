#include <stdio.h>
#include "stm32f4xx.h"
#include "dht11.h"
#include "usart.h"
#include "delay.h"

/*======================= 拓展功能参数 =======================*/

/* 指定采集次数 */
#define DHT11_SAMPLE_COUNT          10

/* 每次采集之间的间隔，单位：毫秒 */
#define DHT11_SAMPLE_INTERVAL_MS    10000

/* 温度报警阈值：30.0℃ */
#define TEMP_ALARM_THRESHOLD        300

/* 湿度报警阈值：80.0% */
#define HUMI_ALARM_THRESHOLD        800

/* LED1：PB5，低电平点亮 */
#define ALARM_LED_PORT              GPIOB
#define ALARM_LED_PIN               GPIO_Pin_5

/* 蜂鸣器：PA8，低电平有效 */
#define ALARM_BEEP_PORT             GPIOA
#define ALARM_BEEP_PIN              GPIO_Pin_8


/*======================= 函数声明 =======================*/

static void Alarm_GPIO_Init(void);
static void Alarm_Set(uint8_t state);
static void Print_DHT11_Value(uint16_t value);


/**
  * @brief  初始化报警LED和蜂鸣器
  * @param  无
  * @retval 无
  */
static void Alarm_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOA和GPIOB时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    /*
     * 初始化LED1引脚PB5
     * LED1低电平点亮，高电平熄灭
     */
    GPIO_InitStructure.GPIO_Pin   = ALARM_LED_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ALARM_LED_PORT, &GPIO_InitStructure);

    /*
     * 初始化蜂鸣器引脚PA8
     * 蜂鸣器低电平响，高电平关闭
     */
    GPIO_InitStructure.GPIO_Pin   = ALARM_BEEP_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ALARM_BEEP_PORT, &GPIO_InitStructure);

    /* 初始状态：LED熄灭，蜂鸣器关闭 */
    GPIO_SetBits(ALARM_LED_PORT, ALARM_LED_PIN);
    GPIO_SetBits(ALARM_BEEP_PORT, ALARM_BEEP_PIN);
}


/**
  * @brief  控制LED和蜂鸣器报警
  * @param  state：
  *         0：关闭报警
  *         1：开启报警
  * @retval 无
  */
static void Alarm_Set(uint8_t state)
{
    if(state == 1)
    {
        /* 低电平有效：LED点亮，蜂鸣器响 */
        GPIO_ResetBits(ALARM_LED_PORT, ALARM_LED_PIN);
        GPIO_ResetBits(ALARM_BEEP_PORT, ALARM_BEEP_PIN);
    }
    else
    {
        /* 高电平：LED熄灭，蜂鸣器关闭 */
        GPIO_SetBits(ALARM_LED_PORT, ALARM_LED_PIN);
        GPIO_SetBits(ALARM_BEEP_PORT, ALARM_BEEP_PIN);
    }
}


/**
  * @brief  输出扩大10倍保存的温湿度数值
  * @param  value：扩大10倍后的数值
  *                例如253表示25.3
  * @retval 无
  */
static void Print_DHT11_Value(uint16_t value)
{
    printf("%u.%u",
           (unsigned int)(value / 10),
           (unsigned int)(value % 10));
}


/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    DHT11_Data Data;

    /*
     * 温湿度值扩大10倍保存。
     * 例如温度25.3℃保存为253。
     */
    uint16_t temp_value = 0;
    uint16_t humi_value = 0;

    /* 最大值和最小值 */
    uint16_t temp_max = 0;
    uint16_t temp_min = 0;
    uint16_t humi_max = 0;
    uint16_t humi_min = 0;

    /* 采集次数统计 */
    uint8_t sample_count  = 0;
    uint8_t success_count = 0;
    uint8_t error_count   = 0;

    /*
     * 标记是否已经获得过有效数据。
     * 第一次成功读取时，用当前数据初始化最大值和最小值。
     */
    uint8_t data_valid = 0;

    /* 初始化UART2，波特率115200 */
    UART2_Init(115200);

    /* 初始化DHT11 */
    DHT11_Init();

    /* 初始化报警LED和蜂鸣器 */
    Alarm_GPIO_Init();

    printf("\r\n");
    printf("========================================\r\n");
    printf("DHT11 Temperature and Humidity System\r\n");
    printf("DHT11 Init OK\r\n");
    printf("Sample Count: %d\r\n", DHT11_SAMPLE_COUNT);
    printf("Temperature Alarm: 30.0 C\r\n");
    printf("Humidity Alarm: 80.0 %%\r\n");
    printf("========================================\r\n");

    /* 等待DHT11稳定 */
    delay_ms(1000);

    /*
     * 按指定次数重复采集。
     * 本程序共执行10次采集操作。
     */
    for(sample_count = 1;
        sample_count <= DHT11_SAMPLE_COUNT;
        sample_count++)
    {
        printf("\r\n");
        printf("---------- Sample %d/%d ----------\r\n",
               sample_count,
               DHT11_SAMPLE_COUNT);

        /*
         * 读取DHT11数据。
         * 返回0表示读取成功，返回-1表示读取失败。
         */
        if(DHT11_ReadData(&Data) == 0)
        {
            success_count++;

            /*
             * 将整数部分和小数部分组合，
             * 并扩大10倍保存。
             */
            temp_value =
                (uint16_t)Data.temp_int * 10 +
                Data.temp_deci;

            humi_value =
                (uint16_t)Data.humi_int * 10 +
                Data.humi_deci;

            /* 输出本次采集到的温湿度 */
            printf("Temperature: ");
            Print_DHT11_Value(temp_value);
            printf(" C\r\n");

            printf("Humidity   : ");
            Print_DHT11_Value(humi_value);
            printf(" %%\r\n");

            /*
             * 第一次成功读取数据时，
             * 使用当前数据初始化最大值和最小值。
             */
            if(data_valid == 0)
            {
                temp_max = temp_value;
                temp_min = temp_value;

                humi_max = humi_value;
                humi_min = humi_value;

                data_valid = 1;
            }
            else
            {
                /* 更新最高温度 */
                if(temp_value > temp_max)
                {
                    temp_max = temp_value;
                }

                /* 更新最低温度 */
                if(temp_value < temp_min)
                {
                    temp_min = temp_value;
                }

                /* 更新最高湿度 */
                if(humi_value > humi_max)
                {
                    humi_max = humi_value;
                }

                /* 更新最低湿度 */
                if(humi_value < humi_min)
                {
                    humi_min = humi_value;
                }
            }

            /* 输出当前最大值和最小值 */
            printf("Temperature MAX: ");
            Print_DHT11_Value(temp_max);
            printf(" C\r\n");

            printf("Temperature MIN: ");
            Print_DHT11_Value(temp_min);
            printf(" C\r\n");

            printf("Humidity MAX   : ");
            Print_DHT11_Value(humi_max);
            printf(" %%\r\n");

            printf("Humidity MIN   : ");
            Print_DHT11_Value(humi_min);
            printf(" %%\r\n");

            /*
             * 温湿度阈值判断。
             * 温度达到30.0℃或湿度达到80.0%，
             * 任意一个条件满足就开启报警。
             */
            if((temp_value >= TEMP_ALARM_THRESHOLD) ||
               (humi_value >= HUMI_ALARM_THRESHOLD))
            {
                Alarm_Set(1);

                printf("ALARM: ON\r\n");

                if(temp_value >= TEMP_ALARM_THRESHOLD)
                {
                    printf("Warning: Temperature is too high!\r\n");
                }

                if(humi_value >= HUMI_ALARM_THRESHOLD)
                {
                    printf("Warning: Humidity is too high!\r\n");
                }
            }
            else
            {
                Alarm_Set(0);

                printf("ALARM: OFF\r\n");
                printf("Environment status is normal.\r\n");
            }
        }
        else
        {
            /*
             * 读取失败时累计错误次数，
             * 同时关闭报警。
             */
            error_count++;
            Alarm_Set(0);

            printf("Read DHT11 ERROR!\r\n");
            printf("ALARM: OFF\r\n");
        }

        /* 输出当前采集统计 */
        printf("Successful Reads: %d\r\n", success_count);
        printf("Failed Reads    : %d\r\n", error_count);

        /*
         * 最后一次采集完成后不再延时，
         * 其余每次采集后延时10秒。
         */
        if(sample_count < DHT11_SAMPLE_COUNT)
        {
            printf("Wait for next sample...\r\n");
            delay_ms(DHT11_SAMPLE_INTERVAL_MS);
        }
    }

    /*======================= 输出最终统计结果 =======================*/

    /* 指定次数完成后关闭报警 */
    Alarm_Set(0);

    printf("\r\n");
    printf("========================================\r\n");
    printf("DHT11 Sampling Completed!\r\n");
    printf("Total Samples   : %d\r\n", DHT11_SAMPLE_COUNT);
    printf("Successful Reads: %d\r\n", success_count);
    printf("Failed Reads    : %d\r\n", error_count);

    /*
     * 只有至少成功读取过一次数据，
     * 才输出最终最大值和最小值。
     */
    if(data_valid == 1)
    {
        printf("----------------------------------------\r\n");

        printf("Final Temperature MAX: ");
        Print_DHT11_Value(temp_max);
        printf(" C\r\n");

        printf("Final Temperature MIN: ");
        Print_DHT11_Value(temp_min);
        printf(" C\r\n");

        printf("Final Humidity MAX   : ");
        Print_DHT11_Value(humi_max);
        printf(" %%\r\n");

        printf("Final Humidity MIN   : ");
        Print_DHT11_Value(humi_min);
        printf(" %%\r\n");
    }
    else
    {
        printf("No valid DHT11 data was obtained.\r\n");
    }

    printf("Alarm has been turned off.\r\n");
    printf("========================================\r\n");

    /*
     * 采集完成后停在此处，
     * 不再继续读取DHT11。
     */
    while(1)
    {
    }
}
