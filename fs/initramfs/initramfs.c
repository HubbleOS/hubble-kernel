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
#define MAX_SYMLINK_DEPTH 8

#define S_IFMT_ 0170000
#define S_IFDIR_ 0040000
#define S_IFLNK_ 0120000

typedef struct initramfs_node {
  char name[MAX_NAME_LEN];
  uint32_t mode;
  uint8_t *data; /* file contents, or the target path of a symlink */
  uint32_t size;
  struct initramfs_node **children;
  int child_count;
  int child_cap;
  struct initramfs_node *parent;
} initramfs_node_t;

static bool node_is_dir(const initramfs_node_t *n) {
  return (n->mode & S_IFMT_) == S_IFDIR_;
}

static bool node_is_symlink(const initramfs_node_t *n) {
  return (n->mode & S_IFMT_) == S_IFLNK_;
}

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
static uint64_t cpio_align4(uint64_t val) { return (val + 3) & ~3ULL; }

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

  if (parent) {
    if (parent->child_count == parent->child_cap) {
      int cap = parent->child_cap ? parent->child_cap * 2 : 8;
      initramfs_node_t **grown = krealloc(
          parent->children, cap * sizeof(*grown), GFP_KERNEL);
      if (!grown) {
        kfree(node);
        return NULL;
      }
      parent->children = grown;
      parent->child_cap = cap;
    }
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
 * @brief Copy the next '/'-separated component of *p into out
 * @return Component length, 0 at end of path, -1 if it is too long
 */
static int next_component(const char **p, char out[MAX_NAME_LEN]) {
  while (**p == '/')
    (*p)++;
  const char *start = *p;
  while (**p != '\0' && **p != '/')
    (*p)++;

  size_t len = *p - start;
  if (len >= MAX_NAME_LEN)
    return -1;
  memcpy(out, start, len);
  out[len] = '\0';
  return (int)len;
}

/**
 * @brief Find or create the node for an archive path
 *
 * Archive names are taken literally (no symlink following). Missing
 * parents are created as directories, since an archive may list a file
 * before its directory; the entry's own mode is applied by the caller.
 */
static initramfs_node_t *create_path(const char *path) {
  initramfs_node_t *cur = g_root;
  char comp[MAX_NAME_LEN];
  int len;

  while ((len = next_component(&path, comp)) > 0) {
    if (strcmp(comp, ".") == 0)
      continue;
    if (strcmp(comp, "..") == 0) {
      if (cur->parent)
        cur = cur->parent;
      continue;
    }

    initramfs_node_t *child = find_child(cur, comp);
    if (!child) {
      child = create_node(comp, S_IFDIR_ | 0755, cur);
      if (!child)
        return NULL;
    }
    cur = child;
  }
  return len < 0 ? NULL : cur;
}

/**
 * @brief Look up a path, following symlinks (including the last one)
 *
 * Relative paths and relative link targets resolve from @p start (a
 * link's own directory); absolute ones from the initramfs root, which
 * is mounted at "/".
 */
static initramfs_node_t *lookup(initramfs_node_t *start, const char *path,
                                int depth) {
  if (depth > MAX_SYMLINK_DEPTH)
    return NULL;

  initramfs_node_t *cur = (*path == '/') ? g_root : start;
  char comp[MAX_NAME_LEN];
  int len;

  while ((len = next_component(&path, comp)) > 0) {
    if (strcmp(comp, ".") == 0)
      continue;
    if (strcmp(comp, "..") == 0) {
      if (cur->parent)
        cur = cur->parent;
      continue;
    }
    if (!node_is_dir(cur))
      return NULL;

    initramfs_node_t *child = find_child(cur, comp);
    if (!child)
      return NULL;

    if (node_is_symlink(child)) {
      if (!child->data || child->size == 0)
        return NULL;

      /* Continue with "<target>/<rest of path>". */
      size_t rest = strlen(path);
      char *next = kmalloc(child->size + 1 + rest + 1, GFP_KERNEL);
      if (!next)
        return NULL;
      memcpy(next, child->data, child->size);
      next[child->size] = '/';
      memcpy(next + child->size + 1, path, rest + 1);

      initramfs_node_t *found = lookup(cur, next, depth + 1);
      kfree(next);
      return found;
    }
    cur = child;
  }
  return len < 0 ? NULL : cur;
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

    /* newc pads header + name together (not the name alone) to 4 bytes,
     * so the file data starts 4-byte aligned in the archive. */
    uint64_t data_offset =
        offset + cpio_align4(CPIO_HEADER_SIZE + (uint64_t)namesize);

    if (data_offset + cpio_align4(filesize) > size) {
      printk(KERN_ERR
             "[initramfs] entry exceeds archive bounds at offset %llu\n",
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

    uint8_t *file_data = base + data_offset;

    /* namesize counts the terminating NUL; copy so it is guaranteed. */
    char *clean_name = kmalloc(namesize, GFP_KERNEL);
    if (!clean_name)
      return -1;
    memcpy(clean_name, name, namesize - 1);
    clean_name[namesize - 1] = '\0';

    initramfs_node_t *node = create_path(clean_name);
    if (!node) {
      printk(KERN_ERR "[initramfs] cannot add '%s', skipping\n", clean_name);
    } else if (node != g_root) {
      node->mode = mode;
      if (!node_is_dir(node) && filesize > 0) {
        node->data = kmalloc(filesize, GFP_KERNEL);
        if (node->data) {
          memcpy(node->data, file_data, filesize);
          node->size = filesize;
        } else {
          printk(KERN_ERR "[initramfs] out of memory for '%s'\n",
                 clean_name);
        }
      }
    }
    kfree(clean_name);

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

  return lookup(g_root, path, 0);
}

/**
 * @brief Get file mode type bits for VFS
 */
static uint32_t get_vfs_mode(initramfs_node_t *node) {
  if (node_is_dir(node))
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
  vfs_node->is_dir = node_is_dir(node);
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
  Directory dir = Directory_init((Directory){.entries = NULL});

  initramfs_node_t *node = find_node_by_path(path);
  if (!node || !node_is_dir(node) || node->child_count == 0)
    return dir;

  dir.entries = kmalloc(node->child_count * sizeof(Entry), GFP_KERNEL);
  if (!dir.entries)
    return dir;

  dir.count = node->child_count;
  for (int i = 0; i < node->child_count; i++) {
    dir.entries[i].name =
        kmalloc(strlen(node->children[i]->name) + 1, GFP_KERNEL);
    if (dir.entries[i].name) {
      strcpy(dir.entries[i].name, node->children[i]->name);
    }
    dir.entries[i].is_dir = node_is_dir(node->children[i]);
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
