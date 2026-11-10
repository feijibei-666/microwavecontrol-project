#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/* 腔体内部 LED 灯：PC13，低电平点亮 */
#define LED_PORT    GPIOC
#define LED_PIN     GPIO_Pin_13

#define LED_ON()    GPIO_ResetBits(LED_PORT, LED_PIN)   /* 点亮 */
#define LED_OFF()   GPIO_SetBits(LED_PORT, LED_PIN)     /* 熄灭 */

void LED_Init(void);

#endif
