# AC ilib: regex — pure-Python fallback (used when libacregex is not built).
# Same dialect as the C engine (std::regex, ECMAScript flavour) so results match:
#   - replacement strings use $1..$99, $&, $`, $' and $$ (not Python's \1)
#   - split returns the pieces between matches and never includes capture groups
#   - an unmatched capture group reads as "" (never None)
#   - escape() escapes the pattern specials \ ^ $ . | ? * + ( ) [ ] { }
# use ilib regex
import re as _re

_SPECIALS = set('\\^$.|?*+()[]{}')

def _repl(repl):
    # Turns an AC/ECMAScript replacement template into a function re.sub can call.
    def expand(m):
        out, i = [], 0
        while i < len(repl):
            c = repl[i]
            if c == '$' and i + 1 < len(repl):
                n = repl[i + 1]
                if n == '&':  out.append(m.group(0)); i += 2; continue
                if n == '$':  out.append('$'); i += 2; continue
                if n == '`':  out.append(m.string[:m.start()]); i += 2; continue
                if n == "'":  out.append(m.string[m.end():]); i += 2; continue
                if n.isdigit():
                    j = i + 1
                    while j < len(repl) and repl[j].isdigit() and j - i < 3: j += 1
                    k = int(repl[i + 1:j])
                    if 1 <= k <= m.re.groups:
                        out.append(m.group(k) or ''); i = j; continue
            out.append(c); i += 1
        return ''.join(out)
    return expand

def regex_match(s, pat):
    try:    return bool(_re.fullmatch(pat, s))
    except: return False

def regex_test(s, pat):
    try:    return bool(_re.search(pat, s))
    except: return False

def regex_search(s, pat):
    try:
        m = _re.search(pat, s)
        return m.group(0) if m else ""
    except: return ""

def regex_replace(s, pat, repl):
    try:    return _re.sub(pat, _repl(repl), s, count=1)
    except: return s

def regex_replace_all(s, pat, repl):
    try:    return _re.sub(pat, _repl(repl), s)
    except: return s

def regex_count(s, pat):
    try:    return sum(1 for _ in _re.finditer(pat, s))
    except: return 0

def regex_escape(s):
    return ''.join('\\' + c if c in _SPECIALS else c for c in s)

def regex_find_all(s, pat):
    try:
        return [m.group(0) for m in _re.finditer(pat, s)]
    except: return []

def regex_split(s, pat):
    # The pieces between matches. re.split would also splice in capture groups.
    try:
        out, last = [], 0
        for m in _re.finditer(pat, s):
            out.append(s[last:m.start()])
            last = m.end()
        out.append(s[last:])
        return out
    except: return [s]

def regex_groups(s, pat):
    try:
        m = _re.search(pat, s)
        return ['' if g is None else g for g in m.groups()] if m else []
    except: return []

# namespace object — AC-generated Python uses regex.match(s, p), etc.
class _AcRegexNS:
    def match(self, s, p):          return regex_match(s, p)
    def test(self, s, p):           return regex_test(s, p)
    def search(self, s, p):         return regex_search(s, p)
    def replace(self, s, p, r):     return regex_replace(s, p, r)
    def replace_all(self, s, p, r): return regex_replace_all(s, p, r)
    def count(self, s, p):          return regex_count(s, p)
    def escape(self, s):            return regex_escape(s)
    def find_all(self, s, p):       return regex_find_all(s, p)
    def split(self, s, p):          return regex_split(s, p)
    def groups(self, s, p):         return regex_groups(s, p)
