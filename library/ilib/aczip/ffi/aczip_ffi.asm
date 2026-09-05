; AC ilib: aczip — x86-64 NASM extern declarations (libaczip.so)
; AC has no raw byte-buffer type, so this binding calls the file-to-file convenience
; functions in aczip_c.h/.cpp (shared with every other backend's FFI — see that file's
; own comment) instead of marshaling ACZipByteArray by hand. AsmStrategy's
; asmIlibSymbolMap translates AC's `aczip.compress(...)` etc directly to these real
; symbols (ir_codegen.cpp) — no bare-name wrapper functions needed here, just the
; extern declarations every call site needs at link time.

; long long ac_zip_compress_to_file(const char* path, int parallel, const char* output_path)
; Signature: (rdi: path, esi: parallel, rdx: output_path) -> rax: bytes written (-1 on failure)
extern ac_zip_compress_to_file

; int ac_zip_decompress_from_file(const char* archive_path, const char* output_path)
; Signature: (rdi: archive_path, rsi: output_path) -> eax: status (0 = success)
extern ac_zip_decompress_from_file

; double ac_get_compression_ratio(size_t original, size_t compressed)
; Signature: (rdi: original, rsi: compressed) -> xmm0: ratio percentage
extern ac_get_compression_ratio
