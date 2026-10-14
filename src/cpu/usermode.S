.section .text
.global enter_usermode
.type enter_usermode, @function

enter_usermode:
    mov 4(%esp), %eax
    mov 8(%esp), %ecx

    mov $0x23, %dx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs

    push $0x23
    push %ecx
    pushf
    pop %edx
    or  $0x200, %edx
    and $0xFFFFCFFF, %edx
    push %edx
    push $0x1B
    push %eax
    iret