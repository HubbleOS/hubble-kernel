/**
 * @file initramfs.h
 * @brief initramfs (CPIO newc) filesystem interface
 */

#pragma once

#include <stdint.h>

/**
 * @brief Initialize the initramfs subsystem.
 *
 * Parses the CPIO newc archive from Limine module data
 * and registers it as a VFS filesystem at "/".
 *
 * @param data  Pointer to the CPIO archive in memory
 * @param size  Size of the archive in bytes
 * @return 0 on success, negative on failure
 */
int initramfs_init(void *data, uint64_t size);
