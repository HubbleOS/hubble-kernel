#pragma once

#include <bootinfo/bootinfo.h>

typedef char symbol[];

void clear_bss(void);
void relocate_boot_info(BootInfo *bi);
