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
    mov $32, %edi
    call malloc
    mov %rax, -8(%rbp)  # a
    mov $3, (%rax)
    mov $1, %rax
    mov -8(%rbp), %rcx
    mov %rax, 8(%rcx)
    mov $2, %rax
    mov -8(%rbp), %rcx
    mov %rax, 16(%rcx)
    mov $3, %rax
    mov -8(%rbp), %rcx
    mov %rax, 24(%rcx)
    mov $0, %eax
    mov %rax, %rbx  # index
    mov -8(%rbp), %rcx  # list pointer
    mov $10, %eax
    lea 8(%rcx), %rdx
    mov %rax, (%rdx, %rbx, 8)
    mov $0, %eax
    mov %rax, %rbx
    mov -8(%rbp), %rcx
    lea 8(%rcx), %rdx
    mov (%rdx, %rbx, 8), %rax
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    mov $1, %eax
    mov %rax, %rbx
    mov -8(%rbp), %rcx
    lea 8(%rcx), %rdx
    mov (%rdx, %rbx, 8), %rax
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    mov $5, %eax
    mov %rax, -16(%rbp)  # x
    mov -16(%rbp), %rax
    cmp $5, %rax
    jne .L3
    mov $100, %eax
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    jmp .L2
.L3:
    mov -16(%rbp), %rax
    cmp $7, %rax
    jne .L4
    mov $200, %eax
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    jmp .L2
.L4:
    mov $300, %eax
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    jmp .L2
.L2:
    mov $0, %edi
    call exit
    jmp .L0
.L1:
    xor %eax, %eax
    add $256, %rsp
    pop %rbp
    ret
