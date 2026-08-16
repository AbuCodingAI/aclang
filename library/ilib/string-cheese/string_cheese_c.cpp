// AC ilib: string-cheese — C++ implementation (compiled as extern "C")
#include "string_cheese_c.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>
#include <algorithm>
#include <regex>

// Thread-local output buffers, round-robin. A SINGLE shared buffer made two stringm.* results in
// one expression alias — `stringm.upper(a) + stringm.lower(b)` returned the same pointer twice, so
// the first result was overwritten by the second. Rotating across N buffers keeps up to N results
// live simultaneously (covers any realistic chained-call expression).
static const int _SC_NBUF = 8;
static thread_local char _sc_bufs[_SC_NBUF][65536];
static thread_local int  _sc_idx = 0;

static const char* _sc(const std::string& s) {
    char* buf = _sc_bufs[_sc_idx];
    _sc_idx = (_sc_idx + 1) % _SC_NBUF;
    size_t n = s.size();
    if (n >= 65536) n = 65535;
    memcpy(buf, s.data(), n);
    buf[n] = '\0';
    return buf;
}

extern "C" {

const char* ac_stringm_lower(const char* s) {
    if (!s) return "";
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return _sc(r);
}

const char* ac_stringm_upper(const char* s) {
    if (!s) return "";
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return _sc(r);
}

const char* ac_stringm_trim(const char* s) {
    if (!s) return "";
    std::string r(s);
    auto b = r.find_first_not_of(" \t\n\r\f\v");
    if (b == std::string::npos) return _sc("");
    auto e = r.find_last_not_of(" \t\n\r\f\v");
    return _sc(r.substr(b, e - b + 1));
}

const char* ac_stringm_strip(const char* s, const char* chars) {
    if (!s) return "";
    std::string r(s);
    // If chars is the ws sentinel or empty, strip all whitespace
    bool wsMode = (!chars || !*chars || strcmp(chars, " \t\n\r") == 0);
    if (wsMode) {
        auto b = r.find_first_not_of(" \t\n\r\f\v");
        if (b == std::string::npos) return _sc("");
        auto e = r.find_last_not_of(" \t\n\r\f\v");
        return _sc(r.substr(b, e - b + 1));
    }
    std::string cs(chars);
    auto b = r.find_first_not_of(cs);
    if (b == std::string::npos) return _sc("");
    auto e = r.find_last_not_of(cs);
    return _sc(r.substr(b, e - b + 1));
}

int ac_stringm_find(const char* s, const char* pattern) {
    if (!s || !pattern) return -1;
    // ws sentinel: find first whitespace
    if (strcmp(pattern, " \t\n\r") == 0) {
        for (int i = 0; s[i]; i++)
            if (isspace((unsigned char)s[i])) return i;
        return -1;
    }
    const char* p = strstr(s, pattern);
    return p ? (int)(p - s) : -1;
}

const char* ac_stringm_replace(const char* s, const char* old_sub, const char* new_sub) {
    if (!s || !old_sub || !new_sub) return s ? s : "";
    std::string r(s);
    std::string from(old_sub), to(new_sub);
    // ws sentinel: replace all whitespace runs. Compile the pattern ONCE (a std::regex build is
    // orders of magnitude costlier than the replace) — it's a constant, so make it static.
    if (from == " \t\n\r") {
        static const std::regex ws_re("\\s+");
        std::string out = std::regex_replace(r, ws_re, to);
        return _sc(out);
    }
    size_t pos = 0;
    while ((pos = r.find(from, pos)) != std::string::npos) {
        r.replace(pos, from.size(), to);
        pos += to.size();
    }
    return _sc(r);
}

const char* ac_stringm_split_nth(const char* s, const char* sep, int n) {
    if (!s) return "";
    std::string str(s);
    std::string delim = (!sep || !*sep) ? " " : std::string(sep);
    size_t pos = 0, idx = 0;
    while (true) {
        size_t next = str.find(delim, pos);
        if (idx == (size_t)n) {
            std::string part = (next == std::string::npos) ? str.substr(pos) : str.substr(pos, next - pos);
            return _sc(part);
        }
        if (next == std::string::npos) break;
        pos = next + delim.size();
        idx++;
    }
    return _sc("");
}

long long ac_stringm_len(const char* s) {
    return s ? (long long)strlen(s) : 0LL;
}

int ac_stringm_startswith(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0 ? 1 : 0;
}

int ac_stringm_endswith(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    size_t sl = strlen(s), pl = strlen(suffix);
    if (pl > sl) return 0;
    return strcmp(s + sl - pl, suffix) == 0 ? 1 : 0;
}

int ac_stringm_count(const char* s, const char* sub) {
    if (!s || !sub || !*sub) return 0;
    if (strcmp(sub, " \t\n\r") == 0) {
        int n = 0;
        for (const char* p = s; *p; p++) if (isspace((unsigned char)*p)) n++;
        return n;
    }
    int n = 0;
    size_t sublen = strlen(sub);
    for (const char* p = s; (p = strstr(p, sub)); p += sublen) n++;
    return n;
}

const char* ac_stringm_format(const char* template_str) {
    return template_str ? template_str : "";
}

/* b / f / t string-prefix constructors. f-string (formatted) and t-string (template,
   PEP 750) interpolate at the compiler/IR level; the runtime is a passthrough, exactly
   like ac_stringm_format. (ac_stringm_b is the bytes constructor, defined below.) */
const char* ac_stringm_f(const char* s) { return s ? s : ""; }
const char* ac_stringm_t(const char* s) { return s ? s : ""; }

const char* ac_stringm_getline() {
    // Read a line from stdin and return it
    static thread_local std::string line;
    line.clear();
    if (std::getline(std::cin, line)) {
        return _sc(line);
    }
    return _sc("");
}

int ac_stringm_scan(const char* needle) {
    // Read a line from stdin and check if it contains the needle
    // Returns 1 if found, 0 if not found or EOF
    if (!needle) return 0;
    static thread_local std::string line;
    line.clear();
    if (std::getline(std::cin, line)) {
        return (line.find(needle) != std::string::npos) ? 1 : 0;
    }
    return 0;
}

int ac_stringm_ischar(const char* s) {
    // Check if string contains only alphabetic characters
    if (!s || !*s) return 0;
    for (const char* p = s; *p; p++) {
        if (!isalpha((unsigned char)*p)) return 0;
    }
    return 1;
}

int ac_stringm_isws(const char* s) {
    // Check if string contains only whitespace characters
    if (!s) return 0;
    if (!*s) return 1;  // empty string is all whitespace
    for (const char* p = s; *p; p++) {
        if (!isspace((unsigned char)*p)) return 0;
    }
    return 1;
}

} // extern "C"

/* bytes of s — for C, a string already is its bytes */
const char* ac_stringm_b(const char* s) { return s ? s : ""; }

/* Interpret a byte string as an unsigned integer, little or big endian. */
long long ac_stringm_endian(const char* bytes, const char* order) {
    if (!bytes) return 0;
    size_t n = strlen(bytes);
    if (n > 8) n = 8;  /* clamp to 64-bit */
    unsigned long long v = 0;
    int little = (!order || order[0]=='l' || order[0]=='L');
    for (size_t i = 0; i < n; i++) {
        unsigned char byte = (unsigned char)bytes[little ? (n - 1 - i) : i];
        v = (v << 8) | byte;
    }
    return (long long)v;
}
