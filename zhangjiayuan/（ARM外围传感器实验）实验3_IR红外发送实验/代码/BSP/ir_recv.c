#include <stdio.h>
#include "stm32f4xx.h"
#include "ir_recv.h"

/* 红外接收使用TIM9通道1，输入引脚为PE5 */
#define IR_RECV_TIM                 TIM9

/* TIM9计数频率设置为1 MHz，每个计数值代表1 us */
#define IR_TIMER_FREQUENCY          1000000UL

/*
 * 使用下降沿间隔进行NEC协议判断。
 *
 * NEC完整引导码：
 * 9 ms低电平 + 4.5 ms高电平 = 13.5 ms
 */
#define IR_LEAD_MIN                 12500UL
#define IR_LEAD_MAX                 14500UL

/*
 * NEC重复码：
 * 9 ms低电平 + 2.25 ms高电平 = 11.25 ms
 */
#define IR_REPEAT_MIN               10500UL
#define IR_REPEAT_MAX               12000UL

/*
 * NEC数据0：
 * 560 us低电平 + 560 us高电平 = 1.12 ms
 */
#define IR_BIT0_MIN                 850UL
#define IR_BIT0_MAX                 1450UL

/*
 * NEC数据1：
 * 560 us低电平 + 1680 us高电平 = 2.24 ms
 */
#define IR_BIT1_MIN                 1800UL
#define IR_BIT1_MAX                 2800UL

/* 是否已经捕获第一个下降沿 */
static volatile uint8_t IR_FirstEdge = 0;

/* 是否已经识别到NEC引导码 */
static volatile uint8_t IR_Receiving = 0;

/* 当前已经接收的数据位数量 */
static volatile uint8_t IR_BitCount = 0;

/* 是否存在一帧完整数据 */
static volatile uint8_t IR_FrameReady = 0;

/* 当前正在接收的32位数据 */
static volatile uint32_t IR_ShiftData = 0;

/* 已经完成接收的32位数据 */
static volatile uint32_t IR_FrameData = 0;

/* 重复码计数 */
static volatile uint32_t IR_RepeatCount = 0;


/* 内部函数声明 */
static void IR_GPIO_Init(void);
static void IR_TIM_Init(void);
static void IR_ResetFrame(void);


/**
  * @brief  初始化红外接收模块
  * @param  无
  * @retval 无
  */
void IR_Recv_Init(void)
{
    IR_GPIO_Init();
    IR_TIM_Init();
}


/**
  * @brief  初始化PE5为TIM9_CH1复用输入
  * @param  无
  * @retval 无
  */
static void IR_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* 开启GPIOE时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOE,
        ENABLE
    );

    /* PE5配置为复用功能 */
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;

    GPIO_Init(
        GPIOE,
        &GPIO_InitStruct
    );

    /* PE5复用为TIM9_CH1 */
    GPIO_PinAFConfig(
        GPIOE,
        GPIO_PinSource5,
        GPIO_AF_TIM9
    );
}


/**
  * @brief  初始化TIM9输入捕获
  * @note   TIM9计数频率自动设置为1 MHz
  * @param  无
  * @retval 无
  */
static void IR_TIM_Init(void)
{
    RCC_ClocksTypeDef RCC_Clocks;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_ICInitTypeDef TIM_ICInitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    uint32_t timClock;
    uint16_t prescaler;

    /* 开启TIM9时钟 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_TIM9,
        ENABLE
    );

    /*
     * 获取APB2时钟。
     * 当APB2存在分频时，定时器时钟为PCLK2的2倍。
     */
    RCC_GetClocksFreq(&RCC_Clocks);

    timClock = RCC_Clocks.PCLK2_Frequency;

    if((RCC->CFGR & RCC_CFGR_PPRE2) != 0)
    {
        timClock = timClock * 2;
    }

    /*
     * TIM9设置为1 MHz：
     * 每计数一次代表1 us。
     */
    prescaler = (uint16_t)(
        timClock / IR_TIMER_FREQUENCY - 1UL
    );

    /*
     * 定时器最大计时20 ms。
     * NEC完整引导间隔约13.5 ms，因此不能继续使用10 ms周期。
     */
    TIM_TimeBaseStruct.TIM_Period        = 19999;
    TIM_TimeBaseStruct.TIM_Prescaler     = prescaler;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up;

    TIM_TimeBaseInit(
        IR_RECV_TIM,
        &TIM_TimeBaseStruct
    );

    /*
     * TIM9_CH1输入捕获。
     * 红外接收头空闲时为高电平，收到红外载波时输出低电平，
     * 因此捕获下降沿。
     */
    TIM_ICInitStruct.TIM_Channel     = TIM_Channel_1;
    TIM_ICInitStruct.TIM_ICPolarity  = TIM_ICPolarity_Falling;
    TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStruct.TIM_ICFilter    = 0x03;

    TIM_ICInit(
        IR_RECV_TIM,
        &TIM_ICInitStruct
    );

    /* 清空计数器 */
    TIM_SetCounter(
        IR_RECV_TIM,
        0
    );

    /* 清除可能残留的中断标志 */
    TIM_ClearITPendingBit(
        IR_RECV_TIM,
        TIM_IT_Update | TIM_IT_CC1
    );

    /* 开启输入捕获中断和更新溢出中断 */
    TIM_ITConfig(
        IR_RECV_TIM,
        TIM_IT_Update | TIM_IT_CC1,
        ENABLE
    );

    /* 配置TIM9中断 */
    NVIC_InitStruct.NVIC_IRQChannel =
        TIM1_BRK_TIM9_IRQn;

    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(
        &NVIC_InitStruct
    );

    /* 启动TIM9 */
    TIM_Cmd(
        IR_RECV_TIM,
        ENABLE
    );
}


/**
  * @brief  放弃当前未完成的红外数据帧
  * @param  无
  * @retval 无
  */
static void IR_ResetFrame(void)
{
    IR_Receiving = 0;
    IR_BitCount = 0;
    IR_ShiftData = 0;
}


/**
  * @brief  TIM9输入捕获及更新中断函数
  * @param  无
  * @retval 无
  */
void TIM1_BRK_TIM9_IRQHandler(void)
{
    uint32_t captureValue;

    /*
     * TIM9通道1输入捕获中断。
     */
    if(
        TIM_GetITStatus(
            IR_RECV_TIM,
            TIM_IT_CC1
        ) != RESET
    )
    {
        /*
         * 获取两个下降沿之间的时间。
         * 定时器为1 MHz，因此捕获值单位为us。
         */
        captureValue = TIM_GetCapture1(
            IR_RECV_TIM
        );

        /* 清除输入捕获中断标志 */
        TIM_ClearITPendingBit(
            IR_RECV_TIM,
            TIM_IT_CC1
        );

        /*
         * 输入捕获到来时同时清除溢出标志，
         * 防止本次有效下降沿与超时中断发生冲突。
         */
        TIM_ClearITPendingBit(
            IR_RECV_TIM,
            TIM_IT_Update
        );

        /* 从当前下降沿重新开始计时 */
        TIM_SetCounter(
            IR_RECV_TIM,
            0
        );

        /*
         * 第一次下降沿只有起始作用，
         * 没有上一个下降沿用于计算间隔。
         */
        if(IR_FirstEdge == 0)
        {
            IR_FirstEdge = 1;
            return;
        }

        /*
         * NEC完整引导码。
         * 下降沿间隔约为13.5 ms。
         */
        if(
            captureValue >= IR_LEAD_MIN &&
            captureValue <= IR_LEAD_MAX
        )
        {
            IR_Receiving = 1;
            IR_BitCount = 0;
            IR_ShiftData = 0;
            IR_RepeatCount = 0;

            return;
        }

        /*
         * NEC重复码。
         * 本实验仅记录重复码次数，不重新生成键码。
         */
        if(
            captureValue >= IR_REPEAT_MIN &&
            captureValue <= IR_REPEAT_MAX
        )
        {
            IR_RepeatCount++;

            IR_Receiving = 0;
            IR_BitCount = 0;
            IR_ShiftData = 0;

            return;
        }

        /*
         * 尚未识别到引导码时，
         * 不将普通脉冲作为有效数据位。
         */
        if(IR_Receiving == 0)
        {
            return;
        }

        /*
         * 数据位0，下降沿间隔约为1.12 ms。
         */
        if(
            captureValue >= IR_BIT0_MIN &&
            captureValue <= IR_BIT0_MAX
        )
        {
            /*
             * 当前位为0，对应数据位保持为0。
             * NEC协议低位先发送。
             */
            IR_BitCount++;
        }
        /*
         * 数据位1，下降沿间隔约为2.24 ms。
         */
        else if(
            captureValue >= IR_BIT1_MIN &&
            captureValue <= IR_BIT1_MAX
        )
        {
            /*
             * NEC低位先发送，因此第一个数据位
             * 放在bit0位置。
             */
            IR_ShiftData |= (
                1UL << IR_BitCount
            );

            IR_BitCount++;
        }
        else
        {
            /*
             * 当前脉冲不属于NEC数据0或数据1，
             * 放弃本次未完成的数据帧。
             */
            IR_ResetFrame();
            return;
        }

        /*
         * NEC完整数据部分共32位。
         */
        if(IR_BitCount == 32)
        {
            /*
             * 只有完整接收32位后，
             * 才保存数据并通知主程序解析。
             */
            IR_FrameData = IR_ShiftData;
            IR_FrameReady = 1;

            IR_Receiving = 0;
            IR_BitCount = 0;
            IR_ShiftData = 0;
        }
    }

    /*
     * TIM9更新溢出中断。
     * 连续20 ms没有新的下降沿，认为本次信号已经结束。
     */
    if(
        TIM_GetITStatus(
            IR_RECV_TIM,
            TIM_IT_Update
        ) != RESET
    )
    {
        TIM_ClearITPendingBit(
            IR_RECV_TIM,
            TIM_IT_Update
        );

        /*
         * 超时只清除未完成的接收状态，
         * 不再把残缺数据当作完整数据。
         */
        IR_FirstEdge = 0;
        IR_Receiving = 0;
        IR_BitCount = 0;
        IR_ShiftData = 0;
    }
}


/**
  * @brief  红外键码处理函数
  * @note   使用weak定义，可在其他应用文件中重写
  * @param  addr：红外地址码
  * @param  code：红外按键码
  * @retval 无
  */
__attribute__((weak))
void IR_Rece_Proc(uint16_t addr, uint8_t code)
{
    printf(
        "IRRecv:addr:%u, code:%u\r\n",
        addr,
        code
    );
}


/**
  * @brief  解析一帧完整的NEC红外数据
  * @param  无
  * @retval 无
  */
void IR_Recv(void)
{
    uint32_t frameData;

    uint16_t addr;

    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;

    /*
     * 没有接收到完整32位数据时直接返回。
     */
    if(IR_FrameReady == 0)
    {
        return;
    }

    /*
     * 在关闭中断的短时间内复制数据，
     * 避免复制过程中数据被中断修改。
     */
    __disable_irq();

    frameData = IR_FrameData;
    IR_FrameReady = 0;

    __enable_irq();

    /*
     * NEC数据顺序：
     *
     * byte1：地址码
     * byte2：地址反码或扩展地址高字节
     * byte3：键码
     * byte4：键码反码
     */
    byte1 = (uint8_t)(
        frameData & 0xFF
    );

    byte2 = (uint8_t)(
        (frameData >> 8) & 0xFF
    );

    byte3 = (uint8_t)(
        (frameData >> 16) & 0xFF
    );

    byte4 = (uint8_t)(
        (frameData >> 24) & 0xFF
    );

    /* 输出完整32位原始数据 */
    printf(
        "IRRecv:[%08lX]\r\n",
        (unsigned long)frameData
    );

    /*
     * 校验键码及键码反码。
     * 两个字节按位异或后应为0xFF。
     */
    if(
        (uint8_t)(byte3 ^ byte4) != 0xFF
    )
    {
        printf(
            "IRRecv:code check error\r\n"
        );

        return;
    }

    /*
     * 标准NEC协议：
     * 地址码和地址反码互为反码。
     */
    if(
        (uint8_t)(byte1 ^ byte2) == 0xFF
    )
    {
        addr = byte1;
    }
    else
    {
        /*
         * 扩展NEC协议：
         * 前两个字节组成16位地址码。
         */
        addr = (uint16_t)byte1 |
               ((uint16_t)byte2 << 8);
    }

    IR_Rece_Proc(
        addr,
        byte3
    );
}
