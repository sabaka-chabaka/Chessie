.macro UWRITE msg, msg_end
    mov $1, %eax
    mov $1, %ebx
    lea \msg - .Lbase(%esi), %ecx
    mov $(\msg_end - \msg), %edx
    int $0x80
.endm

.macro UEXIT code
    mov $2, %eax
    mov $\code, %ebx
    int $0x80
.endm

.section .rodata
.align 4
.global user_blob_start
.global user_blob_end

user_blob_start:
    call .Lbase
.Lbase:
    pop %esi
    mov (%esp), %edi

    cmp $1, %edi
    je .Lmode_cli
    cmp $2, %edi
    je .Lmode_kread
    cmp $3, %edi
    je .Lmode_wcode
    cmp $4, %edi
    je .Lmode_outb

    UWRITE .Lmsg_hello, .Lmsg_hello_end

    mov $1, %eax
    mov $1, %ebx
    mov $0x00100000, %ecx
    mov $16, %edx
    int $0x80
    cmp $-1, %eax
    jne .Lfail

    mov $1, %eax
    mov $1, %ebx
    mov $0x50000000, %ecx
    mov $16, %edx
    int $0x80
    cmp $-1, %eax
    jne .Lfail

    UWRITE .Lmsg_ok, .Lmsg_ok_end
    UEXIT 0

.Lfail:
    UWRITE .Lmsg_fail, .Lmsg_fail_end
    UEXIT 1

.Lmode_cli:
    cli
    jmp .Lhang

.Lmode_kread:
    mov 0x00100000, %eax
    jmp .Lhang

.Lmode_wcode:
    mov %eax, (%esi)
    jmp .Lhang

.Lmode_outb:
    mov $0x3F8, %edx
    mov $0x58, %eax
    outb %al, %dx

.Lhang:
    jmp .Lhang

.Lmsg_hello:
    .ascii "hello from ring 3\n"
.Lmsg_hello_end:
.Lmsg_ok:
    .ascii "ok: kernel and unmapped pointers rejected\n"
.Lmsg_ok_end:
.Lmsg_fail:
    .ascii "FAIL: kernel allowed a bad pointer\n"
.Lmsg_fail_end:
user_blob_end: