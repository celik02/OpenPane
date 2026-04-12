#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdint.h>

#define SRAM_START           0x20000000U
#define SRAM_SIZE            (128U * 1024U)   /* 128 KB */
#define SRAM_END             (SRAM_START + SRAM_SIZE)

#define SIZEOF_STACK         1024U            /* bytes per task stack */
#define MAX_TASKS            4U
#define SIZE_SCHEDULER_STACK 1024U

/* Task stacks are laid out from the top of SRAM downward */
#define T1_STACK_START        (SRAM_END)
#define T2_STACK_START        (T1_STACK_START - SIZEOF_STACK)
#define T3_STACK_START        (T2_STACK_START - SIZEOF_STACK)
#define T4_STACK_START        (T3_STACK_START - SIZEOF_STACK)
#define SCHEDULER_STACK_START (T4_STACK_START - SIZEOF_STACK)

#endif /* MEMORY_MAP_H */
