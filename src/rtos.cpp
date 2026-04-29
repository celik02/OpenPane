#include "rtos.hpp"
#include "stm32f4xx.h"
#include <cstdio>
#include <stdint.h>

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


__attribute__((naked)) void PendSV_Handler(void)
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

extern "C" void update_next_task(void){
    current_task_index = (current_task_index + 1) % MAX_TASKS;
}

/* Switch the active stack pointer from MSP to PSP.
 * After reset the CPU uses MSP for everything. Calling this before starting the
 * scheduler ensures tasks run on PSP, keeping task stacks isolated from the
 * scheduler's MSP stack. Must be called after PSP is initialized for the first task. */
__attribute__((naked)) void switch_sp_to_psp(void)
{
    //1. initialize PSP to the first task's stack pointer
    __asm volatile ("PUSH {LR}"); // save LR since we're naked and will clobber it
    __asm volatile ("BL get_psp_value"); // get current task's PSP value into R0
    __asm volatile ("MSR PSP, R0"); // set PSP to the task's stack pointer
    __asm volatile ("POP {LR}"); // restore LR

    //2. switch to PSP by setting CONTROL register bit 1
    __asm volatile (
        "MRS R0, CONTROL\n"   /* Read CONTROL register into R0 */
        "ORR R0, R0, #2\n"    /* Set bit 1 to select PSP */
        "MSR CONTROL, R0\n"   /* Write back to CONTROL register */
        "ISB\n"              /* Instruction Synchronization Barrier to ensure the change takes effect immediately */
        "BX LR\n"           /* Return from function */
        :
        :
        : "r0"  // clobber R0 since we're using it to manipulate CONTROL
    );
}

void uart_task(void *)
{
    while (1)
    {
        printf("Hello from UART Task!\n");
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
        printf("<l5, 5>\n");
        systick_delay_ms(500);
    }
}

void dummy_task(void *)
{
    while (1)
    {
        printf("Hello from Dummy Task!\n");
        systick_delay_ms(2000);
    }
}

void idle_task(void *)
{
    while (1)
        __WFI();
}


/* ---- SysTick ---- */

static volatile uint32_t global_ms_tick = 0;

extern "C" void SysTick_Handler(void)
{
    global_ms_tick++;
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;  /* request context switch */
    // unblock_taks_waiting_on_tick(global_ms_tick);
    // if (should_context_switch()) {
    //     SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    // }
}

void systick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t systick_get_ms(void)
{
    return global_ms_tick;
}

void systick_delay_ms(uint32_t ms)
{
    uint32_t start = global_ms_tick;
    while ((global_ms_tick - start) < ms);
}
