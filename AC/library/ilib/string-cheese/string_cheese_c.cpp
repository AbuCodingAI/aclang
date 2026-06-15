// AC ilib: string-cheese — C++ implementation (compiled as extern "C")
#include "string_cheese_c.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <algorithm>
#include <regex>

// Thread-local output buffer — avoids static state issues in single-call patterns
static thread_local char _sc_buf[65536];

static const char* _sc(const std::string& s) {
    size_t n = s.size();
    if (n >= sizeof(_sc_buf)) n = sizeof(_sc_buf) - 1;
    memcpy(_sc_buf, s.data(), n);
    _sc_buf[n] = '\0';
    return _sc_buf;
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
    // ws sentinel: replace all whitespace runs
    if (from == " \t\n\r") {
        std::string out = std::regex_replace(r, std::regex("\\s+"), to);
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

} // extern "C"
