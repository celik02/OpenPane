#include "stm32f4xx.h"
#include "systick.h"

int main(void)
{
    // initialize the SysTick timer.
    systick_init();

    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5 (LD2) as output */
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));

    while (1)
    {
        GPIOA->ODR ^= (1U << 5);
        systick_delay_ms(500);
    }
}