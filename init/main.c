/*
 * Kernel entry point and initialization.
 *
 * This file implements the architecture-independent kernel bootstrap:
 * Limine response consumption, platform info setup, initcall execution,
 * and the main kernel thread that spawns userspace.
 *
 * Limine is the boot protocol. All boot information is obtained
 * directly from Limine request/response structures. There is no
 * intermediate boot-info abstraction.
 */

#include <hubble/cpu.h>
#include <hubble/device.h>
#include <hubble/init.h>
#include <hubble/memory.h>
#include <hubble/module.h>
#include <hubble/platform.h>
#include <hubble/printk.h>
#include <hubble/string.h>

#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/smp.h>
#include <smp/spinlock.h>

#include <asm.h>
#include <higher_half.h>
#include <io.h>

#include <mm/vmm.h>

#include <boot/limine.h>
#include <requests.h>

#include <src/console.h>
#include <user/exec.h>

#include <fs/vfs/vfs.h>
#include <net/arp.h>
#include <net/eth.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <sound/core/dev.h>

/**
 * @brief Global platform information, populated from Limine framebuffer.
 */
platform_info_t g_platform;

/* -- Initcall section boundaries defined by the linker script. */
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
 */
void do_initcalls(void) {
  do_initcalls_range(__start___initcalls_early, __stop___initcalls_early);
  do_initcalls_range(__start___initcalls_core, __stop___initcalls_core);
  do_initcalls_range(__start___initcalls_fs, __stop___initcalls_fs);
  do_initcalls_range(__start___initcalls_device, __stop___initcalls_device);
  do_initcalls_range(__start___initcalls_late, __stop___initcalls_late);
}

extern void rust_init(void);
extern int rust_sum(int a, int b);

/**
 * @brief Architecture-independent kernel entry point.
 *
 * Called from head.asm after BSS clearing. Consumes Limine responses
 * directly for all boot information — framebuffer, memory map, RSDP,
 * initramfs module.
 */
void start_kernel(void) {
  /* -- Limine: bootloader info ---------------------------------------- */
  if (limine_bootloader_info_req.response) {
    printk(KERN_INFO "[boot] bootloader: %s %s\n",
           limine_bootloader_info_req.response->name,
           limine_bootloader_info_req.response->version);
  }

  /* -- Limine: HHDM -------------------------------------------------- */
  uint64_t hhdm = limine_hhdm_req.response->offset;
  printk(KERN_INFO "[boot] HHDM offset: 0x%llx\n", hhdm);

  /* -- Set up recursive page table mapping ---------------------------- */
  /* Limine does not set up recursive page tables. The kernel VMM and
   * APIC MMIO code require PML4[510] to point back to the PML4 itself,
   * enabling access to page table entries via a fixed virtual address. */
  {
    uint64_t cr3 = get_cr3();
    uint64_t pml4_phys = cr3 & ~0xFFFULL;
    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);
    pml4[RECURSIVE_PML4_INDEX] = pml4_phys | PTE_PRESENT | PTE_WRITE;
    asm volatile("invlpg (%0)" : : "r"(pml4_table()) : "memory");
    printk(KERN_INFO "[boot] Recursive PML4[510] = 0x%llx\n",
           pml4[RECURSIVE_PML4_INDEX]);
  }

  /* -- Limine: kernel addresses --------------------------------------- */
  if (limine_exec_addr_req.response) {
    printk(KERN_INFO "[boot] kernel: phys=0x%llx virt=0x%llx\n",
           limine_exec_addr_req.response->physical_base,
           limine_exec_addr_req.response->virtual_base);
  }

  /* -- Limine: memory map --------------------------------------------- */
  uint64_t mem_entries = 0;
  uint64_t largest_base = 0;
  uint64_t largest_size = 0;
  if (limine_memmap_req.response) {
    mem_entries = limine_memmap_req.response->entry_count;
    printk(KERN_INFO "[boot] memory map entries: %llu\n", mem_entries);

    for (uint64_t i = 0; i < mem_entries; i++) {
      struct limine_memmap_entry *e = limine_memmap_req.response->entries[i];
      if (e->type == LIMINE_MEMMAP_USABLE && e->length > largest_size) {
        largest_base = e->base;
        largest_size = e->length;
      }
    }
    printk(KERN_INFO "[boot] largest usable: 0x%llx - 0x%llx (%llu bytes)\n",
           largest_base, largest_base + largest_size, largest_size);
  } else {
    printk(KERN_ERR "[boot] Limine memory map not available\n");
    while (1)
      hlt();
  }

  /* -- Limine: framebuffer -------------------------------------------- */
  if (limine_framebuffer_req.response &&
      limine_framebuffer_req.response->framebuffer_count > 0) {
    struct limine_framebuffer *lfb =
        limine_framebuffer_req.response->framebuffers[0];

    g_platform.fb_base = (uint64_t)lfb->address;
    g_platform.fb_width = lfb->width;
    g_platform.fb_height = lfb->height;
    g_platform.fb_pitch = lfb->pitch;
    g_platform.fb_bpp = lfb->bpp;
    g_platform.fb_red_shift = lfb->red_mask_shift;
    g_platform.fb_red_size = lfb->red_mask_size;
    g_platform.fb_green_shift = lfb->green_mask_shift;
    g_platform.fb_green_size = lfb->green_mask_size;
    g_platform.fb_blue_shift = lfb->blue_mask_shift;
    g_platform.fb_blue_size = lfb->blue_mask_size;

    framebuffer_info_t fb;
    fb.base = lfb->address;
    fb.width = lfb->width;
    fb.height = lfb->height;
    fb.pitch = lfb->pitch;
    fb.bpp = lfb->bpp;

    printk_init(&fb);
    printk(KERN_INFO "[boot] framebuffer: %llux%llu %ubits pitch=%llu\n",
           lfb->width, lfb->height, lfb->bpp, lfb->pitch);
  } else {
    printk(KERN_WARNING "[boot] no framebuffer provided\n");
    printk_init(NULL);
  }

  /* -- Limine: RSDP --------------------------------------------------- */
  /* Limine returns HHDM virtual address; acpi_init expects physical. */
  void *rsdp = NULL;
  if (limine_rsdp_req.response) {
    uint64_t rsdp_virt = (uint64_t)limine_rsdp_req.response->address;
    rsdp = (void *)virt_to_phys(rsdp_virt);
    printk(KERN_INFO "[boot] RSDP: virt=%p phys=%p\n", (void *)rsdp_virt, rsdp);
  } else {
    printk(KERN_WARNING "[boot] RSDP not available\n");
  }

  /* -- Limine: initramfs module --------------------------------------- */
  void *initramfs_data = NULL;
  uint64_t initramfs_size = 0;
  if (limine_module_req.response &&
      limine_module_req.response->module_count > 0) {
    struct limine_file *mod = limine_module_req.response->modules[0];
    initramfs_data = mod->address;
    initramfs_size = mod->size;
    printk(KERN_INFO "[initramfs] module: address=%p size=%llu\n",
           initramfs_data, initramfs_size);
  } else {
    printk(KERN_WARNING "[initramfs] no module found\n");
  }

  /* -- Initialize PMM with Limine memory map -------------------------- */
  /* Store the largest usable region bounds for PMM.
   * The PMM will use the HHDM to access physical memory directly. */
  extern uint64_t g_pmm_heap_phys_start;
  extern uint64_t g_pmm_heap_phys_end;
  g_pmm_heap_phys_start = largest_base;
  g_pmm_heap_phys_end = largest_base + largest_size;

  /* -- Early subsystem init ------------------------------------------- */
  rust_init();
  int sum = rust_sum(3, 4);
  printk(KERN_INFO "Rust sum result: %d\n", sum);

  boot_cpu_init();
  acpi_init(rsdp);
  boot_memory_init();
  console_init_late();
  hpet_init();
  apic_init_bsp();

  smp_init();

  /* -- Initramfs init (must happen before initcalls mount it) --------- */
  if (initramfs_data && initramfs_size > 0) {
    extern int initramfs_init(void *data, uint64_t size);
    if (initramfs_init(initramfs_data, initramfs_size) < 0) {
      printk(KERN_ERR "[initramfs] failed to initialize\n");
    }
  }

  do_initcalls();

  scheduler_init();

  while (1)
    hlt();
}

/* -- Starting init ------------------------------------------------------ */

#define INIT_PATH_MAX 128

/* Tried in order when the command line names no init, as in Linux. */
static const char *const default_inits[] = {
    "/init", "/sbin/init", "/etc/init", "/bin/init", "/bin/sh", NULL,
};

/**
 * @brief Copy the value of "@p key=" on the kernel command line to @p buf
 * @return false if the command line has no such option
 */
static bool cmdline_get(const char *key, char *buf, size_t size) {
  if (!limine_cmdline_req.response || !limine_cmdline_req.response->cmdline)
    return false;

  size_t keylen = strlen(key);
  for (const char *p = limine_cmdline_req.response->cmdline; *p;) {
    while (*p == ' ')
      p++;
    const char *end = p;
    while (*end && *end != ' ')
      end++;

    if ((size_t)(end - p) > keylen && !strncmp(p, key, keylen) &&
        p[keylen] == '=') {
      size_t n = end - p - keylen - 1;
      if (n >= size)
        n = size - 1;
      memcpy(buf, p + keylen + 1, n);
      buf[n] = '\0';
      return true;
    }
    p = end;
  }
  return false;
}

/**
 * @brief Start @p path as pid 1 on the console
 * @return false if it is not there or will not load
 */
static bool try_init(const char *path) {
  VFS_File *f = vfs_open(path, VFS_O_RDONLY);
  if (IS_ERR(f) || !f)
    return false;
  vfs_close(f);

  char *argv[] = {(char *)path, NULL};
  char *envp[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", "HOME=/", "TERM=linux",
                  NULL};
  task_t *init = execv(path, argv, envp);
  if (!init)
    return false;

  init->id.pid = 1;
  if (fd_open_console(init) < 0)
    printk(KERN_ERR "init: cannot open the console for %s\n", path);
  scheduler_add_task(init);
  printk(KERN_INFO "init: running %s\n", path);
  return true;
}

/**
 * @brief Start the first user process: "init=" from the command line, or
 *        the first of default_inits that exists
 */
static void start_init(void) {
  char path[INIT_PATH_MAX];
  if (cmdline_get("init", path, sizeof(path))) {
    if (try_init(path))
      return;
    printk(KERN_ERR "init: cannot run init=%s, trying the defaults\n", path);
  }

  for (int i = 0; default_inits[i]; i++)
    if (try_init(default_inits[i]))
      return;

  printk(KERN_EMERG "init: no init found; put one in the initramfs or pass "
                    "init=<path> on the kernel command line\n");
}

/**
 * @brief Main kernel thread running after scheduler start.
 *
 * Sets up pipes and spawns the initial userspace process.
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
  printk(KERN_INFO "opened pipe /pipe/term\n");

  VFS_File *tty_out = vfs_open("/pipe/tty0_out", VFS_O_RDWR | VFS_O_CREAT);
  if (IS_ERR(tty_out) || tty_out == NULL) {
    printk(KERN_ERR "failed to open pipe /pipe/tty0_out: %d\n",
           IS_ERR(tty_out) ? PTR_ERR(tty_out) : -1);
    while (1)
      hlt();
  }
  module_load("/modules/input.ko");
  module_load("/modules/tty.ko");
  module_load("/modules/usb.ko"); /* after input.ko: registers keyboards */
  module_load("/modules/procfs.ko");
  module_load("/modules/video.ko");

  start_init();

  printk(KERN_INFO "kmain thread done\n");

  /* Nothing left to do: block for good. Spinning in hlt here kept this
   * task "running" for its whole (long, priority-255) slice, delaying
   * every other task on this CPU - e.g. a poll() timeout in vi. */
  while (1)
    task_sleep();
}
