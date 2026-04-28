#include "stm32f4xx.h"
#include "rtos.hpp"

int main(void)
{

    enable_processor_faults();
    init_tasks_stack();
    init_scheduler_stack(SCHEDULER_STACK_START);
    switch_sp_to_psp();
    systick_init();   /* starts SysTick — first tick will trigger PendSV */

    while (1)
        __WFI(); /* should never reach here once scheduler is running */
}
