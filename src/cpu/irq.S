.section .text

.macro IRQ n, vec
.global irq\n
irq\n:
    push $0
    push $\vec
    jmp irq_common
.endm

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common:
    pusha

    mov %ds, %ax
    push %eax

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    cld
    push %esp
    call irq_handler
    add $4, %esp

    pop %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    popa
    add $8, %esp
    iret

.section .data
.global irq_stub_table
irq_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .long irq\n
.endr