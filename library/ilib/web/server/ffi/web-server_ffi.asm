; ASM backend FFI for web-server library. Dynamically loads libacserver.so via
; dlopen/dlsym, mirroring web_ffi.asm's pattern exactly. HTTP serving is real
; sockets entirely inside the .so (C++), so this file is pure call-through — no
; raw-socket work needed in hand-written x86-64, same as db_run already was.

.section .data
    lib_path: .asciz "libacserver.so"
    func_db_run: .asciz "ac_server_db_run"
    func_db_run_p: .asciz "ac_server_db_run_p"
    func_db_import: .asciz "ac_server_db_import"
    func_db_reset: .asciz "ac_server_db_reset"
    func_db_stop: .asciz "ac_server_db_stop"
    func_help: .asciz "ac_server_help"
    func_listen: .asciz "ac_server_listen"
    func_accept: .asciz "ac_server_accept"
    func_req_method: .asciz "ac_server_req_method"
    func_req_path: .asciz "ac_server_req_path"
    func_req_query: .asciz "ac_server_req_query"
    func_req_body: .asciz "ac_server_req_body"
    func_req_header: .asciz "ac_server_req_header"
    func_respond: .asciz "ac_server_respond"
    func_respond_json: .asciz "ac_server_respond_json"
    func_close: .asciz "ac_server_close"
    error_msg: .asciz "Error loading web-server library\n"

    lib_handle: .quad 0

.section .text
.globl server_db_run
.globl server_db_run_p
.globl server_db_import
.globl server_db_reset
.globl server_db_stop
.globl server_help
.globl server_listen
.globl server_accept
.globl server_req_method
.globl server_req_path
.globl server_req_query
.globl server_req_body
.globl server_req_header
.globl server_respond
.globl server_respond_json
.globl server_close

; Helper to load library once
load_server_library:
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

; server.db_run(statement) -> const char*
server_db_run:
    push %rdi                   ; save statement pointer
    call load_server_library
    test %rax, %rax
    jz .db_run_fail

    mov %rax, %rdi              ; lib_handle
    lea func_db_run(%rip), %rsi
    call dlsym@plt

    pop %rdi                    ; restore statement pointer
    jmp *%rax

.db_run_fail:
    pop %rdi
    ret

; server.db_run_p(statement, params_json) -> const char*
server_db_run_p:
    push %rdi                   ; save statement pointer
    push %rsi                   ; save params_json pointer
    call load_server_library
    test %rax, %rax
    jz .db_run_p_fail

    mov %rax, %rdi              ; lib_handle
    lea func_db_run_p(%rip), %rsi
    call dlsym@plt

    pop %rsi                    ; restore params_json (LIFO: rsi was pushed last)
    pop %rdi                    ; restore statement
    jmp *%rax

.db_run_p_fail:
    pop %rsi
    pop %rdi
    ret

; server.db_import(path) -> const char*
server_db_import:
    push %rdi
    call load_server_library
    test %rax, %rax
    jz .db_import_fail

    mov %rax, %rdi
    lea func_db_import(%rip), %rsi
    call dlsym@plt

    pop %rdi
    jmp *%rax

.db_import_fail:
    pop %rdi
    ret

; server.db_reset() -> const char*
server_db_reset:
    call load_server_library
    test %rax, %rax
    jz .db_reset_fail

    mov %rax, %rdi
    lea func_db_reset(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.db_reset_fail:
    ret

; server.db_stop() -> void
server_db_stop:
    call load_server_library
    test %rax, %rax
    jz .db_stop_fail

    mov %rax, %rdi
    lea func_db_stop(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.db_stop_fail:
    ret

; server.help() -> const char*
server_help:
    call load_server_library
    test %rax, %rax
    jz .help_fail

    mov %rax, %rdi
    lea func_help(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.help_fail:
    ret

; server.listen(port) -> int
server_listen:
    push %rdi
    call load_server_library
    test %rax, %rax
    jz .listen_fail

    mov %rax, %rdi
    lea func_listen(%rip), %rsi
    call dlsym@plt

    pop %rdi
    jmp *%rax

.listen_fail:
    pop %rdi
    xor %eax, %eax
    ret

; server.accept() -> int
server_accept:
    call load_server_library
    test %rax, %rax
    jz .accept_fail

    mov %rax, %rdi
    lea func_accept(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.accept_fail:
    xor %eax, %eax
    ret

; server.req_method() -> const char*
server_req_method:
    call load_server_library
    test %rax, %rax
    jz .req_method_fail

    mov %rax, %rdi
    lea func_req_method(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.req_method_fail:
    ret

; server.req_path() -> const char*
server_req_path:
    call load_server_library
    test %rax, %rax
    jz .req_path_fail

    mov %rax, %rdi
    lea func_req_path(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.req_path_fail:
    ret

; server.req_query(key) -> const char*
server_req_query:
    push %rdi
    call load_server_library
    test %rax, %rax
    jz .req_query_fail

    mov %rax, %rdi
    lea func_req_query(%rip), %rsi
    call dlsym@plt

    pop %rdi
    jmp *%rax

.req_query_fail:
    pop %rdi
    ret

; server.req_body() -> const char*
server_req_body:
    call load_server_library
    test %rax, %rax
    jz .req_body_fail

    mov %rax, %rdi
    lea func_req_body(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.req_body_fail:
    ret

; server.req_header(name) -> const char*
server_req_header:
    push %rdi
    call load_server_library
    test %rax, %rax
    jz .req_header_fail

    mov %rax, %rdi
    lea func_req_header(%rip), %rsi
    call dlsym@plt

    pop %rdi
    jmp *%rax

.req_header_fail:
    pop %rdi
    ret

; server.respond(status, body) -> int
server_respond:
    push %rdi                   ; status
    push %rsi                   ; body
    call load_server_library
    test %rax, %rax
    jz .respond_fail

    mov %rax, %rdi
    lea func_respond(%rip), %rsi
    call dlsym@plt

    pop %rsi                    ; body (LIFO: rsi was pushed last)
    pop %rdi                    ; status
    jmp *%rax

.respond_fail:
    pop %rsi
    pop %rdi
    xor %eax, %eax
    ret

; server.respond_json(status, json_body) -> int
server_respond_json:
    push %rdi
    push %rsi
    call load_server_library
    test %rax, %rax
    jz .respond_json_fail

    mov %rax, %rdi
    lea func_respond_json(%rip), %rsi
    call dlsym@plt

    pop %rsi
    pop %rdi
    jmp *%rax

.respond_json_fail:
    pop %rsi
    pop %rdi
    xor %eax, %eax
    ret

; server.close() -> void
server_close:
    call load_server_library
    test %rax, %rax
    jz .close_fail

    mov %rax, %rdi
    lea func_close(%rip), %rsi
    call dlsym@plt
    jmp *%rax

.close_fail:
    ret
