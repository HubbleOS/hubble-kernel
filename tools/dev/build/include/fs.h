/**
 * @file fs.h
 * @brief File system utilities for the build system
 *
 * Provides helper functions to manage directories, scan source files,
 * check file modification times, and generate object file paths.
 */

#pragma once

#include <time.h>

/// Maximum allowed path length
#define MAX_PATH 4096
/// Maximum number of source files
#define MAX_FILES 2048

/**
 * @struct SourceFile
 * @brief Stores information about a source file
 */
typedef struct
{
	/// Full path to the file
	char path[MAX_PATH];
	/// Last modification time
	time_t mtime;
} SourceFile;

/**
 * @brief Creates a directory and all intermediate directories
 * @param path Directory path to create
 * @return 0 on success, -1 on failure
 */
int mkdir_p(const char *path);

/**
 * @brief Gets the last modification time of a file
 * @param path File path
 * @return Last modification time (time_t) or 0 if file does not exist
 */
time_t get_mtime(const char *path);

/**
 * @brief Checks if the target needs to be rebuilt
 * @param src Source file path
 * @param obj Target file path (object or output)
 * @return 1 if rebuild is needed, 0 otherwise
 */
int needs_rebuild(const char *src, const char *obj);

/**
 * @brief Recursively scans a directory for files with a specific extension
 * @param dir Directory to scan
 * @param ext File extension to match (e.g., ".c", ".cpp", ".asm", ".tbl")
 * @param files Array to store found files
 * @param count Pointer to integer storing current count of found files
 * @param max Maximum number of files to store
 */
void scan_directory(const char *dir, const char *ext, SourceFile *files, int *count, int max);

/**
 * @brief Generates the object file path for a source file
 * @param src Source file path
 * @param src_dir Root source directory
 * @param build_dir Root build directory
 * @param obj Buffer to store the generated object path
 * @param obj_size Size of the buffer
 *
 * The generated object path preserves the relative path inside the build directory
 * and changes the file extension to ".o".
 */
void get_obj_path(const char *src, const char *src_dir, const char *build_dir, char *obj, size_t obj_size);
