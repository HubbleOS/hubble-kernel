// mm/vm_map.h
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <smp/spinlock.h>

typedef enum
{
	VMA_ANONYMOUS,
	VMA_FILE,
	VMA_DEVICE,
} vma_type_t;

typedef struct vm_area
{
	uint64_t base;
	size_t size;
	uint32_t flags; // VM_READ | VM_WRITE | VM_EXEC

	vma_type_t type;
	uint64_t phys_base; // VMA_DEVICE only

	struct vm_area *next;
	struct vm_area *prev;
} vm_area_t;

typedef struct
{
	vm_area_t *areas;
	uint64_t mmap_base;
	spinlock_t lock;
} vm_map_t;

#define VM_READ (1 << 0)
#define VM_WRITE (1 << 1)
#define VM_EXEC (1 << 2)

#define MMAP_BASE 0x0000700000000000ULL

vm_map_t *vm_map_create(void);
void vm_insert_area(vm_map_t *map, vm_area_t *vma);
void vm_remove_area(vm_map_t *map, vm_area_t *vma);
vm_area_t *vm_find_area(vm_map_t *map, uint64_t addr);
uint64_t vm_find_free_range(vm_map_t *map, size_t size);
