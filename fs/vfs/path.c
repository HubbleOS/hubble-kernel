/* -- Path parsing shared by the filesystems ------------------------
 * See path.h.
 * ------------------------------------------------------------------ */

#include "path.h"

#include <hubble/string.h>

int vfs_path_next(const char **path, char name[VFS_NAME_MAX]) {
  const char *p = *path;
  while (*p == '/')
    p++;
  const char *start = p;
  while (*p && *p != '/')
    p++;
  *path = p;

  size_t len = p - start;
  if (len >= VFS_NAME_MAX)
    return -1;
  memcpy(name, start, len);
  name[len] = '\0';
  return (int)len;
}

int vfs_path_last(const char *path, size_t *dir_len, char name[VFS_NAME_MAX]) {
  size_t end = strlen(path);
  while (end > 0 && path[end - 1] == '/')
    end--;
  size_t start = end;
  while (start > 0 && path[start - 1] != '/')
    start--;

  size_t len = end - start;
  if (len == 0 || len >= VFS_NAME_MAX)
    return -1;
  memcpy(name, path + start, len);
  name[len] = '\0';
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    return -1;
  *dir_len = start;
  return 0;
}
