#pragma once

#include <stdint.h>

#include "aspace.h"

#define EI_NIDENT   16
#define ELFCLASS32  1
#define ELFDATA2LSB 1
#define EV_CURRENT  1
#define ET_EXEC     2
#define EM_386      3

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3

#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) Elf32_Phdr;

_Static_assert(sizeof(Elf32_Ehdr) == 52, "Elf32_Ehdr layout");
_Static_assert(sizeof(Elf32_Phdr) == 32, "Elf32_Phdr layout");

#define ELF_MAX_FILE      (256u * 1024u)
#define ELF_MAX_PHNUM     16
#define ELF_MAX_PAGES     1024
#define ELF_STACK_RESERVE (1u << 20)

#define ELF_EFORMAT  (-20)
#define ELF_ESEGMENT (-21)
#define ELF_ENOMEM   (-22)
#define ELF_ETOOBIG  (-23)
#define ELF_EIO      (-24)

int elf_load_mem(const void *img, uint32_t size, aspace_t *as, uint32_t *entry);

int elf_load(const char *path, aspace_t *as, uint32_t *entry);

const char *elf_strerror(int err);