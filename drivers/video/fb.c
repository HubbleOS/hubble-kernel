/**
 * @file fb.c
 * @brief Framebuffer VFS device — exposes linear framebuffer to user-space
 */
#include <fs/vfs/dev.h>
#include <hubble/module.h>
#include <hubble/platform.h>

static uint64_t fb_mmap(uint64_t offset, size_t size) {
  (void)offset;
  (void)size;
  return g_platform.fb_base;
}

static int fb_init(void) {
  dev_vfs_register("fb0", fb_mmap, NULL, NULL);
  return 0;
}

module_init(fb_init);
MODULE_NAME("fb");
