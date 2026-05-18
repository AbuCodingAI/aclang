# AC ilib: math — Mathematics library (Python backend)
import math as _math

PI  = _math.pi
E   = _math.e
TAU = _math.tau
PHI = (1 + _math.sqrt(5)) / 2

def math_sqrt(x):    return _math.sqrt(x)
def math_pow(b, e):  return b ** e
def math_abs(x):     return abs(x)
def math_floor(x):   return _math.floor(x)
def math_ceil(x):    return _math.ceil(x)
def math_round(x):   return round(x)
def math_sin(x):     return _math.sin(x)
def math_cos(x):     return _math.cos(x)
def math_tan(x):     return _math.tan(x)
def math_log(x):     return _math.log(x)
def math_log2(x):    return _math.log2(x)
def math_log10(x):   return _math.log10(x)
def math_gcd(a, b):  return _math.gcd(int(a), int(b))
def math_lcm(a, b):  return _math.lcm(int(a), int(b))
def math_is_prime(n):
    n = int(n)
    if n < 2: return False
    if n < 4: return True
    if n % 2 == 0 or n % 3 == 0: return False
    i = 5
    while i * i <= n:
        if n % i == 0 or n % (i + 2) == 0: return False
        i += 6
    return True
def math_hypot(a, b): return _math.hypot(a, b)
def math_deg2rad(d): return _math.radians(d)
def math_rad2deg(r): return _math.degrees(r)
