#ifndef PERCPU_H
#define PERCPU_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_CPUS 256

typedef struct
{
	uint8_t apic_id; // Local APIC ID
	uint8_t cpu_id;	 // Logical CPU ID (0, 1, 2...)
	bool online;	 // CPU is running
	bool bsp;	 // Bootstrap processor

	void *stack_top;     // Kernel stack top
	uint64_t stack_size; // Stack size

	// Task/scheduling info
	void *current_task; // Current task pointer
	uint64_t ticks;	    // CPU-local tick counter

	// Statistics
	uint64_t idle_time;
	uint64_t kernel_time;
	uint64_t user_time;
} cpu_info_t;

// Global per-CPU data
extern cpu_info_t cpu_data[MAX_CPUS];
extern volatile uint32_t num_cpus_online;

// Get current CPU's info (using APIC ID)
cpu_info_t *get_current_cpu(void);

// Get CPU info by logical ID
cpu_info_t *get_cpu(uint8_t cpu_id);

// Initialize per-CPU data for BSP
void percpu_init_bsp(void);

// Initialize per-CPU data for an AP
void percpu_init_ap(uint8_t apic_id);

// Get current CPU ID (fast path using gs register)
static inline uint8_t cpu_id(void)
{
	// TODO: Store in gs:0 for fast access
	// For now, use LAPIC ID
	extern uint8_t lapic_get_id(void);
	return lapic_get_id();
}

#endif // PERCPU_H
