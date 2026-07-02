/* -- VFS device filesystem implementation -------------------------
 * Provides a virtual filesystem for device files, allowing
 * registered hardware devices to be accessed via standard VFS
 * open/read/write operations.
 * ------------------------------------------------------------------ */

#include "dev.h"
#include "higher_half.h"
#include "vfs.h"
#include "vfs_standart_struct.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <stdbool.h>

static VFS_device_reg *dev_vfs_devices = NULL;

VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path);
VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path);
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size);
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size);
int dev_vfs_read_device(VFS_File *file, void *buf, uint32_t size);

/** @brief Initialise the device VFS instance. */
bool dev_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba) {
  printk(KERN_INFO "Initializing device fs\n");

  fs->create_file = dev_vfs_create_device;
  fs->open = dev_vfs_open_device;
  fs->write = dev_vfs_write_device;
  fs->read = dev_vfs_read_device;
  fs->fs = fs;
  return 1;
}

/** @brief Find a registered device by name. */
VFS_device_reg *dev_vfs_find_device(VFS_FS *fs, const char *path) {
  VFS_device_reg *dev = dev_vfs_devices;
  while (dev) {
    if (strcmp(dev->name, path) == 0)
      return dev;
    dev = dev->next;
  }
  return NULL;
}

/** @brief Read from a device file. */
int dev_vfs_read_device(VFS_File *file, void *buf, uint32_t size) {
  VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
  if (!dev->read)
    return 0;
  int ret = (int)dev->read(0, size, buf);
  file->pos = 0;
  return ret;
}

/** @brief Write to a device file. */
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size) {
  VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
  if (!dev->write)
    return 0;
  return (int)dev->write(0, size, buf);
}

/** @brief Open a device file by path. */
VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path) {
  VFS_device_reg *dev = dev_vfs_find_device(fs, path);
  if (dev) {
    VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
    memset(node, 0, sizeof(VFS_Node));
    strncpy(node->name, path, 255);
    node->fs = fs;
    node->fs_node = dev;
    node->size = 1;
    return node;
  }
  return NULL;
}

/** @brief Create a new device file node. */
VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path) {
  VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
  memset(node, 0, sizeof(VFS_Node));
  strncpy(node->name, path, 255);
  node->fs = fs;

  VFS_device_reg *dev = kmalloc(sizeof(VFS_device_reg), GFP_KERNEL);
  memset(dev, 0, sizeof(VFS_device_reg));
  strncpy(dev->name, path, 31);

  dev->mmap = NULL;
  dev->read = NULL;
  dev->write = NULL;
  dev->next = dev_vfs_devices;
  dev_vfs_devices = dev;

  node->fs_node = dev;
  return node;
}

/** @brief Register a new device with the VFS. */
void dev_vfs_register(const char *name, uint64_t (*mmap)(uint64_t, size_t),
                      uint64_t (*read)(uint64_t, size_t, void *),
                      uint64_t (*write)(uint64_t, size_t, const void *)) {
  VFS_device_reg *dev = kmalloc(sizeof(VFS_device_reg), GFP_KERNEL);
  memset(dev, 0, sizeof(VFS_device_reg));
  strncpy(dev->name, name, 31);
  dev->mmap = mmap;
  dev->read = read;
  dev->write = write;
  dev->next = dev_vfs_devices;
  dev_vfs_devices = dev;
}

/** @brief MMAP a device file. */
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size) {
  VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
  if (dev->mmap)
    return dev->mmap(offset, size);
  return 0;
}
