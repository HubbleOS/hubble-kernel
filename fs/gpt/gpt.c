/* -- GPT partition table parser -----------------------------------
 * Reads the GPT header and partition entries from a block device,
 * validates the signature, CRCs, and field ranges, and populates
 * an array of partition descriptors for use by the VFS layer.
 * ------------------------------------------------------------------ */

#include "gpt.h"
#include "gpt_struct.h"
#include "higher_half.h"
#include <hubble/printk.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>

#include <hubble/string.h>
#include <stdint.h>

/* -- GPT Constants ---------------------------------------------- */

#define GPT_SECTOR_SIZE 512
#define GPT_SIGNATURE 0x5452415020494645ULL /* "EFI PART" */
#define GPT_REVISION_1_0 0x00010000
#define GPT_REVISION_2_0 0x00020000
#define GPT_MIN_HEADER_SIZE 92
#define GPT_MAX_PARTITION_ENTRIES 128
#define GPT_NAME_MAX_CHARS 36
#define GPT_PARTITION_TYPE_EMPTY                                                     \
  {                                                                                \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                            \
  }

/* -- CRC32 Implementation (ISO 3309 / ITU-T V.42) --------------- */

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
    crc32_table[i] = crc;
  }
  crc32_table_ready = 1;
}

static uint32_t crc32_compute(const void *data, size_t len) {
  if (!crc32_table_ready)
    crc32_init();

  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++)
    crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFF;
}

/* -- UTF-16LE to ASCII Conversion ------------------------------- */

/**
 * @brief Convert a UTF-16LE string to ASCII (lossy).
 *
 * Stops at the first UTF-16 NUL. Non-ASCII characters are
 * replaced with '?'. Always NUL-terminates the destination.
 *
 * @param src       Source UTF-16LE string (may be NULL).
 * @param dest      Destination buffer (must not be NULL).
 * @param dest_size Size of destination buffer including NUL space.
 * @return Number of characters written (excluding NUL).
 */
static size_t utf16le_to_ascii(const uint16_t *src, char *dest,
                               size_t dest_size) {
  if (!src || !dest || dest_size == 0)
    return 0;

  size_t i = 0;
  for (; i < dest_size - 1 && src[i] != 0; i++) {
    uint16_t c = src[i];
    if (c < 0x80)
      dest[i] = (char)c;
    else
      dest[i] = '?';
  }
  dest[i] = '\0';
  return i;
}

/* -- GPT Header Validation -------------------------------------- */

/**
 * @brief Validate GPT header fields against the specification.
 *
 * @param hdr        Pointer to the GPT header in a 512-byte buffer.
 * @param read_rc    Return value from the block-device read.
 * @return 0 on success, -1 on error (with printk diagnostics).
 */
static int gpt_validate_header(const GPT_Header *hdr, int read_rc) {
  if (read_rc != 0) {
    printk(KERN_ERR "GPT: block-device read failed (%d)\n", read_rc);
    return -1;
  }

  if (hdr->signature != GPT_SIGNATURE) {
    printk(KERN_ERR "GPT: invalid signature 0x%llx (expected 0x%llx)\n",
           (unsigned long long)hdr->signature,
           (unsigned long long)GPT_SIGNATURE);
    return -1;
  }

  if (hdr->revision != GPT_REVISION_1_0 &&
      hdr->revision != GPT_REVISION_2_0) {
    printk(KERN_ERR "GPT: unsupported revision 0x%08x\n", hdr->revision);
    return -1;
  }

  if (hdr->header_size < GPT_MIN_HEADER_SIZE ||
      hdr->header_size > GPT_SECTOR_SIZE) {
    printk(KERN_ERR "GPT: invalid header_size %u (expected %u..%u)\n",
           hdr->header_size, GPT_MIN_HEADER_SIZE, GPT_SECTOR_SIZE);
    return -1;
  }

  if (hdr->sizeof_partition_entry == 0 ||
      hdr->sizeof_partition_entry > 256) {
    printk(KERN_ERR "GPT: invalid sizeof_partition_entry %u\n",
           hdr->sizeof_partition_entry);
    return -1;
  }

  if (hdr->num_partition_entries == 0 ||
      hdr->num_partition_entries > GPT_MAX_PARTITION_ENTRIES) {
    printk(KERN_ERR "GPT: invalid num_partition_entries %u\n",
           hdr->num_partition_entries);
    return -1;
  }

  if (hdr->partition_entries_lba == 0) {
    printk(KERN_ERR "GPT: partition_entries_lba is 0\n");
    return -1;
  }

  if (hdr->first_usable_lba > hdr->last_usable_lba) {
    printk(KERN_ERR "GPT: first_usable_lba (%llu) > last_usable_lba (%llu)\n",
           (unsigned long long)hdr->first_usable_lba,
           (unsigned long long)hdr->last_usable_lba);
    return -1;
  }

  return 0;
}

/* -- Partition Validation ---------------------------------------- */

/**
 * @brief Check if a partition entry's type GUID is all zeros (empty).
 */
static int gpt_entry_is_empty(const GPT_Partition_Entry *entry) {
  static const uint8_t empty_guid[16] = GPT_PARTITION_TYPE_EMPTY;
  for (int b = 0; b < 16; b++) {
    if (entry->partition_type_guid[b] != empty_guid[b])
      return 0;
  }
  return 1;
}

/**
 * @brief Validate a non-empty partition entry's LBA bounds.
 *
 * @param entry  The partition entry to validate.
 * @param hdr    The GPT header (provides usable LBA range).
 * @return 1 if valid, 0 if invalid.
 */
static int gpt_validate_partition(const GPT_Partition_Entry *entry,
                                  const GPT_Header *hdr) {
  if (entry->first_lba > entry->last_lba) {
    printk(KERN_WARNING "GPT: partition first_lba (%llu) > last_lba (%llu)\n",
           (unsigned long long)entry->first_lba,
           (unsigned long long)entry->last_lba);
    return 0;
  }

  if (entry->first_lba < hdr->first_usable_lba) {
    printk(KERN_WARNING "GPT: partition first_lba (%llu) < header.first_usable_lba (%llu)\n",
           (unsigned long long)entry->first_lba,
           (unsigned long long)hdr->first_usable_lba);
    return 0;
  }

  if (entry->last_lba > hdr->last_usable_lba) {
    printk(KERN_WARNING "GPT: partition last_lba (%llu) > header.last_usable_lba (%llu)\n",
           (unsigned long long)entry->last_lba,
           (unsigned long long)hdr->last_usable_lba);
    return 0;
  }

  return 1;
}

/* -- Main GPT Initialization ------------------------------------ */

int gpt_init(gpt_partition_t *partitions, uint32_t capacity) {
  if (!partitions || capacity == 0) {
    printk(KERN_ERR "GPT: invalid partitions array or capacity\n");
    return -1;
  }

  /* -- Validate block device ---------------------------------- */
  if (!partitions->device) {
    printk(KERN_ERR "GPT: device is NULL\n");
    return -1;
  }
  if (!partitions->device->read) {
    printk(KERN_ERR "GPT: device->read is NULL\n");
    return -1;
  }
  if (!partitions->device->device) {
    printk(KERN_ERR "GPT: device->device is NULL\n");
    return -1;
  }

  printk(KERN_INFO "GPT: device=%p read=%p\n", partitions->device->device,
         (void *)(uint64_t)partitions->device->read);

  /* -- Read GPT header (LBA 1) ------------------------------- */
  uint8_t hdr_buf[GPT_SECTOR_SIZE];
  printk(KERN_INFO "GPT: BEFORE device->read() LBA=1 buffer=%p\n", hdr_buf);
  int read_rc =
      partitions->device->read(partitions->device->device, 1, hdr_buf);
  printk(KERN_INFO "GPT: AFTER device->read() result=%d\n", read_rc);

  const GPT_Header *hdr = (const GPT_Header *)hdr_buf;

  if (gpt_validate_header(hdr, read_rc) != 0)
    return -1;

  printk(KERN_INFO "GPT: signature OK, rev=0x%08x, entries=%u, entry_size=%u\n",
         hdr->revision, hdr->num_partition_entries,
         hdr->sizeof_partition_entry);

  /* -- Verify header CRC32 ----------------------------------- */
  {
    uint32_t saved_crc = hdr->header_crc32;
    /* Per spec, the CRC32 field is zeroed before computing the checksum. */
    uint8_t hdr_copy[GPT_SECTOR_SIZE];
    memcpy(hdr_copy, hdr_buf, GPT_SECTOR_SIZE);
    ((GPT_Header *)hdr_copy)->header_crc32 = 0;
    uint32_t computed_crc = crc32_compute(hdr_copy, hdr->header_size);
    if (computed_crc != saved_crc) {
      printk(KERN_WARNING
             "GPT: header CRC mismatch (computed=0x%08x stored=0x%08x)\n",
             computed_crc, saved_crc);
      /* Continue anyway — some firmware writes invalid CRCs. */
    } else {
      printk(KERN_INFO "GPT: header CRC32 OK (0x%08x)\n", saved_crc);
    }
  }

  /* -- Allocate buffer for partition entries ------------------ */
  uint64_t entry_count = hdr->num_partition_entries;
  uint64_t entry_size = hdr->sizeof_partition_entry;

  /* Overflow check: entry_count * entry_size must not wrap. */
  if (entry_count != 0 && entry_size > UINT64_MAX / entry_count) {
    printk(KERN_ERR "GPT: entry_count * entry_size overflow\n");
    return -1;
  }
  uint64_t total_size = entry_count * entry_size;

  uint64_t alloc_pages = (total_size + 0xFFF) / 0x1000;
  if (alloc_pages > UINT32_MAX) {
    printk(KERN_ERR "GPT: allocation too large (%llu pages)\n",
           (unsigned long long)alloc_pages);
    return -1;
  }

  uint8_t *entry_buf_phys =
      (uint8_t *)pmm_alloc_pages((uint32_t)alloc_pages);
  if (!entry_buf_phys) {
    printk(KERN_ERR "GPT: failed to allocate %llu pages for entries\n",
           (unsigned long long)alloc_pages);
    return -1;
  }

  uint8_t *entry_buf = (uint8_t *)phys_to_virt((uint64_t)entry_buf_phys);
  if (!entry_buf) {
    printk(KERN_ERR "GPT: phys_to_virt failed for entry buffer\n");
    pmm_free_pages((uint64_t)entry_buf_phys, (uint32_t)alloc_pages);
    return -1;
  }

  memset(entry_buf, 0, total_size);

  /* -- Read partition entries --------------------------------- */
  uint64_t sectors_to_read = (total_size + GPT_SECTOR_SIZE - 1) / GPT_SECTOR_SIZE;
  uint64_t base_lba = hdr->partition_entries_lba;

  for (uint64_t i = 0; i < sectors_to_read; i++) {
    /* Overflow check: base_lba + i must not wrap. */
    if (base_lba + i < base_lba) {
      printk(KERN_ERR "GPT: partition_entries_lba overflow\n");
      pmm_free_pages((uint64_t)entry_buf_phys, (uint32_t)alloc_pages);
      return -1;
    }
    read_rc = partitions->device->read(partitions->device->device,
                                       (uint32_t)(base_lba + i),
                                       entry_buf + (i * GPT_SECTOR_SIZE));
    if (read_rc != 0) {
      printk(KERN_ERR "GPT: failed to read entry sector %llu\n",
             (unsigned long long)i);
      pmm_free_pages((uint64_t)entry_buf_phys, (uint32_t)alloc_pages);
      return -1;
    }
  }

  /* -- Verify partition entry CRC32 -------------------------- */
  {
    uint32_t saved_crc = hdr->partition_entries_crc32;
    uint32_t computed_crc = crc32_compute(entry_buf, (uint32_t)total_size);
    if (computed_crc != saved_crc) {
      printk(KERN_WARNING
             "GPT: partition entries CRC mismatch (computed=0x%08x "
             "stored=0x%08x)\n",
             computed_crc, saved_crc);
      /* Continue anyway — some disks write invalid CRCs. */
    } else {
      printk(KERN_INFO "GPT: partition entries CRC32 OK (0x%08x)\n",
             saved_crc);
    }
  }

  /* -- Parse entries ----------------------------------------- */
  uint32_t partition_count = 0;

  for (uint64_t i = 0; i < entry_count; i++) {
    /* Bounds check: ensure the entry fits within the allocated buffer. */
    uint64_t offset = i * entry_size;
    if (offset + sizeof(GPT_Partition_Entry) > total_size) {
      printk(KERN_WARNING "GPT: entry %llu extends beyond buffer\n",
             (unsigned long long)i);
      break;
    }

    const GPT_Partition_Entry *entry =
        (const GPT_Partition_Entry *)(entry_buf + offset);

    if (gpt_entry_is_empty(entry))
      continue;

    /* Validate partition LBA bounds. */
    if (!gpt_validate_partition(entry, hdr))
      continue;

    /* Bounds check: must not overflow the destination array. */
    if (partition_count >= capacity) {
      printk(KERN_WARNING
             "GPT: too many partitions (capacity=%u), ignoring rest\n",
             capacity);
      break;
    }

    partitions[partition_count].first_lba = entry->first_lba;
    partitions[partition_count].last_lba = entry->last_lba;
    partitions[partition_count].device = partitions->device;

    /* Convert the UTF-16LE name to ASCII. */
    utf16le_to_ascii(entry->name, partitions[partition_count].name,
                     sizeof(partitions[partition_count].name));

    printk(KERN_INFO "GPT: partition %u: LBA %llu..%llu name=\"%s\"\n",
           partition_count, (unsigned long long)entry->first_lba,
           (unsigned long long)entry->last_lba,
           partitions[partition_count].name);

    partition_count++;
  }

  /* -- Cleanup ----------------------------------------------- */
  pmm_free_pages((uint64_t)entry_buf_phys, (uint32_t)alloc_pages);

  printk(KERN_OK "GPT: %u partition(s) found\n", partition_count);
  return (int)partition_count;
}
