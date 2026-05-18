// AC ilib: math — extern "C" shared library implementation
// Build: see Makefile  →  libacmath.so (Linux) / acmath.dll (Windows)
#include "math.hpp"
#include "statistics.hpp"
#include "calculus.hpp"
#include <cstdint>
#include <cstring>

extern "C" {

// ── Constants ──────────────────────────────────────────────────────────────

double ac_math_pi_const()  { return ac_math::PI_VAL; }
double ac_math_e_const()   { return ac_math::E_VAL; }
double ac_math_tau_const() { return ac_math::TAU_VAL; }
double ac_math_em_const()  { return ac_math::EM_VAL; }
double ac_math_phi_const() { return ac_math::PHI_VAL; }
double ac_math_inf()       { return ac_math::INF; }
double ac_math_pi(int digits)  { return ac_math::pi(digits); }
double ac_math_e(int digits)   { return ac_math::e(digits); }
double ac_math_phi(int digits) { return ac_math::phi(digits); }

// ── Trig ───────────────────────────────────────────────────────────────────

double ac_sin(double x)  { return ac_math::sin(x); }
double ac_cos(double x)  { return ac_math::cos(x); }
double ac_tan(double x)  { return ac_math::tan(x); }
double ac_csc(double x)  { return ac_math::csc(x); }
double ac_sec(double x)  { return ac_math::sec(x); }
double ac_cot(double x)  { return ac_math::cot(x); }
double ac_asin(double x) { return ac_math::asin(x); }
double ac_acos(double x) { return ac_math::acos(x); }
double ac_atan(double x) { return ac_math::atan(x); }
double ac_acsc(double x) { return ac_math::acsc(x); }
double ac_asec(double x) { return ac_math::asec(x); }
double ac_acot(double x) { return ac_math::acot(x); }
double ac_atan2(double y, double x) { return ac_math::atan2(y, x); }
double ac_deg2rad(double d) { return ac_math::deg2rad(d); }
double ac_rad2deg(double r) { return ac_math::rad2deg(r); }

// ── Arithmetic ─────────────────────────────────────────────────────────────

double   ac_pow(double b, double e)           { return ac_math::pow(b, e); }
double   ac_sqrt(double x)                   { return ac_math::sqrt(x); }
double   ac_cbrt(double x)                   { return ac_math::cbrt(x); }
double   ac_abs(double x)                    { return ac_math::abs_val(x); }
int64_t  ac_abs_int(int64_t x)               { return ac_math::abs_int(x); }
double   ac_floor(double x)                  { return ac_math::floor(x); }
double   ac_ceil(double x)                   { return ac_math::ceil(x); }
double   ac_round(double x)                  { return ac_math::round(x); }
double   ac_hypot(double a, double b)        { return ac_math::hypot(a, b); }
double   ac_ln(double x)                     { return ac_math::ln(x); }
double   ac_log_base(double base, double x)  { return ac_math::log_base(base, x); }
double   ac_log2(double x)                   { return ac_math::log2(x); }
double   ac_log10(double x)                  { return ac_math::log10(x); }
double   ac_mod(double a, double b)          { return ac_math::mod(a, b); }
int64_t  ac_mod_int(int64_t a, int64_t b)   { return ac_math::mod_int(a, b); }
int64_t  ac_to_int(double x)                 { return ac_math::to_int(x); }
double   ac_to_dec(int64_t x)               { return ac_math::to_dec(x); }
int64_t  ac_gcd(int64_t a, int64_t b)       { return ac_math::gcd(a, b); }
int64_t  ac_lcm(int64_t a, int64_t b)       { return ac_math::lcm(a, b); }
int      ac_is_prime(int64_t n)              { return ac_math::is_prime(n) ? 1 : 0; }
double   ac_clamp(double v, double lo, double hi) { return ac_math::clamp(v, lo, hi); }

// ── Aggregates (pointer + length C API) ────────────────────────────────────

double ac_sigma(const double* arr, int len) {
    std::vector<double> v(arr, arr + len);
    return ac_math::sigma(v);
}
double ac_PI_product(const double* arr, int len) {
    std::vector<double> v(arr, arr + len);
    return ac_math::PI_product(v);
}
// gradient: writes len-1 results into out (caller allocates)
void ac_gradient(const double* arr, int len, double* out) {
    std::vector<double> v(arr, arr + len);
    auto g = ac_math::gradient(v);
    for (size_t i = 0; i < g.size(); i++) out[i] = g[i];
}

// ── Statistics (pointer + length C API) ────────────────────────────────────

double ac_stat_avg(const double* arr, int len) {
    return ac_math::statistics::avg(std::vector<double>(arr, arr + len));
}
double ac_stat_median(const double* arr, int len) {
    return ac_math::statistics::median(std::vector<double>(arr, arr + len));
}
double ac_stat_q1(const double* arr, int len) {
    return ac_math::statistics::q1(std::vector<double>(arr, arr + len));
}
double ac_stat_q3(const double* arr, int len) {
    return ac_math::statistics::q3(std::vector<double>(arr, arr + len));
}
double ac_stat_mode(const double* arr, int len) {
    return ac_math::statistics::mode(std::vector<double>(arr, arr + len));
}
double ac_stat_min(const double* arr, int len) {
    return ac_math::statistics::min_val(std::vector<double>(arr, arr + len));
}
double ac_stat_max(const double* arr, int len) {
    return ac_math::statistics::max_val(std::vector<double>(arr, arr + len));
}
// boxnum: writes 5 results into out [min,q1,median,q3,max]
void ac_stat_boxnum(const double* arr, int len, double* out) {
    auto v = ac_math::statistics::boxnum(std::vector<double>(arr, arr + len));
    for (int i = 0; i < 5; i++) out[i] = v[i];
}

// ── Expression evaluator ───────────────────────────────────────────────────

namespace {
    struct EvalCtx { const char* p; };
    static double ev_expr(EvalCtx& s);
    static double ev_term(EvalCtx& s);
    static double ev_factor(EvalCtx& s);
    static double ev_base(EvalCtx& s);

    static void ev_skip(EvalCtx& s) {
        while (*s.p && (*s.p == ' ' || *s.p == '\t')) s.p++;
    }

    static double ev_base(EvalCtx& s) {
        ev_skip(s);
        if (*s.p == '(') {
            s.p++;
            double v = ev_expr(s); ev_skip(s);
            if (*s.p == ')') s.p++;
            return v;
        }
        if (*s.p == '-') { s.p++; return -ev_base(s); }
        if (*s.p == '+') { s.p++; return ev_base(s); }
        if (std::isalpha((unsigned char)*s.p) || *s.p == '_') {
            char name[64]; int ni = 0;
            while ((std::isalnum((unsigned char)*s.p) || *s.p == '_') && ni < 63)
                name[ni++] = *s.p++;
            name[ni] = 0;
            ev_skip(s);
            if (!strcmp(name, "pi"))  return ac_math::PI_VAL;
            if (!strcmp(name, "e"))   return ac_math::E_VAL;
            if (!strcmp(name, "tau")) return ac_math::TAU_VAL;
            if (!strcmp(name, "em"))  return ac_math::EM_VAL;
            if (!strcmp(name, "phi")) return ac_math::PHI_VAL;
            if (!strcmp(name, "inf")) return ac_math::INF;
            if (*s.p == '(') {
                s.p++;
                double a = ev_expr(s); ev_skip(s);
                if (*s.p == ',') {
                    s.p++; double b = ev_expr(s); ev_skip(s);
                    if (*s.p == ')') s.p++;
                    if (!strcmp(name, "pow"))   return std::pow(a, b);
                    if (!strcmp(name, "atan2")) return std::atan2(a, b);
                    if (!strcmp(name, "log"))   return std::log(b) / std::log(a);
                    if (!strcmp(name, "hypot")) return std::hypot(a, b);
                    if (!strcmp(name, "mod"))   return std::fmod(a, b);
                    if (!strcmp(name, "min"))   return a < b ? a : b;
                    if (!strcmp(name, "max"))   return a > b ? a : b;
                    return 0.0;
                }
                if (*s.p == ')') s.p++;
                if (!strcmp(name, "sin"))     return std::sin(a);
                if (!strcmp(name, "cos"))     return std::cos(a);
                if (!strcmp(name, "tan"))     return std::tan(a);
                if (!strcmp(name, "asin"))    return std::asin(a);
                if (!strcmp(name, "acos"))    return std::acos(a);
                if (!strcmp(name, "atan"))    return std::atan(a);
                if (!strcmp(name, "sqrt"))    return std::sqrt(a);
                if (!strcmp(name, "cbrt"))    return std::cbrt(a);
                if (!strcmp(name, "abs"))     return std::fabs(a);
                if (!strcmp(name, "floor"))   return std::floor(a);
                if (!strcmp(name, "ceil"))    return std::ceil(a);
                if (!strcmp(name, "round"))   return std::round(a);
                if (!strcmp(name, "ln"))      return std::log(a);
                if (!strcmp(name, "log"))     return std::log(a);
                if (!strcmp(name, "log2"))    return std::log2(a);
                if (!strcmp(name, "log10"))   return std::log10(a);
                if (!strcmp(name, "exp"))     return std::exp(a);
                if (!strcmp(name, "deg2rad")) return a * ac_math::PI_VAL / 180.0;
                if (!strcmp(name, "rad2deg")) return a * 180.0 / ac_math::PI_VAL;
                return 0.0;
            }
            return 0.0;
        }
        char* end; double v = strtod(s.p, &end); s.p = end; return v;
    }

    static double ev_factor(EvalCtx& s) {
        double base = ev_base(s); ev_skip(s);
        if (*s.p == '^') { s.p++; double e = ev_factor(s); return std::pow(base, e); }
        return base;
    }

    static double ev_term(EvalCtx& s) {
        double v = ev_factor(s); ev_skip(s);
        while (*s.p == '*' || *s.p == '/' || *s.p == '%') {
            char op = *s.p++; double r = ev_factor(s);
            v = op == '*' ? v * r : op == '/' ? (r != 0.0 ? v / r : 0.0) : std::fmod(v, r);
            ev_skip(s);
        }
        return v;
    }

    static double ev_expr(EvalCtx& s) {
        double v = ev_term(s); ev_skip(s);
        while (*s.p == '+' || *s.p == '-') {
            char op = *s.p++; double r = ev_term(s);
            v = op == '+' ? v + r : v - r;
            ev_skip(s);
        }
        return v;
    }
} // anonymous namespace

double ac_eval(const char* expr) {
    if (!expr) return 0.0;
    EvalCtx s{ expr };
    return ev_expr(s);
}

} // extern "C"
