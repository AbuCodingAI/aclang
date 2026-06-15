// AC ilib: regex — Go CGO FFI (libacregex.so / acregex.dll)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/../
#cgo LDFLAGS: -L${SRCDIR}/../ -lacregex -Wl,-rpath,${SRCDIR}/../
#include "regex_c.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"

func regex_match(s, pat string) bool {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	return C.ac_regex_match(cs, cp) != 0
}
func regex_test(s, pat string) bool {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	return C.ac_regex_test(cs, cp) != 0
}
func regex_search(s, pat string) string {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	return C.GoString(C.ac_regex_search(cs, cp))
}
func regex_replace(s, pat, repl string) string {
	cs, cp, cr := C.CString(s), C.CString(pat), C.CString(repl)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp)); defer C.free(unsafe.Pointer(cr))
	return C.GoString(C.ac_regex_replace(cs, cp, cr))
}
func regex_replace_all(s, pat, repl string) string {
	cs, cp, cr := C.CString(s), C.CString(pat), C.CString(repl)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp)); defer C.free(unsafe.Pointer(cr))
	return C.GoString(C.ac_regex_replace_all(cs, cp, cr))
}
func regex_count(s, pat string) int {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	return int(C.ac_regex_count(cs, cp))
}
func regex_escape(s string) string {
	cs := C.CString(s); defer C.free(unsafe.Pointer(cs))
	return C.GoString(C.ac_regex_escape(cs))
}

func _cListToGo(arr **C.char, n C.int) []string {
	count := int(n)
	result := make([]string, count)
	slice := (*[1 << 20]*C.char)(unsafe.Pointer(arr))[:count:count]
	for i, p := range slice { result[i] = C.GoString(p) }
	C.ac_regex_free_list(arr, n)
	return result
}
func regex_find_all(s, pat string) []string {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	var n C.int
	arr := C.ac_regex_find_all(cs, cp, &n)
	return _cListToGo(arr, n)
}
func regex_split(s, pat string) []string {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	var n C.int
	arr := C.ac_regex_split(cs, cp, &n)
	return _cListToGo(arr, n)
}
func regex_groups(s, pat string) []string {
	cs, cp := C.CString(s), C.CString(pat)
	defer C.free(unsafe.Pointer(cs)); defer C.free(unsafe.Pointer(cp))
	var n C.int
	arr := C.ac_regex_groups(cs, cp, &n)
	return _cListToGo(arr, n)
}

// regex namespace object — AC-generated Go uses regex.match(s, p), etc.
type _AcRegexNS struct{}
func (r _AcRegexNS) match(s, p string) bool              { return regex_match(s, p)        }
func (r _AcRegexNS) test(s, p string) bool               { return regex_test(s, p)         }
func (r _AcRegexNS) search(s, p string) string           { return regex_search(s, p)       }
func (r _AcRegexNS) replace(s, p, rep string) string     { return regex_replace(s, p, rep) }
func (r _AcRegexNS) replace_all(s, p, rep string) string { return regex_replace_all(s, p, rep) }
func (r _AcRegexNS) count(s, p string) int               { return regex_count(s, p)        }
func (r _AcRegexNS) escape(s string) string              { return regex_escape(s)          }
func (r _AcRegexNS) find_all(s, p string) []string       { return regex_find_all(s, p)     }
func (r _AcRegexNS) split(s, p string) []string          { return regex_split(s, p)        }
func (r _AcRegexNS) groups(s, p string) []string         { return regex_groups(s, p)       }

var regex = _AcRegexNS{}
