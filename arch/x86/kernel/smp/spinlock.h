// ============================================================================
// spinlock.h - Basic synchronization primitives
// ============================================================================

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	volatile uint32_t lock;
	const char *name; // For debugging
} spinlock_t;

#define SPINLOCK_INIT(name_in) \
	(spinlock_t) { .lock = 0, .name = name_in }

// Initialize a spinlock
static inline void spinlock_init(spinlock_t *lock, const char *name)
{
	lock->lock = 0;
	lock->name = name;
}

// Acquire spinlock (busy-wait)
static inline void spinlock_acquire(spinlock_t *lock)
{
	while (__sync_lock_test_and_set(&lock->lock, 1))
	{
		// Spin with pause instruction (reduces contention)
		while (lock->lock)
			asm volatile("pause");
	}

	__sync_synchronize(); // Memory barrier
}

// Try to acquire spinlock (non-blocking)
static inline bool spinlock_try_acquire(spinlock_t *lock)
{
	if (__sync_lock_test_and_set(&lock->lock, 1) == 0)
	{
		__sync_synchronize();
		return true;
	}
	return false;
}

// Release spinlock
static inline void spinlock_release(spinlock_t *lock)
{
	__sync_synchronize(); // Memory barrier
	__sync_lock_release(&lock->lock);
}

// Check if spinlock is held
static inline bool spinlock_is_held(spinlock_t *lock)
{
	return lock->lock != 0;
}

// Interrupt-safe spinlock (saves/restores interrupt flag)
typedef struct
{
	spinlock_t lock;
	uint64_t flags;
} irqlock_t;

#define IRQLOCK_INIT(name) {.lock = SPINLOCK_INIT(name), .flags = 0}

static inline void irqlock_init(irqlock_t *lock, const char *name)
{
	spinlock_init(&lock->lock, name);
	lock->flags = 0;
}

static inline void irqlock_acquire(irqlock_t *lock)
{
	// Save interrupt flag and disable interrupts
	asm volatile(
	    "pushfq\n"
	    "pop %0\n"
	    "cli\n"
	    : "=r"(lock->flags)::"memory");

	spinlock_acquire(&lock->lock);
}

static inline void irqlock_release(irqlock_t *lock)
{
	spinlock_release(&lock->lock);

	// Restore interrupt flag
	if (lock->flags & (1 << 9)) // IF flag
		asm volatile("sti" ::: "memory");
}

#endif // SPINLOCK_H
