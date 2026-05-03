.section .rodata
    fmt_int: .asciz "%d\n"
    fmt_str: .asciz "%s\n"

.section .text
.globl main
factorial:
    push %rbp
    mov %rsp, %rbp
    sub $256, %rsp
    mov %edi, -8(%rbp)  # parameter n
    mov -8(%rbp), %eax
    cmp $0 , %eax
    jne .L0
    mov $1, %eax
    add $256, %rsp
    pop %rbp
    ret
.L0:
    mov -8(%rbp), %eax
    cmp $1 , %eax
    jne .L1
    mov $1, %eax
    add $256, %rsp
    pop %rbp
    ret
.L1:
    mov -8(%rbp), %eax
    push %rax
    mov -8(%rbp), %eax
    push %rax
    mov $1, %eax
    mov %rax, %rdx
    pop %rax
    sub %rdx, %rax
    mov %rax, %rdi
    call factorial
    mov %rax, %rdx
    pop %rax
    imul %rdx
    add $256, %rsp
    pop %rbp
    ret
    mov $0, %eax
    add $256, %rsp
    pop %rbp
    ret

main:
    push %rbp
    mov %rsp, %rbp
    sub $256, %rsp
.L2:
    lea .str_4(%rip), %rdi
    xor %eax, %eax
    call printf
.section .rodata
.str_4: .asciz "Starting up..."
.section .text
.L5:
    mov $3, -32(%rbp)
    lea .str_0(%rip), %rax
    mov %rax, -24(%rbp)
    lea .str_1(%rip), %rax
    mov %rax, -16(%rbp)
    lea .str_2(%rip), %rax
    mov %rax, -8(%rbp)
    mov $0, -48(%rbp)  # loop counter
.L6:
    mov -48(%rbp), %rax
    mov -32(%rbp), %rcx  # list length
    cmp %rcx, %rax
    jge .L7
    lea -24(%rbp), %rdx
    mov (%rdx, %rax, 8), %rdi
    mov %rdi, -40(%rbp)  # fruit
    lea fmt_str(%rip), %rdi
    mov -40(%rbp), %rsi
    xor %eax, %eax
    call printf
    incq -48(%rbp)
    jmp .L6
.L7:
    mov $5, %eax
    mov %rax, %rdi
    call factorial
    mov %rax, -56(%rbp)  # result
    lea fmt_int(%rip), %rdi
    mov -56(%rbp), %esi
    xor %eax, %eax
    call printf
    mov $0, %edi
    call exit
    jmp .L5
    jmp .L2
.L3:
    xor %eax, %eax
    add $256, %rsp
    pop %rbp
    ret
.section .rodata
.str_0: .asciz "apple"
.str_1: .asciz "banana"
.str_2: .asciz "cherry"
.section .text
