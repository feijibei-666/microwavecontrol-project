#include "led.h"
#include "delay.h"
#include "key.h"
#include "app_rtos.h"
#include "heater.h"
#include "buzzer.h"
#include "outputs.h"

int main(void)
{
    /* Illuminate before scheduler/display initialization. */
    LED_Init();
    /* 先建立继电器低电平和蜂鸣器静音，避免等待屏幕或调度器时输出悬空。 */
    Heater_Init();
    Buzzer_Init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    delay_init();
    KEY_Init();
    /* 双舵机在调度器启动前进入开门、停转状态，本阶段加热始终关闭。 */
    Outputs_Init();
    AppRTOS_Start();
    for (;;) { /* Scheduler failure is handled by RTOS_Fatal(). */ }
}
