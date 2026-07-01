/**
 * @file hpet.h
 * @brief HPET (High Precision Event Timer) driver interface
 *
 * Provides initialisation, counter access, time conversion,
 * busy-wait delays, and one-shot / periodic timer setup.
 */

#ifndef HPET_H
#define HPET_H

#include <stdbool.h>
#include <stdint.h>

/* -- Core HPET Functions --------------------------------------- */

int hpet_init(void);
bool hpet_is_initialized(void);
uint64_t hpet_get_frequency(void);

/* -- Counter Access -------------------------------------------- */

uint64_t hpet_get_counter(void);
uint64_t hpet_get_time_ns(void);

/* -- Time Conversion ------------------------------------------- */

uint64_t hpet_ticks_to_ns(uint64_t ticks);
uint64_t hpet_ns_to_ticks(uint64_t ns);

/* -- Delays (Busy-Wait) ---------------------------------------- */

void hpet_delay_ns(uint64_t ns);
void hpet_delay_us(uint64_t us);
void hpet_delay_ms(uint64_t ms);

/* -- Timer Setup (for interrupts) ------------------------------ */

int hpet_timer_oneshot(uint8_t timer_num, uint64_t ns, uint8_t vector);
int hpet_timer_periodic(uint8_t timer_num, uint64_t period_ns, uint8_t vector);
void hpet_timer_stop(uint8_t timer_num);

/* -- Calibration ----------------------------------------------- */

uint64_t hpet_calibrate_timer(volatile uint64_t *counter_fn, uint32_t ms);

#endif /* HPET_H */
