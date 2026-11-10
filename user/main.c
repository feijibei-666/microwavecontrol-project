#include "led.h"
#include "delay.h"
#include "key.h"
#include "app_rtos.h"

int main(void)
{
    /* Illuminate before scheduler/display initialization. */
    LED_Init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();
    KEY_Init();
    AppRTOS_Start();
    for (;;) { /* Scheduler failure is handled by RTOS_Fatal(). */ }
}
