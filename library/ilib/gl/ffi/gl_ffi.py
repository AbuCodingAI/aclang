# AC ilib: gl — Python ctypes FFI (libacgl.so / acgl.dll)
# Inlined by AC->PY compiler when "use ilib gl" is declared.
import ctypes as _ct, os as _os, platform as _pl

def _gl_load():
    _base = globals().get('_ac_gl_lib_dir', _os.path.join(_os.getcwd(), 'library', 'ilib', 'gl'))
    _name = 'acgl.dll' if _pl.system() == 'Windows' else ('libacgl.dylib' if _pl.system() == 'Darwin' else 'libacgl.so')
    _p = _os.path.join(_base, _name)
    if _os.path.exists(_p): return _ct.CDLL(_os.path.abspath(_p))
    return None

_gl_lib = _gl_load()

if _gl_lib:
    _u8 = _ct.c_uint8; _i32 = _ct.c_int; _f32 = _ct.c_float; _cs = _ct.c_char_p
    _gl_lib.ac_gl_init.restype                    = _i32;  _gl_lib.ac_gl_init.argtypes                    = []
    _gl_lib.ac_gl_quit.restype                    = None;  _gl_lib.ac_gl_quit.argtypes                    = []
    _gl_lib.ac_gl_screen_create.restype           = _i32;  _gl_lib.ac_gl_screen_create.argtypes           = [_i32, _i32, _cs]
    _gl_lib.ac_gl_screen_set_bg.restype           = None;  _gl_lib.ac_gl_screen_set_bg.argtypes           = [_u8, _u8, _u8]
    _gl_lib.ac_gl_screen_set_fps.restype          = None;  _gl_lib.ac_gl_screen_set_fps.argtypes          = [_i32]
    _gl_lib.ac_gl_screen_w.restype                = _i32;  _gl_lib.ac_gl_screen_w.argtypes                = []
    _gl_lib.ac_gl_screen_h.restype                = _i32;  _gl_lib.ac_gl_screen_h.argtypes                = []
    _gl_lib.ac_gl_obj_create.restype              = None;  _gl_lib.ac_gl_obj_create.argtypes              = [_cs]
    _gl_lib.ac_gl_obj_geometry.restype            = None;  _gl_lib.ac_gl_obj_geometry.argtypes            = [_cs, _i32, _i32]
    _gl_lib.ac_gl_obj_square.restype              = None;  _gl_lib.ac_gl_obj_square.argtypes              = [_cs, _i32]
    _gl_lib.ac_gl_obj_pos.restype                 = None;  _gl_lib.ac_gl_obj_pos.argtypes                 = [_cs, _i32, _i32]
    _gl_lib.ac_gl_obj_color.restype               = None;  _gl_lib.ac_gl_obj_color.argtypes               = [_cs, _u8, _u8, _u8]
    _gl_lib.ac_gl_obj_velocity.restype            = None;  _gl_lib.ac_gl_obj_velocity.argtypes            = [_cs, _f32, _f32]
    _gl_lib.ac_gl_obj_set_speed.restype           = None;  _gl_lib.ac_gl_obj_set_speed.argtypes           = [_cs, _f32]
    _gl_lib.ac_gl_obj_set_direction.restype       = None;  _gl_lib.ac_gl_obj_set_direction.argtypes       = [_cs, _f32]
    _gl_lib.ac_gl_obj_speed_mult.restype          = None;  _gl_lib.ac_gl_obj_speed_mult.argtypes          = [_cs, _f32]
    _gl_lib.ac_gl_obj_move_x.restype              = None;  _gl_lib.ac_gl_obj_move_x.argtypes              = [_cs, _i32]
    _gl_lib.ac_gl_obj_move_y.restype              = None;  _gl_lib.ac_gl_obj_move_y.argtypes              = [_cs, _i32]
    _gl_lib.ac_gl_obj_x.restype                   = _i32;  _gl_lib.ac_gl_obj_x.argtypes                   = [_cs]
    _gl_lib.ac_gl_obj_y.restype                   = _i32;  _gl_lib.ac_gl_obj_y.argtypes                   = [_cs]
    _gl_lib.ac_gl_obj_w.restype                   = _i32;  _gl_lib.ac_gl_obj_w.argtypes                   = [_cs]
    _gl_lib.ac_gl_obj_h.restype                   = _i32;  _gl_lib.ac_gl_obj_h.argtypes                   = [_cs]
    _gl_lib.ac_gl_obj_curveshape.restype          = None;  _gl_lib.ac_gl_obj_curveshape.argtypes          = [_cs, _cs]
    _gl_lib.ac_gl_obj_vertex.restype              = None;  _gl_lib.ac_gl_obj_vertex.argtypes              = [_cs, _f32, _f32]
    _gl_lib.ac_gl_obj_circle_fall.restype         = None;  _gl_lib.ac_gl_obj_circle_fall.argtypes         = [_cs, _f32, _cs]
    _gl_lib.ac_gl_obj_circle_fell.restype         = _i32;  _gl_lib.ac_gl_obj_circle_fell.argtypes         = [_cs]
    _gl_lib.ac_gl_obj_set_spawn.restype           = None;  _gl_lib.ac_gl_obj_set_spawn.argtypes           = [_cs]
    _gl_lib.ac_gl_obj_regen.restype               = None;  _gl_lib.ac_gl_obj_regen.argtypes               = [_cs]
    _gl_lib.ac_gl_obj_animate.restype             = None;  _gl_lib.ac_gl_obj_animate.argtypes             = [_cs, _cs, _f32]
    _gl_lib.ac_gl_obj_to_draw.restype             = None;  _gl_lib.ac_gl_obj_to_draw.argtypes             = [_cs]
    _gl_lib.ac_gl_hitbox_many_overlap.restype     = _i32;  _gl_lib.ac_gl_hitbox_many_overlap.argtypes     = []
    _gl_lib.ac_gl_draw_create.restype             = None;  _gl_lib.ac_gl_draw_create.argtypes             = [_cs]
    _gl_lib.ac_gl_draw_curveshape.restype         = None;  _gl_lib.ac_gl_draw_curveshape.argtypes         = [_cs, _cs]
    _gl_lib.ac_gl_draw_vertex.restype             = None;  _gl_lib.ac_gl_draw_vertex.argtypes             = [_cs, _f32, _f32]
    _gl_lib.ac_gl_draw_line.restype               = None;  _gl_lib.ac_gl_draw_line.argtypes               = [_cs, _f32, _f32, _f32, _f32, _u8, _u8, _u8]
    _gl_lib.ac_gl_draw_circle.restype             = None;  _gl_lib.ac_gl_draw_circle.argtypes             = [_cs, _f32, _f32, _f32, _u8, _u8, _u8]
    _gl_lib.ac_gl_draw_clear.restype              = None;  _gl_lib.ac_gl_draw_clear.argtypes              = [_cs]
    _gl_lib.ac_gl_draw_to_obj.restype             = None;  _gl_lib.ac_gl_draw_to_obj.argtypes             = [_cs]
    _gl_lib.ac_gl_is_draw.restype                 = _i32;  _gl_lib.ac_gl_is_draw.argtypes                 = [_cs]
    _gl_lib.ac_gl_is_obj.restype                  = _i32;  _gl_lib.ac_gl_is_obj.argtypes                  = [_cs]
    _gl_lib.ac_gl_hitbox_overlap.restype          = _i32;  _gl_lib.ac_gl_hitbox_overlap.argtypes          = [_cs, _cs]
    _gl_lib.ac_gl_hitbox_overlap_boundary.restype = _i32;  _gl_lib.ac_gl_hitbox_overlap_boundary.argtypes = [_cs]
    _gl_lib.ac_gl_hitbox_overlap_pattern.restype  = _i32;  _gl_lib.ac_gl_hitbox_overlap_pattern.argtypes  = [_cs, _cs]
    _gl_lib.ac_gl_key_pressed.restype             = _i32;  _gl_lib.ac_gl_key_pressed.argtypes             = [_cs]
    _gl_lib.ac_gl_key_just_pressed.restype        = _i32;  _gl_lib.ac_gl_key_just_pressed.argtypes        = [_cs]
    _gl_lib.ac_gl_frame_begin.restype             = _i32;  _gl_lib.ac_gl_frame_begin.argtypes             = []
    _gl_lib.ac_gl_frame_update.restype            = None;  _gl_lib.ac_gl_frame_update.argtypes            = [_f32]
    _gl_lib.ac_gl_frame_render.restype            = None;  _gl_lib.ac_gl_frame_render.argtypes            = []
    _gl_lib.ac_gl_frame_end.restype               = None;  _gl_lib.ac_gl_frame_end.argtypes               = []
    _gl_lib.ac_gl_delta_time.restype              = _f32;  _gl_lib.ac_gl_delta_time.argtypes              = []

    def _b(s): return s.encode() if isinstance(s, str) else s

    def gl_init():                              return bool(_gl_lib.ac_gl_init())
    def gl_quit():                              _gl_lib.ac_gl_quit()
    def gl_screen_create(w, h, title="AC"):     return bool(_gl_lib.ac_gl_screen_create(w, h, _b(title)))
    def gl_screen_set_bg(r, g, b):              _gl_lib.ac_gl_screen_set_bg(r, g, b)
    def gl_screen_set_fps(fps):                 _gl_lib.ac_gl_screen_set_fps(fps)
    def gl_screen_w():                          return _gl_lib.ac_gl_screen_w()
    def gl_screen_h():                          return _gl_lib.ac_gl_screen_h()
    def gl_obj_create(name):                    _gl_lib.ac_gl_obj_create(_b(name))
    def gl_obj_geometry(name, w, h):            _gl_lib.ac_gl_obj_geometry(_b(name), w, h)
    def gl_obj_square(name, size):              _gl_lib.ac_gl_obj_square(_b(name), size)
    def gl_obj_pos(name, x, y):                 _gl_lib.ac_gl_obj_pos(_b(name), x, y)
    def gl_obj_color(name, r, g, b):            _gl_lib.ac_gl_obj_color(_b(name), r, g, b)
    def gl_obj_velocity(name, vx, vy):          _gl_lib.ac_gl_obj_velocity(_b(name), vx, vy)
    def gl_obj_set_speed(name, s):              _gl_lib.ac_gl_obj_set_speed(_b(name), s)
    def gl_obj_set_direction(name, deg):        _gl_lib.ac_gl_obj_set_direction(_b(name), deg)
    def gl_obj_speed_mult(name, m):             _gl_lib.ac_gl_obj_speed_mult(_b(name), m)
    def gl_obj_move_x(name, dx):               _gl_lib.ac_gl_obj_move_x(_b(name), dx)
    def gl_obj_move_y(name, dy):               _gl_lib.ac_gl_obj_move_y(_b(name), dy)
    def gl_obj_x(name):                         return _gl_lib.ac_gl_obj_x(_b(name))
    def gl_obj_y(name):                         return _gl_lib.ac_gl_obj_y(_b(name))
    def gl_obj_w(name):                         return _gl_lib.ac_gl_obj_w(_b(name))
    def gl_obj_h(name):                         return _gl_lib.ac_gl_obj_h(_b(name))
    def gl_obj_curveshape(name, expr):          _gl_lib.ac_gl_obj_curveshape(_b(name), _b(expr))
    def gl_obj_vertex(name, vx, vy):            _gl_lib.ac_gl_obj_vertex(_b(name), vx, vy)
    def gl_obj_circle_fall(name, frac, dir):    _gl_lib.ac_gl_obj_circle_fall(_b(name), frac, _b(dir))
    def gl_obj_circle_fell(name):               return bool(_gl_lib.ac_gl_obj_circle_fell(_b(name)))
    def gl_obj_set_spawn(name):                 _gl_lib.ac_gl_obj_set_spawn(_b(name))
    def gl_obj_regen(name):                     _gl_lib.ac_gl_obj_regen(_b(name))
    def gl_obj_animate(name, dir, speed):       _gl_lib.ac_gl_obj_animate(_b(name), _b(dir), speed)
    def gl_obj_to_draw(name):                   _gl_lib.ac_gl_obj_to_draw(_b(name))
    def gl_hitbox_many_overlap():               return bool(_gl_lib.ac_gl_hitbox_many_overlap())
    def gl_draw_create(name):                   _gl_lib.ac_gl_draw_create(_b(name))
    def gl_draw_curveshape(name, expr):         _gl_lib.ac_gl_draw_curveshape(_b(name), _b(expr))
    def gl_draw_vertex(name, vx, vy):           _gl_lib.ac_gl_draw_vertex(_b(name), vx, vy)
    def gl_draw_line(name,x1,y1,x2,y2,r,g,b):  _gl_lib.ac_gl_draw_line(_b(name),x1,y1,x2,y2,r,g,b)
    def gl_draw_circle(name,cx,cy,rad,r,g,b):   _gl_lib.ac_gl_draw_circle(_b(name),cx,cy,rad,r,g,b)
    def gl_draw_clear(name):                    _gl_lib.ac_gl_draw_clear(_b(name))
    def gl_draw_to_obj(name):                   _gl_lib.ac_gl_draw_to_obj(_b(name))
    def gl_is_draw(name):                       return bool(_gl_lib.ac_gl_is_draw(_b(name)))
    def gl_is_obj(name):                        return bool(_gl_lib.ac_gl_is_obj(_b(name)))
    def gl_hitbox_overlap(a, b):               return bool(_gl_lib.ac_gl_hitbox_overlap(_b(a), _b(b)))
    def gl_hitbox_boundary(name):              return bool(_gl_lib.ac_gl_hitbox_overlap_boundary(_b(name)))
    def gl_hitbox_overlap_pattern(name, pat):  return bool(_gl_lib.ac_gl_hitbox_overlap_pattern(_b(name), _b(pat)))
    def gl_key_pressed(k):                     return bool(_gl_lib.ac_gl_key_pressed(_b(k)))
    def gl_key_just_pressed(k):               return bool(_gl_lib.ac_gl_key_just_pressed(_b(k)))
    def gl_frame_begin():
        result = bool(_gl_lib.ac_gl_frame_begin())
        if result:
            dt = _gl_lib.ac_gl_delta_time()
            _gl_lib.ac_gl_frame_update(dt)
            _evts = globals().get('_ac_events', {})
            for _k, _fn in list(_evts.items()):
                if bool(_gl_lib.ac_gl_key_just_pressed(_b(_k))):
                    _fn()
        return result
    def gl_frame_update(dt):                   _gl_lib.ac_gl_frame_update(dt)
    def gl_frame_render():                     _gl_lib.ac_gl_frame_render()
    def gl_frame_end():                        _gl_lib.ac_gl_frame_end()
    def gl_delta_time():                       return _gl_lib.ac_gl_delta_time()

else:
    import sys as _sys
    _sys.stderr.write("[gl] WARNING: libacgl.so not found — stub mode\n")
    def gl_init(): return False
    def gl_quit(): pass
    def gl_screen_create(w,h,title="AC"): return False
    def gl_screen_set_bg(r,g,b): pass
    def gl_screen_set_fps(fps): pass
    def gl_screen_w(): return 0
    def gl_screen_h(): return 0
    def gl_obj_create(name): pass
    def gl_obj_geometry(name,w,h): pass
    def gl_obj_square(name,size): pass
    def gl_obj_pos(name,x,y): pass
    def gl_obj_color(name,r,g,b): pass
    def gl_obj_velocity(name,vx,vy): pass
    def gl_obj_set_speed(name,s): pass
    def gl_obj_set_direction(name,d): pass
    def gl_obj_speed_mult(name,m): pass
    def gl_obj_move_x(name,dx): pass
    def gl_obj_move_y(name,dy): pass
    def gl_obj_x(name): return 0
    def gl_obj_y(name): return 0
    def gl_obj_w(name): return 0
    def gl_obj_h(name): return 0
    def gl_obj_curveshape(name,e): pass
    def gl_obj_vertex(name,vx,vy): pass
    def gl_obj_circle_fall(name,f,d): pass
    def gl_obj_circle_fell(name): return False
    def gl_obj_set_spawn(name): pass
    def gl_obj_regen(name): pass
    def gl_obj_animate(name,d,s): pass
    def gl_obj_to_draw(name): pass
    def gl_hitbox_many_overlap(): return False
    def gl_draw_create(name): pass
    def gl_draw_curveshape(name,e): pass
    def gl_draw_vertex(name,vx,vy): pass
    def gl_draw_line(name,x1,y1,x2,y2,r,g,b): pass
    def gl_draw_circle(name,cx,cy,rad,r,g,b): pass
    def gl_draw_clear(name): pass
    def gl_draw_to_obj(name): pass
    def gl_is_draw(name): return False
    def gl_is_obj(name): return False
    def gl_hitbox_overlap(a,b): return False
    def gl_hitbox_boundary(name): return False
    def gl_hitbox_overlap_pattern(name,pat): return False
    def gl_key_pressed(k): return False
    def gl_key_just_pressed(k): return False
    def gl_frame_begin(): return False
    def gl_frame_update(dt): pass
    def gl_frame_render(): pass
    def gl_frame_end(): pass
    def gl_delta_time(): return 0.016

_COLOR_MAP = {
    "white":(255,255,255),"black":(0,0,0),"red":(255,0,0),"green":(0,255,0),
    "blue":(0,0,255),"yellow":(255,255,0),"cyan":(0,255,255),"magenta":(255,0,255),
    "gray":(128,128,128),"grey":(128,128,128),"orange":(255,165,0),"purple":(128,0,128),
    "brown":(139,69,19),"pink":(255,182,193),"lime":(0,255,127),"teal":(0,128,128),
}

_pending_locs = {}  # name -> (xv_str, yv_str) — re-applied after screen creation

def _gl_resolve_and_pos(name, xv, yv):
    def _rc(v, is_x):
        v = v.strip()
        if v == "center": return (gl_screen_w() if is_x else gl_screen_h()) // 2
        if v == "right":  return gl_screen_w() - gl_obj_w(name)
        if v == "bottom": return gl_screen_h() - gl_obj_h(name)
        if v.startswith("screen-"):
            try: return (gl_screen_w() if is_x else gl_screen_h()) - int(v[7:])
            except: pass
        try: return int(v)
        except: return 0
    gl_obj_pos(name, _rc(xv, True), _rc(yv, False))

def gl_hitbox_coords_overlap(a, b):
    na = a._name if hasattr(a, '_name') else str(a)
    nb = b._name if hasattr(b, '_name') else str(b)
    return gl_hitbox_overlap(na, nb)

class _GlObjectHitbox:
    def __init__(self, name): self._name = name
    @property
    def coords(self): return self
    def overlap(self, other):
        if other == "boundary": return gl_hitbox_boundary(self._name)
        return gl_hitbox_overlap(self._name, other)
    def overlap_pattern(self, pat): return gl_hitbox_overlap_pattern(self._name, pat)

class _GlObject:
    def __init__(self, name):
        self._name = name
        if not gl_is_obj(name):
            gl_obj_create(name)
        self.hitbox = _GlObjectHitbox(name)

    def config(self, *args):
        for arg in args:
            arg = arg.strip()
            if arg.startswith("item="):
                spec = arg[5:].strip()
                if spec.startswith("square(") and spec.endswith(")"):
                    gl_obj_square(self._name, int(spec[7:-1].strip()))
                elif spec.startswith("rect(") and spec.endswith(")"):
                    p = [s.strip() for s in spec[5:-1].split(",")]
                    gl_obj_geometry(self._name, int(p[0]), int(p[1]))
                elif spec.startswith("circle(") and spec.endswith(")"):
                    r = int(spec[7:-1].strip()); gl_obj_geometry(self._name, r*2, r*2)
                elif spec.startswith("geometry("):
                    # "geometry(100 width @ 300 height)" — extract numbers
                    import re as _re
                    nums = _re.findall(r'\d+', spec)
                    if len(nums) >= 2: gl_obj_geometry(self._name, int(nums[0]), int(nums[1]))
            elif arg.startswith("location="):
                loc = arg[9:].strip().strip("()")
                parts = {}
                for p in loc.split(","):
                    p = p.strip()
                    if "=" in p:
                        k, v = p.split("=", 1); parts[k.strip()] = v.strip()
                xv = parts.get("x","0"); yv = parts.get("y","0")
                _pending_locs[self._name] = (xv, yv)
                _gl_resolve_and_pos(self._name, xv, yv)
            elif arg.startswith("velocity="):
                spec = arg[9:].strip().strip("()")
                parts = {}
                for p in spec.split(","):
                    p = p.strip()
                    if "=" in p:
                        k, v = p.split("=", 1); parts[k.strip()] = v.strip()
                gl_obj_velocity(self._name, float(parts.get("x",0)), float(parts.get("y",0)))
            elif arg.startswith("speed="):
                gl_obj_set_speed(self._name, float(arg[6:].strip()))
            elif arg.startswith("direction="):
                gl_obj_set_direction(self._name, float(arg[10:].strip()))
            elif arg.startswith("color="):
                spec = arg[6:].strip()
                if spec.startswith("(") and spec.endswith(")"):
                    p = [s.strip() for s in spec[1:-1].split(",")]
                    gl_obj_color(self._name, int(p[0]), int(p[1]), int(p[2]))
                elif spec in _COLOR_MAP:
                    gl_obj_color(self._name, *_COLOR_MAP[spec])
            elif arg in _COLOR_MAP:
                gl_obj_color(self._name, *_COLOR_MAP[arg])

    def CircleFall(self, spec_or_frac=0.25, dir_arg="right"):
        try:
            import re as _re2
            _dir_map = {"RightDir":"right","LeftDir":"left","UpDir":"up","DownDir":"down",
                        "right":"right","left":"left","up":"up","down":"down"}
            if isinstance(spec_or_frac, str):
                spec = spec_or_frac
                m = _re2.match(r'^([0-9/.\s]+)([A-Za-z].*)$', spec)
                if m:
                    frac_str, dir_str = m.group(1).strip(), m.group(2).strip()
                else:
                    parts = spec.split(); frac_str = parts[0] if parts else "0.25"
                    dir_str = parts[1] if len(parts) > 1 else "right"
                frac = eval(frac_str) if frac_str else 0.25
            else:
                frac = float(spec_or_frac); dir_str = str(dir_arg)
            d = _dir_map.get(dir_str, dir_str)
            gl_obj_circle_fall(self._name, float(frac), d)
        except: pass
    def CircleFell(self, *args):    return gl_obj_circle_fell(self._name)
    def ANIMATE(self, *args):       pass
    def vertex(self, vx, vy):       gl_obj_vertex(self._name, float(vx), float(vy))
    def curveshape(self, expr):     gl_obj_curveshape(self._name, str(expr))
    def move_x(self, dx):          gl_obj_move_x(self._name, int(dx))
    def move_y(self, dy):          gl_obj_move_y(self._name, int(dy))
    def x(self):                   return gl_obj_x(self._name)
    def y(self):                   return gl_obj_y(self._name)
    def w(self):                   return gl_obj_w(self._name)
    def h(self):                   return gl_obj_h(self._name)
    def velocity(self, vx, vy):    gl_obj_velocity(self._name, vx, vy)
    def set_speed(self, s):        gl_obj_set_speed(self._name, s)
    def set_direction(self, d):    gl_obj_set_direction(self._name, d)
    def speed_mult(self, m):       gl_obj_speed_mult(self._name, m)
    def curveshape(self, expr):    gl_obj_curveshape(self._name, expr)
    def vertex(self, vx, vy):      gl_obj_vertex(self._name, vx, vy)
    def to_draw(self):             gl_obj_to_draw(self._name)
    def circle_fall(self, f, d):   gl_obj_circle_fall(self._name, f, d)
    def circle_fell(self):         return gl_obj_circle_fell(self._name)
    def set_spawn(self):           gl_obj_set_spawn(self._name)
    def regen(self):               gl_obj_regen(self._name)
    def animate(self, d, s):       gl_obj_animate(self._name, d, s)

class _AcGlScreen:
    def create(self,w,h,title="AC"): return gl_screen_create(w,h,title)
    def set_bg(self,r,g,b):          gl_screen_set_bg(r,g,b)
    def set_fps(self,fps):           gl_screen_set_fps(fps)
    def w(self):                     return gl_screen_w()
    def h(self):                     return gl_screen_h()
class _AcGlObj:
    def create(self,name):           gl_obj_create(name)
    def geometry(self,name,w,h):     gl_obj_geometry(name,w,h)
    def square(self,name,size):      gl_obj_square(name,size)
    def pos(self,name,x,y):          gl_obj_pos(name,x,y)
    def color(self,name,r,g,b):      gl_obj_color(name,r,g,b)
    def velocity(self,name,vx,vy):   gl_obj_velocity(name,vx,vy)
    def set_speed(self,name,s):      gl_obj_set_speed(name,s)
    def set_direction(self,name,d):  gl_obj_set_direction(name,d)
    def speed_mult(self,name,m):     gl_obj_speed_mult(name,m)
    def move_x(self,name,dx):        gl_obj_move_x(name,dx)
    def move_y(self,name,dy):        gl_obj_move_y(name,dy)
    def x(self,name):                return gl_obj_x(name)
    def y(self,name):                return gl_obj_y(name)
    def w(self,name):                return gl_obj_w(name)
    def h(self,name):                return gl_obj_h(name)
    def curveshape(self,name,expr):  gl_obj_curveshape(name,expr)
    def vertex(self,name,vx,vy):     gl_obj_vertex(name,vx,vy)
    def to_draw(self,name):          gl_obj_to_draw(name)
    def circle_fall(self,name,f,d):  gl_obj_circle_fall(name,f,d)
    def circle_fell(self,name):      return gl_obj_circle_fell(name)
    def set_spawn(self,name):        gl_obj_set_spawn(name)
    def regen(self,name):            gl_obj_regen(name)
    def animate(self,name,d,s):      gl_obj_animate(name,d,s)
class _AcGlDraw:
    def create(self,name):           gl_draw_create(name)
    def curveshape(self,name,expr):  gl_draw_curveshape(name,expr)
    def vertex(self,name,vx,vy):     gl_draw_vertex(name,vx,vy)
    def line(self,name,x1,y1,x2,y2,r,g,b): gl_draw_line(name,x1,y1,x2,y2,r,g,b)
    def circle(self,name,cx,cy,rad,r,g,b):  gl_draw_circle(name,cx,cy,rad,r,g,b)
    def clear(self,name):            gl_draw_clear(name)
    def to_obj(self,name):           gl_draw_to_obj(name)
class _AcGlHitbox:
    def overlap(self,a,b):           return gl_hitbox_overlap(a,b)
    def boundary(self,name):         return gl_hitbox_boundary(name)
    def overlap_pattern(self,n,pat): return gl_hitbox_overlap_pattern(n,pat)
    def many_overlap(self):          return gl_hitbox_many_overlap()
class _AcGlKey:
    def pressed(self,k):             return gl_key_pressed(k)
    def just_pressed(self,k):        return gl_key_just_pressed(k)
class _AcGlFrame:
    def begin(self):                 return gl_frame_begin()
    def update(self,dt):             gl_frame_update(dt)
    def render(self):                gl_frame_render()
    def end(self):                   gl_frame_end()
    def delta(self):                 return gl_delta_time()
class _AcGlNS:
    def __init__(self):
        self.screen  = _AcGlScreen()
        self.obj     = _AcGlObj()
        self.draw    = _AcGlDraw()
        self.hitbox  = _AcGlHitbox()
        self.key     = _AcGlKey()
        self.frame   = _AcGlFrame()
    def init(self):              return gl_init()
    def quit(self):              gl_quit()
    def is_draw(self,name):      return gl_is_draw(name)
    def is_obj(self,name):       return gl_is_obj(name)

gl = _AcGlNS()

def is_obj(x):
    if isinstance(x, _GlObject): return gl_is_obj(x._name)
    return gl_is_obj(str(x))
def is_draw(x):
    if isinstance(x, _GlObject): return gl_is_draw(x._name)
    return gl_is_draw(str(x))

# ── ac_gl_* aliases — used by AC-compiled code on all backends ────────────────
def ac_gl_init():                          return gl_init()
def ac_gl_quit():                          gl_quit()
def ac_gl_screen_init(w, h, title="Geodeo"): gl_init(); gl_screen_create(w, h, title)
def ac_gl_screen_animate(spec):            animate(str(spec))
def ac_gl_screen_set_bg_by_name(color):
    r, g, b = _COLOR_MAP.get(str(color).strip('"'), (0,0,0)); gl_screen_set_bg(r, g, b)
def ac_gl_obj_create(name):               gl_obj_create(name.strip('"'))
def ac_gl_obj_config_item(name, spec):
    n = name.strip('"'); s = spec.strip('"')
    if s.startswith("square(") and s.endswith(")"): gl_obj_square(n, int(s[7:-1].strip()))
    elif s.startswith("geometry("):
        import re as _re; nums = _re.findall(r'\d+', s)
        if len(nums)>=2: gl_obj_geometry(n, int(nums[0]), int(nums[1]))
    elif s.startswith("rect(") and s.endswith(")"):
        p = s[5:-1].split(","); gl_obj_geometry(n, int(p[0].strip()), int(p[1].strip()))
def ac_gl_obj_color_by_name(name, color):
    n = name.strip('"'); c = color.strip('"')
    r, g, b = _COLOR_MAP.get(c, (255,255,255)); gl_obj_color(n, r, g, b)
def ac_gl_obj_pos_from_spec(name, x_spec, y_spec):
    n = name.strip('"'); xv = x_spec.strip('"'); yv = y_spec.strip('"')
    def _rc(v, is_x):
        if v == "center": return (gl_screen_w() if is_x else gl_screen_h()) // 2
        if v == "right":  return gl_screen_w() - gl_obj_w(n)
        if v == "bottom": return gl_screen_h() - gl_obj_h(n)
        if v.startswith("screen-"):
            try: return (gl_screen_w() if is_x else gl_screen_h()) - int(v[7:])
            except: pass
        try: return int(v)
        except: return 0
    _pending_locs[n] = (xv, yv)
    gl_obj_pos(n, _rc(xv, True), _rc(yv, False))
def ac_gl_obj_move_y(name, dy):           gl_obj_move_y(name.strip('"'), int(dy))
def ac_gl_obj_move_x(name, dx):           gl_obj_move_x(name.strip('"'), int(dx))
def ac_gl_obj_vertex(name, vx, vy):       gl_obj_vertex(name.strip('"'), float(vx), float(vy))
def ac_gl_obj_curveshape(name, expr):     gl_obj_curveshape(name.strip('"'), str(expr).strip('"'))
def ac_gl_obj_circle_fall(name, frac, d): gl_obj_circle_fall(name.strip('"'), float(frac), str(d).strip('"'))
def ac_gl_obj_circle_fell(name):          return gl_obj_circle_fell(name.strip('"'))
def ac_gl_obj_regen(name):                gl_obj_regen(name.strip('"'))
def ac_gl_obj_set_spawn(name):            gl_obj_set_spawn(name.strip('"'))
def ac_gl_obj_save_spawn(name):           gl_obj_set_spawn(name.strip('"'))
def ac_gl_obj_animate(name, dir_, speed): gl_obj_animate(name.strip('"'), str(dir_).strip('"'), float(speed))
def ac_gl_is_obj(name):                   return gl_is_obj(name.strip('"') if isinstance(name,str) else str(name))
def ac_gl_is_draw(name):                  return gl_is_draw(name.strip('"') if isinstance(name,str) else str(name))
def ac_gl_hitbox_overlap(a, b):           return gl_hitbox_overlap(a.strip('"'), b.strip('"'))
def ac_gl_hitbox_many_overlap():          return gl_hitbox_many_overlap()
def ac_gl_frame_begin():                  return gl_frame_begin()
def ac_gl_frame_update(dt):               gl_frame_update(dt)
def ac_gl_frame_render():                 gl_frame_render()
def ac_gl_frame_end():                    gl_frame_end()
def ac_gl_delta_time():                   return gl_delta_time()
def ac_gl_key_pressed(k):                 return gl_key_pressed(k.strip('"'))
def ac_gl_key_just_pressed(k):            return gl_key_just_pressed(k.strip('"'))

RightDir = "right"
LeftDir  = "left"
UpDir    = "up"
DownDir  = "down"

def gl_screen_init_from_ac(dims="1720x1080", title="Geodeo"):
    gl_init()
    parts = str(dims).replace('x', ' ').split()
    w = int(''.join(c for c in parts[0] if c.isdigit())) if parts else 1720
    h = int(''.join(c for c in parts[1] if c.isdigit())) if len(parts) > 1 else 1080
    gl_screen_create(w, h, title)
    for name, (xv, yv) in list(_pending_locs.items()):
        _gl_resolve_and_pos(name, xv, yv)
        gl_obj_set_spawn(name)

def animate(spec):
    s = str(spec)
    n = ''.join(c for c in s if c.isdigit())
    if n: gl_screen_set_fps(int(n))

class _AcBackground:
    def config(self, spec):
        if not isinstance(spec, str): return
        if spec.startswith("color="):
            c = spec[6:].strip()
            r, g, b = _COLOR_MAP.get(c, (0, 0, 0))
            gl_screen_set_bg(r, g, b)
        elif spec in _COLOR_MAP:
            gl_screen_set_bg(*_COLOR_MAP[spec])

background = _AcBackground()
