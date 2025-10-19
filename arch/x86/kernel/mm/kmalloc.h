#pragma once

#include <stdint.h>
#include <stddef.h>

// Инициализация heap
void kmalloc_init(void);

// Основные функции
void *kmalloc(size_t size);
void *kzalloc(size_t size); // Обнулённый
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);

// Aligned allocation
void *kmalloc_aligned(size_t size, size_t alignment);

// Статистика
void kmalloc_stats(void);
