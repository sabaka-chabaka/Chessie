#include <stddef.h>
#include <stdint.h>

#include <string.h>
#include <sync.h>
#include <vfs.h>

typedef struct file {
    uint8_t  used;
    void    *node;
    uint32_t size;
    uint32_t pos;
} file_t;

static const fs_ops_t *root_ops;
static void           *root_ctx;

static file_t  files[VFS_MAX_FDS];
static mutex_t vfs_lock;

int vfs_mount_root(const fs_ops_t *ops, void *ctx)
{
    if (!ops || !ops->lookup || !ops->read || !ops->readdir)
        return VFS_EINVAL;

    mutex_init(&vfs_lock);
    root_ops = ops;
    root_ctx = ctx;
    return 0;
}

int vfs_normalize(const char *path, char *out, size_t max)
{
    size_t len = 0;
    const char *p = path;

    out[0] = '\0';

    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        const char *start = p;

        while (*p && *p != '/')
            p++;

        size_t n = (size_t)(p - start);

        if (n == 1 && start[0] == '.')
            continue;

        if (n == 2 && start[0] == '.' && start[1] == '.') {
            while (len && out[len - 1] != '/')
                len--;
            if (len)
                len--;
            out[len] = '\0';
            continue;
        }

        size_t need = n + (len ? 1 : 0);

        if (len + need + 1 > max)
            return -1;

        if (len)
            out[len++] = '/';
        memcpy(out + len, start, n);
        len += n;
        out[len] = '\0';
    }

    return 0;
}

static file_t *getfile(int fd)
{
    if (fd < VFS_FIRST_FD || fd >= VFS_MAX_FDS || !files[fd].used)
        return NULL;
    return &files[fd];
}

int vfs_stat(const char *path, vfs_stat_t *st)
{
    char p[VFS_PATH_MAX];
    void *node;

    if (!root_ops)
        return VFS_ENODEV;
    if (!path || !st || vfs_normalize(path, p, sizeof(p)) != 0)
        return VFS_EINVAL;

    return root_ops->lookup(root_ctx, p, &node, st);
}

int vfs_open(const char *path)
{
    char p[VFS_PATH_MAX];
    void *node;
    vfs_stat_t st;

    if (!root_ops)
        return VFS_ENODEV;
    if (!path || vfs_normalize(path, p, sizeof(p)) != 0)
        return VFS_EINVAL;

    int r = root_ops->lookup(root_ctx, p, &node, &st);

    if (r != 0)
        return r;
    if (st.type == VFS_DIR)
        return VFS_EISDIR;

    int fd = VFS_ENFILE;

    mutex_lock(&vfs_lock);
    for (int i = VFS_FIRST_FD; i < VFS_MAX_FDS; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            files[i].node = node;
            files[i].size = st.size;
            files[i].pos  = 0;
            fd = i;
            break;
        }
    }
    mutex_unlock(&vfs_lock);

    return fd;
}

int32_t vfs_read(int fd, void *buf, uint32_t len)
{
    if (!buf && len)
        return VFS_EINVAL;

    int32_t r;

    mutex_lock(&vfs_lock);

    file_t *f = getfile(fd);

    if (!f) {
        r = VFS_EBADF;
    } else {
        r = root_ops->read(root_ctx, f->node, f->pos, buf, len);
        if (r > 0)
            f->pos += (uint32_t)r;
    }

    mutex_unlock(&vfs_lock);
    return r;
}

int32_t vfs_seek(int fd, int32_t off, int whence)
{
    int32_t r;

    mutex_lock(&vfs_lock);

    file_t *f = getfile(fd);

    if (!f) {
        r = VFS_EBADF;
    } else if (whence != VFS_SEEK_SET && whence != VFS_SEEK_CUR && whence != VFS_SEEK_END) {
        r = VFS_EINVAL;
    } else {
        int64_t base = whence == VFS_SEEK_SET ? 0
                     : whence == VFS_SEEK_CUR ? (int64_t)f->pos
                                              : (int64_t)f->size;
        int64_t target = base + off;

        if (target < 0 || target > (int64_t)f->size) {
            r = VFS_EINVAL;
        } else {
            f->pos = (uint32_t)target;
            r = (int32_t)target;
        }
    }

    mutex_unlock(&vfs_lock);
    return r;
}

int vfs_close(int fd)
{
    int r = 0;

    mutex_lock(&vfs_lock);

    file_t *f = getfile(fd);

    if (!f)
        r = VFS_EBADF;
    else
        f->used = 0;

    mutex_unlock(&vfs_lock);
    return r;
}

int vfs_readdir(const char *path, uint32_t index, vfs_dirent_t *out)
{
    char p[VFS_PATH_MAX];
    void *node;
    vfs_stat_t st;

    if (!root_ops)
        return VFS_ENODEV;
    if (!path || !out || vfs_normalize(path, p, sizeof(p)) != 0)
        return VFS_EINVAL;

    int r = root_ops->lookup(root_ctx, p, &node, &st);

    if (r != 0)
        return r;
    if (st.type != VFS_DIR)
        return VFS_ENOTDIR;

    return root_ops->readdir(root_ctx, p, index, out);
}

uint32_t vfs_open_files(void)
{
    uint32_t n = 0;

    mutex_lock(&vfs_lock);
    for (int i = VFS_FIRST_FD; i < VFS_MAX_FDS; i++)
        n += files[i].used;
    mutex_unlock(&vfs_lock);

    return n;
}

const char *vfs_strerror(int err)
{
    switch (err) {
    case VFS_ENOENT:  return "no such file or directory";
    case VFS_EISDIR:  return "is a directory";
    case VFS_ENOTDIR: return "not a directory";
    case VFS_ENFILE:  return "too many open files";
    case VFS_EBADF:   return "bad file descriptor";
    case VFS_EINVAL:  return "invalid argument";
    case VFS_ENODEV:  return "no filesystem mounted";
    default:          return "error";
    }
}