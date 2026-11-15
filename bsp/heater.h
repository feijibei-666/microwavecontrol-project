#ifndef __HEATER_H
#define __HEATER_H

#include "stm32f10x.h"
#include "relay_test_config.h"

/* ����Ƭ��PA2 -> TIM2_CH3����ת�̹��� TIM2 �� 50Hz ʱ��
   ռ�ձȣ�Low 30% / Medium 60% / High 100% */
/* 上方为历史PWM说明，现已停用。当前PA2普通GPIO，低电平关闭；所有档位均强制关闭。 */
void Heater_Init(void);
void Heater_SetPower(uint8_t level);    /* 0=Low 1=Medium 2=High */
/* 初始化前及中断/故障上下文均可调用，不使用RTOS、延时或堆分配。 */
void Heater_Off(void);

#if RELAY_PULSE_TEST || RELAY_COUNTDOWN_ENABLED
/* 由获准控制输出的构建提供，在 GPIO 初始化后调用。 */
void Heater_Energize(void);
#endif

#endif
