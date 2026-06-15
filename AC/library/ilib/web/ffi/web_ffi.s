; ASM backend FFI for web library
; Dynamically loads libacweb.so and calls exported functions
; Uses dlopen/dlsym at runtime to invoke C functions

.section .data
    lib_path: .asciz "libacweb.so"
    func_open: .asciz "ac_web_open"
    func_file_open: .asciz "ac_web_file_open"
    func_popen: .asciz "ac_web_popen"
    func_ropen: .asciz "ac_web_ropen"
    func_browser: .asciz "ac_web_browser"
    func_pdf: .asciz "ac_web_pdf"
    func_text: .asciz "ac_web_text"
    func_inspect: .asciz "ac_web_inspect"
    func_ac_page: .asciz "ac_web_ac_page"
    func_help: .asciz "ac_web_help"
    error_msg: .asciz "Error loading web library\n"
    
    lib_handle: .quad 0

.section .text
.globl web_open
.globl web_file_open
.globl web_popen
.globl web_ropen
.globl web_browser
.globl web_pdf
.globl web_text
.globl web_inspect
.globl web_ac_page
.globl web_help

; Helper to load library once
load_web_library:
    cmp $0, lib_handle(%rip)
    jne .load_done
    
    lea lib_path(%rip), %rdi
    xor %esi, %esi              ; RTLD_LAZY = 0
    call dlopen@plt
    
    mov %rax, lib_handle(%rip)
    test %rax, %rax
    jz .load_error
    
.load_done:
    mov lib_handle(%rip), %rax
    ret
    
.load_error:
    lea error_msg(%rip), %rdi
    call printf@plt
    xor %eax, %eax
    ret

; web.open(link)
web_open:
    push %rdi                   ; save link pointer
    call load_web_library
    test %rax, %rax
    jz .web_open_fail
    
    mov %rax, %rdi              ; lib_handle
    lea func_open(%rip), %rsi
    call dlsym@plt
    
    pop %rdi                    ; restore link pointer
    jmp *%rax
    
.web_open_fail:
    pop %rdi
    ret

; web.file_open(file)
web_file_open:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_file_open_fail
    
    mov %rax, %rdi
    lea func_file_open(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_file_open_fail:
    pop %rdi
    ret

; web.popen(raw_link)
web_popen:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_popen_fail
    
    mov %rax, %rdi
    lea func_popen(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_popen_fail:
    pop %rdi
    ret

; web.ropen(identifier)
web_ropen:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_ropen_fail
    
    mov %rax, %rdi
    lea func_ropen(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_ropen_fail:
    pop %rdi
    ret

; web.browser()
web_browser:
    call load_web_library
    test %rax, %rax
    jz .web_browser_fail
    
    mov %rax, %rdi
    lea func_browser(%rip), %rsi
    call dlsym@plt
    
    jmp *%rax
    
.web_browser_fail:
    ret

; web.pdf(pdf)
web_pdf:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_pdf_fail
    
    mov %rax, %rdi
    lea func_pdf(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_pdf_fail:
    pop %rdi
    xor %eax, %eax
    ret

; web.text(text)
web_text:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_text_fail
    
    mov %rax, %rdi
    lea func_text(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_text_fail:
    pop %rdi
    xor %eax, %eax
    ret

; web.inspect(program)
web_inspect:
    push %rdi
    call load_web_library
    test %rax, %rax
    jz .web_inspect_fail
    
    mov %rax, %rdi
    lea func_inspect(%rip), %rsi
    call dlsym@plt
    
    pop %rdi
    jmp *%rax
    
.web_inspect_fail:
    pop %rdi
    xor %eax, %eax
    ret

; web.ac_page()
web_ac_page:
    call load_web_library
    test %rax, %rax
    jz .web_ac_page_fail
    
    mov %rax, %rdi
    lea func_ac_page(%rip), %rsi
    call dlsym@plt
    
    jmp *%rax
    
.web_ac_page_fail:
    ret

; web.help()
web_help:
    call load_web_library
    test %rax, %rax
    jz .web_help_fail
    
    mov %rax, %rdi
    lea func_help(%rip), %rsi
    call dlsym@plt
    
    jmp *%rax
    
.web_help_fail:
    xor %eax, %eax
    ret
