#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <kprintf.h>
#include <string.h>
#include <tarfs.h>
#include <vfs.h>

#define TAR_BLOCK    512
#define TAR_PATH_MAX 256

typedef struct tar_entry {
    char           path[TAR_PATH_MAX];
    const uint8_t *data;
    uint32_t       size;
    uint8_t        is_dir;
} tar_entry_t;

typedef struct tarfs {
    tar_entry_t *entries;
    uint32_t     count;
} tarfs_t;

static tarfs_t tar;

static int parse_octal(const char *s, uint32_t n, uint32_t *out)
{
    uint32_t i = 0, v = 0, digits = 0;

    while (i < n && s[i] == ' ')
        i++;

    for (; i < n && s[i] >= '0' && s[i] <= '7'; i++) {
        if (v > (0xFFFFFFFFu >> 3))
            return -1;
        v = (v << 3) | (uint32_t)(s[i] - '0');
        digits++;
    }

    if (!digits)
        return -1;
    *out = v;
    return 0;
}

static int is_zero_block(const uint8_t *h)
{
    for (int i = 0; i < TAR_BLOCK; i++) {
        if (h[i])
            return 0;
    }
    return 1;
}

static int header_ok(const uint8_t *h)
{
    uint32_t sum = 0, want;

    for (int i = 0; i < TAR_BLOCK; i++)
        sum += (i >= 148 && i < 156) ? ' ' : h[i];

    if (parse_octal((const char *)h + 148, 8, &want) != 0)
        return 0;
    return sum == want;
}

static uint32_t field_len(const uint8_t *f, uint32_t max)
{
    uint32_t n = 0;

    while (n < max && f[n])
        n++;
    return n;
}

static int scan(const uint8_t *base, uint32_t size, tar_entry_t *out,
                uint32_t *count, uint32_t *skipped)
{
    uint32_t off = 0, n = 0, skip = 0;

    while (off + TAR_BLOCK <= size) {
        const uint8_t *h = base + off;

        if (is_zero_block(h))
            break;

        if (!header_ok(h)) {
            kprintf("tarfs: bad header checksum at offset %u (not a tar archive?)\n", off);
            return -1;
        }

        uint32_t fsize;

        if ((h[124] & 0x80) || parse_octal((const char *)h + 124, 12, &fsize) != 0) {
            kprintf("tarfs: unsupported size field at offset %u\n", off);
            return -1;
        }

        uint32_t data_off = off + TAR_BLOCK;

        if (fsize > size - data_off) {
            kprintf("tarfs: truncated archive: entry at offset %u wants %u bytes\n", off, fsize);
            return -1;
        }

        uint8_t type = h[156];
        int is_dir  = (type == '5');
        int is_file = (type == '0' || type == 0);

        if (is_file || is_dir) {
            char raw[260];
            uint32_t pos = 0;

            if (memcmp(h + 257, "ustar", 6) == 0) {
                uint32_t pl = field_len(h + 345, 155);

                if (pl) {
                    memcpy(raw, h + 345, pl);
                    pos = pl;
                    raw[pos++] = '/';
                }
            }

            uint32_t nl = field_len(h, 100);

            memcpy(raw + pos, h, nl);
            pos += nl;
            raw[pos] = '\0';

            if (pos && raw[pos - 1] == '/')
                is_dir = 1;

            char tmp[TAR_PATH_MAX];

            if (vfs_normalize(raw, tmp, sizeof(tmp)) == 0 && tmp[0]) {
                if (out) {
                    memcpy(out[n].path, tmp, strlen(tmp) + 1);
                    out[n].data   = base + data_off;
                    out[n].size   = is_dir ? 0 : fsize;
                    out[n].is_dir = (uint8_t)is_dir;
                }
                n++;
            } else {
                skip++;
            }
        } else {
            skip++;
        }

        off = data_off + ((fsize + TAR_BLOCK - 1) & ~(uint32_t)(TAR_BLOCK - 1));
    }

    *count = n;
    *skipped = skip;
    return 0;
}

static int tar_lookup(void *ctx, const char *path, void **node, vfs_stat_t *st)
{
    tarfs_t *fs = ctx;
    size_t plen = strlen(path);
    int dir = 0;

    if (plen == 0) {
        *node = NULL;
        st->type = VFS_DIR;
        st->size = 0;
        return 0;
    }

    for (uint32_t i = 0; i < fs->count; i++) {
        tar_entry_t *e = &fs->entries[i];

        if (strcmp(e->path, path) == 0) {
            if (!e->is_dir) {
                *node = e;
                st->type = VFS_FILE;
                st->size = e->size;
                return 0;
            }
            dir = 1;
        } else if (strncmp(e->path, path, plen) == 0 && e->path[plen] == '/') {
            dir = 1;
        }
    }

    if (dir) {
        *node = NULL;
        st->type = VFS_DIR;
        st->size = 0;
        return 0;
    }
    return VFS_ENOENT;
}

static int32_t tar_read(void *ctx, void *node, uint32_t off, void *buf, uint32_t len)
{
    (void)ctx;

    const tar_entry_t *e = node;

    if (off >= e->size)
        return 0;

    uint32_t n = e->size - off;

    if (n > len)
        n = len;

    memcpy(buf, e->data + off, n);
    return (int32_t)n;
}

static int child_of(const char *dir, size_t dlen, const char *path,
                    const char **name, size_t *nlen, int *nested)
{
    const char *rest;

    if (dlen == 0) {
        rest = path;
    } else {
        if (strncmp(path, dir, dlen) != 0 || path[dlen] != '/')
            return 0;
        rest = path + dlen + 1;
    }

    if (*rest == '\0')
        return 0;

    size_t n = 0;

    while (rest[n] && rest[n] != '/')
        n++;

    *name   = rest;
    *nlen   = n;
    *nested = (rest[n] == '/');
    return 1;
}

static int tar_readdir(void *ctx, const char *dir, uint32_t index, vfs_dirent_t *out)
{
    tarfs_t *fs = ctx;
    size_t dlen = strlen(dir);
    uint32_t seen = 0;

    for (uint32_t i = 0; i < fs->count; i++) {
        const tar_entry_t *e = &fs->entries[i];
        const char *name;
        size_t nlen;
        int nested;

        if (!child_of(dir, dlen, e->path, &name, &nlen, &nested))
            continue;

        int dup = 0;

        for (uint32_t j = 0; j < i && !dup; j++) {
            const char *n2;
            size_t l2;
            int nest2;

            if (child_of(dir, dlen, fs->entries[j].path, &n2, &l2, &nest2) &&
                l2 == nlen && memcmp(n2, name, nlen) == 0)
                dup = 1;
        }
        if (dup)
            continue;

        if (seen++ != index)
            continue;

        if (nlen > VFS_NAME_MAX)
            nlen = VFS_NAME_MAX;

        memcpy(out->name, name, nlen);
        out->name[nlen] = '\0';

        if (nested || e->is_dir) {
            out->type = VFS_DIR;
            out->size = 0;
        } else {
            out->type = VFS_FILE;
            out->size = e->size;
        }
        return 0;
    }

    return 1;
}

static const fs_ops_t tar_ops = {
    .lookup  = tar_lookup,
    .read    = tar_read,
    .readdir = tar_readdir,
};

int tarfs_mount(const void *data, uint32_t size)
{
    uint32_t count, skipped;

    if (tar.entries) {
        kprintf("tarfs: already mounted\n");
        return -1;
    }

    if (scan(data, size, NULL, &count, &skipped) != 0)
        return -1;

    tar.entries = NULL;
    tar.count = 0;

    if (count) {
        tar.entries = kmalloc(count * sizeof(tar_entry_t));
        if (!tar.entries) {
            kprintf("tarfs: out of memory for %u entries\n", count);
            return -1;
        }
        if (scan(data, size, tar.entries, &count, &skipped) != 0)
            return -1;
        tar.count = count;
    }

    kprintf("tarfs: %u entries", count);
    if (skipped)
        kprintf(", %u skipped (unsupported type or path)", skipped);
    kprintf("\n");

    return vfs_mount_root(&tar_ops, &tar);
}