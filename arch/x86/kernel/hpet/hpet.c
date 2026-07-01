/**
 * @file hpet.c
 * @brief HPET (High Precision Event Timer) driver
 *
 * Implements initialisation from the ACPI HPET table, MMIO
 * mapping, counter read, time conversion, busy-wait delays,
 * and one-shot / periodic timer configuration.
 */

#include "hpet.h"
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <asm.h>
#include <hubble/printk.h>
#include <mm/vmm.h>

#include "higher_half.h"

#include <stddef.h>

/* -- HPET Register Offsets ------------------------------------- */

#define HPET_GENERAL_CAPS 0x000
#define HPET_GENERAL_CONFIG 0x010
#define HPET_GENERAL_INT_STATUS 0x020
#define HPET_MAIN_COUNTER 0x0F0
#define HPET_TIMER_CONFIG(n) (0x100 + (n) * 0x20)
#define HPET_TIMER_COMPARATOR(n) (0x108 + (n) * 0x20)

/* -- Configuration Bits ---------------------------------------- */

#define HPET_ENABLE_CNF (1 << 0)
#define HPET_LEG_RT_CNF (1 << 1)

/* -- Timer Configuration Bits ---------------------------------- */

#define HPET_Tn_INT_TYPE_CNF (1 << 1)
#define HPET_Tn_INT_ENB_CNF (1 << 2)
#define HPET_Tn_TYPE_CNF (1 << 3)
#define HPET_Tn_PER_INT_CAP (1 << 4)
#define HPET_Tn_SIZE_CAP (1 << 5)
#define HPET_Tn_VAL_SET_CNF (1 << 6)
#define HPET_Tn_32MODE_CNF (1 << 8)
#define HPET_Tn_FSB_INT_DEL_CAP (1 << 15)

/* -- Global HPET State ----------------------------------------- */

static struct {
  volatile uint64_t *base;
  uint64_t frequency;
  uint64_t period_fs;
  uint8_t num_timers;
  bool initialized;
} hpet_state = {0};

/* -- MMIO Helpers (static) ------------------------------------- */

/**
 * @brief Check whether a virtual address is already page-mapped
 *
 * @param virt Virtual address to check
 * @return true if mapped, false otherwise
 */
static bool hpet_mmio_is_mapped(uint64_t virt) {
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
 * @brief Map one 4 KiB page of HPET MMIO space uncacheable
 *
 * @param phys Physical address of the HPET base
 * @return 0 on success, -1 on failure
 */
static int hpet_map_mmio_page(uint64_t phys) {
  uint64_t page = phys & ~0xFFFULL;
  uint64_t virt = phys_to_virt(page);

  if (hpet_mmio_is_mapped(virt))
    return 0;

  if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0) {
    printk(KERN_ERR "ERROR: failed to map HPET MMIO phys=0x%lx virt=%p\n", page,
           (void *)virt);
    return -1;
  }

  return 0;
}

/**
 * @brief Read a 64-bit HPET MMIO register
 *
 * @param offset Byte offset from the HPET base
 * @return Register value
 */
static inline uint64_t hpet_read(uint32_t offset) {
  return hpet_state.base[offset / 8];
}

/**
 * @brief Write a 64-bit HPET MMIO register
 *
 * @param offset Byte offset from the HPET base
 * @param value  Value to write
 */
static inline void hpet_write(uint32_t offset, uint64_t value) {
  hpet_state.base[offset / 8] = value;
}

/* -- Counter Access -------------------------------------------- */

/**
 * @brief Read the HPET main counter
 *
 * @return Current counter value in ticks, or 0 if not initialised
 */
uint64_t hpet_get_counter(void) {
  if (!hpet_state.initialized)
    return 0;
  return hpet_read(HPET_MAIN_COUNTER);
}

/**
 * @brief Get the current time in nanoseconds since HPET init
 *
 * @return Nanoseconds
 */
uint64_t hpet_get_time_ns(void) { return hpet_ticks_to_ns(hpet_get_counter()); }

/* -- Time Conversion ------------------------------------------- */

/**
 * @brief Convert HPET ticks to nanoseconds
 *
 * @param ticks Counter ticks
 * @return Equivalent nanoseconds
 */
uint64_t hpet_ticks_to_ns(uint64_t ticks) {
  return (ticks * hpet_state.period_fs) / 1000000;
}

/**
 * @brief Convert nanoseconds to HPET ticks
 *
 * @param ns Nanoseconds
 * @return Equivalent counter ticks
 */
uint64_t hpet_ns_to_ticks(uint64_t ns) {
  return (ns * 1000000) / hpet_state.period_fs;
}

/* -- Delays (Busy-Wait) ---------------------------------------- */

/**
 * @brief Busy-wait for a given number of nanoseconds
 *
 * @param ns Delay duration in nanoseconds
 */
void hpet_delay_ns(uint64_t ns) {
  if (!hpet_state.initialized)
    return;

  uint64_t start = hpet_get_counter();
  uint64_t ticks = hpet_ns_to_ticks(ns);

  while ((hpet_get_counter() - start) < ticks)
    cpu_pause();
}

/**
 * @brief Busy-wait for a given number of microseconds
 *
 * @param us Delay duration in microseconds
 */
void hpet_delay_us(uint64_t us) { hpet_delay_ns(us * 1000); }

/**
 * @brief Busy-wait for a given number of milliseconds
 *
 * @param ms Delay duration in milliseconds
 */
void hpet_delay_ms(uint64_t ms) { hpet_delay_ns(ms * 1000000); }

/* -- Timer Setup ----------------------------------------------- */

/**
 * @brief Configure an HPET timer in one-shot mode
 *
 * @param timer_num Timer index (0 .. num_timers - 1)
 * @param ns        Delay in nanoseconds
 * @param vector    Interrupt vector (32-255)
 * @return 0 on success, -1 on failure
 */
int hpet_timer_oneshot(uint8_t timer_num, uint64_t ns, uint8_t vector) {
  if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
    return -1;

  uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
  config &= ~HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  uint64_t ticks = hpet_ns_to_ticks(ns);
  uint64_t target = hpet_get_counter() + ticks;
  hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

  config = (uint64_t)vector << 9;
  config |= HPET_Tn_INT_ENB_CNF;

  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  printk(KERN_INFO "HPET timer %u: one-shot in %lu ns (vector %u)\n", timer_num,
         ns, vector);
  return 0;
}

/**
 * @brief Configure an HPET timer in periodic mode
 *
 * @param timer_num  Timer index (0 .. num_timers - 1)
 * @param period_ns  Period in nanoseconds
 * @param vector     Interrupt vector (32-255)
 * @return 0 on success, -1 on failure
 */
int hpet_timer_periodic(uint8_t timer_num, uint64_t period_ns, uint8_t vector) {
  if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
    return -1;

  uint64_t caps = hpet_read(HPET_TIMER_CONFIG(timer_num));
  if (!(caps & HPET_Tn_PER_INT_CAP)) {
    printk(KERN_ERR "HPET timer %u doesn't support periodic mode\n", timer_num);
    return -1;
  }

  uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
  config &= ~HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  uint64_t period_ticks = hpet_ns_to_ticks(period_ns);

  uint64_t target = hpet_get_counter() + period_ticks;
  hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

  config = (uint64_t)vector << 9;
  config |= HPET_Tn_TYPE_CNF;
  config |= HPET_Tn_VAL_SET_CNF;
  config |= HPET_Tn_INT_ENB_CNF;

  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  hpet_write(HPET_TIMER_COMPARATOR(timer_num), period_ticks);

  printk(KERN_INFO "HPET timer %u: periodic every %lu ns (vector %u)\n",
         timer_num, period_ns, vector);
  return 0;
}

/**
 * @brief Stop an HPET timer
 *
 * @param timer_num Timer index to stop
 */
void hpet_timer_stop(uint8_t timer_num) {
  if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
    return;

  uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
  config &= ~HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);
}

/* -- Initialisation -------------------------------------------- */

/**
 * @brief Initialise the HPET from ACPI-provided information
 *
 * Maps the HPET MMIO region, reads capabilities, resets the
 * counter, disables all timers, and enables the HPET.
 *
 * @return 0 on success, -1 on failure
 */
int hpet_init(void) {
  if (!acpi_is_initialized()) {
    printk(KERN_ERR "ACPI not initialized\n");
    return -1;
  }

  uint64_t hpet_phys = acpi_get_hpet_address();
  if (!hpet_phys) {
    printk(KERN_ERR "HPET not found in ACPI\n");
    return -1;
  }

  if (hpet_map_mmio_page(hpet_phys) < 0)
    return -1;

  hpet_state.base = (volatile uint64_t *)phys_to_virt(hpet_phys);

  printk(KERN_INFO "HPET at phys=0x%lx virt=%p\n", hpet_phys, hpet_state.base);

  uint64_t caps = hpet_read(HPET_GENERAL_CAPS);
  hpet_state.period_fs = caps >> 32;
  hpet_state.frequency = 1000000000000000ULL / hpet_state.period_fs;
  hpet_state.num_timers = ((caps >> 8) & 0x1F) + 1;
  bool is_64bit = caps & (1 << 13);

  printk(KERN_INFO "HPET: period=%lu fs, freq=%lu MHz, timers=%u, %s-bit\n",
         hpet_state.period_fs, hpet_state.frequency / 1000000,
         hpet_state.num_timers, is_64bit ? "64" : "32");

  uint64_t config = hpet_read(HPET_GENERAL_CONFIG);
  config &= ~HPET_ENABLE_CNF;
  hpet_write(HPET_GENERAL_CONFIG, config);

  hpet_write(HPET_MAIN_COUNTER, 0);

  for (uint8_t i = 0; i < hpet_state.num_timers; i++) {
    uint64_t tc = hpet_read(HPET_TIMER_CONFIG(i));
    tc &= ~HPET_Tn_INT_ENB_CNF;
    hpet_write(HPET_TIMER_CONFIG(i), tc);
  }

  config = hpet_read(HPET_GENERAL_CONFIG);
  config |= HPET_ENABLE_CNF;
  hpet_write(HPET_GENERAL_CONFIG, config);

  hpet_state.initialized = true;
  printk(KERN_OK "HPET initialized\n");
  return 0;
}

/* -- Public Helpers -------------------------------------------- */

/**
 * @brief Check whether HPET has been initialised
 *
 * @return true if initialised, false otherwise
 */
bool hpet_is_initialized(void) { return hpet_state.initialized; }

/**
 * @brief Get the HPET clock frequency in Hz
 *
 * @return Frequency in Hz
 */
uint64_t hpet_get_frequency(void) { return hpet_state.frequency; }

/**
 * @brief Calibrate an external timer against the HPET
 *
 * Reads the target counter before and after an HPET-based delay
 * and returns the difference.
 *
 * @param counter_fn Pointer to the external timer counter (volatile)
 * @param ms         Calibration period in milliseconds
 * @return Ticks of the external timer during the period, or 0 on failure
 */
uint64_t hpet_calibrate_timer(volatile uint64_t *counter_fn, uint32_t ms) {
  if (!hpet_state.initialized)
    return 0;

  uint64_t start_hpet = hpet_get_counter();
  uint64_t start_counter = *counter_fn;

  hpet_delay_ms(ms);

  uint64_t end_hpet = hpet_get_counter();
  uint64_t end_counter = *counter_fn;

  uint64_t hpet_ticks = end_hpet - start_hpet;
  if (hpet_ticks == 0)
    return 0;

  return end_counter - start_counter;
}
