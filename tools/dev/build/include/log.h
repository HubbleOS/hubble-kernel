/**
 * @file log.h
 * @brief
 *
 */

#pragma once

#include <stdio.h>

/**
 * @brief
 *
 * @param log_dir
 */
void log_init(const char *log_dir);
// log.h
void log_init_file(const char *log_file_path);

/**
 * @brief
 *
 */
void log_close(void);

/**
 * @brief
 *
 * @param level
 * @param format
 * @param ...
 */
void log_message(const char *level, const char *format, ...);

#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_WHITE "\x1b[37m"

#define LOG_INFO(...) log_message("INFO", __VA_ARGS__)
#define LOG_WARN(...) log_message("WARN", __VA_ARGS__)
#define LOG_ERROR(...) log_message("ERROR", __VA_ARGS__)
#define LOG_SUCCESS(...) log_message("SUCCESS", __VA_ARGS__)

extern FILE *log_file;
