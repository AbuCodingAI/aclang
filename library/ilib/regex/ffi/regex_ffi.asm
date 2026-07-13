; AC ilib: regex — x86-64 NASM extern declarations (libacregex.so / acregex.dll)
; Link with: nasm -f elf64 output.asm && ld output.o -L../../library/regex -lacregex -o output
; Or via gcc: gcc output.asm -L../../library/regex -lacregex -Wl,-rpath,../../library/regex -o output

section .text

; Match predicates — return int (1=true, 0=false)
; Signature: (rdi: const char* str, rsi: const char* pat) -> eax: int
extern ac_regex_match
extern ac_regex_test

; Single-string results — return const char* (static thread-local buffer)
; Signature: (rdi: const char* str, rsi: const char* pat) -> rax: const char*
extern ac_regex_search

; Signature: (rdi: str, rsi: pat, rdx: repl) -> rax: const char*
extern ac_regex_replace
extern ac_regex_replace_all

; Signature: (rdi: const char* str) -> rax: const char*
extern ac_regex_escape

; Integer result
; Signature: (rdi: str, rsi: pat) -> eax: int
extern ac_regex_count

; List results — return char** (heap-allocated, free with ac_regex_free_list)
; Signature: (rdi: str, rsi: pat, rdx: int* out_count) -> rax: char**
extern ac_regex_find_all
extern ac_regex_split
extern ac_regex_groups

; Free list returned by find_all / split / groups
; Signature: (rdi: char** list, rsi: int count) -> void
extern ac_regex_free_list
