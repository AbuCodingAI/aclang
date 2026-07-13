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

/* Call-form shims: the AC compiler emits regex_x(...) on C and regex.x(...) on C++. */
#ifdef __cplusplus
struct _ac_regex_ns {
    int (*match)(const char*, const char*) = ac_regex_match;
    int (*test)(const char*, const char*) = ac_regex_test;
    const char* (*search)(const char*, const char*) = ac_regex_search;
    const char* (*replace)(const char*, const char*, const char*) = ac_regex_replace;
    const char* (*replace_all)(const char*, const char*, const char*) = ac_regex_replace_all;
    int (*count)(const char*, const char*) = ac_regex_count;
    const char* (*escape)(const char*) = ac_regex_escape;
};
static _ac_regex_ns regex;
#else
#define regex_match ac_regex_match
#define regex_test ac_regex_test
#define regex_search ac_regex_search
#define regex_replace ac_regex_replace
#define regex_replace_all ac_regex_replace_all
#define regex_count ac_regex_count
#define regex_escape ac_regex_escape
#endif
