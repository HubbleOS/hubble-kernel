/**
 * @file hpet.c
 * @brief HPET (High Precision Event Timer) driver
 *
 * Implements initialisation from the ACPI HPET table, uncached
 * MMIO access, overflow-safe time conversion, busy-wait delays,
 * and one-shot / periodic timer configuration.
 *
 * All HPET registers are 128 bits wide per the spec.  Only the
 * lower 64 bits contain data; the upper 64 bits are reserved.
 * The driver reads/writes each register as a 64-bit value at
 * the correct byte offset through a uint8_t* base pointer.
 *
 * Key fixes over the original implementation:
 *   - MMIO pointer is uint8_t* with byte offsets (was uint64_t*
 *     with offset/8, which assumed wrong register alignment).
 *   - Timer interrupt vector is placed in the route field
 *     (bits 32-38), not in the config register (was << 9).
 *   - Arithmetic overflow protection using __uint128_t.
 *   - Delay functions compute ticks directly from their unit
 *     to avoid overflow in intermediate multiplications.
 *   - Periodic timer initialization follows the HPET spec
 *     sequence (VAL_SET_CNF + comparator writes in correct
 *     order).
 *   - Dead calibration API removed.
 */

#include "hpet.h"
#include <acpi/acpi.h>
#include <asm.h>
#include <hubble/printk.h>
#include <mm/vmm.h>

#include "higher_half.h"

#include <stddef.h>

/* -- HPET Register Offsets (128-bit aligned) ------------------- */

/*
 * Per the HPET spec, all registers are 128-bit aligned.
 * Only the lower 64 bits are used; the upper 64 bits are
 * reserved and read as zero.  The offsets below are byte
 * offsets from the MMIO base.
 */
#define HPET_GENERAL_CAPS       0x000
#define HPET_GENERAL_CONFIG     0x010
#define HPET_GENERAL_INT_STATUS 0x020
#define HPET_MAIN_COUNTER       0x0F0

/* Timer n registers: 128-bit aligned, 0x20 (32) bytes apart */
#define HPET_TIMER_CONFIG(n)    (0x100 + (n) * 0x20)
#define HPET_TIMER_COMPARATOR(n) (0x108 + (n) * 0x20)

/* -- General Configuration Bits -------------------------------- */

#define HPET_ENABLE_CNF  (1ULL << 0)
#define HPET_LEG_RT_CNF  (1ULL << 1)

/* -- General Capabilities Bits --------------------------------- */

#define HPET_COUNTER_SIZE_CAP (1ULL << 13)
#define HPET_LEG_RT_CAP       (1ULL << 15)

/* -- Timer Config Register: Lower 64 bits (R/W) ---------------- */

#define HPET_Tn_INT_TYPE_CNF   (1ULL << 1)  /* 0=edge, 1=level  */
#define HPET_Tn_INT_ENB_CNF    (1ULL << 2)  /* interrupt enable  */
#define HPET_Tn_TYPE_CNF       (1ULL << 3)  /* 0=one-shot, 1=periodic */
#define HPET_Tn_PER_INT_CAP    (1ULL << 4)  /* periodic capable (RO) */
#define HPET_Tn_SIZE_CAP       (1ULL << 5)  /* 64-bit counter (RO) */
#define HPET_Tn_VAL_SET_CNF    (1ULL << 6)  /* value set enable  */
#define HPET_Tn_32MODE_CNF     (1ULL << 8)  /* 32-bit mode       */
#define HPET_Tn_FSB_DEL_CAP    (1ULL << 15) /* FSB delivery cap (RO) */

/* -- Timer Config Register: Upper 64 bits (R/W) --------------- */

/*
 * Bits 63:32 of the timer config register are the interrupt
 * route field.  Each bit corresponds to an APIC ID; setting
 * bit N routes the interrupt to APIC ID N.  In practice this
 * is often configured via the I/O APIC redirection table
 * instead, but we write the vector here for correctness.
 */
#define HPET_Tn_ROUTE_SHIFT 32

/* -- Minimum safe one-shot delay (ticks) ----------------------- */

/*
 * The HPET spec requires comparator values to be at least
 * MIN_TICKS (typically 3-5) ticks in the future.  We use a
 * conservative value to cover all implementations.
 */
#define HPET_MIN_TICKS 5

/* -- Global HPET State ----------------------------------------- */

static struct {
  volatile uint8_t *base; /* MMIO base (page-aligned, byte pointer) */
  uint64_t period_fs;     /* period in femtoseconds per tick        */
  uint64_t frequency;     /* computed frequency in Hz               */
  uint8_t num_timers;     /* number of timers (from capabilities)   */
  bool is_64bit;          /* true if main counter is 64-bit        */
  bool initialized;
} hpet_state = {0};

/* -- MMIO Helpers (static, inline) ----------------------------- */

/**
 * @brief Read a 64-bit HPET MMIO register
 *
 * All HPET registers are 128-bit aligned.  Only the lower
 * 64 bits are meaningful.  Access through a uint8_t* base
 * with byte offsets ensures correct address calculation.
 *
 * @param offset Byte offset from the HPET MMIO base
 * @return Lower 64 bits of the register
 */
static inline uint64_t hpet_read(uint32_t offset) {
  return *(volatile uint64_t *)(hpet_state.base + offset);
}

/**
 * @brief Write a 64-bit value to the lower half of an HPET register
 *
 * @param offset Byte offset from the HPET MMIO base
 * @param value  64-bit value to write (upper 64 bits unchanged)
 */
static inline void hpet_write(uint32_t offset, uint64_t value) {
  *(volatile uint64_t *)(hpet_state.base + offset) = value;
}

/* -- MMIO Mapping ---------------------------------------------- */

/**
 * @brief Check whether a virtual address is already page-mapped
 *
 * @param virt Virtual address to check
 * @return true if the page is present, false otherwise
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
 * @brief Map the HPET MMIO region as uncacheable
 *
 * Maps a single 4 KiB page.  The HPET register block is
 * small enough (offsets up to ~0x200) to fit in one page.
 *
 * @param phys Physical base address of the HPET registers
 * @return 0 on success, -1 on failure
 */
static int hpet_map_mmio(uint64_t phys) {
  uint64_t page = phys & ~0xFFFULL;
  uint64_t virt = phys_to_virt(page);

  if (hpet_mmio_is_mapped(virt))
    return 0;

  if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0) {
    printk(KERN_ERR "HPET: failed to map MMIO phys=0x%lx\n", page);
    return -1;
  }

  return 0;
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
 * @brief Get the current HPET counter value in nanoseconds
 *
 * Returns the absolute time derived from the raw counter.
 * On 32-bit HPET hardware, the counter wraps after ~4294 s.
 * There is no built-in wraparound detection; callers that
 * need monotonic time must handle wrap themselves.
 *
 * @return Nanoseconds corresponding to the current counter
 */
uint64_t hpet_get_time_ns(void) {
  return hpet_ticks_to_ns(hpet_get_counter());
}

/* -- Time Conversion (overflow-safe) --------------------------- */

/**
 * @brief Convert HPET ticks to nanoseconds
 *
 * Uses 128-bit intermediate arithmetic to avoid overflow.
 * For a 32-bit counter (~4.3 × 10^9 ticks) and a 100 ns
 * period, the intermediate product is ~4.3 × 10^11, which
 * fits in 64 bits — but larger tick counts or periods
 * require 128-bit safety.
 *
 * @param ticks Counter ticks
 * @return Equivalent nanoseconds
 */
uint64_t hpet_ticks_to_ns(uint64_t ticks) {
  return (uint64_t)(((__uint128_t)ticks * hpet_state.period_fs) / 1000000ULL);
}

/**
 * @brief Convert nanoseconds to HPET ticks (rounds up)
 *
 * Ceiling division ensures that a delay of N ns never
 * truncates to fewer ticks than required.  For example,
 * with a 100 ns period, delay(1 ns) yields 1 tick.
 *
 * @param ns Nanoseconds
 * @return Equivalent counter ticks (rounded up)
 */
uint64_t hpet_ns_to_ticks(uint64_t ns) {
  uint64_t fs = hpet_state.period_fs;
  return (((__uint128_t)ns * 1000000ULL) + fs - 1) / fs;
}

/* -- Delays (Busy-Wait) ---------------------------------------- */

/**
 * @brief Busy-wait for a given number of nanoseconds
 *
 * Uses unsigned subtraction to handle counter wraparound
 * naturally.  Calls hpet_ns_to_ticks() which rounds up, so
 * the delay is never shorter than requested.
 *
 * @param ns Delay duration in nanoseconds
 */
void hpet_delay_ns(uint64_t ns) {
  if (!hpet_state.initialized || ns == 0)
    return;

  uint64_t start = hpet_get_counter();
  uint64_t ticks = hpet_ns_to_ticks(ns);

  while ((hpet_get_counter() - start) < ticks)
    cpu_pause();
}

/**
 * @brief Busy-wait for a given number of microseconds
 *
 * Computes ticks directly from microseconds to avoid the
 * overflow that would occur from converting to nanoseconds
 * first (us * 1000 overflows for us > ~18.4 × 10^12).
 *
 * @param us Delay duration in microseconds
 */
void hpet_delay_us(uint64_t us) {
  if (!hpet_state.initialized || us == 0)
    return;

  uint64_t start = hpet_get_counter();
  uint64_t ticks =
      (((__uint128_t)us * 1000000ULL) + hpet_state.period_fs - 1) /
      hpet_state.period_fs;

  while ((hpet_get_counter() - start) < ticks)
    cpu_pause();
}

/**
 * @brief Busy-wait for a given number of milliseconds
 *
 * Computes ticks directly from milliseconds to avoid the
 * overflow that would occur from converting to nanoseconds
 * first (ms * 1000000 overflows for ms > ~18.4 × 10^9).
 *
 * @param ms Delay duration in milliseconds
 */
void hpet_delay_ms(uint64_t ms) {
  if (!hpet_state.initialized || ms == 0)
    return;

  uint64_t start = hpet_get_counter();
  uint64_t ticks =
      (((__uint128_t)ms * 1000000000ULL) + hpet_state.period_fs - 1) /
      hpet_state.period_fs;

  while ((hpet_get_counter() - start) < ticks)
    cpu_pause();
}

/* -- Timer Setup ----------------------------------------------- */

/**
 * @brief Configure an HPET timer in one-shot mode
 *
 * Programs the timer to fire a single interrupt after @p ns
 * nanoseconds.  The interrupt is delivered to the CPU via
 * the I/O APIC; the vector must be configured in the I/O APIC
 * redirection table separately.
 *
 * The interrupt route field in the timer config register
 * (bits 63:32) is set to a bitmask with the requested vector
 * encoded.  On most hardware this is informational; actual
 * routing is done through the I/O APIC.
 *
 * @param timer_num Timer index (0 .. num_timers - 1)
 * @param ns        Delay in nanoseconds
 * @param vector    Interrupt vector (32-255)
 * @return 0 on success, -1 on failure
 */
int hpet_timer_oneshot(uint8_t timer_num, uint64_t ns, uint8_t vector) {
  if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
    return -1;

  /* Disable the timer before programming */
  uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
  config &= ~HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  /* Compute target comparator value */
  uint64_t ticks = hpet_ns_to_ticks(ns);
  if (ticks < HPET_MIN_TICKS)
    ticks = HPET_MIN_TICKS;

  uint64_t target = hpet_get_counter() + ticks;
  hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

  /* Build config: vector in route field (bits 63:32), enable */
  config = ((uint64_t)vector << HPET_Tn_ROUTE_SHIFT) | HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  return 0;
}

/**
 * @brief Configure an HPET timer in periodic mode
 *
 * Programs the timer to fire interrupts every @p period_ns
 * nanoseconds.  Uses the value-set mechanism:
 *   1. Disable the timer.
 *   2. Write the first absolute expiration to the comparator.
 *   3. Write config with TYPE_CNF | VAL_SET_CNF | vector | enable.
 *   4. Write the period in ticks to the comparator.
 *
 * This sequence is required by the HPET spec to correctly
 * initialize the periodic accumulator.
 *
 * @param timer_num  Timer index (0 .. num_timers - 1)
 * @param period_ns  Period in nanoseconds
 * @param vector     Interrupt vector (32-255)
 * @return 0 on success, -1 on failure
 */
int hpet_timer_periodic(uint8_t timer_num, uint64_t period_ns,
                        uint8_t vector) {
  if (!hpet_state.initialized || timer_num >= hpet_state.num_timers)
    return -1;

  /* Check periodic capability */
  uint64_t caps = hpet_read(HPET_TIMER_CONFIG(timer_num));
  if (!(caps & HPET_Tn_PER_INT_CAP)) {
    printk(KERN_ERR "HPET timer %u: not periodic-capable\n", timer_num);
    return -1;
  }

  /* Disable the timer before programming */
  uint64_t config = hpet_read(HPET_TIMER_CONFIG(timer_num));
  config &= ~HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  uint64_t period_ticks = hpet_ns_to_ticks(period_ns);
  if (period_ticks < HPET_MIN_TICKS)
    period_ticks = HPET_MIN_TICKS;

  /* Step 1: Write first absolute expiration */
  uint64_t target = hpet_get_counter() + period_ticks;
  hpet_write(HPET_TIMER_COMPARATOR(timer_num), target);

  /* Step 2: Write config with periodic mode + value-set */
  config = ((uint64_t)vector << HPET_Tn_ROUTE_SHIFT) | HPET_Tn_TYPE_CNF |
           HPET_Tn_VAL_SET_CNF | HPET_Tn_INT_ENB_CNF;
  hpet_write(HPET_TIMER_CONFIG(timer_num), config);

  /* Step 3: Write period into the comparator (value-set) */
  hpet_write(HPET_TIMER_COMPARATOR(timer_num), period_ticks);

  return 0;
}

/**
 * @brief Stop an HPET timer
 *
 * Clears the interrupt-enable bit.  The timer continues to
 * count but will not generate interrupts.
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
 * Sequence (per HPET spec):
 *   1. Validate ACPI HPET table and extract address.
 *   2. Map the MMIO region as uncacheable.
 *   3. Read capabilities: period, timer count, counter width.
 *   4. Disable the main counter (clear ENABLE_CNF).
 *   5. Disable all individual timers (clear INT_ENB_CNF).
 *   6. Reset the main counter to zero.
 *   7. Re-enable the main counter (set ENABLE_CNF).
 *
 * @return 0 on success, -1 on failure
 */
int hpet_init(void) {
  if (!acpi_is_initialized()) {
    printk(KERN_ERR "HPET: ACPI not initialized\n");
    return -1;
  }

  uint64_t hpet_phys = acpi_get_hpet_address();
  if (!hpet_phys) {
    printk(KERN_ERR "HPET: not found in ACPI\n");
    return -1;
  }

  /* Validate page alignment (HPET spec requires this) */
  if (hpet_phys & 0xFFFULL) {
    printk(KERN_WARNING "HPET: address 0x%lx not page-aligned\n", hpet_phys);
  }

  /* Map the MMIO region */
  if (hpet_map_mmio(hpet_phys) < 0)
    return -1;

  /*
   * Store the page-aligned virtual address.  phys_to_virt()
   * gives us the HHDM mapping of the page; register offsets
   * are added to this base in hpet_read()/hpet_write().
   */
  hpet_state.base = (volatile uint8_t *)phys_to_virt(hpet_phys & ~0xFFFULL);

  printk(KERN_INFO "HPET: phys=0x%lx virt=%p\n", hpet_phys, hpet_state.base);

  /* Read and validate capabilities */
  uint64_t caps = hpet_read(HPET_GENERAL_CAPS);

  hpet_state.period_fs = caps >> 32;
  if (hpet_state.period_fs == 0) {
    printk(KERN_ERR "HPET: invalid period (0 fs)\n");
    return -1;
  }

  hpet_state.frequency = (uint64_t)(1000000000000000ULL / hpet_state.period_fs);
  hpet_state.num_timers = (uint8_t)(((caps >> 8) & 0x1F) + 1);
  hpet_state.is_64bit = (caps & HPET_COUNTER_SIZE_CAP) != 0;

  printk(KERN_INFO "HPET: period=%lu fs, freq=%lu Hz, timers=%u, %s-bit\n",
         hpet_state.period_fs, hpet_state.frequency, hpet_state.num_timers,
         hpet_state.is_64bit ? "64" : "32");

  /* Step 4: Disable the main counter */
  uint64_t config = hpet_read(HPET_GENERAL_CONFIG);
  config &= ~HPET_ENABLE_CNF;
  hpet_write(HPET_GENERAL_CONFIG, config);

  /* Step 5: Disable all individual timers */
  for (uint8_t i = 0; i < hpet_state.num_timers; i++) {
    uint64_t tc = hpet_read(HPET_TIMER_CONFIG(i));
    tc &= ~HPET_Tn_INT_ENB_CNF;
    hpet_write(HPET_TIMER_CONFIG(i), tc);
  }

  /* Step 6: Reset main counter to zero */
  hpet_write(HPET_MAIN_COUNTER, 0);

  /* Step 7: Re-enable the main counter */
  config = hpet_read(HPET_GENERAL_CONFIG);
  config |= HPET_ENABLE_CNF;
  hpet_write(HPET_GENERAL_CONFIG, config);

  /* Ensure state is fully visible before marking initialized */
  asm volatile("mfence" ::: "memory");
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
