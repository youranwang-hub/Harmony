#include "stm32f4xx.h"
#include <string.h>

/* DMA接收缓冲区最大长度 */
#define RX_BUFFER_SIZE       128

/* DMA发送缓冲区最大长度 */
#define TX_BUFFER_SIZE       128

/*
 * 串口允许输入的最大重复次数。
 * 防止误输入过大的数字导致串口长时间发送。
 */
#define MAX_REPEAT_COUNT     1000


 /* DMA接收缓冲区 */
uint8_t RxBuf[RX_BUFFER_SIZE];

/* DMA发送缓冲区 */
uint8_t TxBuf[TX_BUFFER_SIZE];

/* DMA发送状态：0为空闲，1为正在发送 */
volatile uint8_t DMA_TxBusy = 0;

/* 当前一帧数据的发送长度 */
volatile uint16_t DMA_TxLength = 0;

/* 当前剩余发送次数 */
volatile uint16_t DMA_TxRepeatRemain = 0;

/* 当前发送数据的地址 */
uint8_t* volatile DMA_TxBuffer = 0;

/* 用于观察CPU仍可执行其他任务 */
volatile uint32_t CPU_TaskCounter = 0;


/* 函数声明 */
void USART2_Init(uint32_t baudrate);
void USART2_DMA_RX_Init(void);
void USART2_DMA_TX_Init(void);

uint8_t USART2_DMA_SendRepeat(
    uint8_t* buffer,
    uint16_t length,
    uint16_t repeatCount
);

static void USART2_DMA_TX_Start(void);

static uint8_t USART2_ParseCommand(
    uint8_t* input,
    uint16_t inputLength,
    uint16_t* repeatCount,
    uint16_t* dataOffset,
    uint16_t* dataLength
);


/**
  * @brief  主函数
  */
int main(void)
{
    /* 配置中断优先级分组 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 初始化USART2，波特率115200 */
    USART2_Init(115200);

    /* 初始化DMA接收 */
    USART2_DMA_RX_Init();

    /* 初始化DMA发送 */
    USART2_DMA_TX_Init();

    /* 开启USART2的DMA接收请求 */
    USART_DMACmd(
        USART2,
        USART_DMAReq_Rx,
        ENABLE
    );

    /* 开启USART2的DMA发送请求 */
    USART_DMACmd(
        USART2,
        USART_DMAReq_Tx,
        ENABLE
    );

    /*
     * 开启USART2空闲中断。
     * 用于判断一帧不定长数据接收结束。
     */
    USART_ITConfig(
        USART2,
        USART_IT_IDLE,
        ENABLE
    );

    while (1)
    {
        /*
         * 串口数据收发由DMA完成。
         * CPU仍然可以执行其他任务。
         */
        CPU_TaskCounter++;
    }
}


/**
  * @brief  初始化USART2
  * @param  baudrate：串口波特率
  */
void USART2_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 开启GPIOA时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    /* 开启USART2时钟 */
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_USART2,
        ENABLE
    );

    /*
     * USART2引脚：
     * PA2：USART2_TX
     * PA3：USART2_RX
     */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_2 | GPIO_Pin_3;

    GPIO_InitStructure.GPIO_Mode =
        GPIO_Mode_AF;

    GPIO_InitStructure.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_OType =
        GPIO_OType_PP;

    GPIO_InitStructure.GPIO_PuPd =
        GPIO_PuPd_UP;

    GPIO_Init(
        GPIOA,
        &GPIO_InitStructure
    );

    /* PA2映射为USART2_TX */
    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource2,
        GPIO_AF_USART2
    );

    /* PA3映射为USART2_RX */
    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource3,
        GPIO_AF_USART2
    );

    /* 配置USART2通信参数 */
    USART_InitStructure.USART_BaudRate =
        baudrate;

    USART_InitStructure.USART_WordLength =
        USART_WordLength_8b;

    USART_InitStructure.USART_StopBits =
        USART_StopBits_1;

    USART_InitStructure.USART_Parity =
        USART_Parity_No;

    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;

    USART_InitStructure.USART_Mode =
        USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(
        USART2,
        &USART_InitStructure
    );

    /* 配置USART2中断 */
    NVIC_InitStructure.NVIC_IRQChannel =
        USART2_IRQn;

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        1;

    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        1;

    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(
        &NVIC_InitStructure
    );

    /* 使能USART2 */
    USART_Cmd(
        USART2,
        ENABLE
    );
}


/**
  * @brief  初始化USART2的DMA接收
  *
  * USART2_RX：
  * DMA1 Stream5 Channel4
  */
void USART2_DMA_RX_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* 开启DMA1时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_DMA1,
        ENABLE
    );

    /* 配置前关闭DMA1 Stream5 */
    DMA_Cmd(
        DMA1_Stream5,
        DISABLE
    );

    /* 等待DMA数据流完全关闭 */
    while (
        DMA_GetCmdStatus(DMA1_Stream5)
        != DISABLE
        )
    {
    }

    /* 恢复DMA1 Stream5默认状态 */
    DMA_DeInit(DMA1_Stream5);

    /* 清除DMA接收流原有标志 */
    DMA_ClearFlag(
        DMA1_Stream5,
        DMA_FLAG_FEIF5 |
        DMA_FLAG_DMEIF5 |
        DMA_FLAG_TEIF5 |
        DMA_FLAG_HTIF5 |
        DMA_FLAG_TCIF5
    );

    /* USART2_RX对应DMA通道4 */
    DMA_InitStructure.DMA_Channel =
        DMA_Channel_4;

    /* 外设地址为USART2数据寄存器 */
    DMA_InitStructure.DMA_PeripheralBaseAddr =
        (uint32_t)&USART2->DR;

    /* 存储器地址为接收缓冲区 */
    DMA_InitStructure.DMA_Memory0BaseAddr =
        (uint32_t)RxBuf;

    /* 数据方向：外设到存储器 */
    DMA_InitStructure.DMA_DIR =
        DMA_DIR_PeripheralToMemory;

    /* 最大接收长度 */
    DMA_InitStructure.DMA_BufferSize =
        RX_BUFFER_SIZE;

    /* 外设地址不递增 */
    DMA_InitStructure.DMA_PeripheralInc =
        DMA_PeripheralInc_Disable;

    /* 存储器地址递增 */
    DMA_InitStructure.DMA_MemoryInc =
        DMA_MemoryInc_Enable;

    /* 外设数据宽度为8位 */
    DMA_InitStructure.DMA_PeripheralDataSize =
        DMA_PeripheralDataSize_Byte;

    /* 存储器数据宽度为8位 */
    DMA_InitStructure.DMA_MemoryDataSize =
        DMA_MemoryDataSize_Byte;

    /* 普通模式 */
    DMA_InitStructure.DMA_Mode =
        DMA_Mode_Normal;

    /* 高优先级 */
    DMA_InitStructure.DMA_Priority =
        DMA_Priority_High;

    /* 不使用FIFO */
    DMA_InitStructure.DMA_FIFOMode =
        DMA_FIFOMode_Disable;

    DMA_InitStructure.DMA_FIFOThreshold =
        DMA_FIFOThreshold_Full;

    /* 单次突发传输 */
    DMA_InitStructure.DMA_MemoryBurst =
        DMA_MemoryBurst_Single;

    DMA_InitStructure.DMA_PeripheralBurst =
        DMA_PeripheralBurst_Single;

    /* 初始化DMA1 Stream5 */
    DMA_Init(
        DMA1_Stream5,
        &DMA_InitStructure
    );

    /* 开启DMA接收 */
    DMA_Cmd(
        DMA1_Stream5,
        ENABLE
    );
}


/**
  * @brief  初始化USART2的DMA发送
  *
  * USART2_TX：
  * DMA1 Stream6 Channel4
  */
void USART2_DMA_TX_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 开启DMA1时钟 */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_DMA1,
        ENABLE
    );

    /* 配置前关闭DMA1 Stream6 */
    DMA_Cmd(
        DMA1_Stream6,
        DISABLE
    );

    /* 等待DMA数据流完全关闭 */
    while (
        DMA_GetCmdStatus(DMA1_Stream6)
        != DISABLE
        )
    {
    }

    /* 恢复默认状态 */
    DMA_DeInit(DMA1_Stream6);

    /* 清除DMA发送流原有标志 */
    DMA_ClearFlag(
        DMA1_Stream6,
        DMA_FLAG_FEIF6 |
        DMA_FLAG_DMEIF6 |
        DMA_FLAG_TEIF6 |
        DMA_FLAG_HTIF6 |
        DMA_FLAG_TCIF6
    );

    /* USART2_TX对应DMA通道4 */
    DMA_InitStructure.DMA_Channel =
        DMA_Channel_4;

    /* 外设地址为USART2数据寄存器 */
    DMA_InitStructure.DMA_PeripheralBaseAddr =
        (uint32_t)&USART2->DR;

    /* 存储器地址为发送缓冲区 */
    DMA_InitStructure.DMA_Memory0BaseAddr =
        (uint32_t)TxBuf;

    /* 数据方向：存储器到外设 */
    DMA_InitStructure.DMA_DIR =
        DMA_DIR_MemoryToPeripheral;

    /*
     * 初始化时暂时设置为1。
     * 实际发送前会重新设置发送长度。
     */
    DMA_InitStructure.DMA_BufferSize =
        1;

    /* 外设地址不递增 */
    DMA_InitStructure.DMA_PeripheralInc =
        DMA_PeripheralInc_Disable;

    /* 存储器地址递增 */
    DMA_InitStructure.DMA_MemoryInc =
        DMA_MemoryInc_Enable;

    /* 外设数据宽度8位 */
    DMA_InitStructure.DMA_PeripheralDataSize =
        DMA_PeripheralDataSize_Byte;

    /* 存储器数据宽度8位 */
    DMA_InitStructure.DMA_MemoryDataSize =
        DMA_MemoryDataSize_Byte;

    /* 普通模式 */
    DMA_InitStructure.DMA_Mode =
        DMA_Mode_Normal;

    /* 中等优先级 */
    DMA_InitStructure.DMA_Priority =
        DMA_Priority_Medium;

    /* 不使用FIFO */
    DMA_InitStructure.DMA_FIFOMode =
        DMA_FIFOMode_Disable;

    DMA_InitStructure.DMA_FIFOThreshold =
        DMA_FIFOThreshold_Full;

    /* 单次突发传输 */
    DMA_InitStructure.DMA_MemoryBurst =
        DMA_MemoryBurst_Single;

    DMA_InitStructure.DMA_PeripheralBurst =
        DMA_PeripheralBurst_Single;

    /* 初始化DMA1 Stream6 */
    DMA_Init(
        DMA1_Stream6,
        &DMA_InitStructure
    );

    /* 开启发送完成中断和传输错误中断 */
    DMA_ITConfig(
        DMA1_Stream6,
        DMA_IT_TC | DMA_IT_TE,
        ENABLE
    );

    /* 配置DMA1 Stream6中断 */
    NVIC_InitStructure.NVIC_IRQChannel =
        DMA1_Stream6_IRQn;

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        1;

    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        0;

    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(
        &NVIC_InitStructure
    );
}


/**
  * @brief  启动一次DMA发送
  *
  * 每调用一次，完整发送一遍当前数据。
  */
static void USART2_DMA_TX_Start(void)
{
    /* 关闭DMA发送流 */
    DMA_Cmd(
        DMA1_Stream6,
        DISABLE
    );

    /* 等待DMA发送流关闭 */
    while (
        DMA_GetCmdStatus(DMA1_Stream6)
        != DISABLE
        )
    {
    }

    /* 清除DMA发送状态标志 */
    DMA_ClearFlag(
        DMA1_Stream6,
        DMA_FLAG_FEIF6 |
        DMA_FLAG_DMEIF6 |
        DMA_FLAG_TEIF6 |
        DMA_FLAG_HTIF6 |
        DMA_FLAG_TCIF6
    );

    /* 设置当前发送数据地址 */
    DMA1_Stream6->M0AR =
        (uint32_t)DMA_TxBuffer;

    /* 设置当前发送数据长度 */
    DMA_SetCurrDataCounter(
        DMA1_Stream6,
        DMA_TxLength
    );

    /* 启动DMA发送 */
    DMA_Cmd(
        DMA1_Stream6,
        ENABLE
    );
}


/**
  * @brief  通过DMA指定次数重复发送
  * @param  buffer：待发送数据地址
  * @param  length：单次发送长度
  * @param  repeatCount：重复发送总次数
  * @retval 1：启动成功
  * @retval 0：参数错误或DMA正忙
  */
uint8_t USART2_DMA_SendRepeat(
    uint8_t* buffer,
    uint16_t length,
    uint16_t repeatCount
)
{
    /* 判断参数是否合法 */
    if (
        buffer == 0 ||
        length == 0 ||
        repeatCount == 0
        )
    {
        return 0;
    }

    /* DMA正在发送，不能启动新任务 */
    if (DMA_TxBusy != 0)
    {
        return 0;
    }

    /* 保存本次发送参数 */
    DMA_TxBuffer = buffer;
    DMA_TxLength = length;
    DMA_TxRepeatRemain = repeatCount;

    /* 标记DMA正在发送 */
    DMA_TxBusy = 1;

    /* 启动第一次发送 */
    USART2_DMA_TX_Start();

    return 1;
}


/**
  * @brief  解析串口命令
  *
  * 命令格式：
  * 次数:内容
  *
  * 例如：
  * 5:Hello DMA
  *
  * @retval 1：命令格式正确
  * @retval 0：命令格式错误
  */
static uint8_t USART2_ParseCommand(
    uint8_t* input,
    uint16_t inputLength,
    uint16_t* repeatCount,
    uint16_t* dataOffset,
    uint16_t* dataLength
)
{
    uint16_t i;
    uint16_t commandEnd;
    uint16_t separatorPosition;
    uint32_t number;

    /* 去除命令末尾的回车和换行 */
    commandEnd = inputLength;

    while (commandEnd > 0)
    {
        if (
            input[commandEnd - 1] == '\r' ||
            input[commandEnd - 1] == '\n'
            )
        {
            commandEnd--;
        }
        else
        {
            break;
        }
    }

    /* 命令为空 */
    if (commandEnd == 0)
    {
        return 0;
    }

    /* 查找英文冒号分隔符 */
    separatorPosition = 0xFFFF;

    for (i = 0; i < commandEnd; i++)
    {
        if (input[i] == ':')
        {
            separatorPosition = i;
            break;
        }
    }

    /*
     * 没有找到冒号，或冒号前后没有有效内容。
     */
    if (
        separatorPosition == 0xFFFF ||
        separatorPosition == 0 ||
        separatorPosition + 1 >= commandEnd
        )
    {
        return 0;
    }

    /* 将冒号前面的数字字符转换成重复次数 */
    number = 0;

    for (i = 0; i < separatorPosition; i++)
    {
        /* 重复次数中只能出现数字 */
        if (
            input[i] < '0' ||
            input[i] > '9'
            )
        {
            return 0;
        }

        number =
            number * 10 +
            (input[i] - '0');

        /* 超过允许的最大重复次数 */
        if (number > MAX_REPEAT_COUNT)
        {
            return 0;
        }
    }

    /* 重复次数不能为0 */
    if (number == 0)
    {
        return 0;
    }

    /* 保存解析出的重复次数 */
    *repeatCount = (uint16_t)number;

    /* 保存发送内容的起始位置 */
    *dataOffset =
        separatorPosition + 1;

    /* 计算发送内容长度 */
    *dataLength =
        commandEnd -
        separatorPosition -
        1;

    /*
     * 发送内容后会自动添加\r\n两个字节，
     * 因此内容长度最多为TX_BUFFER_SIZE - 2。
     */
    if (
        *dataLength == 0 ||
        *dataLength > TX_BUFFER_SIZE - 2
        )
    {
        return 0;
    }

    return 1;
}


/**
  * @brief  USART2中断服务函数
  *
  * 利用USART2空闲中断判断一帧串口数据接收完成。
  */
void USART2_IRQHandler(void)
{
    uint16_t receiveLength;
    uint16_t repeatCount;
    uint16_t dataOffset;
    uint16_t dataLength;
    uint16_t sendLength;

    uint8_t commandValid;

    volatile uint32_t temporary;

    /* 判断是否发生USART2空闲中断 */
    if (
        USART_GetITStatus(
            USART2,
            USART_IT_IDLE
        ) != RESET
        )
    {
        /*
         * 清除IDLE标志：
         * 先读取SR寄存器，再读取DR寄存器。
         */
        temporary = USART2->SR;
        temporary = USART2->DR;
        (void)temporary;

        /* 暂停DMA接收 */
        DMA_Cmd(
            DMA1_Stream5,
            DISABLE
        );

        /* 等待DMA接收流完全关闭 */
        while (
            DMA_GetCmdStatus(DMA1_Stream5)
            != DISABLE
            )
        {
        }

        /*
         * 实际接收长度 =
         * 缓冲区总长度 - DMA当前剩余数量
         */
        receiveLength =
            RX_BUFFER_SIZE -
            DMA_GetCurrDataCounter(
                DMA1_Stream5
            );

        commandValid = 0;
        repeatCount = 0;
        dataOffset = 0;
        dataLength = 0;
        sendLength = 0;

        /*
         * 只有DMA发送空闲时才处理新的命令。
         * 重复发送未完成时输入的新命令将被忽略。
         */
        if (
            receiveLength > 0 &&
            DMA_TxBusy == 0
            )
        {
            commandValid =
                USART2_ParseCommand(
                    RxBuf,
                    receiveLength,
                    &repeatCount,
                    &dataOffset,
                    &dataLength
                );

            if (commandValid != 0)
            {
                /* 将命令中的内容复制到发送缓冲区 */
                memcpy(
                    TxBuf,
                    &RxBuf[dataOffset],
                    dataLength
                );

                /*
                 * 每次发送内容末尾自动添加回车换行，
                 * 使重复内容分行显示。
                 */
                TxBuf[dataLength] = '\r';
                TxBuf[dataLength + 1] = '\n';

                sendLength =
                    dataLength + 2;
            }
        }

        /* 清除DMA接收流全部状态标志 */
        DMA_ClearFlag(
            DMA1_Stream5,
            DMA_FLAG_FEIF5 |
            DMA_FLAG_DMEIF5 |
            DMA_FLAG_TEIF5 |
            DMA_FLAG_HTIF5 |
            DMA_FLAG_TCIF5
        );

        /* 恢复DMA接收缓冲区地址 */
        DMA1_Stream5->M0AR =
            (uint32_t)RxBuf;

        /* 恢复DMA最大接收长度 */
        DMA_SetCurrDataCounter(
            DMA1_Stream5,
            RX_BUFFER_SIZE
        );

        /* 重新启动DMA接收 */
        DMA_Cmd(
            DMA1_Stream5,
            ENABLE
        );

        /* 命令正确时，按照输入次数重复发送 */
        if (commandValid != 0)
        {
            USART2_DMA_SendRepeat(
                TxBuf,
                sendLength,
                repeatCount
            );
        }
    }
}


/**
  * @brief  DMA1 Stream6中断服务函数
  *
  * 每完成一次发送，判断是否还需要继续重复发送。
  */
void DMA1_Stream6_IRQHandler(void)
{
    /* 判断是否发生DMA传输错误 */
    if (
        DMA_GetITStatus(
            DMA1_Stream6,
            DMA_IT_TEIF6
        ) != RESET
        )
    {
        /* 清除DMA传输错误标志 */
        DMA_ClearITPendingBit(
            DMA1_Stream6,
            DMA_IT_TEIF6
        );

        /* 关闭DMA发送流 */
        DMA_Cmd(
            DMA1_Stream6,
            DISABLE
        );

        /* 清除发送状态 */
        DMA_TxRepeatRemain = 0;
        DMA_TxBusy = 0;
        DMA_TxBuffer = 0;

        return;
    }

    /* 判断一次DMA发送是否完成 */
    if (
        DMA_GetITStatus(
            DMA1_Stream6,
            DMA_IT_TCIF6
        ) != RESET
        )
    {
        /* 清除发送完成中断标志 */
        DMA_ClearITPendingBit(
            DMA1_Stream6,
            DMA_IT_TCIF6
        );

        /* 关闭当前DMA发送流 */
        DMA_Cmd(
            DMA1_Stream6,
            DISABLE
        );

        /* 等待DMA流完全关闭 */
        while (
            DMA_GetCmdStatus(DMA1_Stream6)
            != DISABLE
            )
        {
        }

        /* 当前一次发送完成，剩余次数减1 */
        if (DMA_TxRepeatRemain > 0)
        {
            DMA_TxRepeatRemain--;
        }

        /* 仍有剩余次数，重新启动DMA */
        if (DMA_TxRepeatRemain > 0)
        {
            USART2_DMA_TX_Start();
        }
        else
        {
            /* 全部发送完成 */
            DMA_TxBusy = 0;
            DMA_TxBuffer = 0;
        }
    }
}