// mm/vm_map.c

#include <mm/map/vm_map.h>
#include <mm/kmalloc.h>
#include <higher_half.h>
#include <mm/pmm.h>

vm_map_t *vm_map_create(void)
{
	vm_map_t *map = kmalloc(sizeof(vm_map_t), GFP_ZERO);
	map->areas = NULL;
	map->mmap_base = MMAP_BASE;
	return map;
}

void vm_insert_area(vm_map_t *map, vm_area_t *vma)
{
	// Insert sorted by base address
	vm_area_t **cur = &map->areas;
	while (*cur && (*cur)->base < vma->base)
		cur = &(*cur)->next;

	vma->next = *cur;
	vma->prev = (*cur) ? (*cur)->prev : NULL;
	if (*cur)
		(*cur)->prev = vma;
	*cur = vma;
}

vm_area_t *vm_find_area(vm_map_t *map, uint64_t addr)
{
	vm_area_t *vma = map->areas;
	while (vma)
	{
		if (addr >= vma->base && addr < vma->base + vma->size)
			return vma;
		vma = vma->next;
	}
	return NULL;
}

void vm_remove_area(vm_map_t *map, vm_area_t *vma)
{
	if (vma->prev)
		vma->prev->next = vma->next;
	else
		map->areas = vma->next;
	if (vma->next)
		vma->next->prev = vma->prev;
}

uint64_t vm_find_free_range(vm_map_t *map, size_t size)
{
	uint64_t addr = map->mmap_base;
	vm_area_t *vma = map->areas;

	// Skip VMAs that are entirely below our starting address
	while (vma && (vma->base + vma->size) <= addr)
		vma = vma->next;

	while (vma)
	{
		if (addr + size <= vma->base)
			return addr;
		addr = PAGE_ALIGN_UP(vma->base + vma->size);
		vma = vma->next;
	}
	return addr;
}
