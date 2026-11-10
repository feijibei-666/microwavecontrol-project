#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/* PC13 board LED: low = illuminated, high = off. */
#define LED_PORT    GPIOC
#define LED_PIN     GPIO_Pin_13
#define LED_ON()    GPIO_ResetBits(LED_PORT, LED_PIN)
#define LED_OFF()   GPIO_SetBits(LED_PORT, LED_PIN)

void LED_Init(void);            /* Initialize and illuminate at startup. */

#endif
