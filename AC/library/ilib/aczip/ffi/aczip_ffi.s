; ACZip FFI for x86-64 Assembly (NASM)
; Bindings to libaczip.so

extern ac_zip_compress
extern ac_zip_decompress
extern ac_zip_compress_hdd
extern ac_zip_compress_sata
extern ac_get_compression_ratio
extern ac_free_bytes

section .text

; ac_zip_compress(path: rdi, parallel: rsi) -> ByteArray {rax, rdx}
global aczip_compress
aczip_compress:
    mov rax, rdi        ; path pointer
    mov rcx, rsi        ; parallel flag
    call ac_zip_compress
    ; Returns: rax = data pointer, rdx = size
    ret

; ac_zip_decompress(data: rdi, size: rsi, output_path: rdx) -> status: rax
global aczip_decompress
aczip_decompress:
    mov rax, rdi        ; data pointer
    mov rcx, rsi        ; size
    mov rdx, rdx        ; output_path (already in rdx)
    call ac_zip_decompress
    ret

; ac_zip_compress_hdd(path: rdi) -> ByteArray {rax, rdx}
global aczip_compress_hdd
aczip_compress_hdd:
    mov rax, rdi        ; path pointer
    call ac_zip_compress_hdd
    ret

; ac_zip_compress_sata(path: rdi) -> ByteArray {rax, rdx}
global aczip_compress_sata
aczip_compress_sata:
    mov rax, rdi        ; path pointer
    call ac_zip_compress_sata
    ret

; ac_get_compression_ratio(original: rdi, compressed: rsi) -> ratio: xmm0
global aczip_get_ratio
aczip_get_ratio:
    mov rax, rdi        ; original size
    mov rcx, rsi        ; compressed size
    call ac_get_compression_ratio
    ; Returns: xmm0 (double)
    ret

; ac_free_bytes(arr: rdi)
global aczip_free
aczip_free:
    mov rax, rdi        ; ByteArray pointer
    call ac_free_bytes
    ret
