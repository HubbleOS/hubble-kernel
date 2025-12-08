/**
 * @file log.h
 * @brief
 *
 */

#pragma once

/**
 * @brief
 *
 * @param log_dir
 */
void log_init(const char *log_dir);

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

#define LOG_INFO(...) log_message("INFO", __VA_ARGS__);
#define LOG_WARN(...) log_message("WARN", __VA_ARGS__);
#define LOG_ERROR(...) log_message("ERROR", __VA_ARGS__);
#define LOG_SUCCESS(...) log_message("SUCCESS", __VA_ARGS__);
