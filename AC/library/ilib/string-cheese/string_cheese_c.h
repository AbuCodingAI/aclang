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
const char* ac_stringm_getline();  /* read line from stdin */
int         ac_stringm_scan(const char* needle);  /* read line from stdin and check if it contains needle; returns 1 if found, 0 otherwise */

#ifdef __cplusplus
}
#endif
