const _ffi=require('ffi-napi'),_ref=require('ref-napi'),_path=require('path');
const _lib=_path.join(process.cwd(),'library','math',process.platform==='win32'?'acmath':'libacmath');
const _D='double',_I='int64',_INT='int',_V='void';
const _m=_ffi.Library(_lib,{
// Constants
'ac_math_pi_const':[_D,[]],'ac_math_e_const':[_D,[]],'ac_math_tau_const':[_D,[]],'ac_math_inf':[_D,[]],
'ac_math_pi':[_D,[_INT]],'ac_math_e':[_D,[_INT]],
// Trig
'ac_sin':[_D,[_D]],'ac_cos':[_D,[_D]],'ac_tan':[_D,[_D]],
'ac_csc':[_D,[_D]],'ac_sec':[_D,[_D]],'ac_cot':[_D,[_D]],
'ac_asin':[_D,[_D]],'ac_acos':[_D,[_D]],'ac_atan':[_D,[_D]],
'ac_acsc':[_D,[_D]],'ac_asec':[_D,[_D]],'ac_acot':[_D,[_D]],
'ac_atan2':[_D,[_D,_D]],'ac_deg2rad':[_D,[_D]],'ac_rad2deg':[_D,[_D]],
// Arithmetic
'ac_pow':[_D,[_D,_D]],'ac_sqrt':[_D,[_D]],'ac_cbrt':[_D,[_D]],
'ac_abs':[_D,[_D]],'ac_abs_int':[_I,[_I]],
'ac_floor':[_D,[_D]],'ac_ceil':[_D,[_D]],'ac_round':[_D,[_D]],
'ac_hypot':[_D,[_D,_D]],'ac_log':[_D,[_D]],'ac_log2':[_D,[_D]],'ac_log10':[_D,[_D]],
'ac_mod':[_D,[_D,_D]],'ac_mod_int':[_I,[_I,_I]],
'ac_to_int':[_I,[_D]],'ac_to_dec':[_D,[_I]],
'ac_gcd':[_I,[_I,_I]],'ac_lcm':[_I,[_I,_I]],
'ac_is_prime':[_INT,[_I]],'ac_clamp':[_D,[_D,_D,_D]],
// Aggregates
'ac_sigma':[_D,['pointer',_INT]],'ac_PI_product':[_D,['pointer',_INT]],
'ac_gradient':[_V,['pointer',_INT,'pointer']],
// Statistics
'ac_stat_avg':[_D,['pointer',_INT]],'ac_stat_median':[_D,['pointer',_INT]],
'ac_stat_q1':[_D,['pointer',_INT]],'ac_stat_q3':[_D,['pointer',_INT]],
'ac_stat_mode':[_D,['pointer',_INT]],'ac_stat_min':[_D,['pointer',_INT]],
'ac_stat_max':[_D,['pointer',_INT]],'ac_stat_boxnum':[_V,['pointer',_INT,'pointer']],
});
function _arr(lst){const b=Buffer.alloc(lst.length*8);lst.forEach((v,i)=>b.writeDoubleBE(v,i*8));return b;}
function _out(n){return Buffer.alloc(n*8);}
function _rout(b,n){const r=[];for(let i=0;i<n;i++)r.push(b.readDoubleBE(i*8));return r;}
const math_PI=_m.ac_math_pi_const(),math_E=_m.ac_math_e_const(),math_TAU=_m.ac_math_tau_const(),math_inf=_m.ac_math_inf();
const math_pi=n=>_m.ac_math_pi(n),math_e=n=>_m.ac_math_e(n);
const math_sin=x=>_m.ac_sin(x),math_cos=x=>_m.ac_cos(x),math_tan=x=>_m.ac_tan(x);
const math_csc=x=>_m.ac_csc(x),math_sec=x=>_m.ac_sec(x),math_cot=x=>_m.ac_cot(x);
const math_asin=x=>_m.ac_asin(x),math_acos=x=>_m.ac_acos(x),math_atan=x=>_m.ac_atan(x);
const math_acsc=x=>_m.ac_acsc(x),math_asec=x=>_m.ac_asec(x),math_acot=x=>_m.ac_acot(x);
const math_atan2=(y,x)=>_m.ac_atan2(y,x),math_deg2rad=d=>_m.ac_deg2rad(d),math_rad2deg=r=>_m.ac_rad2deg(r);
const math_pow=(b,e)=>_m.ac_pow(b,e),math_sqrt=x=>_m.ac_sqrt(x),math_cbrt=x=>_m.ac_cbrt(x);
const math_abs=x=>_m.ac_abs(x),math_abs_int=x=>_m.ac_abs_int(x);
const math_floor=x=>_m.ac_floor(x),math_ceil=x=>_m.ac_ceil(x),math_round=x=>_m.ac_round(x);
const math_hypot=(a,b)=>_m.ac_hypot(a,b),math_log=x=>_m.ac_log(x),math_log2=x=>_m.ac_log2(x),math_log10=x=>_m.ac_log10(x);
const math_mod=(a,b)=>_m.ac_mod(a,b),math_mod_int=(a,b)=>_m.ac_mod_int(a,b);
const math_to_int=x=>_m.ac_to_int(x),math_to_dec=x=>_m.ac_to_dec(x);
const math_gcd=(a,b)=>_m.ac_gcd(a,b),math_lcm=(a,b)=>_m.ac_lcm(a,b);
const math_is_prime=n=>!!_m.ac_is_prime(n),math_clamp=(v,lo,hi)=>_m.ac_clamp(v,lo,hi);
function math_sigma(lst){return _m.ac_sigma(_arr(lst),lst.length);}
function math_PI_product(lst){return _m.ac_PI_product(_arr(lst),lst.length);}
function math_gradient(lst){const o=_out(lst.length-1);_m.ac_gradient(_arr(lst),lst.length,o);return _rout(o,lst.length-1);}
function stat_avg(l){return _m.ac_stat_avg(_arr(l),l.length);}
function stat_median(l){return _m.ac_stat_median(_arr(l),l.length);}
function stat_q1(l){return _m.ac_stat_q1(_arr(l),l.length);}
function stat_q3(l){return _m.ac_stat_q3(_arr(l),l.length);}
function stat_mode(l){return _m.ac_stat_mode(_arr(l),l.length);}
function stat_min(l){return _m.ac_stat_min(_arr(l),l.length);}
function stat_max(l){return _m.ac_stat_max(_arr(l),l.length);}
function stat_boxnum(lst){const o=_out(5);_m.ac_stat_boxnum(_arr(lst),lst.length,o);return _rout(o,5);}
// math namespace object — AC-generated JS uses math.sin(x), math.pi, etc.
const math = {
    sin:math_sin, cos:math_cos, tan:math_tan,
    csc:math_csc, sec:math_sec, cot:math_cot,
    asin:math_asin, acos:math_acos, atan:math_atan, atan2:math_atan2,
    acsc:math_acsc, asec:math_asec, acot:math_acot,
    deg2rad:math_deg2rad, rad2deg:math_rad2deg,
    sqrt:math_sqrt, cbrt:math_cbrt, abs:math_abs,
    floor:math_floor, ceil:math_ceil, round:math_round,
    ln:math_ln, log:math_log, log2:math_log2, log10:math_log10,
    pow:math_pow, hypot:math_hypot, mod:math_mod, clamp:math_clamp,
    to_int:math_to_int, to_dec:math_to_dec,
    abs_int:math_abs_int, mod_int:math_mod_int,
    gcd:math_gcd, lcm:math_lcm, is_prime:math_is_prime,
    sigma:math_sigma, PI:math_PI_product, gradient:math_gradient,
    pi:math_PI, e:math_E, tau:math_TAU, inf:math_inf,
    pi_digits:math_pi, e_digits:math_e,
};
