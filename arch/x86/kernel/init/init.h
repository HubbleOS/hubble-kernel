#pragma once

#include <bootinfo/bootinfo.h>

typedef struct
{
	void (*cpu)(void);
	void (*memory)(BootInfo *bi);
	void (*filesystems)(void);
} KernelInit;

extern KernelInit init;
