#include "dht11.h"
#include "delay.h"
#include "stm32f10x.h"

#define DHT_PIN GPIO_Pin_8
#define DHT_FRAME_BITS 40U

static volatile uint8_t frame[5];
static volatile uint8_t bit_count;
static volatile uint8_t response_seen;
static volatile uint8_t timing_error;

void Dht11_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_ICInitTypeDef capture;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);
    /* 开漏输出写 1 时释放总线，外部上拉建立高电平，MCU 不主动驱动高电平。 */
    GPIO_SetBits(GPIOA, DHT_PIN);
    gpio.GPIO_Pin = DHT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    /* 当前时钟树 APB2 不分频，TIM1 每微秒计数一次。 */
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(SystemCoreClock / 1000000U - 1U);
    TIM_TimeBaseInit(TIM1, &timer);

    /* 同一 TI1 输入由 CH1 锁存上升沿、CH2 锁存下降沿。 */
    TIM_ICStructInit(&capture);
    capture.TIM_Channel = TIM_Channel_1;
    TIM_ICInit(TIM1, &capture);
    capture.TIM_Channel = TIM_Channel_2;
    capture.TIM_ICSelection = TIM_ICSelection_IndirectTI;
    capture.TIM_ICPolarity = TIM_ICPolarity_Falling;
    TIM_ICInit(TIM1, &capture);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    NVIC_SetPriority(TIM1_CC_IRQn, 6);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
}

Dht11Reading Dht11_Read(void)
{
    Dht11Reading reading = {0, 0, DHT11_TIMEOUT};
    uint8_t i;

    GPIO_ResetBits(GPIOA, DHT_PIN);
    /* 毫秒级起始低电平和整帧等待都让出 CPU，控制任务可以继续调度。 */
    delay_ms(20);
    bit_count = 0;
    response_seen = 0;
    timing_error = 0;
    for (i = 0; i < sizeof(frame); i++) { frame[i] = 0; }

    TIM_SetCounter(TIM1, 0);
    TIM1->SR = 0;
    TIM_ITConfig(TIM1, TIM_IT_CC2, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
    GPIO_SetBits(GPIOA, DHT_PIN);
    delay_ms(8);
    /* 先关捕获中断源再读取帧，避免中断继续修改缓冲区。 */
    TIM_ITConfig(TIM1, TIM_IT_CC2, DISABLE);
    TIM_Cmd(TIM1, DISABLE);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);

    if (timing_error)
    {
        reading.status = DHT11_TIMING;
    }
    else if (bit_count == DHT_FRAME_BITS)
    {
        if ((uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]) != frame[4] ||
            frame[0] > 100U)
        {
            reading.status = DHT11_CHECKSUM;
        }
        else
        {
            reading.humidity_percent = frame[0];
            reading.temperature_c = frame[2];
            reading.status = DHT11_OK;
        }
    }
    return reading;
}

void TIM1_CC_IRQHandler(void)
{
    uint16_t flags = TIM1->SR;
    uint16_t rise;
    uint16_t fall;
    uint16_t high_us;
    uint8_t byte;

    if (!(flags & TIM_SR_CC2IF)) { return; }
    /* 读取 CCR 清除各自的捕获标志。ISR 只组帧，不调用 FreeRTOS API。 */
    rise = TIM_GetCapture1(TIM1);
    fall = TIM_GetCapture2(TIM1);
    high_us = (uint16_t)(fall - rise);
    if ((flags & (TIM_SR_CC1IF | TIM_SR_CC1OF | TIM_SR_CC2OF)) != TIM_SR_CC1IF)
    {
        timing_error = 1;
    }
    else if (!response_seen)
    {
        /* 释放总线后的短高电平在响应前出现，只有约 80 us 的响应高脉冲用于同步。 */
        response_seen = (uint8_t)(high_us >= 65U && high_us <= 100U);
    }
    else if ((high_us >= 15U && high_us <= 40U) ||
             (high_us >= 55U && high_us <= 90U))
    {
        byte = bit_count / 8U;
        frame[byte] = (uint8_t)((frame[byte] << 1) | (high_us >= 55U));
        bit_count++;
    }
    else
    {
        timing_error = 1;
    }
    if (timing_error || bit_count == DHT_FRAME_BITS)
    {
        TIM_ITConfig(TIM1, TIM_IT_CC2, DISABLE);
    }
}
