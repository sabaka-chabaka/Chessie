.section .text
.global _start
.type _start, @function

_start:
    xor %ebp, %ebp
    mov (%esp), %eax
    lea 4(%esp), %edx
    and $-16, %esp
    sub $8, %esp
    push %edx
    push %eax
    call main

    mov %eax, %ebx
    mov $2, %eax
    int $0x80
1:  jmp 1b