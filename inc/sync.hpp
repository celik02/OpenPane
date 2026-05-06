#ifndef SYNC_HPP
#define SYNC_HPP

#include <stdint.h>

/* Maximum number of tasks that can block on a single Mutex or Semaphore.
 * Raise this if you add more tasks that contend on the same resource. */
#define MAX_WAITERS 3

/* ---- Mutex ---------------------------------------------------------------
 * Binary lock: only one task may hold it at a time.
 * Any task that calls lock() while it is held is put to sleep (BLOCKED) until
 * the holder calls unlock(). Up to MAX_WAITERS tasks may queue up.
 *
 * Typical use:
 *   Mutex uart_mutex;
 *   uart_mutex.lock();
 *   printf("...");          // only one task in here at a time
 *   uart_mutex.unlock();
 * ----------------------------------------------------------------------- */
class Mutex {
public:
    void lock();
    void unlock();

private:
    volatile uint32_t locked_  = 0;       /* 0 = free, 1 = held            */
    uint8_t           owner_   = 0xFF;    /* task index that holds the lock */
    uint8_t           waiters_[MAX_WAITERS];
    uint8_t           waiter_count_ = 0;
};

/* ---- Semaphore -----------------------------------------------------------
 * Counting semaphore.  Useful for two patterns:
 *
 *   1. Resource pool (initial count = N): like a mutex but allows N holders.
 *   2. Task signalling (initial count = 0): one task calls signal() to wake
 *      another that is blocked in wait().  Used for producer/consumer between
 *      tasks — e.g. WiFi task signals display task when new data is ready.
 *
 * wait()   — decrement count; block if already 0.
 * signal() — increment count; unblock one waiter if any.
 * ----------------------------------------------------------------------- */
class Semaphore {
public:
    explicit Semaphore(uint32_t initial = 0) : count_(static_cast<int32_t>(initial)) {}

    void wait();    /* P / acquire / down */
    void signal();  /* V / release / up   */

private:
    volatile int32_t count_;
    uint8_t          waiters_[MAX_WAITERS];
    uint8_t          waiter_count_ = 0;
};

#endif /* SYNC_HPP */