/**
 * @file vm_map.c
 * @brief Virtual Memory Area map implementation
 *
 * Manages a sorted linked list of VMAs representing a process address space.
 */

#include <mm/kmalloc.h>
#include <mm/map/vm_map.h>
#include <mm/pmm.h>

/* -- Map Lifecycle --------------------------------------------------------- */

/**
 * @brief Create a new virtual memory map
 *
 * @return Pointer to the new map, or NULL on failure
 */
vm_map_t *vm_map_create(void) {
  vm_map_t *map = kmalloc(sizeof(vm_map_t), GFP_ZERO);
  map->areas = NULL;
  map->mmap_base = MMAP_BASE;
  return map;
}

/* -- VMA Insertion --------------------------------------------------------- */

/**
 * @brief Insert a VMA into the map, sorted by base address
 *
 * @param map Target map
 * @param vma VMA to insert
 */
void vm_insert_area(vm_map_t *map, vm_area_t *vma) {
  vm_area_t **cur = &map->areas;
  while (*cur && (*cur)->base < vma->base)
    cur = &(*cur)->next;

  vma->next = *cur;
  vma->prev = (*cur) ? (*cur)->prev : NULL;
  if (*cur)
    (*cur)->prev = vma;
  *cur = vma;
}

/* -- VMA Removal ----------------------------------------------------------- */

/**
 * @brief Remove a VMA from the map
 *
 * @param map Target map
 * @param vma VMA to remove
 */
void vm_remove_area(vm_map_t *map, vm_area_t *vma) {
  if (vma->prev)
    vma->prev->next = vma->next;
  else
    map->areas = vma->next;
  if (vma->next)
    vma->next->prev = vma->prev;
}

/* -- VMA Lookup ------------------------------------------------------------ */

/**
 * @brief Find the VMA containing a given address
 *
 * @param map Map to search
 * @param addr Virtual address
 * @return VMA containing the address, or NULL
 */
vm_area_t *vm_find_area(vm_map_t *map, uint64_t addr) {
  vm_area_t *vma = map->areas;
  while (vma) {
    if (addr >= vma->base && addr < vma->base + vma->size)
      return vma;
    vma = vma->next;
  }
  return NULL;
}

/* -- Free Range Search ----------------------------------------------------- */

/**
 * @brief Find a free address range of the given size
 *
 * Walks the VMA list and finds a gap large enough to fit the requested size.
 *
 * @param map Map to search
 * @param size Required size in bytes
 * @return Base address of the free range
 */
uint64_t vm_find_free_range(vm_map_t *map, size_t size) {
  uint64_t addr = map->mmap_base;
  vm_area_t *vma = map->areas;

  while (vma && (vma->base + vma->size) <= addr)
    vma = vma->next;

  while (vma) {
    if (addr + size <= vma->base)
      return addr;
    addr = PAGE_ALIGN_UP(vma->base + vma->size);
    vma = vma->next;
  }
  return addr;
}
