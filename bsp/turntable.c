#include "turntable.h"

/* PA1 -> TIM2_CH2
   ��ʱ��ʱ�� 72MHz��Ԥ��Ƶ 71 �õ� 1MHz ������1us һ��
   ��װ�� 19999 -> ���� 20ms��50Hz��
   ����Ƭ PWM ���� TIM2 �� CH3��PA2����ʱ��������˴�����һ�� */
#define TURNTABLE_TIM   TIM2

/* 本阶段TIM2只服务转盘；PA2继电器改为GPIO，不再初始化或共用TIM2。 */
static volatile uint8_t turntable_ready = 0;

void Turntable_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    turntable_ready = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* 初始化前先让信号保持低；首帧有效PWM必须是停止脉宽。 */
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    TIM_Cmd(TURNTABLE_TIM, DISABLE);

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
    TIM_GenerateEvent(TURNTABLE_TIM, TIM_EventSource_Update);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    TIM_Cmd(TURNTABLE_TIM, ENABLE);
    turntable_ready = 1;
}

void Turntable_Start(void)
{
    TIM_SetCompare2(TURNTABLE_TIM, TURNTABLE_PULSE_RUN);
}

void Turntable_Stop(void)
{
    TIM_SetCompare2(TURNTABLE_TIM, TURNTABLE_PULSE_STOP);
}

void Turntable_EmergencyStop(void)
{
    uint32_t clock_ready;

    if (turntable_ready)
    {
        /* 连续旋转舵机用中位脉宽停转；预装载在下一个20 ms帧边界生效。 */
        TURNTABLE_TIM->CCR2 = TURNTABLE_PULSE_STOP;
    }
    else
    {
        /* 故障可能早于初始化；不启动未知PWM，只将信号固定低电平。 */
        RCC->APB2ENR |= RCC_APB2Periph_GPIOA;
        clock_ready = RCC->APB2ENR;
        (void)clock_ready;
        GPIOA->BRR = GPIO_Pin_1;
        GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << 4)) | (0x2UL << 4);
    }
}
