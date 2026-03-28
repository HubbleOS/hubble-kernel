#pragma once

#include <stdint.h>

typedef struct
{
	uint32_t s_inodes_count;      // Загальна кількість inode
	uint32_t s_blocks_count;      // Загальна кількість блоків
	uint32_t s_r_blocks_count;    // Зарезервовані блоки (для root)
	uint32_t s_free_blocks_count; // Кількість вільних блоків
	uint32_t s_free_inodes_count; // Кількість вільних inode
	uint32_t s_first_data_block;  // Номер першого блоку даних
	uint32_t s_log_block_size;    // Розмір блоку = 1024 << s_log_block_size
	uint32_t s_log_frag_size;     // Розмір фрагменту (застаріло)
	uint32_t s_blocks_per_group;  // Блоків у групі
	uint32_t s_frags_per_group;   // Фрагментів у групі
	uint32_t s_inodes_per_group;  // Inode у групі
	uint32_t s_mtime;	      // Час останнього монтування
	uint32_t s_wtime;	      // Час останнього запису суперблоку
	uint16_t s_mnt_count;	      // Кількість монтувань від останньої перевірки
	uint16_t s_max_mnt_count;     // Максимальна кількість монтувань між перевірками
	uint16_t s_magic;	      // Магічне число (0xEF53)
	uint16_t s_state;	      // Стан файлової системи
	uint16_t s_errors;	      // Поведінка при помилках
	uint16_t s_minor_rev_level;   // Молодший номер ревізії
	uint32_t s_lastcheck;	      // Остання перевірка fsck
	uint32_t s_checkinterval;     // Інтервал між перевірками
	uint32_t s_creator_os;	      // ОС, що створила файлову систему
	uint32_t s_rev_level;	      // Рівень ревізії
	uint16_t s_def_resuid;	      // UID користувача для зарезервованих блоків
	uint16_t s_def_resgid;	      // GID користувача для зарезервованих блоків

	// --- Розширення для ревізії >= 1 ---
	uint32_t s_first_ino;		   // Перший не зарезервований inode
	uint16_t s_inode_size;		   // Розмір структури inode
	uint16_t s_block_group_nr;	   // Номер блоку групи (для копій суперблоку)
	uint32_t s_feature_compat;	   // Сумісні опції
	uint32_t s_feature_incompat;	   // Несумісні опції
	uint32_t s_feature_ro_compat;	   // Readonly-сумісні опції
	uint8_t s_uuid[16];		   // UUID файлової системи
	char s_volume_name[16];		   // Назва тома
	char s_last_mounted[64];	   // Шлях останнього монтування
	uint32_t s_algorithm_usage_bitmap; // Для компресії (не використовується)

	// --- Для журналювання (EXT3/EXT4) ---
	uint8_t s_prealloc_blocks;     // Кількість попередньо аллокованих блоків
	uint8_t s_prealloc_dir_blocks; // Для директорій
	uint16_t s_padding1;
	uint8_t s_journal_uuid[16]; // UUID журналу
	uint32_t s_journal_inum;    // inode журналу
	uint32_t s_journal_dev;	    // Номер пристрою журналу
	uint32_t s_last_orphan;	    // Список осиротілих inode

	uint32_t s_hash_seed[4];    // Хешування імен файлів
	uint8_t s_def_hash_version; // Алгоритм хешу
	uint8_t s_jnl_backup_type;
	uint16_t s_desc_size; // Розмір дескриптора групи
	uint32_t s_default_mount_opts;
	uint32_t s_first_meta_bg; // Перша мета-група
	uint32_t s_mkfs_time;	  // Час створення файлової системи

	// --- EXT4 специфічні поля ---
	uint32_t s_jnl_blocks[17]; // Блоки журналу
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
	uint32_t s_reserved[94]; // Запас для майбутнього
	uint32_t s_checksum;	 // CRC32 суперблоку
} __attribute__((packed)) Ext2Superblock;

typedef struct
{
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t pad;
	uint8_t reserved[12];
} __attribute__((packed)) Ext2GroupDesc;

typedef struct
{
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
	uint32_t block[15]; // pointers: 0–11 direct, 12 single indirect, 13 double, 14 triple
	uint32_t generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t faddr;
	uint8_t osd2[12];
} __attribute__((packed)) Ext2Inode;

typedef struct
{
	uint32_t inode;	   // номер inode цього файлу/папки
	uint16_t rec_len;  // довжина запису (щоб перейти до наступного)
	uint8_t name_len;  // довжина імені (в байтах)
	uint8_t file_type; // тип (1 = файл, 2 = каталог, інше — спец)
	char name[];	   // саме ім'я (без \0)
} __attribute__((packed)) Ext2DirEntry;

typedef struct
{
	void *device;
	int (*read_sector)(void *device, uint32_t lba, void *buffer);
	int (*write_sector)(void *device, uint32_t lba, const void *buffer);

	uint32_t first_lba;

	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t blocks_per_group;
	uint32_t inodes_per_group;
	uint16_t magic;
	uint32_t block_size;

	uint32_t inode_size;

	Ext2GroupDesc *groups;
	Ext2Inode *inodes;
} EXT2_FS;

#define MAX_PARTS 16
typedef struct
{
	char *name;
} PathPart_ext;

typedef struct
{
	int count;
	PathPart_ext parts[MAX_PARTS];
} PathParts_ext;
