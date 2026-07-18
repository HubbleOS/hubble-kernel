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
static int IS_DIR(uint16_t mode);
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

static int IS_DIR(uint16_t mode) { return (mode & 0xF000) == 0x4000; }

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
  uint32_t desc_block = (fs->block_size == 1024) ? 2 : 0;

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
  memcpy(0, buf, blocks_needed * fs->block_size);

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
  uint32_t desc_block = (fs->block_size == 1024) ? 2 : 0;

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
  Ext2Inode root_inode;
  printk(KERN_INFO "Reading root inode\n");
  ext2_read_inode(fs, 2, &root_inode);

  printk(KERN_INFO "Root inode size = %u bytes\n", root_inode.size);
  printk(KERN_INFO "Root inode first block = %u\n", root_inode.block[0]);
  printk(KERN_INFO "inode blocks count = %u\n", root_inode.blocks);
  ext2_list_dir(fs, &root_inode);
  return 0;
}

/** @brief Read an inode from disk. */
int ext2_read_inode(EXT2_FS *fs, uint32_t inode_number, Ext2Inode *out_inode) {
  printk(KERN_INFO "Reading inode in func %u\n", inode_number);
  uint32_t group = (inode_number - 1) / fs->sb.s_inodes_per_group;
  uint32_t index = (inode_number - 1) % fs->sb.s_inodes_per_group;
  printk(KERN_INFO "group=%u index=%u\n", group, index);

  printk(KERN_INFO "inode_table=%u\n", fs->groups[0].inode_table);
  printk(KERN_INFO "inode_size=%u\n", fs->inode_size);
  Ext2GroupDesc *gd = &fs->groups[group];
  uint32_t inode_table_block = gd->inode_table;

  if (inode_table_block == 0) {
    printk(KERN_INFO "inode_table_block == 0\n");
    return -1;
  }
  printk(KERN_INFO "inode_table_block=%u\n", inode_table_block);

  uint32_t inode_size = fs->inode_size;
  uint32_t offset = index * inode_size;

  uint32_t block_offset = offset / fs->block_size;
  uint32_t offset_in_block = offset % fs->block_size;

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf)
    return -1;
  printk(KERN_INFO "inode_table_block=%u block_offset=%u offset_in_block=%u\n",
         inode_table_block, block_offset, offset_in_block);
  ext2_read_block(fs, inode_table_block + block_offset, block_buf);
  printk(KERN_INFO "inode_size=%u\n", inode_size);
  memcpy(out_inode, block_buf + offset_in_block, sizeof(Ext2Inode));
  printk(KERN_INFO "inode read\n");
  kfree(block_buf);
  return 0;
}

/** @brief List the contents of a directory inode. */
Directory ext2_list_dir(EXT2_FS *fs, Ext2Inode *dir_inode) {
  if (!IS_DIR(dir_inode->mode)) {
    printk(KERN_INFO "Not a directory\n");
    return Directory_init((Directory){.entries = NULL, .count = 0});
  }

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);

  printk(KERN_INFO "Listing directory (size=%u bytes)\n", dir_inode->size);

  Directory dir = Directory_init(
      (Directory){.entries = kmalloc(1024, GFP_KERNEL), .count = 0});

  for (int i = 0; i < 12 && dir_inode->block[i]; i++) {
    printk(KERN_INFO "read in for");
    ext2_read_block(fs, dir_inode->block[i], block_buf);
    printk(KERN_INFO "test");
    uint32_t offset = 0;
    while (offset < fs->block_size) {
      Ext2DirEntry *entry = (Ext2DirEntry *)(block_buf + offset);

      if (entry->inode == 0)
        break;
      dir.entries[dir.count].name = kmalloc(entry->name_len + 1, GFP_KERNEL);
      if (!dir.entries[dir.count].name)
        printk(KERN_ERR "failed");

      memcpy(dir.entries[dir.count].name, entry->name, entry->name_len);
      dir.entries[dir.count].name[entry->name_len] = '\0';
      dir.entries[dir.count].is_dir = entry->file_type == 0x10;
      dir.entries[dir.count].cluster = entry->inode;
      dir.count++;

      offset += entry->rec_len;
      if (entry->rec_len == 0)
        break;
    }
  }
  kfree(block_buf);
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

  uint32_t inode_size = sizeof(Ext2Inode);
  uint32_t inodes_per_block = fs->block_size / inode_size;
  uint32_t block_offset = index / inodes_per_block;
  uint32_t offset = index % inodes_per_block;

  uint8_t *buf = kmalloc(fs->block_size, GFP_KERNEL);
  ext2_read_block(fs, table_block + block_offset, buf);

  memcpy(buf + offset * inode_size, inode, inode_size);
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
    printk("Failed to allocate inode\n");
    return 0;
  }
  uint32_t new_block = ext2_allocate_block(fs, new_inode);
  if (!new_block) {
    printk("Failed to allocate block\n");
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

/** @brief Find a directory entry by name within a given inode. */
uint32_t ext2_find_dir_entry(EXT2_FS *fs, uint32_t inode, const char *name) {
  //   printk("ext2_find_dir_entry: %s \n", name);
  Ext2Inode dir_inode;
  ext2_read_inode(fs, inode, &dir_inode);

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  ext2_read_block(fs, dir_inode.block[0], block_buf);

  uint32_t offset = 0;
  while (offset < fs->block_size) {
    Ext2DirEntry *entry = (Ext2DirEntry *)(block_buf + offset);
    if (entry->inode == 0)
      break;
    if (strncmp(entry->name, name, entry->name_len) == 0) {
      kfree(block_buf);
      //       printk("entry found: %s\n", entry->name);
      return entry->inode;
    }
    //     printk("entry name: %s, name_len: %d, file_type: %d\n", entry->name,
    //            entry->name_len, entry->file_type);
    offset += entry->rec_len;
  }
  kfree(block_buf);
  return 0;
}

/** @brief Resolve a path to an inode number. */
uint32_t ext2_parse_path(EXT2_FS *fs, uint32_t inode, const char *path) {
  PathParts_ext parts = format_folder_path_ext(path);
  uint32_t cur = inode;

  for (int i = 0; i < parts.count; i++) {
    cur = ext2_find_dir_entry(fs, cur, parts.parts[i].name);

    if (cur == 0)
      break;
  }

  free_path_parts_ext(&parts);
  return cur;
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
