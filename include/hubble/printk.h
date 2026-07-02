#pragma once

/**
 * @brief printk — kernel logging interface.
 *
 * Provides printf-compatible formatted output with log-level prefixes
 * and colourisation. Backends can be plugged in for serial, framebuffer,
 * or full console output.
 */

#include <_cheader.h>
#include <hubble/color.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/* -- Log levels -------------------------------------------------------------
 */

#define KERN_EMERG "<0>"   /**< System is unusable.            */
#define KERN_ALERT "<1>"   /**< Action must be taken immediately. */
#define KERN_CRIT "<2>"    /**< Critical conditions.           */
#define KERN_ERR "<3>"     /**< Error conditions.              */
#define KERN_WARNING "<4>" /**< Warning conditions.            */
#define KERN_NOTICE "<5>"  /**< Normal but significant condition. */
#define KERN_INFO "<6>"    /**< Informational.                 */
#define KERN_DEBUG "<7>"   /**< Debug-level messages.          */
#define KERN_OK "<8>"      /**< Successful status messages.    */

#define PRINTK_BUFFER_SIZE                                                     \
  (16 * 1024) /**< Size of the internal ring buffer.                           \
               */

_Begin_C_Header;

/**
 * @brief Set the single-character raw output function (early boot).
 */
void printk_set_output(void (*fn)(char c));

/**
 * @brief Set the colour-aware single-character output function.
 */
void printk_set_color_output(void (*fn)(char c, color_t color));

/**
 * @brief Register a full console backend.
 *
 * Once registered, all buffered log contents are flushed to the
 * console. Replaces the simpler early-output backends.
 *
 * @param write_fn  Callback invoked with each log chunk.
 * @param user_data Opaque pointer passed to the callback.
 */
void printk_register_console(void (*write_fn)(const char *, size_t, void *),
                             void *user_data);

/**
 * @brief Unregister the console backend.
 */
void printk_unregister_console(void);

/**
 * @brief Formatted kernel print (printf-compatible).
 *
 * @param fmt Format string, optionally prefixed with a KERN_* level.
 */
void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief va_list variant of printk.
 */
void vprintk(const char *fmt, va_list args);

/**
 * @brief Copy buffered log output into a caller-provided buffer.
 *
 * @param dest    Destination buffer.
 * @param max_len Maximum number of bytes to copy.
 * @return Number of bytes actually copied.
 */
size_t printk_get_log(char *dest, size_t max_len);

/**
 * @brief Clear the log ring buffer.
 */
void printk_clear_log(void);

/**
 * @brief Return the number of bytes currently stored in the log buffer.
 */
size_t printk_log_size(void);

_End_C_Header;
