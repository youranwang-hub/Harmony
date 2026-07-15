#include "xpt2046.h"
#include "system_config.h"
#include "stm32f4xx.h"
#include "usart_debug.h"
#include <stdio.h>

#if APP_ENABLE_REAL_TOUCH
#include "delay.h"
#endif

static volatile TouchEvent s_event;

#if APP_ENABLE_REAL_TOUCH

#define XPT2046_CHANNEL_X        0x90U
#define XPT2046_CHANNEL_Y        0xD0U
#define XPT2046_PENIRQ_ACTIVE    0U
#define XPT2046_DEBOUNCE_COUNT   1U
#define TOUCH_LCD_WIDTH          240U
#define TOUCH_LCD_HEIGHT         320U

#define XPT2046_CS_ENABLE()      GPIO_ResetBits(GPIOD, GPIO_Pin_13)
#define XPT2046_CS_DISABLE()     GPIO_SetBits(GPIOD, GPIO_Pin_13)
#define XPT2046_CLK_HIGH()       GPIO_SetBits(GPIOE, GPIO_Pin_0)
#define XPT2046_CLK_LOW()        GPIO_ResetBits(GPIOE, GPIO_Pin_0)
#define XPT2046_MOSI_1()         GPIO_SetBits(GPIOE, GPIO_Pin_2)
#define XPT2046_MOSI_0()         GPIO_ResetBits(GPIOE, GPIO_Pin_2)
#define XPT2046_MISO()           GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3)
#define XPT2046_READ_IRQ()       GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4)

typedef struct {
    int16_t x;
    int16_t y;
} XptCoord;

typedef struct {
    long double A;
    long double B;
    long double C;
    long double D;
    long double E;
    long double F;
} XptFactor;

static const XptFactor s_factor_scan0 = {
    -0.006464L, -0.073259L, 280.358032L,
     0.074878L,  0.002052L,  -6.545977L
};

static uint8_t s_debounce_count;
static uint8_t s_hw_event_sent;

static uint8_t xpt_is_pressed(void)
{
    return (XPT2046_READ_IRQ() == XPT2046_PENIRQ_ACTIVE) ? 1U : 0U;
}

static void xpt2046_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_25MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOE, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOD, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_25MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &gpio);

    XPT2046_CS_DISABLE();
    XPT2046_CLK_LOW();
    XPT2046_MOSI_0();
}

static void xpt_write_cmd(uint8_t cmd)
{
    uint8_t i;

    XPT2046_MOSI_0();
    XPT2046_CLK_LOW();

    for (i = 0U; i < 8U; i++) {
        if ((cmd & (uint8_t)(0x80U >> i)) != 0U) {
            XPT2046_MOSI_1();
        } else {
            XPT2046_MOSI_0();
        }

        delay_us(5U);
        XPT2046_CLK_HIGH();
        delay_us(5U);
        XPT2046_CLK_LOW();
    }
}

static uint16_t xpt_read_cmd(void)
{
    uint8_t i;
    uint16_t value = 0U;

    XPT2046_MOSI_0();
    XPT2046_CLK_HIGH();

    for (i = 0U; i < 12U; i++) {
        XPT2046_CLK_LOW();
        if (XPT2046_MISO() != 0U) {
            value |= (uint16_t)(1U << (11U - i));
        }
        XPT2046_CLK_HIGH();
    }

    return value;
}

static uint16_t xpt_read_adc(uint8_t channel)
{
    uint16_t value;

    XPT2046_CS_ENABLE();
    delay_us(1U);
    xpt_write_cmd(channel);
    value = xpt_read_cmd();
    XPT2046_CS_DISABLE();

    return value;
}

static void xpt_read_adc_xy(uint16_t *x, uint16_t *y)
{
    *x = xpt_read_adc(XPT2046_CHANNEL_X);
    delay_us(1U);
    *y = xpt_read_adc(XPT2046_CHANNEL_Y);
}

static int8_t xpt_filter_xy(XptCoord *coord)
{
    uint8_t i;
    uint8_t count = 0U;
    uint16_t xs[10];
    uint16_t ys[10];
    uint32_t sum_x = 0U;
    uint32_t sum_y = 0U;
    uint16_t min_x;
    uint16_t max_x;
    uint16_t min_y;
    uint16_t max_y;

    while (xpt_is_pressed() && count < 10U) {
        xpt_read_adc_xy(&xs[count], &ys[count]);
        count++;
    }

    if (count < 10U) {
        return -1;
    }

    min_x = max_x = xs[0];
    min_y = max_y = ys[0];

    for (i = 0U; i < 10U; i++) {
        if (xs[i] < min_x) min_x = xs[i];
        if (xs[i] > max_x) max_x = xs[i];
        if (ys[i] < min_y) min_y = ys[i];
        if (ys[i] > max_y) max_y = ys[i];
        sum_x += xs[i];
        sum_y += ys[i];
    }

    coord->x = (int16_t)((sum_x - min_x - max_x) >> 3);
    coord->y = (int16_t)((sum_y - min_y - max_y) >> 3);

    return 0;
}

static uint16_t clamp_coord(int32_t value, uint16_t max)
{
    if (value < 0) {
        return 0U;
    }
    if (value >= (int32_t)max) {
        return (uint16_t)(max - 1U);
    }
    return (uint16_t)value;
}

static int8_t xpt_get_lcd_point(TouchEvent *event)
{
    XptCoord raw;
    int32_t x;
    int32_t y;

    if (xpt_filter_xy(&raw) < 0) {
        return -1;
    }

    x = (int32_t)(s_factor_scan0.A * raw.x +
                  s_factor_scan0.B * raw.y +
                  s_factor_scan0.C);
    y = (int32_t)(s_factor_scan0.D * raw.x +
                  s_factor_scan0.E * raw.y +
                  s_factor_scan0.F);

    event->x = clamp_coord(x, TOUCH_LCD_WIDTH);
    event->y = clamp_coord(y, TOUCH_LCD_HEIGHT);
    event->pressed = 1U;

    return 0;
}

static void xpt_log_touch(const TouchEvent *event)
{
    char text[48];

    snprintf(text, sizeof(text), "[TOUCH] x=%u y=%u\r\n",
             event->x, event->y);
    UART_SendString(USART2, text);
}

static uint8_t xpt_poll_real_touch(TouchEvent *event)
{
    TouchEvent sample;

    if (xpt_is_pressed()) {
        if (s_debounce_count < XPT2046_DEBOUNCE_COUNT) {
            s_debounce_count++;
            return 0U;
        }

        if (xpt_get_lcd_point(&sample) == 0) {
            if (s_hw_event_sent == 0U) {
                *event = sample;
                s_hw_event_sent = 1U;
                xpt_log_touch(event);
                return 1U;
            }
        }

        return 0U;
    }

    s_debounce_count = 0U;
    s_hw_event_sent = 0U;

    return 0U;
}

#endif

void Adapter_Touch_Init(void)
{
    s_event.pressed = 0U;
#if APP_ENABLE_REAL_TOUCH
    s_debounce_count = 0U;
    s_hw_event_sent = 0U;
    xpt2046_gpio_init();
#endif
}

uint8_t Adapter_Touch_GetEvent(TouchEvent *event)
{
    if (event == 0) {
        return 0U;
    }

    if (s_event.pressed != 0U) {
        *event = s_event;
        s_event.pressed = 0U;
        return 1U;
    }

#if APP_ENABLE_REAL_TOUCH
    return xpt_poll_real_touch(event);
#else
    return 0U;
#endif
}

void Adapter_Touch_Inject(uint16_t x, uint16_t y)
{
    s_event.x = x;
    s_event.y = y;
    s_event.pressed = 1U;
}
