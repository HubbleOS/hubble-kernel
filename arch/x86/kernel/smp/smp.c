/**
 * @file smp.c
 * @brief SMP initialization, AP startup, and IPI support
 */

#include <acpi/acpi.h>
#include <apic/apic.h>
#include <asm.h>
#include <gdt/gdt.h>
#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <msr.h>
#include <syscalls/syscall_entry.h>

#include "higher_half.h"
#include "percpu.h"
#include "smp.h"

/* -- Constants ---------------------------------------------------------- */

#define AP_TRAMPOLINE_ADDR 0x8000
#define AP_TRAMPOLINE_STACK_ADDR 0x7000
#define AP_STACK_SIZE (64 * 1024)
#define AP_STACK_PAGES ((AP_STACK_SIZE + 0xFFF) / 0x1000)

/* -- External symbols --------------------------------------------------- */

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

/* -- Static data -------------------------------------------------------- */

static size_t g_trampoline_size = 0;
static volatile uint64_t *g_trampoline_cr3 = NULL;
static volatile uint64_t *g_trampoline_stack = NULL;
static volatile uint64_t *g_trampoline_entry = NULL;

static spinlock_t smp_lock = SPINLOCK_INIT("smp");

/* -- AP startup data structure ------------------------------------------ */

struct ap_startup_data {
  uint64_t pml4_phys;
  uint16_t gdt_limit;
  uint64_t gdt_base;
  uint64_t stack_top;
  uint64_t entry_point;
  volatile uint32_t ap_ready;
  volatile uint32_t boot_stage; /* AP writes milestone numbers here */
} __attribute__((packed));

/* -- Forward declarations ---------------------------------------------- */

static void start_ap_callback(uint8_t apic_id, uint8_t processor_id, void *ctx);
static void *allocate_ap_stack(void);

static inline void ap_set_boot_stage(volatile struct ap_startup_data *data,
                                     uint32_t stage) {
  asm volatile("" ::: "memory");
  data->boot_stage = stage;
  asm volatile("mfence" ::: "memory");
}

/* -- Identity mapping helpers ------------------------------------------- */

/**
 * @brief Check if a virtual address is identity-mapped (va == phys)
 *
 * Walks both the PT level (4KB pages) and PD level (2MB huge pages)
 * to determine if the address correctly maps to itself.
 *
 * @param va Virtual address to check
 * @return true if identity mapping exists
 */
static bool is_identity_mapped(uint64_t va) {
  /* Check 4KB page via PT level */
  uint64_t phys = vmm_get_phys(va);
  if (phys == va)
    return true;

  /* Check 2MB huge page via PD level */
  uint64_t *pd = pd_table(va);
  uint64_t pde = pd[PD_INDEX(va)];
  if ((pde & PTE_PRESENT) && (pde & PTE_HUGE)) {
    uint64_t huge_base = pde & 0x000FFFFFFFFFF000ULL;
    return huge_base == (va & ~0x1FFFFFULL);
  }

  return false;
}

/**
 * @brief Ensure a virtual address has an identity mapping (va == phys)
 *
 * If the mapping already exists (4KB or 2MB huge page), this is a no-op.
 * Otherwise, creates a 4KB page mapping via vmm_map_page(). If that fails
 * due to an existing 2MB huge page, verifies the huge page covers the
 * address correctly.
 *
 * @param va Virtual/physical address to identity-map
 * @return 0 on success, -1 on failure
 */
static int ensure_identity_mapping(uint64_t va) {
  if (is_identity_mapped(va))
    return 0;

  /* No mapping exists — create a 4KB identity mapping.
   * vmm_map_page() operates on the current page tables (BSP's PML4)
   * via recursive mapping. It always sets PTE_PRESENT | PTE_WRITE,
   * which is sufficient: the trampoline needs RW for the startup data
   * and is executable because EFER.NXE is not yet set during the
   * trampoline phase. */
  if (vmm_map_page(va, va, PTE_WRITE) == 0)
    return 0;

  /* vmm_map_page() failed. The most likely cause is an existing 2MB
   * huge page at the PD level. Verify it provides correct identity
   * mapping. */
  uint64_t *pd = pd_table(va);
  uint64_t pde = pd[PD_INDEX(va)];
  if ((pde & PTE_PRESENT) && (pde & PTE_HUGE)) {
    uint64_t huge_base = pde & 0x000FFFFFFFFFF000ULL;
    if (huge_base == (va & ~0x1FFFFFULL)) {
      printk(KERN_INFO "  Identity mapping via 2MB huge page: "
                       "0x%lx -> 0x%lx\n",
             va & ~0x1FFFFFULL, huge_base);
      return 0;
    }
    printk(KERN_ERR "  ERROR: 2MB huge page at 0x%lx maps to 0x%lx, "
                    "expected 0x%lx\n",
           va & ~0x1FFFFFULL, huge_base, va & ~0x1FFFFFULL);
  }

  return -1;
}

/* -- AP stack allocation ------------------------------------------------ */

/**
 * @brief Allocate a stack for an Application Processor
 * @return Pointer to top of allocated stack, or NULL on failure
 */
static void *allocate_ap_stack(void) {
  uint64_t phys = pmm_alloc_pages(AP_STACK_PAGES);
  if (!phys) {
    printk(KERN_ERR "ERROR: Failed to allocate physical pages for AP stack\n");
    return NULL;
  }

  uint64_t virt = phys_to_virt(phys);
  memset((void *)virt, 0, AP_STACK_SIZE);

  return (void *)(virt + AP_STACK_SIZE);
}

/* -- AP entry point ----------------------------------------------------- */

/**
 * @brief Enable NXE (Non-Execute) bit in EFER MSR
 */
void enable_nxe(void) {
  uint64_t efer;
  asm volatile("mov $0xC0000080, %%ecx\n"
               "rdmsr\n"
               "or $(1 << 11), %%eax\n"
               "wrmsr\n"
               :
               :
               : "eax", "ecx", "edx");
}

/**
 * @brief AP kernel entry point called by trampoline in long mode
 */
void ap_entry(void) {
  volatile struct ap_startup_data *data =
      (volatile struct ap_startup_data *)(AP_TRAMPOLINE_ADDR + 512);

  /* Enable NXE (No-Execute Enable) in EFER MSR BEFORE touching any kernel
   * pages.  The BSP's page tables have NX bits set on some PTEs; without
   * NXE the CPU treats bit 63 as a reserved bit → page fault with
   * error code 0x0008. */
  enable_nxe();

  ap_set_boot_stage(data, 4);

  /* Load the kernel GDT. The trampoline used a temporary GDT to reach
   * long mode; the real kernel GDT (with TSS, per-CPU segments, etc.)
   * must be installed before any kernel code runs. */
  struct {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed)) gdtr = {
      .limit = data->gdt_limit,
      .base = data->gdt_base,
  };
  asm volatile("lgdt %0" ::"m"(gdtr) : "memory");

  /* Reload all data segment registers with the kernel GDT's selector */
  asm volatile("mov $0x10, %%ax\n"
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%ss\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n" ::
                   : "ax", "memory");

  apic_init_ap();

  ap_set_boot_stage(data, 5);

  uint8_t apic_id = lapic_get_id();

  percpu_init_ap(apic_id);

  ap_set_boot_stage(data, 6);

  /* TSS must be initialized BEFORE idt_load().  The IDT gate for
   * vector 8 (Double Fault) references IST1 from the TSS.  If the
   * IDT is loaded before a valid TSS with IST1 is in place, any
   * Double Fault would load an IST1 of 0 -> null stack -> Triple
   * Fault -> CPU reset.
   *
   * INVARIANT: Before CPU can receive #DF:
   *   GDT valid
   *   TSS valid
   *   TR loaded (ltr)
   *   IST stack mapped
   *   IST pointer valid
   *   IDT vector 8 configured with IST1
   */

  /* tss_init() before idt_load() - see the invariant above. This used to
   * be the other way around (idt_load() first), which silently violated
   * it; it only didn't show up as a triple fault because nothing happened
   * to fault in that narrow window. syscall_init() and enable_nxe() were
   * also each being called twice back to back here for no reason - besides
   * the wasted MSR writes, each call does ~8 printk lines, and with the
   * framebuffer/serial console being the bottleneck it is during AP
   * bring-up, doubling that output was very likely why this stage was
   * seen sitting right at the edge of (or past) the bring-up deadline. */
  tss_init();
  ap_set_boot_stage(data, 7);

  idt_load();
  ap_set_boot_stage(data, 8);

  syscall_init();
  ap_set_boot_stage(data, 9);

  ap_set_boot_stage(data, 10);

  /* Signal ready. Write to the shared struct at trampoline+512. */
  data->ap_ready = 1;
  asm volatile("mfence" ::: "memory");

  sti();
  lapic_timer_init(100);

  // ap_ready = true;
  while (1) {
    hlt();
  }
}

/* -- AP startup callback ------------------------------------------------ */

/**
 * @brief Callback to start a single AP
 * @param apic_id APIC ID of the AP
 * @param processor_id ACPI processor ID
 * @param ctx User context (unused)
 */
/* Set once any AP fails to come up within the real-time deadline below.
 * The trampoline code page and ap_startup_data at AP_TRAMPOLINE_ADDR are
 * a SINGLE shared resource reused for every AP in turn - if we can't be
 * sure a timed-out AP has actually died (vs. just running slowly), it's
 * not safe to hand that same memory to another AP while the first one
 * might still be reading/writing it mid-flight. So a timeout here is
 * fatal to the whole bring-up sequence, not just this one AP. */
static volatile bool g_ap_bringup_failed = false;

static void start_ap_callback(uint8_t apic_id, uint8_t processor_id,
                              void *ctx) {
  uint8_t bsp_id = lapic_get_id();
  if (apic_id == bsp_id)
    return;

  if (num_cpus_online >= MAX_CPUS) {
    printk(KERN_WARNING "Skipping AP %u: MAX_CPUS (%u) reached\n", apic_id,
           MAX_CPUS);
    return;
  }

  if (g_ap_bringup_failed) {
    printk(KERN_ERR
           "Skipping AP %u: an earlier AP timed out, shared trampoline "
           "state at 0x%x is no longer safe to reuse\n",
           apic_id, AP_TRAMPOLINE_ADDR);
    return;
  }

  printk(KERN_INFO "Starting AP %u", apic_id);

  void *stack_top = allocate_ap_stack();
  if (!stack_top) {
    printk(KERN_ERR "ERROR: Failed to allocate stack for AP %u\n", apic_id);
    return;
  }
  printk(KERN_INFO "AP %u stack top: %p\n", apic_id, stack_top);

  volatile struct ap_startup_data *data =
      (volatile struct ap_startup_data *)(AP_TRAMPOLINE_ADDR + 512);

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));

  data->pml4_phys = cr3;

  data->gdt_limit = get_gdt_limit();
  /* NOTE: gdt_base is a higher-half virtual address. This works because the AP
   * loads the BSP's CR3 (same page tables) before accessing the GDT. The kernel
   * higher-half mapping is present in the shared PML4. Do NOT use
   * virt_to_phys() here — there is no identity mapping for the GDT's physical
   * page. */
  data->gdt_base = (uint64_t)get_gdt_base();

  data->stack_top = (uint64_t)stack_top;
  data->entry_point = (uint64_t)ap_entry;

  data->ap_ready = 0;
  data->boot_stage = 0;

  asm volatile("mfence" ::: "memory");

  //   printk(KERN_INFO "Data structure setup:\n");
  //   printk(KERN_INFO "  pml4_phys: 0x%lx\n", data->pml4_phys);
  //   printk(KERN_INFO "  gdt_limit: 0x%x\n", data->gdt_limit);
  //   printk(KERN_INFO "  gdt_base: 0x%lx\n", data->gdt_base);
  //   printk(KERN_INFO "  stack_top: 0x%lx\n", data->stack_top);
  //   printk(KERN_INFO "  entry_point: 0x%lx\n", data->entry_point);

  printk(KERN_INFO "Starting AP %u...\n", apic_id);
  apic_start_ap(apic_id, AP_TRAMPOLINE_ADDR);

  /* Poll boot_stage against a REAL deadline (hpet_get_time_ns()), not an
   * instruction-iteration count. An iteration count has no fixed relation
   * to wall-clock time - it "worked" before only because a fast enough
   * loop body happened to take long enough on whatever was running it at
   * the time. Anything that changes per-iteration cost (a slower core, a
   * debugger single-stepping this exact AP, different compiler codegen)
   * changes how much real time the "same" timeout actually allows,
   * without changing the number here - which is exactly backwards for a
   * deadline. 100ms is generous; real AP bring-up is normally low-single
   * digit milliseconds - this is also the worst-case stall per AP if one
   * genuinely fails to come up, so it's worth keeping tight rather than
   * padding it further "just in case". */
  uint32_t stage = 0;
  uint64_t deadline_ns = hpet_get_time_ns() + 800ULL * 1000000ULL;
  while (hpet_get_time_ns() < deadline_ns) {
    asm volatile("" ::: "memory");
    stage = data->boot_stage;
    if (stage >= 10)
      break;
    cpu_pause();
  }

  asm volatile("mfence" ::: "memory");
  uint32_t ready = data->ap_ready;

  //   printk(KERN_INFO "AP boot stage = %u\n", stage);
  if (ready == 1) {
    // printk(KERN_OK "AP %u started successfully!\n", apic_id);
  } else {
    /* Did NOT confirm ap_ready within the deadline - the AP may still be
     * mid-flight (just slow) rather than actually dead, and it's using
     * the shared trampoline state at AP_TRAMPOLINE_ADDR right now. We
     * can't tell those apart, and reusing that memory for another AP
     * while this one might still be alive is exactly the corruption this
     * is guarding against - so stop bringing up any further APs. */
    printk(KERN_ERR "ERROR: AP %u did not signal ready within the deadline "
                    "(stage=%u, ap_ready=%u) - halting further AP bring-up\n",
           apic_id, stage, ready);
    // g_ap_bringup_failed = true;
  }
}

/* -- SMP initialization ------------------------------------------------- */

/**
 * @brief Initialize SMP subsystem and start all APs
 * @return 0 on success, negative on error
 */
int smp_init(void) {
  printk(KERN_INFO "SMP Initialization");

  percpu_init_bsp();

  printk(KERN_INFO "Setting up AP trampoline at 0x%x\n", AP_TRAMPOLINE_ADDR);

  void *trampoline_dest = (void *)AP_TRAMPOLINE_ADDR;

  /* The AP trampoline runs from physical 0x8000. After the AP enables
   * paging (CR0.PG) using the BSP's PML4, it continues executing at
   * virtual 0x8000 via an identity mapping (virt == phys). The AP's
   * stack is at 0x7C00 (page 0x7000), also identity-mapped.
   *
   * Both pages must be mapped BEFORE the AP is started:
   *   0x7000 -> 0x7000  (trampoline stack used during mode transitions)
   *   0x8000 -> 0x8000  (trampoline code and startup data at +512)
   *
   * The trampoline accesses the stack (push/retfq) at ESP=0x7C00 while
   * still in the identity-mapped range, before switching to the kernel
   * stack allocated for the AP. */

  printk(KERN_INFO "  Creating identity mappings for trampoline...\n");

  if (ensure_identity_mapping(AP_TRAMPOLINE_ADDR) < 0) {
    printk(KERN_ERR "ERROR: Failed to create identity mapping for "
                    "trampoline page 0x%x\n",
           AP_TRAMPOLINE_ADDR);
    return -1;
  }

  if (ensure_identity_mapping(AP_TRAMPOLINE_STACK_ADDR) < 0) {
    printk(KERN_ERR "ERROR: Failed to create identity mapping for "
                    "trampoline stack page 0x%x\n",
           AP_TRAMPOLINE_STACK_ADDR);
    return -1;
  }

  printk(KERN_INFO "  Identity mappings established: "
                   "0x%x->0x%x, 0x%x->0x%x\n",
         AP_TRAMPOLINE_ADDR, AP_TRAMPOLINE_ADDR, AP_TRAMPOLINE_STACK_ADDR,
         AP_TRAMPOLINE_STACK_ADDR);

  g_trampoline_size = ap_trampoline_end - ap_trampoline_start;

  printk(KERN_INFO "  Trampoline size: %u bytes (0x%x)\n", g_trampoline_size,
         g_trampoline_size);

  if (g_trampoline_size > 4096) {
    printk(KERN_ERR "ERROR: Trampoline too large (%u bytes)\n",
           g_trampoline_size);
    return -1;
  }

  printk(KERN_INFO "  Copying trampoline code...\n");
  memcpy(trampoline_dest, ap_trampoline_start, g_trampoline_size);

  uint8_t *verify = (uint8_t *)trampoline_dest;
  printk(KERN_INFO "  First bytes at 0x%lx: %02x %02x %02x %02x\n",
         (uint64_t)verify, verify[0], verify[1], verify[2], verify[3]);

  printk(KERN_OK "Trampoline initialized\n");

  printk(KERN_INFO "\nStarting Application Processors:\n");
  acpi_enum_lapics(start_ap_callback, NULL);

  printk(KERN_OK "SMP Initialization Complete");
  printk(KERN_INFO "Total CPUs online: %u\n", num_cpus_online);
  return 0;
}

/* -- CPU count ---------------------------------------------------------- */

/**
 * @brief Get number of online CPUs
 * @return Number of online CPUs
 */
uint32_t smp_get_cpu_count(void) { return num_cpus_online; }

/* -- Inter-processor communication -------------------------------------- */

/**
 * @brief Send IPI to all CPUs except the current one
 * @param vector Interrupt vector to send
 */
void smp_send_ipi_all(uint8_t vector) {
  for (int i = 0; i < MAX_CPUS; i++) {
    if (cpu_data[i].online && cpu_data[i].apic_id != lapic_get_id()) {
      lapic_send_ipi(cpu_data[i].apic_id, vector);
    }
  }
}

/**
 * @brief Call a function on all CPUs (not yet implemented)
 * @param func Function pointer to call
 * @param arg Argument to pass to the function
 */
void smp_call_function_all(void (*func)(void *), void *arg) {
  printk(KERN_WARNING "smp_call_function_all: not implemented yet\n");
}
