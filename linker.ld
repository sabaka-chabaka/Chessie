ENTRY(_start)

SECTIONS
{
    . = 1M;
    kernel_start = .;

    .multiboot : { KEEP(*(.multiboot)) }
    .text   : ALIGN(4K) { *(.text*) }
    .rodata : ALIGN(4K) { *(.rodata*) }
    .data   : ALIGN(4K) { *(.data*) }
    .bss    : ALIGN(4K) { *(COMMON) *(.bss*) }

    . = ALIGN(4K);
    kernel_end = .;

    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}