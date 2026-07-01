/*
 * Kernel entry point and initialization.
 *
 * This file implements the architecture-independent kernel bootstrap:
 * platform info setup, initcall execution, and the main kernel thread
 * that spawns userspace.
 */

#include <hubble/cpu.h>
#include <hubble/device.h>
#include <hubble/init.h>
#include <hubble/memory.h>
#include <hubble/module.h>
#include <hubble/platform.h>
#include <hubble/printk.h>

#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/smp.h>
#include <smp/spinlock.h>

#include <asm.h>
#include <higher_half.h>
#include <io.h>

#include <bootinfo/bootinfo.h>
#include <src/console.h>
#include <user/exec.h>

#include <fs/vfs/vfs.h>
#include <net/arp.h>
#include <net/eth.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <sound/core/dev.h>

/**
 * @brief Global platform information, populated from boot info.
 */
platform_info_t g_platform;

/**
 * @brief Boot info passed by the bootloader.
 */
BootInfo *g_boot_info;

/* Initcall section boundaries defined by the linker script. */
extern initcall_t __initcalls_start[];
extern initcall_t __initcalls_end[];
extern initcall_t __start___initcalls_early[];
extern initcall_t __stop___initcalls_early[];
extern initcall_t __start___initcalls_core[];
extern initcall_t __stop___initcalls_core[];
extern initcall_t __start___initcalls_fs[];
extern initcall_t __stop___initcalls_fs[];
extern initcall_t __start___initcalls_device[];
extern initcall_t __stop___initcalls_device[];
extern initcall_t __start___initcalls_late[];
extern initcall_t __stop___initcalls_late[];

/**
 * @brief Execute all initcall functions in the given range.
 *
 * @param start Pointer to the first initcall in the section.
 * @param end   Pointer past the last initcall in the section.
 */
static void do_initcalls_range(initcall_t *start, initcall_t *end) {
  for (initcall_t *fn = start; fn < end; fn++) {
    int ret = (*fn)();
    if (ret != 0)
      printk(KERN_ERR "[init] initcall %p failed: %d\n", fn, ret);
  }
}

/**
 * @brief Execute all registered initcalls in order.
 *
 * Initcalls are placed in named ELF sections (early, core, fs, device, late)
 * by the linker. This function calls each group sequentially, preserving the
 * dependency order required for correct hardware and subsystem bring-up.
 */
void do_initcalls(void) {
  do_initcalls_range(__start___initcalls_early, __stop___initcalls_early);
  do_initcalls_range(__start___initcalls_core, __stop___initcalls_core);
  do_initcalls_range(__start___initcalls_fs, __stop___initcalls_fs);
  do_initcalls_range(__start___initcalls_device, __stop___initcalls_device);
  do_initcalls_range(__start___initcalls_late, __stop___initcalls_late);
}

/**
 * @brief Architecture-independent kernel entry point.
 *
 * Called from the architecture-specific bootstrap code after initial
 * page tables and a basic C runtime are set up. Performs:
 *   1. Platform info population from boot info
 *   2. Printk (console) initialization
 *   3. CPU, ACPI, memory, APIC, and HPET bring-up
 *   4. SMP start
 *   5. Initcall execution for all subsystems
 *   6. Scheduler start
 */
void start_kernel(void) {
  g_platform.fb_base = (uint64_t)g_boot_info->framebuffer.base;
  g_platform.fb_width = g_boot_info->framebuffer.width;
  g_platform.fb_height = g_boot_info->framebuffer.height;
  g_platform.fb_pitch = g_boot_info->framebuffer.pitch;

  printk_init(&g_boot_info->framebuffer);

  boot_cpu_init();
  acpi_init(g_boot_info->rsdp);
  boot_memory_init();
  apic_init_bsp();
  hpet_init();

  smp_init();

  do_initcalls();

  scheduler_init();

  while (1)
    hlt();
}

/**
 * @brief Main kernel thread running after scheduler start.
 *
 * This is the first task scheduled by the kernel. It sets up inter-process
 * communication pipes and spawns the initial userspace process.
 */
void kmain_thread(void) {
  printk(KERN_INFO "kmain thread\n");

  VFS_File *pipe = vfs_open("/pipe/term", VFS_O_RDWR | VFS_O_CREAT);
  if (IS_ERR(pipe) || pipe == NULL) {
    printk(KERN_ERR "failed to open pipe: %d\n",
           IS_ERR(pipe) ? PTR_ERR(pipe) : -1);
    while (1)
      hlt();
  }

  VFS_File *tty_out = vfs_open("/pipe/tty0_out", VFS_O_RDWR | VFS_O_CREAT);
  if (IS_ERR(tty_out) || tty_out == NULL) {
    printk(KERN_ERR "failed to open pipe /pipe/tty0_out: %d\n",
           IS_ERR(tty_out) ? PTR_ERR(tty_out) : -1);
    while (1)
      hlt();
  }

  task_t *task1 = exec("/usr/bin/user1.elf");
  if (task1 != NULL)
    scheduler_add_task(task1);

  while (1)
    hlt();
}
