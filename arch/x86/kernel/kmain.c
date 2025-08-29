#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"
#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/ata/ata.h"
#include "utils/gpt/gpt.h"
#include "utils/gpt/gpt_struct.h"

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
    printf("%s\n", buffer);
    printf("FAT32 init done\n");
    // printf("Enter 1 to list files, 2 to delete file, 3 to create directory, 4 to delete dir\n");
    // while (1)
    // {
    //     int c = getchar();
    //     if (c == '1')
    //     {
    //         char foldername11[1024];
    //         scanf("%s", foldername11);
    //         Directory dir = fat32_list_files_from_path(foldername11);
    //         for (int i = 0; i < dir.count; i++)
    //         {
    //             printf("%d ", i);
    //             printf("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
    //         }
    //     }
    //     if (c == '2')
    //     {
    //         // create file
    //         char foldername11[1024];
    //         scanf("%s", foldername11);
    //         fat32_create_file(foldername11);
    //     }
    //     if (c == '3')
    //     {
    //         char foldername11[1024];
    //         scanf("%s", foldername11);
    //         fat32_delete_file(foldername11);
    //     }
    //     if (c == '4')
    //     {
    //         char foldername11[1024];
    //         scanf("%s", foldername11);
    //         fat32_create_directory(foldername11);
    //     }
    //     if (c == '5')
    //     {
    //         char foldername11[1024];
    //         scanf("%s", foldername11);
    //         fat32_delete_directory(foldername11);
    //     }

    //     printf("char:%d \n", c);
    // }
    while (1)
    {
        ;
    }
}
