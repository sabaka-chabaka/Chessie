#pragma once

#include <stddef.h>
#include <stdint.h>

#define VFS_PATH_MAX 256
#define VFS_NAME_MAX 255
#define VFS_MAX_FDS  32
#define VFS_FIRST_FD 3

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

#define VFS_ENOENT  (-1)
#define VFS_EISDIR  (-2)
#define VFS_ENOTDIR (-3)
#define VFS_ENFILE  (-4)
#define VFS_EBADF   (-5)
#define VFS_EINVAL  (-6)
#define VFS_ENODEV  (-7)

typedef enum { VFS_FILE = 1, VFS_DIR = 2 } vfs_type_t;

typedef struct vfs_stat {
    vfs_type_t type;
    uint32_t   size;
} vfs_stat_t;

typedef struct vfs_dirent {
    char       name[VFS_NAME_MAX + 1];
    vfs_type_t type;
    uint32_t   size;
} vfs_dirent_t;

typedef struct fs_ops {
    int     (*lookup)(void *ctx, const char *path, void **node, vfs_stat_t *st);
    int32_t (*read)(void *ctx, void *node, uint32_t off, void *buf, uint32_t len);
    int     (*readdir)(void *ctx, const char *path, uint32_t index, vfs_dirent_t *out);
} fs_ops_t;

int  vfs_mount_root(const fs_ops_t *ops, void *ctx);

int  vfs_normalize(const char *path, char *out, size_t max);

int      vfs_stat(const char *path, vfs_stat_t *st);
int      vfs_open(const char *path);
int32_t  vfs_read(int fd, void *buf, uint32_t len);
int32_t  vfs_seek(int fd, int32_t off, int whence);
int      vfs_close(int fd);
int      vfs_readdir(const char *path, uint32_t index, vfs_dirent_t *out);

uint32_t    vfs_open_files(void);
const char *vfs_strerror(int err);