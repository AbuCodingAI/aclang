/* AC ilib: regex — C-compatible header (for CGo, C backends, ctypes) */
/* use ilib regex */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Match predicates — return 1 (true) or 0 (false) */
int         ac_regex_match(const char* str, const char* pat);
int         ac_regex_test(const char* str, const char* pat);

/* Single-string results — pointer into a static 64 KB thread-local buffer.
   Copy immediately; value is overwritten on the next call. */
const char* ac_regex_search(const char* str, const char* pat);
const char* ac_regex_replace(const char* str, const char* pat, const char* repl);
const char* ac_regex_replace_all(const char* str, const char* pat, const char* repl);
const char* ac_regex_escape(const char* str);

/* Integer result */
int         ac_regex_count(const char* str, const char* pat);

/* List results — heap-allocated char** of length *out_count.
   Call ac_regex_free_list when done. */
char**      ac_regex_find_all(const char* str, const char* pat, int* out_count);
char**      ac_regex_split(const char* str, const char* pat, int* out_count);
char**      ac_regex_groups(const char* str, const char* pat, int* out_count);
void        ac_regex_free_list(char** list, int count);

#ifdef __cplusplus
}
#endif
