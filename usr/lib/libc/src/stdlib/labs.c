/**
 * @file labs.c
 * @brief Compute absolute value of a long
 */

#include <stdlib.h>

long labs(long x) { return x > 0 ? x : -x; }
