#include <stdio.h>
#include "stm32f4xx.h"
#include "xpt2046.h"
#include "usart.h"
#include "delay.h"

/* 长按检测次数
 * 主循环每50ms检测一次，20次约为1秒
 */
#define LONG_PRESS_COUNT    20

/* 滑动距离判断门限 */
#define SWIPE_THRESHOLD     30

/* 当前是否处于触摸状态 */
static uint8_t touch_active = 0;

/* 是否已经识别为长按 */
static uint8_t long_press_detected = 0;

/* 连续按压检测次数 */
static uint16_t press_count = 0;

/* 单击累计次数 */
static uint32_t tap_count = 0;

/* 记录触摸起点 */
static XPT2046_Coord touch_start = {-1, -1, -1, -1};

/* 记录触摸过程中最后一个坐标 */
static XPT2046_Coord touch_last = {-1, -1, -1, -1};


/**
  * @brief  求16位有符号数绝对值
  * @param  value：待处理数据
  * @retval 绝对值
  */
static int16_t AbsValue(int16_t value)
{
    if(value < 0)
    {
        return -value;
    }

    return value;
}


/**
  * @brief  触摸屏按下处理函数
  * @param  touch：当前触摸坐标
  * @retval 无
  */
void XPT2046_TouchDown(XPT2046_Coord *touch)
{
    if(touch_active == 0)
    {
        /* 第一次检测到有效触摸，记录起点 */
        touch_active = 1;
        long_press_detected = 0;
        press_count = 1;

        touch_start.x = touch->x;
        touch_start.y = touch->y;
        touch_start.pre_x = touch->pre_x;
        touch_start.pre_y = touch->pre_y;

        touch_last.x = touch->x;
        touch_last.y = touch->y;
        touch_last.pre_x = touch->pre_x;
        touch_last.pre_y = touch->pre_y;

        printf("TouchStart,x:%d,y:%d\r\n",
               touch_start.x,
               touch_start.y);
    }
    else
    {
        /* 触摸持续按下，更新最后坐标 */
        press_count++;

        touch_last.x = touch->x;
        touch_last.y = touch->y;
        touch_last.pre_x = touch->pre_x;
        touch_last.pre_y = touch->pre_y;

        /* 持续按压达到门限，判定为长按 */
        if((press_count >= LONG_PRESS_COUNT) &&
           (long_press_detected == 0))
        {
            long_press_detected = 1;

            printf("LongPress,x:%d,y:%d\r\n",
                   touch_last.x,
                   touch_last.y);
        }
    }
}


/**
  * @brief  触摸屏释放处理函数
  * @param  touch：释放前最后一次触摸坐标
  * @retval 无
  */
void XPT2046_TouchUp(XPT2046_Coord *touch)
{
    int16_t dx;
    int16_t dy;
    int16_t abs_dx;
    int16_t abs_dy;

    if(touch_active == 0)
    {
        return;
    }

    /* 保存释放前最后一次有效坐标 */
    touch_last.x = touch->x;
    touch_last.y = touch->y;

    printf("TouchUp,x:%d,y:%d\r\n",
           touch_last.x,
           touch_last.y);

    /* 计算终点和起点的坐标差 */
    dx = touch_last.x - touch_start.x;
    dy = touch_last.y - touch_start.y;

    abs_dx = AbsValue(dx);
    abs_dy = AbsValue(dy);

    if(long_press_detected == 1)
    {
        /* 已经达到长按时间，判定为长按 */
        printf("Result: Long Press\r\n");
    }
    else if((abs_dx < SWIPE_THRESHOLD) &&
            (abs_dy < SWIPE_THRESHOLD))
    {
        /* 坐标移动较小，判定为单击 */
        tap_count++;

        printf("Result: Tap,count:%lu\r\n",
               (unsigned long)tap_count);
    }
    else
    {
        /* 水平方向位移大于垂直方向位移 */
        if(abs_dx >= abs_dy)
        {
            if(dx > 0)
            {
                printf("Result: Swipe Right\r\n");
            }
            else
            {
                printf("Result: Swipe Left\r\n");
            }
        }
        else
        {
            /* 垂直方向位移大于水平方向位移 */
            if(dy > 0)
            {
                printf("Result: Swipe Down\r\n");
            }
            else
            {
                printf("Result: Swipe Up\r\n");
            }
        }
    }

    /* 一次触摸结束，恢复初始状态 */
    touch_active = 0;
    long_press_detected = 0;
    press_count = 0;

    touch_start.x = -1;
    touch_start.y = -1;
    touch_start.pre_x = -1;
    touch_start.pre_y = -1;

    touch_last.x = -1;
    touch_last.y = -1;
    touch_last.pre_x = -1;
    touch_last.pre_y = -1;
}


/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    UART2_Init(115200);   //调试串口，方便打印

    XPT2046_Init();

    printf("XPT2046 EXTEND TEST\r\n");
    printf("Tap LongPress Swipe Test\r\n");

    while(1)
    {
        XPT2046_DetectTouch();

        delay_ms(50);     //延时
    }
}
