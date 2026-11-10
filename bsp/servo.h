#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

/* SG90 舵机：PB0 -> TIM3_CH3，50Hz PWM
   0.5ms 脉宽对应 0 度，2.5ms 对应 180 度 */
#define SERVO_ANGLE_OPEN    0       /* 开门位置（门锁未闭合） */
#define SERVO_ANGLE_CLOSE   90      /* 关门位置 */

void Servo_Init(void);
void Servo_SetAngle(uint8_t angle);

#endif

