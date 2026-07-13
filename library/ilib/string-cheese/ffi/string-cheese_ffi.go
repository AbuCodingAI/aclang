// AC ilib: string-cheese — Go implementation
package stringcheese

import (
	"strings"
	"unicode"
)

const _WS = " \t\n\r"

func isWS(s string) bool {
	return s == _WS
}

// stringm_lower - Convert to lowercase
func Lower(s string) string {
	return strings.ToLower(s)
}

// stringm_upper - Convert to uppercase
func Upper(s string) string {
	return strings.ToUpper(s)
}

// stringm_trim - Trim whitespace
func Trim(s string) string {
	return strings.TrimSpace(s)
}

// stringm_strip - Strip characters
func Strip(s string, chars string) string {
	if isWS(chars) {
		return strings.TrimSpace(s)
	}
	return strings.Trim(s, chars)
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
	idx := strings.Index(s, pattern)
	return idx
}

// stringm_replace - Replace pattern
func Replace(s string, old string, new string) string {
	if isWS(old) {
		parts := strings.Fields(s)
		return strings.Join(parts, new)
	}
	return strings.ReplaceAll(s, old, new)
}

// stringm_split - Split string
func Split(s string, sep string) []string {
	if isWS(sep) {
		return strings.Fields(s)
	}
	return strings.Split(s, sep)
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
	return strings.HasPrefix(s, prefix)
}

// stringm_endswith - Check suffix
func EndsWith(s string, suffix string) bool {
	return strings.HasSuffix(s, suffix)
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
	return int64(strings.Count(s, sub))
}

// stringm_join - Join strings
func Join(sep string, parts []string) string {
	return strings.Join(parts, sep)
}

// stringm_format - Format string (passthrough)
func Format(t string) string {
	return t
}

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

// stringm_getline - Read line from stdin (stub for now)
func Getline() string {
	// Would need bufio.Scanner in actual implementation
	return ""
}

// stringm_scan - Scan stdin (stub for now)
func Scan(needle string) bool {
	// Would need bufio.Scanner in actual implementation
	return false
}
