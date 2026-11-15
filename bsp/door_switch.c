#include "door_switch.h"
#include "stm32f10x.h"

#define DOOR_PIN GPIO_Pin_10
#define DOOR_CLOSE_SAMPLES 20U

static uint8_t low_samples;
static uint32_t last_sample_ms;

void DoorSwitch_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    /* 关门触点接 GND。开门或开关线路断开时，由输入上拉读高。 */
    gpio.GPIO_Pin = DOOR_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    low_samples = 0;
    last_sample_ms = 0;
}

uint8_t DoorSwitch_Sample(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - last_sample_ms;
    last_sample_ms = now_ms;

    if (GPIO_ReadInputDataBit(GPIOB, DOOR_PIN) == Bit_SET)
    {
        /* 开门第一拍生效，并重新开始下次关门的消抖。 */
        low_samples = 0;
    }
    else if (low_samples < DOOR_CLOSE_SAMPLES)
    {
        /* 只累计不同毫秒的连续样本；任务追赶时不能快速重复计数。 */
        if (elapsed > 1U) { low_samples = 1; }
        else if (elapsed == 1U) { low_samples++; }
    }
    return (uint8_t)(low_samples == DOOR_CLOSE_SAMPLES);
}
