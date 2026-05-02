.section .rodata
    fmt_int: .asciz "%d\n"
    fmt_str: .asciz "%s\n"

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    sub $256, %rsp
.L0:
    mov $5, %eax
    mov %rax, -8(%rbp)  # a
    mov $3, %eax
    mov %rax, -16(%rbp)  # b
    mov -8(%rbp), %eax
    push %rax
    mov -16(%rbp), %eax
    push %rax
    mov $2, %eax
    mov %rax, %rdx
    pop %rax
    imul %rdx
    mov %rax, %rdx
    pop %rax
    add %rdx, %rax
    mov %rax, -24(%rbp)  # result
    lea fmt_int(%rip), %rdi
    mov -24(%rbp), %esi
    xor %eax, %eax
    call printf
    mov $0, %edi
    call exit
    jmp .L0
.L1:
    xor %eax, %eax
    add $256, %rsp
    pop %rbp
    ret
