#pragma once

#include <_cheader.h>

_Begin_C_Header;

#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, l) __builtin_va_arg(v, l)
#define va_copy(d, s) __builtin_va_copy(d, s)

typedef __builtin_va_list va_list;
typedef __builtin_va_list __isoc_va_list;

_End_C_Header;
