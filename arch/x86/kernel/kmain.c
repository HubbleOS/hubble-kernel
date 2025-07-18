#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"
#include "fat.h"
#include "utils/ata.h"
#include "utils/gpt/gpt.h"
#include "utils/gpt/gpt_struct.h"

// #include <stdint.h>
#include <stdio.h>
#include <string.h>

static gpt_partition_t partitions[128];
extern void os_main(framebuffer_info_t *fb);
extern void libc_init(void);

extern uint32_t root_cluster;

void kernel_main(BootInfo *bi)
{

    heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
    framebuffer_info_t *fb = bi->framebuffer;

    init_font(fb);
    libc_init();
    printf("GPT init\n");

    gpt_init(partitions);

    printf("FAT32 init at LBA %d\n", partitions[0].first_lba);
    fat32_init_from_lba(partitions[0]);
    char buffer[1024];
    printf("root cluster: %d\n", root_cluster);

    while (1)
        ;
}
