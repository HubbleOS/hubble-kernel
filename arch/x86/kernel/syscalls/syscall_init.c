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

cpu_local_t cpu_locals[256];

#define SYSCALL_MAX_CPUS 8
static uint8_t syscall_stacks[SYSCALL_MAX_CPUS][64 * 1024]
    __attribute__((aligned(16)));

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

  if (cpu_id < SYSCALL_MAX_CPUS) {
    cpu_locals[cpu_id].rsp0 =
        (uint64_t)(syscall_stacks[cpu_id] + sizeof(syscall_stacks[cpu_id]));
  } else {
    printk(KERN_WARNING "  Syscall stack not available for cpu_id %u\n",
           cpu_id);
    cpu_locals[cpu_id].rsp0 = 0;
  }
  cpu_locals[cpu_id].cpu_id = cpu_id;
  printk(KERN_INFO "  Syscall stack at 0x%016llx\n", cpu_locals[cpu_id].rsp0);

  uint64_t efer = rdmsr(MSR_EFER);
  efer |= EFER_SCE;
  wrmsr(MSR_EFER, efer);
  printk(KERN_OK "  EFER.SCE enabled\n");

  /* STAR[63:48] is conventionally programmed as the plain (RPL-less)
   * kernel data selector (0x10) and left to SYSRET to OR in RPL=3 for
   * both CS ((STAR[63:48]+16)|3) and SS ((STAR[63:48]+8)|3) on return -
   * that's what this used to do, and it's what the AMD64 manual documents.
   * Empirically (traced live with a hardware watchpoint on a task's saved
   * SS field: it flips from the correct 0x1B to 0x18 - RPL bits gone -
   * the very first time a task returns from a syscall and is later
   * resumed via iretq) that OR-3 isn't happening for SS on this
   * KVM/host CPU combination, even though it clearly does for CS (0x23,
   * confirmed correct in the same traces). iretq then rejects that stale
   * SS: CS.RPL=3 vs SS.RPL=0 is exactly error code 0x18 (GDT selector
   * index 3, i.e. the SS descriptor) on General Protection Fault. This
   * matches a known class of SYSRET erratum/emulation bug (e.g. a long-
   * standing QEMU TCG bug tracked as LP #1428352 had the identical
   * "0x2b -> 0x28" symptom from the same missing OR-3 on SS).
   *
   * Fix: bake RPL=3 into STAR[63:48] itself (0x13 instead of 0x10), so
   * SS ends up correct (0x13+8=0x1B) even without hardware adding the
   * RPL bits. This is harmless on hardware that *does* OR in RPL=3
   * (0x1B|3 == 0x1B, idempotent) and likewise for CS (0x23|3 == 0x23),
   * so it isn't a regression for a correctly-behaving SYSRET either. */
  uint64_t star = 0;
  star |= ((uint64_t)0x08 << 32);
  star |= ((uint64_t)0x13 << 48);
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
