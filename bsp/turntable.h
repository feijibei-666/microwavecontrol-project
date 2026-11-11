#ifndef __TURNTABLE_H
#define __TURNTABLE_H

#include "stm32f10x.h"

/* ת�̣�360 ��������ת������ź��� PA1 -> TIM2_CH2��50Hz PWM
   1.5ms = ͣת��1.0ms = �㶨�ٶ���ת
   ���������෴�������������ĳ� 2000 ���ɣ� */
#define TURNTABLE_PULSE_STOP    1500
#define TURNTABLE_PULSE_RUN     1000

void Turntable_Init(void);
void Turntable_Start(void);
void Turntable_Stop(void);
/* 无RTOS、无等待；允许在初始化前或故障中断内调用。 */
void Turntable_EmergencyStop(void);

#endif
