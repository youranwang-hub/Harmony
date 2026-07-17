#include "stm32f4xx.h"
#include "gps_parser.h"
#include "usart.h"

#define GPS_USART UART4

uint8_t  g_GPS_RxBuf[500];
volatile uint16_t g_GPS_RxBufLen = 0;
volatile uint8_t  g_GPS_RxDataOK = 0; //标志GPS当前帧是否接收完

/*
初始化GPS，主要为GPIO与UART接口
*/
void GPS_Init(uint32_t baud){
    GPIO_InitTypeDef GPIO_InitStructure; 
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;  

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
   
    //USART GPIO初始化
    //配置UART引脚为复用功能
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11;  
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_UART4);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4);

    //配置串口USART模式
    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(GPS_USART, &USART_InitStructure);

    USART_ITConfig(GPS_USART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(GPS_USART, USART_IT_IDLE, ENABLE);
    USART_Cmd(GPS_USART, ENABLE);
    
    //配置NVIC中断
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*
UART4中断处理函数
*/
void UART4_IRQHandler(void){
    uint8_t data;
    volatile uint32_t temp;

    //接收中断
    if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET){
        /*
        必须读取DR寄存器。
        即使上一批数据还没有处理，也需要读出数据，
        防止RXNE中断一直保持。
        */
        data = (uint8_t)USART_ReceiveData(UART4);

        if(g_GPS_RxDataOK == 0){
            /*
            最后保留一个字节用于字符串结束符'\0'
            */
            if(g_GPS_RxBufLen < 499){
                g_GPS_RxBuf[g_GPS_RxBufLen++] = data;
            }
        }
    }
    
    //空闲中断，表示一批数据接收完成
    if(USART_GetITStatus(UART4, USART_IT_IDLE) != RESET){
        /*
        先读SR，再读DR，清除IDLE中断标志
        */
        temp = UART4->SR;
        temp = UART4->DR;
        (void)temp;

        if(g_GPS_RxBufLen > 0){
            /*
            给接收数据补充字符串结束符
            */
            g_GPS_RxBuf[g_GPS_RxBufLen] = '\0';
            g_GPS_RxDataOK = 1;
        }
    }
}

/*
读取并解析GPS数据
*/
void GPS_ReadAndParse(void){
    char *pGpsStart = NULL;
    char *pGpsEnd   = NULL;

#if 0  //For Test
    //char *gps_str = "$GNVTG,5.78,T,,M,0.00,N,0.00,K,D*2C\r\n";
    //char *gps_str = "$GPGSV,4,1,13,194,72,074,43,26,61,222,45,31,61,352,43,32,60,116,47,1*5A\r\n";
    //char *gps_str = "$GNGSA,A,3,22,29,26,25,03,32,31,16,194,193,,,1.35,0.74,1.13,1*0E\r\n";
    //char *gps_str = "$GNGLL,3149.333190,N,11706.911552,E,080237.000,A,D*42\r\n";
    //char *gps_str = "$GNRMC,080237.000,A,3149.333190,N,11706.911552,E,0.00,5.78,221121,,,D,V*06\r\n";
    char *gps_str = "$GNGGA,080237.000,3149.333190,N,11706.911552,E,2,15,0.74,53.489,M,-0.337,M,,*5F\r\n";
    gps_parse(gps_str);
#endif

    //等待接收完一批GPS数据
    while(g_GPS_RxDataOK != 1){}

    /*
    再次保证缓冲区有字符串结束符
    */
    if(g_GPS_RxBufLen >= 500){
        g_GPS_RxBufLen = 499;
    }

    g_GPS_RxBuf[g_GPS_RxBufLen] = '\0';

    printf("================GPS DATA:\r\n");
    printf("%s", g_GPS_RxBuf);

    //依次解析每个语句
    pGpsStart = (char *)g_GPS_RxBuf;

    while(1){
        //查找一条GPS报文的开始位置
        pGpsStart = strpbrk(pGpsStart, "$");

        if(pGpsStart == NULL){
            break;
        }

        //查找当前报文的换行符
        pGpsEnd = strpbrk(pGpsStart, "\n");

        if(pGpsEnd == NULL){
            break;
        }

        /*
        将当前报文末尾的换行符临时改为字符串结束符。

        这样gps_parse()只能解析当前这一条报文，
        不会继续进入下一条GGA、GSA或GSV报文。
        */
        *pGpsEnd = '\0';

        gps_parse(pGpsStart);

        /*
        继续查找下一条报文。
        虽然换行符已经被改成'\0'，
        但pGpsEnd + 1仍然指向下一条报文。
        */
        pGpsStart = pGpsEnd + 1;
    }

    //解析完成，清除接收缓冲区
    memset(g_GPS_RxBuf, 0, sizeof(g_GPS_RxBuf));
    g_GPS_RxBufLen = 0;
    g_GPS_RxDataOK = 0;
}
