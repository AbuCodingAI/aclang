// AC ilib: math — Calculus submodule (C++ backend, numerical)
// from ilib math use calculus
// All operations are numerical approximations.
// For symbolic math use Python backend with sympy via foreign block.
#pragma once
#include "math.hpp"
#include <functional>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ac_math {
namespace calculus {

using Fn = std::function<double(double)>;

// ── Numerical integration (adaptive Simpson's rule) ────────────────────────

namespace _impl {
inline double simpson(Fn f, double a, double b) {
    double c = (a + b) / 2.0;
    return (b - a) / 6.0 * (f(a) + 4.0 * f(c) + f(b));
}
inline double adaptive(Fn f, double a, double b, double tol,
                       double whole, int depth) {
    double c    = (a + b) / 2.0;
    double left = simpson(f, a, c);
    double right= simpson(f, c, b);
    if (depth <= 0 || std::fabs(left + right - whole) <= 15.0 * tol)
        return left + right + (left + right - whole) / 15.0;
    return adaptive(f, a, c, tol / 2.0, left, depth - 1) +
           adaptive(f, c, b, tol / 2.0, right, depth - 1);
}
} // namespace _impl

// integrate(f, a, b) — ∫_a^b f(x) dx
inline double integrate(Fn f, double a, double b, double tol = 1e-9) {
    return _impl::adaptive(f, a, b, tol, _impl::simpson(f, a, b), 50);
}

// ── Numerical limit ────────────────────────────────────────────────────────

// limit(f, x) — two-sided numerical limit as t → x
// Uses Richardson extrapolation with h → 0
inline double limit(Fn f, double x, double h = 1e-5) {
    // Try two-sided average; if x is a pole, returns inf/nan
    double left  = f(x - h);
    double right = f(x + h);
    if (std::isinf(left) || std::isnan(left) || std::isinf(right) || std::isnan(right))
        return std::numeric_limits<double>::quiet_NaN();
    return (left + right) / 2.0;
}

// ── Numerical derivative ───────────────────────────────────────────────────

// derivative(f, x) — f'(x) via central finite difference
inline double derivative(Fn f, double x, double h = 1e-7) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

// ── Extrema (golden-section search) ───────────────────────────────────────

// minima(f, a, b) — x where f has a local minimum on [a, b]
inline double minima(Fn f, double a, double b, double tol = 1e-9) {
    const double phi = (std::sqrt(5.0) - 1.0) / 2.0;
    double c = b - phi * (b - a);
    double d = a + phi * (b - a);
    while (std::fabs(b - a) > tol) {
        if (f(c) < f(d)) { b = d; }
        else             { a = c; }
        c = b - phi * (b - a);
        d = a + phi * (b - a);
    }
    return (a + b) / 2.0;
}

// maxima(f, a, b) — x where f has a local maximum on [a, b]
inline double maxima(Fn f, double a, double b, double tol = 1e-9) {
    return minima([&f](double x){ return -f(x); }, a, b, tol);
}

// extrema(f, a, b) — returns {x_min, x_max} on [a, b]
inline std::vector<double> extrema(Fn f, double a, double b) {
    return { minima(f, a, b), maxima(f, a, b) };
}

} // namespace calculus
} // namespace ac_math

// ── Flat API ───────────────────────────────────────────────────────────────

using AcFn = std::function<double(double)>;

inline double calc_integrate(AcFn f, double a, double b)
    { return ac_math::calculus::integrate(f, a, b); }

inline double calc_limit(AcFn f, double x)
    { return ac_math::calculus::limit(f, x); }

inline double calc_derivative(AcFn f, double x)
    { return ac_math::calculus::derivative(f, x); }

inline double calc_minima(AcFn f, double a, double b)
    { return ac_math::calculus::minima(f, a, b); }

inline double calc_maxima(AcFn f, double a, double b)
    { return ac_math::calculus::maxima(f, a, b); }

inline std::vector<double> calc_extrema(AcFn f, double a, double b)
    { return ac_math::calculus::extrema(f, a, b); }
