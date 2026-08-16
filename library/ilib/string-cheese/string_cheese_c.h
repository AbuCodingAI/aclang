/* AC ilib: string-cheese — C API (libacstringcheese.so)
   use ilib string-cheese
   Link: -L./library/ilib/string-cheese -lacstringcheese
*/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* All functions that return strings write into a caller-supplied buffer.
   Returns the buffer pointer on success, NULL on truncation.
   Alternatively: functions returning const char* use an internal static buffer — single-threaded use only. */

const char* ac_stringm_lower(const char* s);
const char* ac_stringm_upper(const char* s);
const char* ac_stringm_trim(const char* s);
const char* ac_stringm_strip(const char* s, const char* chars);
int         ac_stringm_find(const char* s, const char* pattern);
const char* ac_stringm_replace(const char* s, const char* old_sub, const char* new_sub);
const char* ac_stringm_split_nth(const char* s, const char* sep, int n);
long long   ac_stringm_len(const char* s);
int         ac_stringm_startswith(const char* s, const char* prefix);
int         ac_stringm_endswith(const char* s, const char* suffix);
int         ac_stringm_count(const char* s, const char* sub);
const char* ac_stringm_format(const char* template_str);  /* literal passthrough; AC compiler handles {} at IR level */
const char* ac_stringm_f(const char* s);  /* f-string (formatted) — passthrough; compiler interpolates {} */
const char* ac_stringm_t(const char* s);  /* t-string (template, PEP 750) — passthrough; resolved at IR level */
const char* ac_stringm_b(const char* s);  /* bytes of s (identity for char*) */
long long   ac_stringm_endian(const char* bytes, const char* order);  /* bytes -> unsigned int, "little"/"big" */
const char* ac_stringm_getline();  /* read line from stdin */
int         ac_stringm_scan(const char* needle);  /* read line from stdin and check if it contains needle; returns 1 if found, 0 otherwise */
int         ac_stringm_ischar(const char* s);  /* check if string contains only alphabetic characters; returns 1 if true, 0 if false or empty */
int         ac_stringm_isws(const char* s);  /* check if string contains only whitespace; returns 1 if true, 0 if false */

#ifdef __cplusplus
}
#endif

/* ── Call-form shims ─────────────────────────────────────────────────────────
   The AC compiler emits stringm_upper(...) on C and stringm.upper(...) on C++.
   Map both onto the exported ac_stringm_* symbols. */
#ifdef __cplusplus
struct _ac_stringm_ns {
    const char* (*upper)(const char*)               = ac_stringm_upper;
    const char* (*lower)(const char*)               = ac_stringm_lower;
    const char* (*trim)(const char*)                = ac_stringm_trim;
    const char* (*strip)(const char*)              = ac_stringm_trim; /* AC strip = 1-arg trim */
    int         (*find)(const char*, const char*)   = ac_stringm_find;
    const char* (*replace)(const char*, const char*, const char*) = ac_stringm_replace;
    long long   (*len)(const char*)                 = ac_stringm_len;
    long long   (*length)(const char*)              = ac_stringm_len; /* string-cheese.acl aliases both "length" and "len" to the "stringm.length" call name — Java/Go/Rust/V's FFI wrappers already expose both spellings, C++ needs the member to match */
    int         (*startswith)(const char*, const char*) = ac_stringm_startswith;
    int         (*endswith)(const char*, const char*)   = ac_stringm_endswith;
    int         (*count)(const char*, const char*)  = ac_stringm_count;
    const char* (*b)(const char*)                   = ac_stringm_b;
    const char* (*f)(const char*)                   = ac_stringm_f;
    const char* (*t)(const char*)                   = ac_stringm_t;
    long long   (*endian)(const char*, const char*) = ac_stringm_endian;
    const char* (*getline)()                        = ac_stringm_getline;
    int         (*scan)(const char*)                = ac_stringm_scan;
    int         (*ischar)(const char*)              = ac_stringm_ischar;
    int         (*isws)(const char*)                = ac_stringm_isws;
};
static _ac_stringm_ns stringm;
#else
#define stringm_upper      ac_stringm_upper
#define stringm_lower      ac_stringm_lower
#define stringm_trim       ac_stringm_trim
#define stringm_strip      ac_stringm_trim /* AC strip = 1-arg trim */
#define stringm_find       ac_stringm_find
#define stringm_replace    ac_stringm_replace
#define stringm_len        ac_stringm_len
#define stringm_length     ac_stringm_len /* string-cheese.acl aliases both "length" and "len" to "stringm.length" — only "length" is ever actually emitted */
#define stringm_startswith ac_stringm_startswith
#define stringm_endswith   ac_stringm_endswith
#define stringm_count      ac_stringm_count
#define stringm_b          ac_stringm_b
#define stringm_f          ac_stringm_f
#define stringm_t          ac_stringm_t
#define stringm_endian     ac_stringm_endian
#define stringm_getline    ac_stringm_getline
#define stringm_scan       ac_stringm_scan
#define stringm_ischar     ac_stringm_ischar
#define stringm_isws       ac_stringm_isws
#endif
