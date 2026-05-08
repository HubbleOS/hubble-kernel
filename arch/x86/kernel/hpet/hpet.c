#include "hpet.h"
#include <acpi/acpi.h>
#include "higher_half.h"
#include <apic/apic.h>
#include <hubble/printk.h>
#include <stddef.h>

#include <mm/vmm.h>

#include <asm.h>

// HPET Register Offsets
#define HPET_GENERAL_CAPS 0x000
#define HPET_GENERAL_CONFIG 0x010
#define HPET_GENERAL_INT_STATUS 0x020
#define HPET_MAIN_COUNTER 0x0F0
#define HPET_TIMER_CONFIG(n) (0x100 + (n) * 0x20)
#define HPET_TIMER_COMPARATOR(n) (0x108 + (n) * 0x20)

// Configuration bits
#define HPET_ENABLE_CNF (1 << 0)
#define HPET_LEG_RT_CNF (1 << 1) // Legacy Replacement Route

// Timer configuration bits
#define HPET_Tn_INT_TYPE_CNF (1 << 1)	  // 1=level, 0=edge
#define HPET_Tn_INT_ENB_CNF (1 << 2)	  // Enable interrupt
#define HPET_Tn_TYPE_CNF (1 << 3)	  // 1=periodic, 0=one-shot
#define HPET_Tn_PER_INT_CAP (1 << 4)	  // Periodic capable (RO)
#define HPET_Tn_SIZE_CAP (1 << 5)	  // 64-bit capable (RO)
#define HPET_Tn_VAL_SET_CNF (1 << 6)	  // Set accumulator
#define HPET_Tn_32MODE_CNF (1 << 8)	  // Force 32-bit mode
#define HPET_Tn_FSB_INT_DEL_CAP (1 << 15) // FSB interrupt capable (RO)

// Global HPET state
static struct
{
	volatile uint64_t *base;
	uint64_t frequency; // Hz
	uint64_t period_fs; // femtoseconds per tick
	uint8_t num_timers;
	bool initialized;
} hpet_state = {0};

static bool hpet_mmio_is_mapped(uint64_t virt)
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

static int hpet_map_mmio_page(uint64_t phys)
{
	uint64_t page = phys & ~0xFFFULL;
	uint64_t virt = phys_to_virt(page);

	if (hpet_mmio_is_mapped(virt))
		return 0;

	if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0)
	{
		printk("ERROR: failed to map HPET MMIO phys=0x%lx virt=%p\n",
		       page, (void *)virt);
		return -1;
	}

	return 0;
}

// Read/Write helpers
static inline uint64_t hpet_read(uint32_t offset)
{
	// printk("hpet_read(%d) = 0x%lx\n", offset, hpet_state.base[offset / 8]);
	return hpet_state.base[offset / 8];
}

static inline void hpet_write(uint32_t offset, uint64_t value)
{
	hpet_state.base[offset / 8] = value;
}

// Get main counter value
uint64_t hpet_get_counter(void)
{
	if (!hpet_state.initialized)
		return 0;
	return hpet_read(HPET_MAIN_COUNTER);
}

// Convert counter ticks to nanoseconds
uint64_t hpet_ticks_to_ns(uint64_t ticks)
{
	// period_fs is in femtoseconds (10^-15 seconds)
	// Convert to nanoseconds (10^-9 seconds)
	// ns = ticks * period_fs / 1000000
	return (ticks * hpet_state.period_fs) / 1000000;
}

// Convert nanoseconds to counter ticks
uint64_t hpet_ns_to_ticks(uint64_t ns)
{
	// ticks = ns * 1000000 / period_fs
	return (ns * 1000000) / hpet_state.period_fs;
}

// Get current time in nanoseconds
uint64_t hpet_get_time_ns(void)
{
	return hpet_ticks_to_ns(hpet_get_counter());
}

// Busy-wait delay in nanoseconds
void hpet_delay_ns(uint64_t ns)
{
	if (!hpet_state.initialized)
		return;

	uint64_t start = hpet_get_counter();
	uint64_t ticks = hpet_ns_to_ticks(ns);

	while ((hpet_get_counter() - start) < ticks)
		cpu_pause();
}

// Busy-wait delay in microseconds
void hpet_delay_us(uint64_t us)
{
	hpet_delay_ns(us * 1000);
}

// Busy-wait delay in milliseconds
void hpet_delay_ms(uint64_t ms)
{
	hpet_delay_ns(ms * 1000000);
}

// Setup timer for one-shot interrupt
int hpet_timer_oneshot(uint8_t timer_num, uint64_t ns, uint8_t vector)
{
	if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
		return -1;

	// Disable timer first
	uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
	config &= ~HPET_Tn_INT_ENB_CNF;
	hpet_write(HPET_TIMER_CONFIG(timer_num), config);

	// Set comparator value (current counter + desired ticks)
	uint64_t ticks = hpet_ns_to_ticks(ns);
	uint64_t target = hpet_get_counter() + ticks;
	hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

	// Configure: edge-triggered, one-shot, interrupts enabled
	config = (uint64_t)vector << 9; // Set interrupt vector
	config |= HPET_Tn_INT_ENB_CNF;	// Enable interrupt
	// One-shot is default (HPET_Tn_TYPE_CNF = 0)

	hpet_write(HPET_TIMER_CONFIG(timer_num), config);

	printk("HPET timer %u: one-shot in %lu ns (vector %u)\n",
	       timer_num, ns, vector);
	return 0;
}

// Setup timer for periodic interrupts
int hpet_timer_periodic(uint8_t timer_num, uint64_t period_ns, uint8_t vector)
{
	if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
		return -1;

	// Check if timer supports periodic mode
	uint64_t caps = hpet_read(HPET_TIMER_CONFIG(timer_num));
	if (!(caps & HPET_Tn_PER_INT_CAP))
	{
		printk("HPET timer %u doesn't support periodic mode\n", timer_num);
		return -1;
	}

	// Disable timer first
	uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
	config &= ~HPET_Tn_INT_ENB_CNF;
	hpet_write(HPET_TIMER_CONFIG(timer_num), config);

	// Calculate period in ticks
	uint64_t period_ticks = hpet_ns_to_ticks(period_ns);

	// Set comparator to (current counter + period)
	uint64_t target = hpet_get_counter() + period_ticks;
	hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

	// Configure: periodic mode
	config = (uint64_t)vector << 9; // Set interrupt vector
	config |= HPET_Tn_TYPE_CNF;	// Periodic mode
	config |= HPET_Tn_VAL_SET_CNF;	// Set accumulator
	config |= HPET_Tn_INT_ENB_CNF;	// Enable interrupt

	hpet_write(HPET_TIMER_CONFIG(timer_num), config);

	// Write period to comparator again (required for periodic)
	hpet_write(HPET_TIMER_COMPARATOR(timer_num), period_ticks);

	printk("HPET timer %u: periodic every %lu ns (vector %u)\n",
	       timer_num, period_ns, vector);
	return 0;
}

// Stop a timer
void hpet_timer_stop(uint8_t timer_num)
{
	if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
		return;

	uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
	config &= ~HPET_Tn_INT_ENB_CNF;
	hpet_write(HPET_TIMER_CONFIG(timer_num), config);
}

// Initialize HPET
int hpet_init(void)
{
	if (!acpi_is_initialized())
	{
		printk("ACPI not initialized\n");
		return -1;
	}

	uint64_t hpet_phys = acpi_get_hpet_address();
	if (!hpet_phys)
	{
		printk("HPET not found in ACPI\n");
		return -1;
	}

	if (hpet_map_mmio_page(hpet_phys) < 0)
		return -1;

	hpet_state.base = (volatile uint64_t *)phys_to_virt(hpet_phys);

	printk("HPET at phys=0x%lx virt=%p\n", hpet_phys, hpet_state.base);

	uint64_t caps = hpet_read(HPET_GENERAL_CAPS);
	hpet_state.period_fs = caps >> 32;
	hpet_state.frequency = 1000000000000000ULL / hpet_state.period_fs;
	hpet_state.num_timers = ((caps >> 8) & 0x1F) + 1;
	bool is_64bit = caps & (1 << 13);

	printk("HPET: period=%lu fs, freq=%lu MHz, timers=%u, %s-bit\n",
	       hpet_state.period_fs,
	       hpet_state.frequency / 1000000,
	       hpet_state.num_timers,
	       is_64bit ? "64" : "32");

	uint64_t config = hpet_read(HPET_GENERAL_CONFIG);
	config &= ~HPET_ENABLE_CNF;
	hpet_write(HPET_GENERAL_CONFIG, config);

	hpet_write(HPET_MAIN_COUNTER, 0);

	for (uint8_t i = 0; i < hpet_state.num_timers; i++)
	{
		uint64_t tc = hpet_read(HPET_TIMER_CONFIG(i));
		tc &= ~HPET_Tn_INT_ENB_CNF;
		hpet_write(HPET_TIMER_CONFIG(i), tc);
	}

	config = hpet_read(HPET_GENERAL_CONFIG);
	config |= HPET_ENABLE_CNF;
	hpet_write(HPET_GENERAL_CONFIG, config);

	hpet_state.initialized = true;
	printk("HPET initialized\n");
	return 0;
}

bool hpet_is_initialized(void)
{
	return hpet_state.initialized;
}

// Get HPET frequency
uint64_t hpet_get_frequency(void)
{
	return hpet_state.frequency;
}

// Calibrate another timer using HPET (e.g., TSC, LAPIC timer)
// Returns ticks of the target timer that correspond to 'ms' milliseconds
uint64_t hpet_calibrate_timer(volatile uint64_t *counter_fn, uint32_t ms)
{
	if (!hpet_state.initialized)
		return 0;

	uint64_t start_hpet = hpet_get_counter();
	uint64_t start_counter = *counter_fn;

	// Wait for specified time
	hpet_delay_ms(ms);

	uint64_t end_hpet = hpet_get_counter();
	uint64_t end_counter = *counter_fn;

	// Verify HPET actually advanced
	uint64_t hpet_ticks = end_hpet - start_hpet;
	if (hpet_ticks == 0)
		return 0;

	return end_counter - start_counter;
}
