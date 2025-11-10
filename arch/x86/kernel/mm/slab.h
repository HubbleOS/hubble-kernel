#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Розміри об'єктів для стандартних slab кешів
#define SLAB_SIZE_8 8
#define SLAB_SIZE_16 16
#define SLAB_SIZE_32 32
#define SLAB_SIZE_64 64
#define SLAB_SIZE_128 128
#define SLAB_SIZE_256 256
#define SLAB_SIZE_512 512
#define SLAB_SIZE_1024 1024
#define SLAB_SIZE_2048 2048

// Магічне число для перевірки цілісності
#define SLAB_MAGIC 0x534C4142 // "SLAB" в ASCII

// Структура для одного slab'а (одна сторінка пам'яті)
typedef struct slab
{
	struct slab *next;     // Наступний slab у списку
	void *free_list;       // Список вільних об'єктів
	uint32_t free_count;   // Кількість вільних об'єктів
	uint32_t object_count; // Загальна кількість об'єктів
} slab_t;

// Структура для slab cache (керує множиною slab'ів одного розміру)
typedef struct slab_cache
{
	const char *name;   // Ім'я кешу (для debug)
	size_t object_size; // Розмір одного об'єкта
	size_t align;	    // Вирівнювання об'єктів

	slab_t *slabs_full;    // Повністю зайняті slab'и
	slab_t *slabs_partial; // Частково зайняті slab'и
	slab_t *slabs_free;    // Повністю вільні slab'и

	uint32_t total_objects; // Загальна кількість об'єктів
	uint32_t used_objects;	// Використані об'єкти
	uint32_t slab_count;	// Кількість slab'ів

	struct slab_cache *next; // Для списку всіх кешів
} slab_cache_t;

/**
 * Ініціалізує slab allocator та створює стандартні кеші
 */
void slab_init(void);

/**
 * Створює новий slab cache для об'єктів певного розміру
 * @param name Ім'я кешу (для debug)
 * @param size Розмір об'єкта
 * @param align Вирівнювання (0 = за замовчуванням)
 * @return Вказівник на створений cache або NULL при помилці
 */
slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align);

/**
 * Видаляє slab cache та звільняє всю пам'ять
 * @param cache Вказівник на cache
 */
void slab_cache_destroy(slab_cache_t *cache);

/**
 * Виділяє об'єкт із slab cache
 * @param cache Вказівник на cache
 * @return Вказівник на виділений об'єкт або NULL
 */
void *slab_alloc(slab_cache_t *cache);

/**
 * Звільняє об'єкт назад у slab cache
 * @param cache Вказівник на cache
 * @param ptr Вказівник на об'єкт
 */
void slab_free(slab_cache_t *cache, void *ptr);

/**
 * Загальний kmalloc - виділяє пам'ять оптимального розміру
 * @param size Розмір у байтах
 * @return Вказівник на виділену пам'ять або NULL
 */
void *kmalloc(size_t size);

/**
 * Загальний kmalloc - виділяє пам'ять оптимального розміру
 * @param ptr вказіник на памʼять
 * @param new_size новий розмір
 * @return Вказівник на виділену пам'ять або NULL
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * Загальний kfree - звільняє пам'ять
 * @param ptr Вказівник на пам'ять
 */
void kfree(void *ptr);

/**
 * kmalloc з вирівнюванням
 * @param size Розмір у байтах
 * @param align Вирівнювання (повинно бути степенем 2)
 * @return Вказівник на виділену пам'ять або NULL
 */
void *kmalloc_aligned(size_t size, size_t align);

/**
 * Отримує cache за розміром об'єкта
 * @param size Розмір об'єкта
 * @return Вказівник на відповідний cache або NULL
 */
slab_cache_t *slab_get_cache(size_t size);
