/**
 * @file spinlock.h
 * @brief Spinlock and interrupt-safe lock primitives for SMP
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <asm.h>

/* -- Spinlock ----------------------------------------------------------- */

/**
 * @brief Simple spinlock structure
 */
typedef struct {
  volatile uint32_t lock;
  const char *name;
} spinlock_t;

/**
 * @brief Static spinlock initializer
 * @param name_in Name string for debugging
 */
#define SPINLOCK_INIT(name_in)                                                 \
  (spinlock_t) { .lock = 0, .name = name_in }

/**
 * @brief Initialize a spinlock
 * @param lock Pointer to spinlock
 * @param name Name string for debugging
 */
static inline void spinlock_init(spinlock_t *lock, const char *name) {
  lock->lock = 0;
  lock->name = name;
}

/**
 * @brief Acquire spinlock (busy-wait with pause instruction)
 * @param lock Pointer to spinlock
 */
static inline void spinlock_acquire(spinlock_t *lock) {
  while (__sync_lock_test_and_set(&lock->lock, 1)) {
    while (lock->lock)
      cpu_pause();
  }
  __sync_synchronize();
}

/**
 * @brief Try to acquire spinlock (non-blocking)
 * @param lock Pointer to spinlock
 * @return true if lock was acquired
 */
static inline bool spinlock_try_acquire(spinlock_t *lock) {
  if (__sync_lock_test_and_set(&lock->lock, 1) == 0) {
    __sync_synchronize();
    return true;
  }
  return false;
}

/**
 * @brief Release spinlock
 * @param lock Pointer to spinlock
 */
static inline void spinlock_release(spinlock_t *lock) {
  __sync_synchronize();
  __sync_lock_release(&lock->lock);
}

/**
 * @brief Check if spinlock is held
 * @param lock Pointer to spinlock
 * @return true if lock is currently held
 */
static inline bool spinlock_is_held(spinlock_t *lock) {
  return lock->lock != 0;
}

/* -- Interrupt-safe lock ------------------------------------------------ */

/**
 * @brief Interrupt-safe spinlock (saves/restores interrupt flag)
 */
typedef struct {
  spinlock_t lock;
  uint64_t flags;
} irqlock_t;

/**
 * @brief Static irqlock initializer
 * @param name Name string for debugging
 */
#define IRQLOCK_INIT(name) {.lock = SPINLOCK_INIT(name), .flags = 0}

/**
 * @brief Initialize an interrupt-safe lock
 * @param lock Pointer to irqlock
 * @param name Name string for debugging
 */
static inline void irqlock_init(irqlock_t *lock, const char *name) {
  spinlock_init(&lock->lock, name);
  lock->flags = 0;
}

/**
 * @brief Acquire irqlock (disables interrupts while holding)
 * @param lock Pointer to irqlock
 */
static inline void irqlock_acquire(irqlock_t *lock) {
  asm volatile("pushfq\n"
               "pop %0\n"
               "cli\n"
               : "=r"(lock->flags)::"memory");

  spinlock_acquire(&lock->lock);
}

/**
 * @brief Release irqlock (restores interrupt flag)
 * @param lock Pointer to irqlock
 */
static inline void irqlock_release(irqlock_t *lock) {
  spinlock_release(&lock->lock);

  if (lock->flags & (1 << 9))
    asm volatile("sti" ::: "memory");
}
