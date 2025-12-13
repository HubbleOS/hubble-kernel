// ============================================================================
// atomic.h - Atomic operations
// ============================================================================

#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdint.h>

typedef struct
{
	volatile uint32_t value;
} atomic_t;

typedef struct
{
	volatile uint64_t value;
} atomic64_t;

// Atomic operations (32-bit)
static inline uint32_t atomic_read(atomic_t *a)
{
	return a->value;
}

static inline void atomic_set(atomic_t *a, uint32_t val)
{
	a->value = val;
}

static inline uint32_t atomic_add(atomic_t *a, uint32_t val)
{
	return __sync_fetch_and_add(&a->value, val);
}

static inline uint32_t atomic_sub(atomic_t *a, uint32_t val)
{
	return __sync_fetch_and_sub(&a->value, val);
}

static inline uint32_t atomic_inc(atomic_t *a)
{
	return __sync_fetch_and_add(&a->value, 1);
}

static inline uint32_t atomic_dec(atomic_t *a)
{
	return __sync_fetch_and_sub(&a->value, 1);
}

static inline bool atomic_cmpxchg(atomic_t *a, uint32_t old_val, uint32_t new_val)
{
	return __sync_bool_compare_and_swap(&a->value, old_val, new_val);
}

// Atomic operations (64-bit)
static inline uint64_t atomic64_read(atomic64_t *a)
{
	return a->value;
}

static inline void atomic64_set(atomic64_t *a, uint64_t val)
{
	a->value = val;
}

static inline uint64_t atomic64_add(atomic64_t *a, uint64_t val)
{
	return __sync_fetch_and_add(&a->value, val);
}

static inline uint64_t atomic64_inc(atomic64_t *a)
{
	return __sync_fetch_and_add(&a->value, 1);
}

// Memory barriers
static inline void memory_barrier(void)
{
	__sync_synchronize();
}

static inline void compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

#endif // ATOMIC_H
