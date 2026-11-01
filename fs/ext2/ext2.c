/* -- EXT2 filesystem driver ----------------------------------------
 * Superblock and group descriptors, inode and block I/O, block
 * allocation, directory walking, path lookup (following symlinks), and
 * file data reads and writes. ext2.h lists the API; ext2_vfs.c hooks it
 * into the VFS.
 * ------------------------------------------------------------------ */

#include "ext2.h"
#include "ext2_struct.h"

#include <fs/vfs/path.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

#define EXT2_SUPERBLOCK_OFFSET 1024 /* bytes into the partition */
#define EXT2_SYMLINK_MAX_DEPTH 8

static int ext2_write_superblock(EXT2_FS *fs);
static int ext2_write_group_desc(EXT2_FS *fs);

/* -- Block I/O ------------------------------------------------------ */

int ext2_read_block(EXT2_FS *fs, uint32_t block, void *buf) {
  uint32_t spb = fs->block_size / VFS_SECTOR_SIZE;
  return vfs_dev_read(&fs->dev, fs->first_lba + block * spb, spb, buf) < 0
             ? -EIO
             : 0;
}

int ext2_write_block(EXT2_FS *fs, uint32_t block, const void *buf) {
  uint32_t spb = fs->block_size / VFS_SECTOR_SIZE;
  return vfs_dev_write(&fs->dev, fs->first_lba + block * spb, spb, buf) < 0
             ? -EIO
             : 0;
}

/* -- Superblock / group descriptors --------------------------------- */

/* The descriptor table starts in the block after the superblock's:
 * block 2 for 1K blocks, block 1 for larger ones. */
static uint32_t ext2_desc_block(EXT2_FS *fs) {
  return fs->sb.s_first_data_block + 1;
}

static uint32_t ext2_desc_blocks(EXT2_FS *fs) {
  uint32_t size = sizeof(Ext2GroupDesc) * fs->groups_count;
  return (size + fs->block_size - 1) / fs->block_size;
}

static int ext2_read_superblock(EXT2_FS *fs) {
  uint8_t buf[1024];
  if (vfs_dev_read(&fs->dev, fs->first_lba + EXT2_SUPERBLOCK_OFFSET / 512, 2,
                   buf) < 0)
    return -EIO;

  Ext2Superblock *sb = (Ext2Superblock *)buf;
  if (sb->s_magic != 0xEF53) {
    printk(KERN_ERR "EXT2: invalid magic 0x%x (expected 0xEF53)\n",
           sb->s_magic);
    return -EINVAL;
  }
  if (sb->s_blocks_per_group == 0 || sb->s_inodes_per_group == 0 ||
      sb->s_log_block_size > 6) {
    printk(KERN_ERR "EXT2: bad superblock geometry\n");
    return -EINVAL;
  }

  memcpy(&fs->sb, sb, sizeof(Ext2Superblock));
  fs->block_size = 1024 << sb->s_log_block_size;
  fs->inode_size =
      (sb->s_inode_size && sb->s_inode_size >= 128) ? sb->s_inode_size : 128;
  fs->groups_count = (sb->s_blocks_count + sb->s_blocks_per_group - 1) /
                     sb->s_blocks_per_group;

  printk(KERN_INFO "EXT2: block size %u, %u inodes, %u blocks, %u groups\n",
         fs->block_size, sb->s_inodes_count, sb->s_blocks_count,
         fs->groups_count);
  return 0;
}

static int ext2_write_superblock(EXT2_FS *fs) {
  uint8_t buf[1024] = {0};
  memcpy(buf, &fs->sb, sizeof(Ext2Superblock));
  return vfs_dev_write(&fs->dev, fs->first_lba + EXT2_SUPERBLOCK_OFFSET / 512,
                       2, buf) < 0
             ? -EIO
             : 0;
}

static int ext2_read_group_desc(EXT2_FS *fs) {
  uint32_t blocks = ext2_desc_blocks(fs);
  uint8_t *buf = kmalloc(blocks * fs->block_size, GFP_KERNEL);
  fs->groups = kmalloc(sizeof(Ext2GroupDesc) * fs->groups_count, GFP_KERNEL);
  if (!buf || !fs->groups) {
    kfree(buf);
    kfree(fs->groups);
    fs->groups = NULL;
    return -ENOMEM;
  }
  for (uint32_t i = 0; i < blocks; i++) {
    if (ext2_read_block(fs, ext2_desc_block(fs) + i,
                        buf + i * fs->block_size) < 0) {
      kfree(buf);
      kfree(fs->groups);
      fs->groups = NULL;
      return -EIO;
    }
  }
  memcpy(fs->groups, buf, sizeof(Ext2GroupDesc) * fs->groups_count);
  kfree(buf);
  return 0;
}

static int ext2_write_group_desc(EXT2_FS *fs) {
  uint32_t blocks = ext2_desc_blocks(fs);
  uint8_t *buf = kzalloc(blocks * fs->block_size);
  if (!buf)
    return -ENOMEM;
  memcpy(buf, fs->groups, sizeof(Ext2GroupDesc) * fs->groups_count);

  int err = 0;
  for (uint32_t i = 0; i < blocks && !err; i++)
    err = ext2_write_block(fs, ext2_desc_block(fs) + i,
                           buf + i * fs->block_size);
  kfree(buf);
  return err;
}

/* -- Mount ------------------------------------------------------------ */

int ext2_mount(EXT2_FS *fs, const VFS_Device *dev, uint32_t first_lba) {
  memset(fs, 0, sizeof(*fs));
  fs->dev = *dev;
  fs->first_lba = first_lba;

  int err = ext2_read_superblock(fs);
  if (err < 0)
    return err;
  return ext2_read_group_desc(fs);
}

void ext2_unmount(EXT2_FS *fs) {
  kfree(fs->groups);
  fs->groups = NULL;
}

/* -- Inodes ----------------------------------------------------------- */

/* The block of the inode table holding @p ino, and its offset there. */
static int ext2_inode_pos(EXT2_FS *fs, uint32_t ino, uint32_t *block,
                          uint32_t *offset) {
  if (ino == 0 || ino > fs->sb.s_inodes_count)
    return -EINVAL;
  uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
  uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;
  if (group >= fs->groups_count || fs->groups[group].inode_table == 0)
    return -EINVAL;

  /* Slots are fs->inode_size apart (256 on modern mkfs); only the
   * 128-byte base we know about is read and written. */
  uint32_t byte = index * fs->inode_size;
  *block = fs->groups[group].inode_table + byte / fs->block_size;
  *offset = byte % fs->block_size;
  return 0;
}

int ext2_read_inode(EXT2_FS *fs, uint32_t ino, Ext2Inode *out) {
  uint32_t block, offset;
  int err = ext2_inode_pos(fs, ino, &block, &offset);
  if (err < 0)
    return err;

  uint8_t *buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;
  err = ext2_read_block(fs, block, buf);
  if (!err)
    memcpy(out, buf + offset, sizeof(Ext2Inode));
  kfree(buf);
  return err;
}

int ext2_write_inode(EXT2_FS *fs, uint32_t ino, const Ext2Inode *inode) {
  uint32_t block, offset;
  int err = ext2_inode_pos(fs, ino, &block, &offset);
  if (err < 0)
    return err;

  uint8_t *buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;
  err = ext2_read_block(fs, block, buf);
  if (!err) {
    memcpy(buf + offset, inode, sizeof(Ext2Inode));
    err = ext2_write_block(fs, block, buf);
  }
  kfree(buf);
  return err;
}

/* -- Block mapping ---------------------------------------------------- */

/** @brief Map a file-relative block index to an on-disk block number.
 *
 * Walks the single, double and triple indirect blocks. Returns 0 for a
 * hole or an index beyond the triple-indirect range. */
uint32_t ext2_bmap(EXT2_FS *fs, const Ext2Inode *inode, uint32_t index) {
  uint64_t per_block = fs->block_size / sizeof(uint32_t);
  uint64_t idx = index;
  uint64_t span; /* file blocks covered by one entry of the top table */
  int levels;

  if (idx < 12)
    return inode->block[idx];
  idx -= 12;

  if (idx < per_block) {
    levels = 1;
    span = 1;
  } else if ((idx -= per_block) < per_block * per_block) {
    levels = 2;
    span = per_block;
  } else if ((idx -= per_block * per_block) <
             per_block * per_block * per_block) {
    levels = 3;
    span = per_block * per_block;
  } else {
    return 0;
  }

  uint32_t block = inode->block[11 + levels];
  uint32_t *table = kmalloc(fs->block_size, GFP_KERNEL);
  if (!table)
    return 0;

  while (block && levels-- > 0) {
    if (ext2_read_block(fs, block, table) < 0) {
      block = 0;
      break;
    }
    block = table[idx / span];
    idx %= span;
    span /= per_block;
  }

  kfree(table);
  return block;
}

/* -- Bitmap allocation -------------------------------------------------- */

/* Take the first clear bit of a group's bitmap, starting with the group
 * of @p hint_ino. @return The bit's number counted from group 0, or -1 */
static int64_t ext2_bitmap_alloc(EXT2_FS *fs, uint32_t hint_ino, bool inode) {
  uint32_t per_group =
      inode ? fs->sb.s_inodes_per_group : fs->sb.s_blocks_per_group;
  uint32_t first = hint_ino ? (hint_ino - 1) / fs->sb.s_inodes_per_group : 0;

  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return -1;

  for (uint32_t attempt = 0; attempt < fs->groups_count; attempt++) {
    uint32_t group = (first + attempt) % fs->groups_count;
    Ext2GroupDesc *gd = &fs->groups[group];
    if ((inode ? gd->free_inodes_count : gd->free_blocks_count) == 0)
      continue;
    uint32_t map = inode ? gd->inode_bitmap : gd->block_bitmap;
    if (ext2_read_block(fs, map, bitmap) < 0)
      continue;

    for (uint32_t i = 0; i < per_group; i++) {
      uint8_t bit = 1 << (i % 8);
      if (bitmap[i / 8] & bit)
        continue;
      bitmap[i / 8] |= bit;
      if (ext2_write_block(fs, map, bitmap) < 0)
        break;
      if (inode) {
        gd->free_inodes_count--;
        fs->sb.s_free_inodes_count--;
      } else {
        gd->free_blocks_count--;
        fs->sb.s_free_blocks_count--;
      }
      ext2_write_group_desc(fs);
      ext2_write_superblock(fs);
      kfree(bitmap);
      return (int64_t)group * per_group + i;
    }
  }

  kfree(bitmap);
  return -1;
}

/* Clear bit @p n (counted from group 0) of the inode or block bitmap. */
static int ext2_bitmap_free(EXT2_FS *fs, uint32_t n, bool inode) {
  uint32_t per_group =
      inode ? fs->sb.s_inodes_per_group : fs->sb.s_blocks_per_group;
  uint32_t group = n / per_group, index = n % per_group;
  if (group >= fs->groups_count)
    return -EINVAL;

  Ext2GroupDesc *gd = &fs->groups[group];
  uint32_t map = inode ? gd->inode_bitmap : gd->block_bitmap;
  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return -ENOMEM;

  int err = ext2_read_block(fs, map, bitmap);
  uint8_t bit = 1 << (index % 8);
  if (!err && !(bitmap[index / 8] & bit))
    err = -EINVAL; /* already free */
  if (!err) {
    bitmap[index / 8] &= ~bit;
    err = ext2_write_block(fs, map, bitmap);
  }
  kfree(bitmap);
  if (err)
    return err;

  if (inode) {
    gd->free_inodes_count++;
    fs->sb.s_free_inodes_count++;
  } else {
    gd->free_blocks_count++;
    fs->sb.s_free_blocks_count++;
  }
  ext2_write_group_desc(fs);
  ext2_write_superblock(fs);
  return 0;
}

uint32_t ext2_alloc_block(EXT2_FS *fs, uint32_t hint) {
  int64_t n = ext2_bitmap_alloc(fs, hint, false);
  return n < 0 ? 0 : fs->sb.s_first_data_block + (uint32_t)n;
}

int ext2_free_block(EXT2_FS *fs, uint32_t block) {
  if (block < fs->sb.s_first_data_block)
    return -EINVAL;
  return ext2_bitmap_free(fs, block - fs->sb.s_first_data_block, false);
}

static uint32_t ext2_alloc_inode(EXT2_FS *fs, uint32_t hint) {
  int64_t n = ext2_bitmap_alloc(fs, hint, true);
  return n < 0 ? 0 : (uint32_t)n + 1;
}

static int ext2_free_inode(EXT2_FS *fs, uint32_t ino) {
  if (ino == 0)
    return -EINVAL;
  return ext2_bitmap_free(fs, ino - 1, true);
}

/* -- Directory walking ------------------------------------------------ */

/* Callback per live directory entry; return true to stop the walk. */
typedef bool (*ext2_dirent_fn)(const Ext2DirEntry *entry, void *ctx);

/* Visit every live entry in every block of a directory. Deleted entries
 * (inode 0) are skipped, not treated as the end; a malformed rec_len
 * ends that block instead of looping or running off its end. */
static void ext2_dir_foreach(EXT2_FS *fs, const Ext2Inode *dir,
                             ext2_dirent_fn fn, void *ctx) {
  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf)
    return;

  uint32_t nblocks = (dir->size + fs->block_size - 1) / fs->block_size;
  for (uint32_t b = 0; b < nblocks; b++) {
    uint32_t disk_block = ext2_bmap(fs, dir, b);
    if (!disk_block || ext2_read_block(fs, disk_block, block_buf) < 0)
      continue;

    uint32_t offset = 0;
    while (offset + sizeof(Ext2DirEntry) <= fs->block_size) {
      Ext2DirEntry *entry = (Ext2DirEntry *)(block_buf + offset);
      if (entry->rec_len < sizeof(Ext2DirEntry) ||
          offset + entry->rec_len > fs->block_size ||
          entry->name_len > entry->rec_len - sizeof(Ext2DirEntry))
        break;

      if (entry->inode && fn(entry, ctx)) {
        kfree(block_buf);
        return;
      }
      offset += entry->rec_len;
    }
  }
  kfree(block_buf);
}

typedef struct {
  Directory *dir;
  int cap;
} ext2_list_ctx;

static bool ext2_list_add(const Ext2DirEntry *entry, void *ctx) {
  ext2_list_ctx *c = ctx;

  if (c->dir->count == c->cap) {
    int cap = c->cap ? c->cap * 2 : 16;
    Entry *grown = krealloc(c->dir->entries, cap * sizeof(Entry), GFP_KERNEL);
    if (!grown)
      return true;
    c->dir->entries = grown;
    c->cap = cap;
  }

  char *name = kmalloc(entry->name_len + 1, GFP_KERNEL);
  if (!name)
    return true;
  memcpy(name, entry->name, entry->name_len);
  name[entry->name_len] = '\0';

  Entry *out = &c->dir->entries[c->dir->count++];
  out->name = name;
  out->is_dir = entry->file_type == EXT2_FT_DIR;
  out->cluster = entry->inode;
  return false;
}

Directory ext2_readdir(EXT2_FS *fs, const EXT2_Node *dir) {
  Directory list = Directory_init((Directory){.entries = NULL, .count = 0});
  if (!ext2_is_dir(dir->inode.mode))
    return list;

  ext2_list_ctx ctx = {.dir = &list, .cap = 0};
  ext2_dir_foreach(fs, &dir->inode, ext2_list_add, &ctx);
  return list;
}

typedef struct {
  const char *name;
  size_t name_len;
  uint32_t inode;
} ext2_find_ctx;

static bool ext2_find_match(const Ext2DirEntry *entry, void *ctx) {
  ext2_find_ctx *c = ctx;
  if (entry->name_len != c->name_len ||
      memcmp(entry->name, c->name, c->name_len) != 0)
    return false;
  c->inode = entry->inode;
  return true;
}

/* The inode number of @p name in directory @p dir_ino, or 0. */
static uint32_t ext2_find_entry(EXT2_FS *fs, uint32_t dir_ino,
                                const char *name) {
  Ext2Inode dir;
  if (ext2_read_inode(fs, dir_ino, &dir) < 0 || !ext2_is_dir(dir.mode))
    return 0;

  ext2_find_ctx ctx = {.name = name, .name_len = strlen(name), .inode = 0};
  ext2_dir_foreach(fs, &dir, ext2_find_match, &ctx);
  return ctx.inode;
}

/* -- Symlinks and path lookup ------------------------------------------ */

/* Read a symlink target into buf (NUL-terminated, at most bufsz - 1
 * bytes). Fast symlinks (< 60 bytes) keep the target in i_block itself
 * and own no data block; any other target lives in data block 0. */
static int ext2_read_symlink(EXT2_FS *fs, const Ext2Inode *link, char *buf,
                             size_t bufsz) {
  uint32_t len = link->size;
  if (len == 0 || len >= bufsz || len > fs->block_size)
    return -EINVAL;

  uint32_t acl_sectors = link->file_acl ? fs->block_size / 512 : 0;
  if (len < sizeof(link->block) && link->blocks == acl_sectors) {
    memcpy(buf, link->block, len);
  } else {
    uint32_t disk_block = ext2_bmap(fs, link, 0);
    if (!disk_block)
      return -EIO;
    uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
    if (!block_buf)
      return -ENOMEM;
    int err = ext2_read_block(fs, disk_block, block_buf);
    if (!err)
      memcpy(buf, block_buf, len);
    kfree(block_buf);
    if (err)
      return err;
  }
  buf[len] = '\0';
  return 0;
}

static uint32_t ext2_resolve(EXT2_FS *fs, uint32_t start, const char *path,
                             int depth);

/* Continue a walk through a symlink: resolve "<target><rest>", with a
 * relative target taken from the link's own directory and an absolute
 * one from this filesystem's root. */
static uint32_t ext2_follow_symlink(EXT2_FS *fs, uint32_t link_dir,
                                    const Ext2Inode *link, const char *rest,
                                    int depth) {
  size_t cap = fs->block_size + 1 + strlen(rest) + 1;
  char *path = kmalloc(cap, GFP_KERNEL);
  if (!path)
    return 0;
  if (ext2_read_symlink(fs, link, path, fs->block_size + 1) < 0) {
    kfree(path);
    return 0;
  }
  if (*rest) {
    strcat(path, "/");
    strcat(path, rest);
  }

  uint32_t result = ext2_resolve(fs, link_dir, path, depth + 1);
  kfree(path);
  return result;
}

/* Walk path component by component from start (or the root, for an
 * absolute path), following every symlink including the last one. */
static uint32_t ext2_resolve(EXT2_FS *fs, uint32_t start, const char *path,
                             int depth) {
  if (depth > EXT2_SYMLINK_MAX_DEPTH)
    return 0;

  uint32_t dir = (*path == '/') ? EXT2_ROOT_INO : start;
  uint32_t cur = dir;
  char name[VFS_NAME_MAX];
  int len;

  while ((len = vfs_path_next(&path, name)) > 0) {
    cur = ext2_find_entry(fs, dir, name);
    if (cur == 0)
      return 0;

    Ext2Inode inode;
    if (ext2_read_inode(fs, cur, &inode) < 0)
      return 0;
    if (ext2_is_symlink(inode.mode))
      return ext2_follow_symlink(fs, dir, &inode, path, depth);
    dir = cur;
  }
  return len < 0 ? 0 : cur;
}

int ext2_lookup(EXT2_FS *fs, const char *path, EXT2_Node *out) {
  uint32_t ino = ext2_resolve(fs, EXT2_ROOT_INO, path, 0);
  if (ino == 0)
    return -ENOENT;
  out->ino = ino;
  return ext2_read_inode(fs, ino, &out->inode);
}

/* -- File data ---------------------------------------------------------- */

int ext2_read(EXT2_FS *fs, EXT2_Node *node, uint32_t pos, void *buf,
              uint32_t size) {
  Ext2Inode *inode = &node->inode;
  if (ext2_is_dir(inode->mode))
    return -EISDIR;
  if (pos >= inode->size)
    return 0;
  if (size > inode->size - pos)
    size = inode->size - pos;

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf)
    return -ENOMEM;

  uint32_t done = 0;
  while (done < size) {
    uint32_t at = pos + done;
    uint32_t offset = at % fs->block_size;
    uint32_t chunk = fs->block_size - offset;
    if (chunk > size - done)
      chunk = size - done;

    /* A hole (block 0) reads as zeros. */
    uint32_t block = ext2_bmap(fs, inode, at / fs->block_size);
    if (!block)
      memset((uint8_t *)buf + done, 0, chunk);
    else if (ext2_read_block(fs, block, block_buf) < 0)
      break;
    else
      memcpy((uint8_t *)buf + done, block_buf + offset, chunk);
    done += chunk;
  }

  kfree(block_buf);
  return done ? (int)done : (size ? -EIO : 0);
}

int ext2_write(EXT2_FS *fs, EXT2_Node *node, uint32_t pos, const void *buf,
               uint32_t size) {
  Ext2Inode *inode = &node->inode;
  if (ext2_is_dir(inode->mode))
    return -EISDIR;

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf)
    return -ENOMEM;

  uint32_t done = 0;
  bool dirty = false;
  int err = 0;
  while (done < size) {
    uint32_t at = pos + done;
    uint32_t index = at / fs->block_size;
    uint32_t offset = at % fs->block_size;
    uint32_t chunk = fs->block_size - offset;
    if (chunk > size - done)
      chunk = size - done;

    if (index >= 12) {
      err = -EFBIG; /* indirect blocks cannot be written yet */
      break;
    }

    uint32_t block = inode->block[index];
    if (block == 0) {
      block = ext2_alloc_block(fs, node->ino);
      if (block == 0) {
        err = -ENOSPC;
        break;
      }
      inode->block[index] = block;
      inode->blocks += fs->block_size / 512;
      dirty = true;
      memset(block_buf, 0, fs->block_size);
    } else if (offset != 0 || chunk < fs->block_size) {
      if ((err = ext2_read_block(fs, block, block_buf)) < 0)
        break;
    }

    memcpy(block_buf + offset, (const uint8_t *)buf + done, chunk);
    if ((err = ext2_write_block(fs, block, block_buf)) < 0)
      break;
    done += chunk;
  }
  kfree(block_buf);

  if (pos + done > inode->size) {
    inode->size = pos + done;
    dirty = true;
  }
  if (dirty)
    ext2_write_inode(fs, node->ino, inode);
  return done ? (int)done : err;
}

/* -- Creating files (not wired to the VFS yet) ------------------------- */

static int ext2_add_dir_entry(EXT2_FS *fs, uint32_t dir_ino, const char *name,
                              uint32_t ino, uint8_t file_type) {
  Ext2Inode dir;
  int err = ext2_read_inode(fs, dir_ino, &dir);
  if (err < 0)
    return err;

  uint8_t *block = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block)
    return -ENOMEM;
  if ((err = ext2_read_block(fs, dir.block[0], block)) < 0) {
    kfree(block);
    return err;
  }

  uint32_t offset = 0;
  while (offset < fs->block_size) {
    Ext2DirEntry *entry = (Ext2DirEntry *)(block + offset);
    if (offset + entry->rec_len >= fs->block_size) {
      uint16_t actual_len = 8 + ((entry->name_len + 3) & ~3);
      uint16_t new_len = fs->block_size - offset - actual_len;
      entry->rec_len = actual_len;

      Ext2DirEntry *new_entry = (Ext2DirEntry *)(block + offset + actual_len);
      new_entry->inode = ino;
      new_entry->rec_len = new_len;
      new_entry->name_len = strlen(name);
      new_entry->file_type = file_type;
      memcpy(new_entry->name, name, new_entry->name_len);

      err = ext2_write_block(fs, dir.block[0], block);
      kfree(block);
      return err;
    }
    offset += entry->rec_len;
  }

  kfree(block);
  return -ENOSPC;
}

/** @brief Create an empty file @p name in directory @p parent_ino. Only
 * the directory's first block is used, so this is not hooked into the VFS
 * yet. @return The new inode number, or 0 */
uint32_t ext2_create_file(EXT2_FS *fs, uint32_t parent_ino, const char *name) {
  uint32_t ino = ext2_alloc_inode(fs, parent_ino);
  if (!ino) {
    printk(KERN_ERR "EXT2: failed to allocate inode\n");
    return 0;
  }
  uint32_t block = ext2_alloc_block(fs, ino);
  if (!block) {
    printk(KERN_ERR "EXT2: failed to allocate block\n");
    ext2_free_inode(fs, ino);
    return 0;
  }
  Ext2Inode inode = {0};
  inode.mode = EXT2_S_IFREG | 0644;
  inode.blocks = fs->block_size / 512;
  inode.block[0] = block;
  inode.links_count = 1;

  ext2_write_inode(fs, ino, &inode);
  ext2_add_dir_entry(fs, parent_ino, name, ino, 1);
  return ino;
}
