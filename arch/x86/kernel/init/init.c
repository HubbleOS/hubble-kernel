#include "init.h"

extern void init_cpu(void);
static void init_cpu_wrapper(void) { init_cpu(); }

extern void init_memory(BootInfo *bi);
static void init_memory_wrapper(BootInfo *bi) { init_memory(bi); }

extern void init_filesystems(void);
static void init_fs_wrapper(void) { init_filesystems(); }

KernelInit init = {
    .cpu = init_cpu_wrapper,
    .memory = init_memory_wrapper,
    .filesystems = init_fs_wrapper,
};
