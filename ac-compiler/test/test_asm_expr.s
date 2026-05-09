.section .rodata
    fmt_int: .asciz "%d\n"
    fmt_str: .asciz "%s\n"
    input_fmt: .asciz "%255s"
.section .bss
    input_buffer: .space 256
.section .rodata
.section .text
.globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $256, %rsp
    # Unknown operand: a
    push %rax
    # Unknown operand: b
    push %rax
    mov $2, %rax
    mov %rax, %rdx
    pop %rax
    imul %rdx
    mov %rax, %rdx
    pop %rax
    add %rdx, %rax
    mov %rax, -8(%rbp)  # result
    lea fmt_int(%rip), %rdi
    mov -8(%rbp), %esi
    xor %eax, %eax
    call printf
    mov $0, %edi
    call exit
    xor %rax, %rax
    add $256, %rsp
    pop %rbp
    ret
.section .note.GNU-stack,"",@progbits
