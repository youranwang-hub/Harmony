#include "stm32f4xx.h"
#include<stdio.h>
#include "dht11.h"
#include "mq.h"
#include "usart.h"
#include "xpt2046.h"
#include "lcd_ili9341.h"
#include "lcd_fonts.h"
#include "delay.h"

// PWM全局结构体
u16 PWM_Value = 0;
GPIO_InitTypeDef GPIO_InitStructure;
TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
TIM_OCInitTypeDef TIM_OCInitStructure;

// 触摸报警关闭标志
uint8_t alarm_manual_close = 0;

// 函数声明
void PWM_FastBlink(void);
void PWM_SlowBlink(void);
void PWM_LED_Off(void);
void LCD_ShowNormal(uint8_t temp,uint8_t humi,uint16_t gas);
void LCD_ShowWarningRed(uint8_t temp,uint8_t humi,uint16_t gas);
void LCD_ShowWarningYellow(uint8_t temp,uint8_t humi,uint16_t gas);
uint8_t Touch_Scan(void);

int main(void)
{
    // 1. 系统底层初始化
    delay_init();
    UART2_Init(115200);
    printf("室内环境安防系统启动\r\n");

    // 2. 全部外设初始化
    DHT11_Init();
    MQ_Init();
    XPT2046_Init();
    LCD_Init();

    // 3. TIM3 PB5 PWM报警LED初始化
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_PinAFConfig(GPIOB,GPIO_PinSource5,GPIO_AF_TIM3);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB,&GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Prescaler = 84;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 500-1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3,&TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3,ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    // 开机待机界面
    LCD_ShowNormal(0,0,0);
    delay_ms(1000);

    // 主循环
    while(1)
    {
        // 触摸检测：点击屏幕关闭报警
        if(Touch_Scan() == 1)
        {
            alarm_manual_close = 1;
            printf("触摸屏幕，手动屏蔽报警灯光\r\n");
        }

        DHT11_Data dht_data;
        uint16_t gas_val;
        int8_t dht_ret;

        // 读取温湿度、气体浓度
        dht_ret = DHT11_ReadData(&dht_data);
        gas_val = MQ_ReadValue();

        // 串口打印数据
        if(dht_ret == 0)
        {
            printf("温度:%d.%d℃ 湿度:%d.%d%%RH 气体浓度:%d\r\n",
                   dht_data.temp_int,dht_data.temp_deci,
                   dht_data.humi_int,dht_data.humi_deci,
                   gas_val);
        }
        else
        {
            printf("DHT11温湿度采集失败\r\n");
        }

        // 分级报警逻辑
        if(gas_val > 800)
        {
            alarm_manual_close = 0; // 新险情自动清除手动屏蔽
            LCD_ShowWarningRed(dht_data.temp_int,dht_data.humi_int,gas_val);
            if(alarm_manual_close == 0)
                PWM_FastBlink();
        }
        else if(dht_data.temp_int > 28 && gas_val > 300)
        {
            alarm_manual_close = 0;
            LCD_ShowWarningYellow(dht_data.temp_int,dht_data.humi_int,gas_val);
            if(alarm_manual_close == 0)
                PWM_SlowBlink();
        }
        else
        {
            alarm_manual_close = 0;
            LCD_ShowNormal(dht_data.temp_int,dht_data.humi_int,gas_val);
            PWM_LED_Off();
        }

        // 手动关闭报警强制覆盖
        if(alarm_manual_close == 1)
        {
            LCD_ShowNormal(dht_data.temp_int,dht_data.humi_int,gas_val);
            PWM_LED_Off();
        }

        delay_ms(1000);
    }
}



// PWM灯光控制函数
void PWM_FastBlink(void)
{
    TIM_SetCompare2(TIM3, 450);
    delay_ms(100);
    TIM_SetCompare2(TIM3, 0);
    delay_ms(100);
}

void PWM_SlowBlink(void)
{
    TIM_SetCompare2(TIM3, 200);
    delay_ms(500);
    TIM_SetCompare2(TIM3, 0);
    delay_ms(500);
}

void PWM_LED_Off(void)
{
    TIM_SetCompare2(TIM3, 0);
}

// 正常监测界面（白底黑字）
void LCD_ShowNormal(uint8_t temp,uint8_t humi,uint16_t gas)
{
    LCD_ClearFull(WHITE);
    LCD_ShowString(20,20,"Temp:    C",BLACK);
    LCD_ShowNum(80,20,temp,2,BLACK);
    LCD_ShowString(20,50,"Humi:    %RH",BLACK);
    LCD_ShowNum(80,50,humi,2,BLACK);
    LCD_ShowString(20,80,"Gas:      ",BLACK);
    LCD_ShowNum(80,80,gas,4,BLACK);
    LCD_ShowString(20,110,"Touch screen to stop alarm",BLACK);
}

// 红色高危报警界面（气体泄漏）
void LCD_ShowWarningRed(uint8_t temp,uint8_t humi,uint16_t gas)
{
    LCD_ClearFull(RED);
    LCD_ShowString(20,20,"!!!GAS DANGER!!!",WHITE);
    LCD_ShowString(20,50,"Temp:    C",WHITE);
    LCD_ShowNum(80,50,temp,2,WHITE);
    LCD_ShowString(20,80,"Gas:      ",WHITE);
    LCD_ShowNum(80,80,gas,4,WHITE);
    LCD_ShowString(20,110,"Touch screen to stop alarm",WHITE);
}

// 黄色温度预警界面
void LCD_ShowWarningYellow(uint8_t temp,uint8_t humi,uint16_t gas)
{
    LCD_ClearFull(YELLOW);
    LCD_ShowString(20,20,"Temperature Warning",BLACK);
    LCD_ShowString(20,50,"Temp:    C",BLACK);
    LCD_ShowNum(80,50,temp,2,BLACK);
    LCD_ShowString(20,80,"Gas:      ",BLACK);
    LCD_ShowNum(80,80,gas,4,BLACK);
    LCD_ShowString(20,110,"Touch screen to stop alarm",BLACK);
}
