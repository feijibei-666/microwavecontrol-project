#include "delay.h"

static uint8_t  fac_us = 0;     /* 1us 需要的 SysTick 计数 */
static uint16_t fac_ms = 0;     /* 1ms 需要的 SysTick 计数 */

/* SysTick 时钟源取 HCLK/8，计数一次即 1us（72MHz 时 HCLK/8 = 9MHz） */
void delay_init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    fac_us = (uint8_t)(SystemCoreClock / 8000000);
    fac_ms = (uint16_t)fac_us * 1000;
}

void delay_us(uint32_t nus)
{
    uint32_t temp;

    SysTick->LOAD = nus * fac_us;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));   /* 等待计数到 0 */

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL   = 0x00;
}

/* 逐毫秒累加，避免 nms 较大时超出 SysTick 的 24 位重装载范围 */
void delay_ms(uint16_t nms)
{
    uint32_t temp;

    SysTick->LOAD = fac_ms;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    while (nms--)
    {
        do
        {
            temp = SysTick->CTRL;
        } while ((temp & 0x01) && !(temp & (1 << 16)));
        SysTick->VAL = 0x00;
    }

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL   = 0x00;
}
