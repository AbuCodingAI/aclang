// AC ilib: string-cheese — Go implementation (pure Go, no cgo calls)
//
// NOTE on the leading `import "C"`: the AC compiler's parseGoFFI() (ac-compiler/src/
// ir_codegen.cpp) only knows how to split a Go FFI file into two parts — a "cgo preamble"
// (everything up to and including a literal `import "C"` line) and "wrapper code" (emitted
// AFTER the compiler's own `import (...)` block, alongside other injected helper funcs).
// A file with no `import "C"` line at all gets dumped ENTIRELY as if it were preamble,
// landing before the compiler's own imports — which breaks Go's "imports must appear
// before other declarations" rule the moment the file has any top-level decl (it always
// does). So this file declares a no-op `import "C"` purely as a section marker; it makes
// no actual cgo calls. Everything below it is 100% standard-library Go.
package stringcheese

import (
	scbufio "bufio"
	scos "os"
	scstrings "strings"
	"unicode"
)

import "C"

const _WS = " \t\n\r"

func isWS(s string) bool {
	return s == _WS
}

// stringm_lower - Convert to lowercase
func Lower(s string) string {
	return scstrings.ToLower(s)
}

// stringm_upper - Convert to uppercase
func Upper(s string) string {
	return scstrings.ToUpper(s)
}

// stringm_trim - Trim whitespace
func Trim(s string) string {
	return scstrings.TrimSpace(s)
}

// stringm_strip - Strip characters
func Strip(s string, chars string) string {
	if isWS(chars) {
		return scstrings.TrimSpace(s)
	}
	return scstrings.Trim(s, chars)
}

// stringm_find - Find pattern position
func Find(s string, pattern string) int {
	if isWS(pattern) {
		for i, r := range s {
			if unicode.IsSpace(r) {
				return i
			}
		}
		return -1
	}
	idx := scstrings.Index(s, pattern)
	return idx
}

// stringm_replace - Replace pattern
func Replace(s string, old string, new string) string {
	if isWS(old) {
		parts := scstrings.Fields(s)
		return scstrings.Join(parts, new)
	}
	return scstrings.ReplaceAll(s, old, new)
}

// stringm_split - Split string
func Split(s string, sep string) []string {
	if isWS(sep) {
		return scstrings.Fields(s)
	}
	return scstrings.Split(s, sep)
}

// stringm_split_nth - Get nth split part
func SplitNth(s string, sep string, n int) string {
	parts := Split(s, sep)
	if n >= 0 && n < len(parts) {
		return parts[n]
	}
	return ""
}

// stringm_len - String length (runes)
func Len(s string) int64 {
	return int64(len([]rune(s)))
}

// stringm_startswith - Check prefix
func StartsWith(s string, prefix string) bool {
	return scstrings.HasPrefix(s, prefix)
}

// stringm_endswith - Check suffix
func EndsWith(s string, suffix string) bool {
	return scstrings.HasSuffix(s, suffix)
}

// stringm_count - Count occurrences
func Count(s string, sub string) int64 {
	if isWS(sub) {
		count := 0
		for _, r := range s {
			if unicode.IsSpace(r) {
				count++
			}
		}
		return int64(count)
	}
	return int64(scstrings.Count(s, sub))
}

// stringm_join - Join strings
func Join(sep string, parts []string) string {
	return scstrings.Join(parts, sep)
}

// stringm_format - Format string (passthrough; AC compiler handles {} at IR level)
func Format(t string) string {
	return t
}

// stringm_b / f / t - string-prefix constructors (bytes / f-string / t-string).
// f-string and t-string interpolate at the compiler/IR level; the runtime helpers are
// passthroughs (mirrors ac_stringm_b/f/t in string_cheese_c.h — identity for a Go string).
func B(s string) string { return s }
func F(s string) string { return s }
func T(s string) string { return s }

// stringm_ischar - Check if all alphabetic
func IsChar(s string) bool {
	if s == "" {
		return false
	}
	for _, r := range s {
		if !unicode.IsLetter(r) {
			return false
		}
	}
	return true
}

// stringm_isws - Check if all whitespace
func IsWS(s string) bool {
	if s == "" {
		return true
	}
	for _, r := range s {
		if !unicode.IsSpace(r) {
			return false
		}
	}
	return true
}

// _scStdin is a package-level, lazily-created buffered reader over os.Stdin shared by
// Getline/Scan so successive calls keep advancing through the same stream instead of
// each dropping whatever the previous call had already buffered.
var _scStdin *scbufio.Reader

func _scStdinReader() *scbufio.Reader {
	if _scStdin == nil {
		_scStdin = scbufio.NewReader(scos.Stdin)
	}
	return _scStdin
}

// stringm_getline - Read a line from stdin (real read, no trailing newline).
func Getline() string {
	line, err := _scStdinReader().ReadString('\n')
	if err != nil && line == "" {
		return ""
	}
	return scstrings.TrimRight(line, "\r\n")
}

// stringm_scan - Read a line from stdin and report whether it contains needle.
func Scan(needle string) bool {
	line, err := _scStdinReader().ReadString('\n')
	if err != nil && line == "" {
		return false
	}
	return scstrings.Contains(line, needle)
}

// stringm namespace object — AC-generated Go uses stringm.upper(s), stringm.find(s, p), etc.
type _AcStringmNS struct{}

func (_AcStringmNS) upper(s string) string                        { return Upper(s) }
func (_AcStringmNS) lower(s string) string                        { return Lower(s) }
func (_AcStringmNS) trim(s string) string                         { return Trim(s) }
// strip is variadic because AC call sites use both the 1-arg form (whitespace strip,
// same as trim) and the 2-arg form (strip a specific charset) — Go has no default args.
func (_AcStringmNS) strip(s string, chars ...string) string {
	if len(chars) == 0 {
		return Trim(s)
	}
	return Strip(s, chars[0])
}
// find/startswith/endswith/ischar/isws/scan return int64 (0/1), not bool: the AC compiler's
// Go backend declares any ilib call result it doesn't specifically recognize as a string
// or []string as `int64` (see BackendStrategy::isAcStrFunc/isAcStrListFunc in ir_codegen.cpp
// and its use in GoStrategy::emitCall/decl) — booleans flow through this compiler as 0/1
// int64 on the Go backend, matching the `_b(bool) int64` helper it emits for the same
// purpose elsewhere. Returning a Go `bool` here would be a hard type error at the call site.
func (_AcStringmNS) find(s string, pattern string) int64 { return int64(Find(s, pattern)) }
func (_AcStringmNS) replace(s string, old string, new string) string { return Replace(s, old, new) }
func (_AcStringmNS) split(s string, sep string) []string          { return Split(s, sep) }
func (_AcStringmNS) split_nth(s string, sep string, n int) string { return SplitNth(s, sep, n) }
func (_AcStringmNS) join(sep string, parts []string) string       { return Join(sep, parts) }
func (_AcStringmNS) length(s string) int64                        { return Len(s) }
func (_AcStringmNS) len(s string) int64                           { return Len(s) }
func (_AcStringmNS) startswith(s string, prefix string) int64     { return _b(StartsWith(s, prefix)) }
func (_AcStringmNS) endswith(s string, suffix string) int64       { return _b(EndsWith(s, suffix)) }
func (_AcStringmNS) count(s string, sub string) int64             { return Count(s, sub) }
func (_AcStringmNS) format(t string) string                       { return Format(t) }
func (_AcStringmNS) b(s string) string                            { return B(s) }
func (_AcStringmNS) f(s string) string                            { return F(s) }
func (_AcStringmNS) t(s string) string                            { return T(s) }
func (_AcStringmNS) ischar(s string) int64                        { return _b(IsChar(s)) }
func (_AcStringmNS) isws(s string) int64                          { return _b(IsWS(s)) }
func (_AcStringmNS) getline() string                              { return Getline() }
func (_AcStringmNS) scan(needle string) int64                     { return _b(Scan(needle)) }

var stringm = _AcStringmNS{}
