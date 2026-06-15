// AC ilib: string-cheese — V C interop (libacstringcheese.so)
// Inlined by AC->V compiler when "use ilib string-cheese" is declared.
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

struct _AcStringmNS {}
fn (_AcStringmNS) lower(s string) string       { return stringm_lower(s)           }
fn (_AcStringmNS) upper(s string) string       { return stringm_upper(s)           }
fn (_AcStringmNS) trim(s string) string        { return stringm_trim(s)            }
fn (_AcStringmNS) strip(s string, c string) string { return stringm_strip(s, c)   }
fn (_AcStringmNS) find(s string, p string) int { return stringm_find(s, p)         }
fn (_AcStringmNS) replace(s string, o string, n string) string { return stringm_replace(s, o, n) }
fn (_AcStringmNS) split_nth(s string, sep string, n int) string { return stringm_split_nth(s, sep, n) }
fn (_AcStringmNS) length(s string) i64         { return stringm_len(s)             }
fn (_AcStringmNS) startswith(s string, p string) bool { return stringm_startswith(s, p) }
fn (_AcStringmNS) endswith(s string, p string) bool   { return stringm_endswith(s, p)   }
fn (_AcStringmNS) count(s string, sub string) int { return stringm_count(s, sub)   }
fn (_AcStringmNS) format(t string) string      { return stringm_format(t)          }

const stringm = _AcStringmNS{}
