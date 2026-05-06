#include "sync.hpp"
#include "rtos.hpp"
#include "stm32f4xx.h"

/* ---- Mutex ---------------------------------------------------------------*/

/* Cortex-M4 provides LDREX/STREX — Load/Store Exclusive — for lock-free
 * atomic operations without disabling interrupts.
 *
 * LDREX marks an address in the CPU's "exclusive monitor". Any interrupt or
 * other core access between LDREX and STREX causes STREX to fail (returns 1).
 * On success STREX returns 0 and the write went through atomically.
 * This retry loop is the standard ARM pattern for a compare-and-swap. */
static bool atomic_try_set(volatile uint32_t *addr)
{
    if (__LDREXW(addr) != 0) {
        __CLREX();   /* clear exclusive monitor — we're not going to STREX */
        return false;
    }
    /* __DMB after a successful STREX ensures that all memory writes before
     * the lock acquisition are visible to other observers before any writes
     * inside the critical section. */
    if (__STREXW(1u, addr) != 0)
        return false;  /* another writer (e.g. ISR) snuck in between */
    __DMB();
    return true;
}

void Mutex::lock()
{
    while (true) {
        if (atomic_try_set(&locked_)) {
            owner_ = current_task_index;
            return;  /* lock acquired */
        }

        /* Lock is held by another task.
         * Add ourselves to the wait queue if there is room, then sleep.
         * When unlock() wakes us we loop back and try again — the lock may
         * have been grabbed by a higher-priority waiter before we ran. */
        if (waiter_count_ < MAX_WAITERS)
            waiters_[waiter_count_++] = current_task_index;

        tasks[current_task_index].state = TaskState::BLOCKED;
        __DSB();  /* ensure state write reaches memory before PendSV fires */
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;  /* yield — switch away now */
        /* execution resumes here after unlock() sets us back to READY */
    }
}

void Mutex::unlock()
{
    /* __DMB before clearing the lock ensures all writes inside the critical
     * section are visible to the next task before it acquires the lock. */
    __DMB();
    owner_  = 0xFF;
    locked_ = 0;

    /* Wake all queued waiters so the scheduler can pick the best one.
     * They will race to re-acquire the lock on their next run; only one
     * will succeed (atomic_try_set), the rest will re-block. */
    for (uint8_t i = 0; i < waiter_count_; i++)
        tasks[waiters_[i]].state = TaskState::READY;
    waiter_count_ = 0;
}

/* ---- Semaphore -----------------------------------------------------------*/

void Semaphore::wait()
{
    while (true) {
        /* Atomically read count and decrement it if > 0.
         * LDREX/STREX loop handles the case where an ISR (e.g. SysTick) or
         * another task calls signal() between our read and our write. */
        int32_t val;
        do {
            val = static_cast<int32_t>(__LDREXW(reinterpret_cast<volatile uint32_t *>(&count_)));
            if (val <= 0) {
                __CLREX();
                break;   /* count is 0 — must block */
            }
        } while (__STREXW(static_cast<uint32_t>(val - 1),
                          reinterpret_cast<volatile uint32_t *>(&count_)) != 0);

        if (val > 0) {
            __DMB();
            return;  /* decremented successfully — resource acquired */
        }

        /* Count was 0 — block until signal() increments it. */
        if (waiter_count_ < MAX_WAITERS)
            waiters_[waiter_count_++] = current_task_index;

        tasks[current_task_index].state = TaskState::BLOCKED;
        __DSB();
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        /* resume here after signal() — loop and try to decrement again */
    }
}

void Semaphore::signal()
{
    /* Atomically increment count. */
    uint32_t val;
    do {
        val = __LDREXW(reinterpret_cast<volatile uint32_t *>(&count_));
    } while (__STREXW(val + 1, reinterpret_cast<volatile uint32_t *>(&count_)) != 0);
    __DMB();

    /* Wake the oldest waiter (index 0).  It will try to decrement count on
     * its next run; if it fails (another task raced ahead) it blocks again. */
    if (waiter_count_ > 0) {
        tasks[waiters_[0]].state = TaskState::READY;
        /* shift remaining waiters down */
        for (uint8_t i = 1; i < waiter_count_; i++)
            waiters_[i - 1] = waiters_[i];
        waiter_count_--;
    }
}
