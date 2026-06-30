/**
 * @file msr.h
 * @brief x86 Model-Specific Register (MSR) read/write helpers
 *
 * Provides inline wrappers for the RDMSR and WRMSR instructions
 * to access CPU model-specific registers.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Read a 64-bit MSR
 *
 * @param msr The MSR index
 * @return The 64-bit value of the MSR
 */
static inline uint64_t rdmsr(uint32_t msr)
{
	uint32_t lo, hi;
	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Write a 64-bit MSR
 *
 * @param msr The MSR index
 * @param v   The 64-bit value to write
 */
static inline void wrmsr(uint32_t msr, uint64_t v)
{
	uint32_t lo = v, hi = v >> 32;
	__asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}
