#include "heater.h"

/* 历史PWM比较值仅保留作档位比例记录，当前关闭接口不使用这些值。 */
#define HEATER_LOW      6000    /* 30% */
#define HEATER_MEDIUM   12000   /* 60% */
#define HEATER_HIGH     19999   /* 100% */

void Heater_Init(void)
{
    /* 机械继电器禁止使用原50 Hz PWM；本阶段仅配置安全关闭输出。 */
    Heater_Off();
}

void Heater_SetPower(uint8_t level)
{
    /* 旧接口维持关闭。控制任务通过输出层应用档位时间窗。 */
    (void)level;
    Heater_Off();
}

#if RELAY_PULSE_TEST || RELAY_COUNTDOWN_ENABLED
void Heater_Energize(void)
{
    /* main 已初始化 PA2，置位不会影响同端口的其他引脚。 */
    GPIOA->BSRR = GPIO_Pin_2;
}
#endif

void Heater_Off(void)
{
    uint32_t clock_ready;

    /* 故障可发生在main初始化前；直接开GPIO时钟，不依赖RTOS或驱动就绪标志。 */
    RCC->APB2ENR |= RCC_APB2Periph_GPIOA;
    clock_ready = RCC->APB2ENR;
    (void)clock_ready;
    /* 先预装低电平，再切2 MHz推挽输出；PA2不再连接TIM2_CH3。 */
    GPIOA->BRR = GPIO_Pin_2;
    GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << 8)) | (0x2UL << 8);
}
