#pragma once

#include <stdint.h>

typedef struct
{
	uint64_t rip;	 // Адрес кода в user space
	uint64_t rsp;	 // User stack pointer
	uint64_t rflags; // Флаги (обычно 0x202 - IF enabled)
} usermode_context_t;

void enter_usermode(usermode_context_t *ctx);
