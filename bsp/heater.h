#ifndef __HEATER_H
#define __HEATER_H

#include "stm32f10x.h"

/* ����Ƭ��PA2 -> TIM2_CH3����ת�̹��� TIM2 �� 50Hz ʱ��
   ռ�ձȣ�Low 30% / Medium 60% / High 100% */
void Heater_Init(void);
void Heater_SetPower(uint8_t level);    /* 0=Low 1=Medium 2=High */
void Heater_Off(void);

#endif
