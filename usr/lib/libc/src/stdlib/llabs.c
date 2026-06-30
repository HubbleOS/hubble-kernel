/**
 * @file llabs.c
 * @brief Compute absolute value of a long long
 */

#include <stdlib.h>

long long llabs(long long x) { return x > 0 ? x : -x; }
