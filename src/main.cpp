#include "stm32f4xx.h"
#include "systick.hpp"
#include "rtos.hpp"

int main(void)
{

    enable_processor_faults(); /* TODO  */
    init_scheduler_stack(SCHEDULER_STACK_START);
    systick_init();

    init_tasks_stack();


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
