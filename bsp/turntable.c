#include "turntable.h"

/* PA1 -> TIM2_CH2
   ��ʱ��ʱ�� 72MHz��Ԥ��Ƶ 71 �õ� 1MHz ������1us һ��
   ��װ�� 19999 -> ���� 20ms��50Hz��
   ����Ƭ PWM ���� TIM2 �� CH3��PA2����ʱ��������˴�����һ�� */
#define TURNTABLE_TIM   TIM2

void Turntable_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = 19999;
    TIM_TimeBaseStructure.TIM_Prescaler     = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TURNTABLE_TIM, &TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = TURNTABLE_PULSE_STOP;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC2Init(TURNTABLE_TIM, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(TURNTABLE_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TURNTABLE_TIM, ENABLE);
    TIM_Cmd(TURNTABLE_TIM, ENABLE);
}

void Turntable_Start(void)
{
    TIM_SetCompare2(TURNTABLE_TIM, TURNTABLE_PULSE_RUN);
}

void Turntable_Stop(void)
{
    TIM_SetCompare2(TURNTABLE_TIM, TURNTABLE_PULSE_STOP);
}
