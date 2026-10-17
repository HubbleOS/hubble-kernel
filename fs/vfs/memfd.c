/**
 * @file memfd.c
 * @brief memfd_create(2): anonymous memory behind a file descriptor
 *
 * The file owns an array of physical pages, sized with ftruncate. Every
 * mmap(MAP_SHARED) of it maps those same pages, so processes that pass the
 * descriptor to each other (fork, or SCM_RIGHTS over a socket) share the
 * memory - a window's pixels between a client and the compositor.
 *
 * Pages are reference counted (pmm): the memfd holds one reference to each
 * page it owns, every mapping holds its own. Shrinking or closing drops
 * the memfd's references only, so live mappings keep their pages.
 */
#include "file.h"
#include <higher_half.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/spinlock.h>

/* Far beyond any window buffer; keeps the page array bounded. */
#define MEMFD_MAX_SIZE (1ULL << 30)

typedef struct {
  spinlock_t lock;
  uint64_t size;   /* bytes, as ftruncate set it */
  uint64_t *pages; /* physical addresses, one per page of size */
  size_t npages;
} memfd_t;

static size_t pages_for(uint64_t size) {
  return (size_t)(PAGE_ALIGN_UP(size) / PAGE_SIZE);
}

/* Grow to @p want pages: new pages are zeroed. Allocates without the lock
 * (the allocator may take its own), then installs under it. */
static long memfd_grow(memfd_t *m, uint64_t length) {
  size_t want = pages_for(length);
  uint64_t *pages = kmalloc(want * sizeof(uint64_t), GFP_ZERO);
  if (!pages)
    return -ENOMEM;

  spinlock_acquire(&m->lock);
  size_t have = m->npages;
  spinlock_release(&m->lock);

  for (size_t i = have; i < want; i++) {
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
      for (size_t j = have; j < i; j++)
        pmm_free_page(pages[j]);
      kfree(pages);
      return -ENOMEM;
    }
    memset((void *)phys_to_virt(phys), 0, PAGE_SIZE);
    pages[i] = phys;
  }

  spinlock_acquire(&m->lock);
  if (m->npages != have) { /* resized meanwhile: let the caller retry */
    spinlock_release(&m->lock);
    for (size_t i = have; i < want; i++)
      pmm_free_page(pages[i]);
    kfree(pages);
    return -EAGAIN;
  }
  memcpy(pages, m->pages, have * sizeof(uint64_t));
  uint64_t *old = m->pages;
  m->pages = pages;
  m->npages = want;
  m->size = length;
  spinlock_release(&m->lock);

  kfree(old);
  return 0;
}

static long memfd_truncate(file_t *file, uint64_t length) {
  memfd_t *m = file->priv;
  if (length > MEMFD_MAX_SIZE)
    return -EFBIG;

  for (;;) {
    spinlock_acquire(&m->lock);
    size_t want = pages_for(length);
    if (want <= m->npages) {
      /* Shrink (or same page count): drop the memfd's references to
       * the pages past the end; mappings keep theirs. The tail of the
       * last page is cleared, as the bytes past EOF read as zero. */
      uint64_t dropped[16];
      size_t ndropped = 0;
      while (m->npages > want && ndropped < 16)
        dropped[ndropped++] = m->pages[--m->npages];
      bool done = m->npages == want;
      if (done) {
        if (length % PAGE_SIZE && want)
          memset((uint8_t *)phys_to_virt(m->pages[want - 1]) +
                     length % PAGE_SIZE,
                 0, PAGE_SIZE - length % PAGE_SIZE);
        m->size = length;
      }
      spinlock_release(&m->lock);
      for (size_t i = 0; i < ndropped; i++)
        pmm_free_page(dropped[i]);
      if (done)
        return 0;
      continue;
    }
    spinlock_release(&m->lock);

    long err = memfd_grow(m, length);
    if (err != -EAGAIN)
      return err;
  }
}

static uint64_t memfd_get_page(file_t *file, uint64_t offset) {
  memfd_t *m = file->priv;
  uint64_t phys = 0;
  spinlock_acquire(&m->lock);
  size_t i = (size_t)(offset / PAGE_SIZE);
  if (i < m->npages) {
    phys = m->pages[i];
    pmm_inc_refcount(phys); /* the caller's mapping */
  }
  spinlock_release(&m->lock);
  return phys;
}

static void memfd_stat(file_t *file, uint32_t *type, uint64_t *size) {
  *type = FILE_S_IFREG;
  *size = ((memfd_t *)file->priv)->size;
}

static void memfd_release(file_t *file) {
  memfd_t *m = file->priv;
  for (size_t i = 0; i < m->npages; i++)
    pmm_free_page(m->pages[i]);
  kfree(m->pages);
  kfree(m);
}

static const file_ops_t memfd_ops = {
    .truncate = memfd_truncate,
    .get_page = memfd_get_page,
    .stat = memfd_stat,
    .release = memfd_release,
};

file_t *memfd_create_file(void) {
  memfd_t *m = kzalloc(sizeof(*m));
  if (!m)
    return NULL;
  spinlock_init(&m->lock, "memfd");
  file_t *file = file_alloc(&memfd_ops, FILE_O_RDWR, m);
  if (!file)
    kfree(m);
  return file;
}
