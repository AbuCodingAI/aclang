# AC ilib: camera — Python ctypes FFI (libaccamera.so / libaccamera.dll)
# Inlined by AC->PY compiler when "use ilib camera" is declared.
import ctypes as _ct, os as _os, platform as _pl, types as _types

def _camera_load():
    _base = globals().get('_ac_camera_lib_dir',
                          _os.path.join(_os.getcwd(), 'library', 'ilib', 'camera'))
    _name = 'libaccamera.dll' if _pl.system() == 'Windows' else 'libaccamera.so'
    _p = _os.path.join(_base, _name)
    if _os.path.exists(_p):
        return _ct.CDLL(_os.path.abspath(_p))
    return None

_c = _camera_load()

if _c:
    _CS = _ct.c_char_p; _I = _ct.c_int; _V = None
    _c.ac_camera_init.argtypes = []; _c.ac_camera_init.restype = _I
    _c.ac_camera_capture.argtypes = [_CS]; _c.ac_camera_capture.restype = _I
    _c.ac_camera_capture_latest.argtypes = [_CS]; _c.ac_camera_capture_latest.restype = _I
    _c.ac_camera_capture_first.argtypes = [_CS]; _c.ac_camera_capture_first.restype = _I
    _c.ac_camera_release.argtypes = []; _c.ac_camera_release.restype = _V
    _c.ac_sidebar_config.argtypes = [_CS, _CS]; _c.ac_sidebar_config.restype = _V
    _c.ac_sidebar_setregion.argtypes = [_CS]; _c.ac_sidebar_setregion.restype = _V
    _c.ac_sidebar_setinteractive.argtypes = [_I]; _c.ac_sidebar_setinteractive.restype = _V
    _c.ac_sidebar_display.argtypes = [_CS]; _c.ac_sidebar_display.restype = _V
    _c.ac_sidebar_ask.argtypes = [_CS]; _c.ac_sidebar_ask.restype = _CS
    _c.ac_sidebar_getinput.argtypes = []; _c.ac_sidebar_getinput.restype = _CS
    _c.ac_screen_setmode.argtypes = [_CS]; _c.ac_screen_setmode.restype = _V
    _c.ac_screen_update.argtypes = []; _c.ac_screen_update.restype = _V

    def _b(s): return s.encode() if isinstance(s, str) else s

    def camera_init(): return _c.ac_camera_init()
    def camera_capture(f): return _c.ac_camera_capture(_b(f))
    def camera_capture_latest(f): return _c.ac_camera_capture_latest(_b(f))
    def camera_capture_first(f): return _c.ac_camera_capture_first(_b(f))
    def camera_release(): _c.ac_camera_release()
    def sidebar_config(k, v): _c.ac_sidebar_config(_b(k), _b(v))
    def sidebar_setregion(r): _c.ac_sidebar_setregion(_b(r))
    def sidebar_setinteractive(v): _c.ac_sidebar_setinteractive(1 if v else 0)
    def sidebar_display(m): _c.ac_sidebar_display(_b(m))
    def sidebar_ask(p):
        r = _c.ac_sidebar_ask(_b(p))
        return r.decode() if r else ""
    def sidebar_getinput():
        r = _c.ac_sidebar_getinput()
        return r.decode() if r else ""
    def screen_setmode(m): _c.ac_screen_setmode(_b(m))
    def screen_update(): _c.ac_screen_update()

else:
    import sys as _sys
    _sys.stderr.write("[camera] WARNING: libaccamera.so not found — stub mode\n")
    def camera_init(): return 0
    def camera_capture(f): return 0
    def camera_capture_latest(f): return 0
    def camera_capture_first(f): return 0
    def camera_release(): pass
    def sidebar_config(k, v): pass
    def sidebar_setregion(r): pass
    def sidebar_setinteractive(v): pass
    def sidebar_display(m): pass
    def sidebar_ask(p): return ""
    def sidebar_getinput(): return ""
    def screen_setmode(m): pass
    def screen_update(): pass

camera = _types.SimpleNamespace(
    init=camera_init, capture=camera_capture,
    capture_latest=camera_capture_latest, capture_first=camera_capture_first,
    release=camera_release,
)
sidebar = _types.SimpleNamespace(
    config=sidebar_config, setregion=sidebar_setregion,
    setinteractive=sidebar_setinteractive, display=sidebar_display,
    ask=sidebar_ask, getinput=sidebar_getinput,
)
screen = _types.SimpleNamespace(setmode=screen_setmode, update=screen_update)
