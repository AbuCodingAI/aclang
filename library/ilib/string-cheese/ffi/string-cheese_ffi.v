// AC ilib: string-cheese — V FFI
// Inlined by AC->V compiler when "use ilib string-cheese" is declared.
//
// upper/lower/trim/strip/find/replace/split_nth/len/startswith/endswith/count/format go
// through the shared C core (libacstringcheese.so) via C interop below. split/join/
// getline/scan/ischar/isws are implemented natively in V instead — the C ABI only offers
// split_nth (no full split, no join at all) and no stdin reading, and V's stdlib already
// covers all of this directly (s.split/arr.join/os.get_line), so there is no reason to
// route them through C.
//
// NOTE: this V version (0.5.1) requires NAMED method receivers (`fn (n Foo) bar()`) and a
// capitalized struct name — a bare/unnamed receiver (`fn (Foo) bar()`) or an
// underscore-prefixed lowercase struct name both fail to compile on this V version. The
// previous revision of this file used the broken pattern (verified: `v run` failed with
// "expecting type declaration" on the first bare-receiver method) and is fixed here.
//
// `os` is imported here under an alias (`scos`) rather than plainly: the compiler's own V
// backend conditionally emits its own bare `import os` when the AC program separately uses
// input-reading, and V rejects a duplicate `import os` in the same file — the alias avoids
// that collision entirely.
import os as scos

#flag -L @AC_LIBDIR@ -lacstringcheese
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/string_cheese_c.h"

fn C.ac_stringm_lower(s &char) &char
fn C.ac_stringm_upper(s &char) &char
fn C.ac_stringm_trim(s &char) &char
fn C.ac_stringm_strip(s &char, chars &char) &char
fn C.ac_stringm_find(s &char, pattern &char) int
fn C.ac_stringm_replace(s &char, old_sub &char, new_sub &char) &char
fn C.ac_stringm_split_nth(s &char, sep &char, n int) &char
fn C.ac_stringm_len(s &char) i64
fn C.ac_stringm_startswith(s &char, prefix &char) int
fn C.ac_stringm_endswith(s &char, suffix &char) int
fn C.ac_stringm_count(s &char, sub &char) int
fn C.ac_stringm_format(template_str &char) &char

fn stringm_lower(s string) string  { return unsafe { cstring_to_vstring(C.ac_stringm_lower(s.str)) } }
fn stringm_upper(s string) string  { return unsafe { cstring_to_vstring(C.ac_stringm_upper(s.str)) } }
fn stringm_trim(s string) string   { return unsafe { cstring_to_vstring(C.ac_stringm_trim(s.str)) } }
fn stringm_strip(s string, chars string) string {
    return unsafe { cstring_to_vstring(C.ac_stringm_strip(s.str, chars.str)) }
}
fn stringm_find(s string, pattern string) int  { return C.ac_stringm_find(s.str, pattern.str) }
fn stringm_replace(s string, old_sub string, new_sub string) string {
    return unsafe { cstring_to_vstring(C.ac_stringm_replace(s.str, old_sub.str, new_sub.str)) }
}
fn stringm_split_nth(s string, sep string, n int) string {
    return unsafe { cstring_to_vstring(C.ac_stringm_split_nth(s.str, sep.str, n)) }
}
fn stringm_len(s string) i64       { return C.ac_stringm_len(s.str) }
fn stringm_startswith(s string, prefix string) bool { return C.ac_stringm_startswith(s.str, prefix.str) != 0 }
fn stringm_endswith(s string, suffix string) bool   { return C.ac_stringm_endswith(s.str, suffix.str) != 0 }
fn stringm_count(s string, sub string) int   { return C.ac_stringm_count(s.str, sub.str) }
fn stringm_format(t string) string { return unsafe { cstring_to_vstring(C.ac_stringm_format(t.str)) } }

// b / f / t — string-prefix constructors (bytes / f-string / t-string). f/t interpolate at
// the compiler/IR level; the runtime is a passthrough (mirrors ac_stringm_b/f/t in
// string_cheese_c.h — identity for a V string).
fn stringm_b(s string) string { return s }
fn stringm_f(s string) string { return s }
fn stringm_t(s string) string { return s }

const sc_ws = " \t\n\r"

// stringm_split - real split, native V (the C ABI only has split_nth). Whitespace-sentinel
// splits on runs of whitespace (V's .fields()); everything else is a plain .split(sep).
fn stringm_split(s string, sep string) []string {
    if sep == sc_ws {
        return s.fields()
    }
    return s.split(sep)
}

// stringm_join - real join, native V (no C ABI equivalent exists at all).
fn stringm_join(sep string, parts []string) string {
    return parts.join(sep)
}

fn sc_is_alpha_byte(b u8) bool {
    return (b >= `a` && b <= `z`) || (b >= `A` && b <= `Z`)
}
fn sc_is_space_byte(b u8) bool {
    return b == u8(32) || b == u8(9) || b == u8(10) || b == u8(13)
}

// stringm_ischar - all-alphabetic check, native V.
fn stringm_ischar(s string) bool {
    if s.len == 0 { return false }
    for i := 0; i < s.len; i++ {
        if !sc_is_alpha_byte(s[i]) { return false }
    }
    return true
}

// stringm_isws - all-whitespace check, native V.
fn stringm_isws(s string) bool {
    if s.len == 0 { return true }
    for i := 0; i < s.len; i++ {
        if !sc_is_space_byte(s[i]) { return false }
    }
    return true
}

// stringm_getline - read a real line from stdin (os.get_line() strips the trailing
// newline itself and returns "" at EOF).
fn stringm_getline() string {
    return scos.get_line()
}

// stringm_scan - read a real line from stdin and report whether it contains needle.
fn stringm_scan(needle string) bool {
    line := scos.get_line()
    return line.contains(needle)
}

struct AcStringmNS {}

fn (n AcStringmNS) lower(s string) string       { return stringm_lower(s) }
fn (n AcStringmNS) upper(s string) string       { return stringm_upper(s) }
fn (n AcStringmNS) trim(s string) string        { return stringm_trim(s) }
// AC strip = 1-arg trim, same convention as the C header comment ("AC strip = 1-arg
// trim") and the C/C++ bindings in this ilib.
fn (n AcStringmNS) strip(s string) string       { return stringm_trim(s) }
fn (n AcStringmNS) find(s string, p string) int { return stringm_find(s, p) }
fn (n AcStringmNS) replace(s string, o string, r string) string { return stringm_replace(s, o, r) }
fn (n AcStringmNS) split(s string, sep string) []string { return stringm_split(s, sep) }
fn (n AcStringmNS) split_nth(s string, sep string, i int) string { return stringm_split_nth(s, sep, i) }
fn (n AcStringmNS) join(sep string, parts []string) string { return stringm_join(sep, parts) }
fn (n AcStringmNS) length(s string) i64         { return stringm_len(s) }
fn (n AcStringmNS) startswith(s string, p string) bool { return stringm_startswith(s, p) }
fn (n AcStringmNS) endswith(s string, p string) bool   { return stringm_endswith(s, p) }
fn (n AcStringmNS) count(s string, sub string) int { return stringm_count(s, sub) }
fn (n AcStringmNS) format(t string) string      { return stringm_format(t) }
fn (n AcStringmNS) b(s string) string           { return stringm_b(s) }
fn (n AcStringmNS) f(s string) string           { return stringm_f(s) }
fn (n AcStringmNS) t(s string) string           { return stringm_t(s) }
fn (n AcStringmNS) ischar(s string) bool        { return stringm_ischar(s) }
fn (n AcStringmNS) isws(s string) bool          { return stringm_isws(s) }
fn (n AcStringmNS) getline() string             { return stringm_getline() }
fn (n AcStringmNS) scan(needle string) bool     { return stringm_scan(needle) }

const stringm = AcStringmNS{}
