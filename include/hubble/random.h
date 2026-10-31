#pragma once

#include <stddef.h>

/**
 * @brief Fill @p buf with @p len random bytes
 *
 * RDRAND where the CPU has it, otherwise a generator seeded and stirred
 * with the time stamp counter. Unpredictable enough for stack canaries
 * and hash seeds; not a cryptographic source without RDRAND.
 */
void get_random_bytes(void *buf, size_t len);
