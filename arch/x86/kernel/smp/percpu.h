/**
 * @file percpu.h
 * @brief Per-CPU data structures and management
 */

#ifndef PERCPU_H
#define PERCPU_H

#include <stdint.h>
#include <stdbool.h>

#include <apic/apic.h>

/* ── Constants ────────────────────────────────────────────────────────── */

#define MAX_CPUS 256

/* ── Per-CPU information structure ───────────────────────────────────── */

/**
 * @brief Per-CPU information structure
 */
typedef struct {
	uint8_t apic_id;
	uint8_t cpu_id;
	bool online;
	bool bsp;

	void *stack_top;
	uint64_t stack_size;

	void *current_task;
	uint64_t ticks;

	uint64_t idle_time;
	uint64_t kernel_time;
	uint64_t user_time;
} cpu_info_t;

/* ── Global per-CPU arrays ────────────────────────────────────────────── */

extern cpu_info_t cpu_data[MAX_CPUS];
extern volatile uint32_t num_cpus_online;

/* ── Public functions ─────────────────────────────────────────────────── */

/**
 * @brief Get current CPU info structure using APIC ID
 * @return Pointer to current CPU's cpu_info_t, or NULL on failure
 */
cpu_info_t *get_current_cpu(void);

/**
 * @brief Get CPU info by logical CPU ID
 * @param cpu_id Logical CPU ID
 * @return Pointer to cpu_info_t, or NULL if out of range
 */
cpu_info_t *get_cpu(uint8_t cpu_id);

/**
 * @brief Initialize per-CPU data for the BSP
 */
void percpu_init_bsp(void);

/**
 * @brief Initialize per-CPU data for an AP
 * @param apic_id APIC ID of the AP being initialized
 */
void percpu_init_ap(uint8_t apic_id);

/**
 * @brief Get current CPU ID (fast path using LAPIC)
 * @return Current CPU's APIC ID
 */
static inline uint8_t cpu_id(void)
{
	return lapic_get_id();
}

#endif /* PERCPU_H */
