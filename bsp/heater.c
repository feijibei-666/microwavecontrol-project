#include "heater.h"

#define HEATER_TIM      TIM2

#define HEATER_LOW      6000    /* 30% */
#define HEATER_MEDIUM   12000   /* 60% */
#define HEATER_HIGH     19999   /* 100% */

void Heater_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = 19999;
    TIM_TimeBaseStructure.TIM_Prescaler     = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(HEATER_TIM, &TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(HEATER_TIM, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(HEATER_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(HEATER_TIM, ENABLE);
    TIM_Cmd(HEATER_TIM, ENABLE);
}

void Heater_SetPower(uint8_t level)
{
    switch (level)
    {
        case 0:
            TIM_SetCompare3(HEATER_TIM, HEATER_LOW);
            break;
        case 1:
            TIM_SetCompare3(HEATER_TIM, HEATER_MEDIUM);
            break;
        default:
            TIM_SetCompare3(HEATER_TIM, HEATER_HIGH);
            break;
    }
}

void Heater_Off(void)
{
    TIM_SetCompare3(HEATER_TIM, 0);
}
