#include <stddef.h>
#include <stdint.h>

#include <aspace.h>
#include <elf.h>
#include <heap.h>
#include <pmm.h>
#include <string.h>
#include <vfs.h>
#include <vmm.h>

#define LOAD_LIMIT (VMM_USER_END - ELF_STACK_RESERVE)

static const uint8_t elf_magic[4] = { 0x7F, 'E', 'L', 'F' };

int elf_load_mem(const void *img, uint32_t size, aspace_t *as, uint32_t *entry_out)
{
    const uint8_t *base = img;
    const Elf32_Ehdr *eh = img;

    if (size < sizeof(*eh))
        return ELF_EFORMAT;
    if (memcmp(eh->e_ident, elf_magic, 4) != 0 || eh->e_ident[4] != ELFCLASS32 ||
        eh->e_ident[5] != ELFDATA2LSB || eh->e_ident[6] != EV_CURRENT)
        return ELF_EFORMAT;
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_386 || eh->e_version != EV_CURRENT)
        return ELF_EFORMAT;
    if (eh->e_phentsize != sizeof(Elf32_Phdr) || eh->e_phnum == 0 || eh->e_phnum > ELF_MAX_PHNUM)
        return ELF_EFORMAT;

    uint32_t ph_bytes = (uint32_t)eh->e_phnum * sizeof(Elf32_Phdr);

    if (eh->e_phoff > size || ph_bytes > size - eh->e_phoff)
        return ELF_EFORMAT;

    const Elf32_Phdr *ph = (const Elf32_Phdr *)(base + eh->e_phoff);

    uint32_t loads = 0, total_pages = 0;
    int entry_ok = 0;

    for (uint32_t i = 0; i < eh->e_phnum; i++) {
        const Elf32_Phdr *p = &ph[i];

        if (p->p_type == PT_INTERP || p->p_type == PT_DYNAMIC)
            return ELF_EFORMAT;
        if (p->p_type != PT_LOAD || p->p_memsz == 0)
            continue;

        if (p->p_filesz > p->p_memsz)
            return ELF_ESEGMENT;
        if (p->p_offset > size || p->p_filesz > size - p->p_offset)
            return ELF_ESEGMENT;
        if (p->p_vaddr < VMM_USER_BASE)
            return ELF_ESEGMENT;

        uint32_t end = p->p_vaddr + p->p_memsz;

        if (end < p->p_vaddr || end > LOAD_LIMIT)
            return ELF_ESEGMENT;

        total_pages += ((end + PAGE_SIZE - 1) >> PAGE_SHIFT) - (p->p_vaddr >> PAGE_SHIFT);
        if (total_pages > ELF_MAX_PAGES)
            return ELF_ETOOBIG;

        if ((p->p_flags & PF_X) && eh->e_entry >= p->p_vaddr && eh->e_entry - p->p_vaddr < p->p_filesz)
            entry_ok = 1;
        loads++;
    }

    if (!loads || !entry_ok)
        return ELF_EFORMAT;

    for (uint32_t i = 0; i < eh->e_phnum; i++) {
        const Elf32_Phdr *p = &ph[i];

        if (p->p_type != PT_LOAD || p->p_memsz == 0)
            continue;

        uint32_t flags = VMM_USER | ((p->p_flags & PF_W) ? VMM_WRITE : 0);
        uint32_t end = p->p_vaddr + p->p_memsz;

        for (uint32_t va = p->p_vaddr & ~0xFFFu; va < end; va += PAGE_SIZE) {
            uint32_t phys = aspace_phys(as, va);

            if (!phys) {
                if (aspace_map_new(as, va, flags) != 0)
                    return ELF_ENOMEM;
            } else if (flags & VMM_WRITE) {
                aspace_map(as, va, phys & ~0xFFFu, VMM_USER | VMM_WRITE);
            }
        }

        uint32_t src = p->p_offset, dst = p->p_vaddr, left = p->p_filesz;

        while (left) {
            uint32_t phys = aspace_phys(as, dst);
            uint32_t chunk = PAGE_SIZE - (dst & 0xFFF);

            if (chunk > left)
                chunk = left;
            memcpy((void *)phys, base + src, chunk);
            src += chunk;
            dst += chunk;
            left -= chunk;
        }
    }

    *entry_out = eh->e_entry;
    return 0;
}

int elf_load(const char *path, aspace_t *as, uint32_t *entry)
{
    vfs_stat_t st;
    int r = vfs_stat(path, &st);

    if (r < 0)
        return r;
    if (st.type == VFS_DIR)
        return VFS_EISDIR;
    if (st.size == 0)
        return ELF_EFORMAT;
    if (st.size > ELF_MAX_FILE)
        return ELF_ETOOBIG;

    int fd = vfs_open(path);

    if (fd < 0)
        return fd;

    uint8_t *buf = kmalloc(st.size);

    if (!buf) {
        vfs_close(fd);
        return ELF_ENOMEM;
    }

    uint32_t got = 0;
    int32_t n = 1;

    while (got < st.size && (n = vfs_read(fd, buf + got, st.size - got)) > 0)
        got += (uint32_t)n;
    vfs_close(fd);

    r = (got == st.size) ? elf_load_mem(buf, got, as, entry) : ELF_EIO;
    kfree(buf);
    return r;
}

const char *elf_strerror(int err)
{
    switch (err) {
    case ELF_EFORMAT:  return "not a valid static i386 ELF executable";
    case ELF_ESEGMENT: return "bad segment (outside file or allowed addresses)";
    case ELF_ENOMEM:   return "out of memory while loading";
    case ELF_ETOOBIG:  return "program too large";
    case ELF_EIO:      return "short read";
    default:           return "error";
    }
}