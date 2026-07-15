#include <stdio.h>
#include "stm32f4xx.h"
#include "mq.h"
#include "usart.h"
#include "delay.h"

/*
 * 每次正式读取MQ2时的ADC采样次数。
 * 采样10次，去掉最大值和最小值后求平均。
 */
#define MQ_FILTER_SAMPLE_COUNT       10

/*
 * 基线校准参数。
 * 共采集30组数据，每组之间间隔50ms。
 */
#define MQ_BASELINE_GROUP_COUNT      30
#define MQ_BASELINE_INTERVAL_MS      50

/*
 * 上电预热时间。
 * 实验中先等待10秒，再进行基线采集。
 */
#define MQ_WARM_UP_SECONDS           10

/*
 * 状态判断阈值。
 *
 * 当前值相对基线变化小于20%：
 * NORMAL
 *
 * 当前值相对基线变化达到20%，但小于50%：
 * WARNING
 *
 * 当前值相对基线变化达到50%及以上：
 * DANGER
 */
#define MQ_WARNING_PERCENT           20
#define MQ_DANGER_PERCENT            50

/*
 * 新状态必须连续出现3次才正式切换，
 * 防止ADC瞬时波动造成状态频繁变化。
 */
#define MQ_STATE_CONFIRM_COUNT       3

/**
  * @brief  MQ2检测状态
  */
typedef enum
{
    MQ_STATE_NORMAL = 0,
    MQ_STATE_WARNING,
    MQ_STATE_DANGER
} MQ_StateTypeDef;

/**
  * @brief  计算当前ADC值相对基线的偏差百分比
  * @param  currentValue：当前ADC值
  * @param  baselineValue：基线ADC值
  * @retval 相对偏差百分比
  */
static uint16_t MQ_GetChangePercent(
    uint16_t currentValue,
    uint16_t baselineValue
)
{
    int32_t difference;
    uint32_t percent;

    if (baselineValue == 0)
    {
        return 0;
    }

    difference =
        (int32_t)currentValue -
        (int32_t)baselineValue;

    /*
     * 取绝对值，使ADC升高或降低都可以检测到。
     */
    if (difference < 0)
    {
        difference = -difference;
    }

    percent =
        ((uint32_t)difference * 100U)
        / baselineValue;

    if (percent > 999U)
    {
        percent = 999U;
    }

    return (uint16_t)percent;
}

/**
  * @brief  根据偏差百分比确定检测状态
  * @param  changePercent：相对基线偏差百分比
  * @retval MQ2状态
  */
static MQ_StateTypeDef MQ_DetectState(
    uint16_t changePercent
)
{
    if (changePercent >= MQ_DANGER_PERCENT)
    {
        return MQ_STATE_DANGER;
    }
    else if (changePercent >= MQ_WARNING_PERCENT)
    {
        return MQ_STATE_WARNING;
    }
    else
    {
        return MQ_STATE_NORMAL;
    }
}

/**
  * @brief  将状态转换为字符串
  * @param  state：MQ2检测状态
  * @retval 状态字符串
  */
static const char *MQ_GetStateString(
    MQ_StateTypeDef state
)
{
    switch (state)
    {
        case MQ_STATE_WARNING:
            return "WARNING";

        case MQ_STATE_DANGER:
            return "DANGER";

        case MQ_STATE_NORMAL:
        default:
            return "NORMAL";
    }
}

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    uint8_t i;

    uint16_t adcValue;
    uint16_t voltageMv;
    uint16_t baselineValue;
    uint16_t changePercent;

    MQ_StateTypeDef detectedState;
    MQ_StateTypeDef stableState;
    MQ_StateTypeDef candidateState;

    uint8_t candidateCount;

    /*
     * 初始化串口2。
     * 波特率为115200。
     */
    UART2_Init(115200);

    /* 初始化MQ2使用的GPIO和ADC */
    MQ_Init();

    printf("\r\n");
    printf("================================\r\n");
    printf("MQ2 Extended Test Start\r\n");
    printf("Filter + Baseline + State Judge\r\n");
    printf("================================\r\n");

    /*
     * MQ2传感器内部带有加热元件。
     * 上电后先进行短时间预热，使数据相对稳定。
     */
    printf("MQ2 warming up...\r\n");

    for (i = MQ_WARM_UP_SECONDS; i > 0; i--)
    {
        printf(
            "Warm-up remaining: %u s\r\n",
            (unsigned int)i
        );

        delay_ms(1000);
    }

    /*
     * 在正常空气环境中进行基线采集。
     * 校准期间不要对传感器哈气，
     * 也不要让明显气味靠近传感器。
     */
    printf("\r\n");
    printf("Baseline calibration start.\r\n");
    printf("Keep the sensor in clean air.\r\n");

    baselineValue = MQ_CalibrateBaseline(
        MQ_BASELINE_GROUP_COUNT,
        MQ_BASELINE_INTERVAL_MS
    );

    /*
     * 防止因硬件异常得到0基线值。
     */
    if (baselineValue == 0)
    {
        baselineValue = 1;
    }

    voltageMv =
        MQ_ADCToMillivolt(baselineValue);

    printf(
        "Baseline calibration completed.\r\n"
    );

    printf(
        "Baseline AD=%04u, V=%u.%03uV\r\n",
        (unsigned int)baselineValue,
        (unsigned int)(voltageMv / 1000),
        (unsigned int)(voltageMv % 1000)
    );

    printf("\r\n");
    printf(
        "WARNING threshold: %u%%\r\n",
        MQ_WARNING_PERCENT
    );

    printf(
        "DANGER threshold : %u%%\r\n",
        MQ_DANGER_PERCENT
    );

    printf(
        "State confirm count: %u\r\n",
        MQ_STATE_CONFIRM_COUNT
    );

    printf("\r\n");

    /* 初始化状态变量 */
    stableState =
        MQ_STATE_NORMAL;

    candidateState =
        MQ_STATE_NORMAL;

    candidateCount = 0;

    while (1)
    {
        /*
         * 读取滤波后的ADC值。
         */
        adcValue = MQ_ReadAverage(
            MQ_FILTER_SAMPLE_COUNT
        );

        /*
         * 将ADC值转换为毫伏。
         */
        voltageMv =
            MQ_ADCToMillivolt(adcValue);

        /*
         * 计算当前值与基线值之间的偏差百分比。
         */
        changePercent =
            MQ_GetChangePercent(
                adcValue,
                baselineValue
            );

        /*
         * 根据偏差百分比得到本次检测状态。
         */
        detectedState =
            MQ_DetectState(
                changePercent
            );

        /*
         * 状态连续确认处理。
         *
         * 新状态需要连续出现指定次数，
         * 才会替换当前稳定状态。
         */
        if (detectedState == stableState)
        {
            /*
             * 检测状态与当前稳定状态相同，
             * 清除候选状态计数。
             */
            candidateState = stableState;
            candidateCount = 0;
        }
        else
        {
            if (detectedState == candidateState)
            {
                /*
                 * 新状态连续出现，
                 * 候选状态计数加1。
                 */
                candidateCount++;
            }
            else
            {
                /*
                 * 出现另一个新状态，
                 * 重新开始计数。
                 */
                candidateState = detectedState;
                candidateCount = 1;
            }

            /*
             * 候选状态连续出现3次，
             * 正式更新稳定状态。
             */
            if (
                candidateCount >=
                MQ_STATE_CONFIRM_COUNT
            )
            {
                stableState = candidateState;
                candidateCount = 0;

                printf(
                    "State changed to %s\r\n",
                    MQ_GetStateString(
                        stableState
                    )
                );
            }
        }

        /*
         * 输出当前检测数据。
         *
         * 使用整数形式输出电压，
         * 不依赖printf浮点数支持。
         */
        printf(
            "AD=%04u, "
            "V=%u.%03uV, "
            "Base=%04u, "
            "Change=%u%%, "
            "State=%s\r\n",

            (unsigned int)adcValue,

            (unsigned int)(
                voltageMv / 1000
            ),

            (unsigned int)(
                voltageMv % 1000
            ),

            (unsigned int)baselineValue,

            (unsigned int)changePercent,

            MQ_GetStateString(
                stableState
            )
        );

        /* 每隔1秒更新一次检测结果 */
        delay_ms(1000);
    }
}
