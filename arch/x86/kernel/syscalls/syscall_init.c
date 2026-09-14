/*
 * System call initialisation.
 *
 * Sets up MSR registers for SYSCALL/SYSRET fast system call
 * invocation on x86-64, allocates per-CPU syscall stacks, and
 * installs the system call entry point.
 */

#include <stdint.h>

#include "syscall_entry.h"
#include <apic/apic.h>
#include <hubble/printk.h>
#include <msr.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084
#define EFER_SCE (1 << 0)
#define MSR_KERNEL_GS_BASE 0xC0000102
#define MSR_GS_BASE 0xC0000101

extern void syscall_entry(void);

cpu_local_t cpu_locals[MAX_CPUS];

#define MAX_CPUS 8
static uint8_t syscall_stacks[MAX_CPUS][64 * 1024] __attribute__((aligned(16)));

/**
 * @brief Initialise the SYSCALL/SYSRET fast system call mechanism.
 *
 * Configures the MSR registers needed for x86-64 fast system calls,
 * allocates a per-CPU syscall stack, and sets up the kernel's GS-base
 * for per-CPU data.
 */
void syscall_init(void) {
  printk(KERN_INFO "Initializing SYSCALL/SYSRET...\n");

  uint8_t cpu_id = lapic_get_id();

  cpu_locals[cpu_id].rsp0 =
      (uint64_t)(syscall_stacks[cpu_id] + sizeof(syscall_stacks[cpu_id]));
  cpu_locals[cpu_id].cpu_id = cpu_id;
  printk(KERN_INFO "  Syscall stack at 0x%016llx\n", cpu_locals[cpu_id].rsp0);

  uint64_t efer = rdmsr(MSR_EFER);
  efer |= EFER_SCE;
  wrmsr(MSR_EFER, efer);
  printk(KERN_OK "  EFER.SCE enabled\n");

  uint64_t star = 0;
  star |= ((uint64_t)0x08 << 32);
  star |= ((uint64_t)0x10 << 48);
  wrmsr(MSR_STAR, star);
  printk(KERN_INFO "  STAR = 0x%016llx\n", star);

  wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
  printk(KERN_INFO "  LSTAR = 0x%016llx\n", (uint64_t)syscall_entry);

  wrmsr(MSR_SFMASK, 0x100 | 0x200 | 0x400);
  printk(KERN_INFO "  SFMASK = 0x%llx\n", 0x700ULL);

  wrmsr(MSR_GS_BASE, 0);
  wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&cpu_locals[cpu_id]);

  printk(KERN_OK "SYSCALL/SYSRET initialized successfully\n");
}
