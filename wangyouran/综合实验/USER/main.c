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

/* GPS parsed data (defined in gps_parser.c) */
extern GGA gga;
extern RMC rmc;

/* Thresholds */
#define TEMP_LIMIT   25.0f
#define HUMI_LIMIT   70.0f
#define MQ2_LIMIT    2000

/* E2PROM / FLASH addresses */
#define EE_ADDR_NAME  0
#define FLASH_SECTOR  0

static char g_DriverName[20] = "Wang";

/* LCD helpers */
static void lcd_title(const char *s)
{
    LCD_Clear(0, 0, LCD_GetLenX(), 16);
    LCD_SetColors(RED, BLACK);
    LCD_DispStringEN(0, 0, 0, (char*)s);
    LCD_SetColors(WHITE, BLACK);
}

static void lcd_line(uint8_t row, const char *s)
{
    LCD_DispStringEN(0, LINE_EN(row), 0, (char*)s);
}

/* Helper: getchar that skips leftover \r \n from QCOM line-send */
char uart_getch(void)
{
    char c;
    delay_ms(5);  /* let printf finish */
    do { c = getchar(); } while(c == '\r' || c == '\n');
    return c;
}

/* ===== Menu Functions ===== */

/* Menu 1: Fingerprint */
void menu_fingerprint(void)
{
    uint8_t ch;
    printf("\r\n=== Fingerprint ===\r\n");
    printf("1-Enroll  2-Match  0-Back\r\n");

    /* LCD: show fingerprint mode immediately */
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("Fingerprint");

    ch = uart_getch();
    printf("[%c]\r\n", ch);
    if(ch == '0') return;

    FPR_Init(57600);
    if(ZN632_VryPwd() == 0)
        printf("Password OK\r\n");
    if(ZN632_ReadIndexTable() == 0)
        printf("IndexTable OK\r\n");

    if(ch == '1') {
        lcd_line(2, "Enroll...");
        FPR_AddFinger();
        lcd_line(3, "OK");
    }
    if(ch == '2') {
        lcd_line(2, "Matching...");
        FPR_MatchFinger();
        lcd_line(3, "Done");
    }
    delay_ms(1500);
}

/* Menu 2: GPS — get one fix then return */
void menu_gps(void)
{
    char buf[32];
    uint32_t timeout;

    printf("\r\n=== GPS ===\r\n");
    printf("Acquiring GPS fix (timeout 60s)...\r\n");

    GPS_Init(9600);

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("GPS Tracking");
    lcd_line(2, "Acquiring...");

    /* wait for first valid fix, max 60 seconds (240 x 250ms) */
    for(timeout = 0; timeout < 240; timeout++) {
        GPS_ReadAndParse();
        if(gga.lat != 0.0) break;       /* got a fix */
        delay_ms(250);
    }

    if(gga.lat == 0.0) {
        printf("GPS: No fix (timeout)\r\n");
        lcd_line(2, "No fix");
        delay_ms(2000);
        return;
    }

    /* print one clean fix */
    printf("========================================\r\n");
    printf("  GPS Fix\r\n");
    printf("  Lat : %.4f %c\r\n", gga.lat, gga.lat_dir);
    printf("  Lon : %.4f %c\r\n", gga.lon, gga.lon_dir);
    printf("  Sats: %d   HDOP: %.1f\r\n", gga.sats, gga.hdop);
    printf("  Alt : %.1f m\r\n", gga.alt);
    printf("========================================\r\n");

    /* update LCD */
    LCD_Clear(0, 32, LCD_GetLenX(), LCD_GetLenY() - 32);
    sprintf(buf, "Lat:%.4f %c", gga.lat, gga.lat_dir);
    lcd_line(2, buf);
    sprintf(buf, "Lon:%.4f %c", gga.lon, gga.lon_dir);
    lcd_line(3, buf);
    sprintf(buf, "Sat:%d Alt:%.0fm", gga.sats, gga.alt);
    lcd_line(4, buf);
}

/* Menu 3: Environment Monitor (DHT11 + MQ2) */
void menu_env(void)
{
    DHT11_Data dht;
    uint16_t mq_val;
    RTC_TimeTypeDef RTC_T;
    char buf[32];

    printf("\r\n=== Env Monitor (any key to exit) ===\r\n");

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("Env Monitor");

    while(1) {
        if(DHT11_ReadData(&dht) == 0) {
            printf("T:%d.%dC H:%d.%d%% ",
                dht.temp_int, dht.temp_deci,
                dht.humi_int, dht.humi_deci);

            if(dht.temp_int > TEMP_LIMIT || dht.humi_int > HUMI_LIMIT) {
                printf("<<<ALERT!>>>");
                LED1_ON(); BUZZ_ON();
                lcd_line(2, "ALERT! HIGH T/H");
            } else {
                LED1_OFF(); BUZZ_OFF();
                LCD_Clear(0, 32, LCD_GetLenX(), LCD_GetLenY() - 32);
            }

            sprintf(buf, "T:%dC H:%d%% ", dht.temp_int, dht.humi_int);
            lcd_line(3, buf);
        }

        mq_val = MQ_ReadValue();
        printf("MQ2:%u\r\n", mq_val);
        sprintf(buf, "MQ2:%-4u", mq_val);
        lcd_line(4, buf);

        if(mq_val > MQ2_LIMIT) {
            printf("MQ2 ALARM!\r\n");
            LED2_ON(); BUZZ_ON();
            lcd_line(5, "MQ2 ALARM!");
        } else {
            LED2_OFF();
            if(dht.temp_int <= TEMP_LIMIT) BUZZ_OFF();
        }

        RTC_GetTime(RTC_Format_BIN, &RTC_T);
        sprintf(buf, "%02d:%02d:%02d",
            RTC_T.RTC_Hours, RTC_T.RTC_Minutes, RTC_T.RTC_Seconds);
        lcd_line(6, buf);

        delay_ms(1000);
        if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE)) break;
    }
    LED1_OFF(); LED2_OFF(); BUZZ_OFF();
    uart_getch();
}

/* Menu 4: LCD Refresh */
void menu_lcd(void)
{
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;

    printf("\r\n=== LCD Display ===\r\n");

    RTC_GetTime(RTC_Format_BIN, &RTC_T);
    RTC_GetDate(RTC_Format_BIN, &RTC_D);

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    LCD_SetFontEN(&ASCII_8x16);

    LCD_SetColors(RED, BLACK);
    LCD_DispStringEN(0, 0, 0, "SmartLogi V1.0");

    LCD_SetColors(WHITE, BLACK);
    LCD_DispStringEN(0, LINE_EN(1), 0, g_DriverName);

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

/* Menu 5: IR Sign-off */
void menu_ir(void)
{
    printf("\r\n=== IR Sign-off ===\r\n");
    printf("Press remote key to sign...\r\n");

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("IR Sign-off");
    lcd_line(2, "Press remote...");

    IR_Recv_Init();
    while(1) {
        IR_Recv();
        delay_ms(50);
        if(USART_GetFlagStatus(USART2, USART_FLAG_RXNE)) {
            uart_getch(); break;
        }
    }
}

/* IR callback: sign-off on remote press */
void IR_Rece_Proc(uint16_t addr, uint8_t code)
{
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;
    char log[64];
    static uint32_t flash_offset = 0;

    printf("IR: addr=0x%04X code=0x%02X --> Sign OK!\r\n", addr, code);

    /* LCD: show sign-off confirmation */
    lcd_title("IR Sign-off");
    lcd_line(2, "Sign OK!");

    BUZZ_ON(); LED3_ON();
    delay_ms(200);
    BUZZ_OFF(); LED3_OFF();

    RTC_GetTime(RTC_Format_BIN, &RTC_T);
    RTC_GetDate(RTC_Format_BIN, &RTC_D);
    sprintf(log, "SIGN:20%02d-%02d-%02d %02d:%02d:%02d Key:%02X\r\n",
        RTC_D.RTC_Year, RTC_D.RTC_Month, RTC_D.RTC_Date,
        RTC_T.RTC_Hours, RTC_T.RTC_Minutes, RTC_T.RTC_Seconds,
        code);

    if(flash_offset == 0)
        W25QXX_SectorErase(FLASH_SECTOR * 4096);

    W25QXX_BufferWrite((uint8_t*)log, flash_offset, strlen(log));
    flash_offset += strlen(log);
    printf("FLASH record: %s", log);
}

/* Menu 6: FLASH Dump */
void menu_flash_dump(void)
{
    uint8_t buf[256];
    printf("\r\n=== FLASH Dump ===\r\n");

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("FLASH Dump");
    lcd_line(2, "Reading...");

    W25QXX_BufferRead(buf, 0, sizeof(buf));
    buf[255] = '\0';
    printf("%s\r\n", buf);
    printf("=== Dump Done ===\r\n");

    lcd_line(3, "Done");
}

/* Menu 7: CAN Test */
void menu_can(void)
{
    uint8_t data[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    printf("\r\n=== CAN LoopBack Test ===\r\n");

    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());
    lcd_title("CAN Test");
    lcd_line(2, "LoopBack...");

    CAN1_Init();
    CAN1_SendMsg(0x1234, data, 8);
    delay_ms(100);
    printf("CAN test done\r\n");

    lcd_line(3, "OK");
}

/* Show menu */
void show_menu(void)
{
    printf("\r\n========================================\r\n");
    printf("  SmartLogi Terminal Demo\r\n");
    printf("========================================\r\n");
    printf("1 - Fingerprint\r\n");
    printf("2 - GPS\r\n");
    printf("3 - Env Monitor (DHT11 + MQ2)\r\n");
    printf("4 - LCD Refresh\r\n");
    printf("5 - IR Sign-off\r\n");
    printf("6 - FLASH Dump\r\n");
    printf("7 - CAN Test\r\n");
    printf("0 - Exit\r\n");
    printf("========================================\r\n");
    printf("Select: ");
}

/* ===== Main ===== */
int main(void)
{
    char ch;
    RTC_TimeTypeDef RTC_T;
    RTC_DateTypeDef RTC_D;

    delay_ms(100);
    UART2_Init(115200);
    LED_BUZZ_Init();

    AT24C02_Init();
    W25QXX_Init();
    BSP_RTC_Init();

    AT24C02_BufferRead((uint8_t*)g_DriverName, EE_ADDR_NAME, 16);
    if(g_DriverName[0] == 0xFF || g_DriverName[0] == 0) {
        strcpy(g_DriverName, "Wang");
        AT24C02_BufferWrite((uint8_t*)g_DriverName, EE_ADDR_NAME, 16);
    }

    LCD_Init();
    LCD_SetFontEN(&ASCII_8x16);
    LCD_SetColors(WHITE, BLACK);
    LCD_Clear(0, 0, LCD_GetLenX(), LCD_GetLenY());

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

    DHT11_Init();
    MQ_Init();
    XPT2046_Init();

    printf("\r\n=== SmartLogi System Ready ===\r\n");

    while(1) {
        show_menu();
        ch = uart_getch();
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
                printf("Invalid option\r\n");
                break;
        }
    }
}
