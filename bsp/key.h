#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/* Three-pin modules: VCC=3.3V, OUT=high when pressed, GND=common ground. */
#define KEY1_PORT       GPIOB
#define KEY1_PIN        GPIO_Pin_12
#define KEY1_READ()     GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN)
#define KEY2_PORT       GPIOB
#define KEY2_PIN        GPIO_Pin_1
#define KEY2_READ()     GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN)

#define KEY_NONE         0
#define KEY_SHORT        1
#define KEY_LONG         2
#define KEY_ACTIVE_LEVEL 1
#define KEY_DEBOUNCE_MS  20
#define KEY_LONG_MS      1000

void KEY_Init(void);
void KEY_Tick1ms(void);         /* Call once per millisecond from SysTick. */
void KEY_ClearEvents(void);
uint8_t KEY1_Pressed(void);     /* Compatibility helper: consume a short event. */
uint8_t KEY1_Scan(void);        /* Non-blocking: consume one pending event. */
uint8_t KEY2_Scan(void);

#endif
