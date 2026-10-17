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
Directory dev_vfs_list_devices(VFS_FS *fs, const char *path);

/* -- /dev/null and /dev/zero ------------------------------ */

static uint64_t null_read(uint64_t offset, size_t size, void *buf) {
  (void)offset, (void)size, (void)buf;
  return 0; /* always at end of file */
}

static uint64_t null_write(uint64_t offset, size_t size, const void *buf) {
  (void)offset, (void)buf;
  return size; /* everything goes, nowhere */
}

static uint64_t zero_read(uint64_t offset, size_t size, void *buf) {
  (void)offset;
  memset(buf, 0, size);
  return size;
}

/** @brief Initialise the device VFS instance. */
bool dev_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba) {
  printk(KERN_INFO "Initializing device fs\n");

  fs->create_file = dev_vfs_create_device;
  fs->open = dev_vfs_open_device;
  fs->write = dev_vfs_write_device;
  fs->read = dev_vfs_read_device;
  fs->fs = fs;
  fs->readdir = dev_vfs_list_devices;

  static bool basic_devices;
  if (!basic_devices) { /* once, however often /dev is mounted */
    basic_devices = true;
    dev_vfs_register("null", NULL, null_read, null_write);
    dev_vfs_register("zero", NULL, zero_read, null_write);
  }
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
  if (!dev || !dev->read)
    return 0;
  int ret = (int)dev->read(0, size, buf);
  file->pos = 0;
  return ret;
}

/** @brief Write to a device file. */
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size) {
  VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
  if (!dev || !dev->write)
    return 0;
  return (int)dev->write(0, size, buf);
}

/* Whether @p path names a directory: the root (""), or the first part of
 * some device name ("pts" for "pts/0"). */
static bool dev_vfs_is_dir(const char *path) {
  if (!*path)
    return true;
  size_t len = strlen(path);
  for (VFS_device_reg *dev = dev_vfs_devices; dev; dev = dev->next)
    if (strncmp(dev->name, path, len) == 0 && dev->name[len] == '/')
      return true;
  return false;
}

/** @brief Open a device file by path. */
VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path) {
  while (*path == '/')
    path++;
  /* Directories: the mountpoint itself (stat, ls probing /dev) and the
   * prefixes of device names, such as pts for the pty slaves. */
  if (dev_vfs_is_dir(path)) {
    VFS_Node *root = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
    if (!root)
      return NULL;
    memset(root, 0, sizeof(VFS_Node));
    root->fs = fs;
    root->is_dir = true;
    return root;
  }
  VFS_device_reg *dev = dev_vfs_find_device(fs, path);
  if (dev) {
    if (dev->open)
      dev->open();
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
VFS_device_reg *dev_vfs_register_file(const char *name,
                                      long (*open_file)(VFS_device_reg *dev,
                                                        int flags,
                                                        struct file **out),
                                      void *priv) {
  VFS_device_reg *dev = kzalloc(sizeof(VFS_device_reg));
  if (!dev)
    return NULL;
  strncpy(dev->name, name, sizeof(dev->name) - 1);
  dev->open_file = open_file;
  dev->priv = priv;
  /* Published last: lookups walk the list without a lock. */
  dev->next = dev_vfs_devices;
  __atomic_store_n(&dev_vfs_devices, dev, __ATOMIC_RELEASE);
  return dev;
}

/* File names point into the device registrations, which are never
 * freed; subdirectory names are the listing's own copies. */
static bool dev_vfs_free_entries(Directory *dir) {
  if (!dir)
    return false;
  for (int i = 0; i < dir->count; i++)
    if (dir->entries[i].is_dir) /* subdirectory names are copies */
      kfree(dir->entries[i].name);
  kfree(dir->entries);
  dir->entries = NULL;
  dir->count = 0;
  return true;
}

/** @brief List the registered devices (the root is the only directory). */
Directory dev_vfs_list_devices(VFS_FS *fs, const char *path) {
  (void)fs;
  Directory dir = {0};
  /* vfs_readdir passes "/" for the mount root (vfs_open passes ""). */
  while (path && *path == '/')
    path++;
  if (!path)
    path = "";
  size_t plen = strlen(path);
  if (plen && !dev_vfs_is_dir(path))
    return dir;

  int count = 0;
  for (VFS_device_reg *dev = dev_vfs_devices; dev; dev = dev->next)
    count++;
  if (!count)
    return dir;
  dir.entries = kmalloc(sizeof(Entry) * count, GFP_KERNEL);
  if (!dir.entries)
    return dir;
  dir.free_entries = dev_vfs_free_entries;

  /* The names one level below @p path: "pts/0" shows as "0" in pts, and
   * as the directory "pts" (once) at the root. Entries point into the
   * names, which live as long as the registrations. */
  for (VFS_device_reg *dev = dev_vfs_devices; dev; dev = dev->next) {
    const char *name = dev->name;
    if (plen) {
      if (strncmp(name, path, plen) != 0 || name[plen] != '/')
        continue;
      name += plen + 1;
    }
    const char *slash = strchr(name, '/');
    if (slash) {
      /* A subdirectory: list it once, under the first name in it. Its
       * entry text must end at the slash, which only a copy can do. */
      size_t n = (size_t)(slash - name);
      bool seen = false;
      for (int i = 0; i < dir.count && !seen; i++)
        seen = dir.entries[i].is_dir &&
               strncmp(dir.entries[i].name, name, n) == 0 &&
               dir.entries[i].name[n] == '\0';
      if (seen)
        continue;
      char *copy = kmalloc(n + 1, GFP_KERNEL);
      if (!copy)
        continue;
      memcpy(copy, name, n);
      copy[n] = '\0';
      dir.entries[dir.count].name = copy;
      dir.entries[dir.count].is_dir = true;
    } else {
      dir.entries[dir.count].name = (char *)name;
      dir.entries[dir.count].is_dir = false;
    }
    dir.entries[dir.count].cluster = 0;
    dir.count++;
  }
  return dir;
}

/** @brief MMAP a device file. */
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size) {
  VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
  if (dev && dev->mmap)
    return dev->mmap(offset, size);
  return 0;
}

bool dev_vfs_set_char_ops(const char *name, void (*open)(void),
                          long (*ioctl)(unsigned long request, void *arg),
                          bool (*readable)(void), struct wait_queue *read_wq) {
  VFS_device_reg *dev = dev_vfs_find_device(NULL, name);
  if (!dev)
    return false;
  dev->open = open;
  dev->ioctl = ioctl;
  dev->readable = readable;
  dev->read_wq = read_wq;
  return true;
}
