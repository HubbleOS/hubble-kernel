/**
 * @file initramfs.c
 * @brief initramfs VFS implementation — CPIO newc parser
 *
 * Parses a CPIO newc archive and exposes its contents through VFS.
 * Files are stored in an in-memory tree structure.
 *
 * CPIO newc format:
 *   - 110-byte ASCII header per entry
 *   - File data follows header
 *   - Entries are 4-byte aligned
 *   - "TRAILER!!!" marks end of archive
 */

#include "initramfs.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

#include <fs/vfs/vfs.h>

/* -- CPIO newc header -------------------------------------------------- */

#define CPIO_HEADER_SIZE 110
#define CPIO_MAGIC "070701"
#define CPIO_TRAILER "TRAILER!!!"

/* Offsets within the 110-byte ASCII header (all hex ASCII) */
#define CPIO_OFFSET_MODE 14
#define CPIO_OFFSET_SIZE 54
#define CPIO_OFFSET_NAMESIZE 94
#define CPIO_OFFSET_CHECKSUM 102

/* -- In-memory filesystem tree ----------------------------------------- */

#define MAX_NAME_LEN 256
#define MAX_CHILDREN 64

typedef struct initramfs_node {
  char name[MAX_NAME_LEN];
  uint32_t mode;
  uint8_t *data;
  uint32_t size;
  struct initramfs_node *children[MAX_CHILDREN];
  int child_count;
  struct initramfs_node *parent;
} initramfs_node_t;

static initramfs_node_t *g_root = NULL;
static void *g_archive_data = NULL;
static uint64_t g_archive_size = 0;

/* -- CPIO parsing helpers ---------------------------------------------- */

/**
 * @brief Parse a hex ASCII field from the CPIO header
 */
static uint32_t cpio_hex_field(const char *header, int offset, int width) {
  uint32_t val = 0;
  for (int i = 0; i < width; i++) {
    char c = header[offset + i];
    val <<= 4;
    if (c >= '0' && c <= '9')
      val |= (c - '0');
    else if (c >= 'a' && c <= 'f')
      val |= (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
      val |= (c - 'A' + 10);
  }
  return val;
}

/**
 * @brief Align a value up to 4-byte boundary
 */
static uint64_t cpio_align4(uint64_t val) {
  return (val + 3) & ~3ULL;
}

/* -- Tree manipulation ------------------------------------------------- */

/**
 * @brief Create a new filesystem node
 */
static initramfs_node_t *create_node(const char *name, uint32_t mode,
                                     initramfs_node_t *parent) {
  initramfs_node_t *node = kmalloc(sizeof(initramfs_node_t), GFP_KERNEL);
  if (!node)
    return NULL;
  memset(node, 0, sizeof(initramfs_node_t));

  strncpy(node->name, name, MAX_NAME_LEN - 1);
  node->name[MAX_NAME_LEN - 1] = '\0';
  node->mode = mode;
  node->parent = parent;

  if (parent && parent->child_count < MAX_CHILDREN) {
    parent->children[parent->child_count++] = node;
  }

  return node;
}

/**
 * @brief Find a child node by name
 */
static initramfs_node_t *find_child(initramfs_node_t *parent,
                                    const char *name) {
  for (int i = 0; i < parent->child_count; i++) {
    if (strcmp(parent->children[i]->name, name) == 0)
      return parent->children[i];
  }
  return NULL;
}

/**
 * @brief Navigate to or create a path in the tree
 */
static initramfs_node_t *resolve_path(const char *path, uint32_t mode,
                                      int create) {
  if (!g_root)
    return NULL;

  initramfs_node_t *current = g_root;
  const char *p = path;

  /* Skip leading slash */
  while (*p == '/')
    p++;

  while (*p != '\0') {
    /* Extract next component */
    const char *start = p;
    while (*p != '\0' && *p != '/')
      p++;
    size_t len = p - start;
    if (len == 0) {
      p++;
      continue;
    }

    char component[MAX_NAME_LEN];
    if (len >= MAX_NAME_LEN)
      len = MAX_NAME_LEN - 1;
    memcpy(component, start, len);
    component[len] = '\0';

    initramfs_node_t *child = find_child(current, component);
    if (!child && create) {
      child = create_node(component, mode, current);
      if (!child)
        return NULL;
    }
    if (!child)
      return NULL;

    current = child;
    while (*p == '/')
      p++;
  }

  return current;
}

/* -- CPIO Archive Parsing ---------------------------------------------- */

/**
 * @brief Parse the CPIO newc archive and build the in-memory tree
 */
static int parse_cpio(void *data, uint64_t size) {
  if (size < CPIO_HEADER_SIZE) {
    printk(KERN_ERR "[initramfs] archive too small\n");
    return -1;
  }

  uint8_t *base = (uint8_t *)data;
  uint64_t offset = 0;

  while (offset + CPIO_HEADER_SIZE <= size) {
    const char *header = (const char *)(base + offset);

    /* Validate magic */
    if (memcmp(header, CPIO_MAGIC, 6) != 0) {
      printk(KERN_ERR "[initramfs] invalid CPIO magic at offset %llu\n",
             offset);
      return -1;
    }

    uint32_t namesize = cpio_hex_field(header, CPIO_OFFSET_NAMESIZE, 8);
    uint32_t filesize = cpio_hex_field(header, CPIO_OFFSET_SIZE, 8);
    uint32_t mode = cpio_hex_field(header, CPIO_OFFSET_MODE, 8);

    /* Validate sizes */
    if (namesize == 0 || namesize > 4096) {
      printk(KERN_ERR "[initramfs] invalid namesize %u at offset %llu\n",
             namesize, offset);
      return -1;
    }

    if (offset + CPIO_HEADER_SIZE + cpio_align4(namesize) +
            cpio_align4(filesize) >
        size) {
      printk(KERN_ERR "[initramfs] entry exceeds archive bounds at offset %llu\n",
             offset);
      return -1;
    }

    /* Get filename (null-terminated after the namesize bytes) */
    const char *name = header + CPIO_HEADER_SIZE;

    /* Check for end-of-archive trailer */
    if (namesize >= 11 && memcmp(name, CPIO_TRAILER, 10) == 0) {
      printk(KERN_INFO "[initramfs] end of archive at offset %llu\n", offset);
      break;
    }

    /* File data follows the name (aligned to 4 bytes) */
    uint64_t data_offset = offset + CPIO_HEADER_SIZE + cpio_align4(namesize);
    uint8_t *file_data = base + data_offset;

    /* Strip trailing slash from name for directory entries */
    char clean_name[MAX_NAME_LEN];
    size_t name_len = namesize > 0 ? namesize - 1 : 0;
    if (name_len >= MAX_NAME_LEN)
      name_len = MAX_NAME_LEN - 1;
    memcpy(clean_name, name, name_len);
    clean_name[name_len] = '\0';

    /* Remove trailing slash */
    if (name_len > 0 && clean_name[name_len - 1] == '/')
      clean_name[name_len - 1] = '\0';

    /* Skip empty names */
    if (clean_name[0] == '\0') {
      offset = data_offset + cpio_align4(filesize);
      continue;
    }

    /* Determine if this is a directory (mode has S_IFDIR bit set) */
    int is_dir = (mode & 0170000) == 0040000;

    /* Resolve or create parent path */
    initramfs_node_t *node = resolve_path(clean_name, mode, 1);
    if (node) {
      node->mode = mode;
      if (!is_dir && filesize > 0) {
        /* Allocate and copy file data */
        node->data = kmalloc(filesize, GFP_KERNEL);
        if (node->data) {
          memcpy(node->data, file_data, filesize);
          node->size = filesize;
        }
      }
    }

    printk(KERN_DEBUG "[initramfs] %s: %s (size=%u, mode=0%o)\n",
           is_dir ? "dir " : "file", clean_name, filesize, mode & 0777);

    offset = data_offset + cpio_align4(filesize);
  }

  return 0;
}

/* -- VFS Integration --------------------------------------------------- */

/* Forward declarations for VFS callbacks */
static bool initramfs_mount(VFS_FS *fs, VFS_Device *device, uint32_t start_lba);
static void initramfs_unmount(VFS_FS *fs);
static VFS_Node *initramfs_open(VFS_FS *fs, const char *path);
static int initramfs_read(VFS_File *file, void *buf, uint32_t size);
static int initramfs_close(VFS_File *file);
static Directory initramfs_readdir(VFS_FS *fs, const char *path);

/**
 * @brief Initialize the initramfs VFS filesystem
 */
void initramfs_init_vfs(VFS_FS *fs) {
  fs->mount = initramfs_mount;
  fs->unmount = initramfs_unmount;
  fs->open = initramfs_open;
  fs->read = initramfs_read;
  fs->write = NULL;
  fs->mkdir = NULL;
  fs->unlink = NULL;
  fs->close = initramfs_close;
  fs->readdir = initramfs_readdir;
  fs->create_file = NULL;
  fs->mmap = NULL;
}

/**
 * @brief VFS mount callback for initramfs
 */
static bool initramfs_mount(VFS_FS *fs, VFS_Device *device,
                            uint32_t start_lba) {
  (void)device;
  (void)start_lba;
  /* The archive is already parsed; this just links the VFS */
  if (!g_root) {
    printk(KERN_ERR "[initramfs] no archive loaded\n");
    return false;
  }
  fs->fs = g_root;
  return true;
}

/**
 * @brief VFS unmount callback (no-op)
 */
static void initramfs_unmount(VFS_FS *fs) { (void)fs; }

/**
 * @brief Find a node by VFS path
 */
static initramfs_node_t *find_node_by_path(const char *path) {
  if (!g_root)
    return NULL;

  /* Root path */
  if (strcmp(path, "/") == 0 || path[0] == '\0')
    return g_root;

  return resolve_path(path, 0, 0);
}

/**
 * @brief Get file mode type bits for VFS
 */
static uint32_t get_vfs_mode(initramfs_node_t *node) {
  if ((node->mode & 0170000) == 0040000)
    return MODE_DIR;
  return MODE_FILE;
}

/**
 * @brief VFS open callback
 */
static VFS_Node *initramfs_open(VFS_FS *fs, const char *path) {
  (void)fs;
  initramfs_node_t *node = find_node_by_path(path);
  if (!node)
    return NULL;

  VFS_Node *vfs_node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
  if (!vfs_node)
    return NULL;

  strncpy(vfs_node->name, node->name, 255);
  vfs_node->name[255] = '\0';
  vfs_node->is_dir = ((node->mode & 0170000) == 0040000);
  vfs_node->size = node->size;
  vfs_node->mode = get_vfs_mode(node);
  vfs_node->pos = 0;
  vfs_node->fs_node = node;
  vfs_node->fs = fs;

  return vfs_node;
}

/**
 * @brief VFS read callback
 */
static int initramfs_read(VFS_File *file, void *buf, uint32_t size) {
  if (!file || !file->node || !file->node->fs_node)
    return -1;

  initramfs_node_t *node = (initramfs_node_t *)file->node->fs_node;
  if (!node->data || node->size == 0)
    return 0;

  if (file->pos >= node->size)
    return 0;

  uint32_t available = node->size - file->pos;
  uint32_t to_read = size < available ? size : available;

  memcpy(buf, node->data + file->pos, to_read);
  file->pos += to_read;

  return to_read;
}

/**
 * @brief VFS close callback
 */
static int initramfs_close(VFS_File *file) {
  if (file && file->node) {
    kfree(file->node);
    file->node = NULL;
  }
  return 0;
}

/**
 * @brief VFS readdir callback
 */
static Directory initramfs_readdir(VFS_FS *fs, const char *path) {
  (void)fs;
  Directory dir = Directory_init(dir);

  initramfs_node_t *node = find_node_by_path(path);
  if (!node || (node->mode & 0170000) != 0040000)
    return dir;

  dir.entries = kmalloc(node->child_count * sizeof(Entry), GFP_KERNEL);
  if (!dir.entries)
    return dir;

  dir.count = node->child_count;
  for (int i = 0; i < node->child_count; i++) {
    dir.entries[i].name = kmalloc(strlen(node->children[i]->name) + 1,
                                  GFP_KERNEL);
    if (dir.entries[i].name) {
      strcpy(dir.entries[i].name, node->children[i]->name);
    }
    dir.entries[i].is_dir =
        ((node->children[i]->mode & 0170000) == 0040000);
    dir.entries[i].cluster = 0;
  }

  return dir;
}

/* -- Public API -------------------------------------------------------- */

/**
 * @brief Initialize initramfs from CPIO data and mount at /
 */
int initramfs_init(void *data, uint64_t size) {
  if (!data || size == 0) {
    printk(KERN_WARNING "[initramfs] no data provided\n");
    return -1;
  }

  printk(KERN_INFO "[initramfs] parsing CPIO archive (%llu bytes)\n", size);

  g_archive_data = data;
  g_archive_size = size;

  /* Create root node */
  g_root = create_node("/", 0040000 | 0755, NULL);
  if (!g_root) {
    printk(KERN_ERR "[initramfs] failed to create root node\n");
    return -1;
  }

  /* Parse the archive */
  if (parse_cpio(data, size) < 0) {
    printk(KERN_ERR "[initramfs] CPIO parse failed\n");
    return -1;
  }

  printk(KERN_OK "[initramfs] mounted as /\n");
  return 0;
}
