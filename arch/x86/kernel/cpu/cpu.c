/**
 * @file cpu.c
 * @brief CPU subsystem initialisation (BSP)
 *
 * Detects and enables CPU features (NX, SSE) and initialises
 * GDT, IDT, TSS, interrupt controllers, and syscall entry.
 */

#include <hubble/printk.h>

#include <gdt/gdt.h>
#include <interrupt/interrupt.h>
#include <syscalls/syscall_entry.h>

/* -- Feature Detection / Enable -------------------------------- */

/**
 * @brief Check for NX (No-eXecute) support and enable it via EFER
 */
static inline void check_nx_support(void) {
  uint32_t eax, ebx, ecx, edx;

  asm volatile("mov $0x80000001, %%eax\n"
               "cpuid"
               : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));

  if (edx & (1 << 20)) {
    printk(KERN_INFO "[CPU] NX bit supported\n");

    uint64_t efer;
    asm volatile("rdmsr" : "=A"(efer) : "c"(0xC0000080));
    efer |= (1 << 11);
    asm volatile("wrmsr" ::"A"(efer), "c"(0xC0000080));
    printk(KERN_OK "[CPU] NX bit enabled\n");
  } else {
    printk(KERN_WARNING "[CPU] WARNING: NX bit not supported!\n");
  }
}

/**
 * @brief Enable SSE / FXSR / XMM exceptions in CR0 and CR4
 */
static void enable_sse(void) {
  uint64_t cr0, cr4;

  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 &= ~(1ULL << 2);
  cr0 |= (1ULL << 1);
  asm volatile("mov %0, %%cr0" ::"r"(cr0));

  asm volatile("mov %%cr4, %0" : "=r"(cr4));
  cr4 |= (1ULL << 9);
  cr4 |= (1ULL << 10);
  asm volatile("mov %0, %%cr4" ::"r"(cr4));

  asm volatile("mov %%cr4, %0" : "=r"(cr4));
  printk(KERN_INFO
         "[CPU] CR4 after SSE init: 0x%llx, OSFXSR=%d OSXMMEXCPT=%d\n",
         cr4, !!(cr4 & (1 << 9)), !!(cr4 & (1 << 10)));
}

/* -- Public API ------------------------------------------------ */

/**
 * @brief Initialise all CPU subsystems on the BSP
 *
 * Called once during boot to enable CPU features and set up
 * segmentation, interrupts, and syscall handling.
 */
void boot_cpu_init(void) {
  printk(KERN_INFO "Initializing CPU subsystems...\n");
  check_nx_support();
  enable_sse();

  gdt_init();
  idt_init();
  tss_init();
  interrupts_init();
  syscall_init();

  printk(KERN_OK "CPU initialization complete\n");
}
