#ifndef RTOS_HPP
#define RTOS_HPP

#include <stdint.h>
#include "memory_map.h"

using TaskFunc = void(*)(void*);

void uart_task(void *);
void led200_task(void *);
void led500_task(void *);
void dummy_task(void *);
void idle_task(void *);

enum class TaskState : uint8_t {
    READY,
    RUNNING,
    BLOCKED,
};

struct TCB {
    volatile uint32_t *stack_pointer; /* top of stack — must be first field, accessed by asm at offset 0 */
    TaskFunc           task_func;
    TaskState          state;
    uint32_t           priority;      /* lower number = higher priority */
    const char        *name;
    uint32_t          *stack_base;    /* base of allocated stack (for bounds checking) */
    uint32_t           stack_size;
    uint8_t            pid;
};

extern TCB tasks[MAX_TASKS];
extern uint8_t current_task_index;

__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
extern "C" __attribute__((naked)) void PendSV_Handler(void);

extern "C" void     save_psp_value(uint32_t psp);
extern "C" uint32_t get_psp_value(void);
extern "C" void     update_next_task(void);

/* ---- USART2 ---- */
void usart2_init(void);

/* ---- SysTick ---- */
void     systick_init(void);
uint32_t systick_get_ms(void);
void     systick_delay_ms(uint32_t ms);

#endif /* RTOS_HPP */
