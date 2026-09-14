/* -- EXT2 on-disk and in-memory data structures -------------------
 * Defines the EXT2 superblock, block group descriptor, inode,
 * directory entry, filesystem context, and path parsing helpers.
 * ------------------------------------------------------------------ */

#pragma once

#include <stdint.h>

/** @brief EXT2 superblock structure (packed on-disk format). */
typedef struct {
  uint32_t s_inodes_count;
  uint32_t s_blocks_count;
  uint32_t s_r_blocks_count;
  uint32_t s_free_blocks_count;
  uint32_t s_free_inodes_count;
  uint32_t s_first_data_block;
  uint32_t s_log_block_size;
  uint32_t s_log_frag_size;
  uint32_t s_blocks_per_group;
  uint32_t s_frags_per_group;
  uint32_t s_inodes_per_group;
  uint32_t s_mtime;
  uint32_t s_wtime;
  uint16_t s_mnt_count;
  uint16_t s_max_mnt_count;
  uint16_t s_magic;
  uint16_t s_state;
  uint16_t s_errors;
  uint16_t s_minor_rev_level;
  uint32_t s_lastcheck;
  uint32_t s_checkinterval;
  uint32_t s_creator_os;
  uint32_t s_rev_level;
  uint16_t s_def_resuid;
  uint16_t s_def_resgid;

  /* -- Rev >= 1 extensions -------------------------------- */
  uint32_t s_first_ino;
  uint16_t s_inode_size;
  uint16_t s_block_group_nr;
  uint32_t s_feature_compat;
  uint32_t s_feature_incompat;
  uint32_t s_feature_ro_compat;
  uint8_t s_uuid[16];
  char s_volume_name[16];
  char s_last_mounted[64];
  uint32_t s_algorithm_usage_bitmap;

  /* -- Journaling (EXT3/EXT4) -------------------------------- */
  uint8_t s_prealloc_blocks;
  uint8_t s_prealloc_dir_blocks;
  uint16_t s_padding1;
  uint8_t s_journal_uuid[16];
  uint32_t s_journal_inum;
  uint32_t s_journal_dev;
  uint32_t s_last_orphan;

  uint32_t s_hash_seed[4];
  uint8_t s_def_hash_version;
  uint8_t s_jnl_backup_type;
  uint16_t s_desc_size;
  uint32_t s_default_mount_opts;
  uint32_t s_first_meta_bg;
  uint32_t s_mkfs_time;

  /* -- EXT4-specific fields -------------------------------- */
  uint32_t s_jnl_blocks[17];
  uint32_t s_blocks_count_hi;
  uint32_t s_r_blocks_count_hi;
  uint32_t s_free_blocks_count_hi;
  uint16_t s_min_extra_isize;
  uint16_t s_want_extra_isize;
  uint32_t s_flags;
  uint16_t s_raid_stride;
  uint16_t s_mmp_interval;
  uint64_t s_mmp_block;
  uint32_t s_raid_stripe_width;
  uint8_t s_log_groups_per_flex;
  uint8_t s_checksum_type;
  uint16_t s_reserved_pad;
  uint64_t s_kbytes_written;
  uint32_t s_snapshot_inum;
  uint32_t s_snapshot_id;
  uint64_t s_snapshot_r_blocks_count;
  uint32_t s_snapshot_list;
  uint32_t s_error_count;
  uint32_t s_first_error_time;
  uint32_t s_first_error_ino;
  uint64_t s_first_error_block;
  uint8_t s_first_error_func[32];
  uint32_t s_first_error_line;
  uint32_t s_last_error_time;
  uint32_t s_last_error_ino;
  uint32_t s_last_error_line;
  uint64_t s_last_error_block;
  uint8_t s_last_error_func[32];
  uint32_t s_mount_opts;
  uint32_t s_usr_quota_inum;
  uint32_t s_grp_quota_inum;
  uint32_t s_overhead_blocks;
  uint32_t s_backup_bgs[2];
  uint8_t s_encrypt_algos[4];
  uint8_t s_encrypt_pw_salt[16];
  uint32_t s_lpf_ino;
  uint32_t s_prj_quota_inum;
  uint32_t s_checksum_seed;
  uint8_t s_wtime_hi;
  uint8_t s_mtime_hi;
  uint8_t s_mkfs_time_hi;
  uint8_t s_lastcheck_hi;
  uint8_t s_first_error_time_hi;
  uint8_t s_last_error_time_hi;
  uint8_t s_first_error_errcode;
  uint8_t s_last_error_errcode;
  uint16_t s_encoding;
  uint16_t s_encoding_flags;
  uint32_t s_orphan_file_inum;
  uint32_t s_reserved[94];
  uint32_t s_checksum;
} __attribute__((packed)) Ext2Superblock;

/** @brief Block group descriptor (packed). */
typedef struct {
  uint32_t block_bitmap;
  uint32_t inode_bitmap;
  uint32_t inode_table;
  uint16_t free_blocks_count;
  uint16_t free_inodes_count;
  uint16_t used_dirs_count;
  uint16_t pad;
  uint8_t reserved[12];
} __attribute__((packed)) Ext2GroupDesc;

/** @brief Inode structure (packed, 128 bytes base). */
typedef struct {
  uint16_t mode;
  uint16_t uid;
  uint32_t size;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint16_t gid;
  uint16_t links_count;
  uint32_t blocks;
  uint32_t flags;
  uint32_t osd1;
  uint32_t block[15];
  uint32_t generation;
  uint32_t file_acl;
  uint32_t dir_acl;
  uint32_t faddr;
  uint8_t osd2[12];
} __attribute__((packed)) Ext2Inode;

/** @brief Directory entry (packed, variable-length name). */
typedef struct {
  uint32_t inode;
  uint16_t rec_len;
  uint8_t name_len;
  uint8_t file_type;
  char name[];
} __attribute__((packed)) Ext2DirEntry;

/** @brief In-memory EXT2 filesystem context. */
typedef struct {
  void *device;
  int (*read_sector)(void *device, uint32_t lba, void *buffer);
  int (*write_sector)(void *device, uint32_t lba, const void *buffer);

  uint32_t first_lba;

  uint32_t block_size;
  uint32_t inode_size;

  Ext2Superblock sb;

  Ext2GroupDesc *groups;
  Ext2Inode *inodes;
} EXT2_FS;

#define MAX_PARTS 16

/** @brief EXT2 path component. */
typedef struct {
  char *name;
} PathPart_ext;

/** @brief Parsed EXT2 path. */
typedef struct {
  int count;
  PathPart_ext parts[MAX_PARTS];
} PathParts_ext;
