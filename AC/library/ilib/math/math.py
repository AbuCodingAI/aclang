# AC ilib: math — Mathematics library (Python backend)
import math as _math

class LongInt:
    MIN = -(1 << 95)
    MAX = (1 << 95) - 1

    def __init__(self, value=0):
        ivalue = int(value)
        if ivalue < self.MIN or ivalue > self.MAX:
            raise OverflowError("math.LongInt supports -(2^95) through (2^95)-1")
        self.value = ivalue

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return str(self.value)

class GoodDec:
    # value = unscaled * (10 ** scale)
    # Example: (123, -2) => 1.23
    def __init__(self, unscaled=0, scale=0):
        self.unscaled = int(unscaled)
        self.scale = int(scale)

    def __str__(self):
        u = str(abs(self.unscaled))
        neg = self.unscaled < 0
        if self.scale == 0:
            body = u
        elif self.scale > 0:
            body = u + ("0" * self.scale)
        else:
            places = -self.scale
            if len(u) > places:
                cut = len(u) - places
                body = u[:cut] + "." + u[cut:]
            else:
                body = "0." + ("0" * (places - len(u))) + u
        return ("-" + body) if (neg and body != "0") else body

    def __repr__(self):
        return str(self)

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
