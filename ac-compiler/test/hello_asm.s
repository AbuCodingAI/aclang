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
    lea .str_2(%rip), %rdi
    xor %eax, %eax
    call printf
.section .rodata
.str_2: .asciz "Starting up..."
.section .text
    mov $120, %eax
    mov %eax, -8(%rbp)  # result
    lea fmt_int(%rip), %rdi
    mov -8(%rbp), %esi
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
