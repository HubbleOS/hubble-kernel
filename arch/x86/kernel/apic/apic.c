#include "apic.h"
#include <acpi/acpi.h>
#include "higher_half.h"
#include <printk.h>
#include <io.h>
#include <string.h>

// Local APIC register offsets
#define LAPIC_ID 0x020
#define LAPIC_VERSION 0x030
#define LAPIC_TPR 0x080	      // Task Priority Register
#define LAPIC_EOI 0x0B0	      // End of Interrupt
#define LAPIC_SVR 0x0F0	      // Spurious Interrupt Vector Register
#define LAPIC_ESR 0x280	      // Error Status Register
#define LAPIC_ICR_LOW 0x300   // Interrupt Command Register (low)
#define LAPIC_ICR_HIGH 0x310  // Interrupt Command Register (high)
#define LAPIC_TIMER 0x320     // LVT Timer Register
#define LAPIC_LINT0 0x350     // LVT LINT0 Register
#define LAPIC_LINT1 0x360     // LVT LINT1 Register
#define LAPIC_ERROR 0x370     // LVT Error Register
#define LAPIC_TIMER_ICR 0x380 // Timer Initial Count
#define LAPIC_TIMER_CCR 0x390 // Timer Current Count
#define LAPIC_TIMER_DCR 0x3E0 // Timer Divide Configuration

// I/O APIC register offsets
#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_ARB 0x02
#define IOAPIC_REDTBL_BASE 0x10

// Global state
static struct
{
	volatile uint32_t *lapic_base;
	volatile uint32_t *ioapic_base;
	uint8_t bsp_id;
	uint32_t ioapic_gsi_base;
	uint32_t ioapic_max_redirect;
	bool initialized;
} apic_state = {0};

// === Local APIC Functions ===

static inline uint32_t lapic_read(uint32_t reg)
{
	return apic_state.lapic_base[reg / 4];
}

static inline void lapic_write(uint32_t reg, uint32_t value)
{
	apic_state.lapic_base[reg / 4] = value;
}

void lapic_eoi(void)
{
	if (!apic_state.lapic_base)
		return;
	lapic_write(LAPIC_EOI, 0);
}

uint8_t lapic_get_id(void)
{
	if (!apic_state.lapic_base)
		return 0;
	return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

void lapic_enable(void)
{
	if (!apic_state.lapic_base)
		return;

	// Enable Local APIC by setting bit 8 in SVR
	// and set spurious interrupt vector to 0xFF
	lapic_write(LAPIC_SVR, 0x1FF);

	// Clear error status register (write 0 twice)
	lapic_write(LAPIC_ESR, 0);
	lapic_write(LAPIC_ESR, 0);

	// Set Task Priority to 0 (accept all interrupts)
	lapic_write(LAPIC_TPR, 0);

	printk("Local APIC enabled (ID=%u)\n", lapic_get_id());
}

void lapic_timer_init(uint32_t frequency_hz)
{
	if (!apic_state.lapic_base)
		return;

	// Set divide value to 16
	lapic_write(LAPIC_TIMER_DCR, 0x3);

	// Set initial count for calibration
	lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

	// Wait 10ms using PIT or other timer
	// TODO: Implement proper timing
	for (volatile int i = 0; i < 1000000; i++)
		;

	// Read current count
	uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CCR);

	// Calculate ticks per desired frequency
	uint32_t ticks_per_interrupt = elapsed / (frequency_hz / 100);

	// Setup timer in periodic mode (vector 32, periodic mode)
	lapic_write(LAPIC_TIMER, 0x20020); // Vector 32, Periodic
	lapic_write(LAPIC_TIMER_DCR, 0x3); // Divide by 16
	lapic_write(LAPIC_TIMER_ICR, ticks_per_interrupt);

	printk("LAPIC timer initialized (%u Hz)\n", frequency_hz);
}

void lapic_send_ipi(uint8_t dest_apic_id, uint8_t vector)
{
	if (!apic_state.lapic_base)
		return;

	// Set destination
	lapic_write(LAPIC_ICR_HIGH, ((uint32_t)dest_apic_id) << 24);

	// Send IPI (physical mode, edge triggered, assert)
	lapic_write(LAPIC_ICR_LOW, vector | (1 << 14));

	// Wait for delivery
	while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
		;
}

void lapic_send_init_ipi(uint8_t dest_apic_id)
{
	if (!apic_state.lapic_base)
		return;

	lapic_write(LAPIC_ICR_HIGH, ((uint32_t)dest_apic_id) << 24);
	lapic_write(LAPIC_ICR_LOW, 0x4500); // INIT, Level, Assert
	while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
		;

	lapic_write(LAPIC_ICR_LOW, 0x4500 | (1 << 15)); // Deassert
	while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
		;
}

void lapic_send_startup_ipi(uint8_t dest_apic_id, uint8_t vector)
{
	if (!apic_state.lapic_base)
		return;

	lapic_write(LAPIC_ICR_HIGH, ((uint32_t)dest_apic_id) << 24);
	lapic_write(LAPIC_ICR_LOW, 0x4600 | vector); // SIPI
	while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
		;
}

// === I/O APIC Functions ===

static inline uint32_t ioapic_read(uint8_t reg)
{
	apic_state.ioapic_base[0] = reg;
	return apic_state.ioapic_base[4];
}

static inline void ioapic_write(uint8_t reg, uint32_t value)
{
	apic_state.ioapic_base[0] = reg;
	apic_state.ioapic_base[4] = value;
}

void ioapic_set_redirect(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked)
{
	if (!apic_state.ioapic_base)
		return;

	uint32_t low = vector;
	uint32_t high = ((uint32_t)dest_apic_id) << 24;

	if (masked)
		low |= (1 << 16); // Mask bit

	// Active high, edge triggered (can be changed based on ISO flags)
	uint8_t reg_low = IOAPIC_REDTBL_BASE + (irq * 2);
	uint8_t reg_high = IOAPIC_REDTBL_BASE + (irq * 2) + 1;

	ioapic_write(reg_high, high);
	ioapic_write(reg_low, low);
}

void ioapic_mask_irq(uint8_t irq)
{
	if (!apic_state.ioapic_base)
		return;

	uint8_t reg = IOAPIC_REDTBL_BASE + (irq * 2);
	uint32_t val = ioapic_read(reg);
	ioapic_write(reg, val | (1 << 16));
}

void ioapic_unmask_irq(uint8_t irq)
{
	if (!apic_state.ioapic_base)
		return;

	uint8_t reg = IOAPIC_REDTBL_BASE + (irq * 2);
	uint32_t val = ioapic_read(reg);
	ioapic_write(reg, val & ~(1 << 16));
}

// === Initialization ===

static void disable_pic(void)
{
	// Mask all interrupts on PIC
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);

	printk("Legacy PIC disabled\n");
}

static void setup_iso_callback(uint8_t irq_source, uint32_t gsi, uint16_t flags, void *ctx)
{
	// Map IRQ to interrupt vector (32 + IRQ)
	uint8_t vector = 32 + irq_source;

	// Determine polarity and trigger mode from flags
	bool active_low = flags & 0x2;
	bool level_triggered = flags & 0x8;

	// Setup redirect entry
	uint32_t low = vector;
	uint32_t high = ((uint32_t)apic_state.bsp_id) << 24;

	if (active_low)
		low |= (1 << 13); // Active low
	if (level_triggered)
		low |= (1 << 15); // Level triggered

	uint8_t reg_low = IOAPIC_REDTBL_BASE + (gsi * 2);
	uint8_t reg_high = IOAPIC_REDTBL_BASE + (gsi * 2) + 1;

	ioapic_write(reg_high, high);
	ioapic_write(reg_low, low);

	printk("IRQ %u -> GSI %u (vector %u, %s, %s)\n",
	       irq_source, gsi, vector,
	       active_low ? "active_low" : "active_high",
	       level_triggered ? "level" : "edge");
}

int apic_init(void)
{
	if (!acpi_is_initialized())
	{
		printk("ACPI not initialized\n");
		return -1;
	}

	// Get Local APIC address from ACPI
	uint64_t lapic_phys = acpi_get_lapic_address();
	if (!lapic_phys)
	{
		printk("Local APIC address not found\n");
		return -1;
	}

	apic_state.lapic_base = (volatile uint32_t *)PHYS_TO_VIRT(lapic_phys);
	printk("Local APIC at phys=0x%lx virt=%p\n", lapic_phys, apic_state.lapic_base);

	// Get BSP (Bootstrap Processor) APIC ID
	apic_state.bsp_id = lapic_get_id();
	printk("BSP APIC ID: %u\n", apic_state.bsp_id);

	// Disable legacy PIC
	disable_pic();

	// Enable Local APIC
	lapic_enable();

	// Find I/O APIC
	struct
	{
		bool found;
		uint32_t address;
		uint32_t gsi_base;
	} ioapic_ctx = {0};

	void find_ioapic(uint8_t id, uint32_t addr, uint32_t gsi, void *ctx)
	{
		struct
		{
			bool found;
			uint32_t address;
			uint32_t gsi_base;
		} *data = ctx;
		if (!data->found)
		{
			data->found = true;
			data->address = addr;
			data->gsi_base = gsi;
			printk("I/O APIC found: ID=%u addr=0x%x GSI_base=%u\n", id, addr, gsi);
		}
	}

	acpi_enum_ioapics(find_ioapic, &ioapic_ctx);

	if (!ioapic_ctx.found)
	{
		printk("No I/O APIC found\n");
		return -1;
	}

	apic_state.ioapic_base = (volatile uint32_t *)PHYS_TO_VIRT((uint64_t)ioapic_ctx.address);
	apic_state.ioapic_gsi_base = ioapic_ctx.gsi_base;

	// Get I/O APIC version and max redirects
	uint32_t ver = ioapic_read(IOAPIC_REG_VER);
	apic_state.ioapic_max_redirect = ((ver >> 16) & 0xFF) + 1;
	printk("I/O APIC version: 0x%x, max redirects: %u\n",
	       ver & 0xFF, apic_state.ioapic_max_redirect);

	// Setup default redirects (IRQ -> Vector 32+IRQ, BSP)
	for (uint32_t i = 0; i < apic_state.ioapic_max_redirect; i++)
	{
		ioapic_set_redirect(i, 32 + i, apic_state.bsp_id, true);
	}

	// Apply Interrupt Source Overrides from ACPI
	acpi_enum_isos(setup_iso_callback, NULL);

	apic_state.initialized = true;
	printk("APIC initialized\n");
	return 0;
}

bool apic_is_initialized(void)
{
	return apic_state.initialized;
}

// === SMP Support ===

void apic_start_ap(uint8_t apic_id, uint32_t trampoline_addr)
{
	if (!apic_state.initialized)
		return;

	printk("Starting AP with APIC ID %u at 0x%x\n", apic_id, trampoline_addr);

	// Send INIT IPI
	lapic_send_init_ipi(apic_id);

	// Wait 10ms
	for (volatile int i = 0; i < 10000000; i++)
		;

	// Send STARTUP IPI (vector = page in 4K units)
	uint8_t vector = (trampoline_addr >> 12) & 0xFF;
	lapic_send_startup_ipi(apic_id, vector);

	// Wait 200us
	for (volatile int i = 0; i < 200000; i++)
		;

	// Send second STARTUP IPI (per Intel spec)
	lapic_send_startup_ipi(apic_id, vector);
}
