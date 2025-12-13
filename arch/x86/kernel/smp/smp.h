#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include "spinlock.h"
#include "percpu.h"
#include "atomic.h"

// Initialize SMP (start all Application Processors)
int smp_init(void);

// Get number of online CPUs
uint32_t smp_get_cpu_count(void);

// Send IPI to all CPUs except current
void smp_send_ipi_all(uint8_t vector);

// Call function on all CPUs
void smp_call_function_all(void (*func)(void *), void *arg);

#endif // SMP_H
