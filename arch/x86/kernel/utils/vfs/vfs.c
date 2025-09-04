#include "utils/vfs/vfs.h"
#include "utils/vfs/vfs_standart_struct.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
// #include <errno.h>

// Список змонтованих ФС (поки що 1)
VFS_FS *root_fs = NULL;

// ==== Реалізація VFS API ==== //

bool vfs_mount(void *device, uint32_t start_lba, FileSystemType type)
{
    if (root_fs != NULL)
    {
        printf("VFS: already mounted\n");
        return false;
    }

    root_fs = malloc(sizeof(VFS_FS));
    memset(root_fs, 0, sizeof(VFS_FS));
    root_fs->type = type;

    // вибір драйвера
    switch (type)
    {
    case FS_FAT32:
        extern void fat32_init_vfs(VFS_FS * fs); // функція з fat32_vfs.c
        fat32_init_vfs(root_fs);
        break;
    default:
        printf("VFS: unsupported FS type %d\n", type);
        free(root_fs);
        root_fs = NULL;
        return false;
    }

    return root_fs->mount(root_fs, device, start_lba);
}

VFS_File *vfs_open(const char *path, int flags)

{

    if (!root_fs || !root_fs->open)
        return NULL;
    VFS_Node *node = root_fs->open(root_fs, path);

    printf("VFS: opening file %s\n", path);

    if (!node)
    {
        printf("VFS: file %s not found\n", path);

        if (flags & VFS_O_CREAT)
        {
            printf("VFS: creating file %s\n", path);
            node = vfs_create_file(path);
            if (!node)
            {
                return NULL;
                // return -EIO;
            }
        }
        else
        {
            return NULL;
            // return -ENOENT;
        }
    }
    else
    {
        // Якщо файл вже існує
        if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL))
        {
            return NULL;
            // return -EEXIST; // існує, а ми хочемо створити з EXCL
        }
    }

    // --- перевірка режимів ---
    int access_mode = flags & 0x03; // беремо тільки нижні біти
    switch (access_mode)
    {
    case VFS_O_RDONLY:
        if (!(node->mode & MODE_READ))
        {
            return NULL;
            // return -EACCES;
        }
        break;
    case VFS_O_WRONLY:
        if (!(node->mode & MODE_WRITE))
        {
            return NULL;
            // return -EACCES;
        }
        break;
    case VFS_O_RDWR:
        if (!(node->mode & MODE_READ) || !(node->mode & MODE_WRITE))
        {
            return NULL;
            // return -EACCES;
        }
        break;
    default:
        return NULL;
        // return -EINVAL;
    }

    // --- trunc ---
    // if ((flags & VFS_O_TRUNC) && (access_mode != VFS_O_RDONLY))
    //{
    //    node->size = 0;
    //    fs_truncate(node); // драйвер FS реально обрізає
    //}

    // --- append ---
    VFS_File *f = malloc(sizeof(VFS_File));

    f->node = node;
    f->flags = flags;
    f->pos = (flags & VFS_O_APPEND) ? node->size : 0;

    return f;
}

int vfs_read(VFS_File *file, void *buf, uint32_t size)
{
    if (!file || !file->node->fs || !file->node->fs->write)
        return -1;
    return file->node->fs->read(file, buf, size);
}

int vfs_write(VFS_File *file, const void *buf, uint32_t size)
{
    if (!file || !file->node->fs || !file->node->fs->write)
        return -1;
    return file->node->fs->write(file, buf, size);
}

VFS_Node *vfs_create_file(const char *path)
{
    if (!root_fs || !root_fs->create_file)
        return NULL;
    return root_fs->create_file(root_fs, path);
}

bool vfs_mkdir(const char *path)
{
    if (!root_fs || !root_fs->mkdir)
        return false;
    return root_fs->mkdir(root_fs, path);
}
Directory vfs_readdir(const char *path)
{
    if (!root_fs || !root_fs->readdir)
        return (Directory){0};
    return root_fs->readdir(root_fs, path);
}

bool vfs_unlink(const char *path)
{
    if (!root_fs || !root_fs->unlink)
        return false;
    return root_fs->unlink(root_fs, path);
}
