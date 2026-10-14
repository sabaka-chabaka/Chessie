#include <stddef.h>
#include <stdint.h>

#include <initrd.h>
#include <kprintf.h>
#include <multiboot.h>
#include <string.h>
#include <tarfs.h>
#include <vmm.h>

static uint32_t mod_start;
static uint32_t mod_end;

static int contains(const char *s, const char *word)
{
    size_t n = strlen(word);

    for (; *s; s++) {
        if (strncmp(s, word, n) == 0)
            return 1;
    }
    return 0;
}

int initrd_init(const multiboot_info_t *mbi)
{
    if (!(mbi->flags & MULTIBOOT_INFO_MODS) || mbi->mods_count == 0) {
        kprintf("initrd: no modules from the bootloader\n");
        return -1;
    }

    const multiboot_module_t *m = (const multiboot_module_t *)mbi->mods_addr;

    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        const char *name = m[i].string ? (const char *)m[i].string : "";

        if (!contains(name, "initrd"))
            continue;

        if (m[i].mod_end <= m[i].mod_start) {
            kprintf("initrd: empty module\n");
            return -1;
        }
        if (m[i].mod_end > VMM_IDENTITY_LIMIT) {
            kprintf("initrd: module at %p is above the mapped area\n", (void *)m[i].mod_start);
            return -1;
        }

        mod_start = m[i].mod_start;
        mod_end   = m[i].mod_end;

        kprintf("initrd: '%s' at %p, %u bytes\n",
                name, (void *)mod_start, mod_end - mod_start);

        return tarfs_mount((const void *)mod_start, mod_end - mod_start);
    }

    kprintf("initrd: no module named 'initrd' among %u\n", mbi->mods_count);
    return -1;
}

int initrd_range(uint32_t *start, uint32_t *end)
{
    if (!mod_end)
        return -1;

    *start = mod_start;
    *end   = mod_end;
    return 0;
}