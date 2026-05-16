import ctypes as _ct, os as _os, platform as _pl
_acmath_lib = _os.path.join(_os.getcwd(), 'library', 'math', 'acmath.dll' if _pl.system() == 'Windows' else 'libacmath.so')
_m = _ct.CDLL(_os.path.normpath(_acmath_lib))
_D = _ct.c_double; _I = _ct.c_int64; _INT = _ct.c_int; _CS = _ct.c_char_p
def _d1(n):  f=getattr(_m,n); f.argtypes=[_D];       f.restype=_D;  return f
def _d2(n):  f=getattr(_m,n); f.argtypes=[_D,_D];    f.restype=_D;  return f
def _d3(n):  f=getattr(_m,n); f.argtypes=[_D,_D,_D]; f.restype=_D;  return f
def _i1(n):  f=getattr(_m,n); f.argtypes=[_I];       f.restype=_I;  return f
def _i2(n):  f=getattr(_m,n); f.argtypes=[_I,_I];    f.restype=_I;  return f
def _di(n):  f=getattr(_m,n); f.argtypes=[_D];       f.restype=_I;  return f
def _id(n):  f=getattr(_m,n); f.argtypes=[_I];       f.restype=_D;  return f
def _ii2(n): f=getattr(_m,n); f.argtypes=[_I,_I];    f.restype=_I;  return f
def _c0(n):  f=getattr(_m,n); f.argtypes=[];          f.restype=_D;  return f
# Trig
math_sin=_d1('ac_sin'); math_cos=_d1('ac_cos'); math_tan=_d1('ac_tan')
math_csc=_d1('ac_csc'); math_sec=_d1('ac_sec'); math_cot=_d1('ac_cot')
math_asin=_d1('ac_asin'); math_acos=_d1('ac_acos'); math_atan=_d1('ac_atan')
math_acsc=_d1('ac_acsc'); math_asec=_d1('ac_asec'); math_acot=_d1('ac_acot')
math_deg2rad=_d1('ac_deg2rad'); math_rad2deg=_d1('ac_rad2deg')
# Arithmetic
math_sqrt=_d1('ac_sqrt'); math_cbrt=_d1('ac_cbrt'); math_abs=_d1('ac_abs')
math_floor=_d1('ac_floor'); math_ceil=_d1('ac_ceil'); math_round=_d1('ac_round')
math_ln=_d1('ac_ln'); math_log=_d2('ac_log_base')
math_log2=_d1('ac_log2'); math_log10=_d1('ac_log10')
math_atan2=_d2('ac_atan2')
math_pow=_d2('ac_pow'); math_hypot=_d2('ac_hypot'); math_mod=_d2('ac_mod')
math_clamp=_d3('ac_clamp')
math_to_int=_di('ac_to_int'); math_to_dec=_id('ac_to_dec')
math_abs_int=_i1('ac_abs_int'); math_mod_int=_ii2('ac_mod_int')
math_gcd=_ii2('ac_gcd'); math_lcm=_ii2('ac_lcm')
_m.ac_is_prime.argtypes=[_I]; _m.ac_is_prime.restype=_INT
math_is_prime=lambda n: int(_m.ac_is_prime(int(n)))
# eval — delegate to C library evaluator (or Python's native eval)
_m.ac_eval.argtypes=[_CS]; _m.ac_eval.restype=_D
def math_eval(expr): return _m.ac_eval(str(expr).encode())
# Constants
math_pi=_c0('ac_math_pi_const')(); math_e=_c0('ac_math_e_const')()
math_tau=_c0('ac_math_tau_const')(); math_em=_c0('ac_math_em_const')()
math_inf=_c0('ac_math_inf')()
_m.ac_math_pi.argtypes=[_INT]; _m.ac_math_pi.restype=_D
_m.ac_math_e.argtypes=[_INT]; _m.ac_math_e.restype=_D
math_pi_digits=lambda n: _m.ac_math_pi(int(n)); math_e_digits=lambda n: _m.ac_math_e(int(n))
# Complex — Python has native complex, no FFI needed
math_i = 1j
def math_re(z): return float(z.real) if hasattr(z,'real') else float(z)
def math_im(z): return float(z.imag) if hasattr(z,'imag') else 0.0
# Aggregates
def math_sigma(lst):
    a=(_D*len(lst))(*lst); return _m.ac_sigma(a,len(lst))
_m.ac_sigma.argtypes=[_ct.POINTER(_D),_INT]; _m.ac_sigma.restype=_D
def math_PI(lst):
    a=(_D*len(lst))(*lst); return _m.ac_PI_product(a,len(lst))
_m.ac_PI_product.argtypes=[_ct.POINTER(_D),_INT]; _m.ac_PI_product.restype=_D
def math_gradient(lst):
    n=len(lst); a=(_D*n)(*lst); o=(_D*(n-1))()
    _m.ac_gradient(a,n,o); return list(o)
_m.ac_gradient.argtypes=[_ct.POINTER(_D),_INT,_ct.POINTER(_D)]; _m.ac_gradient.restype=None
# Statistics
def _stat(fn,lst):
    a=(_D*len(lst))(*lst); return getattr(_m,fn)(a,len(lst))
for _fn in ['ac_stat_avg','ac_stat_median','ac_stat_q1','ac_stat_q3','ac_stat_mode','ac_stat_min','ac_stat_max']:
    getattr(_m,_fn).argtypes=[_ct.POINTER(_D),_INT]; getattr(_m,_fn).restype=_D
stat_avg    = lambda l: _stat('ac_stat_avg',l)
stat_median = lambda l: _stat('ac_stat_median',l)
stat_q1     = lambda l: _stat('ac_stat_q1',l)
stat_q3     = lambda l: _stat('ac_stat_q3',l)
stat_mode   = lambda l: _stat('ac_stat_mode',l)
stat_min    = lambda l: _stat('ac_stat_min',l)
stat_max    = lambda l: _stat('ac_stat_max',l)
def stat_boxnum(lst):
    a=(_D*len(lst))(*lst); o=(_D*5)()
    _m.ac_stat_boxnum(a,len(lst),o); return list(o)
_m.ac_stat_boxnum.argtypes=[_ct.POINTER(_D),_INT,_ct.POINTER(_D)]; _m.ac_stat_boxnum.restype=None
# Short-name aliases for eval() expressions
sin=math_sin; cos=math_cos; tan=math_tan
asin=math_asin; acos=math_acos; atan=math_atan; atan2=math_atan2
sqrt=math_sqrt; cbrt=math_cbrt; floor=math_floor; ceil=math_ceil
ln=math_ln; log=math_log; log2=math_log2; log10=math_log10
hypot=math_hypot; clamp=math_clamp
pi=math_pi; e=math_e; tau=math_tau; em=math_em
i=math_i; re=math_re; im=math_im

# Dot-call namespace — allows math.sin(x), math.pi, math.PI etc. in AC->PY output
import types as _types
math = _types.SimpleNamespace(
    sin=math_sin, cos=math_cos, tan=math_tan,
    csc=math_csc, sec=math_sec, cot=math_cot,
    asin=math_asin, acos=math_acos, atan=math_atan, atan2=math_atan2,
    acsc=math_acsc, asec=math_asec, acot=math_acot,
    deg2rad=math_deg2rad, rad2deg=math_rad2deg,
    sqrt=math_sqrt, cbrt=math_cbrt, abs=math_abs,
    floor=math_floor, ceil=math_ceil, round=math_round,
    ln=math_ln, log=math_log, log2=math_log2, log10=math_log10,
    pow=math_pow, hypot=math_hypot, mod=math_mod, clamp=math_clamp,
    to_int=math_to_int, to_dec=math_to_dec,
    abs_int=math_abs_int, mod_int=math_mod_int,
    gcd=math_gcd, lcm=math_lcm,
    is_prime=math_is_prime, eval=math_eval,
    pi=math_pi, e=math_e, tau=math_tau, em=math_em, inf=math_inf,
    PI=math_PI, sigma=math_sigma, gradient=math_gradient,
    i=math_i, re=math_re, im=math_im,
)
