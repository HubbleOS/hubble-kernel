#ifndef HPET_H
#define HPET_H

#include <stdint.h>
#include <stdbool.h>

// === Core HPET Functions ===

// Initialize HPET (must call after acpi_init)
int hpet_init(void);

// Check if HPET is initialized
bool hpet_is_initialized(void);

// Get HPET frequency in Hz
uint64_t hpet_get_frequency(void);

// === Counter Access ===

// Get current counter value (raw ticks)
uint64_t hpet_get_counter(void);

// Get current time in nanoseconds since boot
uint64_t hpet_get_time_ns(void);

// === Time Conversion ===

// Convert HPET ticks to nanoseconds
uint64_t hpet_ticks_to_ns(uint64_t ticks);

// Convert nanoseconds to HPET ticks
uint64_t hpet_ns_to_ticks(uint64_t ns);

// === Delays (Busy-Wait) ===

// Precise delay in nanoseconds
void hpet_delay_ns(uint64_t ns);

// Precise delay in microseconds
void hpet_delay_us(uint64_t us);

// Precise delay in milliseconds
void hpet_delay_ms(uint64_t ms);

// === Timer Setup (for interrupts) ===

// Setup timer for one-shot interrupt after 'ns' nanoseconds
// timer_num: 0 to (num_timers-1)
// ns: delay in nanoseconds
// vector: interrupt vector (32-255)
int hpet_timer_oneshot(uint8_t timer_num, uint64_t ns, uint8_t vector);

// Setup timer for periodic interrupts every 'period_ns' nanoseconds
// timer_num: 0 to (num_timers-1)
// period_ns: period in nanoseconds
// vector: interrupt vector (32-255)
int hpet_timer_periodic(uint8_t timer_num, uint64_t period_ns, uint8_t vector);

// Stop a timer
void hpet_timer_stop(uint8_t timer_num);

// === Calibration ===

// Calibrate another timer using HPET as reference
// Returns how many ticks of the target timer occur in 'ms' milliseconds
// counter_fn: pointer to a function that reads the timer counter
uint64_t hpet_calibrate_timer(volatile uint64_t *counter_fn, uint32_t ms);

#endif // HPET_H
