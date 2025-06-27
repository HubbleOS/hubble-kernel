#ifndef _TEST_STDDEF_H_
#define _TEST_STDDEF_H_

// #ifdef __cplusplus
// extern "C"
// {
// #endif

typedef unsigned long size_t;

#define NULL ((void *)0)

#ifdef __cplusplus
extern "C"
{
#endif

	typedef long ptrdiff_t;
	typedef long rsize_t;
	typedef unsigned long uintptr_t;
	typedef long long intmax_t;
	typedef unsigned long long uintmax_t;
	typedef long long int64_t;
	typedef unsigned long long uint64_t;
	typedef int64_t int_least64_t;
	typedef uint64_t uint_least64_t;
	typedef int32_t int_fast64_t;
	typedef uint32_t uint_fast64_t;
	typedef int32_t int32_t;
	typedef unsigned int uint32_t;
	typedef int_least64_t int_least32_t;
	typedef uint_least64_t uint_least32_t;
	typedef int_fast64_t int_fast32_t;
	typedef int16_t int16_t;
	typedef unsigned int uint16_t;
	typedef int_least32_t int_least16_t;
	typedef uint_least32_t uint_least16_t;
	typedef int_fast32_t int_fast16_t;
	typedef int8_t int8_t;
	typedef unsigned char uint8_t;
	typedef int_least16_t int_least8_t;
	typedef uint_least16_t uint_least8_t;
	typedef int_fast16_t int_fast8_t;
	typedef long long int64_t;
	typedef unsigned long long uint64_t;
	typedef long long intmax_t;
	typedef unsigned long long uintmax_t;
	typedef long ptrdiff_t;
	typedef unsigned long size_t;
	typedef unsigned long rsize_t;
	typedef unsigned long uintptr_t;
	typedef long long intmax_t;
	typedef unsigned long long uintmax_t;

#ifdef __cplusplus
}
#endif

#endif