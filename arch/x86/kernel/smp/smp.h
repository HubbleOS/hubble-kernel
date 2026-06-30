/**
 * @file smp.h
 * @brief SMP initialization and inter-processor communication
 */

#ifndef SMP_H
#define SMP_H

#include <stdint.h>

#include "atomic.h"
#include "percpu.h"
#include "spinlock.h"

/* ── Public functions ─────────────────────────────────────────────────── */

/**
 * @brief Initialize SMP (start all Application Processors)
 * @return 0 on success, negative on error
 */
int smp_init(void);

/**
 * @brief Get number of online CPUs
 * @return Number of online CPUs
 */
uint32_t smp_get_cpu_count(void);

/**
 * @brief Send IPI to all CPUs except the current one
 * @param vector Interrupt vector to send
 */
void smp_send_ipi_all(uint8_t vector);

/**
 * @brief Call a function on all CPUs
 * @param func Function pointer to call
 * @param arg Argument to pass to the function
 */
void smp_call_function_all(void (*func)(void *), void *arg);

#endif /* SMP_H */
