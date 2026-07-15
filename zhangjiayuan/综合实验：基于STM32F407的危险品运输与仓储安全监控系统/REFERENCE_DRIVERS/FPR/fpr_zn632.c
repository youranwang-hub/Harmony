#include <string.h>
#include "fpr_zn632.h"
#include "delay.h"

#define CMD_GetImage        0x01
#define CMD_GetEnrollImage  0x29
#define CMD_GenChar         0x02
#define CMD_Match           0x03
#define CMD_Search          0x04
#define CMD_RegModel        0x05
#define CMD_StoreChar       0x06
#define CMD_LoadChar        0x07
#define CMD_UpChar          0x08
#define CMD_DownChar        0x09
#define CMD_UpImage         0x0A
#define CMD_DownImage       0x0B
#define CMD_DeletChar       0x0C
#define CMD_Empty           0x0D
#define CMD_WriteReg        0x0E
#define CMD_ReadSysPara     0x0F
#define CMD_VryPwd          0x13
#define CMD_GetRandomCode   0x14
#define CMD_SetChipAddr     0x15
#define CMD_ReadINFpage     0x16
#define CMD_Port_Control    0x17
#define CMD_HighSpeedSearch 0x1B
#define CMD_ReadIndexTable  0x1F

#define ZN632_DELAY_MS(x) delay_ms(x)

#define ZN632_USART USART1

#define ZN632_BUF_LEN 128

uint32_t ZN632_RxBufLen = 0;
uint8_t ZN632_RxBuf[ZN632_BUF_LEN];
uint8_t g_uExitToChange = 0;

uint8_t ZN632_ADDR[4] = {
    0xFF, 0xFF, 0xFF, 0xFF
};

#define ZN632_INDEX_MAX 240

uint8_t ZN632_INDEX[32] = {0};

void ZN632_GPIO_Init(void);
void ZN632_UART_Init(uint32_t baud);
void ZN632_SendData(uint8_t *data, uint8_t length);
void ZN632_SendHead(void);
void ZN632_ShowError(uint16_t code);
void ZN632_ClearRxBuf(void);
void ZN632_UnsetIndex(uint16_t uIndex);

int16_t ZN632_CheckAck(void);
int8_t FPR_GetImage(void);

static uint16_t FPR_ReadNumber(void);


/**
* @brief  初始化配置
* @param  无
* @retval 无
*/
void FPR_Init(uint32_t baud)
{
    ZN632_GPIO_Init();
    ZN632_UART_Init(baud);
    ZN632_PowerOn();
}


/*
GPIO初始化
*/
void ZN632_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    //电源GPC9 FPR_PWR线电源线
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}


/*
串口初始化
*/
void ZN632_UART_Init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_USART1,
        ENABLE
    );

    //GPIO初始化
    //配置GPA9-Tx GPA10-Rx复用为UART1
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_9 | GPIO_Pin_10;

    GPIO_InitStructure.GPIO_OType =
        GPIO_OType_PP;

    GPIO_InitStructure.GPIO_PuPd =
        GPIO_PuPd_UP;

    GPIO_InitStructure.GPIO_Speed =
        GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Mode =
        GPIO_Mode_AF;

    GPIO_Init(
        GPIOA,
        &GPIO_InitStructure
    );

    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource9,
        GPIO_AF_USART1
    );

    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource10,
        GPIO_AF_USART1
    );

    //配置ZN632_USART工作参数
    USART_InitStructure.USART_BaudRate =
        baud;

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
        ZN632_USART,
        &USART_InitStructure
    );

    USART_ITConfig(
        ZN632_USART,
        USART_IT_RXNE,
        ENABLE
    );

    USART_Cmd(
        ZN632_USART,
        ENABLE
    );

    //UART配置NVIC中断
    NVIC_InitStructure.NVIC_IRQChannel =
        USART1_IRQn;

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        1;

    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        1;

    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(
        &NVIC_InitStructure
    );
}


/*
上电启动
*/
int16_t ZN632_PowerOn(void)
{
    uint32_t i = 0;

    GPIO_ResetBits(
        GPIOC,
        GPIO_Pin_9
    );

    while(1)
    {
        if(ZN632_RxBuf[0] == 0x55)
        {
            ZN632_ClearRxBuf();
            return 0;
        }

        i++;

        if(i > 1000)
        {
            return -1;
        }

        ZN632_DELAY_MS(1);
    }
}


/**
  * @brief  ZN632_USART串口向指纹模块传递数据
  * @param  data;传输的数据
  * @param  length;数据长度
  */
void ZN632_SendData(uint8_t *data, uint8_t length)
{
    uint8_t i = 0;

    for(i = 0; i < length; i++)
    {
        USART_SendData(
            ZN632_USART,
            data[i]
        );

        while(
            USART_GetFlagStatus(
                ZN632_USART,
                USART_FLAG_TXE
            ) == RESET
        )
        {
        }
    }
}


/*
发送命令头
*/
void ZN632_SendHead(void)
{
    uint8_t head[6] = {0};

    head[0] = 0xEF;
    head[1] = 0x01;

    head[2] = ZN632_ADDR[0];
    head[3] = ZN632_ADDR[1];
    head[4] = ZN632_ADDR[2];
    head[5] = ZN632_ADDR[3];

    ZN632_SendData(
        head,
        6
    );
}


/**
* @brief  串口中断服务函数,把接收到的数据写入缓冲区
* @param  None
* @retval None
*/
void USART1_IRQHandler(void)
{
    if(
        USART_GetITStatus(
            USART1,
            USART_IT_RXNE
        ) != RESET
    )
    {
        ZN632_RxBuf[ZN632_RxBufLen++] =
            USART_ReceiveData(USART1);

        USART_ClearITPendingBit(
            USART1,
            USART_IT_RXNE
        );
    }
}


/*
串口清空接收缓冲区
*/
void ZN632_ClearRxBuf(void)
{
    memset(
        ZN632_RxBuf,
        0,
        ZN632_RxBufLen
    );

    ZN632_RxBufLen = 0;
}


/*
检查返回ACK包是否合规
*/
int16_t ZN632_CheckAck(void)
{
    uint16_t i = 0;
    uint16_t temp = 0;
    uint16_t sum = 0;
    uint16_t len = 0;

    //包头检查
    if(
        ZN632_RxBuf[0] != 0xEF ||
        ZN632_RxBuf[1] != 0x01
    )
    {
        return -1;
    }

    //地址码检查
    if(
        ZN632_RxBuf[2] != ZN632_ADDR[0] ||
        ZN632_RxBuf[3] != ZN632_ADDR[1] ||
        ZN632_RxBuf[4] != ZN632_ADDR[2] ||
        ZN632_RxBuf[5] != ZN632_ADDR[3]
    )
    {
        return -1;
    }

    len =
        ZN632_RxBuf[7] << 8 |
        ZN632_RxBuf[8];

    temp = 0;

    for(i = 0; i <= len; i++)
    {
        temp += ZN632_RxBuf[6 + i];
    }

    sum =
        ZN632_RxBuf[9 + len - 2] << 8 |
        ZN632_RxBuf[9 + len - 1];

    if(sum != temp)
    {
        return -1;
    }

    return ZN632_RxBuf[9];
}


/*
验证密码
*/
int16_t ZN632_VryPwd(void)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[10] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x07;
    cmd[3] = CMD_VryPwd;

    cmd[4] = 0;
    cmd[5] = 0;
    cmd[6] = 0;
    cmd[7] = 0;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4] +
        cmd[5] +
        cmd[6] +
        cmd[7];

    cmd[8] = temp >> 8;
    cmd[9] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        10
    );

    ZN632_DELAY_MS(300);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/**
获取指纹索引列表
*/
int16_t ZN632_ReadIndexTable(void)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[7] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x04;
    cmd[3] = CMD_ReadIndexTable;
    cmd[4] = 0;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4];

    cmd[5] = temp >> 8;
    cmd[6] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        7
    );

    ZN632_DELAY_MS(300);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
        goto _END;
    }

    memcpy(
        ZN632_INDEX,
        ZN632_RxBuf + 10,
        32
    );

_END:

    ZN632_ClearRxBuf();

    return ret;
}


/*
获取当前最小的可用空索引值
*/
uint16_t ZN632_GetIndexEmpty(void)
{
    uint16_t index = 0;
    uint16_t i = 0;
    uint16_t j = 0;

    for(
        i = 0;
        i < sizeof(ZN632_INDEX);
        i++
    )
    {
        for(j = 0; j < 8; j++)
        {
            if(
                ZN632_INDEX[i] &
                (0x01 << j)
            )
            {
                index++;
            }
            else
            {
                return index;
            }
        }
    }

    return ZN632_INDEX_MAX;
}


/*
统计已经录入的指纹数量
*/
uint16_t ZN632_GetUsedCount(void)
{
    uint16_t index = 0;
    uint16_t count = 0;
    uint8_t uByte = 0;
    uint8_t uBit = 0;

    for(
        index = 0;
        index < ZN632_INDEX_MAX;
        index++
    )
    {
        uByte = index / 8;
        uBit = index % 8;

        if(
            ZN632_INDEX[uByte] &
            (0x01 << uBit)
        )
        {
            count++;
        }
    }

    return count;
}


/*
将模板编号对应的标志位置1，表示已录入
*/
void ZN632_SetIndex(uint16_t uIndex)
{
    uint8_t uByte = uIndex / 8;
    uint8_t uBit = uIndex % 8;

    if(uIndex >= ZN632_INDEX_MAX)
    {
        return;
    }

    ZN632_INDEX[uByte] |=
        0x01 << uBit;
}


/*
清除标识志
*/
void ZN632_UnsetIndex(uint16_t uIndex)
{
    uint8_t uByte = uIndex / 8;
    uint8_t uBit = uIndex % 8;

    if(uIndex >= ZN632_INDEX_MAX)
    {
        return;
    }

    ZN632_INDEX[uByte] &=
        ~(0x01 << uBit);
}


/**
  * @brief   获取图像
  * @param   无
  * @retval  确认码
  */
int16_t ZN632_GetImage(void)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[6] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x03;
    cmd[3] = CMD_GetImage;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3];

    cmd[4] = temp >> 8;
    cmd[5] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        sizeof(cmd)
    );

    ZN632_DELAY_MS(500);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/**
  * @brief  生成指纹特征
  * @param  BufferID
  * @retval 确认码
  */
int16_t ZN632_GenChar(uint8_t BufferID)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[7] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x04;
    cmd[3] = CMD_GenChar;
    cmd[4] = BufferID;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4];

    cmd[5] = temp >> 8;
    cmd[6] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        sizeof(cmd)
    );

    ZN632_DELAY_MS(500);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/**
  * @brief  合成指纹模板
  * @param  无
  * @retval 确认码
  */
int16_t ZN632_RegModel(void)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[6] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x03;
    cmd[3] = CMD_RegModel;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3];

    cmd[4] = temp >> 8;
    cmd[5] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        6
    );

    ZN632_DELAY_MS(500);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/**
  * @brief  储存指纹模板
  * @param  BufferID
  * @param  PageID
  * @retval 确认码
  */
int16_t ZN632_StoreChar(
    uint8_t BufferID,
    uint16_t PageID
)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[9] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x06;
    cmd[3] = CMD_StoreChar;
    cmd[4] = BufferID;
    cmd[5] = PageID >> 8;
    cmd[6] = PageID;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4] +
        cmd[5] +
        cmd[6];

    cmd[7] = temp >> 8;
    cmd[8] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        sizeof(cmd)
    );

    ZN632_DELAY_MS(300);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/**
  * @brief  高速搜索指纹库
  * @param  BufferID
  * @param  pID
  * @param  pScore
  * @retval 确认码
  */
int16_t ZN632_HighSpeedSearch(
    uint8_t BufferID,
    uint16_t *pID,
    uint16_t *pScore
)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[11] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x08;
    cmd[3] = CMD_HighSpeedSearch;
    cmd[4] = BufferID;

    cmd[5] = 0;
    cmd[6] = 0;

    cmd[7] =
        ZN632_INDEX_MAX >> 8;

    cmd[8] =
        ZN632_INDEX_MAX;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4] +
        cmd[5] +
        cmd[6] +
        cmd[7] +
        cmd[8];

    cmd[9] = temp >> 8;
    cmd[10] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        sizeof(cmd)
    );

    ZN632_DELAY_MS(300);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
        goto _END;
    }

    *pID =
        ZN632_RxBuf[10] << 8 |
        ZN632_RxBuf[11];

    *pScore =
        ZN632_RxBuf[12] << 8 |
        ZN632_RxBuf[13];

_END:

    ZN632_ClearRxBuf();

    return ret;
}


/**
  * @brief  删除指定数量的指纹模板
  * @param  PageID:起始指纹编号
  * @param  N:删除数量
  * @retval 确认码
  */
int16_t ZN632_DeletChar(
    uint16_t PageID,
    uint16_t N
)
{
    uint16_t temp = 0;
    int16_t ret = 0;
    uint8_t cmd[10] = {0};

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    cmd[2] = 0x07;
    cmd[3] = CMD_DeletChar;

    cmd[4] = PageID >> 8;
    cmd[5] = PageID;

    cmd[6] = N >> 8;
    cmd[7] = N;

    temp =
        cmd[0] +
        cmd[1] +
        cmd[2] +
        cmd[3] +
        cmd[4] +
        cmd[5] +
        cmd[6] +
        cmd[7];

    cmd[8] = temp >> 8;
    cmd[9] = temp;

    ZN632_SendHead();

    ZN632_SendData(
        cmd,
        sizeof(cmd)
    );

    ZN632_DELAY_MS(300);

    ret = ZN632_CheckAck();

    if(ret != 0)
    {
        ZN632_ShowError(ret);
    }

    ZN632_ClearRxBuf();

    return ret;
}


/*
阻塞获取手指图像
*/
int8_t FPR_GetImage(void)
{
    while(1)
    {
        if(ZN632_GetImage() == 0)
        {
            printf(
                "Get One Finger\r\n"
            );

            return 0;
        }

        if(g_uExitToChange == 1)
        {
            g_uExitToChange = 0;
            return -1;
        }
    }
}


/**
  * @brief  录入指纹
  * @param  无
  * @retval 无
  */
void FPR_AddFinger(void)
{
    int16_t ret = 0;
    uint16_t uID = 0;
    uint16_t uScore = 0;
    uint8_t uStep = 1;

    while(uStep <= 2)
    {
        printf(
            "Put Finger On Senser:%d\r\n",
            uStep
        );

        ret = FPR_GetImage();

        if(ret != 0)
        {
            return;
        }

        ret = ZN632_GenChar(uStep);

        if(ret != 0)
        {
            continue;
        }

        ret = ZN632_HighSpeedSearch(
            uStep,
            &uID,
            &uScore
        );

        if(ret == 0)
        {
            printf(
                "Finger is Already registed\r\n"
            );

            continue;
        }

        uStep++;
    }

    ret = ZN632_RegModel();

    if(ret != 0)
    {
        return;
    }

    uID = ZN632_GetIndexEmpty();

    if(uID >= ZN632_INDEX_MAX)
    {
        printf(
            "Finger Library Full\r\n"
        );

        return;
    }

    ret = ZN632_StoreChar(
        2,
        uID
    );

    if(ret != 0)
    {
        return;
    }

    ZN632_SetIndex(uID);

    printf(
        "Add Finger OK,ID:%d\r\n",
        uID
    );
}


/**
  * @brief  比对指纹
  * @param  无
  * @retval 无
  */
void FPR_MatchFinger(void)
{
    uint16_t uID = 0;
    uint16_t uScore = 0;
    int16_t temp = 0;

    printf(
        "Put Finger On Sensor:\r\n"
    );

    temp = FPR_GetImage();

    if(temp != 0x00)
    {
        return;
    }

    temp = ZN632_GenChar(1);

    if(temp != 0)
    {
        return;
    }

    temp = ZN632_HighSpeedSearch(
        1,
        &uID,
        &uScore
    );

    if(temp != 0)
    {
        printf(
            "Finger NOT Match\r\n"
        );

        return;
    }

    /*
    拓展：显示匹配编号和匹配分数
    */
    printf(
        "Finger Match, ID:%d, Score:%d\r\n",
        uID,
        uScore
    );
}


/*
显示指纹库容量信息
*/
void FPR_ShowLibraryInfo(void)
{
    uint16_t used = 0;
    uint16_t empty = 0;
    int16_t ret = 0;

    /*
    重新读取模块中的索引表，
    避免只使用旧的内存数据
    */
    ret = ZN632_ReadIndexTable();

    if(ret != 0)
    {
        printf(
            "Read Finger Library Failed\r\n"
        );

        return;
    }

    used = ZN632_GetUsedCount();
    empty = ZN632_INDEX_MAX - used;

    printf(
        "Finger Library Information\r\n"
    );

    printf(
        "Registered:%d\r\n",
        used
    );

    printf(
        "Empty:%d\r\n",
        empty
    );

    printf(
        "Total:%d\r\n",
        ZN632_INDEX_MAX
    );
}


/*
从调试串口读取一个十进制整数
以回车结束
*/
static uint16_t FPR_ReadNumber(void)
{
    char ch = 0;
    uint16_t value = 0;
    uint8_t started = 0;

    while(1)
    {
        ch = getchar();

        if(
            ch >= '0' &&
            ch <= '9'
        )
        {
            started = 1;

            value =
                value * 10 +
                ch - '0';

            printf(
                "%c",
                ch
            );
        }
        else if(
            (
                ch == '\r' ||
                ch == '\n'
            ) &&
            started == 1
        )
        {
            printf("\r\n");
            break;
        }
    }

    return value;
}


/*
删除指定编号的指纹
*/
void FPR_DeleteFinger(void)
{
    uint16_t uID = 0;
    uint8_t uByte = 0;
    uint8_t uBit = 0;
    int16_t ret = 0;

    printf(
        "Input Finger ID(0-239), End With Enter:\r\n"
    );

    uID = FPR_ReadNumber();

    if(uID >= ZN632_INDEX_MAX)
    {
        printf(
            "Finger ID Invalid\r\n"
        );

        return;
    }

    uByte = uID / 8;
    uBit = uID % 8;

    /*
    检查当前索引是否已录入
    */
    if(
        (
            ZN632_INDEX[uByte] &
            (0x01 << uBit)
        ) == 0
    )
    {
        printf(
            "Finger ID is Empty\r\n"
        );

        return;
    }

    /*
    从uID开始删除1个模板
    */
    ret = ZN632_DeletChar(
        uID,
        1
    );

    if(ret != 0)
    {
        printf(
            "Delete Finger Failed\r\n"
        );

        return;
    }

    ZN632_UnsetIndex(uID);

    printf(
        "Delete Finger OK,ID:%d\r\n",
        uID
    );
}


/**
  * @brief  显示错误信息
  * @param  code:确认码
  * @retval 无
  */
void ZN632_ShowError(uint16_t code)
{
#if 0

    switch(code)
    {
        case 0x00:
            printf("OK\r\n");
            break;

        case 0x01:
            printf("Data Packet Error\r\n");
            break;

        case 0x02:
            printf("No Finger\r\n");
            break;

        case 0x03:
            printf("Get Image Failed\r\n");
            break;

        case 0x06:
            printf("Generate Feature Failed\r\n");
            break;

        case 0x07:
            printf("Feature Too Few\r\n");
            break;

        case 0x09:
            printf("Finger Not Found\r\n");
            break;

        case 0x0A:
            printf("Merge Feature Failed\r\n");
            break;

        case 0x0B:
            printf("Finger ID Out Of Range\r\n");
            break;

        case 0x10:
            printf("Delete Template Failed\r\n");
            break;

        case 0x18:
            printf("Flash Error\r\n");
            break;

        case 0x1F:
            printf("Finger Library Full\r\n");
            break;

        default:
            printf(
                "ZN632 Error Code:%d\r\n",
                code
            );
            break;
    }

#endif
}
