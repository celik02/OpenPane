#include "systick.h"
#include "stm32f4xx.h"

static volatile uint32_t ms_tick = 0;

void SysTick_Handler(void)
{
    ms_tick++;
}

void systick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t systick_get_ms(void)
{
    return ms_tick;
}

void systick_delay_ms(uint32_t ms)
{
    uint32_t start = ms_tick;
    while ((ms_tick - start) < ms);
}