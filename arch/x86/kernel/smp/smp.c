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

static volatile bool ap_ready = false;
static spinlock_t smp_lock = SPINLOCK_INIT("smp");

/* -- AP startup data structure ------------------------------------------ */

struct ap_startup_data {
  uint64_t pml4_phys;
  uint16_t gdt_limit;
  uint64_t gdt_base;
  uint64_t stack_top;
  uint64_t entry_point;
  volatile uint32_t ap_ready;
} __attribute__((packed));

/* -- Forward declarations ---------------------------------------------- */

static void start_ap_callback(uint8_t apic_id, uint8_t processor_id, void *ctx);
static void *allocate_ap_stack(void);

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
  apic_init_ap();

  uint8_t apic_id = lapic_get_id();

  percpu_init_ap(apic_id);

  hpet_init();

  idt_load();
  tss_init();
  syscall_init();
  enable_nxe();
  ap_ready = true;

  uint64_t rflags;
  asm volatile("pushfq; pop %0" : "=r"(rflags));
  printk(KERN_INFO "AP %u: RFLAGS=0x%lx, IF=%d\n", lapic_get_id(), rflags,
         (rflags >> 9) & 1);
  sti();
  printk(KERN_INFO "\nAP %u online!\nHello from AP %u \n\n", apic_id, apic_id);
  lapic_timer_init(100);
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
static void start_ap_callback(uint8_t apic_id, uint8_t processor_id,
                              void *ctx) {
  uint8_t bsp_id = lapic_get_id();
  if (apic_id == bsp_id)
    return;

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
  data->gdt_base = virt_to_phys((uint64_t)get_gdt_base());

  data->stack_top = (uint64_t)stack_top;
  data->entry_point = (uint64_t)ap_entry;

  data->ap_ready = 0;

  asm volatile("mfence" ::: "memory");

  printk(KERN_INFO "Data structure setup:\n");
  printk(KERN_INFO "  pml4_phys: 0x%lx\n", data->pml4_phys);
  printk(KERN_INFO "  gdt_limit: 0x%x\n", data->gdt_limit);
  printk(KERN_INFO "  gdt_base: 0x%lx\n", data->gdt_base);
  printk(KERN_INFO "  stack_top: 0x%lx\n", data->stack_top);
  printk(KERN_INFO "  entry_point: 0x%lx\n", data->entry_point);

  printk(KERN_INFO "Starting AP %u...\n", apic_id);
  apic_start_ap(apic_id, AP_TRAMPOLINE_ADDR);

  for (volatile int i = 0; i < 10000000; i++)
    ;

  volatile uint32_t *marker =
      (volatile uint32_t *)(AP_TRAMPOLINE_ADDR +
                            offsetof(struct ap_startup_data, ap_ready));

  printk(KERN_INFO "AP marker: 0x%x\n", *marker);

  printk(KERN_INFO "Waiting for AP %u to signal ready...\n", apic_id);
  uint64_t timeout = 1000000000;

  while (data->ap_ready == 0 && timeout > 0) {
    timeout--;

    if (timeout % 100000000 == 0) {
      printk(KERN_INFO "  Still waiting... (ap_ready=%u)\n", data->ap_ready);
    }

    asm volatile("pause" ::: "memory");
  }

  if (data->ap_ready) {
    printk(KERN_OK "AP %u started successfully!\n", apic_id);
  } else {
    printk(KERN_ERR "AP %u failed to start (timeout)\n", apic_id);
    printk(KERN_INFO "  Final ap_ready value: %u\n", data->ap_ready);
  }

  for (volatile int i = 0; i < 10000000; i++)
    ;
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

  printk(KERN_INFO "  Using identity mapping: virt 0x%lx = phys 0x%x\n",
         (uint64_t)trampoline_dest, AP_TRAMPOLINE_ADDR);

  printk(KERN_INFO "  Creating identity mapping for trampoline...\n");

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
