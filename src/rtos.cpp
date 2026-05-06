#include "rtos.hpp"
#include "sync.hpp"
#include "stm32f4xx.h"
#include <cstdio>
#include <stdint.h>

Mutex uart_mutex;


TCB tasks[MAX_TASKS] = {
    { (volatile uint32_t *)T1_STACK_START,   uart_task,   TaskState::READY, 0,   "uart",   (uint32_t *)(T1_STACK_START   - SIZEOF_STACK), SIZEOF_STACK, 0 },
    { (volatile uint32_t *)T2_STACK_START,   led200_task, TaskState::READY, 1,   "led200", (uint32_t *)(T2_STACK_START   - SIZEOF_STACK), SIZEOF_STACK, 1 },
    { (volatile uint32_t *)T3_STACK_START,   led500_task, TaskState::READY, 2,   "led500", (uint32_t *)(T3_STACK_START   - SIZEOF_STACK), SIZEOF_STACK, 2 },
    { (volatile uint32_t *)T4_STACK_START,   dummy_task,  TaskState::READY, 3,   "dummy",  (uint32_t *)(T4_STACK_START   - SIZEOF_STACK), SIZEOF_STACK, 3 },
    { (volatile uint32_t *)IDLE_STACK_START, idle_task,   TaskState::READY, 255, "idle",   (uint32_t *)(IDLE_STACK_START - SIZEOF_STACK), SIZEOF_STACK, 4 },
};

uint8_t current_task_index=0; // index of currently running task in task_handles and psp_of_tasks

/* Enable UsageFault, BusFault, and MemManage fault handlers individually.
 * By default these are all demoted to HardFault, which makes debugging harder —
 * you lose the specific fault type and the dedicated status registers.
 * Setting SHCSR bits promotes them so each fires its own handler with its own
 * status register (CFSR, BFSR, MMFSR), making faults much easier to diagnose. */
void enable_processor_faults(void)
{
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk   /* UsageFault: div-by-zero, unaligned access, etc. */
               |  SCB_SHCSR_BUSFAULTENA_Msk    /* BusFault:   invalid memory access               */
               |  SCB_SHCSR_MEMFAULTENA_Msk;   /* MemManage:  MPU violations                      */

    /* PendSV must be the lowest priority interrupt so it never preempts another
     * handler mid-execution. SHP[10] maps to PendSV in the SHPR3 register. */
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
}

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


extern "C" __attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile (
        "PUSH {LR}\n"              /* save EXC_RETURN — tells CPU how to return from exception */

        /* Save current task */
        "MRS R0, PSP\n"            /* R0 = current task's stack pointer */
        "STMDB R0!, {R4-R11}\n"   /* push R4-R11 onto current task's stack; R0 updated */
        "BL save_psp_value\n"      /* save updated PSP (R0) into tasks[current].stack_pointer */

        /* Switch to next task */
        "BL update_next_task\n"    /* advance current_task_index */

        /* Restore next task */
        "BL get_psp_value\n"       /* R0 = tasks[next].stack_pointer */
        "LDMIA R0!, {R4-R11}\n"   /* pop R4-R11 from next task's stack; R0 updated */
        "MSR PSP, R0\n"            /* set PSP to next task's stack pointer */

        "POP {LR}\n"               /* restore EXC_RETURN */
        "BX LR\n"                  /* exception return — CPU pops hardware frame, jumps to next task's PC */
    );
}

/* Context switching works by saving/restoring a task's stack frame. When PendSV switches
 * to a task, it expects to find a valid Cortex-M4 exception frame on that task's stack.
 * Tasks that have never run have no such frame, so we plant a fake one here.
 * This makes a brand-new task indistinguishable from a task that was previously interrupted,
 * allowing the scheduler to treat all tasks uniformly from the very first switch. */
void init_tasks_stack(void){
    for (uint8_t i = 0; i < MAX_TASKS; ++i) {
        uint32_t *pPSP = (uint32_t *)tasks[i].stack_pointer;
        /* Hardware exception frame — CPU pops these automatically on exception return */
        *(--pPSP) = 0x01000000;                    /* xPSR: Thumb bit must be set */
        *(--pPSP) = (uint32_t)tasks[i].task_func;  /* PC: task entry point */
        *(--pPSP) = 0xFFFFFFFD;                    /* LR: return to Thread mode, use PSP */
        *(--pPSP) = 0;                             /* R12 */
        *(--pPSP) = 0;                             /* R3 */
        *(--pPSP) = 0;                             /* R2 */
        *(--pPSP) = 0;                             /* R1 */
        *(--pPSP) = 0;                             /* R0 */
        /* Software frame — PendSV saves/restores these manually */
        for (int j = 0; j < 8; ++j)
            *(--pPSP) = 0;                         /* R11–R4 */
        tasks[i].stack_pointer = (volatile uint32_t *)pPSP;
    }
}

extern "C" void save_psp_value(uint32_t current_psp_value){
    tasks[current_task_index].stack_pointer = (volatile uint32_t *)current_psp_value;
}

extern "C" uint32_t get_psp_value(){
    return (uint32_t)tasks[current_task_index].stack_pointer;
}

/* Walk the real tasks (0..MAX_TASKS-2) in round-robin order starting after the
 * current task. Pick the first one whose state is READY.
 * If every real task is BLOCKED, fall through to the idle task (MAX_TASKS-1).
 * Idle is never BLOCKED — it always runs when nothing else can. */
extern "C" void update_next_task(void) {
    uint8_t real_tasks = MAX_TASKS - 1;  /* idle is the last slot, not part of rotation */

    for (uint8_t i = 1; i <= real_tasks; i++) {
        uint8_t candidate = (current_task_index + i) % real_tasks;
        if (tasks[candidate].state == TaskState::READY) {
            current_task_index = candidate;
            return;
        }
    }

    current_task_index = MAX_TASKS - 1;  /* all real tasks blocked — run idle */
}

/* Issue SVC #0 to hand control to SVC_Handler, which performs the first task launch.
 * This function never returns — once SVC_Handler does its exception return it jumps
 * directly into task 0's entry point. */
void scheduler_start(void)
{
    __asm volatile ("SVC #0");
}

/* SVC_Handler — fires immediately when scheduler_start() issues SVC #0.
 *
 * Goal: jump into task 0 as if PendSV had just restored it from a prior run.
 * We are in Handler mode here, so BX LR is a real exception return that makes
 * the hardware pop a stack frame — exactly what we need to bootstrap the first task.
 *
 * Step-by-step:
 *   1. get_psp_value() returns tasks[0].stack_pointer, which points to the TOP of
 *      task 0's fake software frame (the R11 slot planted by init_tasks_stack).
 *   2. LDMIA pops R4–R11 from that software frame, advancing R0 past them so it
 *      now points to the start of the fake hardware frame (the R0 slot).
 *   3. MSR PSP, R0 — sets PSP to that address. When we do the exception return
 *      the CPU will pop {R0-R3,R12,LR,PC,xPSR} from here, landing in uart_task().
 *   4. CONTROL |= 2 — tells the CPU to use PSP in Thread mode from now on.
 *      All tasks will run on their own PSP stacks; only handlers use MSP.
 *   5. MVN LR, #2 — sets LR to 0xFFFFFFFD (bitwise NOT of 2).
 *      This is the EXC_RETURN magic value meaning:
 *        "return to Thread mode and use PSP as the stack pointer".
 *      We cannot keep the original LR value because the SVC was called from
 *      Thread/MSP mode, so the CPU put 0xFFFFFFF9 in LR (return to Thread/MSP).
 *      We override it to 0xFFFFFFFD so the hardware pops from PSP instead.
 *   6. BX LR — triggers the exception return. Hardware pops the fake frame from
 *      PSP, restoring R0-R3/R12/LR/PC/xPSR, and execution begins at uart_task(). */
extern "C" __attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile (
        "BL  get_psp_value\n"       /* R0 = tasks[0].stack_pointer (top of SW frame) */
        "LDMIA R0!, {R4-R11}\n"    /* pop software frame (R11..R4); R0 now → HW frame */
        "MSR PSP, R0\n"             /* PSP now points at the fake hardware frame */

        "MRS R0, CONTROL\n"
        "ORR R0, R0, #2\n"          /* CONTROL.SPSEL = 1 → Thread mode uses PSP */
        "MSR CONTROL, R0\n"
        "ISB\n"                     /* barrier — CONTROL change must be visible before BX */

        "MVN LR, #2\n"              /* LR = ~2 = 0xFFFFFFFD (EXC_RETURN: Thread + PSP) */
        "BX  LR\n"                  /* exception return → CPU pops HW frame → uart_task() */
    );
}

/* defined in the SysTick section below; declared here so task functions can read them */
extern volatile uint32_t global_ms_tick;
extern volatile uint32_t idle_count;

void uart_task(void *)
{
    while (1)
    {
        uart_mutex.lock();
        printf("tick=%lu  idle_hits=%lu\n", global_ms_tick, idle_count);
        uart_mutex.unlock();
        systick_delay_ms(1000);
    }
}

void led200_task(void *)
{
    /* Configure PA5 (LD2) as output */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));

    while (1)
    {
        GPIOA->ODR ^= (1U << 5);
        systick_delay_ms(200);
    }
}

void led500_task(void *)
{
    while (1){
        uart_mutex.lock();
        printf("<l5, 5>\n");
        uart_mutex.unlock();
        systick_delay_ms(500);
    }
}

void dummy_task(void *)
{
    while (1)
    {
        uart_mutex.lock();
        printf("Hello from Dummy Task!\n");
        uart_mutex.unlock();
        systick_delay_ms(200);
    }
}
volatile uint32_t global_ms_tick = 0;
volatile uint32_t idle_count = 0;

void idle_task(void *)
{
    while (1)
    {
        idle_count++;
        __WFI();
    }
}


/* ---- SysTick ---- */



extern "C" void SysTick_Handler(void)
{
    global_ms_tick++;

    /* Wake any task whose delay has expired. We only check real tasks (not idle)
     * because idle is never put to sleep — it runs whenever nothing else can. */
    for (uint8_t i = 0; i < MAX_TASKS - 1; i++) {
        if (tasks[i].state == TaskState::BLOCKED && global_ms_tick >= tasks[i].wake_tick) {
            tasks[i].state = TaskState::READY;
        }
    }

    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;  /* request context switch every tick */
}

/*
 * SysTick_Config sets LOAD = (ticks - 1), clears VAL, enables the counter
 * and its interrupt using the processor clock.
 * SystemCoreClock / 1000 ticks at 16 MHz HSI = 16000 ticks → fires every 1 ms.
 * every tick is 1/16M seconds
 */
void systick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t systick_get_ms(void)
{
    return global_ms_tick;
}

/* Block the calling task for `ms` milliseconds.
 *
 * Instead of spinning (busy-wait), we mark this task BLOCKED and set its
 * wake_tick. Then we pend PendSV to yield the CPU immediately — the scheduler
 * will skip us and run whoever is READY next (or idle if nobody is).
 *
 * When SysTick_Handler sees global_ms_tick >= wake_tick it sets us back to
 * READY. The next PendSV will pick us up and resume right after the SCB write
 * below, which returns to the caller as if the delay had just finished. */
void systick_delay_ms(uint32_t ms)
{
    tasks[current_task_index].wake_tick = global_ms_tick + ms;
    tasks[current_task_index].state     = TaskState::BLOCKED;

    /* Yield: pend PendSV so the scheduler switches away from us immediately.
     * __DSB() ensures the state/wake_tick writes reach memory before the
     * PendSV request is seen by the hardware. */
    __DSB();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}
