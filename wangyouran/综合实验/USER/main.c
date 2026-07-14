#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "usart.h"
#include "delay.h"
#include "led_buzz.h"
#include "rtc.h"
#include "e2prom_at24c02.h"
#include "flash_w25qxx.h"
#include "lcd_ili9341.h"
#include "lcd_fonts.h"
#include "xpt2046.h"
#include "dht11.h"
#include "mq.h"
#include "gps.h"
#include "fpr_zn632.h"
#include "ir_recv.h"
#include "can.h"

/* 阈值定义 */
#define TEMP_LIMIT   25.0f    // 温度上限(故意设低以演示告警)
#define HUMI_LIMIT   70.0f    // 湿度上限
#define MQ2_LIMIT    2000     // MQ2 ADC 告警阈值

/* E2PROM / FLASH 存储地址 */
#define EE_ADDR_NAME  0       // E2PROM 地址: 司机名
#define FLASH_SECTOR  0       // FLASH 扇区: 签收日志

static char g_DriverName[20] = "Wang";  // 默认司机名

/* ==================== 单个菜单功能 ==================== */

/* 菜单 1: 指纹 */
void menu_fingerprint(void)
{
    uint8_t ch;
    printf("\r\n=== 指纹登录 ===\r\n");
    printf("1-录入新指纹  2-匹配验证  0-返回\r\n");
    ch = getchar();
    if(ch == '0') return;

    FPR_Init(57600);                // 切 UART4 给指纹
    if(ZN632_VryPwd() == 0)
        printf("Password OK\r\n");
    if(ZN632_ReadIndexTable() == 0)
        printf("IndexTable OK\r\n");

    if(ch == '1') FPR_AddFinger();
    if(ch == '2') FPR_MatchFinger();
}

/* 菜单 2: GPS */
void menu_gps(void)
{
    uint8_t ch;
    printf("\r\n=== GPS 定位 ===\r\n");
    printf("按任意键开始(GPS冷启动约35s, 请放窗边), q返回\r\n");
    ch = getchar();
    if(ch == 'q') return;

    GPS_Init(9600);                 // 切 UART4 给 GPS
    printf("等待 GPS 数据...\r\n");
    while(1) {
        GPS_ReadAndParse();
        if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE)) {
            ch = USART_ReceiveData(USART2);
            if(ch == 'q') break;
        }
    }
}

/* 菜单 3: 环境监控 (DHT11 + MQ2) */
void menu_env(void)
{
    DHT11_Data dht;
    uint16_t mq_val;
    RTC_TimeTypeDef RTC_T;

    printf("\r\n=== 环境监控 (任意键退出) ===\r\n");
    while(1) {
        /* DHT11 */
        if(DHT11_ReadData(&dht) == 0) {
            printf("T:%d.%dC H:%d.%d%% ",
                dht.temp_int, dht.temp_deci,
                dht.humi_int, dht.humi_deci);

            /* 超温报警 */
            if(dht.temp_int > TEMP_LIMIT || dht.humi_int > HUMI_LIMIT) {
                printf("<<<ALERT!>>>");
                LED1_ON(); BUZZ_ON();
            } else {
                LED1_OFF(); BUZZ_OFF();
            }
        }

        /* MQ2 */
        mq_val = MQ_ReadValue();
        printf("MQ2:%u\r\n", mq_val);
        if(mq_val > MQ2_LIMIT) {
            printf("MQ2 ALARM!\r\n");
            LED2_ON(); BUZZ_ON();
        } else {
            LED2_OFF();
            if(dht.temp_int <= TEMP_LIMIT) BUZZ_OFF();
        }

        /* LCD 刷新 */
        RTC_GetTime(RTC_Format_BIN, &RTC_T);
        LCD_Clear(0, 32, LCD_GetLenX(), LCD_GetLenY() - 32);
        LCD_SetTextColor(WHITE);
        {
            char buf[32];
            sprintf(buf, "T:%dC H:%d%% ", dht.temp_int, dht.humi_int);
            LCD_DispStringEN(0, 32, 0, buf);
            sprintf(buf, "MQ2:%-4u", mq_val);
            LCD_DispStringEN(0, 48, 0, buf);
        }

        delay_ms(1000);
        if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE)) break;
    }
    LED1_OFF(); LED2_OFF(); BUZZ_OFF();
    getchar();  // 吃掉按键
}

/* 菜单 4: LCD 显示综合信息 */
void menu_lcd(void)
{
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;

    printf("\r\n=== LCD 显示 ===\r\n");

    /* 获取 RTC */
    RTC_GetTime(RTC_Format_BIN, &RTC_T);
    RTC_GetDate(RTC_Format_BIN, &RTC_D);

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_SetFontEN(&ASCII_8x16);

    /* 标题 */
    LCD_SetColors(RED, BLACK);
    LCD_DispStringEN(0, 0, 0, "SmartLogi V1.0");

    /* 司机名 */
    LCD_SetColors(WHITE, BLACK);
    LCD_DispStringEN(0, LINE_EN(1), 0, g_DriverName);

    /* 时间 */
    {
        char buf[32];
        sprintf(buf, "%02d:%02d:%02d",
            RTC_T.RTC_Hours, RTC_T.RTC_Minutes, RTC_T.RTC_Seconds);
        LCD_DispStringEN(0, LINE_EN(2), 0, buf);

        sprintf(buf, "20%02d-%02d-%02d",
            RTC_D.RTC_Year, RTC_D.RTC_Month, RTC_D.RTC_Date);
        LCD_DispStringEN(0, LINE_EN(3), 0, buf);
    }
    printf("LCD refreshed\r\n");
}

/* 菜单 5: IR 遥控签收 */
void menu_ir(void)
{
    printf("\r\n=== IR 遥控签收 ===\r\n");
    printf("用遥控器按任意键确认签收...\r\n");

    IR_Recv_Init();
    while(1) {
        IR_Recv();
        delay_ms(50);
        if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE)) {
            getchar(); break;
        }
    }
}

/* IR 回调: 收到遥控器信号触发签收 */
void IR_Rece_Proc(uint16_t addr, uint8_t code)
{
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;
    char log[64];
    static uint32_t flash_offset = 0;       /* 静态变量记录写入偏移, 追加不覆盖 */

    printf("IR: addr=0x%04X code=0x%02X --> 签收确认!\r\n", addr, code);
    BUZZ_ON(); LED3_ON();
    delay_ms(200);
    BUZZ_OFF(); LED3_OFF();

    /* 记录到 FLASH */
    RTC_GetTime(RTC_Format_BIN, &RTC_T);
    RTC_GetDate(RTC_Format_BIN, &RTC_D);
    sprintf(log, "SIGN:20%02d-%02d-%02d %02d:%02d:%02d Key:%02X\r\n",
        RTC_D.RTC_Year, RTC_D.RTC_Month, RTC_D.RTC_Date,
        RTC_T.RTC_Hours, RTC_T.RTC_Minutes, RTC_T.RTC_Seconds,
        code);

    /* 首次写入时擦除扇区 */
    if(flash_offset == 0)
        W25QXX_SectorErase(FLASH_SECTOR * 4096);

    W25QXX_BufferWrite((uint8_t*)log, flash_offset, strlen(log));
    flash_offset += strlen(log);
    printf("FLASH record: %s", log);
}

/* 菜单 6: FLASH 数据导出 */
void menu_flash_dump(void)
{
    uint8_t buf[256];
    printf("\r\n=== FLASH 数据导出 ===\r\n");
    W25QXX_BufferRead(buf, 0, sizeof(buf));
    buf[255] = '\0';
    printf("%s\r\n", buf);
    printf("=== 导出完毕 ===\r\n");
}

/* 菜单 7: CAN 自测 */
void menu_can(void)
{
    uint8_t data[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    printf("\r\n=== CAN LoopBack 自测 ===\r\n");
    CAN1_Init();
    CAN1_SendMsg(0x1234, data, 8);
    delay_ms(100);
    printf("CAN test done\r\n");
}

/* 菜单打印 */
void show_menu(void)
{
    printf("\r\n========================================\r\n");
    printf("  智能物流终端 -- 教室演示版\r\n");
    printf("========================================\r\n");
    printf("1 - 指纹登录\r\n");
    printf("2 - GPS 定位\r\n");
    printf("3 - 环境监控 (DHT11 + MQ2)\r\n");
    printf("4 - LCD 显示刷新\r\n");
    printf("5 - IR 遥控签收\r\n");
    printf("6 - FLASH 数据导出\r\n");
    printf("7 - CAN 自测\r\n");
    printf("0 - 退出\r\n");
    printf("========================================\r\n");
    printf("选择: ");
}

/* ==================== 主函数 ==================== */
int main(void)
{
    char ch;
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;

    /* ---- 系统初始化 ---- */
    delay_ms(100);
    UART2_Init(115200);
    LED_BUZZ_Init();

    /* 存储 */
    AT24C02_Init();
    W25QXX_Init();

    /* RTC */
    BSP_RTC_Init();

    /* 从 E2PROM 读取司机名, 若无则写入默认名 */
    AT24C02_BufferRead((uint8_t*)g_DriverName, EE_ADDR_NAME, 16);
    if(g_DriverName[0] == 0xFF || g_DriverName[0] == 0) {
        strcpy(g_DriverName, "Wang");
        AT24C02_BufferWrite((uint8_t*)g_DriverName, EE_ADDR_NAME, 16);
    }

    /* LCD + 触控 */
    LCD_Init();
    LCD_SetFontEN(&ASCII_8x16);
    LCD_SetColors(WHITE, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());

    /* 显示开机画面 */
    LCD_SetColors(RED, BLACK);
    LCD_DispStringEN(0, 0, 0, "SmartLogi V1.0");
    LCD_SetColors(WHITE, BLACK);
    LCD_DispStringEN(0, LINE_EN(1), 0, g_DriverName);

    RTC_GetTime(RTC_Format_BIN, &RTC_T);
    RTC_GetDate(RTC_Format_BIN, &RTC_D);
    {
        char buf[32];
        sprintf(buf, "%02d:%02d 20%02d-%02d-%02d",
            RTC_T.RTC_Hours, RTC_T.RTC_Minutes,
            RTC_D.RTC_Year, RTC_D.RTC_Month, RTC_D.RTC_Date);
        LCD_DispStringEN(0, LINE_EN(2), 0, buf);
    }

    /* 传感器 */
    DHT11_Init();
    MQ_Init();
    XPT2046_Init();

    /* 触控回调: 轻触 LCD 左侧 → 相当于按'3'进入环境监控 */
    /* (若不需要可屏蔽) */

    printf("\r\n=== SmartLogi System Ready ===\r\n");

    /* ---- 主循环 ---- */
    while(1) {
        show_menu();
        ch = getchar();
        printf("%c\r\n", ch);

        switch(ch) {
            case '1': menu_fingerprint();       break;
            case '2': menu_gps();               break;
            case '3': menu_env();               break;
            case '4': menu_lcd();               break;
            case '5': menu_ir();                break;
            case '6': menu_flash_dump();        break;
            case '7': menu_can();               break;
            case '0':
                printf("System off.\r\n");
                LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
                LCD_DispStringEN(0, 0, 0, "System Off");
                while(1);
            default:
                printf("无效选项\r\n");
                break;
        }
    }
}
