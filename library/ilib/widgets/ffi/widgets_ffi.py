# AC ilib: widgets — Python ctypes FFI (libacwidgets.so / acwidgets.dll)
# GTK3-backed. Inlined by AC->PY when "use ilib widgets" is declared.
import ctypes as _ct, os as _os, platform as _pl

def _load():
    name = 'acwidgets.dll' if _pl.system() == 'Windows' else 'libacwidgets.so'
    lib_dir = globals().get('_ac_widgets_lib_dir',
        _os.path.join(_os.getcwd(), 'library', 'ilib', 'widgets'))
    p = _os.path.join(lib_dir, name)
    if _os.path.exists(p):
        return _ct.CDLL(_os.path.abspath(p))
    return None

_lib = _load()

if _lib:
    _W  = _ct.c_ssize_t    # ac_widget_t = intptr_t
    _I  = _ct.c_int
    _D  = _ct.c_double
    _U8 = _ct.c_uint8
    _CS = _ct.c_char_p
    _CB = _ct.CFUNCTYPE(None, _ct.c_void_p)

    def _b(s): return s.encode() if isinstance(s, str) else (s or b'')
    def _s(b): return (b or b'').decode() if isinstance(b, bytes) else (b or '')

    _lib.ac_widgets_init.restype            = None; _lib.ac_widgets_init.argtypes            = []
    _lib.ac_widgets_screen_new.restype      = _W;   _lib.ac_widgets_screen_new.argtypes      = [_CS, _CS]
    _lib.ac_widgets_screen_mainloop.restype = None; _lib.ac_widgets_screen_mainloop.argtypes = [_W]
    _lib.ac_widgets_screen_update.restype   = None; _lib.ac_widgets_screen_update.argtypes   = [_W]
    _lib.ac_widgets_screen_destroy.restype  = None; _lib.ac_widgets_screen_destroy.argtypes  = [_W]

    _lib.ac_widgets_display_new.restype     = _W;   _lib.ac_widgets_display_new.argtypes     = [_W, _CS]
    _lib.ac_widgets_display_pack.restype    = None; _lib.ac_widgets_display_pack.argtypes    = [_W]
    _lib.ac_widgets_display_set.restype     = None; _lib.ac_widgets_display_set.argtypes     = [_W, _CS]
    _lib.ac_widgets_display_get.restype     = _CS;  _lib.ac_widgets_display_get.argtypes     = [_W]

    _lib.ac_widgets_ask_new.restype         = _W;   _lib.ac_widgets_ask_new.argtypes         = [_W, _I]
    _lib.ac_widgets_ask_pack.restype        = None; _lib.ac_widgets_ask_pack.argtypes        = [_W]
    _lib.ac_widgets_ask_get.restype         = _CS;  _lib.ac_widgets_ask_get.argtypes         = [_W]
    _lib.ac_widgets_ask_set.restype         = None; _lib.ac_widgets_ask_set.argtypes         = [_W, _CS]

    _lib.ac_widgets_btn_new.restype         = _W;   _lib.ac_widgets_btn_new.argtypes         = [_W, _CS]
    _lib.ac_widgets_btn_pack.restype        = None; _lib.ac_widgets_btn_pack.argtypes        = [_W]
    _lib.ac_widgets_btn_on_click.restype    = None; _lib.ac_widgets_btn_on_click.argtypes    = [_W, _CB, _ct.c_void_p]

    _lib.ac_widgets_ckbtn_new.restype       = _W;   _lib.ac_widgets_ckbtn_new.argtypes       = [_W, _CS]
    _lib.ac_widgets_ckbtn_pack.restype      = None; _lib.ac_widgets_ckbtn_pack.argtypes      = [_W]
    _lib.ac_widgets_ckbtn_get.restype       = _I;   _lib.ac_widgets_ckbtn_get.argtypes       = [_W]
    _lib.ac_widgets_ckbtn_set.restype       = None; _lib.ac_widgets_ckbtn_set.argtypes       = [_W, _I]

    _lib.ac_widgets_dropdown_new.restype    = _W;   _lib.ac_widgets_dropdown_new.argtypes    = [_W]
    _lib.ac_widgets_dropdown_pack.restype   = None; _lib.ac_widgets_dropdown_pack.argtypes   = [_W]
    _lib.ac_widgets_dropdown_add.restype    = None; _lib.ac_widgets_dropdown_add.argtypes    = [_W, _CS]
    _lib.ac_widgets_dropdown_get.restype    = _CS;  _lib.ac_widgets_dropdown_get.argtypes    = [_W]
    _lib.ac_widgets_dropdown_set.restype    = None; _lib.ac_widgets_dropdown_set.argtypes    = [_W, _CS]

    _lib.ac_widgets_advance_new.restype     = _W;   _lib.ac_widgets_advance_new.argtypes     = [_W, _I]
    _lib.ac_widgets_advance_pack.restype    = None; _lib.ac_widgets_advance_pack.argtypes    = [_W]
    _lib.ac_widgets_advance_set.restype     = None; _lib.ac_widgets_advance_set.argtypes     = [_W, _D]
    _lib.ac_widgets_advance_get.restype     = _D;   _lib.ac_widgets_advance_get.argtypes     = [_W]

    _lib.ac_widgets_slider_new.restype      = _W;   _lib.ac_widgets_slider_new.argtypes      = [_W, _D, _D, _CS]
    _lib.ac_widgets_slider_pack.restype     = None; _lib.ac_widgets_slider_pack.argtypes     = [_W]
    _lib.ac_widgets_slider_get.restype      = _D;   _lib.ac_widgets_slider_get.argtypes      = [_W]
    _lib.ac_widgets_slider_set.restype      = None; _lib.ac_widgets_slider_set.argtypes      = [_W, _D]

    _lib.ac_widgets_group_new.restype       = _W;   _lib.ac_widgets_group_new.argtypes       = [_W, _CS]
    _lib.ac_widgets_group_pack.restype      = None; _lib.ac_widgets_group_pack.argtypes      = [_W]

    _lib.ac_widgets_listbox_new.restype     = _W;   _lib.ac_widgets_listbox_new.argtypes     = [_W, _I, _I]
    _lib.ac_widgets_listbox_pack.restype    = None; _lib.ac_widgets_listbox_pack.argtypes    = [_W]
    _lib.ac_widgets_listbox_add.restype     = None; _lib.ac_widgets_listbox_add.argtypes     = [_W, _CS]
    _lib.ac_widgets_listbox_item.restype    = _CS;  _lib.ac_widgets_listbox_item.argtypes    = [_W, _I]
    _lib.ac_widgets_listbox_count.restype   = _I;   _lib.ac_widgets_listbox_count.argtypes   = [_W]

    _lib.ac_widgets_sketch_new.restype      = _W;   _lib.ac_widgets_sketch_new.argtypes      = [_W, _I, _I]
    _lib.ac_widgets_sketch_pack.restype     = None; _lib.ac_widgets_sketch_pack.argtypes     = [_W]
    _lib.ac_widgets_sketch_clear.restype    = None; _lib.ac_widgets_sketch_clear.argtypes    = [_W]
    _lib.ac_widgets_sketch_line.restype     = None; _lib.ac_widgets_sketch_line.argtypes     = [_W,_D,_D,_D,_D,_U8,_U8,_U8]
    _lib.ac_widgets_sketch_rect.restype     = None; _lib.ac_widgets_sketch_rect.argtypes     = [_W,_D,_D,_D,_D,_U8,_U8,_U8]
    _lib.ac_widgets_sketch_circle.restype   = None; _lib.ac_widgets_sketch_circle.argtypes   = [_W,_D,_D,_D,_U8,_U8,_U8]
    _lib.ac_widgets_sketch_text.restype     = None; _lib.ac_widgets_sketch_text.argtypes     = [_W,_D,_D,_CS,_U8,_U8,_U8]

    _lib.ac_widgets_set_lazy.restype        = None; _lib.ac_widgets_set_lazy.argtypes        = [_W]
    _lib.ac_widgets_pack_spaced.restype     = None; _lib.ac_widgets_pack_spaced.argtypes     = [_W, _I, _I]

    _lib.ac_widgets_init()

    lazy = "lazy"  # sentinel value — pass as any arg to defer auto-pack

    def _auto_or_lazy(h, pack_fn, lz):
        if lz == "lazy": _lib.ac_widgets_set_lazy(h)
        else: pack_fn(h)

    def _strip_lazy(*args):
        """Return (is_lazy, args_without_lazy) — lazy sentinel may appear in any position."""
        is_lz = "lazy" in args
        return is_lz, tuple(a for a in args if a != "lazy")

    def _pack_or_spaced(h, pack_fn, sx, sy):
        if sx or sy: _lib.ac_widgets_pack_spaced(h, int(sx), int(sy))
        else: pack_fn(h)

    # ── Python wrapper classes ────────────────────────────────────────────────
    class Screen:
        def __init__(self, title='AC App', geometry='800x600', **_):
            self._h = _lib.ac_widgets_screen_new(_b(title), _b(geometry))
        def mainloop(self): _lib.ac_widgets_screen_mainloop(self._h)
        def update(self):   _lib.ac_widgets_screen_update(self._h)
        def destroy(self):  _lib.ac_widgets_screen_destroy(self._h)

    class display:
        def __init__(self, master, text='', _lz=None):
            self._h = _lib.ac_widgets_display_new(master._h, _b(text))
            _auto_or_lazy(self._h, _lib.ac_widgets_display_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_display_pack, space_x, space_y)
        def set(self, v):       _lib.ac_widgets_display_set(self._h, _b(v))
        def get(self):          return _s(_lib.ac_widgets_display_get(self._h))
        def config(self, **kw):
            if 'text' in kw: self.set(kw['text'])

    class ask:
        def __init__(self, master, width=20, _lz=None):
            is_lz, rest = _strip_lazy(width)
            width = rest[0] if rest else 20
            _lz = "lazy" if is_lz else _lz
            self._h = _lib.ac_widgets_ask_new(master._h, int(width) if str(width).isdigit() else 20)
            _auto_or_lazy(self._h, _lib.ac_widgets_ask_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_ask_pack, space_x, space_y)
        def get(self):       return _s(_lib.ac_widgets_ask_get(self._h))
        def set(self, v):    _lib.ac_widgets_ask_set(self._h, _b(v))

    class btn:
        _cbs = []
        def __init__(self, master, text='Button', cmd=None, _lz=None):
            self._h = _lib.ac_widgets_btn_new(master._h, _b(text))
            if cmd: self.on_click(cmd)
            _auto_or_lazy(self._h, _lib.ac_widgets_btn_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_btn_pack, space_x, space_y)
        def on_click(self, cb):
            self._cb = _CB(lambda _: cb())
            btn._cbs.append(self._cb)
            _lib.ac_widgets_btn_on_click(self._h, self._cb, None)

    class ckbtn:
        def __init__(self, master, text='', _lz=None):
            self._h = _lib.ac_widgets_ckbtn_new(master._h, _b(text))
            _auto_or_lazy(self._h, _lib.ac_widgets_ckbtn_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_ckbtn_pack, space_x, space_y)
        def get(self):       return bool(_lib.ac_widgets_ckbtn_get(self._h))
        def set(self, v):    _lib.ac_widgets_ckbtn_set(self._h, int(bool(v)))

    class radbtn:
        def __init__(self, master, text='', _lz=None):
            self._h = _lib.ac_widgets_ckbtn_new(master._h, _b(text))
            _auto_or_lazy(self._h, _lib.ac_widgets_ckbtn_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_ckbtn_pack, space_x, space_y)
        def get(self):       return bool(_lib.ac_widgets_ckbtn_get(self._h))

    class dropdown:
        def __init__(self, master, values='', _lz=None):
            is_lz, rest = _strip_lazy(values)
            values = rest[0] if rest else ''
            _lz = "lazy" if is_lz else _lz
            self._h = _lib.ac_widgets_dropdown_new(master._h)
            if isinstance(values, str) and values:
                for v in values.split(','): _lib.ac_widgets_dropdown_add(self._h, _b(v.strip()))
            _auto_or_lazy(self._h, _lib.ac_widgets_dropdown_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_dropdown_pack, space_x, space_y)
        def add(self, v):    _lib.ac_widgets_dropdown_add(self._h, _b(v))
        def get(self):       return _s(_lib.ac_widgets_dropdown_get(self._h))
        def set(self, v):    _lib.ac_widgets_dropdown_set(self._h, _b(v))

    class advance:
        def __init__(self, master, length=200, _lz=None):
            is_lz, rest = _strip_lazy(length)
            length = rest[0] if rest else 200
            _lz = "lazy" if is_lz else _lz
            self._h = _lib.ac_widgets_advance_new(master._h, int(length) if str(length).lstrip('-').isdigit() else 200)
            _auto_or_lazy(self._h, _lib.ac_widgets_advance_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_advance_pack, space_x, space_y)
        def set(self, v):    _lib.ac_widgets_advance_set(self._h, float(v))
        def get(self):       return _lib.ac_widgets_advance_get(self._h)

    class slider:
        def __init__(self, master, from_val=0, to_val=100, orient='horizontal', _lz=None):
            is_lz, rest = _strip_lazy(from_val, to_val, orient)
            from_val = rest[0] if len(rest) > 0 else 0
            to_val   = rest[1] if len(rest) > 1 else 100
            orient   = rest[2] if len(rest) > 2 else 'horizontal'
            _lz = "lazy" if is_lz else _lz
            self._h = _lib.ac_widgets_slider_new(master._h, float(from_val), float(to_val), _b(str(orient)))
            _auto_or_lazy(self._h, _lib.ac_widgets_slider_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_slider_pack, space_x, space_y)
        def get(self):       return _lib.ac_widgets_slider_get(self._h)
        def set(self, v):    _lib.ac_widgets_slider_set(self._h, float(v))

    class group:
        def __init__(self, master, text='', _lz=None):
            self._h = _lib.ac_widgets_group_new(master._h, _b(text))
            _auto_or_lazy(self._h, _lib.ac_widgets_group_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_group_pack, space_x, space_y)

    class tabs:
        def __init__(self, master, _lz=None):
            self._h = _lib.ac_widgets_group_new(master._h, b'')
            _auto_or_lazy(self._h, _lib.ac_widgets_group_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_group_pack, space_x, space_y)
        def add_tab(self, name): return self

    class scroller:
        def __init__(self, master, _lz=None):
            self._h = _lib.ac_widgets_group_new(master._h, b'')
            _auto_or_lazy(self._h, _lib.ac_widgets_group_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_group_pack, space_x, space_y)

    class table:
        def __init__(self, master, _lz=None):
            self._h = _lib.ac_widgets_listbox_new(master._h, 40, 10)
            _auto_or_lazy(self._h, _lib.ac_widgets_listbox_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_listbox_pack, space_x, space_y)
        def add(self, row):  _lib.ac_widgets_listbox_add(self._h, _b(str(row)))

    class listbox:
        def __init__(self, master, width=30, height=5, _lz=None):
            self._h = _lib.ac_widgets_listbox_new(master._h, int(width), int(height))
            _auto_or_lazy(self._h, _lib.ac_widgets_listbox_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_listbox_pack, space_x, space_y)
        def add(self, item): _lib.ac_widgets_listbox_add(self._h, _b(item))
        def get(self):
            return [_s(_lib.ac_widgets_listbox_item(self._h, i))
                    for i in range(_lib.ac_widgets_listbox_count(self._h))]

    class sketch:
        def __init__(self, master, width=400, height=300, _lz=None):
            self._h = _lib.ac_widgets_sketch_new(master._h, int(width), int(height))
            _auto_or_lazy(self._h, _lib.ac_widgets_sketch_pack, _lz)
        def pack(self, space_x=0, space_y=0): _pack_or_spaced(self._h, _lib.ac_widgets_sketch_pack, space_x, space_y)
        def clear(self):                           _lib.ac_widgets_sketch_clear(self._h)
        def line(self,x1,y1,x2,y2,r=0,g=0,b=0):  _lib.ac_widgets_sketch_line(self._h,x1,y1,x2,y2,r,g,b)
        def rect(self,x1,y1,x2,y2,r=0,g=0,b=0):  _lib.ac_widgets_sketch_rect(self._h,x1,y1,x2,y2,r,g,b)
        def circle(self,cx,cy,rad,r=0,g=0,b=0):   _lib.ac_widgets_sketch_circle(self._h,cx,cy,rad,r,g,b)
        def text(self,x,y,t,r=0,g=0,b=0):         _lib.ac_widgets_sketch_text(self._h,x,y,_b(t),r,g,b)

else:
    import sys as _sys
    _sys.stderr.write("[widgets] WARNING: libacwidgets.so not found — stub mode\n")
    class Screen:
        def __init__(self, title='AC App', geometry='800x600', **_): pass
        def mainloop(self): pass
        def update(self): pass
        def destroy(self): pass
    class display:
        def __init__(self, master, text='', **_): pass
        def pack(self, **_): pass
        def set(self, v): pass
        def get(self): return ''
        def config(self, **kw): pass
    class ask:
        def __init__(self, master, width=20, **_): pass
        def pack(self, **_): pass
        def get(self): return ''
        def set(self, v): pass
    class btn:
        def __init__(self, master, text='Button', cmd=None, **_): pass
        def pack(self, **_): pass
        def on_click(self, cb): pass
    class ckbtn:
        def __init__(self, master, text='', **_): pass
        def pack(self, **_): pass
        def get(self): return False
        def set(self, v): pass
    class radbtn:
        def __init__(self, master, text='', **_): pass
        def pack(self, **_): pass
        def get(self): return False
    class dropdown:
        def __init__(self, master, values='', **_): pass
        def pack(self, **_): pass
        def add(self, v): pass
        def get(self): return ''
        def set(self, v): pass
    class advance:
        def __init__(self, master, length=200, **_): pass
        def pack(self, **_): pass
        def set(self, v): pass
        def get(self): return 0.0
    class slider:
        def __init__(self, master, from_val=0, to_val=100, orient='horizontal', **_): pass
        def pack(self, **_): pass
        def get(self): return 0.0
        def set(self, v): pass
    class group:
        def __init__(self, master, text='', **_): pass
        def pack(self, **_): pass
    class tabs:
        def __init__(self, master, **_): pass
        def pack(self, **_): pass
        def add_tab(self, name): return self
    class scroller:
        def __init__(self, master, **_): pass
        def pack(self, **_): pass
    class table:
        def __init__(self, master, **_): pass
        def pack(self, **_): pass
        def add(self, row): pass
    class listbox:
        def __init__(self, master, width=30, height=5, **_): pass
        def pack(self, **_): pass
        def add(self, item): pass
        def get(self): return []
    class sketch:
        def __init__(self, master, width=400, height=300, **_): pass
        def pack(self, **_): pass
        def clear(self): pass
        def line(self,x1,y1,x2,y2,r=0,g=0,b=0): pass
        def rect(self,x1,y1,x2,y2,r=0,g=0,b=0): pass
        def circle(self,cx,cy,rad,r=0,g=0,b=0): pass
        def text(self,x,y,t,r=0,g=0,b=0): pass
