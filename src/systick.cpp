#include "systick.hpp"
#include "stm32f4xx.h"
#include "rtos.hpp"

static volatile uint32_t global_ms_tick = 0;

/* Interrupt handlers must have C linkage so the linker resolves them against
 * the vector table, which uses unmangled names. Without extern "C", the C++
 * compiler would mangle SysTick_Handler and the handler would never be called. */
extern "C" {


/* Fires every 1 ms. Increments the tick counter used by systick_get_ms() and systick_delay_ms(). */
void SysTick_Handler(void)
{
    global_ms_tick++;
    unblock_taks_waiting_on_tick(global_ms_tick); /* Unblock any tasks whose delay has expired */
    // set pend bit of PendSV to trigger a context switch at the end of this interrupt if needed
    if (should_context_switch()) {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; /* Set PendSV to pending to request a context switch */
    }
}

} /* extern "C" */

void systick_init(void)
{
    /*
     * SysTick is a 24-bit down-counter clocked at the CPU frequency (SystemCoreClock = 84 MHz).
     * SysTick_Config() sets the RELOAD register, which is the number of clock cycles the counter
     * counts down before firing an interrupt and reloading.
     *
     * To get a 1 ms interrupt period:
     *   period = cycles_per_tick / frequency
     *   0.001 s = reload / 84,000,000 Hz
     *   reload  = 84,000,000 / 1000 = 84,000 cycles
     *
     * So SystemCoreClock / 1000 gives exactly 1 tick per millisecond regardless of
     * what clock speed the PLL is configured to — no magic numbers needed.
     * (currently it's 16 MHz. If you change the PLL settings, this will still work correctly.)
     */
    SysTick_Config(SystemCoreClock / 1000);
}

uint32_t systick_get_ms(void)
{
    return global_ms_tick;
}

void systick_delay_ms(uint32_t ms)
{
    // this is a blocking delay function. It will block the CPU until the specified number of milliseconds has passed.
    uint32_t start = global_ms_tick;
    while ((global_ms_tick - start) < ms);
}
