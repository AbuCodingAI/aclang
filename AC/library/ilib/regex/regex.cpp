// AC ilib: regex — extern "C" shared library implementation
// Build: see Makefile  →  libacregex.so (Linux) / acregex.dll (Windows)
#include "regex.hpp"
#include <cstdlib>
#include <cstring>

// ── Static result buffer (per-thread, 64 KB) ──────────────────────────────
static thread_local char _rbuf[65536];

static const char* _store(const std::string& s) {
    size_t n = s.size();
    if (n >= sizeof(_rbuf)) n = sizeof(_rbuf) - 1;
    memcpy(_rbuf, s.data(), n);
    _rbuf[n] = '\0';
    return _rbuf;
}

// ── Heap list helpers ──────────────────────────────────────────────────────

static char** _make_list(const std::vector<std::string>& v, int* out_count) {
    int n = static_cast<int>(v.size());
    char** arr = static_cast<char**>(malloc(sizeof(char*) * static_cast<size_t>(n)));
    if (!arr) { *out_count = 0; return nullptr; }
    for (int i = 0; i < n; ++i) {
        arr[i] = static_cast<char*>(malloc(v[i].size() + 1));
        if (arr[i]) memcpy(arr[i], v[i].c_str(), v[i].size() + 1);
    }
    *out_count = n;
    return arr;
}

extern "C" {

// ── Match predicates ───────────────────────────────────────────────────────

int ac_regex_match(const char* str, const char* pat) {
    return ac_regex::match(str, pat) ? 1 : 0;
}

int ac_regex_test(const char* str, const char* pat) {
    return ac_regex::test(str, pat) ? 1 : 0;
}

// ── Single-string results (static buffer) ─────────────────────────────────

const char* ac_regex_search(const char* str, const char* pat) {
    return _store(ac_regex::search(str, pat));
}

const char* ac_regex_replace(const char* str, const char* pat,
                              const char* repl) {
    return _store(ac_regex::replace(str, pat, repl));
}

const char* ac_regex_replace_all(const char* str, const char* pat,
                                  const char* repl) {
    return _store(ac_regex::replace_all(str, pat, repl));
}

const char* ac_regex_escape(const char* str) {
    return _store(ac_regex::escape(str));
}

// ── Integer result ─────────────────────────────────────────────────────────

int ac_regex_count(const char* str, const char* pat) {
    return ac_regex::count(str, pat);
}

// ── List results (heap-allocated — caller frees with ac_regex_free_list) ──

char** ac_regex_find_all(const char* str, const char* pat, int* out_count) {
    return _make_list(ac_regex::find_all(str, pat), out_count);
}

char** ac_regex_split(const char* str, const char* pat, int* out_count) {
    return _make_list(ac_regex::split(str, pat), out_count);
}

char** ac_regex_groups(const char* str, const char* pat, int* out_count) {
    return _make_list(ac_regex::groups(str, pat), out_count);
}

void ac_regex_free_list(char** list, int count) {
    if (!list) return;
    for (int i = 0; i < count; ++i) free(list[i]);
    free(list);
}

} // extern "C"
