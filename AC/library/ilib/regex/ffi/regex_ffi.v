// AC ilib: regex — V FFI (libacregex.so / acregex.dll)
module main

#flag -L @VMODROOT/../
#flag -lacregex
#flag -Wl,-rpath,@VMODROOT/../
#include "regex_c.h"

fn C.ac_regex_match(str &char, pat &char) int
fn C.ac_regex_test(str &char, pat &char) int
fn C.ac_regex_search(str &char, pat &char) &char
fn C.ac_regex_replace(str &char, pat &char, repl &char) &char
fn C.ac_regex_replace_all(str &char, pat &char, repl &char) &char
fn C.ac_regex_count(str &char, pat &char) int
fn C.ac_regex_escape(str &char) &char
fn C.ac_regex_find_all(str &char, pat &char, out_count &int) &&char
fn C.ac_regex_split(str &char, pat &char, out_count &int) &&char
fn C.ac_regex_groups(str &char, pat &char, out_count &int) &&char
fn C.ac_regex_free_list(list &&char, count int)

fn _clist(arr &&char, n int) []string {
    mut result := []string{cap: n}
    for i in 0 .. n {
        result << unsafe { cstring_to_vstring(*arr[i]) }
    }
    C.ac_regex_free_list(arr, n)
    return result
}

fn regex_match(s string, p string) bool {
    return C.ac_regex_match(s.str, p.str) != 0
}
fn regex_test(s string, p string) bool {
    return C.ac_regex_test(s.str, p.str) != 0
}
fn regex_search(s string, p string) string {
    return unsafe { cstring_to_vstring(C.ac_regex_search(s.str, p.str)) }
}
fn regex_replace(s string, p string, r string) string {
    return unsafe { cstring_to_vstring(C.ac_regex_replace(s.str, p.str, r.str)) }
}
fn regex_replace_all(s string, p string, r string) string {
    return unsafe { cstring_to_vstring(C.ac_regex_replace_all(s.str, p.str, r.str)) }
}
fn regex_count(s string, p string) int {
    return C.ac_regex_count(s.str, p.str)
}
fn regex_escape(s string) string {
    return unsafe { cstring_to_vstring(C.ac_regex_escape(s.str)) }
}
fn regex_find_all(s string, p string) []string {
    mut n := 0
    arr := C.ac_regex_find_all(s.str, p.str, &n)
    return _clist(arr, n)
}
fn regex_split(s string, p string) []string {
    mut n := 0
    arr := C.ac_regex_split(s.str, p.str, &n)
    return _clist(arr, n)
}
fn regex_groups(s string, p string) []string {
    mut n := 0
    arr := C.ac_regex_groups(s.str, p.str, &n)
    return _clist(arr, n)
}

// regex namespace struct — AC-generated V uses regex.match(s, p), etc.
struct _AcRegexNS {}
fn (_AcRegexNS) match(s string, p string) bool          { return regex_match(s, p)         }
fn (_AcRegexNS) test(s string, p string) bool            { return regex_test(s, p)          }
fn (_AcRegexNS) search(s string, p string) string        { return regex_search(s, p)        }
fn (_AcRegexNS) replace(s string, p string, r string) string     { return regex_replace(s, p, r)     }
fn (_AcRegexNS) replace_all(s string, p string, r string) string { return regex_replace_all(s, p, r) }
fn (_AcRegexNS) count(s string, p string) int            { return regex_count(s, p)         }
fn (_AcRegexNS) escape(s string) string                  { return regex_escape(s)           }
fn (_AcRegexNS) find_all(s string, p string) []string    { return regex_find_all(s, p)      }
fn (_AcRegexNS) split(s string, p string) []string       { return regex_split(s, p)         }
fn (_AcRegexNS) groups(s string, p string) []string      { return regex_groups(s, p)        }

const regex = _AcRegexNS{}
