#include "stm32f4xx.h"
#include "app_main.h"

int main(void)
{
    App_Init();
    while (1) {
        App_Run();
    }
}
