#pragma once

#include <_types/_int8_t.h>
#include <_types/_int16_t.h>
#include <_types/_int32_t.h>
#include <_types/_int64_t.h>
#include <_types/_uint8_t.h>
#include <_types/_uint16_t.h>
#include <_types/_uint32_t.h>
#include <_types/_uint64_t.h>

typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;

typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

#define INT8_MIN (-1 - 0x7f)
#define INT16_MIN (-1 - 0x7fff)
#define INT32_MIN (-1 - 0x7fffffff)
#define INT64_MIN (-1 - 0x7fffffffffffffff)

#define INT8_MAX (0x7f)
#define INT16_MAX (0x7fff)
#define INT32_MAX (0x7fffffff)
#define INT64_MAX (0x7fffffffffffffff)

#define UINT8_MAX (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffffu)
#define UINT64_MAX (0xffffffffffffffffu)

#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN

#define INT_LEAST8_MAX INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX

#define UINT_LEAST8_MAX UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define INT_FAST8_MIN INT8_MIN
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST64_MIN INT64_MIN

#define INT_FAST8_MAX INT8_MAX
#define INT_FAST16_MAX INT16_MAX
#define INT_FAST32_MAX INT32_MAX
#define INT_FAST64_MAX INT64_MAX

#define UINT_FAST8_MAX UINT8_MAX
#define UINT_FAST16_MAX UINT16_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define UINT_FAST64_MAX UINT64_MAX

// ###########################################################################
#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX
// ###########################################################################

#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#define PTRDIFF_MIN INTPTR_MIN
#define PTRDIFF_MAX INTPTR_MAX
#define SIZE_MAX UINTPTR_MAX

// wint
// ...
// end wint

#define INT8_C(val) (val)
#define INT16_C(val) (val)
#define INT32_C(val) (val)

#define UINT8_C(val) (val)
#define UINT16_C(val) (val)
#define UINT32_C(val) (val##U)

#if UINTPTR_MAX == UINT64_MAX
#define INT64_C(val) (val##L)
#define UINT64_C(val) (val##UL)
#define INTMAX_C(val) (val##L)
#define UINTMAX_C(val) (val##UL)
#else
#define INT64_C(val) (val##LL)
#define UINT64_C(val) (val##ULL)
#define INTMAX_C(val) (val##LL)
#define UINTMAX_C(val) (val##ULL)
#endif
