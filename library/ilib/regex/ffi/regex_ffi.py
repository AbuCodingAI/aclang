# AC ilib: regex — Python ctypes FFI (libacregex.so / acregex.dll)
# Falls back to pure-Python re implementation if the .so is not present.
import ctypes, ctypes.util, os, sys

def _load_lib():
    # _ac_regex_lib_dir is injected by the AC compiler before this FFI is inlined
    _base = globals().get('_ac_regex_lib_dir', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    candidates = [
        os.path.join(_base, 'libacregex.so'),
        os.path.join(_base, 'acregex.dll'),
        os.path.join(_base, 'libacregex.dylib'),
    ]
    for p in candidates:
        if os.path.exists(p):
            return ctypes.CDLL(os.path.abspath(p))
    return None

_lib = _load_lib()

if _lib:
    _lib.ac_regex_match.restype        = ctypes.c_int
    _lib.ac_regex_match.argtypes       = [ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_test.restype         = ctypes.c_int
    _lib.ac_regex_test.argtypes        = [ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_search.restype       = ctypes.c_char_p
    _lib.ac_regex_search.argtypes      = [ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_replace.restype      = ctypes.c_char_p
    _lib.ac_regex_replace.argtypes     = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_replace_all.restype  = ctypes.c_char_p
    _lib.ac_regex_replace_all.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_count.restype        = ctypes.c_int
    _lib.ac_regex_count.argtypes       = [ctypes.c_char_p, ctypes.c_char_p]
    _lib.ac_regex_escape.restype       = ctypes.c_char_p
    _lib.ac_regex_escape.argtypes      = [ctypes.c_char_p]
    _lib.ac_regex_find_all.restype     = ctypes.POINTER(ctypes.c_char_p)
    _lib.ac_regex_find_all.argtypes    = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
    _lib.ac_regex_split.restype        = ctypes.POINTER(ctypes.c_char_p)
    _lib.ac_regex_split.argtypes       = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
    _lib.ac_regex_groups.restype       = ctypes.POINTER(ctypes.c_char_p)
    _lib.ac_regex_groups.argtypes      = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
    _lib.ac_regex_free_list.restype    = None
    _lib.ac_regex_free_list.argtypes   = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]

    def _clist(fn, *args):
        n = ctypes.c_int(0)
        arr = fn(*args, ctypes.byref(n))
        result = [arr[i].decode() for i in range(n.value)]
        _lib.ac_regex_free_list(arr, n)
        return result

    def _b(s): return s.encode() if isinstance(s, str) else s
    def _s(b): return b.decode() if b else ""

    def regex_match(s, p):            return bool(_lib.ac_regex_match(_b(s), _b(p)))
    def regex_test(s, p):             return bool(_lib.ac_regex_test(_b(s), _b(p)))
    def regex_search(s, p):           return _s(_lib.ac_regex_search(_b(s), _b(p)))
    def regex_replace(s, p, r):       return _s(_lib.ac_regex_replace(_b(s), _b(p), _b(r)))
    def regex_replace_all(s, p, r):   return _s(_lib.ac_regex_replace_all(_b(s), _b(p), _b(r)))
    def regex_count(s, p):            return int(_lib.ac_regex_count(_b(s), _b(p)))
    def regex_escape(s):              return _s(_lib.ac_regex_escape(_b(s)))
    def regex_find_all(s, p):         return _clist(_lib.ac_regex_find_all, _b(s), _b(p))
    def regex_split(s, p):            return _clist(_lib.ac_regex_split, _b(s), _b(p))
    def regex_groups(s, p):           return _clist(_lib.ac_regex_groups, _b(s), _b(p))

else:
    # Pure-Python fallback (delegates to regex.py)
    import importlib.util, pathlib
    _base = globals().get('_ac_regex_lib_dir', str(pathlib.Path(__file__).parent.parent))
    _spec = importlib.util.spec_from_file_location(
        "ac_regex_py", pathlib.Path(_base) / "regex.py")
    _m = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_m)
    regex_match      = _m.regex_match
    regex_test       = _m.regex_test
    regex_search     = _m.regex_search
    regex_replace    = _m.regex_replace
    regex_replace_all= _m.regex_replace_all
    regex_count      = _m.regex_count
    regex_escape     = _m.regex_escape
    regex_find_all   = _m.regex_find_all
    regex_split      = _m.regex_split
    regex_groups     = _m.regex_groups

# namespace object
class _AcRegexNS:
    def match(self, s, p):           return regex_match(s, p)
    def test(self, s, p):            return regex_test(s, p)
    def search(self, s, p):          return regex_search(s, p)
    def replace(self, s, p, r):      return regex_replace(s, p, r)
    def replace_all(self, s, p, r):  return regex_replace_all(s, p, r)
    def count(self, s, p):           return regex_count(s, p)
    def escape(self, s):             return regex_escape(s)
    def find_all(self, s, p):        return regex_find_all(s, p)
    def split(self, s, p):           return regex_split(s, p)
    def groups(self, s, p):          return regex_groups(s, p)

regex = _AcRegexNS()
