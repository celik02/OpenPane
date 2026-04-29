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
    switch_sp_to_psp();
    systick_init();   /* starts SysTick — first tick will trigger PendSV */

    while (1)
        __WFI(); /* should never reach here once scheduler is running */
}
