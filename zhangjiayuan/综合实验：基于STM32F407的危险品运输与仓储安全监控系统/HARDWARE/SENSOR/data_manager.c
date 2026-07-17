#include "data_manager.h"
#include "adc_sensor.h"
#include "dac_output.h"
#include "system_config.h"

static GasDataManager s_gas;
static uint8_t s_ramp_active = 0;
static uint16_t s_ramp_start = 0;
static uint16_t s_ramp_end = 0;
static uint16_t s_ramp_total = 1;
static uint16_t s_ramp_elapsed = 0;

static uint16_t clamp_gas(uint16_t value)
{
    return (value > APP_ADC_GAS_MAX) ? APP_ADC_GAS_MAX : value;
}

void App_Gas_Init(GasDataSource source)
{
    s_gas.source = source;
    s_gas.mq2_real = 0U;
    s_gas.potentiometer = 0U;
    s_gas.dac_loop = 0U;
    s_gas.uart_value = 320;
    s_gas.scene_value = 320;
    s_gas.effective_value = 320U;
    Board_DAC_SetGasValue(0);
    App_Gas_Update();
}

void App_Gas_SetSource(GasDataSource source)
{
    s_gas.source = source;
}

GasDataSource App_Gas_GetSource(void)
{
    return s_gas.source;
}

const char *App_Gas_SourceName(GasDataSource source)
{
    switch (source) {
    case GAS_SOURCE_REAL_MQ2: return "REAL_MQ2";
    case GAS_SOURCE_POT: return "POT";
    case GAS_SOURCE_DAC_LOOP: return "DAC_LOOP";
    case GAS_SOURCE_SCENE: return "SCENE";
    case GAS_SOURCE_UART: return "UART";
    default: return "UNKNOWN";
    }
}

void App_Gas_Update(void)
{
    uint16_t adc_loop;
    s_gas.mq2_real = clamp_gas(Board_ADC_ReadGasReal());
    s_gas.potentiometer = clamp_gas(Board_ADC_ReadPot());
    adc_loop = Board_ADC_ReadDacLoop();
#if APP_DAC_LOOP_FALLBACK_TO_SETPOINT
    if (adc_loop < 20U && Board_DAC_GetLastGasValue() > 20U) {
        adc_loop = Board_DAC_GetLastGasValue();
    }
#endif
    s_gas.dac_loop = clamp_gas(adc_loop);

    switch (s_gas.source) {
    case GAS_SOURCE_REAL_MQ2:
        s_gas.effective_value = s_gas.mq2_real;
        break;
    case GAS_SOURCE_POT:
        s_gas.effective_value = s_gas.potentiometer;
        break;
    case GAS_SOURCE_DAC_LOOP:
        s_gas.effective_value = s_gas.dac_loop;
        break;
    case GAS_SOURCE_SCENE:
        s_gas.effective_value = s_gas.scene_value;
        break;
    case GAS_SOURCE_UART:
        s_gas.effective_value = s_gas.uart_value;
        break;
    default:
        s_gas.effective_value = s_gas.mq2_real;
        break;
    }
    s_gas.effective_value = clamp_gas(s_gas.effective_value);
}

const GasDataManager *App_Gas_Get(void)
{
    return &s_gas;
}

void App_Gas_SetUartValue(uint16_t value)
{
    s_gas.uart_value = clamp_gas(value);
    s_gas.source = GAS_SOURCE_UART;
}

void App_Gas_StartUartRamp(uint16_t start, uint16_t end, uint16_t seconds)
{
    start = clamp_gas(start);
    end = clamp_gas(end);
    if (seconds == 0U) seconds = 1U;
    s_ramp_start = start;
    s_ramp_end = end;
    s_ramp_total = seconds;
    s_ramp_elapsed = 0;
    s_ramp_active = 1U;
    s_gas.source = GAS_SOURCE_UART;
    s_gas.uart_value = start;
}

void App_Gas_Task1000ms(void)
{
    int32_t delta;
    if (!s_ramp_active) return;
    if (s_ramp_elapsed >= s_ramp_total) {
        s_gas.uart_value = s_ramp_end;
        s_ramp_active = 0U;
        return;
    }
    s_ramp_elapsed++;
    delta = (int32_t)s_ramp_end - (int32_t)s_ramp_start;
    s_gas.uart_value = (uint16_t)((int32_t)s_ramp_start + (delta * s_ramp_elapsed) / s_ramp_total);
}

void App_Gas_SetSceneValue(uint16_t value)
{
    s_gas.scene_value = clamp_gas(value);
}
