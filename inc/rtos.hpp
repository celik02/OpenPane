#ifndef RTOS_HPP
#define RTOS_HPP

#include <stdint.h>
#include "memory_map.h"

using TaskFunc = void(*)(void*);
uint32_t psp_of_tasks[MAX_TASKS] = {
    T1_STACK_START,
    T2_STACK_START,
    T3_STACK_START,
    T4_STACK_START,
}; /* space for 4 tasks' stack pointers */

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

#endif /* RTOS_HPP */
