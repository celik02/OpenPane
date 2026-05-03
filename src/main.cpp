#include "stm32f4xx.h"
#include "rtos.hpp"

int main(void)
{

    enable_processor_faults();
    usart2_init();
    /* MSP must be moved off the top of SRAM before writing task fake frames,
     * because T1_STACK_START == _estack (reset MSP value). */
    init_scheduler_stack(SCHEDULER_STACK_START);
    init_tasks_stack();
    systick_init();      /* start 1ms tick so tasks can use systick_delay_ms */
    scheduler_start();   /* SVC #0 → launches task 0, never returns */

    while (1)
        __WFI(); /* unreachable — here only as a safety net */
}
