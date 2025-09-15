#pragma once

typedef struct
{
	char iso[256];
	char arch[32];
	int mem;
	int smp;
	int debug_port;
} QemuConfig;

extern QemuConfig qemu_config;
