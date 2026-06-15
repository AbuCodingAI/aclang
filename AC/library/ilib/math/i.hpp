// AC ilib: math — Complex numbers (C++ backend only)
// from ilib math use i
// Note: Euler's identity: e^(i*pi) = -1. This is a stub.
// Full symbolic complex arithmetic is C++ only.
// Python backend has native complex(); other backends unsupported.
#pragma once
#include <complex>
#include "math.hpp"

namespace ac_math {
namespace complex {

using Complex = std::complex<double>;

// Imaginary unit
constexpr Complex I(0.0, 1.0);

// Construction
inline Complex make(double re, double im)    { return Complex(re, im); }
inline Complex from_polar(double r, double theta) { return std::polar(r, theta); }

// Decomposition
inline double real(Complex z)  { return z.real(); }
inline double imag(Complex z)  { return z.imag(); }
inline double abs(Complex z)   { return std::abs(z); }
inline double arg(Complex z)   { return std::arg(z); }
inline Complex conj(Complex z) { return std::conj(z); }

// Arithmetic
inline Complex pow(Complex z, double e)      { return std::pow(z, e); }
inline Complex exp(Complex z)               { return std::exp(z); }
inline Complex log(Complex z)               { return std::log(z); }
inline Complex sqrt(Complex z)              { return std::sqrt(z); }

// Euler's identity: e^(i*pi) + 1 = 0
// ac_math::complex::euler_check() returns true if floating-point precision holds
inline bool euler_check() {
    Complex result = std::exp(I * PI_VAL) + Complex(1.0, 0.0);
    return std::abs(result) < 1e-15;
}

} // namespace complex
} // namespace ac_math

// ── Flat API ───────────────────────────────────────────────────────────────

using AcComplex = std::complex<double>;
constexpr AcComplex math_i(0.0, 1.0);

inline AcComplex math_complex(double re, double im)    { return ac_math::complex::make(re, im); }
inline double    math_real(AcComplex z)               { return ac_math::complex::real(z); }
inline double    math_imag(AcComplex z)               { return ac_math::complex::imag(z); }
inline double    math_complex_abs(AcComplex z)        { return ac_math::complex::abs(z); }
inline AcComplex math_conj(AcComplex z)               { return ac_math::complex::conj(z); }
inline AcComplex math_complex_exp(AcComplex z)        { return ac_math::complex::exp(z); }
