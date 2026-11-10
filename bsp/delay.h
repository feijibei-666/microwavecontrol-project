#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

void delay_init(void);          /* Initialize DWT only; never configures SysTick. */
uint32_t delay_millis(void);    /* FreeRTOS tick time in milliseconds. */
void delay_ms(uint16_t nms);    /* Task delay after scheduler start, busy wait before. */
void delay_us(uint32_t nus);    /* DWT cycle-counter busy wait for short intervals. */

#endif
