#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "nfc_pn532.h"
#include "usart.h"
#include "delay.h"

#define TEST_SIZE       16

/*
使用同一张S50卡内部的两个普通数据块
块0通常存放厂商信息和UID
块3是扇区控制块
因此本实验选择块1和块2
*/
#define TEST_BLOCK_1    1
#define TEST_BLOCK_2    2

/*
运行模式：
1：写入两个数据块，并立即读取验证
0：只读取两个数据块，验证掉电保存
*/
#define NFC_TEST_MODE   1

/*
第1块数据缓冲区
*/
uint8_t Block1Write[TEST_SIZE] = {0};
uint8_t Block1Read[TEST_SIZE]  = {0};

/*
第2块数据缓冲区
*/
uint8_t Block2Write[TEST_SIZE] = {0};
uint8_t Block2Read[TEST_SIZE]  = {0};


/*
生成两个数据块的测试数据

第1块写入：
0x10～0x1F

第2块写入：
0x20～0x2F
*/
void make_test_data(void)
{
    uint32_t i = 0;

    for (i = 0; i < TEST_SIZE; i++)
    {
        Block1Write[i] = 0x10 + i;
        Block2Write[i] = 0x20 + i;
    }
}


/*
显示当前操作卡片的4字节UID
*/
void print_card_uid(char *operation)
{
    printf(
        "%s Card UID: %02X %02X %02X %02X\r\n",
        operation,
        UID[0],
        UID[1],
        UID[2],
        UID[3]
    );
}


/*
显示指定数据块中的16字节数据

参数：
block     数据块编号
operation 数据类型说明
buf       数据缓冲区
*/
void print_block_data(
    uint8_t block,
    char *operation,
    uint8_t *buf
)
{
    uint32_t i = 0;

    printf(
        "Block %d %s Data:\r\n",
        block,
        operation
    );

    for (i = 0; i < TEST_SIZE; i++)
    {
        printf("0x%02X ", buf[i]);

        /*
        每16字节换行
        */
        if (i % 16 == 15)
        {
            printf("\r\n");
        }
    }
}


/*
清空两个读取缓冲区
*/
void clear_read_buffer(void)
{
    memset(
        Block1Read,
        0,
        sizeof(Block1Read)
    );

    memset(
        Block2Read,
        0,
        sizeof(Block2Read)
    );
}


/*
比较指定数据块的写入数据和读取数据

返回值：
0  数据完全一致
-1 数据不一致
*/
int8_t check_block_data(
    uint8_t block,
    uint8_t *write_buf,
    uint8_t *read_buf
)
{
    uint32_t i = 0;

    for (i = 0; i < TEST_SIZE; i++)
    {
        if (write_buf[i] != read_buf[i])
        {
            printf(
                "Block %d Data Error at byte %d\r\n",
                block,
                i
            );

            printf(
                "Expected: 0x%02X, Read: 0x%02X\r\n",
                write_buf[i],
                read_buf[i]
            );

            return -1;
        }
    }

    printf(
        "Block %d TEST OK\r\n",
        block
    );

    return 0;
}


int main(void)
{
    int8_t block1_result = 0;
    int8_t block2_result = 0;

    /*
    初始化UART2调试串口
    */
    UART2_Init(115200);

    /*
    初始化PN532使用的UART1
    */
    NFC_Init(115200);

    printf("NFC Init OK\r\n");

    /*
    唤醒PN532并配置普通读写器模式
    */
    NFC_WakeUp();

    /*
    生成两个数据块的预期测试数据
    */
    make_test_data();


#if NFC_TEST_MODE == 1

    /*
    模式1：
    写入两个数据块，并立即读回验证
    */
    printf("\r\n");
    printf("================================\r\n");
    printf("NFC Multi-Block WRITE Mode\r\n");
    printf("================================\r\n");

    /*
    显示准备写入第1块的数据
    */
    print_block_data(
        TEST_BLOCK_1,
        "Write",
        Block1Write
    );

    /*
    显示准备写入第2块的数据
    */
    print_block_data(
        TEST_BLOCK_2,
        "Write",
        Block2Write
    );

    printf(
        "Please keep the S50 card on PN532\r\n"
    );

    /*
    向同一张卡的第1块写入16字节
    */
    if (
        NFC_Write(
            TEST_BLOCK_1,
            Block1Write
        ) < 0
    )
    {
        printf("Block 1 Write Error\r\n");

        while (1)
        {
        }
    }

    /*
    NFC_Write内部会重新寻卡，
    寻卡成功后UID保存在UID数组中
    */
    print_card_uid("Write Block 1");

    printf("Block 1 Write OK\r\n");


    /*
    向同一张卡的第2块写入16字节
    */
    if (
        NFC_Write(
            TEST_BLOCK_2,
            Block2Write
        ) < 0
    )
    {
        printf("Block 2 Write Error\r\n");

        while (1)
        {
        }
    }

    print_card_uid("Write Block 2");

    printf("Block 2 Write OK\r\n");


    /*
    写入完成后等待5秒
    等待期间保持卡片不动
    */
    printf(
        "Wait 5 seconds before reading\r\n"
    );

    delay_ms(5000);


    /*
    读取之前清空接收数据缓冲区
    */
    clear_read_buffer();


    /*
    读取同一张卡的第1块
    */
    if (
        NFC_Read(
            TEST_BLOCK_1,
            Block1Read
        ) < 0
    )
    {
        printf("Block 1 Read Error\r\n");

        while (1)
        {
        }
    }

    print_card_uid("Read Block 1");

    printf("Block 1 Read OK\r\n");


    /*
    读取同一张卡的第2块
    */
    if (
        NFC_Read(
            TEST_BLOCK_2,
            Block2Read
        ) < 0
    )
    {
        printf("Block 2 Read Error\r\n");

        while (1)
        {
        }
    }

    print_card_uid("Read Block 2");

    printf("Block 2 Read OK\r\n");


    /*
    显示第1块读取结果
    */
    print_block_data(
        TEST_BLOCK_1,
        "Read",
        Block1Read
    );

    /*
    显示第2块读取结果
    */
    print_block_data(
        TEST_BLOCK_2,
        "Read",
        Block2Read
    );


    /*
    分别比较两个数据块
    */
    block1_result = check_block_data(
        TEST_BLOCK_1,
        Block1Write,
        Block1Read
    );

    block2_result = check_block_data(
        TEST_BLOCK_2,
        Block2Write,
        Block2Read
    );


    /*
    两个数据块都完全一致，
    说明双块读写测试成功
    */
    if (
        block1_result == 0 &&
        block2_result == 0
    )
    {
        printf("ALL BLOCKS TEST OK\r\n");

        printf("\r\n");
        printf(
            "Remove the card and power off the board\r\n"
        );

        printf(
            "Change NFC_TEST_MODE to 0\r\n"
        );

        printf(
            "Rebuild and download the program\r\n"
        );

        printf(
            "Then use the same card for power-off test\r\n"
        );
    }
    else
    {
        printf("MULTI-BLOCK TEST ERROR\r\n");
    }


    /*
    第一阶段测试结束后停止
    */
    while (1)
    {
    }


#else

    /*
    模式0：
    不执行写卡，只读取之前保存的数据
    */
    printf("\r\n");
    printf("================================\r\n");
    printf("NFC Power-Off READ Mode\r\n");
    printf("================================\r\n");

    printf(
        "This mode will not write any data\r\n"
    );

    printf(
        "Please put the same S50 card on PN532\r\n"
    );


    /*
    清空读取缓冲区
    */
    clear_read_buffer();


    /*
    只读取第1块
    */
    if (
        NFC_Read(
            TEST_BLOCK_1,
            Block1Read
        ) < 0
    )
    {
        printf("Block 1 Read Error\r\n");

        while (1)
        {
        }
    }

    print_card_uid(
        "Power-Off Read Block 1"
    );

    printf("Block 1 Read OK\r\n");


    /*
    只读取第2块
    */
    if (
        NFC_Read(
            TEST_BLOCK_2,
            Block2Read
        ) < 0
    )
    {
        printf("Block 2 Read Error\r\n");

        while (1)
        {
        }
    }

    print_card_uid(
        "Power-Off Read Block 2"
    );

    printf("Block 2 Read OK\r\n");


    /*
    显示断电后读取出的第1块数据
    */
    print_block_data(
        TEST_BLOCK_1,
        "Stored",
        Block1Read
    );

    /*
    显示断电后读取出的第2块数据
    */
    print_block_data(
        TEST_BLOCK_2,
        "Stored",
        Block2Read
    );


    /*
    将掉电后读取的数据与预期数据比较
    */
    block1_result = check_block_data(
        TEST_BLOCK_1,
        Block1Write,
        Block1Read
    );

    block2_result = check_block_data(
        TEST_BLOCK_2,
        Block2Write,
        Block2Read
    );


    /*
    两个块的数据都没有丢失
    */
    if (
        block1_result == 0 &&
        block2_result == 0
    )
    {
        printf(
            "POWER-OFF STORAGE TEST OK\r\n"
        );

        printf(
            "The card data was retained after power off\r\n"
        );
    }
    else
    {
        printf(
            "POWER-OFF STORAGE TEST ERROR\r\n"
        );
    }


    /*
    第二阶段测试结束后停止
    */
    while (1)
    {
    }

#endif
}
