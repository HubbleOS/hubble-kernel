#pragma once

#include <_cheader.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

#include <bootinfo/framebuffer.h>

// Log levels (как в Linux)
#define KERN_EMERG "<0>"   // System is unusable
#define KERN_ALERT "<1>"   // Action must be taken immediately
#define KERN_CRIT "<2>"	   // Critical conditions
#define KERN_ERR "<3>"	   // Error conditions
#define KERN_WARNING "<4>" // Warning conditions
#define KERN_NOTICE "<5>"  // Normal but significant condition
#define KERN_INFO "<6>"	   // Informational
#define KERN_DEBUG "<7>"   // Debug-level messages

// Log buffer size (16KB по умолчанию)
#define PRINTK_BUFFER_SIZE (16 * 1024)

_Begin_C_Header;

// Инициализация ранней печати (до терминала)
void early_printk_init(framebuffer_info_t *fb);

// Основная инициализация printk
void printk_init(framebuffer_info_t *fb);

// Регистрация консоли (терминала) для вывода
void printk_register_console(void (*write_fn)(const char *buf, size_t len, void *data), void *user_data);

// Отключение консоли
void printk_unregister_console(void);

// Основные функции печати
void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vprintk(const char *fmt, va_list args);

// Функции для работы с буфером логов
size_t printk_get_log_buffer(char *dest, size_t max_len);
void printk_clear_log_buffer(void);
size_t printk_get_log_size(void);

// Флаги printk
void printk_set_early_mode(bool enable); // Режим ранней загрузки
bool printk_is_early_mode(void);

_End_C_Header;
