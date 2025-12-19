#ifndef APIC_H
#define APIC_H

#include <stdint.h>
#include <stdbool.h>

// === Core APIC Functions ===

#define VMM_FLAGS_PRESENT (1 << 0)
#define VMM_FLAGS_WRITE (1 << 1)
#define VMM_FLAGS_USER (1 << 2)
#define VMM_FLAGS_GLOBAL (1 << 8)
#define VMM_FLAGS_NO_CACHE ((1 << 4) | (1 << 3)) // PCD | PWT bits

// Initialize APIC (must call after acpi_init)
int apic_init(void);

// Check if APIC is initialized
bool apic_is_initialized(void);

// === Local APIC Functions ===

// Signal End of Interrupt (call after handling interrupt)
void lapic_eoi(void);

// Get current CPU's Local APIC ID
uint32_t lapic_get_id(void);

// Enable Local APIC on current CPU
void lapic_enable(void);

// Initialize Local APIC timer
// frequency_hz: desired interrupt frequency (e.g., 100 for 100Hz)
void lapic_timer_init(uint32_t frequency_hz);

// Send Inter-Processor Interrupt
void lapic_send_ipi(uint32_t dest, uint8_t vector);

// Send INIT IPI (used for SMP startup)
void lapic_send_init_ipi(uint8_t dest_apic_id);

// Send STARTUP IPI (used for SMP startup)
void lapic_send_startup_ipi(uint8_t dest_apic_id, uint8_t vector);

// === I/O APIC Functions ===

// Setup interrupt redirect entry
// irq: IRQ number (0-23)
// vector: interrupt vector (32-255)
// dest_apic_id: target CPU's APIC ID
// masked: true to initially mask the interrupt
void ioapic_set_redirect(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked);

// Mask (disable) an IRQ
void ioapic_mask_irq(uint8_t irq);

// Unmask (enable) an IRQ
void ioapic_unmask_irq(uint8_t irq);

// === SMP Functions ===

// Start an Application Processor
// apic_id: APIC ID of the CPU to start
// trampoline_addr: physical address of 16-bit startup code (must be < 1MB)
void apic_start_ap(uint8_t apic_id, uint32_t trampoline_addr);

void apic_debug_check(void);

void apic_init_ap(void);

#endif // APIC_H
