// AC ilib: math — Statistics submodule (C++ backend)
// from ilib math use statistics
#pragma once
#include "math.hpp"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <map>

namespace ac_math {
namespace statistics {

// ── Internal helpers ───────────────────────────────────────────────────────

inline std::vector<double> sorted_copy(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v;
}

// ── Core statistics ────────────────────────────────────────────────────────

inline double avg(const std::vector<double>& v) {
    if (v.empty()) throw std::runtime_error("Preposterous: avg() called on empty list");
    return ac_math::sigma(v) / static_cast<double>(v.size());
}

inline double median(std::vector<double> v) {
    if (v.empty()) throw std::runtime_error("Preposterous: median() called on empty list");
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 0) ? (v[n/2 - 1] + v[n/2]) / 2.0 : v[n/2];
}

inline double q1(std::vector<double> v) {
    if (v.size() < 2) throw std::runtime_error("Preposterous: q1() requires at least 2 elements");
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    // Lower half (excluding median for odd n)
    std::vector<double> lower(v.begin(), v.begin() + n / 2);
    return median(lower);
}

inline double q3(std::vector<double> v) {
    if (v.size() < 2) throw std::runtime_error("Preposterous: q3() requires at least 2 elements");
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    // Upper half (excluding median for odd n)
    std::vector<double> upper(v.begin() + (n + 1) / 2, v.end());
    return median(upper);
}

inline double mode(std::vector<double> v) {
    if (v.empty()) throw std::runtime_error("Preposterous: mode() called on empty list");
    std::map<double, int> freq;
    for (double x : v) freq[x]++;
    double best = v[0];
    int   bestCount = 0;
    for (auto& [val, cnt] : freq)
        if (cnt > bestCount) { bestCount = cnt; best = val; }
    return best;
}

inline double min_val(const std::vector<double>& v) {
    if (v.empty()) throw std::runtime_error("Preposterous: min() called on empty list");
    return *std::min_element(v.begin(), v.end());
}

inline double max_val(const std::vector<double>& v) {
    if (v.empty()) throw std::runtime_error("Preposterous: max() called on empty list");
    return *std::max_element(v.begin(), v.end());
}

// boxnum: five-number summary as a vector [min, q1, median, q3, max]
inline std::vector<double> boxnum(std::vector<double> v) {
    if (v.size() < 4) throw std::runtime_error("Preposterous: boxnum() requires at least 4 elements");
    return { min_val(v), q1(v), median(v), q3(v), max_val(v) };
}

} // namespace statistics
} // namespace ac_math

// ── Flat API ───────────────────────────────────────────────────────────────

inline double stat_avg(const std::vector<double>& v)    { return ac_math::statistics::avg(v); }
inline double stat_median(std::vector<double> v)        { return ac_math::statistics::median(v); }
inline double stat_q1(std::vector<double> v)            { return ac_math::statistics::q1(v); }
inline double stat_q3(std::vector<double> v)            { return ac_math::statistics::q3(v); }
inline double stat_mode(std::vector<double> v)          { return ac_math::statistics::mode(v); }
inline double stat_min(const std::vector<double>& v)    { return ac_math::statistics::min_val(v); }
inline double stat_max(const std::vector<double>& v)    { return ac_math::statistics::max_val(v); }
inline std::vector<double> stat_boxnum(std::vector<double> v) { return ac_math::statistics::boxnum(v); }
