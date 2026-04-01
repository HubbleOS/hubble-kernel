#include "apic.h"
#include <acpi/acpi.h>
#include "higher_half.h"
#include <hubble/printk.h>
#include <io.h>
#include <hubble/string.h>
#include <mm/vmm.h>
#include <msr.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>

#include <asm.h>

// Local APIC register offsets
#define LAPIC_ID 0x020
#define LAPIC_VERSION 0x030
#define LAPIC_TPR 0x080		  // Task Priority Register
#define LAPIC_EOI 0x0B0		  // End of Interrupt
#define LAPIC_SVR 0x0F0		  // Spurious Interrupt Vector Register
#define LAPIC_ESR 0x280		  // Error Status Register
#define LAPIC_ICR_LOW 0x300	  // Interrupt Command Register (low)
#define LAPIC_ICR_HIGH 0x310  // Interrupt Command Register (high)
#define LAPIC_TIMER 0x320	  // LVT Timer Register
#define LAPIC_LINT0 0x350	  // LVT LINT0 Register
#define LAPIC_LINT1 0x360	  // LVT LINT1 Register
#define LAPIC_ERROR 0x370	  // LVT Error Register
#define LAPIC_TIMER_ICR 0x380 // Timer Initial Count
#define LAPIC_TIMER_CCR 0x390 // Timer Current Count
#define LAPIC_TIMER_DCR 0x3E0 // Timer Divide Configuration

#define LAPIC_TIMER_PERIODIC (1 << 17)
#define LAPIC_TIMER_VECTOR 32

// I/O APIC register offsets
#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_ARB 0x02
#define IOAPIC_REDTBL_BASE 0x10

#define IA32_APIC_BASE_MSR 0x1B
#define LAPIC_BASE_MASK 0xFFFFF000ULL

#define IA32_X2APIC_ICR 0x830

#define VMM_MAP_MMIO (VMM_MAP_NO_CACHE)

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

enum
{
	APIC_INIT_NONE = 0,
	APIC_INIT_LAPIC,
	APIC_INIT_IOAPIC,
	APIC_INIT_X2APIC,
	APIC_INIT_XAPIC
};

static uint8_t apic_mode = APIC_INIT_NONE;

static inline bool cpu_has_x2apic(void)
{
	uint32_t eax, ebx, ecx, edx;
	__asm__ volatile("cpuid"
					 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
					 : "a"(1));
	return ecx & (1 << 21);
}

// === Local APIC Functions ===

static inline uint32_t lapic_read(uint32_t reg)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint32_t msr = 0x800 + (reg >> 4);
		return (uint32_t)rdmsr(msr);
	}
	else
	{
		return apic_state.lapic_base[reg / 4];
	}
}

static inline void lapic_write(uint32_t reg, uint32_t val)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint32_t msr = 0x800 + (reg >> 4);
		wrmsr(msr, val);
	}
	else
	{
		apic_state.lapic_base[reg / 4] = val;
	}
}

void lapic_eoi(void)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		wrmsr(0x80B, 0);
		return;
	}
	else if (!apic_state.lapic_base)
		return;
	lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_get_id(void)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		return (uint32_t)rdmsr(0x802);
	}
	else
	{
		return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
	}
}

void lapic_enable(void)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t svr = rdmsr(0x80F);
		svr |= 0x100; // enable
		svr = (svr & ~0xFF) | 0xFF;
		wrmsr(0x80F, svr);
	}
	else
	{
		lapic_write(LAPIC_SVR, 0x1FF);
		lapic_write(LAPIC_ESR, 0);
		lapic_write(LAPIC_ESR, 0);
		lapic_write(LAPIC_TPR, 0);
	}
}

void lapic_timer_init(uint32_t frequency_hz)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk("ERROR: LAPIC not initialized\n");
		return;
	}

	printk("Testing HPET delay...\n");
	uint64_t hpet_before = hpet_get_counter();
	hpet_delay_ms(10);
	uint64_t hpet_after = hpet_get_counter();
	printk("HPET ticks in 10ms: %llu\n", hpet_after - hpet_before);

	lapic_write(LAPIC_TIMER_DCR, 0x3);

	// CRITICAL: Put timer in ONE-SHOT mode for calibration
	// (vector doesn't matter, we're not letting it fire)
	lapic_write(LAPIC_TIMER, 0xFF | (1 << 16)); // Masked, vector 0xFF

	// NOW start counting
	lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

	uint32_t ccr_start = lapic_read(LAPIC_TIMER_CCR);
	printk("Timer started, CCR = 0x%08x\n", ccr_start);

	// Wait 10ms
	hpet_delay_ms(10);

	// Read final value BEFORE stopping
	uint32_t ccr_final = lapic_read(LAPIC_TIMER_CCR);

	// Stop timer
	lapic_write(LAPIC_TIMER_ICR, 0);

	uint32_t elapsed = ccr_start - ccr_final;

	printk("CCR after 10ms: 0x%08x\n", ccr_final);
	printk("Elapsed ticks: %u\n", elapsed);

	if (elapsed == 0 || elapsed < 1000)
	{
		printk("ERROR: LAPIC timer calibration failed (elapsed too small)\n");
		return;
	}

	// elapsed ticks in 10ms -> ticks per second = elapsed * 100
	uint32_t ticks_per_interrupt = (elapsed * 100) / frequency_hz;

	if (ticks_per_interrupt == 0)
	{
		printk("ERROR: Frequency too high for LAPIC timer\n");
		return;
	}

	printk("Calculated: %u ticks for %u Hz interrupt\n", ticks_per_interrupt, frequency_hz);

	// Setup timer in periodic mode
	lapic_write(LAPIC_TIMER, LAPIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
	lapic_write(LAPIC_TIMER_DCR, 0x3);
	lapic_write(LAPIC_TIMER_ICR, ticks_per_interrupt);

	// verify ticks
	uint32_t icr = lapic_read(LAPIC_TIMER_ICR);
	printk("LAPIC timer ICR: 0x%08x\n", icr);

	// Verify it's configured correctly
	uint32_t lvt = lapic_read(LAPIC_TIMER);
	printk("LVT Timer: 0x%08x (Vector=%u, Periodic=%s, Masked=%s)\n",
		   lvt, lvt & 0xFF,
		   (lvt & (1 << 17)) ? "YES" : "NO",
		   (lvt & (1 << 16)) ? "YES" : "NO");

	printk("LAPIC timer initialized (%u Hz, %u ticks/int)\n",
		   frequency_hz, ticks_per_interrupt);
}

void lapic_send_ipi(uint32_t dest, uint8_t vector)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk("ERROR: LAPIC not initialized\n");
		return;
	}

	if (apic_mode == APIC_INIT_X2APIC)
	{
		// x2APIC: Fixed delivery mode
		uint64_t icr = ((uint64_t)dest << 32) |
					   (uint64_t)vector |
					   (0 << 8) | // Delivery Mode: Fixed
					   (1 << 14); // Level: Assert
		wrmsr(0x830, icr);
	}
	else
	{
		// xAPIC mode
		lapic_write(LAPIC_ICR_HIGH, dest << 24);
		lapic_write(LAPIC_ICR_LOW, vector | (1 << 14)); // Fixed delivery + Assert
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;
	}
}

void lapic_send_init_ipi(uint8_t dest_apic_id)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk("ERROR: LAPIC not initialized\n");
		return;
	}

	printk("Sending INIT IPI to APIC ID %u\n", dest_apic_id);

	if (apic_mode == APIC_INIT_X2APIC)
	{
		// INIT ASSERT (level = 1, trigger = level)
		uint64_t icr =
			((uint64_t)dest_apic_id << 32) |
			(5 << 8) |	// INIT
			(1 << 14) | // Level = 1 (assert)
			(1 << 15);	// Trigger = level

		wrmsr(IA32_X2APIC_ICR, icr);

		hpet_delay_ms(10);

		// INIT DEASSERT (level = 0, trigger = level)
		icr =
			((uint64_t)dest_apic_id << 32) |
			(5 << 8) | // INIT
			(1 << 15); // Trigger = level

		wrmsr(IA32_X2APIC_ICR, icr);
	}
	else
	{
		// xAPIC mode
		lapic_write(LAPIC_ICR_HIGH, ((uint32_t)dest_apic_id) << 24);

		// INIT, Level, Assert
		lapic_write(LAPIC_ICR_LOW, 0x4500);
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;

		// Deassert
		lapic_write(LAPIC_ICR_LOW, 0x4500 | (1 << 15));
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;
	}

	printk("INIT IPI sent\n");
}

void lapic_send_startup_ipi(uint8_t dest_apic_id, uint8_t vector)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk("ERROR: LAPIC base not initialized\n");
		return;
	}

	printk("Sending STARTUP IPI");
	printk("Destination APIC ID: %u\n", dest_apic_id);
	printk("Vector: 0x%02x\n", vector);
	printk("Target physical address: 0x%05x\n", (uint32_t)vector << 12);
	printk("APIC Mode: %s\n", apic_mode == APIC_INIT_X2APIC ? "x2APIC" : "xAPIC");

	// Check LAPIC is enabled
	uint32_t svr = lapic_read(LAPIC_SVR);
	printk("LAPIC SVR: 0x%08x %s\n", svr,
		   (svr & 0x100) ? "[ENABLED]" : "[DISABLED!]");

	if (!(svr & 0x100))
	{
		printk("ERROR: LAPIC not enabled!\n");
		return;
	}

	// Check error status before sending
	uint32_t esr_before = lapic_read(LAPIC_ESR);
	printk("ESR before: 0x%08x\n", esr_before);

	// x2APIC uses a single 64-bit ICR write, xAPIC uses two 32-bit writes
	if (apic_mode == APIC_INIT_X2APIC)
	{
		// x2APIC: Single 64-bit MSR write to ICR (MSR 0x830)
		// Format: [63:32] = destination, [31:0] = ICR_LOW fields
		uint64_t icr = ((uint64_t)dest_apic_id << 32) | // Destination
					   (vector & 0xFF) |				// Vector
					   (6 << 8) |						// Delivery Mode: STARTUP (110b)
					   (0 << 11) |						// Destination Mode: Physical
					   (1 << 14) |						// Level: Assert
					   (0 << 15);

		printk("Writing x2APIC ICR (single 64-bit MSR):\n");
		printk("  Destination (bits 63:32): 0x%08x\n", dest_apic_id);
		printk("  Vector (bits 0-7):        0x%02x\n", vector & 0xFF);
		printk("  Delivery mode (bits 8-10): %u (STARTUP)\n", 6);
		printk("  Full ICR value:           0x%016llx\n", icr);

		// Write ICR as a single atomic operation
		wrmsr(0x830, icr);

		// x2APIC ICR writes are self-synchronizing, no busy-wait needed
		printk("x2APIC ICR write completed (self-synchronizing)\n");
	}
	else
	{
		// xAPIC: Traditional two-register write (ICR_HIGH then ICR_LOW)
		uint32_t icr_high = ((uint32_t)dest_apic_id) << 24;
		printk("Writing xAPIC ICR_HIGH: 0x%08x\n", icr_high);
		lapic_write(LAPIC_ICR_HIGH, icr_high);

		// Verify write
		uint32_t icr_high_read = lapic_read(LAPIC_ICR_HIGH);
		printk("ICR_HIGH readback: 0x%08x %s\n", icr_high_read,
			   (icr_high_read == icr_high) ? "[OK]" : "[MISMATCH!]");

		// Memory barrier
		asm volatile("mfence" ::: "memory");

		uint32_t icr_low = (vector & 0xFF) | (6 << 8);

		printk("Writing xAPIC ICR_LOW: 0x%08x\n", icr_low);
		printk("  Vector field (bits 0-7):    0x%02x\n", icr_low & 0xFF);
		printk("  Delivery mode (bits 8-10):  %u (STARTUP)\n", (icr_low >> 8) & 0x7);
		printk("  Level (bit 14):             %u\n", (icr_low >> 14) & 1);
		printk("  Trigger (bit 15):           %u\n", (icr_low >> 15) & 1);

		lapic_write(LAPIC_ICR_LOW, icr_low);

		// Immediate readback
		uint32_t icr_low_read = lapic_read(LAPIC_ICR_LOW);
		printk("ICR_LOW readback: 0x%08x\n", icr_low_read);
		printk("  Delivery Status (bit 12): %s\n",
			   (icr_low_read & (1 << 12)) ? "Send Pending" : "Idle");

		// Wait for delivery to complete
		int timeout = 100000;
		while ((lapic_read(LAPIC_ICR_LOW) & (1 << 12)) && timeout > 0)
		{
			cpu_pause();
			timeout--;
		}

		if (timeout == 0)
		{
			printk("WARNING: SIPI delivery timeout!\n");
		}
		else
		{
			printk("SIPI delivery completed (iterations left: %d)\n", timeout);
		}
	}

	// Check error status after
	uint32_t esr_after = lapic_read(LAPIC_ESR);
	printk("ESR after: 0x%08x\n", esr_after);

	if (esr_after != 0)
	{
		printk("ERROR: LAPIC errors detected!\n");
		if (esr_after & 0x01)
			printk("  - Send Checksum Error\n");
		if (esr_after & 0x02)
			printk("  - Receive Checksum Error\n");
		if (esr_after & 0x04)
			printk("  - Send Accept Error\n");
		if (esr_after & 0x08)
			printk("  - Receive Accept Error\n");
		if (esr_after & 0x20)
			printk("  - Send Illegal Vector\n");
		if (esr_after & 0x40)
			printk("  - Receive Illegal Vector\n");
		if (esr_after & 0x80)
			printk("  - Illegal Register Address\n");
	}

	printk("STARTUP IPI Complete\n");

	// At the END of lapic_send_startup_ipi(), add:
	printk("POST-SIPI VERIFICATION");

	// Small delay to let SIPI process
	for (volatile int i = 0; i < 100000; i++)
		cpu_pause();

	// Read ICR to check delivery status
	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t icr = rdmsr(0x830);
		printk("ICR after delay: 0x%016llx\n", icr);
		printk("  Delivery Status (bit 12): %s\n",
			   (icr & (1ULL << 12)) ? "STILL PENDING (!)" : "Complete");

		if (icr & (1ULL << 12))
		{
			printk("ERROR: SIPI delivery still pending - may not have been sent!\n");
		}
	}

	// Check errors again
	uint32_t esr = lapic_read(LAPIC_ESR);
	if (esr != 0)
	{
		printk("ERROR: LAPIC ESR shows errors: 0x%08x\n", esr);
		if (esr & 0x04)
		{
			printk("  Send Accept Error - Destination AP didn't accept IPI!\n");
			printk("  This means the AP either doesn't exist or isn't ready.\n");
		}
	}

	printk("END POST-SIPI VERIFICATION\n");
}
void apic_debug_check(void)
{
	printk("LAPIC Debug Check");
	printk("APIC Mode: %s\n", apic_mode == APIC_INIT_X2APIC ? "x2APIC" : "xAPIC");

	if (apic_mode == APIC_INIT_X2APIC)
	{
		printk("x2APIC mode - using MSR access\n");
		uint32_t id = (uint32_t)rdmsr(0x802);
		printk("LAPIC ID (MSR 0x802): 0x%08x\n", id);

		uint32_t version = (uint32_t)rdmsr(0x803);
		printk("LAPIC Version (MSR 0x803): 0x%08x\n", version);

		uint64_t svr = rdmsr(0x80F);
		printk("LAPIC SVR (MSR 0x80F): 0x%llx %s\n", svr,
			   (svr & 0x100) ? "[ENABLED]" : "[DISABLED!]");

		printk("End LAPIC Debug\n");
		return;
	}

	// xAPIC mode - MMIO access
	uint64_t lapic_phys = acpi_get_lapic_address();
	printk("LAPIC physical: 0x%lx\n", lapic_phys);
	printk("apic_state.lapic_base: %p\n", apic_state.lapic_base);

	if (!apic_state.lapic_base)
	{
		printk("ERROR: LAPIC base is NULL!\n");
		return;
	}

	// Try to read LAPIC ID
	volatile uint32_t *lapic_id_reg = apic_state.lapic_base + (LAPIC_ID / 4);
	printk("Reading from: %p\n", lapic_id_reg);

	uint32_t id = *lapic_id_reg;
	printk("LAPIC ID register: 0x%08x\n", id);

	if (id == 0xFFFFFFFF || id == 0x00000000)
	{
		printk("ERROR: LAPIC not accessible (got 0x%08x)!\n", id);
		printk("       Either not mapped or wrong address\n");
	}

	// Check if page is mapped
	printk("\nChecking page table mapping:\n");
	uint64_t virt = (uint64_t)apic_state.lapic_base;
	printk("Virtual address: 0x%lx\n", virt);

	// Try direct physical access (if identity mapped)
	volatile uint32_t *direct = (volatile uint32_t *)(0xFFFF800000000000ULL + lapic_phys);
	printk("Direct mapping attempt: %p\n", direct);
	uint32_t direct_read = *direct;
	printk("Direct read: 0x%08x\n", direct_read);

	printk("End LAPIC Debug\n");
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

static void setup_iso_callback(uint8_t irq_source, uint32_t gsi, uint16_t flags, void *ctx)
{
	uint8_t vector = 32 + irq_source;

	bool active_low = flags & 0x2;
	bool level_triggered = flags & 0x8;

	uint32_t low = vector;
	uint32_t high = ((uint32_t)apic_state.bsp_id) << 24;

	if (active_low)
		low |= (1 << 13);
	if (level_triggered)
		low |= (1 << 15);

	low |= (1 << 16); //  MASK

	uint8_t reg_low = IOAPIC_REDTBL_BASE + (gsi * 2);
	uint8_t reg_high = IOAPIC_REDTBL_BASE + (gsi * 2) + 1;

	ioapic_write(reg_high, high);
	ioapic_write(reg_low, low);
}

int lapic_init_x2apic(void)
{
	// printk("x2APIC mode enabled\n");

	uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);

	// enable APIC + x2APIC
	apic_base |= (1ULL << 11);
	apic_base |= (1ULL << 10);

	wrmsr(IA32_APIC_BASE_MSR, apic_base);

	apic_mode = APIC_INIT_X2APIC;
	apic_state.lapic_base = NULL;

	// SVR via MSR
	wrmsr(0x80F, 0x100 | 0xFF);

	// clear ESR
	wrmsr(0x828, 0);
	wrmsr(0x828, 0);

	return 0;
}

int lapic_init_xapic(void)
{
	printk("xAPIC mode enabled\n");

	uint64_t lapic_phys = acpi_get_lapic_address();

	apic_state.lapic_base =
		(volatile uint32_t *)PHYS_TO_VIRT_MMIO(lapic_phys);

	vmm_map_page(
		(uint64_t)apic_state.lapic_base,
		lapic_phys,
		VMM_MAP_MMIO);

	apic_mode = APIC_INIT_XAPIC;

	lapic_write(LAPIC_SVR, 0x100 | 0xFF);
	lapic_write(LAPIC_ESR, 0);
	lapic_write(LAPIC_ESR, 0);

	return 0;
}

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

	printk("Local APIC physical address: 0x%lx\n", lapic_phys);

	// CRITICAL: LAPIC is MMIO at high address (typically 0xFEE00000)
	// It needs explicit mapping with cache disabled
	uint64_t lapic_virt;

	lapic_virt = IS_HIGH_MMIO(lapic_phys) ? PHYS_TO_VIRT_MMIO(lapic_phys) : PHYS_TO_VIRT(lapic_phys);

	if (cpu_has_x2apic())
	{
		printk("ERROR: x2APIC mode detected - MMIO won't work!\n");
		lapic_init_x2apic();
	}
	else
	{
		lapic_init_xapic();
	}

	uint64_t apic_base = rdmsr(0x1B);

	bool apic_enabled = apic_base & (1ULL << 11);
	bool x2apic_enabled = apic_base & (1ULL << 10);

	printk("APIC=%d x2APIC=%d\n", apic_enabled, x2apic_enabled);

	// Get BSP (Bootstrap Processor) APIC ID
	apic_state.bsp_id = lapic_get_id();
	printk("BSP APIC ID: %u\n", apic_state.bsp_id);

	lapic_enable();

	// Find I/O APIC
	struct
	{
		bool found;
		uint32_t address;
		uint32_t gsi_base;
	} ioapic_ctx = {0};

	acpi_enum_ioapics(find_ioapic, &ioapic_ctx);

	if (!ioapic_ctx.found)
	{
		printk("No I/O APIC found\n");
		return -1;
	}

	// Map I/O APIC similarly
	uint64_t ioapic_phys = (uint64_t)ioapic_ctx.address;
	uint64_t ioapic_virt;

	if (IS_HIGH_MMIO(ioapic_phys))
	{
		printk("I/O APIC is high MMIO, mapping explicitly...\n");

		ioapic_virt = PHYS_TO_VIRT_MMIO(ioapic_phys);

		int map_result = vmm_map_page(ioapic_virt, ioapic_phys,
									  VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE |
										  VMM_FLAGS_NO_CACHE | VMM_FLAGS_GLOBAL);

		if (map_result != 0)
		{
			printk("ERROR: Failed to map I/O APIC page\n");
			return -1;
		}

		printk("I/O APIC mapped: phys=0x%lx -> virt=0x%lx\n", ioapic_phys, ioapic_virt);
	}
	else
	{
		ioapic_virt = PHYS_TO_VIRT(ioapic_phys);
	}

	apic_state.ioapic_base = (volatile uint32_t *)ioapic_virt;
	apic_state.ioapic_gsi_base = ioapic_ctx.gsi_base;

	// Test I/O APIC access
	printk("Testing I/O APIC access...\n");
	uint32_t ver = ioapic_read(IOAPIC_REG_VER);

	if (ver == 0 || ver == 0xFFFFFFFF)
	{
		printk("ERROR: I/O APIC not accessible!\n");
		return -1;
	}

	printk("I/O APIC is accessible\n");

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
	printk("APIC initialized successfully\n");
	return 0;
}

void apic_init_ap(void)
{
	// APs just need to enable their local APIC
	// They DON'T touch I/O APIC or ACPI

	if (cpu_has_x2apic())
	{
		lapic_init_x2apic(); // Enable x2APIC mode on this AP
	}
	else
	{
		lapic_init_xapic(); // Enable xAPIC mode on this AP
	}

	lapic_enable(); // Enable this AP's local APIC

	// Optional: get and print this AP's ID
	uint8_t apic_id = lapic_get_id();
	// printk("AP %u APIC initialized\n", apic_id);
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

	printk("Starting AP with APIC ID %u", apic_id);
	printk("Trampoline at physical 0x%x\n", trampoline_addr);

	uint8_t vector = (trampoline_addr >> 12) & 0xFF;
	printk("SIPI vector: 0x%02x (starts at 0x%05x)\n", vector, vector << 12);

	// === STEP 1: INIT IPI (puts AP in wait-for-SIPI state) ===
	printk("\nStep 1: Sending INIT IPI...\n");

	lapic_send_init_ipi(apic_id);
	// === STEP 2: Wait 10ms for INIT to take effect ===
	printk("Step 2: Waiting 10ms...\n");
	for (volatile int i = 0; i < 10000000; i++)
		cpu_pause();

	// === STEP 3: First STARTUP IPI ===
	printk("Step 3: Sending first SIPI...\n");
	lapic_send_startup_ipi(apic_id, vector);

	// === STEP 4: Wait 200us ===
	printk("Step 4: Waiting 200us...\n");
	for (volatile int i = 0; i < 200000; i++)
		cpu_pause();

	// === STEP 5: Second STARTUP IPI (per Intel MP spec) ===
	printk("Step 5: Sending second SIPI...\n");
	lapic_send_startup_ipi(apic_id, vector);

	printk("AP startup sequence complete\n");
}

static bool using_apic = false;

void apic_init_bsp(void)
{
	apic_init();
	lapic_enable();
	ioapic_unmask_irq(1);
	ioapic_unmask_irq(2);
	ioapic_unmask_irq(12);
	lapic_timer_init(100);
	irq_install_handler(0, lapic_timer_handler);
	using_apic = true;
}
