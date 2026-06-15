/* AC ilib: math — Pure C header for FFI consumers (Go, V, ASM)
   All functions exported from libacmath.so / acmath.dll */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */
double  ac_math_pi_const(void);
double  ac_math_e_const(void);
double  ac_math_tau_const(void);
double  ac_math_em_const(void);
double  ac_math_phi_const(void);
double  ac_math_inf(void);
double  ac_math_pi(int digits);
double  ac_math_e(int digits);
double  ac_math_phi(int digits);

/* Trig */
double  ac_sin(double x);
double  ac_cos(double x);
double  ac_tan(double x);
double  ac_csc(double x);
double  ac_sec(double x);
double  ac_cot(double x);
double  ac_asin(double x);
double  ac_acos(double x);
double  ac_atan(double x);
double  ac_acsc(double x);
double  ac_asec(double x);
double  ac_acot(double x);
double  ac_atan2(double y, double x);
double  ac_deg2rad(double d);
double  ac_rad2deg(double r);

/* Arithmetic */
double   ac_pow(double b, double e);
double   ac_sqrt(double x);
double   ac_cbrt(double x);
double   ac_abs(double x);
int64_t  ac_abs_int(int64_t x);
double   ac_floor(double x);
double   ac_ceil(double x);
double   ac_round(double x);
double   ac_hypot(double a, double b);
double   ac_ln(double x);
double   ac_log_base(double base, double x);
double   ac_log2(double x);
double   ac_log10(double x);
double   ac_mod(double a, double b);
int64_t  ac_mod_int(int64_t a, int64_t b);
int64_t  ac_to_int(double x);
double   ac_to_dec(int64_t x);
int64_t  ac_gcd(int64_t a, int64_t b);
int64_t  ac_lcm(int64_t a, int64_t b);
int      ac_is_prime(int64_t n);
double   ac_clamp(double v, double lo, double hi);

/* Aggregates (pointer + length) */
double  ac_sigma(const double* arr, int len);
double  ac_PI_product(const double* arr, int len);
void    ac_gradient(const double* arr, int len, double* out);

/* Statistics (pointer + length) */
double  ac_stat_avg(const double* arr, int len);
double  ac_stat_median(const double* arr, int len);
double  ac_stat_q1(const double* arr, int len);
double  ac_stat_q3(const double* arr, int len);
double  ac_stat_mode(const double* arr, int len);
double  ac_stat_min(const double* arr, int len);
double  ac_stat_max(const double* arr, int len);
void    ac_stat_boxnum(const double* arr, int len, double* out);

/* Expression evaluator */
double  ac_eval(const char* expr);

/* BNY print helper — print double as decimal, used by native binary backend */
void    ac_print_double(double x);

#ifdef __cplusplus
}
#endif

/* Complex number support (C11 _Complex) */
#ifdef __STDC_NO_COMPLEX__
#  define math_i 0  /* complex not available */
#else
#  include <complex.h>
#  define math_i  (_Complex_I)
static inline double math_re(double _Complex z) { return creal(z); }
static inline double math_im(double _Complex z) { return cimag(z); }
#endif

/* AC-style aliases — match AC source code names (math_sin, math_PI, etc.) */
#define math_sin     ac_sin
#define math_cos     ac_cos
#define math_tan     ac_tan
#define math_csc     ac_csc
#define math_sec     ac_sec
#define math_cot     ac_cot
#define math_asin    ac_asin
#define math_acos    ac_acos
#define math_atan    ac_atan
#define math_acsc    ac_acsc
#define math_asec    ac_asec
#define math_acot    ac_acot
#define math_atan2   ac_atan2
#define math_deg2rad ac_deg2rad
#define math_rad2deg ac_rad2deg
#define math_pow     ac_pow
#define math_sqrt    ac_sqrt
#define math_cbrt    ac_cbrt
#define math_abs     ac_abs
#define math_abs_int ac_abs_int
#define math_floor   ac_floor
#define math_ceil    ac_ceil
#define math_round   ac_round
#define math_hypot   ac_hypot
#define math_ln      ac_ln
#define math_log     ac_log_base
#define math_log2    ac_log2
#define math_log10   ac_log10
#define math_mod     ac_mod
#define math_mod_int ac_mod_int
#define math_to_int  ac_to_int
#define math_to_dec  ac_to_dec
#define math_gcd     ac_gcd
#define math_lcm     ac_lcm
#define math_is_prime ac_is_prime
#define math_clamp   ac_clamp
#define math_sigma   ac_sigma
#define math_PI         ac_PI_product
#define math_gradient   ac_gradient
#define math_eval    ac_eval

/* Constants — computed once at link time via inline initializer */
#define math_pi  (ac_math_pi_const())
#define math_e   (ac_math_e_const())
#define math_tau (ac_math_tau_const())
#define math_em  (ac_math_em_const())
#define math_phi (ac_math_phi_const())
#define math_inf (ac_math_inf())
