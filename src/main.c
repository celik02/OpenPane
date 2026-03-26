#include "stm32f4xx.h"

int main(void)
{
    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5 (LD2) as output */
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));

    while (1)
    {
        GPIOA->ODR ^= (1U << 5);
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}