// AC ilib: regex — Classic regular expressions (C++ backend)
// use ilib regex
#pragma once
#include <regex>
#include <string>
#include <vector>
#include <sstream>

namespace ac_regex {

// ── Core match predicates ─────────────────────────────────────────────────

inline bool match(const std::string& str, const std::string& pat) {
    try {
        return std::regex_match(str, std::regex(pat));
    } catch (...) { return false; }
}

inline bool test(const std::string& str, const std::string& pat) {
    try {
        return std::regex_search(str, std::regex(pat));
    } catch (...) { return false; }
}

// ── Single-result operations ───────────────────────────────────────────────

inline std::string search(const std::string& str, const std::string& pat) {
    try {
        std::smatch m;
        if (std::regex_search(str, m, std::regex(pat)))
            return m[0].str();
    } catch (...) {}
    return "";
}

inline std::string replace(const std::string& str, const std::string& pat,
                           const std::string& repl) {
    try {
        return std::regex_replace(str, std::regex(pat), repl,
                                  std::regex_constants::format_first_only);
    } catch (...) { return str; }
}

inline std::string replace_all(const std::string& str, const std::string& pat,
                                const std::string& repl) {
    try {
        return std::regex_replace(str, std::regex(pat), repl);
    } catch (...) { return str; }
}

inline int count(const std::string& str, const std::string& pat) {
    try {
        std::regex re(pat);
        auto begin = std::sregex_iterator(str.begin(), str.end(), re);
        auto end   = std::sregex_iterator();
        return static_cast<int>(std::distance(begin, end));
    } catch (...) { return 0; }
}

inline std::string escape(const std::string& str) {
    static const std::string specials = R"(\.^$|?*+()[]{}/)";
    std::string out;
    out.reserve(str.size() * 2);
    for (char c : str) {
        if (specials.find(c) != std::string::npos) out += '\\';
        out += c;
    }
    return out;
}

// ── List-result operations ─────────────────────────────────────────────────

inline std::vector<std::string> find_all(const std::string& str,
                                          const std::string& pat) {
    std::vector<std::string> out;
    try {
        std::regex re(pat);
        auto begin = std::sregex_iterator(str.begin(), str.end(), re);
        auto end   = std::sregex_iterator();
        for (auto it = begin; it != end; ++it)
            out.push_back((*it)[0].str());
    } catch (...) {}
    return out;
}

inline std::vector<std::string> split(const std::string& str,
                                       const std::string& pat) {
    std::vector<std::string> out;
    try {
        std::regex re(pat);
        std::sregex_token_iterator it(str.begin(), str.end(), re, -1);
        std::sregex_token_iterator end;
        for (; it != end; ++it)
            out.push_back(it->str());
    } catch (...) { out.push_back(str); }
    return out;
}

inline std::vector<std::string> groups(const std::string& str,
                                        const std::string& pat) {
    std::vector<std::string> out;
    try {
        std::smatch m;
        if (std::regex_search(str, m, std::regex(pat)))
            for (size_t i = 1; i < m.size(); ++i)
                out.push_back(m[i].str());
    } catch (...) {}
    return out;
}

} // namespace ac_regex

// ── Flat API (callable from AC-generated C++) ──────────────────────────────

inline bool        regex_match(const std::string& s, const std::string& p)      { return ac_regex::match(s, p);       }
inline bool        regex_test(const std::string& s, const std::string& p)       { return ac_regex::test(s, p);        }
inline std::string regex_search(const std::string& s, const std::string& p)     { return ac_regex::search(s, p);      }
inline std::string regex_replace(const std::string& s, const std::string& p,
                                  const std::string& r)                          { return ac_regex::replace(s, p, r);  }
inline std::string regex_replace_all(const std::string& s, const std::string& p,
                                      const std::string& r)                      { return ac_regex::replace_all(s,p,r);}
inline int         regex_count(const std::string& s, const std::string& p)      { return ac_regex::count(s, p);       }
inline std::string regex_escape(const std::string& s)                           { return ac_regex::escape(s);         }
inline std::vector<std::string> regex_find_all(const std::string& s,
                                                const std::string& p)            { return ac_regex::find_all(s, p);   }
inline std::vector<std::string> regex_split(const std::string& s,
                                             const std::string& p)               { return ac_regex::split(s, p);      }
inline std::vector<std::string> regex_groups(const std::string& s,
                                              const std::string& p)              { return ac_regex::groups(s, p);     }

// ── Namespace object — AC-generated C++ uses regex.match(...) etc. ─────────

struct _AcRegexNS {
    bool match(const std::string& s, const std::string& p) const        { return ac_regex::match(s, p);       }
    bool test(const std::string& s, const std::string& p) const         { return ac_regex::test(s, p);        }
    std::string search(const std::string& s, const std::string& p) const { return ac_regex::search(s, p);     }
    std::string replace(const std::string& s, const std::string& p,
                        const std::string& r) const                      { return ac_regex::replace(s, p, r); }
    std::string replace_all(const std::string& s, const std::string& p,
                             const std::string& r) const                 { return ac_regex::replace_all(s,p,r);}
    int count(const std::string& s, const std::string& p) const         { return ac_regex::count(s, p);       }
    std::string escape(const std::string& s) const                       { return ac_regex::escape(s);         }
    std::vector<std::string> find_all(const std::string& s,
                                      const std::string& p) const        { return ac_regex::find_all(s, p);   }
    std::vector<std::string> split(const std::string& s,
                                   const std::string& p) const           { return ac_regex::split(s, p);      }
    std::vector<std::string> groups(const std::string& s,
                                    const std::string& p) const          { return ac_regex::groups(s, p);     }
};
inline _AcRegexNS regex;
