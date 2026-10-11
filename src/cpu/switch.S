.section .text
.global switch_context
.type switch_context, @function

switch_context:
    mov 4(%esp), %eax
    mov 8(%esp), %edx

    push %ebp
    push %ebx
    push %esi
    push %edi

    mov %esp, (%eax)
    mov %edx, %esp

    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret