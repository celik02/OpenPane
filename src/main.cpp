#include "stm32f4xx.h"
#include "rtos.hpp"

int main(void)
{

    enable_processor_faults();
    usart2_init();
    init_tasks_stack();

    usart2_init();

// Direct hardware test — if this shows up, USART2 works
const char *msg = "USART2 OK\r\n";
for (const char *p = msg; *p; p++) {
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = *p;
}
    /* since we have task stack taking top portion of the sram, we need to initialize the scheduler stack
    by default top of stack would be wrong since it's assigned to task 1*/
    init_scheduler_stack(SCHEDULER_STACK_START);
    switch_sp_to_psp();
    systick_init();   /* starts SysTick — first tick will trigger PendSV */

    while (1)
        __WFI(); /* should never reach here once scheduler is running */
}
