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
    /* Start on idle task's stack so main()'s WFI loop runs there.
     * The first PendSV will save main's context as idle (correct — idle just does WFI),
     * then switch to task 0 with its fake frame intact so uart_task actually starts. */
    current_task_index = MAX_TASKS - 1;
    systick_init();   /* starts SysTick — first tick will trigger PendSV */
    switch_sp_to_psp();  // just updates CPU to use PSP, doesn't context switch yet

    while (1)
        __WFI(); /* should never reach here once scheduler is running */
}
