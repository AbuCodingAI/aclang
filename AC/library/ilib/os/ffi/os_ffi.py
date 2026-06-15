# AC ilib: os — Python FFI
# Inlined by AC->PY compiler when "use ilib os" is declared.
import subprocess as _sp, os as _os, shutil as _sh, re as _re, sys as _sys

_SBASH_FORBIDDEN = [
    _re.compile(r'\bsudo\b'),
    _re.compile(r'\bsu\s'),
    _re.compile(r'function\s+\w+\s*\('),
    _re.compile(r'\(\s*\)\s*\{'),
    _re.compile(r'&\s*$'),
    _re.compile(r'&\s+\w'),
    _re.compile(r'\bnohup\b'),
    _re.compile(r'\bscreen\b'),
    _re.compile(r'\btmux\b'),
    _re.compile(r'\bdisown\b'),
]

def _os_sbash_check(cmd):
    for pat in _SBASH_FORBIDDEN:
        if pat.search(cmd):
            return False, pat.pattern
    return True, None

def os_bash(cmd):
    result = _sp.run(cmd, shell=True, capture_output=False)
    return result.returncode

def os_sbash(cmd):
    ok, reason = _os_sbash_check(str(cmd))
    if not ok:
        _sys.stderr.write(f"[os.sbash] blocked: {reason}\n")
        return -1
    result = _sp.run(cmd, shell=True, capture_output=False)
    return result.returncode

def os_app_open(app):
    for launcher in ["xdg-open", "open", "start"]:
        if _sh.which(launcher):
            _sp.Popen([launcher, str(app)])
            return 0
    _sp.Popen(str(app).split())
    return 0

def os_mkfile(path):
    try:
        with open(str(path), 'a'): pass
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.mkfile] {e}\n")
        return -1

def os_rmfile(path):
    try:
        _os.remove(str(path))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.rmfile] {e}\n")
        return -1

def os_mkdir(path):
    try:
        _os.makedirs(str(path), exist_ok=True)
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.mkdir] {e}\n")
        return -1

def os_rmdir(path):
    try:
        _sh.rmtree(str(path))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.rmdir] {e}\n")
        return -1

def os_exists(path):
    return _os.path.exists(str(path))

def os_cwd():
    return _os.getcwd()

def os_env(key):
    return _os.environ.get(str(key), "")

def os_write_to(path, content):
    try:
        with open(str(path), 'w') as f:
            f.write(str(content))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.write_to] {e}\n")
        return -1

def os_append_to(path, content):
    try:
        with open(str(path), 'a') as f:
            s = str(content)
            f.write(s if s.endswith('\n') else s + '\n')
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.append_to] {e}\n")
        return -1

def os_read(path):
    try:
        with open(str(path), 'r') as f:
            return f.read()
    except OSError as e:
        _sys.stderr.write(f"[os.read] {e}\n")
        return ""

class os:
    bash      = staticmethod(os_bash)
    sbash     = staticmethod(os_sbash)
    app_open  = staticmethod(os_app_open)
    mkfile    = staticmethod(os_mkfile)
    rmfile    = staticmethod(os_rmfile)
    mkdir     = staticmethod(os_mkdir)
    rmdir     = staticmethod(os_rmdir)
    exists    = staticmethod(os_exists)
    cwd       = staticmethod(os_cwd)
    env       = staticmethod(os_env)
    write_to  = staticmethod(os_write_to)
    append_to = staticmethod(os_append_to)
    read      = staticmethod(os_read)
