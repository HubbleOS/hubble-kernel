#pragma once
#include <stdint.h>

// Структура данных для каждого CPU
typedef struct
{
	uint64_t kernel_stack; // Kernel stack для этого CPU
	uint64_t user_stack;   // User stack (сохраняется при syscall)
	uint32_t cpu_id;       // ID процессора
	void *current_task;    // Указатель на текущую задачу
	uint64_t scratch[4];   // Временные данные
} __attribute__((packed)) percpu_data_t;

// Глобальный массив per-CPU данных
extern percpu_data_t percpu_data[256]; // Максимум 256 CPU

void percpu_init(void);
uint32_t get_cpu_id(void);
