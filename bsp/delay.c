#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

/* The bundled legacy CMSIS header has no DWT structure. Cortex-M3 registers. */
#define DWT_CTRL_REG    (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT_REG  (*(volatile uint32_t *)0xE0001004)
#define DEMCR_REG       (*(volatile uint32_t *)0xE000EDFC)
#define ICSR_REG        (*(volatile uint32_t *)0xE000ED04)

void delay_init(void)
{
    SystemCoreClockUpdate();
    DEMCR_REG |= (1UL << 24);
    DWT_CYCCNT_REG = 0;
    DWT_CTRL_REG |= 1UL;
}

uint32_t delay_millis(void)
{
    return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay_us(uint32_t nus)
{
    uint32_t chunk;
    uint32_t start;
    uint32_t cycles;
    uint32_t cycles_per_us = SystemCoreClock / 1000000;

    while (nus != 0)
    {
        chunk = (nus > 1000) ? 1000 : nus;
        cycles = chunk * cycles_per_us;
        start = DWT_CYCCNT_REG;
        while ((uint32_t)(DWT_CYCCNT_REG - start) < cycles)
        {
            /* Interrupts remain enabled. A preemption may lengthen the wait. */
        }
        nus -= chunk;
    }
}

void delay_ms(uint16_t nms)
{
    if (nms == 0)
    {
        return;
    }
    configASSERT((ICSR_REG & 0x1FFUL) == 0);
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        /* One extra tick prevents an early return when called near a tick edge. */
        vTaskDelay(pdMS_TO_TICKS(nms) + 1);
    }
    else
    {
        while (nms-- != 0)
        {
            delay_us(1000);
        }
    }
}
