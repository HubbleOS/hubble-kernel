/* -- FAT32 filesystem driver ---------------------------------------
 * Boot sector and FAT, cluster I/O and allocation, directory walking,
 * path lookup, file data reads and writes, and creating and removing
 * entries. fat32.h lists the API; fat32_vfs.c hooks it into the VFS.
 * ------------------------------------------------------------------ */

#include "fat32.h"
#include "fat32_struct.h"

#include <fs/vfs/path.h>
#include <hubble/ctype.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

/* -- Cluster I/O ---------------------------------------------------- */

static bool fat32_valid_cluster(FAT32_FS *fs, uint32_t c) {
  return c >= 2 && c < fs->cluster_count + 2;
}

static uint32_t fat32_cluster_lba(FAT32_FS *fs, uint32_t c) {
  return fs->data_lba + (c - 2) * fs->sectors_per_cluster;
}

int fat32_read_block(FAT32_FS *fs, uint32_t block, void *buf) {
  if (!fat32_valid_cluster(fs, block))
    return -EIO;
  return vfs_dev_read(&fs->dev, fat32_cluster_lba(fs, block),
                      fs->sectors_per_cluster, buf) < 0
             ? -EIO
             : 0;
}

int fat32_write_block(FAT32_FS *fs, uint32_t block, const void *buf) {
  if (!fat32_valid_cluster(fs, block))
    return -EIO;
  return vfs_dev_write(&fs->dev, fat32_cluster_lba(fs, block),
                       fs->sectors_per_cluster, buf) < 0
             ? -EIO
             : 0;
}

/* -- The FAT -------------------------------------------------------- */

/* The cluster after @p c in its chain (>= FAT32_EOC at the end). */
static uint32_t fat32_next(FAT32_FS *fs, uint32_t c) {
  if (!fat32_valid_cluster(fs, c))
    return FAT32_EOC;
  return fs->fat[c] & 0x0FFFFFFF;
}

static void fat32_set_next(FAT32_FS *fs, uint32_t c, uint32_t next) {
  /* The top four bits are reserved and kept. */
  fs->fat[c] = (fs->fat[c] & 0xF0000000) | (next & 0x0FFFFFFF);

  uint32_t sector = c / (VFS_SECTOR_SIZE / 4);
  if (fs->dirty_lo >= fs->dirty_hi) {
    fs->dirty_lo = sector;
    fs->dirty_hi = sector + 1;
  } else if (sector < fs->dirty_lo) {
    fs->dirty_lo = sector;
  } else if (sector >= fs->dirty_hi) {
    fs->dirty_hi = sector + 1;
  }
}

uint32_t fat32_alloc_block(FAT32_FS *fs, uint32_t hint) {
  if (!fat32_valid_cluster(fs, hint))
    hint = 2;
  for (uint32_t n = 0; n < fs->cluster_count; n++) {
    uint32_t c = 2 + (hint - 2 + n) % fs->cluster_count;
    if ((fs->fat[c] & 0x0FFFFFFF) == 0) {
      fat32_set_next(fs, c, 0x0FFFFFFF);
      return c;
    }
  }
  return 0;
}

int fat32_free_block(FAT32_FS *fs, uint32_t block) {
  if (!fat32_valid_cluster(fs, block))
    return -EINVAL;
  fat32_set_next(fs, block, 0);
  return 0;
}

static void fat32_free_chain(FAT32_FS *fs, uint32_t c) {
  for (uint32_t n = 0; fat32_valid_cluster(fs, c) && n < fs->cluster_count;
       n++) {
    uint32_t next = fat32_next(fs, c);
    fat32_free_block(fs, c);
    c = next;
  }
}

/* A new cluster after @p last (0: a new chain), zeroed. */
static uint32_t fat32_extend(FAT32_FS *fs, uint32_t last, uint8_t *scratch) {
  uint32_t c = fat32_alloc_block(fs, last ? last + 1 : 2);
  if (!c)
    return 0;
  memset(scratch, 0, fs->cluster_size);
  if (fat32_write_block(fs, c, scratch) < 0) {
    fat32_free_block(fs, c);
    return 0;
  }
  if (last)
    fat32_set_next(fs, last, c);
  return c;
}

int fat32_sync(FAT32_FS *fs) {
  if (fs->dirty_lo >= fs->dirty_hi)
    return 0;
  const uint8_t *from = (const uint8_t *)fs->fat + fs->dirty_lo * VFS_SECTOR_SIZE;
  for (uint32_t i = 0; i < fs->num_fats; i++)
    if (vfs_dev_write(&fs->dev, fs->fat_lba + i * fs->fat_sectors + fs->dirty_lo,
                      fs->dirty_hi - fs->dirty_lo, from) < 0)
      return -EIO;
  fs->dirty_lo = fs->dirty_hi = 0;
  return 0;
}

/* -- Mount ------------------------------------------------------------ */

int fat32_mount(FAT32_FS *fs, const VFS_Device *dev, uint32_t first_lba) {
  memset(fs, 0, sizeof(*fs));
  fs->dev = *dev;
  fs->first_lba = first_lba;

  uint8_t sector[VFS_SECTOR_SIZE];
  if (vfs_dev_read(dev, first_lba, 1, sector) < 0)
    return -EIO;
  FAT32_BPB *bpb = (FAT32_BPB *)(sector + 0x0B);
  if (sector[510] != 0x55 || sector[511] != 0xAA ||
      memcmp(bpb->fs_type, "FAT32   ", 8) != 0) {
    printk(KERN_ERR "FAT32: no FAT32 boot sector at LBA %u\n", first_lba);
    return -EINVAL;
  }
  if (bpb->bytes_per_sector != VFS_SECTOR_SIZE ||
      bpb->sectors_per_cluster == 0 || bpb->num_fats == 0 ||
      bpb->fat_size_32 == 0) {
    printk(KERN_ERR "FAT32: unsupported geometry (%u-byte sectors)\n",
           bpb->bytes_per_sector);
    return -EINVAL;
  }

  uint32_t total = bpb->total_sectors_32 ? bpb->total_sectors_32
                                         : bpb->total_sectors_16;
  fs->sectors_per_cluster = bpb->sectors_per_cluster;
  fs->cluster_size = bpb->sectors_per_cluster * VFS_SECTOR_SIZE;
  fs->root_cluster = bpb->root_cluster;
  fs->num_fats = bpb->num_fats;
  fs->fat_sectors = bpb->fat_size_32;
  fs->fat_lba = first_lba + bpb->reserved_sector_count;
  fs->data_lba = fs->fat_lba + fs->num_fats * fs->fat_sectors;

  uint32_t data_sectors = total - (fs->data_lba - first_lba);
  fs->cluster_count = data_sectors / fs->sectors_per_cluster;
  uint32_t fat_entries = fs->fat_sectors * (VFS_SECTOR_SIZE / 4);
  if (fs->cluster_count + 2 > fat_entries)
    fs->cluster_count = fat_entries - 2;

  fs->fat = kmalloc(fs->fat_sectors * VFS_SECTOR_SIZE, GFP_KERNEL);
  if (!fs->fat)
    return -ENOMEM;
  if (vfs_dev_read(dev, fs->fat_lba, fs->fat_sectors, fs->fat) < 0) {
    kfree(fs->fat);
    fs->fat = NULL;
    return -EIO;
  }

  printk(KERN_INFO "FAT32: %u clusters of %u bytes, root at %u\n",
         fs->cluster_count, fs->cluster_size, fs->root_cluster);
  return 0;
}

void fat32_unmount(FAT32_FS *fs) {
  fat32_sync(fs);
  kfree(fs->fat);
  fs->fat = NULL;
}

/* -- Names ------------------------------------------------------------ */

/* @p name as an 8.3 short name: upper case, base and extension padded
 * with spaces. Longer parts are cut off. */
static void fat32_short_name(const char *name, uint8_t out[11]) {
  memset(out, ' ', 11);
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    memcpy(out, name, strlen(name));
    return;
  }
  const char *dot = NULL;
  for (const char *p = name; *p; p++)
    if (*p == '.')
      dot = p;
  if (dot == name)
    dot = NULL; /* ".profile": all base */

  int n = 0;
  for (const char *p = name; *p && p != dot && n < 8; p++)
    out[n++] = toupper(*p);
  n = 8;
  for (const char *p = dot ? dot + 1 : ""; *p && n < 11; p++)
    out[n++] = toupper(*p);
}

/* The readable form of a short name ("README  TXT" -> "README.TXT"). */
static void fat32_entry_name(const FAT32_DirectoryEntry *e, char out[13]) {
  int n = 0;
  for (int i = 0; i < 8 && e->name[i] != ' '; i++)
    out[n++] = e->name[i];
  if (e->name[8] != ' ') {
    out[n++] = '.';
    for (int i = 8; i < 11 && e->name[i] != ' '; i++)
      out[n++] = e->name[i];
  }
  out[n] = '\0';
}

/* A file or directory, not free, a long-name part or the volume label. */
static bool fat32_entry_live(const FAT32_DirectoryEntry *e) {
  return e->name[0] != FAT32_DIRENT_END && e->name[0] != FAT32_DIRENT_FREE &&
         (e->attr & FAT32_ATTR_LFN) != FAT32_ATTR_LFN &&
         !(e->attr & FAT32_ATTR_VOLUME_ID);
}

/* -- Directory walking ------------------------------------------------ */

/* Callback per directory entry (free ones too). Return FAT32_NEXT to go
 * on, FAT32_STOP to stop, or FAT32_STOP_WRITE to stop and write the
 * entry's cluster back (the callback changed the entry). */
enum { FAT32_NEXT, FAT32_STOP, FAT32_STOP_WRITE };
typedef int (*fat32_dirent_fn)(FAT32_DirectoryEntry *e, uint32_t cluster,
                               uint32_t index, void *ctx);

/* Visit the entries of the directory starting at @p cluster until the
 * callback stops or the end marker. @return 1 if the callback stopped,
 * 0 at the end, -errno; *last is set to the chain's last cluster. */
static int fat32_dir_foreach(FAT32_FS *fs, uint32_t cluster,
                             fat32_dirent_fn fn, void *ctx, uint32_t *last) {
  uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  uint32_t per_cluster = fs->cluster_size / sizeof(FAT32_DirectoryEntry);
  int result = 0;
  for (uint32_t n = 0; fat32_valid_cluster(fs, cluster) &&
                       n < fs->cluster_count && result == 0;
       n++) {
    if (last)
      *last = cluster;
    if (fat32_read_block(fs, cluster, buf) < 0) {
      result = -EIO;
      break;
    }
    FAT32_DirectoryEntry *e = (FAT32_DirectoryEntry *)buf;
    for (uint32_t i = 0; i < per_cluster; i++, e++) {
      bool end = e->name[0] == FAT32_DIRENT_END;
      int r = fn(e, cluster, i, ctx);
      if (r == FAT32_STOP_WRITE)
        result = fat32_write_block(fs, cluster, buf) < 0 ? -EIO : 1;
      else if (r == FAT32_STOP)
        result = 1;
      if (result || end)
        break;
    }
    if (result == 0 && e < (FAT32_DirectoryEntry *)buf + per_cluster)
      break; /* stopped at the end marker */
    cluster = fat32_next(fs, cluster);
  }
  kfree(buf);
  return result;
}

typedef struct {
  uint8_t name[11];
  FAT32_Node *out;
} fat32_find_ctx;

static int fat32_find_match(FAT32_DirectoryEntry *e, uint32_t cluster,
                            uint32_t index, void *ctx) {
  fat32_find_ctx *c = ctx;
  if (!fat32_entry_live(e) || memcmp(e->name, c->name, 11) != 0)
    return FAT32_NEXT;
  c->out->entry = *e;
  c->out->dir_cluster = cluster;
  c->out->index = index;
  c->out->is_root = false;
  return FAT32_STOP;
}

static void fat32_root_node(FAT32_FS *fs, FAT32_Node *out) {
  memset(out, 0, sizeof(*out));
  out->entry.attr = FAT32_ATTR_DIRECTORY;
  out->entry.first_cluster_high = fs->root_cluster >> 16;
  out->entry.first_cluster_low = fs->root_cluster & 0xFFFF;
  out->is_root = true;
}

/* Find @p name in directory @p dir, into @p out (may be @p dir). */
static int fat32_find(FAT32_FS *fs, const FAT32_Node *dir, const char *name,
                      FAT32_Node *out) {
  if (!fat32_is_dir(&dir->entry))
    return -ENOTDIR;
  if (strcmp(name, ".") == 0) {
    *out = *dir;
    return 0;
  }
  if (dir->is_root && strcmp(name, "..") == 0) {
    *out = *dir;
    return 0;
  }

  FAT32_Node found;
  fat32_find_ctx ctx = {.out = &found};
  fat32_short_name(name, ctx.name);
  int r = fat32_dir_foreach(fs, fat32_first_cluster(&dir->entry),
                            fat32_find_match, &ctx, NULL);
  if (r < 0)
    return r;
  if (r == 0)
    return -ENOENT;
  /* ".." holding cluster 0 means the root. */
  if (fat32_is_dir(&found.entry) && fat32_first_cluster(&found.entry) == 0)
    fat32_root_node(fs, out);
  else
    *out = found;
  return 0;
}

int fat32_lookup(FAT32_FS *fs, const char *path, FAT32_Node *out) {
  fat32_root_node(fs, out);
  char name[VFS_NAME_MAX];
  int len;
  while ((len = vfs_path_next(&path, name)) > 0) {
    int err = fat32_find(fs, out, name, out);
    if (err < 0)
      return err;
  }
  return len < 0 ? -ENAMETOOLONG : 0;
}

/* -- Listing ------------------------------------------------------------ */

typedef struct {
  Directory *dir;
  int cap;
} fat32_list_ctx;

static int fat32_list_add(FAT32_DirectoryEntry *e, uint32_t cluster,
                          uint32_t index, void *ctx) {
  (void)cluster;
  (void)index;
  fat32_list_ctx *c = ctx;
  if (!fat32_entry_live(e))
    return FAT32_NEXT;

  if (c->dir->count == c->cap) {
    int cap = c->cap ? c->cap * 2 : 16;
    Entry *grown = krealloc(c->dir->entries, cap * sizeof(Entry), GFP_KERNEL);
    if (!grown)
      return FAT32_STOP;
    c->dir->entries = grown;
    c->cap = cap;
  }

  char name[13];
  fat32_entry_name(e, name);
  char *copy = kmalloc(strlen(name) + 1, GFP_KERNEL);
  if (!copy)
    return FAT32_STOP;
  strcpy(copy, name);

  Entry *out = &c->dir->entries[c->dir->count++];
  out->name = copy;
  out->is_dir = fat32_is_dir(e);
  out->cluster = fat32_first_cluster(e);
  return FAT32_NEXT;
}

Directory fat32_readdir(FAT32_FS *fs, const FAT32_Node *dir) {
  Directory list = Directory_init((Directory){.entries = NULL, .count = 0});
  if (!fat32_is_dir(&dir->entry))
    return list;

  fat32_list_ctx ctx = {.dir = &list, .cap = 0};
  fat32_dir_foreach(fs, fat32_first_cluster(&dir->entry), fat32_list_add,
                    &ctx, NULL);
  return list;
}

/* -- File data ---------------------------------------------------------- */

/* Write @p node's directory entry back to its directory. */
static int fat32_update_entry(FAT32_FS *fs, const FAT32_Node *node) {
  if (node->is_root)
    return 0;
  uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;
  int err = fat32_read_block(fs, node->dir_cluster, buf);
  if (!err) {
    ((FAT32_DirectoryEntry *)buf)[node->index] = node->entry;
    err = fat32_write_block(fs, node->dir_cluster, buf);
  }
  kfree(buf);
  return err;
}

int fat32_read(FAT32_FS *fs, FAT32_Node *node, uint32_t pos, void *buf,
               uint32_t size) {
  FAT32_DirectoryEntry *e = &node->entry;
  if (fat32_is_dir(e))
    return -EISDIR;
  if (pos >= e->file_size)
    return 0;
  if (size > e->file_size - pos)
    size = e->file_size - pos;

  uint32_t cluster = fat32_first_cluster(e);
  for (uint32_t i = 0; i < pos / fs->cluster_size; i++)
    cluster = fat32_next(fs, cluster);

  uint8_t *cbuf = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!cbuf)
    return -ENOMEM;

  uint32_t done = 0, offset = pos % fs->cluster_size;
  while (done < size && fat32_read_block(fs, cluster, cbuf) == 0) {
    uint32_t chunk = fs->cluster_size - offset;
    if (chunk > size - done)
      chunk = size - done;
    memcpy((uint8_t *)buf + done, cbuf + offset, chunk);
    done += chunk;
    offset = 0;
    cluster = fat32_next(fs, cluster);
  }
  kfree(cbuf);
  return done ? (int)done : -EIO;
}

int fat32_write(FAT32_FS *fs, FAT32_Node *node, uint32_t pos,
                const void *buf, uint32_t size) {
  FAT32_DirectoryEntry *e = &node->entry;
  if (fat32_is_dir(e))
    return -EISDIR;
  if (size == 0)
    return 0;

  uint8_t *cbuf = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!cbuf)
    return -ENOMEM;

  /* Walk (and grow, with zeroed clusters) the chain up to pos. */
  int err = 0;
  uint32_t cluster = fat32_first_cluster(e), prev = 0;
  for (uint32_t i = 0; i <= pos / fs->cluster_size; i++) {
    if (i > 0) {
      prev = cluster;
      cluster = fat32_next(fs, cluster);
    }
    if (!fat32_valid_cluster(fs, cluster)) {
      cluster = fat32_extend(fs, prev, cbuf);
      if (!cluster) {
        err = -ENOSPC;
        break;
      }
      if (!prev) {
        e->first_cluster_high = cluster >> 16;
        e->first_cluster_low = cluster & 0xFFFF;
      }
    }
  }

  uint32_t done = 0, offset = pos % fs->cluster_size;
  while (!err && done < size) {
    uint32_t chunk = fs->cluster_size - offset;
    if (chunk > size - done)
      chunk = size - done;
    if (chunk < fs->cluster_size &&
        (err = fat32_read_block(fs, cluster, cbuf)) < 0)
      break;
    memcpy(cbuf + offset, (const uint8_t *)buf + done, chunk);
    if ((err = fat32_write_block(fs, cluster, cbuf)) < 0)
      break;
    done += chunk;
    offset = 0;

    if (done < size) {
      uint32_t next = fat32_next(fs, cluster);
      if (!fat32_valid_cluster(fs, next))
        next = fat32_extend(fs, cluster, cbuf);
      if (!next)
        err = -ENOSPC;
      cluster = next;
    }
  }
  kfree(cbuf);

  if (pos + done > e->file_size)
    e->file_size = pos + done;
  fat32_update_entry(fs, node);
  fat32_sync(fs);
  return done ? (int)done : err;
}

/* -- Creating and removing ----------------------------------------------- */

static int fat32_take_free(FAT32_DirectoryEntry *e, uint32_t cluster,
                           uint32_t index, void *ctx) {
  FAT32_Node *node = ctx;
  if (e->name[0] != FAT32_DIRENT_END && e->name[0] != FAT32_DIRENT_FREE)
    return FAT32_NEXT;
  /* Taking the end marker's slot is fine: the entries after it are zero,
   * so the next one becomes the end marker. */
  *e = node->entry;
  node->dir_cluster = cluster;
  node->index = index;
  return FAT32_STOP_WRITE;
}

/* Find @p path's parent directory and split off the last name. */
static int fat32_parent(FAT32_FS *fs, const char *path, FAT32_Node *parent,
                        char name[VFS_NAME_MAX]) {
  size_t dir_len;
  if (vfs_path_last(path, &dir_len, name) < 0)
    return -EINVAL;
  char *dir = kmalloc(dir_len + 1, GFP_KERNEL);
  if (!dir)
    return -ENOMEM;
  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';
  int err = fat32_lookup(fs, dir, parent);
  kfree(dir);
  if (!err && !fat32_is_dir(&parent->entry))
    err = -ENOTDIR;
  return err;
}

int fat32_create(FAT32_FS *fs, const char *path, bool is_dir,
                 FAT32_Node *out) {
  FAT32_Node parent;
  char name[VFS_NAME_MAX];
  int err = fat32_parent(fs, path, &parent, name);
  if (err < 0)
    return err;
  if (fat32_find(fs, &parent, name, out) == 0)
    return -EEXIST;

  uint8_t *buf = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  memset(out, 0, sizeof(*out));
  fat32_short_name(name, out->entry.name);
  out->entry.attr = is_dir ? FAT32_ATTR_DIRECTORY : FAT32_ATTR_ARCHIVE;

  /* A directory gets a cluster holding "." and ".."; a file gets its
   * first cluster when something is written to it. */
  uint32_t cluster = 0;
  if (is_dir) {
    cluster = fat32_extend(fs, 0, buf);
    if (!cluster) {
      kfree(buf);
      return -ENOSPC;
    }
    out->entry.first_cluster_high = cluster >> 16;
    out->entry.first_cluster_low = cluster & 0xFFFF;

    uint32_t up = parent.is_root ? 0 : fat32_first_cluster(&parent.entry);
    FAT32_DirectoryEntry *dots = (FAT32_DirectoryEntry *)buf;
    memset(buf, 0, fs->cluster_size);
    dots[0] = out->entry;
    memcpy(dots[0].name, ".          ", 11);
    dots[1] = out->entry;
    memcpy(dots[1].name, "..         ", 11);
    dots[1].first_cluster_high = up >> 16;
    dots[1].first_cluster_low = up & 0xFFFF;
    err = fat32_write_block(fs, cluster, buf);
  }

  /* A free slot in the parent, or a new cluster at its end. */
  uint32_t last = 0;
  int r = err ? err
              : fat32_dir_foreach(fs, fat32_first_cluster(&parent.entry),
                                  fat32_take_free, out, &last);
  if (r == 0) {
    uint32_t more = fat32_extend(fs, last, buf);
    if (!more) {
      r = -ENOSPC;
    } else {
      ((FAT32_DirectoryEntry *)buf)[0] = out->entry;
      out->dir_cluster = more;
      out->index = 0;
      r = fat32_write_block(fs, more, buf) < 0 ? -EIO : 1;
    }
  }
  kfree(buf);

  if (r < 0 && cluster)
    fat32_free_block(fs, cluster);
  fat32_sync(fs);
  return r < 0 ? r : 0;
}

static int fat32_count_entry(FAT32_DirectoryEntry *e, uint32_t cluster,
                             uint32_t index, void *ctx) {
  (void)cluster;
  (void)index;
  if (fat32_entry_live(e) && memcmp(e->name, ".          ", 11) != 0 &&
      memcmp(e->name, "..         ", 11) != 0) {
    *(bool *)ctx = true;
    return FAT32_STOP;
  }
  return FAT32_NEXT;
}

int fat32_remove(FAT32_FS *fs, const char *path) {
  FAT32_Node node;
  int err = fat32_lookup(fs, path, &node);
  if (err < 0)
    return err;
  if (node.is_root)
    return -EBUSY;

  uint32_t cluster = fat32_first_cluster(&node.entry);
  if (fat32_is_dir(&node.entry)) {
    bool used = false;
    fat32_dir_foreach(fs, cluster, fat32_count_entry, &used, NULL);
    if (used)
      return -ENOTEMPTY;
  }

  node.entry.name[0] = FAT32_DIRENT_FREE;
  if ((err = fat32_update_entry(fs, &node)) < 0)
    return err;
  fat32_free_chain(fs, cluster);
  return fat32_sync(fs);
}
