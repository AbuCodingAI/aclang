# AC ilib: string-cheese — Python FFI
# Inlined by AC->PY compiler when "use ilib string-cheese" is declared.
import re as _re

_WS = " \t\n\r"  # \ws sentinel resolved to this

def _ws(s):
    return s if s != " \t\n\r" else None  # None → Python default strip (all whitespace)

# ── stringm functions ─────────────────────────────────────────────────────────
# b / f / t are the three string-prefix constructors (like Python b"" / f"" / t"").
# f-string and t-string do their interpolation at the compiler/IR level; the runtime
# helpers are passthroughs (mirrors ac_stringm_format in string_cheese_c.h).

def stringm_f(s, *args):
    return str(s)                 # f-string (formatted) — compiler interpolates {} at IR level

def stringm_t(s, *args):
    return str(s)                 # t-string (template, PEP 750) — template resolved at IR level

def stringm_b(s):
    return str(s).encode('utf-8')

def stringm_endian(b, order='little'):
    # Interpret a byte string as an unsigned integer ('little' or 'big' endian).
    if isinstance(b, str):
        b = b.encode('utf-8')
    return int.from_bytes(b, 'little' if str(order).lower().startswith('l') else 'big')

def stringm_upper(s):
    return str(s).upper()

def stringm_lower(s):
    return str(s).lower()

def stringm_find(s, pattern):
    p = str(pattern)
    if p == _WS:
        m = _re.search(r'\s', str(s))
        return m.start() if m else -1
    return str(s).find(p)

def stringm_strip(s, chars=None):
    if chars is None or chars == _WS:
        return str(s).strip()
    return str(s).strip(str(chars))

def stringm_replace(s, old, new):
    if str(old) == _WS:
        return _re.sub(r'\s+', str(new), str(s))
    return str(s).replace(str(old), str(new))

def stringm_split(s, sep=None):
    if sep is None or sep == _WS:
        return str(s).split()
    return str(s).split(str(sep))

def stringm_join(sep, parts):
    return str(sep).join(str(p) for p in parts)

def stringm_len(s):
    return len(str(s))

def stringm_startswith(s, prefix):
    return str(s).startswith(str(prefix))

def stringm_endswith(s, suffix):
    return str(s).endswith(str(suffix))

def stringm_count(s, sub):
    if str(sub) == _WS:
        return sum(1 for c in str(s) if c in _WS)
    return str(s).count(str(sub))

def stringm_format(template, **kwargs):
    return str(template).format(**kwargs)

class stringm:
    f          = staticmethod(stringm_f)
    t          = staticmethod(stringm_t)
    b          = staticmethod(stringm_b)
    endian     = staticmethod(stringm_endian)
    upper      = staticmethod(stringm_upper)
    lower      = staticmethod(stringm_lower)
    find       = staticmethod(stringm_find)
    strip      = staticmethod(stringm_strip)
    replace    = staticmethod(stringm_replace)
    split      = staticmethod(stringm_split)
    join       = staticmethod(stringm_join)
    length     = staticmethod(stringm_len)
    startswith = staticmethod(stringm_startswith)
    endswith   = staticmethod(stringm_endswith)
    count      = staticmethod(stringm_count)
    format     = staticmethod(stringm_format)
