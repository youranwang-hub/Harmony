#include "stm32f4xx.h"
#include "fpr_zn632.h"
#include "delay.h"
#include "usart.h"


void FPR_Usage(void)
{
    printf("\r\n");
    printf("Select one\r\n");
    printf("1 Add New Finger\r\n");
    printf("2 Match Finger\r\n");
    printf("3 Show Finger Library\r\n");
    printf("4 Delete Finger By ID\r\n");
}


/*
读取菜单字符，
自动忽略回车和换行
*/
char FPR_GetMenuChoice(void)
{
    char ch = 0;

    do
    {
        ch = getchar();
    }
    while(
        ch == '\r' ||
        ch == '\n'
    );

    return ch;
}


/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    char ch = 0;

    UART2_Init(115200);
    FPR_Init(56700);

    printf(
        "FPR Init OK\r\n"
    );

    if(ZN632_VryPwd() == 0)
    {
        printf(
            "Passwrd OK\r\n"
        );
    }

    if(ZN632_ReadIndexTable() == 0)
    {
        printf(
            "ReadIndexTable OK\r\n"
        );
    }

    while(1)
    {
        FPR_Usage();

        ch = FPR_GetMenuChoice();

        switch(ch)
        {
            case '1':

                FPR_AddFinger();

                break;

            case '2':

                FPR_MatchFinger();

                break;

            case '3':

                FPR_ShowLibraryInfo();

                break;

            case '4':

                FPR_DeleteFinger();

                break;

            default:

                printf(
                    "No this number\r\n"
                );

                break;
        }
    }
}

