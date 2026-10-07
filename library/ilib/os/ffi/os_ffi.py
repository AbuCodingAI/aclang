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
        _sys.stderr.write(f"[os.sbash] The p in bash stands for protection (blocked: {reason})\n")
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

def os_isdir(p):
    return 1 if _os.path.isdir(str(p)) else 0

def os_isfile(p):
    return 1 if _os.path.isfile(str(p)) else 0

def os_size(p):
    try:
        return _os.path.getsize(str(p))
    except OSError:
        return -1

def os_mtime(p):
    try:
        return int(_os.path.getmtime(str(p)))
    except OSError:
        return -1

def os_copy(src, dst):
    try:
        _sh.copyfile(str(src), str(dst))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.copy] {e}\n")
        return -1

def os_move(src, dst):
    try:
        _sh.move(str(src), str(dst))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.move] {e}\n")
        return -1

def os_listdir(p):
    try:
        return sorted(_os.listdir(str(p)))
    except OSError as e:
        _sys.stderr.write(f"[os.listdir] {e}\n")
        return []

def os_tmpdir():
    for var in ("TMPDIR", "TEMP"):
        if _os.environ.get(var):
            return _os.environ[var]
    return "/tmp"

def os_tmpfile(suffix=""):
    import tempfile
    fd, path = tempfile.mkstemp(prefix="acos_", suffix=str(suffix), dir=os_tmpdir())
    _os.close(fd)
    return path

def os_mktmpdir():
    import tempfile
    return tempfile.mkdtemp(prefix="acos_", dir=os_tmpdir())

def os_chdir(p):
    try:
        _os.chdir(str(p))
        return 0
    except OSError as e:
        _sys.stderr.write(f"[os.chdir] {e}\n")
        return -1

def os_join(a, b):
    a, b = str(a), str(b)
    if not a: return b
    if not b: return a
    return _os.path.join(a, b)

def os_basename(p):
    return str(p).rsplit("/", 1)[-1]

def os_dirname(p):
    p = str(p)
    cut = p.rfind("/")
    if cut < 0: return ""
    if cut == 0: return "/"
    return p[:cut]

def os_homedir():
    return _os.environ.get("HOME", "")

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
    isdir = staticmethod(os_isdir)
    isfile = staticmethod(os_isfile)
    size = staticmethod(os_size)
    mtime = staticmethod(os_mtime)
    copy = staticmethod(os_copy)
    move = staticmethod(os_move)
    listdir = staticmethod(os_listdir)
    tmpdir = staticmethod(os_tmpdir)
    tmpfile = staticmethod(os_tmpfile)
    mktmpdir = staticmethod(os_mktmpdir)
    chdir = staticmethod(os_chdir)
    join = staticmethod(os_join)
    basename = staticmethod(os_basename)
    dirname = staticmethod(os_dirname)
    homedir = staticmethod(os_homedir)
