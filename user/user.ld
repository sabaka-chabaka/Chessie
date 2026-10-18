ENTRY(_start)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x40000000 + SIZEOF_HEADERS;

    .text   : { *(.text*) }     :text
    .rodata : { *(.rodata*) }   :text

    . = ALIGN(0x1000);
    .data   : { *(.data*) }     :data
    .bss    : { *(.bss*) *(COMMON) } :data

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}