/*
 * Syscall: map files or devices into memory.
 *
 * Implements the mmap system call for creating memory mappings, either
 * anonymous (zero-filled) or backed by device memory.  Also provides
 * helper functions for file descriptor lookup used by other syscalls.
 */

#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>
#include <stddef.h>

#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <mm/kmalloc.h>
#include <mm/map/vm_map.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include <higher_half.h>

#include "syscall_entry.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
#define MAP_FIXED 0x10

#define PT_LOAD 0x1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/**
 * @brief Look up a file descriptor entry by number.
 *
 * @param task Task whose FD table to search.
 * @param fd   File descriptor number.
 *
 * @return Pointer to the fd_entry_t on success, or NULL if @p fd is
 *         out of range.
 */
fd_entry_t *task_get_fd(task_t *task, int fd) {
  if (fd < 0 || fd >= MAX_FDS)
    return NULL;
  return &task->fds[fd];
}

/**
 * @brief Find a free file descriptor entry.
 *
 * Scans the task's FD table starting from fd 2 and returns the first
 * unused entry.
 *
 * @param task Task whose FD table to search.
 *
 * @return Pointer to a free fd_entry_t, or NULL if none is available.
 */
fd_entry_t *task_get_free_fd(task_t *task) {
  for (int i = 2; i < MAX_FDS; i++) {
    if (!task->fds[i].data)
      return &task->fds[i];
  }
  return NULL;
}

/**
 * @brief Map files or devices into memory.
 *
 * Creates a virtual memory mapping in the current task's address space.
 * Anonymous mappings allocate zero-filled physical pages; file-backed
 * mappings use the device's mmap callback to obtain physical memory.
 *
 * @param addr   Desired virtual address (hint, or fixed if MAP_FIXED).
 * @param length Size of the mapping in bytes.
 * @param prot   Memory protection flags (PROT_READ, PROT_WRITE, PROT_EXEC).
 * @param flags  Mapping type flags (MAP_ANONYMOUS, MAP_FIXED).
 * @param fd     File descriptor for file-backed mappings.
 * @param offset Offset into the file or device.
 *
 * @return Mapped virtual address on success, or -1 on error.
 */
long sys_mmap(uint64_t addr, size_t length, int prot, int flags, int fd,
              uint64_t offset) {
  if (length == 0) {
    return -1;
  }

  task_t *current = get_current_task();
  if (!current->vm_map)
    current->vm_map = vm_map_create();
  if (!current->vm_map) {
    return -EFAULT;
  }

  size_t size = PAGE_ALIGN_UP(length);

  uint64_t vaddr;
  if ((flags & MAP_FIXED) && addr)
    vaddr = addr;
  else
    vaddr = vm_find_free_range(current->vm_map, size);

  uint32_t vm_flags = 0;
  if (prot & PROT_READ)
    vm_flags |= VM_READ;
  if (prot & PROT_WRITE)
    vm_flags |= VM_WRITE;
  if (prot & PROT_EXEC)
    vm_flags |= VM_EXEC;

  uint64_t pte_flags = PTE_PRESENT | PTE_USER;
  if (prot & PROT_WRITE)
    pte_flags |= PTE_WRITE;
  if (!(prot & PROT_EXEC))
    pte_flags |= PTE_NX;

  if (flags & MAP_ANONYMOUS) {
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
      uint64_t phys = pmm_alloc_page();
      if (!phys)
        return -EFAULT;

      memset((void *)phys_to_virt(phys), 0, PAGE_SIZE);

      if (vmm_map_page(vaddr + off, phys, pte_flags) < 0) {
        pmm_free_page(phys);
        return -1;
      }
    }

    vm_area_t *vma = kmalloc(sizeof(vm_area_t), GFP_ZERO);
    if (!vma)
      return -1;

    vma->base = vaddr;
    vma->size = size;
    vma->flags = vm_flags;
    vma->type = VMA_ANONYMOUS;
    vm_insert_area(current->vm_map, vma);
    return (long)vaddr;
  }

  if (fd < 0)
    return -1;

  fd_entry_t *fd_entry = task_get_fd(current, fd);
  VFS_File *file = fd_entry->data;
  if (!file)
    return -1;
  VFS_device_reg *dev = (VFS_device_reg *)(file->node->fs_node);

  if (!dev) {
    return -1;
  }
  if (dev->mmap) {

    uint64_t phys_base = dev->mmap(offset, size);
    if (!phys_base)
      return -1;

    pte_flags |= PTE_PCD | PTE_PWT;

    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
      if (vmm_map_page(vaddr + off, phys_base + off, pte_flags) < 0)
        return -1;
    }

    vm_area_t *vma = kmalloc(sizeof(vm_area_t), GFP_ZERO);
    vma->base = vaddr;
    vma->size = size;
    vma->flags = vm_flags;
    vma->type = VMA_DEVICE;
    vma->phys_base = phys_base;
    vm_insert_area(current->vm_map, vma);
    return (long)vaddr;
  }
}
