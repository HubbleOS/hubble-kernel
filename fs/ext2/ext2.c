/* -- EXT2 filesystem implementation -------------------------------
 * Core EXT2 driver providing superblock/group descriptor reading,
 * inode I/O, block I/O, directory listing, path resolution, and
 * file/directory creation primitives.
 * ------------------------------------------------------------------ */

#include <hubble/printk.h>

#include <mm/kmalloc.h>

#include <fs/gpt/gpt_struct.h>

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>

#include "ext2.h"
#include "ext2_struct.h"

#include <hubble/string.h>

/* -- Forward declarations ------------------------------------------- */

void ext2_read_block(EXT2_FS *fs, uint32_t block_number, void *buf);
void ext2_write_block(EXT2_FS *fs, uint32_t block_number, void *buf);
static PathParts_ext format_folder_path_ext(const char *in);
static int ext2_read_group_desc(EXT2_FS *fs);
static int ext2_read_superblock(EXT2_FS *fs);
static uint32_t ext2_allocate_inode(EXT2_FS *fs, uint32_t parent_inode);
uint32_t ext2_allocate_block(EXT2_FS *fs, uint32_t group);
int ext2_write_inode(EXT2_FS *fs, uint32_t inode_num, Ext2Inode *inode);
static int ext2_add_dir_entry(EXT2_FS *fs, uint32_t dir_inode_num,
                              const char *name, uint32_t inode_num,
                              uint8_t file_type);
static int ext2_free_block(EXT2_FS *fs, uint32_t block_num);
static int ext2_free_inode(EXT2_FS *fs, uint32_t inode_num);
/* -- Block I/O ------------------------------------------------------ */

void ext2_read_block(EXT2_FS *fs, uint32_t block_number, void *buf) {
  uint32_t sectors_per_block = fs->block_size / 512;
  uint32_t lba = fs->first_lba + block_number * sectors_per_block;

  for (uint32_t i = 0; i < sectors_per_block; i++)
    fs->read_sector(fs->device, lba + i, ((uint8_t *)buf) + i * 512);
}

void ext2_write_block(EXT2_FS *fs, uint32_t block_number, void *buf) {
  uint32_t sectors_per_block = fs->block_size / 512;
  uint32_t lba = fs->first_lba + block_number * sectors_per_block;

  for (uint32_t i = 0; i < sectors_per_block; i++)
    fs->write_sector(fs->device, lba + i, ((uint8_t *)buf) + i * 512);
}

/* -- Helpers -------------------------------------------------------- */

#define EXT2_NAME_LEN 255 /* match your on-disk limit */

static PathParts_ext format_folder_path_ext(const char *in) {
  PathParts_ext result = {0};

  while (*in == '/')
    in++;

  while (*in && result.count < MAX_PARTS) {
    const char *end = in;
    while (*end && *end != '/')
      end++;

    size_t len = end - in;
    if (len > 0) {
      if (len > EXT2_NAME_LEN) {
        /* reject rather than silently truncate a real path */
        goto fail;
      }

      char *name = kmalloc(len + 1, GFP_KERNEL);
      if (!name)
        goto fail;

      memcpy(name, in, len);
      name[len] = '\0';

      result.parts[result.count].name = name;
      result.count++;
    }

    in = end;
    while (*in == '/')
      in++;
  }
  return result;

fail:
  for (int i = 0; i < result.count; i++)
    kfree(result.parts[i].name);
  result.count = 0;
  return result;
}

static void free_path_parts_ext(PathParts_ext *parts) {
  for (int i = 0; i < parts->count; i++)
    kfree(parts->parts[i].name);
}

static int ext2_get_group(EXT2_FS *fs, uint32_t inode_num) {
  return (inode_num - 1) / fs->sb.s_inodes_per_group;
}

static int ext2_get_index(EXT2_FS *fs, uint32_t inode_num) {
  return (inode_num - 1) % fs->sb.s_inodes_per_group;
}

/* -- Superblock / Group descriptor reading -------------------------- */

static int ext2_read_group_desc(EXT2_FS *fs) {
  /* The descriptor table starts in the block after the superblock's:
   * block 2 for 1K blocks, block 1 for larger ones. */
  uint32_t desc_block = fs->sb.s_first_data_block + 1;

  uint32_t groups_count =
      (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) /
      fs->sb.s_blocks_per_group;
  uint32_t desc_size = sizeof(Ext2GroupDesc) * groups_count;

  uint32_t blocks_needed = (desc_size + fs->block_size - 1) / fs->block_size;

  uint8_t *buf = kmalloc(blocks_needed * fs->block_size, GFP_KERNEL);
  if (!buf) {
    printk(KERN_ERR "EXT2: failed to alloc group desc buffer\n");
    return -1;
  }
  for (uint32_t i = 0; i < blocks_needed; i++)
    ext2_read_block(fs, desc_block + i, buf + i * fs->block_size);

  fs->groups = kmalloc(sizeof(Ext2GroupDesc) * groups_count, GFP_KERNEL);
  if (!fs->groups) {
    kfree(buf);
    printk(KERN_ERR "EXT2: failed to alloc fs->groups\n");
    return -1;
  }

  memcpy(fs->groups, buf, desc_size);
  kfree(buf);

  printk(KERN_INFO "EXT2: read %u group descriptors\n", groups_count);
  printk(KERN_INFO "inode_table: %u\n", fs->groups[0].inode_table);
  return 0;
}

int ext2_write_group_desc(EXT2_FS *fs) {
  /* The descriptor table starts in the block after the superblock's:
   * block 2 for 1K blocks, block 1 for larger ones. */
  uint32_t desc_block = fs->sb.s_first_data_block + 1;

  uint32_t groups_count =
      (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) /
      fs->sb.s_blocks_per_group;
  uint32_t desc_size = sizeof(Ext2GroupDesc) * groups_count;

  uint32_t blocks_needed = (desc_size + fs->block_size - 1) / fs->block_size;

  uint8_t *buf = kmalloc(blocks_needed * fs->block_size, GFP_KERNEL);
  if (!buf) {
    printk(KERN_ERR "EXT2: failed to alloc group desc buffer\n");
    return -1;
  }
  memset(buf, 0, blocks_needed * fs->block_size);
  memcpy(buf, fs->groups, desc_size);

  for (uint32_t i = 0; i < blocks_needed; i++)
    ext2_write_block(fs, desc_block + i, buf + i * fs->block_size);

  kfree(buf);
  return 0;
}

static int ext2_read_superblock(EXT2_FS *fs) {
  uint8_t buf[1024];
  printk(KERN_INFO "Reading superblock\n");
  uint32_t superblock_lba = fs->first_lba + 2;
  printk(KERN_INFO "Superblock LBA: %u\n", superblock_lba);
  if (!fs->read_sector) {
    printk(KERN_ERR "EXT2: read_sector is NULL\n");
    return -1;
  }
  for (int i = 0; i < 2; i++)
    fs->read_sector(fs->device, superblock_lba + i, buf + i * 512);

  printk(KERN_INFO "EXT2: read superblock\n");
  Ext2Superblock *sb = (Ext2Superblock *)buf;

  if (sb->s_magic != 0xEF53) {
    printk(KERN_ERR "EXT2: invalid magic 0x%x (expected 0xEF53)\n",
           sb->s_magic);
    printk(KERN_INFO "first_lba=%u superblock_lba=%u\n", fs->first_lba,
           superblock_lba);
    return -1;
  }
  printk(KERN_OK "EXT2: magic ok 0x%x\n", sb->s_magic);

  memcpy(&fs->sb, sb, sizeof(Ext2Superblock));
  fs->block_size = 1024 << sb->s_log_block_size;
  fs->inode_size =
      (sb->s_inode_size && sb->s_inode_size >= 128) ? sb->s_inode_size : 128;

  printk(KERN_OK "EXT2: magic ok 0x%x\n", sb->s_magic);
  printk(KERN_INFO "EXT2: block size = %u bytes\n", fs->block_size);
  printk(KERN_INFO "EXT2: inodes = %u, blocks = %u\n", fs->sb.s_inodes_count,
         fs->sb.s_blocks_count);
  return 0;
}

static int ext2_write_superblock(EXT2_FS *fs) {
  uint8_t buf[1024];
  memcpy(buf, &fs->sb, sizeof(Ext2Superblock));
  uint32_t superblock_lba = fs->first_lba + 2;
  for (int i = 0; i < 2; i++)
    fs->write_sector(fs->device, superblock_lba + i, buf + i * 512);
  return 0;
}

/* -- Public API ------------------------------------------------------ */

/** @brief Initialise the EXT2 filesystem by reading superblock and group
 * descriptors. */
int ext2_init(EXT2_FS *fs) {
  printk(KERN_INFO "Initializing EXT2\n");
  printk(KERN_INFO "Reading superblock\n");
  if (ext2_read_superblock(fs) < 0)
    return -1;
  printk(KERN_INFO "Reading group descriptors\n");
  if (ext2_read_group_desc(fs) < 0)
    return -1;
  return 0;
}

/** @brief Read an inode from disk. */
int ext2_read_inode(EXT2_FS *fs, uint32_t inode_number, Ext2Inode *out_inode) {
  if (inode_number == 0 || inode_number > fs->sb.s_inodes_count)
    return -1;

  uint32_t group = (inode_number - 1) / fs->sb.s_inodes_per_group;
  uint32_t index = (inode_number - 1) % fs->sb.s_inodes_per_group;

  uint32_t inode_table_block = fs->groups[group].inode_table;
  if (inode_table_block == 0)
    return -1;

  uint32_t offset = index * fs->inode_size;
  uint32_t block_offset = offset / fs->block_size;
  uint32_t offset_in_block = offset % fs->block_size;

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf)
    return -1;
  ext2_read_block(fs, inode_table_block + block_offset, block_buf);
  memcpy(out_inode, block_buf + offset_in_block, sizeof(Ext2Inode));
  kfree(block_buf);
  return 0;
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
    ext2_read_block(fs, block, table);
    block = table[idx / span];
    idx %= span;
    span /= per_block;
  }

  kfree(table);
  return block;
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
    if (!disk_block)
      continue;
    ext2_read_block(fs, disk_block, block_buf);

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

/** @brief List the contents of a directory inode. */
Directory ext2_list_dir(EXT2_FS *fs, Ext2Inode *dir_inode) {
  Directory dir = Directory_init((Directory){.entries = NULL, .count = 0});
  if (!ext2_is_dir(dir_inode->mode))
    return dir;

  ext2_list_ctx ctx = {.dir = &dir, .cap = 0};
  ext2_dir_foreach(fs, dir_inode, ext2_list_add, &ctx);
  return dir;
}

/* -- Inode / block allocation ---------------------------------------- */

static uint32_t ext2_allocate_inode(EXT2_FS *fs, uint32_t parent_inode) {
  uint32_t groups_count =
      (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) /
      fs->sb.s_blocks_per_group;

  uint32_t preferred_group = (parent_inode - 1) / fs->sb.s_inodes_per_group;

  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return 0;

  for (uint32_t attempt = 0; attempt < groups_count; attempt++) {
    uint32_t group = (preferred_group + attempt) % groups_count;

    ext2_read_block(fs, fs->groups[group].inode_bitmap, bitmap);

    for (uint32_t i = 0; i < fs->sb.s_inodes_per_group; i++) {
      uint32_t byte = i / 8;
      uint8_t bit = 1 << (i % 8);
      if (!(bitmap[byte] & bit)) {
        bitmap[byte] |= bit;
        ext2_write_block(fs, fs->groups[group].inode_bitmap, bitmap);

        fs->groups[group].free_inodes_count--;
        fs->sb.s_free_inodes_count--;
        ext2_write_group_desc(fs);
        ext2_write_superblock(fs);

        kfree(bitmap);
        return i + 1 + group * fs->sb.s_inodes_per_group;
      }
    }
  }

  kfree(bitmap);
  return 0; // full
}

uint32_t ext2_allocate_block(EXT2_FS *fs, uint32_t parent_inode) {
  uint32_t groups_count =
      (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) /
      fs->sb.s_blocks_per_group;

  uint32_t preferred_group = (parent_inode - 1) / fs->sb.s_inodes_per_group;

  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return 0;

  for (uint32_t attempt = 0; attempt < groups_count; attempt++) {
    uint32_t group = (preferred_group + attempt) % groups_count;

    ext2_read_block(fs, fs->groups[group].block_bitmap, bitmap);

    for (uint32_t i = 0; i < fs->sb.s_blocks_per_group; i++) {
      uint32_t byte = i / 8;
      uint8_t bit = 1 << (i % 8);
      if (!(bitmap[byte] & bit)) {
        bitmap[byte] |= bit;
        ext2_write_block(fs, fs->groups[group].block_bitmap, bitmap);

        fs->groups[group].free_blocks_count--;
        fs->sb.s_free_blocks_count--;

        ext2_write_group_desc(fs);
        ext2_write_superblock(fs);

        kfree(bitmap);
        return fs->sb.s_first_data_block + i +
               group * fs->sb.s_blocks_per_group;
      }
    }
  }

  kfree(bitmap);
  return 0; // full
}

/* -- Inode write / directory entry management ------------------------ */

int ext2_write_inode(EXT2_FS *fs, uint32_t inode_num, Ext2Inode *inode) {
  uint32_t group = (inode_num - 1) / fs->sb.s_inodes_per_group;
  uint32_t index = (inode_num - 1) % fs->sb.s_inodes_per_group;
  uint32_t table_block = fs->groups[group].inode_table;

  /* Slots are fs->inode_size apart (256 on modern mkfs); only the
   * 128-byte base we know about is rewritten. Using sizeof(Ext2Inode)
   * as the stride wrote into the wrong slot whenever they differ. */
  uint32_t offset = index * fs->inode_size;
  uint32_t block_offset = offset / fs->block_size;
  uint32_t offset_in_block = offset % fs->block_size;

  uint8_t *buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!buf)
    return -1;
  ext2_read_block(fs, table_block + block_offset, buf);

  memcpy(buf + offset_in_block, inode, sizeof(Ext2Inode));
  ext2_write_block(fs, table_block + block_offset, buf);

  kfree(buf);
  return 0;
}

static int ext2_add_dir_entry(EXT2_FS *fs, uint32_t dir_inode_num,
                              const char *name, uint32_t inode_num,
                              uint8_t file_type) {
  Ext2Inode dir_inode;
  ext2_read_inode(fs, dir_inode_num, &dir_inode);

  uint8_t *block = kmalloc(fs->block_size, GFP_KERNEL);
  ext2_read_block(fs, dir_inode.block[0], block);

  uint32_t offset = 0;
  while (offset < fs->block_size) {
    Ext2DirEntry *entry = (Ext2DirEntry *)(block + offset);
    if (offset + entry->rec_len >= fs->block_size) {
      uint16_t actual_len = 8 + ((entry->name_len + 3) & ~3);
      uint16_t new_len = fs->block_size - offset - actual_len;
      entry->rec_len = actual_len;

      Ext2DirEntry *new_entry = (Ext2DirEntry *)(block + offset + actual_len);
      new_entry->inode = inode_num;
      new_entry->rec_len = new_len;
      new_entry->name_len = strlen(name);
      new_entry->file_type = file_type;
      memcpy(new_entry->name, name, new_entry->name_len);

      ext2_write_block(fs, dir_inode.block[0], block);
      kfree(block);
      return 0;
    }
    offset += entry->rec_len;
  }

  kfree(block);
  return -1;
}

/** @brief Create a new file under a parent inode. */
uint32_t ext2_create_file(EXT2_FS *fs, uint32_t parent_inode,
                          const char *name) {

  //   uint32_t group = ext2_get_group(fs, parent_inode);

  uint32_t new_inode = ext2_allocate_inode(fs, parent_inode);
  if (!new_inode) {
    printk(KERN_ERR "ext2: failed to allocate inode\n");
    return 0;
  }
  uint32_t new_block = ext2_allocate_block(fs, new_inode);
  if (!new_block) {
    printk(KERN_ERR "ext2: failed to allocate block\n");
    ext2_free_inode(fs, new_inode);
    return 0;
  }
  Ext2Inode inode = {0};
  inode.mode = 0x8000 | 0644;
  inode.size = 0;
  inode.blocks = 2;
  inode.block[0] = new_block;
  inode.links_count = 1;

  ext2_write_inode(fs, new_inode, &inode);
  ext2_add_dir_entry(fs, parent_inode, name, new_inode, 1);

  return new_inode;
}

/* -- Path resolution ------------------------------------------------- */

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

/** @brief Find a directory entry by exact name within a directory inode. */
uint32_t ext2_find_dir_entry(EXT2_FS *fs, uint32_t inode, const char *name) {
  Ext2Inode dir_inode;
  if (ext2_read_inode(fs, inode, &dir_inode) < 0 ||
      !ext2_is_dir(dir_inode.mode))
    return 0;

  ext2_find_ctx ctx = {.name = name, .name_len = strlen(name), .inode = 0};
  ext2_dir_foreach(fs, &dir_inode, ext2_find_match, &ctx);
  return ctx.inode;
}

/* -- Symlinks --------------------------------------------------------- */

#define EXT2_SYMLINK_MAX_DEPTH 8

/* Read a symlink target into buf (NUL-terminated, at most bufsz - 1
 * bytes). Fast symlinks (< 60 bytes) keep the target in i_block itself
 * and own no data block; any other target lives in data block 0. */
static int ext2_read_symlink(EXT2_FS *fs, const Ext2Inode *link, char *buf,
                             size_t bufsz) {
  uint32_t len = link->size;
  if (len == 0 || len >= bufsz || len > fs->block_size)
    return -1;

  uint32_t acl_sectors = link->file_acl ? fs->block_size / 512 : 0;
  if (len < sizeof(link->block) && link->blocks == acl_sectors) {
    memcpy(buf, link->block, len);
  } else {
    uint32_t disk_block = ext2_bmap(fs, link, 0);
    if (!disk_block)
      return -1;
    uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
    if (!block_buf)
      return -1;
    ext2_read_block(fs, disk_block, block_buf);
    memcpy(buf, block_buf, len);
    kfree(block_buf);
  }
  buf[len] = '\0';
  return 0;
}

static uint32_t ext2_resolve(EXT2_FS *fs, uint32_t start, const char *path,
                             int depth);

/* Continue a walk through a symlink: resolve "<target>/<rest...>" with
 * a relative target taken from the link's own directory and an
 * absolute one from this filesystem's root. */
static uint32_t ext2_follow_symlink(EXT2_FS *fs, uint32_t link_dir,
                                    const Ext2Inode *link,
                                    const PathParts_ext *parts, int next,
                                    int depth) {
  size_t rest_len = 0;
  for (int j = next; j < parts->count; j++)
    rest_len += 1 + strlen(parts->parts[j].name);

  size_t cap = fs->block_size + rest_len + 1;
  char *path = kmalloc(cap, GFP_KERNEL);
  if (!path)
    return 0;
  if (ext2_read_symlink(fs, link, path, fs->block_size + 1) < 0) {
    kfree(path);
    return 0;
  }

  size_t len = strlen(path);
  for (int j = next; j < parts->count; j++) {
    size_t n = strlen(parts->parts[j].name);
    path[len++] = '/';
    memcpy(path + len, parts->parts[j].name, n);
    len += n;
  }
  path[len] = '\0';

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
  PathParts_ext parts = format_folder_path_ext(path);

  for (int i = 0; i < parts.count; i++) {
    cur = ext2_find_dir_entry(fs, dir, parts.parts[i].name);
    if (cur == 0)
      break;

    Ext2Inode inode;
    if (ext2_read_inode(fs, cur, &inode) < 0) {
      cur = 0;
      break;
    }
    if (ext2_is_symlink(inode.mode)) {
      cur = ext2_follow_symlink(fs, dir, &inode, &parts, i + 1, depth);
      break;
    }
    dir = cur;
  }

  free_path_parts_ext(&parts);
  return cur;
}

/** @brief Resolve a path to an inode number, following symlinks. */
uint32_t ext2_parse_path(EXT2_FS *fs, uint32_t inode, const char *path) {
  return ext2_resolve(fs, inode, path, 0);
}

static int ext2_free_inode(EXT2_FS *fs, uint32_t inode_num) {
  if (inode_num == 0)
    return -1; /* nothing to free */

  uint32_t group = (inode_num - 1) / fs->sb.s_inodes_per_group;
  uint32_t index = (inode_num - 1) % fs->sb.s_inodes_per_group;

  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return -1;

  ext2_read_block(fs, fs->groups[group].inode_bitmap, bitmap);

  uint32_t byte = index / 8;
  uint8_t bit = 1 << (index % 8);

  if (!(bitmap[byte] & bit)) {
    /* already free */
    kfree(bitmap);
    return -1;
  }

  bitmap[byte] &= ~bit;
  ext2_write_block(fs, fs->groups[group].inode_bitmap, bitmap);
  kfree(bitmap);

  fs->groups[group].free_inodes_count++;
  fs->sb.s_free_inodes_count++;

  ext2_write_group_desc(fs);
  ext2_write_superblock(fs);

  return 0;
}

static int ext2_free_block(EXT2_FS *fs, uint32_t block_num) {
  if (block_num < fs->sb.s_first_data_block)
    return -1; /* invalid / reserved block */

  uint32_t rel = block_num - fs->sb.s_first_data_block;
  uint32_t group = rel / fs->sb.s_blocks_per_group;
  uint32_t index = rel % fs->sb.s_blocks_per_group;

  uint8_t *bitmap = kmalloc(fs->block_size, GFP_KERNEL);
  if (!bitmap)
    return -1;

  ext2_read_block(fs, fs->groups[group].block_bitmap, bitmap);

  uint32_t byte = index / 8;
  uint8_t bit = 1 << (index % 8);

  if (!(bitmap[byte] & bit)) {
    kfree(bitmap);
    return -1; /* double free guard */
  }

  bitmap[byte] &= ~bit;
  ext2_write_block(fs, fs->groups[group].block_bitmap, bitmap);
  kfree(bitmap);

  fs->groups[group].free_blocks_count++;
  fs->sb.s_free_blocks_count++;

  ext2_write_group_desc(fs);
  ext2_write_superblock(fs);

  return 0;
}
