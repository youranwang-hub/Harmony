#include "stm32f4xx.h"

/* 接收数据相关变量 */
volatile u8 Rx_Buf[64
];
volatile u16 Rx_Count = 0
;
volatile u16 Rx_Len = 0
;
volatile u8 Rx_Frame_Flag = 0
;
volatile u8 CR_Flag = 0
;

/* 流水灯状态变量 */
u8 Flow_Flag = 0
;
u8 Flow_Step = 0
;

/* 初始化结构体 */
GPIO_InitTypeDef GPIO_InitStructure
;
USART_InitTypeDef USART_InitStructure
;
NVIC_InitTypeDef NVIC_InitStructure
;

/* 函数声明 */
void LED_Init(void
);
void USART2_Init(void
);
void USART2_Send_String(const char *str
);
u8 Command_Match(const char *cmd
);
void Flow_LED_Run(void
);
void delay_ms(u16 nms
);

int main(void
)
{
    LED_Init
();
    USART2_Init
();

    USART2_Send_String("USART2 LED Control Ready!\r\n"
);
    USART2_Send_String("Single LED: LED1_ON, LED1_OFF, LED2_ON, LED2_OFF, LED3_ON, LED3_OFF\r\n"
);
    USART2_Send_String("Flow LED: FLOW_ON, FLOW_OFF\r\n"
);

    while(1
)
    {
        if(Rx_Frame_Flag
)
        {
            if(Command_Match("LED1_ON"
))
            {
                Flow_Flag = 0
;
                GPIO_ResetBits(GPIOB, GPIO_Pin_5
);
                USART2_Send_String("LED1 ON\r\n"
);
            }
            else if(Command_Match("LED1_OFF"
))
            {
                Flow_Flag = 0
;
                GPIO_SetBits(GPIOB, GPIO_Pin_5
);
                USART2_Send_String("LED1 OFF\r\n"
);
            }
            else if(Command_Match("LED2_ON"
))
            {
                Flow_Flag = 0
;
                GPIO_ResetBits(GPIOB, GPIO_Pin_0
);
                USART2_Send_String("LED2 ON\r\n"
);
            }
            else if(Command_Match("LED2_OFF"
))
            {
                Flow_Flag = 0
;
                GPIO_SetBits(GPIOB, GPIO_Pin_0
);
                USART2_Send_String("LED2 OFF\r\n"
);
            }
            else if(Command_Match("LED3_ON"
))
            {
                Flow_Flag = 0
;
                GPIO_ResetBits(GPIOB, GPIO_Pin_1
);
                USART2_Send_String("LED3 ON\r\n"
);
            }
            else if(Command_Match("LED3_OFF"
))
            {
                Flow_Flag = 0
;
                GPIO_SetBits(GPIOB, GPIO_Pin_1
);
                USART2_Send_String("LED3 OFF\r\n"
);
            }
            else if(Command_Match("FLOW_ON"
))
            {
                Flow_Flag = 1
;
                Flow_Step = 0
;
                USART2_Send_String("FLOW LED ON\r\n"
);
            }
            else if(Command_Match("FLOW_OFF"
))
            {
                Flow_Flag = 0
;
                GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
);
                USART2_Send_String("FLOW LED OFF\r\n"
);
            }
            else
            {
                USART2_Send_String("COMMAND ERROR!\r\n"
);
            }

            Rx_Frame_Flag = 0
;
        }

        if(Flow_Flag
)
        {
            Flow_LED_Run
();
            delay_ms(200
);
        }
    }
}

/* LED初始化：LED1-PB5，LED2-PB0，LED3-PB1 */
void LED_Init(void
)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE
);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT
;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP
;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz
;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL
;
    GPIO_Init(GPIOB, &GPIO_InitStructure
);

    /* LED低电平点亮，初始化时输出高电平，使三个LED全部熄灭 */
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
);
}

/* USART2初始化：TX-PA2，RX-PA3，波特率115200 */
void USART2_Init(void
)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE
);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE
);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3
;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF
;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz
;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP
;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP
;
    GPIO_Init(GPIOA, &GPIO_InitStructure
);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2
);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2
);

    USART_InitStructure.USART_BaudRate = 115200
;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b
;
    USART_InitStructure.USART_StopBits = USART_StopBits_1
;
    USART_InitStructure.USART_Parity = USART_Parity_No
;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None
;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx
;
    USART_Init(USART2, &USART_InitStructure
);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0
);
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn
;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1
;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3
;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE
;
    NVIC_Init(&NVIC_InitStructure
);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE
);
    USART_Cmd(USART2, ENABLE
);
}

/* 发送字符串 */
void USART2_Send_String(const char *str
)
{
    while(*str != '\0'
)
    {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET
);
        USART_SendData(USART2, (u16)(*str
));
        str++
;
    }

    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET
);
}

/* 判断收到的数据是否与指定指令完全相同 */
u8 Command_Match(const char *cmd
)
{
    u16 i = 0
;

    while(cmd[i] != '\0'
)
    {
        if(i >= Rx_Len || Rx_Buf[i] != (u8)cmd[i
])
        {
            return 0
;
        }
        i++
;
    }

    if(i != Rx_Len
)
    {
        return 0
;
    }

    return 1
;
}

/* 执行一次流水灯步骤：LED1 -> LED2 -> LED3 */
void Flow_LED_Run(void
)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1
);

    if(Flow_Step == 0
)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_5
);
    }
    else if(Flow_Step == 1
)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_0
);
    }
    else
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_1
);
    }

    Flow_Step++
;
    if(Flow_Step >= 3
)
    {
        Flow_Step = 0
;
    }
}

/* 简单毫秒延时函数 */
void delay_ms(u16 nms
)
{
    u16 i
;

    while(nms--
)
    {
        i = 33800
;
        while(i--
);
    }
}

/* USART2接收中断函数，以回车换行\r\n作为一帧数据的结束标志 */
void USART2_IRQHandler(void
)
{
    u8 data
;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET
)
    {
        data = (u8)USART_ReceiveData(USART2
);

        if(data == 0x0D
)
        {
            CR_Flag = 1
;
        }
        else if((data == 0x0A) && CR_Flag
)
        {
            if(!Rx_Frame_Flag
)
            {
                Rx_Len = Rx_Count
;
                Rx_Frame_Flag = 1
;
            }

            Rx_Count = 0
;
            CR_Flag = 0
;
        }
        else
        {
            CR_Flag = 0
;

            if(!Rx_Frame_Flag
)
            {
                if(Rx_Count < sizeof(Rx_Buf
))
                {
                    Rx_Buf[Rx_Count] = data
;
                    Rx_Count++
;
                }
                else
                {
                    Rx_Count = 0
;
                }
            }
        }
    }
}
