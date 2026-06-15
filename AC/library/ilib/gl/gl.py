# AC ilib: gl — Python ctypes FFI (libacgl.so / acgl.dll)
# Wraps SDL2-based libacgl.so. No tkinter.
import ctypes, os

def _load_lib():
    base = os.path.join(os.path.dirname(__file__))
    for name in ('libacgl.so', 'acgl.dll', 'libacgl.dylib'):
        p = os.path.join(base, name)
        if os.path.exists(p):
            return ctypes.CDLL(os.path.abspath(p))
    return None

_lib = _load_lib()

if _lib:
    u8  = ctypes.c_uint8
    i32 = ctypes.c_int
    f32 = ctypes.c_float
    cs  = ctypes.c_char_p

    _lib.ac_gl_init.restype                   = i32
    _lib.ac_gl_init.argtypes                  = []
    _lib.ac_gl_quit.restype                   = None
    _lib.ac_gl_quit.argtypes                  = []

    _lib.ac_gl_screen_create.restype          = i32
    _lib.ac_gl_screen_create.argtypes         = [i32, i32, cs]
    _lib.ac_gl_screen_set_bg.restype          = None
    _lib.ac_gl_screen_set_bg.argtypes         = [u8, u8, u8]
    _lib.ac_gl_screen_set_fps.restype         = None
    _lib.ac_gl_screen_set_fps.argtypes        = [i32]
    _lib.ac_gl_screen_w.restype               = i32
    _lib.ac_gl_screen_w.argtypes              = []
    _lib.ac_gl_screen_h.restype               = i32
    _lib.ac_gl_screen_h.argtypes              = []

    _lib.ac_gl_obj_create.restype             = None
    _lib.ac_gl_obj_create.argtypes            = [cs]
    _lib.ac_gl_obj_geometry.restype           = None
    _lib.ac_gl_obj_geometry.argtypes          = [cs, i32, i32]
    _lib.ac_gl_obj_square.restype             = None
    _lib.ac_gl_obj_square.argtypes            = [cs, i32]
    _lib.ac_gl_obj_pos.restype                = None
    _lib.ac_gl_obj_pos.argtypes               = [cs, i32, i32]
    _lib.ac_gl_obj_color.restype              = None
    _lib.ac_gl_obj_color.argtypes             = [cs, u8, u8, u8]
    _lib.ac_gl_obj_velocity.restype           = None
    _lib.ac_gl_obj_velocity.argtypes          = [cs, f32, f32]
    _lib.ac_gl_obj_set_speed.restype          = None
    _lib.ac_gl_obj_set_speed.argtypes         = [cs, f32]
    _lib.ac_gl_obj_set_direction.restype      = None
    _lib.ac_gl_obj_set_direction.argtypes     = [cs, f32]
    _lib.ac_gl_obj_speed_mult.restype         = None
    _lib.ac_gl_obj_speed_mult.argtypes        = [cs, f32]
    _lib.ac_gl_obj_move_x.restype             = None
    _lib.ac_gl_obj_move_x.argtypes            = [cs, i32]
    _lib.ac_gl_obj_move_y.restype             = None
    _lib.ac_gl_obj_move_y.argtypes            = [cs, i32]
    _lib.ac_gl_obj_x.restype                  = i32
    _lib.ac_gl_obj_x.argtypes                 = [cs]
    _lib.ac_gl_obj_y.restype                  = i32
    _lib.ac_gl_obj_y.argtypes                 = [cs]
    _lib.ac_gl_obj_w.restype                  = i32
    _lib.ac_gl_obj_w.argtypes                 = [cs]
    _lib.ac_gl_obj_h.restype                  = i32
    _lib.ac_gl_obj_h.argtypes                 = [cs]

    _lib.ac_gl_obj_curveshape.restype         = None
    _lib.ac_gl_obj_curveshape.argtypes        = [cs, cs]
    _lib.ac_gl_obj_vertex.restype             = None
    _lib.ac_gl_obj_vertex.argtypes            = [cs, f32, f32]
    _lib.ac_gl_obj_circle_fall.restype        = None
    _lib.ac_gl_obj_circle_fall.argtypes       = [cs, f32, cs]
    _lib.ac_gl_obj_circle_fell.restype        = i32
    _lib.ac_gl_obj_circle_fell.argtypes       = [cs]
    _lib.ac_gl_obj_set_spawn.restype          = None
    _lib.ac_gl_obj_set_spawn.argtypes         = [cs]
    _lib.ac_gl_obj_regen.restype              = None
    _lib.ac_gl_obj_regen.argtypes             = [cs]
    _lib.ac_gl_obj_animate.restype            = None
    _lib.ac_gl_obj_animate.argtypes           = [cs, cs, f32]
    _lib.ac_gl_hitbox_many_overlap.restype    = i32
    _lib.ac_gl_hitbox_many_overlap.argtypes   = []
    _lib.ac_gl_draw_create.restype            = None
    _lib.ac_gl_draw_create.argtypes           = [cs]
    _lib.ac_gl_draw_curveshape.restype        = None
    _lib.ac_gl_draw_curveshape.argtypes       = [cs, cs]
    _lib.ac_gl_draw_vertex.restype            = None
    _lib.ac_gl_draw_vertex.argtypes           = [cs, f32, f32]
    _lib.ac_gl_draw_line.restype              = None
    _lib.ac_gl_draw_line.argtypes             = [cs, f32, f32, f32, f32, u8, u8, u8]
    _lib.ac_gl_draw_circle.restype            = None
    _lib.ac_gl_draw_circle.argtypes           = [cs, f32, f32, f32, u8, u8, u8]
    _lib.ac_gl_draw_clear.restype             = None
    _lib.ac_gl_draw_clear.argtypes            = [cs]
    _lib.ac_gl_obj_to_draw.restype            = None
    _lib.ac_gl_obj_to_draw.argtypes           = [cs]
    _lib.ac_gl_draw_to_obj.restype            = None
    _lib.ac_gl_draw_to_obj.argtypes           = [cs]
    _lib.ac_gl_is_draw.restype                = i32
    _lib.ac_gl_is_draw.argtypes               = [cs]
    _lib.ac_gl_is_obj.restype                 = i32
    _lib.ac_gl_is_obj.argtypes                = [cs]

    _lib.ac_gl_hitbox_overlap.restype         = i32
    _lib.ac_gl_hitbox_overlap.argtypes        = [cs, cs]
    _lib.ac_gl_hitbox_overlap_boundary.restype= i32
    _lib.ac_gl_hitbox_overlap_boundary.argtypes=[cs]
    _lib.ac_gl_hitbox_overlap_pattern.restype = i32
    _lib.ac_gl_hitbox_overlap_pattern.argtypes= [cs, cs]

    _lib.ac_gl_key_pressed.restype            = i32
    _lib.ac_gl_key_pressed.argtypes           = [cs]
    _lib.ac_gl_key_just_pressed.restype       = i32
    _lib.ac_gl_key_just_pressed.argtypes      = [cs]

    _lib.ac_gl_frame_begin.restype            = i32
    _lib.ac_gl_frame_begin.argtypes           = []
    _lib.ac_gl_frame_update.restype           = None
    _lib.ac_gl_frame_update.argtypes          = [f32]
    _lib.ac_gl_frame_render.restype           = None
    _lib.ac_gl_frame_render.argtypes          = []
    _lib.ac_gl_frame_end.restype              = None
    _lib.ac_gl_frame_end.argtypes             = []
    _lib.ac_gl_delta_time.restype             = f32
    _lib.ac_gl_delta_time.argtypes            = []

    def _b(s): return s.encode() if isinstance(s, str) else s

    def gl_init():                              return bool(_lib.ac_gl_init())
    def gl_quit():                              _lib.ac_gl_quit()
    def gl_screen_create(w, h, title="AC"):     return bool(_lib.ac_gl_screen_create(w, h, _b(title)))
    def gl_screen_set_bg(r, g, b):              _lib.ac_gl_screen_set_bg(r, g, b)
    def gl_screen_set_fps(fps):                 _lib.ac_gl_screen_set_fps(fps)
    def gl_screen_w():                          return _lib.ac_gl_screen_w()
    def gl_screen_h():                          return _lib.ac_gl_screen_h()
    def gl_obj_create(name):                    _lib.ac_gl_obj_create(_b(name))
    def gl_obj_geometry(name, w, h):            _lib.ac_gl_obj_geometry(_b(name), w, h)
    def gl_obj_square(name, size):              _lib.ac_gl_obj_square(_b(name), size)
    def gl_obj_pos(name, x, y):                 _lib.ac_gl_obj_pos(_b(name), x, y)
    def gl_obj_color(name, r, g, b):            _lib.ac_gl_obj_color(_b(name), r, g, b)
    def gl_obj_velocity(name, vx, vy):          _lib.ac_gl_obj_velocity(_b(name), vx, vy)
    def gl_obj_set_speed(name, s):              _lib.ac_gl_obj_set_speed(_b(name), s)
    def gl_obj_set_direction(name, deg):        _lib.ac_gl_obj_set_direction(_b(name), deg)
    def gl_obj_speed_mult(name, mult):          _lib.ac_gl_obj_speed_mult(_b(name), mult)
    def gl_obj_move_x(name, dx):               _lib.ac_gl_obj_move_x(_b(name), dx)
    def gl_obj_move_y(name, dy):               _lib.ac_gl_obj_move_y(_b(name), dy)
    def gl_obj_x(name):                         return _lib.ac_gl_obj_x(_b(name))
    def gl_obj_y(name):                         return _lib.ac_gl_obj_y(_b(name))
    def gl_obj_w(name):                         return _lib.ac_gl_obj_w(_b(name))
    def gl_obj_h(name):                         return _lib.ac_gl_obj_h(_b(name))
    def gl_obj_curveshape(name, expr):          _lib.ac_gl_obj_curveshape(_b(name), _b(expr))
    def gl_obj_vertex(name, vx, vy):            _lib.ac_gl_obj_vertex(_b(name), vx, vy)
    def gl_obj_circle_fall(name, frac, dir):    _lib.ac_gl_obj_circle_fall(_b(name), frac, _b(dir))
    def gl_obj_circle_fell(name):               return bool(_lib.ac_gl_obj_circle_fell(_b(name)))
    def gl_obj_set_spawn(name):                 _lib.ac_gl_obj_set_spawn(_b(name))
    def gl_obj_regen(name):                     _lib.ac_gl_obj_regen(_b(name))
    def gl_obj_animate(name, dir, speed):       _lib.ac_gl_obj_animate(_b(name), _b(dir), speed)
    def gl_hitbox_many_overlap():               return bool(_lib.ac_gl_hitbox_many_overlap())
    def gl_draw_create(name):                   _lib.ac_gl_draw_create(_b(name))
    def gl_draw_curveshape(name, expr):         _lib.ac_gl_draw_curveshape(_b(name), _b(expr))
    def gl_draw_vertex(name, vx, vy):           _lib.ac_gl_draw_vertex(_b(name), vx, vy)
    def gl_draw_line(name,x1,y1,x2,y2,r,g,b): _lib.ac_gl_draw_line(_b(name),x1,y1,x2,y2,r,g,b)
    def gl_draw_circle(name,cx,cy,rad,r,g,b):  _lib.ac_gl_draw_circle(_b(name),cx,cy,rad,r,g,b)
    def gl_draw_clear(name):                    _lib.ac_gl_draw_clear(_b(name))
    def gl_obj_to_draw(name):                   _lib.ac_gl_obj_to_draw(_b(name))
    def gl_draw_to_obj(name):                   _lib.ac_gl_draw_to_obj(_b(name))
    def gl_is_draw(name):                       return bool(_lib.ac_gl_is_draw(_b(name)))
    def gl_is_obj(name):                        return bool(_lib.ac_gl_is_obj(_b(name)))

    def gl_hitbox_many_overlap():               return bool(_lib.ac_gl_hitbox_many_overlap())
    def gl_hitbox_overlap(a, b):               return bool(_lib.ac_gl_hitbox_overlap(_b(a), _b(b)))
    def gl_hitbox_boundary(name):              return bool(_lib.ac_gl_hitbox_overlap_boundary(_b(name)))
    def gl_hitbox_overlap_pattern(name, pat):  return bool(_lib.ac_gl_hitbox_overlap_pattern(_b(name), _b(pat)))
    def gl_key_pressed(k):                     return bool(_lib.ac_gl_key_pressed(_b(k)))
    def gl_key_just_pressed(k):               return bool(_lib.ac_gl_key_just_pressed(_b(k)))
    def gl_frame_begin():                      return bool(_lib.ac_gl_frame_begin())
    def gl_frame_update(dt):                   _lib.ac_gl_frame_update(dt)
    def gl_frame_render():                     _lib.ac_gl_frame_render()
    def gl_frame_end():                        _lib.ac_gl_frame_end()
    def gl_delta_time():                       return _lib.ac_gl_delta_time()

else:
    import sys
    print("[gl] WARNING: libacgl.so not found — stub mode (no rendering)", file=sys.stderr)

    def gl_init():                              return False
    def gl_quit():                              pass
    def gl_screen_create(w, h, title="AC"):     print(f"[gl] screen {w}x{h} '{title}'"); return False
    def gl_screen_set_bg(r, g, b):              pass
    def gl_screen_set_fps(fps):                 pass
    def gl_screen_w():                          return 0
    def gl_screen_h():                          return 0
    def gl_obj_create(name):                    pass
    def gl_obj_geometry(name, w, h):            pass
    def gl_obj_square(name, size):              pass
    def gl_obj_pos(name, x, y):                 pass
    def gl_obj_color(name, r, g, b):            pass
    def gl_obj_velocity(name, vx, vy):          pass
    def gl_obj_set_speed(name, s):              pass
    def gl_obj_set_direction(name, deg):        pass
    def gl_obj_speed_mult(name, mult):          pass
    def gl_obj_move_x(name, dx):               pass
    def gl_obj_move_y(name, dy):               pass
    def gl_obj_x(name):                         return 0
    def gl_obj_y(name):                         return 0
    def gl_obj_w(name):                         return 0
    def gl_obj_h(name):                         return 0
    def gl_obj_curveshape(name, expr):          pass
    def gl_obj_vertex(name, vx, vy):            pass
    def gl_obj_circle_fall(name, frac, dir):    pass
    def gl_obj_circle_fell(name):               return False
    def gl_obj_set_spawn(name):                 pass
    def gl_obj_regen(name):                     pass
    def gl_obj_animate(name, dir, speed):       pass
    def gl_hitbox_many_overlap():               return False
    def gl_draw_create(name):                   pass
    def gl_draw_curveshape(name, expr):         pass
    def gl_draw_vertex(name, vx, vy):           pass
    def gl_draw_line(name,x1,y1,x2,y2,r,g,b): pass
    def gl_draw_circle(name,cx,cy,rad,r,g,b):  pass
    def gl_draw_clear(name):                    pass
    def gl_obj_to_draw(name):                   pass
    def gl_draw_to_obj(name):                   pass
    def gl_is_draw(name):                       return False
    def gl_is_obj(name):                        return False

    def gl_hitbox_overlap(a, b):               return False
    def gl_hitbox_boundary(name):              return False
    def gl_hitbox_overlap_pattern(name, pat):  return False
    def gl_key_pressed(k):                     return False
    def gl_key_just_pressed(k):               return False
    def gl_frame_begin():                      return False
    def gl_frame_update(dt):                   pass
    def gl_frame_render():                     pass
    def gl_frame_end():                        pass
    def gl_delta_time():                       return 0.016

# ── Namespace object — AC-generated Python uses gl.screen.create(...) etc. ──

class _AcGlScreen:
    def create(self, w, h, title="AC"): return gl_screen_create(w, h, title)
    def set_bg(self, r, g, b):          gl_screen_set_bg(r, g, b)
    def set_fps(self, fps):             gl_screen_set_fps(fps)
    def w(self):                        return gl_screen_w()
    def h(self):                        return gl_screen_h()

class _AcGlObj:
    def create(self, name):              gl_obj_create(name)
    def geometry(self, name, w, h):      gl_obj_geometry(name, w, h)
    def square(self, name, size):        gl_obj_square(name, size)
    def pos(self, name, x, y):           gl_obj_pos(name, x, y)
    def color(self, name, r, g, b):      gl_obj_color(name, r, g, b)
    def velocity(self, name, vx, vy):    gl_obj_velocity(name, vx, vy)
    def set_speed(self, name, s):        gl_obj_set_speed(name, s)
    def set_direction(self, name, deg):  gl_obj_set_direction(name, deg)
    def speed_mult(self, name, m):       gl_obj_speed_mult(name, m)
    def move_x(self, name, dx):         gl_obj_move_x(name, dx)
    def move_y(self, name, dy):         gl_obj_move_y(name, dy)
    def x(self, name):                   return gl_obj_x(name)
    def y(self, name):                   return gl_obj_y(name)
    def w(self, name):                   return gl_obj_w(name)
    def h(self, name):                   return gl_obj_h(name)
    def curveshape(self, name, expr):          gl_obj_curveshape(name, expr)
    def vertex(self, name, vx, vy):            gl_obj_vertex(name, vx, vy)
    def to_draw(self, name):                   gl_obj_to_draw(name)
    def circle_fall(self, name, frac, dir):    gl_obj_circle_fall(name, frac, dir)
    def circle_fell(self, name):               return gl_obj_circle_fell(name)
    def set_spawn(self, name):                 gl_obj_set_spawn(name)
    def regen(self, name):                     gl_obj_regen(name)
    def animate(self, name, dir, speed):       gl_obj_animate(name, dir, speed)

class _AcGlDraw:
    def create(self, name):               gl_draw_create(name)
    def curveshape(self, name, expr):     gl_draw_curveshape(name, expr)
    def vertex(self, name, vx, vy):       gl_draw_vertex(name, vx, vy)
    def line(self, name,x1,y1,x2,y2,r,g,b): gl_draw_line(name,x1,y1,x2,y2,r,g,b)
    def circle(self, name,cx,cy,rad,r,g,b):  gl_draw_circle(name,cx,cy,rad,r,g,b)
    def clear(self, name):                gl_draw_clear(name)
    def to_obj(self, name):               gl_draw_to_obj(name)

class _AcGlHitbox:
    def overlap(self, a, b):             return gl_hitbox_overlap(a, b)
    def boundary(self, name):            return gl_hitbox_boundary(name)
    def overlap_pattern(self, n, pat):   return gl_hitbox_overlap_pattern(n, pat)
    def many_overlap(self):              return gl_hitbox_many_overlap()

class _AcGlKey:
    def pressed(self, k):               return gl_key_pressed(k)
    def just_pressed(self, k):          return gl_key_just_pressed(k)

class _AcGlFrame:
    def begin(self):                    return gl_frame_begin()
    def update(self, dt):               gl_frame_update(dt)
    def render(self):                   gl_frame_render()
    def end(self):                      gl_frame_end()
    def delta(self):                    return gl_delta_time()

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
    def is_draw(self, name):     return gl_is_draw(name)
    def is_obj(self, name):      return gl_is_obj(name)

gl = _AcGlNS()
