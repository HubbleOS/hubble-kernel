/**
 * @file elf.c
 * @brief ELF64 user-space binary loader
 *
 * Implements loading of ELF64 executables: reads the file from
 * the VFS, maps PT_LOAD segments into a user page table with
 * appropriate permissions, copies file content, and transfers
 * control to user space via user_enter().
 */

#include "elf.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <stdint.h>

#include <fs/vfs/vfs.h>

#include "higher_half.h"

/* -- ELF Constants --------------------------------------------- */

#define ELF_MAGIC 0x464c457fUL
#define PAGE_SIZE 4096

#define PT_LOAD 0x1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define USER_STACK_PAGES 128
#define USER_STACK_TOP 0x70000000ULL

/* -- Segment Loading ------------------------------------------- */

/**
 * @brief Load one PT_LOAD segment from an ELF file
 *
 * Allocates physical pages for the segment, maps them into the
 * target page table with the correct user/supervisor, RW, and
 * NX flags, and copies the file-backed portion into memory.
 * The remainder (BSS) is zero-filled by the prior page clearing.
 *
 * @param f      Opened ELF file handle
 * @param phdr   Program header describing the segment
 * @param target_pm  Target user page table (PML4 physical)
 * @return 0 on success, -1 on failure
 */
int elf_load_segment(VFS_File *f, Elf64_Phdr *phdr, uint64_t *target_pm) {
  if (phdr->p_type != PT_LOAD)
    return 0;

  uint64_t vaddr = phdr->p_vaddr;
  uint64_t memsz = phdr->p_memsz;
  uint64_t filesz = phdr->p_filesz;
  uint64_t offset = phdr->p_offset;

  if (memsz < filesz)
    return -1;

  uint64_t map_start = vaddr & ~(PAGE_SIZE - 1);
  uint64_t map_end = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  printk(KERN_INFO
         "[ELF] Loading segment: vaddr=0x%llx size=0x%llx filesz=0x%llx\n",
         (unsigned long long)vaddr, (unsigned long long)memsz,
         (unsigned long long)filesz);

  for (uint64_t addr = map_start; addr < map_end; addr += PAGE_SIZE) {
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
      printk(KERN_ERR "[ELF] ERROR: Failed to allocate page for 0x%llx\n",
             addr);
      return -1;
    }

    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (phdr->p_flags & PF_W)
      flags |= PTE_WRITE;
    if (!(phdr->p_flags & PF_X))
      flags |= PTE_NX;

    if (vmm_map_page_into(target_pm, addr, phys, flags) < 0) {
      printk(KERN_ERR "[ELF] ERROR: Failed to map 0x%llx -> 0x%llx\n", addr,
             phys);
      pmm_free_page(phys);
      return -1;
    }

    uint8_t *kptr = (uint8_t *)phys_to_virt(phys);
    kptr[0] = 0xAA;
    uint8_t val = kptr[0];

    if (val != 0xAA) {
      printk(KERN_ERR "[ELF] ERROR: Failed to map 0x%llx -> 0x%llx\n", addr,
             phys);
      pmm_free_page(phys);
      return -1;
    }

    memset((void *)phys_to_virt(phys), 0, PAGE_SIZE);
  }

  if (filesz > 0) {
    size_t remaining = (size_t)filesz;
    uint64_t file_offset = 0;

    void *kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!kbuf) {
      printk(KERN_ERR "[ELF] ERROR: Failed to allocate temp buffer\n");
      return -1;
    }

    while (remaining > 0) {
      uint64_t dest_va = vaddr + file_offset;
      uint64_t page_base = dest_va & ~(PAGE_SIZE - 1);
      uint64_t in_page_off = dest_va & (PAGE_SIZE - 1);

      size_t chunk = PAGE_SIZE - in_page_off;
      if (chunk > remaining)
        chunk = remaining;

      if (vfs_lseek(f, offset + file_offset, SEEK_SET) < 0) {
        printk(KERN_ERR "[ELF] ERROR: Failed to seek to offset 0x%llx\n",
               (unsigned long long)(offset + file_offset));
        kfree(kbuf);
        return -1;
      }

      size_t got = vfs_read(f, kbuf, chunk);
      if (got != chunk) {
        printk(KERN_ERR "[ELF] ERROR: Read returned %zu, expected %zu\n", got,
               chunk);
        kfree(kbuf);
        return -1;
      }

      uint64_t phys = vmm_get_phys_from(target_pm, page_base);
      if (!phys) {
        printk(KERN_ERR "[ELF] ERROR: Page 0x%llx not mapped!\n",
               (unsigned long long)page_base);
        kfree(kbuf);
        return -1;
      }

      void *kaddr = (void *)phys_to_virt(phys);
      memcpy((uint8_t *)kaddr + in_page_off, kbuf, chunk);
      file_offset += chunk;
      remaining -= chunk;
    }

    kfree(kbuf);
  }

  printk(KERN_INFO "0x40a000 -> 0x%llx\n",
         vmm_get_phys_from(target_pm, 0x40a000));
  printk(KERN_INFO "0x409000 -> 0x%llx\n",
         vmm_get_phys_from(target_pm, 0x409000));
  printk(KERN_INFO "0x408000 -> 0x%llx\n",
         vmm_get_phys_from(target_pm, 0x408000));
  debug_dump_mapping(target_pm, 0x40a000);
  debug_dump_mapping(target_pm, 0x409000);
  debug_dump_mapping(target_pm, 0x408000);
  printk(KERN_OK "[ELF] Segment loaded successfully\n");
  return 0;
}

/* -- Full ELF Load --------------------------------------------- */

/**
 * @brief Load an ELF binary from the VFS
 *
 * Opens the file, validates the ELF magic, reads program headers,
 * and loads each PT_LOAD segment via elf_load_segment().
 *
 * @param path      VFS path to the ELF binary
 * @param entry_out Receives the entry-point virtual address
 * @param pm        Target user page table to map into
 * @return 0 on success, -1 on failure
 */
int elf_load(const char *path, uint64_t *entry_out, uint64_t *pm) {
  VFS_File *f = vfs_open(path, VFS_O_RDONLY);
  printk(KERN_INFO "[ELF] Trying to open %s\n", path);
  if (IS_ERR(f) || !f) {
    printk(KERN_ERR "[ELF] ERROR: Failed to open %s (err=%d)\n", path,
           IS_ERR(f) ? PTR_ERR(f) : -1);
    return -1;
  }

  Elf64_Ehdr ehdr;
  if (vfs_read(f, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
    printk(KERN_ERR "[ELF] ERROR: Failed to read ELF header\n");
    vfs_close(f);
    return -1;
  }

  uint32_t magic = *(uint32_t *)ehdr.e_ident;
  if (magic != ELF_MAGIC) {
    printk(KERN_ERR "[ELF] ERROR: Invalid ELF magic: 0x%x\n", magic);
    vfs_close(f);
    return -1;
  }

  printk(KERN_INFO "[ELF] Loading %s (entry=0x%llx, phnum=%u)\n", path,
         (unsigned long long)ehdr.e_entry, ehdr.e_phnum);

  size_t ph_size = (size_t)ehdr.e_phnum * sizeof(Elf64_Phdr);
  Elf64_Phdr *phdrs = kmalloc(ph_size, GFP_KERNEL);
  if (!phdrs) {
    printk(KERN_ERR "[ELF] ERROR: Failed to allocate phdrs\n");
    vfs_close(f);
    return -1;
  }

  if (vfs_lseek(f, ehdr.e_phoff, SEEK_SET) < 0) {
    printk(KERN_ERR "[ELF] ERROR: Failed to seek to phdrs\n");
    kfree(phdrs);
    vfs_close(f);
    return -1;
  }

  if (vfs_read(f, phdrs, ph_size) != (size_t)ph_size) {
    printk(KERN_ERR "[ELF] ERROR: Failed to read phdrs\n");
    kfree(phdrs);
    vfs_close(f);
    return -1;
  }

  for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
    Elf64_Phdr *p = &phdrs[i];
    if (p->p_type == PT_LOAD) {
      printk(KERN_INFO "[ELF] HERE loading PHDR[%u]: vaddr=0x%llx "
                       "filesz=0x%llx memsz=0x%llx flags=0x%x\n",
             i, (unsigned long long)p->p_vaddr, (unsigned long long)p->p_filesz,
             (unsigned long long)p->p_memsz, p->p_flags);

      if (elf_load_segment(f, p, pm) < 0) {
        printk(KERN_ERR "[ELF] ERROR: Failed to load segment %u\n", i);
        kfree(phdrs);
        vfs_close(f);
        return -1;
      }
    }
  }

  kfree(phdrs);
  vfs_close(f);

  *entry_out = ehdr.e_entry;
  printk(KERN_OK "[ELF] Load complete, entry=0x%llx\n",
         (unsigned long long)*entry_out);
  return 0;
}

/* -- Userspace Entry ------------------------------------------- */

extern void user_enter(uint64_t entry, uint64_t stack);

/**
 * @brief Set up user stack and enter userspace
 *
 * Allocates and maps a user stack, sets up segment registers
 * in the current task context, and calls user_enter() to
 * perform the mode switch.
 *
 * @param entry Entry-point virtual address
 * @return Never returns on success, -1 on failure
 */
int elf_run(uint64_t entry) {
  printk(KERN_INFO "[ELF] Setting up user stack at 0x%llx\n",
         (unsigned long long)USER_STACK_TOP);

  uint64_t stack_base = USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE;

  for (uint64_t addr = stack_base; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
      printk(KERN_ERR "[ELF] Failed to alloc stack page\n");
      return -1;
    }

    if (vmm_map_page(addr, phys, PTE_PRESENT | PTE_USER | PTE_WRITE) < 0) {
      printk(KERN_ERR "[ELF] Failed to map stack page\n");
      pmm_free_page(phys);
      return -1;
    }

    uint8_t *kptr = (uint8_t *)phys_to_virt(phys);
    kptr[0] = 0xAA;
    uint8_t val = kptr[0];
    if (val != 0xAA) {
      printk(KERN_ERR "[ELF] ERROR: Failed to map stack page\n");
      vmm_unmap_user_page(addr);
      pmm_free_page(phys);
      return -1;
    }

    memset((uint8_t *)(phys_to_virt(phys)), 0, PAGE_SIZE);
  }

  printk(KERN_INFO "[ELF] Entering userspace at 0x%llx with stack 0x%llx\n",
         (unsigned long long)entry, (unsigned long long)USER_STACK_TOP);

  printk(KERN_INFO "address of user_enter: %x\n", virt_to_phys(entry));

  printk(KERN_INFO "[ELF] About to enter userspace:\n");
  printk(KERN_INFO "  Entry point: 0x%llx\n", (unsigned long long)entry);
  printk(KERN_INFO "  Stack top: 0x%llx\n", (unsigned long long)USER_STACK_TOP);
  printk(KERN_INFO "  User CS should be: 0x1B\n");
  printk(KERN_INFO "  User SS should be: 0x23\n");

  printk(KERN_INFO "[ELF] Verifying entry point mapping:\n");
  uint64_t entry_phys = vmm_get_phys(entry);
  printk(KERN_INFO "  Virtual: 0x%llx\n", (unsigned long long)entry);
  printk(KERN_INFO "  Physical: 0x%llx\n", (unsigned long long)entry_phys);

  if (!entry_phys) {
    printk(KERN_ERR "[ELF] CRITICAL ERROR: Entry point 0x%llx is NOT mapped!\n",
           entry);
    return -1;
  }

  uint8_t *phys_ptr = (uint8_t *)phys_to_virt(entry_phys);
  printk(KERN_INFO "  Code via phys: %02x %02x %02x %02x %02x %02x %02x %02x\n",
         phys_ptr[0], phys_ptr[1], phys_ptr[2], phys_ptr[3], phys_ptr[4],
         phys_ptr[5], phys_ptr[6], phys_ptr[7]);

  printk(KERN_INFO "[ELF] Flushing TLB before userspace entry...\n");
  asm volatile("invlpg (%0)" : : "r"(entry));

  printk(KERN_INFO "[ELF] Verifying page table chain for 0x%llx:\n", entry);
  uint64_t *pml4 = pml4_table();
  uint64_t pml4e = pml4[PML4_INDEX(entry)];
  printk(KERN_INFO "  PML4E[%d] = 0x%llx (USER=%d)\n", PML4_INDEX(entry), pml4e,
         !!(pml4e & PTE_USER));

  uint64_t *pdpt = pdpt_table(entry);
  uint64_t pdpte = pdpt[PDPT_INDEX(entry)];
  printk(KERN_INFO "  PDPTE[%d] = 0x%llx (USER=%d)\n", PDPT_INDEX(entry), pdpte,
         !!(pdpte & PTE_USER));

  uint64_t *pd = pd_table(entry);
  uint64_t pde = pd[PD_INDEX(entry)];
  printk(KERN_INFO "  PDE[%d] = 0x%llx (USER=%d)\n", PD_INDEX(entry), pde,
         !!(pde & PTE_USER));

  uint64_t *pt = pt_table(entry);
  uint64_t pte = pt[PT_INDEX(entry)];
  printk(KERN_INFO "  PTE[%d] = 0x%llx (USER=%d)\n", PT_INDEX(entry), pte,
         !!(pte & PTE_USER));

  task_t *current_task = get_current_task();

  current_task->context.ds = 0x23;
  current_task->context.es = 0x23;
  current_task->context.fs = 0x23;
  current_task->context.gs = 0x23;

  user_enter(entry, USER_STACK_TOP);

  printk(KERN_ERR "[ELF] ERROR: Returned from userspace!\n");
  return -1;
}
