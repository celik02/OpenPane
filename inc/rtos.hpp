#ifndef RTOS_HPP
#define RTOS_HPP

#include <stdint.h>
#include "memory_map.h"

using TaskFunc = void(*)(void*);

/* Defined once in rtos.cpp — extern here to avoid multiple-definition errors
 * when this header is included by more than one translation unit. */
extern uint32_t psp_of_tasks[MAX_TASKS];
extern uint32_t task_handles[MAX_TASKS];
extern uint8_t current_task_index;

void uart_task(void *);
void led200_task(void *);
void led500_task(void *);
void dummy_task(void *);

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

__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack(void);
void update_next_task(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
__attribute__((naked)) uint32_t get_psp_value(void);
__attribute__((naked)) void PendSV_Handler(void)

#endif /* RTOS_HPP */
