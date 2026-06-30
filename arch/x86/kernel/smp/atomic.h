/**
 * @file atomic.h
 * @brief Atomic operations for SMP synchronization
 */

#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdint.h>

/* ── Type definitions ────────────────────────────────────────────────── */

/**
 * @brief 32-bit atomic integer type
 */
typedef struct {
	volatile uint32_t value;
} atomic_t;

/**
 * @brief 64-bit atomic integer type
 */
typedef struct {
	volatile uint64_t value;
} atomic64_t;

/* ── 32-bit atomic operations ───────────────────────────────────────── */

/**
 * @brief Atomically read a 32-bit value
 * @param a Pointer to atomic variable
 * @return Current value
 */
static inline uint32_t atomic_read(atomic_t *a)
{
	return a->value;
}

/**
 * @brief Atomically set a 32-bit value
 * @param a Pointer to atomic variable
 * @param val Value to set
 */
static inline void atomic_set(atomic_t *a, uint32_t val)
{
	a->value = val;
}

/**
 * @brief Atomically add to a 32-bit value
 * @param a Pointer to atomic variable
 * @param val Value to add
 * @return Previous value
 */
static inline uint32_t atomic_add(atomic_t *a, uint32_t val)
{
	return __sync_fetch_and_add(&a->value, val);
}

/**
 * @brief Atomically subtract from a 32-bit value
 * @param a Pointer to atomic variable
 * @param val Value to subtract
 * @return Previous value
 */
static inline uint32_t atomic_sub(atomic_t *a, uint32_t val)
{
	return __sync_fetch_and_sub(&a->value, val);
}

/**
 * @brief Atomically increment a 32-bit value
 * @param a Pointer to atomic variable
 * @return Previous value
 */
static inline uint32_t atomic_inc(atomic_t *a)
{
	return __sync_fetch_and_add(&a->value, 1);
}

/**
 * @brief Atomically decrement a 32-bit value
 * @param a Pointer to atomic variable
 * @return Previous value
 */
static inline uint32_t atomic_dec(atomic_t *a)
{
	return __sync_fetch_and_sub(&a->value, 1);
}

/**
 * @brief Atomic compare-and-exchange for 32-bit value
 * @param a Pointer to atomic variable
 * @param old_val Expected old value
 * @param new_val New value to write if comparison succeeds
 * @return true if exchange was performed
 */
static inline bool atomic_cmpxchg(atomic_t *a, uint32_t old_val, uint32_t new_val)
{
	return __sync_bool_compare_and_swap(&a->value, old_val, new_val);
}

/* ── 64-bit atomic operations ───────────────────────────────────────── */

/**
 * @brief Atomically read a 64-bit value
 * @param a Pointer to atomic 64-bit variable
 * @return Current value
 */
static inline uint64_t atomic64_read(atomic64_t *a)
{
	return a->value;
}

/**
 * @brief Atomically set a 64-bit value
 * @param a Pointer to atomic 64-bit variable
 * @param val Value to set
 */
static inline void atomic64_set(atomic64_t *a, uint64_t val)
{
	a->value = val;
}

/**
 * @brief Atomically add to a 64-bit value
 * @param a Pointer to atomic 64-bit variable
 * @param val Value to add
 * @return Previous value
 */
static inline uint64_t atomic64_add(atomic64_t *a, uint64_t val)
{
	return __sync_fetch_and_add(&a->value, val);
}

/**
 * @brief Atomically increment a 64-bit value
 * @param a Pointer to atomic 64-bit variable
 * @return Previous value
 */
static inline uint64_t atomic64_inc(atomic64_t *a)
{
	return __sync_fetch_and_add(&a->value, 1);
}

/* ── Memory barriers ─────────────────────────────────────────────────── */

/**
 * @brief Full memory barrier (synchronizes all memory operations)
 */
static inline void memory_barrier(void)
{
	__sync_synchronize();
}

/**
 * @brief Compiler barrier (prevents instruction reordering)
 */
static inline void compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

#endif /* ATOMIC_H */
