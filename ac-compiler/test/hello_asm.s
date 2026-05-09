.section .rodata
    fmt_int: .asciz "%d\n"
    fmt_str: .asciz "%s\n"
    input_fmt: .asciz "%255s"
.section .bss
    input_buffer: .space 256
.section .rodata
.section .rodata
.str_0: .asciz "Starting up..."
.section .text
.section .text
.globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $256, %rsp
    lea .str_0(%rip), %rdi
    xor %eax, %eax
    call printf
    # Unknown operand: result
    lea fmt_int(%rip), %rdi
    mov %rax, %rsi
    xor %eax, %eax
    call printf
    mov $0, %edi
    call exit
    xor %rax, %rax
    add $256, %rsp
    pop %rbp
    ret
.section .note.GNU-stack,"",@progbits
