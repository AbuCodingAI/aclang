// AC ilib: math — Mathematics library (C++ backend)
// use ilib math
#pragma once
#include <cmath>
#include <complex>
#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <limits>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <string>

namespace ac_math {

// ── LongInt: explicit signed 96-bit integer storage ───────────────────────
//
// Normal AC integers are 64-bit. LongInt is intentionally explicit:
// AC code must declare `math.LongInt x = ...`.
class LongInt {
    std::string digits_;

public:
    LongInt() : digits_("0") {}
    explicit LongInt(const std::string& decimal) : digits_(decimal.empty() ? "0" : decimal) {}
    explicit LongInt(const char* decimal) : digits_(decimal && *decimal ? decimal : "0") {}
    explicit LongInt(long long value) : digits_(std::to_string(value)) {}

    const std::string& str() const { return digits_; }
};

// ── GoodDec: decimal (unscaled, scale) storage ─────────────────────────────
//
// Stores an unscaled integer plus a base-10 scale, like Java BigDecimal:
// value = unscaled * 10^scale
// Example: (123, -2) => 1.23
class GoodDec {
    std::string unscaled_;
    long long scale_ = 0;

    static std::string format(const std::string& unscaledDigits, long long scale) {
        if (unscaledDigits.empty()) return "0";
        std::string u = unscaledDigits;
        bool neg = false;
        if (!u.empty() && u[0] == '-') {
            neg = true;
            u.erase(0, 1);
        }
        if (u.empty()) u = "0";
        // Strip leading zeros (keep one)
        size_t nz = u.find_first_not_of('0');
        if (nz == std::string::npos) u = "0";
        else if (nz > 0) u.erase(0, nz);

        auto withSign = [&](const std::string& body) -> std::string {
            return (neg && body != "0") ? ("-" + body) : body;
        };

        if (scale == 0) return withSign(u);
        if (scale > 0) return withSign(u + std::string((size_t)scale, '0'));

        long long places = -scale;
        std::string body;
        if ((long long)u.size() > places) {
            size_t cut = u.size() - (size_t)places;
            body = u.substr(0, cut) + "." + u.substr(cut);
        } else {
            body = "0." + std::string((size_t)places - u.size(), '0') + u;
        }
        return withSign(body);
    }

public:
    GoodDec() : unscaled_("0"), scale_(0) {}
    GoodDec(const std::string& unscaled, long long scale) : unscaled_(unscaled.empty() ? "0" : unscaled), scale_(scale) {}
    GoodDec(long long unscaled, long long scale) : unscaled_(std::to_string(unscaled)), scale_(scale) {}

    const std::string& unscaled_str() const { return unscaled_; }
    long long scale() const { return scale_; }
    std::string str() const { return format(unscaled_, scale_); }
};

// ── Constants ──────────────────────────────────────────────────────────────

constexpr double PI_VAL  = 3.141592653589793238462643383279502884197;
constexpr double E_VAL   = 2.718281828459045235360287471352662497757;
constexpr double TAU_VAL = 6.283185307179586476925286766559005768394;
constexpr double EM_VAL  = 0.5772156649015328606065120900824024310422;
constexpr double PHI_VAL = 1.618033988749894848204586834365638117720;  // golden ratio
constexpr double INF     = std::numeric_limits<double>::infinity();
constexpr double NAN_VAL = std::numeric_limits<double>::quiet_NaN();

// pi(digits)/e(digits)/phi(digits) — truncate to N decimal places (max ~15, IEEE double limit)
inline double pi(int digits) {
    if (digits <= 0 || digits > 15) return PI_VAL;
    double f = std::pow(10.0, digits);
    return std::floor(PI_VAL * f) / f;
}
inline double e(int digits) {
    if (digits <= 0 || digits > 15) return E_VAL;
    double f = std::pow(10.0, digits);
    return std::floor(E_VAL * f) / f;
}
inline double phi(int digits) {
    if (digits <= 0 || digits > 15) return PHI_VAL;
    double f = std::pow(10.0, digits);
    return std::floor(PHI_VAL * f) / f;
}

// ── Core trig ──────────────────────────────────────────────────────────────

inline double sin(double x)  { return std::sin(x); }
inline double cos(double x)  { return std::cos(x); }
inline double tan(double x)  { return std::tan(x); }

// Reciprocal trig
inline double csc(double x)  { return 1.0 / std::sin(x); }
inline double sec(double x)  { return 1.0 / std::cos(x); }
inline double cot(double x)  { return std::cos(x) / std::sin(x); }

// Inverse trig
inline double asin(double x) { return std::asin(x); }
inline double acos(double x) { return std::acos(x); }
inline double atan(double x) { return std::atan(x); }
inline double atan2(double y, double x) { return std::atan2(y, x); }

// Inverse reciprocal trig
inline double acsc(double x) { return std::asin(1.0 / x); }
inline double asec(double x) { return std::acos(1.0 / x); }
inline double acot(double x) { return (x > 0 ? 1.0 : -1.0) * std::atan(1.0 / std::fabs(x)); }

// Degree/radian
inline double deg2rad(double d) { return d * (PI_VAL / 180.0); }
inline double rad2deg(double r) { return r * (180.0 / PI_VAL); }

// ── Basic arithmetic ───────────────────────────────────────────────────────

inline double pow(double base, double exp)  { return std::pow(base, exp); }
inline double sqrt(double x)               { return std::sqrt(x); }
inline double cbrt(double x)               { return std::cbrt(x); }
inline double abs_val(double x)            { return std::fabs(x); }
inline long long abs_int(long long x)      { return std::llabs(x); }
inline double floor(double x)              { return std::floor(x); }
inline double ceil(double x)              { return std::ceil(x); }
inline double round(double x)             { return std::round(x); }
inline double hypot(double a, double b)   { return std::hypot(a, b); }
inline double ln(double x)               { return std::log(x); }
inline double log_base(double base, double x) { return std::log(x) / std::log(base); }
inline double log2(double x)             { return std::log2(x); }
inline double log10(double x)            { return std::log10(x); }

// mod(a, b) — fills AC's no-native-modulus gap; Python-style (result has sign of b)
inline double mod(double a, double b) {
    double r = std::fmod(a, b);
    if (r != 0.0 && (r < 0) != (b < 0)) r += b;
    return r;
}
inline long long mod_int(long long a, long long b) {
    long long r = a % b;
    if (r != 0 && (r < 0) != (b < 0)) r += b;
    return r;
}

// Type conversion
inline long long to_int(double x)  { return static_cast<long long>(x); }
inline double    to_dec(long long x) { return static_cast<double>(x); }

// GCD / LCM
inline long long gcd(long long a, long long b) {
    a = std::llabs(a); b = std::llabs(b);
    while (b) { a %= b; std::swap(a, b); }
    return a;
}
inline long long lcm(long long a, long long b) {
    return std::llabs(a / gcd(a, b) * b);
}

// ── Aggregate (list/tuple operations) ─────────────────────────────────────

// sigma: Σ — sum of all elements
inline double sigma(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0);
}
inline long long sigma_int(const std::vector<long long>& v) {
    return std::accumulate(v.begin(), v.end(), 0LL);
}

// PI_product: Π — product of all elements
inline double PI_product(const std::vector<double>& v) {
    double r = 1.0;
    for (double x : v) r *= x;
    return r;
}
inline long long PI_product_int(const std::vector<long long>& v) {
    long long r = 1;
    for (long long x : v) r *= x;
    return r;
}

// gradient: finite differences [v[1]-v[0], v[2]-v[1], ...]
inline std::vector<double> gradient(const std::vector<double>& v) {
    if (v.size() < 2) return {};
    std::vector<double> out;
    out.reserve(v.size() - 1);
    for (size_t i = 1; i < v.size(); i++)
        out.push_back(v[i] - v[i - 1]);
    return out;
}

// clamp
inline double clamp(double v, double lo, double hi) {
    return std::fmin(std::fmax(v, lo), hi);
}

// is_prime
inline bool is_prime(long long n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (long long i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}

} // namespace ac_math

// ── Flat API (callable from AC-generated C++) ──────────────────────────────

// Constants
constexpr double math_pi  = ac_math::PI_VAL;
constexpr double math_e   = ac_math::E_VAL;
constexpr double math_tau = ac_math::TAU_VAL;
constexpr double math_em  = ac_math::EM_VAL;
constexpr double math_phi = ac_math::PHI_VAL;
constexpr double math_inf = ac_math::INF;

// pi/e/phi with digits — truncated precision
inline double math_pi_digits(int d)  { return ac_math::pi(d); }
inline double math_e_digits(int d)   { return ac_math::e(d); }
inline double math_phi_digits(int d) { return ac_math::phi(d); }

// Trig
inline double math_sin(double x)  { return ac_math::sin(x); }
inline double math_cos(double x)  { return ac_math::cos(x); }
inline double math_tan(double x)  { return ac_math::tan(x); }
inline double math_csc(double x)  { return ac_math::csc(x); }
inline double math_sec(double x)  { return ac_math::sec(x); }
inline double math_cot(double x)  { return ac_math::cot(x); }
inline double math_asin(double x) { return ac_math::asin(x); }
inline double math_acos(double x) { return ac_math::acos(x); }
inline double math_atan(double x) { return ac_math::atan(x); }
inline double math_atan2(double y, double x) { return ac_math::atan2(y, x); }
inline double math_cbrt(double x) { return ac_math::cbrt(x); }
inline double math_acsc(double x) { return ac_math::acsc(x); }
inline double math_asec(double x) { return ac_math::asec(x); }
inline double math_acot(double x) { return ac_math::acot(x); }

// Arithmetic
inline double    math_pow(double b, double e)    { return ac_math::pow(b, e); }
inline double    math_sqrt(double x)             { return ac_math::sqrt(x); }
inline double    math_abs(double x)              { return ac_math::abs_val(x); }
inline long long math_abs_int(long long x)       { return ac_math::abs_int(x); }
inline double    math_floor(double x)            { return ac_math::floor(x); }
inline double    math_ceil(double x)             { return ac_math::ceil(x); }
inline double    math_round(double x)            { return ac_math::round(x); }
inline double    math_ln(double x)               { return ac_math::ln(x); }
inline double    math_log(double base, double x) { return ac_math::log_base(base, x); }
inline double    math_log2(double x)             { return ac_math::log2(x); }
inline double    math_log10(double x)            { return ac_math::log10(x); }
inline double    math_hypot(double a, double b)  { return ac_math::hypot(a, b); }
inline double    math_mod(double a, double b)    { return ac_math::mod(a, b); }
inline long long math_mod_int(long long a, long long b) { return ac_math::mod_int(a, b); }
inline long long math_to_int(double x)           { return ac_math::to_int(x); }
inline double    math_to_dec(long long x)        { return ac_math::to_dec(x); }
inline long long math_gcd(long long a, long long b) { return ac_math::gcd(a, b); }
inline long long math_lcm(long long a, long long b) { return ac_math::lcm(a, b); }
inline bool      math_is_prime(long long n)      { return ac_math::is_prime(n); }

// Aggregate
inline double    math_sigma(const std::vector<double>& v)   { return ac_math::sigma(v); }
inline long long math_sigma_int(const std::vector<long long>& v) { return ac_math::sigma_int(v); }
inline double    math_PI(const std::vector<double>& v)           { return ac_math::PI_product(v); }
inline std::vector<double> math_gradient(const std::vector<double>& v) { return ac_math::gradient(v); }

inline double    math_clamp(double v, double lo, double hi)  { return ac_math::clamp(v, lo, hi); }
inline double    math_deg2rad(double d) { return ac_math::deg2rad(d); }
inline double    math_rad2deg(double r) { return ac_math::rad2deg(r); }

// Complex numbers
using ac_complex = std::complex<double>;
const ac_complex math_i(0.0, 1.0);
inline double math_re(ac_complex z) { return z.real(); }
inline double math_im(ac_complex z) { return z.imag(); }

// Expression evaluator (delegates to ac_eval from shared lib, declared here for header-only use)
extern "C" double ac_eval(const char* expr);
inline double math_eval(const std::string& expr) { return ac_eval(expr.c_str()); }

using math_LongInt = ac_math::LongInt;

// math namespace object — AC-generated C++ uses math.sin(x), math.pi, etc.
struct _AcMathNS {
    double sin(double x) const   { return math_sin(x);   }
    double cos(double x) const   { return math_cos(x);   }
    double tan(double x) const   { return math_tan(x);   }
    double csc(double x) const   { return math_csc(x);   }
    double sec(double x) const   { return math_sec(x);   }
    double cot(double x) const   { return math_cot(x);   }
    double asin(double x) const  { return math_asin(x);  }
    double acos(double x) const  { return math_acos(x);  }
    double atan(double x) const  { return math_atan(x);  }
    double acsc(double x) const  { return math_acsc(x);  }
    double asec(double x) const  { return math_asec(x);  }
    double acot(double x) const  { return math_acot(x);  }
    double atan2(double y, double x) const { return math_atan2(y, x); }
    double deg2rad(double d) const { return math_deg2rad(d); }
    double rad2deg(double r) const { return math_rad2deg(r); }
    double pow(double b, double e) const { return math_pow(b, e); }
    double sqrt(double x) const  { return math_sqrt(x);  }
    double cbrt(double x) const  { return math_cbrt(x);  }
    double abs(double x) const   { return math_abs(x);   }
    long long abs_int(long long x) const { return math_abs_int(x); }
    double floor(double x) const { return math_floor(x); }
    double ceil(double x) const  { return math_ceil(x);  }
    double round(double x) const { return math_round(x); }
    double hypot(double a, double b) const { return math_hypot(a, b); }
    double ln(double x) const    { return math_ln(x);    }
    double log(double base, double x) const { return math_log(base, x); }
    double log2(double x) const  { return math_log2(x);  }
    double log10(double x) const { return math_log10(x); }
    double mod(double a, double b) const { return math_mod(a, b); }
    long long mod_int(long long a, long long b) const { return math_mod_int(a, b); }
    long long to_int(double x) const { return math_to_int(x); }
    double to_dec(long long x) const { return math_to_dec(x); }
    long long gcd(long long a, long long b) const { return math_gcd(a, b); }
    long long lcm(long long a, long long b) const { return math_lcm(a, b); }
    bool is_prime(long long n) const { return math_is_prime(n); }
    double clamp(double v, double lo, double hi) const { return math_clamp(v, lo, hi); }
    double sigma(const std::vector<double>& v) const   { return math_sigma(v); }
    double PI(const std::vector<double>& v) const      { return math_PI(v); }
    std::vector<double> gradient(const std::vector<double>& v) const { return math_gradient(v); }
    double eval(const std::string& expr) const { return math_eval(expr); }
    double pi_digits(int n) const  { return math_pi_digits(n);  }
    double e_digits(int n) const   { return math_e_digits(n);   }
    double phi_digits(int n) const { return math_phi_digits(n); }
    const double pi  = math_pi;
    const double e   = math_e;
    const double tau = math_tau;
    const double em  = math_em;
    const double phi = math_phi;
    const double inf = math_inf;
};
inline _AcMathNS math;
