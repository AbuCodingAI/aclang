# AC ilib: regex — Regular expressions (Python backend)
# use ilib regex
import re as _re

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
    try:    return _re.sub(pat, repl, s, count=1)
    except: return s

def regex_replace_all(s, pat, repl):
    try:    return _re.sub(pat, repl, s)
    except: return s

def regex_count(s, pat):
    try:    return len(_re.findall(pat, s))
    except: return 0

def regex_escape(s):
    return _re.escape(s)

def regex_find_all(s, pat):
    try:
        matches = _re.finditer(pat, s)
        return [m.group(0) for m in matches]
    except: return []

def regex_split(s, pat):
    try:    return _re.split(pat, s)
    except: return [s]

def regex_groups(s, pat):
    try:
        m = _re.search(pat, s)
        return list(m.groups()) if m else []
    except: return []

# namespace object — AC-generated Python uses regex.match(s, p), etc.
class _AcRegexNS:
    def match(self, s, p):        return regex_match(s, p)
    def test(self, s, p):         return regex_test(s, p)
    def search(self, s, p):       return regex_search(s, p)
    def replace(self, s, p, r):   return regex_replace(s, p, r)
    def replace_all(self, s, p, r): return regex_replace_all(s, p, r)
    def count(self, s, p):        return regex_count(s, p)
    def escape(self, s):          return regex_escape(s)
    def find_all(self, s, p):     return regex_find_all(s, p)
    def split(self, s, p):        return regex_split(s, p)
    def groups(self, s, p):       return regex_groups(s, p)

regex = _AcRegexNS()
