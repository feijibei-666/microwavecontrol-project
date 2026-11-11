#include "servo.h"

/* PB0 -> TIM3_CH3
   定时器时钟 72MHz，预分频 71 得到 1MHz 计数（1us 一步）
   重装载 19999 -> 周期 20ms（50Hz）
   比较值 500~2500us 对应 0~180 度 */
#define SERVO_TIM       TIM3

void Servo_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    /* 重新初始化也先隔离信号并停止计数，防止沿用上一轮的输出状态。 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    TIM_Cmd(SERVO_TIM, DISABLE);

    /* 时基：72MHz / 72 = 1MHz，20000 计数 = 20ms */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = 19999;
    TIM_TimeBaseStructure.TIM_Prescaler     = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(SERVO_TIM, &TIM_TimeBaseStructure);

    /* 通道3 PWM1 输出 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 500 + SERVO_ANGLE_OPEN * 2000 / 180;
                                                   /* 上电默认 0 度 */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(SERVO_TIM, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(SERVO_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(SERVO_TIM, ENABLE);
    /* 先装载首帧开门脉宽，再交给复用引脚，避免上电输出未配置的PWM。 */
    TIM_GenerateEvent(SERVO_TIM, TIM_EventSource_Update);

    /* PB0 复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    TIM_Cmd(SERVO_TIM, ENABLE);
}

void Servo_SetAngle(uint8_t angle)
{
    uint16_t ccr;

    if (angle > 180)
    {
        angle = 180;
    }

    /* 只更新预装载比较值，下一帧生效；不重置计数器，保持20 ms周期。 */
    ccr = 500 + (uint16_t)(angle * 2000 / 180);     /* 500~2500us */
    TIM_SetCompare3(SERVO_TIM, ccr);
}

