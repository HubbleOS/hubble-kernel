/* -- Path parsing shared by the filesystems ------------------------
 * Every filesystem gets paths relative to its mountpoint and walks
 * them one component at a time:
 *
 *   char name[VFS_NAME_MAX];
 *   int len;
 *   while ((len = vfs_path_next(&path, name)) > 0)
 *     dir = lookup(dir, name);
 *   if (len < 0)
 *     return -ENAMETOOLONG;
 *
 * Creating or removing works on the parent plus the last name:
 *
 *   size_t dir_len;
 *   if (vfs_path_last(path, &dir_len, name) < 0)
 *     return -EINVAL;
 *   // the parent is the first dir_len bytes of path
 * ------------------------------------------------------------------ */

#pragma once

#include <stddef.h>

/** Longest file name plus its NUL. */
#define VFS_NAME_MAX 256

/**
 * @brief The next component of @p *path into @p name, advancing @p *path
 *
 * Repeated and leading slashes are skipped. "." and ".." are returned as
 * they are; what they mean is up to the filesystem.
 *
 * @return The name's length, 0 at the end of the path, -1 if it is
 *         VFS_NAME_MAX or longer
 */
int vfs_path_next(const char **path, char name[VFS_NAME_MAX]);

/**
 * @brief Split @p path into its parent and last component
 *
 * Trailing slashes are ignored: "a/b/" gives parent "a/" and name "b".
 *
 * @param dir_len Set to the length of the parent's part of @p path
 * @return 0, or -1 if there is no last component, it is "." or "..", or
 *         it is too long
 */
int vfs_path_last(const char *path, size_t *dir_len, char name[VFS_NAME_MAX]);
