#include "buzzer.h"
#include "stm32f10x.h"

#define BUZZER_PORT  GPIOB
#define BUZZER_PIN   GPIO_Pin_15

typedef struct
{
    uint32_t start_tick;
    uint32_t duration_ms;
    uint8_t busy;
    uint8_t output_on;
} BuzzerState;

static BuzzerState buzzer;
static uint8_t gpio_ready;

void Buzzer_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    /* 先预装低电平再切换为输出，避免初始化过程误鸣。 */
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    gpio.GPIO_Pin = BUZZER_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BUZZER_PORT, &gpio);
    gpio_ready = 1;
    buzzer.start_tick = 0;
    buzzer.duration_ms = 0;
    buzzer.busy = 0;
    buzzer.output_on = 0;
}

void Buzzer_Stop(void)
{
    /* 故障可能发生于主程序初始化之前，仍需建立可控的关闭电平。
     * 正常状态只由蜂鸣器任务访问；故障调用后系统不得继续播放。 */
    if (!gpio_ready)
    {
        Buzzer_Init();
    }
    else
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    }
    buzzer.busy = 0;
    buzzer.output_on = 0;
}

void Buzzer_Start(uint8_t pulses, uint32_t now)
{
    if (pulses == 0)
    {
        Buzzer_Stop();
        return;
    }
    if (!gpio_ready)
    {
        Buzzer_Init();
    }
    if (pulses > BUZZER_MAX_PULSES)
    {
        pulses = BUZZER_MAX_PULSES;
    }

    /* 末声结束即关闭，不再附加一个空白间隔；每个新命令重新计时。 */
    buzzer.start_tick = now;
    buzzer.duration_ms = (uint32_t)pulses * BUZZER_ON_MS +
                         (uint32_t)(pulses - 1U) * BUZZER_GAP_MS;
    buzzer.busy = 1;
    buzzer.output_on = 1;
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

void Buzzer_Update(uint32_t now)
{
    uint32_t elapsed;
    uint8_t output_on;

    if (!buzzer.busy)
    {
        return;
    }

    /* 无符号差支持毫秒 Tick 回绕。按起始时刻计算当前相位，任务偶发
     * 延迟时直接跳到正确状态，不补发过期短脉冲，也不逐次累积误差。 */
    elapsed = (uint32_t)(now - buzzer.start_tick);
    if (elapsed >= buzzer.duration_ms)
    {
        Buzzer_Stop();
        return;
    }
    output_on = (uint8_t)((elapsed % (BUZZER_ON_MS + BUZZER_GAP_MS)) <
                          BUZZER_ON_MS);
    if (output_on != buzzer.output_on)
    {
        if (output_on)
        {
            GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
        }
        else
        {
            GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
        }
        buzzer.output_on = output_on;
    }
}

uint8_t Buzzer_IsBusy(void)
{
    return buzzer.busy;
}
