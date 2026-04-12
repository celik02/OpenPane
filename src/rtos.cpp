#include "rtos.hpp"
#include "systick.hpp"
#include "stm32f4xx.h"
#include <cstdio>

/* __attribute__((naked)) suppresses the compiler-generated function prologue and epilogue
 * (no automatic push/pop of registers, no stack frame setup). This is required here because
 * we are manually setting MSP — if the compiler emitted a prologue that touched the stack
 * before our MSR instruction ran, the stack pointer would be corrupted. The trade-off is
 * that we must handle the return ourselves, which is why BX LR is written explicitly. */
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack)
{
    __asm volatile (
        "MSR MSP, %[stack_top]\n"
        "BX LR\n"
        :
        : [stack_top] "r" (sched_top_of_stack)
        :
    );
}


void init_tasks_stack(void){

}



void uart_task(void *)
{
    printf("Hello from UART Task!\n");
}

void led200_task(void *)
{
    // while (1)
    // {
    //     GPIOA->ODR ^= (1U << 5);
    //     systick_delay_ms(200);
    // }
}

void led500_task(void *)
{
    // while (1)
    // {
    //     GPIOA->ODR ^= (1U << 5);
    //     systick_delay_ms(500);
    // }
}

void dummy_task(void *)
{
    printf("Hello from Dummy Task!\n");
    // while (1)
    // {
    //     systick_delay_ms(1000);
    // }
}
