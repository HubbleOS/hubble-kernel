/**
 * @file apic.h
 * @brief APIC, Local APIC, I/O APIC, and SMP startup API
 *
 * Declares the public interface for initialising and using the
 * Advanced Programmable Interrupt Controller, including LAPIC
 * timer, IPI delivery, I/O APIC redirection, and AP bring-up.
 */

#ifndef APIC_H
#define APIC_H

#include <stdbool.h>
#include <stdint.h>

/* -- MMIO Mapping Flags ---------------------------------------- */

#define VMM_FLAGS_PRESENT (1 << 0)
#define VMM_FLAGS_WRITE (1 << 1)
#define VMM_FLAGS_USER (1 << 2)
#define VMM_FLAGS_GLOBAL (1 << 8)
#define VMM_FLAGS_NO_CACHE ((1 << 4) | (1 << 3))

/* -- Core APIC Functions --------------------------------------- */

int apic_init(void);
bool apic_is_initialized(void);

/* -- Local APIC ------------------------------------------------ */

void lapic_eoi(void);
uint32_t lapic_get_id(void);
void lapic_enable(void);
void lapic_timer_init(uint32_t frequency_hz);
void lapic_send_ipi(uint32_t dest, uint8_t vector);
void lapic_send_init_ipi(uint8_t dest_apic_id);
void lapic_send_startup_ipi(uint8_t dest_apic_id, uint8_t vector);

/* -- I/O APIC -------------------------------------------------- */

void ioapic_set_redirect(uint8_t irq, uint8_t vector, uint8_t dest_apic_id,
                         bool masked);
void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);

/* -- SMP ------------------------------------------------------- */

void apic_start_ap(uint8_t apic_id, uint32_t trampoline_addr);
void apic_debug_check(void);
void apic_init_ap(void);
void apic_init_bsp(void);

#endif /* APIC_H */
