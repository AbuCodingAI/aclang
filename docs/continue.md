# Continuation Notes

## Next: `ilib regex` — Classic Regex Library for AC and AI

Build `library/regex/` following the exact same pattern as `library/math/`.

### API surface

```ac
use ilib regex

<mainloop>
    text = $hello world 2025$

    IF regex.match(text, $hello.*$)           /* full-string match  → bool */
        Term.display $matched$

    found = regex.search(text, $\d+$)         /* first match        → string */
    Term.display found                         /* 2025 */

    words = regex.find_all(text, $\w+$)       /* all matches        → list */
    Term.display words                         /* [hello, world, 2025] */

    out = regex.replace(text, $\d+$, $XXXX$)  /* replace first      → string */
    out2 = regex.replace_all(text, $\w+$, $X$)/* replace all        → string */

    parts = regex.split(text, $ $)            /* split by pattern   → list */
    grps  = regex.groups($2025-01-01$, $(\d{4})-(\d{2})-(\d{2})$) /* capture groups → list */
    n     = regex.count(text, $\w+$)          /* count matches      → int */
    safe  = regex.escape($a.b+c$)             /* escape special chars → string */
<mainloop>
```

### Files to create

```
library/regex/
├── regex.hpp          C++ header — namespace ac_regex + flat API + _AcRegexNS struct + inline regex var
├── regex.cpp          shared lib (extern "C") — uses std::regex from <regex>
├── regex.py           Python backend — wraps re module (pure Python, no .so needed)
├── regex_c.h          C-compatible extern declarations (for CGo and C backends)
├── Makefile           builds libacregex.so (Linux) and acregex.dll (Windows, MinGW)
└── ffi/
    ├── regex_ffi.asm  x86-64 NASM extern declarations
    ├── regex_ffi.go   CGO — links libacregex.so via regex_c.h
    ├── regex_ffi.java Panama FFI (Java 22+)
    ├── regex_ffi.js   ffi-napi — links libacregex.so
    ├── regex_ffi.py   ctypes — links libacregex.so
    ├── regex_ffi.rs   Rust extern "C" block linking libacregex
    └── regex_ffi.v    V FFI — links libacregex.so
```

### C API (extern "C" symbols in regex.cpp)

Single-string returns use a `thread_local` static 64 KB buffer (result truncated if longer).
List returns (`char**`) are heap-allocated — caller frees with `ac_regex_free_list`.

```c
int          ac_regex_match(const char* str, const char* pat);          /* 1/0 */
int          ac_regex_test(const char* str, const char* pat);           /* 1/0 */
const char*  ac_regex_search(const char* str, const char* pat);         /* static buf */
const char*  ac_regex_replace(const char* str, const char* pat, const char* repl);
const char*  ac_regex_replace_all(const char* str, const char* pat, const char* repl);
int          ac_regex_count(const char* str, const char* pat);
const char*  ac_regex_escape(const char* str);                          /* static buf */
char**       ac_regex_find_all(const char* str, const char* pat, int* out_count);
char**       ac_regex_split(const char* str, const char* pat, int* out_count);
char**       ac_regex_groups(const char* str, const char* pat, int* out_count);
void         ac_regex_free_list(char** list, int count);
```

### Implementation notes

- C++ backend: `std::regex` (ECMAScript flavour, C++17)
- Python backend: `re` module — no .so dependency
- JS FFI: native `RegExp`, no .so dependency (same pattern as other native backends)
- `_AcRegexNS` struct in `regex.hpp` mirrors `_AcMathNS` so AC codegen emits `regex.match(...)` etc.
- List returns in Python/JS are native lists — no malloc/free needed there
- `regex.groups` returns an empty list if no match (never throws)
