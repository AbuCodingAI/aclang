#pragma once
#include "string_cheese_c.h"

namespace stringm {
    inline const char* lower(const char* s) { return ac_stringm_lower(s); }
    inline const char* upper(const char* s) { return ac_stringm_upper(s); }
    inline const char* trim(const char* s) { return ac_stringm_trim(s); }
    inline const char* strip(const char* s, const char* chars = nullptr) { return ac_stringm_strip(s, chars); }
    inline int find(const char* s, const char* pattern) { return ac_stringm_find(s, pattern); }
    inline const char* replace(const char* s, const char* old_sub, const char* new_sub) { return ac_stringm_replace(s, old_sub, new_sub); }
    inline const char* split(const char* s, const char* sep, int n) { return ac_stringm_split_nth(s, sep, n); }
    inline long long length(const char* s) { return ac_stringm_len(s); }
    inline int startswith(const char* s, const char* prefix) { return ac_stringm_startswith(s, prefix); }
    inline int endswith(const char* s, const char* suffix) { return ac_stringm_endswith(s, suffix); }
    inline int count(const char* s, const char* sub) { return ac_stringm_count(s, sub); }
    inline const char* format(const char* template_str) { return ac_stringm_format(template_str); }
    inline const char* getline() { return ac_stringm_getline(); }
    inline int scan(const char* needle) { return ac_stringm_scan(needle); }

    // Type conversions
    inline const char* f(const char* s) { return s; }  // to string
    inline const char* t(const char* s) { return s; }  // to string
    inline const char* b(const char* s) { return s; }  // to string
}
