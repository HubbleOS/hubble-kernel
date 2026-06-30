/**
 * @file apic.c
 * @brief APIC / LAPIC / I/O APIC driver and SMP startup
 *
 * Implements xAPIC and x2APIC initialisation, Local APIC timer
 * calibration and configuration, IPI delivery (INIT, STARTUP,
 * fixed), I/O APIC redirection entry programming with ISO
 * support, and the BSP/AP bring-up sequence for SMP.
 */

#include "apic.h"
#include <acpi/acpi.h>
#include <asm.h>
#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <mm/vmm.h>
#include <msr.h>
#include <smp/scheduler.h>

#include "higher_half.h"

/* ── Local APIC Register Offsets ─────────────────────────────── */

#define LAPIC_ID          0x020
#define LAPIC_VERSION     0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0
#define LAPIC_ESR         0x280
#define LAPIC_ICR_LOW     0x300
#define LAPIC_ICR_HIGH    0x310
#define LAPIC_TIMER       0x320
#define LAPIC_LINT0       0x350
#define LAPIC_LINT1       0x360
#define LAPIC_ERROR       0x370
#define LAPIC_TIMER_ICR   0x380
#define LAPIC_TIMER_CCR   0x390
#define LAPIC_TIMER_DCR   0x3E0

#define LAPIC_TIMER_PERIODIC (1 << 17)
#define LAPIC_TIMER_VECTOR   32

/* ── I/O APIC Register Offsets ───────────────────────────────── */

#define IOAPIC_REG_ID       0x00
#define IOAPIC_REG_VER      0x01
#define IOAPIC_REG_ARB      0x02
#define IOAPIC_REDTBL_BASE  0x10

#define IA32_APIC_BASE_MSR  0x1B
#define LAPIC_BASE_MASK     0xFFFFF000ULL
#define IA32_X2APIC_ICR     0x830

#define VMM_MAP_MMIO (VMM_MAP_NO_CACHE)

/* ── Global State ────────────────────────────────────────────── */

static struct
{
	volatile uint32_t *lapic_base;
	volatile uint32_t *ioapic_base;
	uint8_t  bsp_id;
	uint32_t ioapic_gsi_base;
	uint32_t ioapic_max_redirect;
	bool     initialized;
} apic_state = {0};

enum
{
	APIC_INIT_NONE   = 0,
	APIC_INIT_LAPIC,
	APIC_INIT_IOAPIC,
	APIC_INIT_X2APIC,
	APIC_INIT_XAPIC
};

static uint8_t apic_mode = APIC_INIT_NONE;

/* ── MMIO Helpers (static) ───────────────────────────────────── */

/**
 * @brief Check whether a virtual address has a full page-table walk
 *
 * @param virt Virtual address to check
 * @return true if a page is mapped at @p virt
 */
static bool apic_mmio_is_mapped(uint64_t virt)
{
	uint64_t *pml4 = pml4_table();
	if (!(pml4[PML4_INDEX(virt)] & PTE_PRESENT))
		return false;

	uint64_t *pdpt = pdpt_table(virt);
	if (!(pdpt[PDPT_INDEX(virt)] & PTE_PRESENT))
		return false;

	uint64_t *pd = pd_table(virt);
	if (!(pd[PD_INDEX(virt)] & PTE_PRESENT))
		return false;

	if (pd[PD_INDEX(virt)] & PTE_HUGE)
		return true;

	uint64_t *pt = pt_table(virt);
	return pt[PT_INDEX(virt)] & PTE_PRESENT;
}

/**
 * @brief Map one page of APIC MMIO space uncacheable
 *
 * @param phys Physical address to map
 * @return 0 on success, -1 on failure
 */
static int map_apic_mmio_page(uint64_t phys)
{
	uint64_t page = phys & ~0xFFFULL;
	uint64_t virt = phys_to_virt(page);

	if (apic_mmio_is_mapped(virt))
		return 0;

	if (vmm_map_page(virt, page, VMM_MAP_MMIO) < 0)
	{
		printk(KERN_ERR "ERROR: failed to map APIC MMIO phys=0x%lx virt=%p\n",
		       page, (void *)virt);
		return -1;
	}

	return 0;
}

/**
 * @brief Check CPUID for x2APIC support
 *
 * @return true if the CPU supports x2APIC
 */
static inline bool cpu_has_x2apic(void)
{
	uint32_t eax, ebx, ecx, edx;
	__asm__ volatile("cpuid"
			 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			 : "a"(1));
	return ecx & (1 << 21);
}

/* ── LAPIC Register Access (inline) ──────────────────────────── */

/**
 * @brief Read a Local APIC register
 *
 * Dispatches to MSR (x2APIC) or MMIO (xAPIC) access.
 *
 * @param reg Register offset
 * @return 32-bit register value
 */
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

/**
 * @brief Write a Local APIC register
 *
 * @param reg Register offset
 * @param val 32-bit value to write
 */
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

/* ── Local APIC Public API ───────────────────────────────────── */

/**
 * @brief Signal End-Of-Interrupt to the Local APIC
 */
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

/**
 * @brief Get the APIC ID of the current CPU
 *
 * @return APIC ID
 */
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

/**
 * @brief Enable the Local APIC on the current CPU
 */
void lapic_enable(void)
{
	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t svr = rdmsr(0x80F);
		svr |= 0x100;
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

/**
 * @brief Calibrate and start the LAPIC timer in periodic mode
 *
 * Uses HPET as a reference to measure the LAPIC timer frequency,
 * then programs the timer to fire at @p frequency_hz.
 *
 * @param frequency_hz Desired interrupt frequency
 */
void lapic_timer_init(uint32_t frequency_hz)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk(KERN_ERR "ERROR: LAPIC not initialized\n");
		return;
	}

	printk(KERN_INFO "Testing HPET delay...\n");
	uint64_t hpet_before = hpet_get_counter();
	hpet_delay_ms(10);
	uint64_t hpet_after = hpet_get_counter();
	printk(KERN_INFO "HPET ticks in 10ms: %llu\n", hpet_after - hpet_before);

	lapic_write(LAPIC_TIMER_DCR, 0x3);

	lapic_write(LAPIC_TIMER, 0xFF | (1 << 16));

	lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

	uint32_t ccr_start = lapic_read(LAPIC_TIMER_CCR);
	printk(KERN_INFO "Timer started, CCR = 0x%08x\n", ccr_start);

	hpet_delay_ms(10);

	uint32_t ccr_final = lapic_read(LAPIC_TIMER_CCR);

	lapic_write(LAPIC_TIMER_ICR, 0);

	uint32_t elapsed = ccr_start - ccr_final;

	printk(KERN_INFO "CCR after 10ms: 0x%08x\n", ccr_final);
	printk(KERN_INFO "Elapsed ticks: %u\n", elapsed);

	if (elapsed == 0 || elapsed < 1000)
	{
		printk(KERN_ERR "ERROR: LAPIC timer calibration failed (elapsed too small)\n");
		return;
	}

	uint32_t ticks_per_interrupt = (elapsed * 100) / frequency_hz;

	if (ticks_per_interrupt == 0)
	{
		printk(KERN_ERR "ERROR: Frequency too high for LAPIC timer\n");
		return;
	}

	printk(KERN_INFO "Calculated: %u ticks for %u Hz interrupt\n", ticks_per_interrupt, frequency_hz);

	lapic_write(LAPIC_TIMER, LAPIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
	lapic_write(LAPIC_TIMER_DCR, 0x3);
	lapic_write(LAPIC_TIMER_ICR, ticks_per_interrupt);

	uint32_t icr = lapic_read(LAPIC_TIMER_ICR);
	printk(KERN_INFO "LAPIC timer ICR: 0x%08x\n", icr);

	uint32_t lvt = lapic_read(LAPIC_TIMER);
	printk(KERN_INFO "LVT Timer: 0x%08x (Vector=%u, Periodic=%s, Masked=%s)\n",
	       lvt, lvt & 0xFF,
	       (lvt & (1 << 17)) ? "YES" : "NO",
	       (lvt & (1 << 16)) ? "YES" : "NO");

	printk(KERN_OK "LAPIC timer initialized (%u Hz, %u ticks/int)\n",
	       frequency_hz, ticks_per_interrupt);
}

/* ── IPI Functions ───────────────────────────────────────────── */

/**
 * @brief Send a fixed IPI to a destination APIC ID
 *
 * @param dest   Destination APIC ID (xAPIC) or x2APIC ID
 * @param vector Interrupt vector
 */
void lapic_send_ipi(uint32_t dest, uint8_t vector)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk(KERN_ERR "ERROR: LAPIC not initialized\n");
		return;
	}

	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t icr = ((uint64_t)dest << 32) |
			       (uint64_t)vector |
			       (0 << 8) |
			       (1 << 14);
		wrmsr(0x830, icr);
	}
	else
	{
		lapic_write(LAPIC_ICR_HIGH, dest << 24);
		lapic_write(LAPIC_ICR_LOW, vector | (1 << 14));
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;
	}
}

/**
 * @brief Send an INIT IPI to bring an AP into wait-for-SIPI state
 *
 * @param dest_apic_id Target APIC ID
 */
void lapic_send_init_ipi(uint8_t dest_apic_id)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk(KERN_ERR "ERROR: LAPIC not initialized\n");
		return;
	}

	printk(KERN_INFO "Sending INIT IPI to APIC ID %u\n", dest_apic_id);

	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t icr =
		    ((uint64_t)dest_apic_id << 32) |
		    (5 << 8) |
		    (1 << 14) |
		    (1 << 15);

		wrmsr(IA32_X2APIC_ICR, icr);

		hpet_delay_ms(10);

		icr =
		    ((uint64_t)dest_apic_id << 32) |
		    (5 << 8) |
		    (1 << 15);

		wrmsr(IA32_X2APIC_ICR, icr);
	}
	else
	{
		lapic_write(LAPIC_ICR_HIGH, ((uint32_t)dest_apic_id) << 24);

		lapic_write(LAPIC_ICR_LOW, 0x4500);
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;

		lapic_write(LAPIC_ICR_LOW, 0x4500 | (1 << 15));
		while (lapic_read(LAPIC_ICR_LOW) & (1 << 12))
			;
	}

	printk(KERN_INFO "INIT IPI sent\n");
}

/**
 * @brief Send a STARTUP IPI to wake an AP
 *
 * @param dest_apic_id Target APIC ID
 * @param vector       Page number of the trampoline (addr >> 12)
 */
void lapic_send_startup_ipi(uint8_t dest_apic_id, uint8_t vector)
{
	if (!apic_state.lapic_base && apic_mode != APIC_INIT_X2APIC)
	{
		printk(KERN_ERR "ERROR: LAPIC base not initialized\n");
		return;
	}

	printk(KERN_INFO "Sending STARTUP IPI");
	printk(KERN_INFO "Destination APIC ID: %u\n", dest_apic_id);
	printk(KERN_INFO "Vector: 0x%02x\n", vector);
	printk(KERN_INFO "Target physical address: 0x%05x\n", (uint32_t)vector << 12);
	printk(KERN_INFO "APIC Mode: %s\n", apic_mode == APIC_INIT_X2APIC ? "x2APIC" : "xAPIC");

	uint32_t svr = lapic_read(LAPIC_SVR);
	printk(KERN_INFO "LAPIC SVR: 0x%08x %s\n", svr,
	       (svr & 0x100) ? "[ENABLED]" : "[DISABLED!]");

	if (!(svr & 0x100))
	{
		printk(KERN_ERR "ERROR: LAPIC not enabled!\n");
		return;
	}

	uint32_t esr_before = lapic_read(LAPIC_ESR);
	printk(KERN_INFO "ESR before: 0x%08x\n", esr_before);

	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t icr = ((uint64_t)dest_apic_id << 32) |
			       (vector & 0xFF) |
			       (6 << 8) |
			       (0 << 11) |
			       (1 << 14) |
			       (0 << 15);

		printk(KERN_INFO "Writing x2APIC ICR (single 64-bit MSR):\n");
		printk(KERN_INFO "  Destination (bits 63:32): 0x%08x\n", dest_apic_id);
		printk(KERN_INFO "  Vector (bits 0-7):        0x%02x\n", vector & 0xFF);
		printk(KERN_INFO "  Delivery mode (bits 8-10): %u (STARTUP)\n", 6);
		printk(KERN_INFO "  Full ICR value:           0x%016llx\n", icr);

		wrmsr(0x830, icr);

		printk(KERN_OK "x2APIC ICR write completed (self-synchronizing)\n");
	}
	else
	{
		uint32_t icr_high = ((uint32_t)dest_apic_id) << 24;
		printk(KERN_INFO "Writing xAPIC ICR_HIGH: 0x%08x\n", icr_high);
		lapic_write(LAPIC_ICR_HIGH, icr_high);

		uint32_t icr_high_read = lapic_read(LAPIC_ICR_HIGH);
		printk(KERN_INFO "ICR_HIGH readback: 0x%08x %s\n", icr_high_read,
		       (icr_high_read == icr_high) ? "[OK]" : "[MISMATCH!]");

		asm volatile("mfence" ::: "memory");

		uint32_t icr_low = (vector & 0xFF) | (6 << 8);

		printk(KERN_INFO "Writing xAPIC ICR_LOW: 0x%08x\n", icr_low);
		printk(KERN_INFO "  Vector field (bits 0-7):    0x%02x\n", icr_low & 0xFF);
		printk(KERN_INFO "  Delivery mode (bits 8-10):  %u (STARTUP)\n", (icr_low >> 8) & 0x7);
		printk(KERN_INFO "  Level (bit 14):             %u\n", (icr_low >> 14) & 1);
		printk(KERN_INFO "  Trigger (bit 15):           %u\n", (icr_low >> 15) & 1);

		lapic_write(LAPIC_ICR_LOW, icr_low);

		uint32_t icr_low_read = lapic_read(LAPIC_ICR_LOW);
		printk(KERN_INFO "ICR_LOW readback: 0x%08x\n", icr_low_read);
		printk(KERN_INFO "  Delivery Status (bit 12): %s\n",
		       (icr_low_read & (1 << 12)) ? "Send Pending" : "Idle");

		int timeout = 100000;
		while ((lapic_read(LAPIC_ICR_LOW) & (1 << 12)) && timeout > 0)
		{
			cpu_pause();
			timeout--;
		}

		if (timeout == 0)
		{
			printk(KERN_WARNING "WARNING: SIPI delivery timeout!\n");
		}
		else
		{
			printk(KERN_OK "SIPI delivery completed (iterations left: %d)\n", timeout);
		}
	}

	uint32_t esr_after = lapic_read(LAPIC_ESR);
	printk(KERN_INFO "ESR after: 0x%08x\n", esr_after);

	if (esr_after != 0)
	{
		printk(KERN_ERR "ERROR: LAPIC errors detected!\n");
		if (esr_after & 0x01) printk(KERN_ERR "  - Send Checksum Error\n");
		if (esr_after & 0x02) printk(KERN_ERR "  - Receive Checksum Error\n");
		if (esr_after & 0x04) printk(KERN_ERR "  - Send Accept Error\n");
		if (esr_after & 0x08) printk(KERN_ERR "  - Receive Accept Error\n");
		if (esr_after & 0x20) printk(KERN_INFO "  - Send Illegal Vector\n");
		if (esr_after & 0x40) printk(KERN_INFO "  - Receive Illegal Vector\n");
		if (esr_after & 0x80) printk(KERN_INFO "  - Illegal Register Address\n");
	}

	printk(KERN_OK "STARTUP IPI Complete\n");

	printk(KERN_INFO "POST-SIPI VERIFICATION");

	for (volatile int i = 0; i < 100000; i++)
		cpu_pause();

	if (apic_mode == APIC_INIT_X2APIC)
	{
		uint64_t icr = rdmsr(0x830);
		printk(KERN_INFO "ICR after delay: 0x%016llx\n", icr);
		printk(KERN_INFO "  Delivery Status (bit 12): %s\n",
		       (icr & (1ULL << 12)) ? "STILL PENDING (!)" : "Complete");

		if (icr & (1ULL << 12))
		{
			printk(KERN_ERR "ERROR: SIPI delivery still pending - may not have been sent!\n");
		}
	}

	uint32_t esr = lapic_read(LAPIC_ESR);
	if (esr != 0)
	{
		printk(KERN_ERR "ERROR: LAPIC ESR shows errors: 0x%08x\n", esr);
		if (esr & 0x04)
		{
			printk(KERN_ERR "  Send Accept Error - Destination AP didn't accept IPI!\n");
			printk(KERN_INFO "  This means the AP either doesn't exist or isn't ready.\n");
		}
	}

	printk(KERN_INFO "END POST-SIPI VERIFICATION\n");
}

/* ── Debug ───────────────────────────────────────────────────── */

/**
 * @brief Dump LAPIC state for debugging
 */
void apic_debug_check(void)
{
	printk(KERN_INFO "LAPIC Debug Check");
	printk(KERN_INFO "APIC Mode: %s\n", apic_mode == APIC_INIT_X2APIC ? "x2APIC" : "xAPIC");

	if (apic_mode == APIC_INIT_X2APIC)
	{
		printk(KERN_INFO "x2APIC mode - using MSR access\n");
		uint32_t id = (uint32_t)rdmsr(0x802);
		printk(KERN_INFO "LAPIC ID (MSR 0x802): 0x%08x\n", id);

		uint32_t version = (uint32_t)rdmsr(0x803);
		printk(KERN_INFO "LAPIC Version (MSR 0x803): 0x%08x\n", version);

		uint64_t svr = rdmsr(0x80F);
		printk(KERN_INFO "LAPIC SVR (MSR 0x80F): 0x%llx %s\n", svr,
		       (svr & 0x100) ? "[ENABLED]" : "[DISABLED!]");

		printk(KERN_INFO "End LAPIC Debug\n");
		return;
	}

	uint64_t lapic_phys = acpi_get_lapic_address();
	printk(KERN_INFO "LAPIC physical: 0x%lx\n", lapic_phys);
	printk(KERN_INFO "apic_state.lapic_base: %p\n", apic_state.lapic_base);

	if (!apic_state.lapic_base)
	{
		printk(KERN_ERR "ERROR: LAPIC base is NULL!\n");
		return;
	}

	volatile uint32_t *lapic_id_reg = apic_state.lapic_base + (LAPIC_ID / 4);
	printk(KERN_INFO "Reading from: %p\n", lapic_id_reg);

	uint32_t id = *lapic_id_reg;
	printk(KERN_INFO "LAPIC ID register: 0x%08x\n", id);

	if (id == 0xFFFFFFFF || id == 0x00000000)
	{
		printk(KERN_ERR "ERROR: LAPIC not accessible (got 0x%08x)!\n", id);
		printk(KERN_ERR "       Either not mapped or wrong address\n");
	}

	printk(KERN_INFO "\nChecking page table mapping:\n");
	uint64_t virt = (uint64_t)apic_state.lapic_base;
	printk(KERN_INFO "Virtual address: 0x%lx\n", virt);

	volatile uint32_t *direct = (volatile uint32_t *)(0xFFFF800000000000ULL + (uint64_t)lapic_phys);
	printk(KERN_INFO "Direct mapping attempt: %p\n", direct);
	uint32_t direct_read = *direct;
	printk(KERN_INFO "Direct read: 0x%08x\n", direct_read);

	printk(KERN_INFO "End LAPIC Debug\n");
}

/* ── I/O APIC Register Access (inline) ───────────────────────── */

/**
 * @brief Read an I/O APIC register via the indirect index/data pair
 *
 * @param reg 8-bit register index
 * @return 32-bit register value
 */
static inline uint32_t ioapic_read(uint8_t reg)
{
	apic_state.ioapic_base[0] = reg;
	return apic_state.ioapic_base[4];
}

/**
 * @brief Write an I/O APIC register
 *
 * @param reg   8-bit register index
 * @param value 32-bit value
 */
static inline void ioapic_write(uint8_t reg, uint32_t value)
{
	apic_state.ioapic_base[0] = reg;
	apic_state.ioapic_base[4] = value;
}

/* ── I/O APIC Public API ─────────────────────────────────────── */

/**
 * @brief Program an I/O APIC redirection entry
 *
 * @param irq          IRQ line index
 * @param vector       Interrupt vector
 * @param dest_apic_id Target CPU APIC ID
 * @param masked       true to initially mask the entry
 */
void ioapic_set_redirect(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool masked)
{
	if (!apic_state.ioapic_base)
		return;

	uint32_t low = vector;
	uint32_t high = ((uint32_t)dest_apic_id) << 24;

	if (masked)
		low |= (1 << 16);

	uint8_t reg_low = IOAPIC_REDTBL_BASE + (irq * 2);
	uint8_t reg_high = IOAPIC_REDTBL_BASE + (irq * 2) + 1;

	ioapic_write(reg_high, high);
	ioapic_write(reg_low, low);
}

/**
 * @brief Mask (disable) an IRQ on the I/O APIC
 *
 * @param irq IRQ line index
 */
void ioapic_mask_irq(uint8_t irq)
{
	if (!apic_state.ioapic_base)
		return;

	uint8_t reg = IOAPIC_REDTBL_BASE + (irq * 2);
	uint32_t val = ioapic_read(reg);
	ioapic_write(reg, val | (1 << 16));
}

/**
 * @brief Unmask (enable) an IRQ on the I/O APIC
 *
 * @param irq IRQ line index
 */
void ioapic_unmask_irq(uint8_t irq)
{
	if (!apic_state.ioapic_base)
		return;

	uint8_t reg = IOAPIC_REDTBL_BASE + (irq * 2);
	uint32_t val = ioapic_read(reg);
	ioapic_write(reg, val & ~(1 << 16));
}

/* ── ISO Setup (static) ──────────────────────────────────────── */

/**
 * @brief Callback for acpi_enum_isos — program an override entry
 */
static void setup_iso_callback(uint8_t irq_source, uint32_t gsi, uint16_t flags, void *ctx)
{
	(void)ctx;
	uint8_t vector = 32 + irq_source;

	bool active_low = flags & 0x2;
	bool level_triggered = flags & 0x8;

	uint32_t low = vector;
	uint32_t high = ((uint32_t)apic_state.bsp_id) << 24;

	if (active_low)
		low |= (1 << 13);
	if (level_triggered)
		low |= (1 << 15);

	low |= (1 << 16);

	uint8_t reg_low = IOAPIC_REDTBL_BASE + (gsi * 2);
	uint8_t reg_high = IOAPIC_REDTBL_BASE + (gsi * 2) + 1;

	ioapic_write(reg_high, high);
	ioapic_write(reg_low, low);
}

/* ── LAPIC Initialisation Modes ──────────────────────────────── */

/**
 * @brief Enable x2APIC mode on the current CPU
 *
 * @return 0 on success
 */
int lapic_init_x2apic(void)
{
	uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);

	apic_base |= (1ULL << 11);
	apic_base |= (1ULL << 10);

	wrmsr(IA32_APIC_BASE_MSR, apic_base);

	apic_mode = APIC_INIT_X2APIC;
	apic_state.lapic_base = NULL;

	wrmsr(0x80F, 0x100 | 0xFF);

	wrmsr(0x828, 0);
	wrmsr(0x828, 0);

	return 0;
}

/**
 * @brief Enable xAPIC (MMIO) mode on the current CPU
 *
 * @return 0 on success, -1 on failure
 */
int lapic_init_xapic(void)
{
	printk(KERN_OK "xAPIC mode enabled\n");

	uint64_t lapic_phys = acpi_get_lapic_address();

	if (map_apic_mmio_page(lapic_phys) < 0)
		return -1;

	apic_state.lapic_base = (volatile uint32_t *)phys_to_virt(lapic_phys);
	apic_mode = APIC_INIT_XAPIC;

	lapic_write(LAPIC_SVR, 0x100 | 0xFF);
	lapic_write(LAPIC_ESR, 0);
	lapic_write(LAPIC_ESR, 0);

	return 0;
}

/* ── APIC Initialisation ─────────────────────────────────────── */

/**
 * @brief Callback to locate the first I/O APIC
 */
static void find_ioapic(uint8_t id, uint32_t addr, uint32_t gsi, void *ctx)
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
		printk(KERN_INFO "I/O APIC found: ID=%u addr=0x%x GSI_base=%u\n", id, addr, gsi);
	}
}

/**
 * @brief Full APIC initialisation for the BSP
 *
 * Detects x2APIC vs xAPIC, sets up the Local APIC, locates
 * the I/O APIC, programs default redirections, and applies
 * interrupt-source overrides from the MADT.
 *
 * @return 0 on success, -1 on failure
 */
int apic_init(void)
{
	if (!acpi_is_initialized())
	{
		printk(KERN_ERR "ACPI not initialized\n");
		return -1;
	}

	uint64_t lapic_phys = acpi_get_lapic_address();
	if (!lapic_phys)
	{
		printk(KERN_ERR "Local APIC address not found\n");
		return -1;
	}

	printk(KERN_INFO "Local APIC physical address: 0x%lx\n", lapic_phys);

	if (cpu_has_x2apic())
	{
		printk(KERN_INFO "x2APIC mode detected\n");
		lapic_init_x2apic();
	}
	else
	{
		lapic_init_xapic();
	}

	uint64_t apic_base = rdmsr(0x1B);
	printk(KERN_INFO "APIC=%d x2APIC=%d\n",
	       !!(apic_base & (1ULL << 11)),
	       !!(apic_base & (1ULL << 10)));

	apic_state.bsp_id = lapic_get_id();
	printk(KERN_INFO "BSP APIC ID: %u\n", apic_state.bsp_id);

	lapic_enable();

	struct
	{
		bool found;
		uint32_t address;
		uint32_t gsi_base;
	} ioapic_ctx = {0};

	acpi_enum_ioapics(find_ioapic, &ioapic_ctx);

	if (!ioapic_ctx.found)
	{
		printk(KERN_INFO "No I/O APIC found\n");
		return -1;
	}

	uint64_t ioapic_phys = (uint64_t)ioapic_ctx.address;

	if (map_apic_mmio_page(ioapic_phys) < 0)
		return -1;

	apic_state.ioapic_base = (volatile uint32_t *)phys_to_virt(ioapic_phys);
	apic_state.ioapic_gsi_base = ioapic_ctx.gsi_base;

	printk(KERN_INFO "I/O APIC: phys=0x%lx virt=%p\n", ioapic_phys, apic_state.ioapic_base);

	uint32_t ver = ioapic_read(IOAPIC_REG_VER);
	if (ver == 0 || ver == 0xFFFFFFFF)
	{
		printk(KERN_ERR "ERROR: I/O APIC not accessible!\n");
		return -1;
	}

	apic_state.ioapic_max_redirect = ((ver >> 16) & 0xFF) + 1;
	printk(KERN_INFO "I/O APIC version: 0x%x, max redirects: %u\n",
	       ver & 0xFF, apic_state.ioapic_max_redirect);

	for (uint32_t i = 0; i < apic_state.ioapic_max_redirect; i++)
		ioapic_set_redirect(i, 32 + i, apic_state.bsp_id, true);

	acpi_enum_isos(setup_iso_callback, NULL);

	apic_state.initialized = true;
	printk(KERN_OK "APIC initialized successfully\n");
	return 0;
}

/**
 * @brief Initialise the Local APIC on an Application Processor
 *
 * APs must not touch the I/O APIC or ACPI — they only enable
 * their own Local APIC.
 */
void apic_init_ap(void)
{
	if (cpu_has_x2apic())
	{
		lapic_init_x2apic();
	}
	else
	{
		lapic_init_xapic();
	}

	lapic_enable();
}

/**
 * @brief Check whether the APIC subsystem has been initialised
 *
 * @return true if initialised
 */
bool apic_is_initialized(void)
{
	return apic_state.initialized;
}

/* ── SMP Support ─────────────────────────────────────────────── */

/**
 * @brief Start an Application Processor
 *
 * Follows the Intel MP initialisation sequence:
 *  - INIT IPI
 *  - 10 ms wait
 *  - First STARTUP IPI
 *  - 200 us wait
 *  - Second STARTUP IPI
 *
 * @param apic_id        Target APIC ID
 * @param trampoline_addr Physical address of the 16-bit boot code
 */
void apic_start_ap(uint8_t apic_id, uint32_t trampoline_addr)
{
	if (!apic_state.initialized)
		return;

	printk(KERN_INFO "Starting AP with APIC ID %u", apic_id);
	printk(KERN_INFO "Trampoline at physical 0x%x\n", trampoline_addr);

	uint8_t vector = (trampoline_addr >> 12) & 0xFF;
	printk(KERN_INFO "SIPI vector: 0x%02x (starts at 0x%05x)\n", vector, vector << 12);

	printk(KERN_INFO "\nStep 1: Sending INIT IPI...\n");
	lapic_send_init_ipi(apic_id);

	printk(KERN_INFO "Step 2: Waiting 10ms...\n");
	for (volatile int i = 0; i < 10000000; i++)
		cpu_pause();

	printk(KERN_INFO "Step 3: Sending first SIPI...\n");
	lapic_send_startup_ipi(apic_id, vector);

	printk(KERN_INFO "Step 4: Waiting 200us...\n");
	for (volatile int i = 0; i < 200000; i++)
		cpu_pause();

	printk(KERN_INFO "Step 5: Sending second SIPI...\n");
	lapic_send_startup_ipi(apic_id, vector);

	printk(KERN_OK "AP startup sequence complete\n");
}

/**
 * @brief Convenience: full BSP init and timer start
 *
 * Calls apic_init(), enables the LAPIC, unmasks keyboard-
 * related IRQs, starts the LAPIC timer at 100 Hz, and
 * installs the timer handler.
 */
void apic_init_bsp(void)
{
	apic_init();
	lapic_enable();
	ioapic_unmask_irq(1);
	ioapic_unmask_irq(2);
	ioapic_unmask_irq(12);
	lapic_timer_init(100);
	irq_install_handler(0, lapic_timer_handler);
}
