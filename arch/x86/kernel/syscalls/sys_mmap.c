#include <stddef.h>
#include <string.h>

#include <sys/syscall.h>
#include "io.h"
#include "printk.h"
#include <mm/map/vm_map.h>
#include <mm/vmm.h>
#include <mm/map/vm_map.h>
#include <mm/kmalloc.h>
#include <smp/scheduler.h>
#include <mm/pmm.h>
#include <higher_half.h>

#include "syscall_entry.h"

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <fs/vfs/dev.h>

#define PROT_READ 0x1  /* Page can be read.  */
#define PROT_WRITE 0x2 /* Page can be written.  */
#define PROT_EXEC 0x4
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
#define MAP_FIXED 0x10

#define PT_LOAD 0x1
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

VFS_File *task_get_fd(task_t *task, int fd)
{
	if (fd < 0 || fd >= MAX_FDS)
		return NULL;
	return task->fds[fd];
}

long sys_mmap(uint64_t addr, size_t length, int prot, int flags,
	      int fd, uint64_t offset)
{
	if (length == 0)
	{
		return -1;
	}
	// printk("sys_mmap(%p, %lu, %x, %x, %d, %lu)\n", addr, length, prot, flags, fd, offset);

	task_t *current = get_current_task();
	if (!current->vm_map)
		current->vm_map = vm_map_create();
	if (!current->vm_map)
	{
		return -1;
	}

	size_t size = PAGE_ALIGN_UP(length);

	// Find or use requested virtual address
	uint64_t vaddr;
	if ((flags & MAP_FIXED) && addr)
		vaddr = addr;
	else
		vaddr = vm_find_free_range(current->vm_map, size);

	// Build VMA flags
	uint32_t vm_flags = 0;
	if (prot & PROT_READ)
		vm_flags |= VM_READ;
	if (prot & PROT_WRITE)
		vm_flags |= VM_WRITE;
	if (prot & PROT_EXEC)
		vm_flags |= VM_EXEC;

	// Build PTE flags
	uint64_t pte_flags = PTE_PRESENT | PTE_USER;
	if (prot & PROT_WRITE)
		pte_flags |= PTE_WRITE;
	if (!(prot & PROT_EXEC))
		pte_flags |= PTE_NX;

	if (flags & MAP_ANONYMOUS)
	{
		// Allocate and map physical pages
		for (uint64_t off = 0; off < size; off += PAGE_SIZE)
		{
			uint64_t phys = pmm_alloc_page();
			if (!phys)
				return -1;

			// Zero the page
			memset(PHYS_TO_VIRT_PTR(void, phys), 0, PAGE_SIZE);

			if (vmm_map_page(vaddr + off, phys, pte_flags) < 0)
			{
				pmm_free_page(phys);
				return -1;
			}
		}

		// Register VMA
		vm_area_t *vma = kmalloc(sizeof(vm_area_t), GFP_ZERO);
		if (!vma)
			return -1;

		vma->base = vaddr;
		vma->size = size;
		vma->flags = vm_flags;
		vma->type = VMA_ANONYMOUS;
		vm_insert_area(current->vm_map, vma);
		// printk("Mapped %p - %p, from physical %p\n", vaddr, vaddr + size, (void *)vaddr);
		return (long)vaddr;
	}

	if (fd < 0)
		return -1;

	VFS_File *file = task_get_fd(current, fd);
	if (!file)
		return -1;
	VFS_device_reg *dev = (VFS_device_reg *)(file->node->fs_node);

	if (!dev)
	{
		return -1;
	}
	if (dev->mmap)
	{

		uint64_t phys_base = dev->mmap(offset, size);
		if (!phys_base)
			return -1;

		// Device mappings are uncached
		pte_flags |= PTE_PCD | PTE_PWT;

		for (uint64_t off = 0; off < size; off += PAGE_SIZE)
		{
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
		// printk("Mapped %p - %p, from physical %p\n", vaddr, vaddr + size, (void *)phys_base);
		return (long)vaddr;
	}
}
