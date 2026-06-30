/**
 * @file io.h
 * @brief x86 port I/O helpers (in/out instructions)
 *
 * Provides inline wrappers for the x86 IN and OUT instruction
 * family to read and write 8-, 16-, and 32-bit values from
 * I/O ports.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Write 8 bits to an I/O port
 *
 * @param port The port number
 * @param val  The value to write
 */
static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Write 16 bits to an I/O port
 *
 * @param port The port number
 * @param val  The value to write
 */
static inline void outw(uint16_t port, uint16_t val)
{
	__asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Read 8 bits from an I/O port
 *
 * @param port The port number
 * @return The value read
 */
static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/**
 * @brief Read 16 bits from an I/O port
 *
 * @param port The port number
 * @return The value read
 */
static inline uint16_t inw(uint16_t port)
{
	uint16_t ret;
	__asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/**
 * @brief Write 32 bits to an I/O port
 *
 * @param port The port number
 * @param val  The value to write
 */
static inline void outl(uint16_t port, uint32_t val)
{
	__asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Read 32 bits from an I/O port
 *
 * @param port The port number
 * @return The value read
 */
static inline uint32_t inl(uint16_t port)
{
	uint32_t ret;
	__asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}
