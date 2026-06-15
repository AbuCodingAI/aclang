#pragma once
#include <map>
#include <string>
#include <iostream>

// dec: exact decimal arithmetic.
// Stored as a sparse map from base-10 exponent (int) to digit (0-9).
// Key k represents the 10^k place.
//   42.7  → { 1:4,  0:2, -1:7 }
//   0.1   → { -1:1 }
//   314   → { 2:3,  1:1,  0:4 }

struct Dec {
    std::map<int, int> digits;  // exponent → digit (0..9)
    bool negative = false;

    Dec() = default;

    static Dec fromInt(long long n) {
        Dec d;
        if (n < 0) { d.negative = true; n = -n; }
        if (n == 0) return d;
        int exp = 0;
        while (n > 0) { d.digits[exp++] = (int)(n % 10); n /= 10; }
        return d;
    }

    static Dec fromString(const std::string& s) {
        Dec d;
        std::string t = s;
        if (!t.empty() && t[0] == '-') { d.negative = true; t = t.substr(1); }
        size_t dot = t.find('.');
        std::string intPart  = (dot == std::string::npos) ? t : t.substr(0, dot);
        std::string fracPart = (dot == std::string::npos) ? "" : t.substr(dot + 1);

        // integer part: rightmost digit is exponent 0
        for (int i = (int)intPart.size() - 1, exp = 0; i >= 0; i--, exp++)
            if (intPart[i] != '0' || exp == 0) {
                int dig = intPart[i] - '0';
                if (dig) d.digits[exp] = dig;
            }
        // fractional part: first digit after dot is exponent -1
        for (int i = 0; i < (int)fracPart.size(); i++) {
            int dig = fracPart[i] - '0';
            if (dig) d.digits[-(i + 1)] = dig;
        }
        d.prune();
        return d;
    }

    void prune() {
        for (auto it = digits.begin(); it != digits.end();)
            it->second == 0 ? it = digits.erase(it) : ++it;
    }

    bool isZero() const { return digits.empty(); }

    std::string toString() const {
        if (isZero()) return "0";
        int minExp = digits.begin()->first;
        int maxExp = digits.rbegin()->first;
        std::string result;
        if (negative) result += '-';
        // Always emit at least "0" for the integer part
        int intTop = std::max(maxExp, 0);
        for (int e = intTop; e >= 0; e--)
            result += (char)('0' + (digits.count(e) ? digits.at(e) : 0));
        if (minExp < 0) {
            result += '.';
            for (int e = -1; e >= minExp; e--)
                result += (char)('0' + (digits.count(e) ? digits.at(e) : 0));
        }
        return result;
    }

    // Compare absolute values. Returns -1, 0, or 1.
    static int cmpAbs(const Dec& a, const Dec& b) {
        int aMax = a.isZero() ? 0 : a.digits.rbegin()->first;
        int bMax = b.isZero() ? 0 : b.digits.rbegin()->first;
        int aMin = a.isZero() ? 0 : a.digits.begin()->first;
        int bMin = b.isZero() ? 0 : b.digits.begin()->first;
        int hi = std::max(aMax, bMax);
        int lo = std::min(aMin, bMin);
        for (int e = hi; e >= lo; e--) {
            int da = a.digits.count(e) ? a.digits.at(e) : 0;
            int db = b.digits.count(e) ? b.digits.at(e) : 0;
            if (da != db) return da < db ? -1 : 1;
        }
        return 0;
    }

    static int cmp(const Dec& a, const Dec& b) {
        if (a.negative != b.negative) return a.negative ? -1 : 1;
        int c = cmpAbs(a, b);
        return a.negative ? -c : c;
    }
};

// === ARITHMETIC ===

inline Dec decAdd(const Dec& a, const Dec& b);
inline Dec decSub(const Dec& a, const Dec& b);

// Add two non-negative decimals
inline Dec addAbs(const Dec& a, const Dec& b) {
    Dec result;
    int lo = 0, hi = 0;
    if (!a.isZero()) { lo = std::min(lo, a.digits.begin()->first);  hi = std::max(hi, a.digits.rbegin()->first); }
    if (!b.isZero()) { lo = std::min(lo, b.digits.begin()->first);  hi = std::max(hi, b.digits.rbegin()->first); }
    int carry = 0;
    for (int e = lo; e <= hi + 1; e++) {
        int da = a.digits.count(e) ? a.digits.at(e) : 0;
        int db = b.digits.count(e) ? b.digits.at(e) : 0;
        int sum = da + db + carry;
        carry = sum / 10;
        if (sum % 10) result.digits[e] = sum % 10;
    }
    return result;
}

// Subtract abs(b) from abs(a), assuming abs(a) >= abs(b)
inline Dec subAbs(const Dec& a, const Dec& b) {
    Dec result;
    int lo = 0, hi = 0;
    if (!a.isZero()) { lo = std::min(lo, a.digits.begin()->first);  hi = std::max(hi, a.digits.rbegin()->first); }
    if (!b.isZero()) { lo = std::min(lo, b.digits.begin()->first); }
    int borrow = 0;
    for (int e = lo; e <= hi; e++) {
        int da = a.digits.count(e) ? a.digits.at(e) : 0;
        int db = b.digits.count(e) ? b.digits.at(e) : 0;
        int diff = da - db - borrow;
        borrow = 0;
        if (diff < 0) { diff += 10; borrow = 1; }
        if (diff) result.digits[e] = diff;
    }
    return result;
}

inline Dec decAdd(const Dec& a, const Dec& b) {
    if (a.negative == b.negative) {
        Dec r = addAbs(a, b);
        r.negative = a.negative;
        return r;
    }
    int c = Dec::cmpAbs(a, b);
    if (c == 0) return Dec{};
    if (c > 0) { Dec r = subAbs(a, b); r.negative = a.negative; return r; }
    else        { Dec r = subAbs(b, a); r.negative = b.negative; return r; }
}

inline Dec decSub(const Dec& a, const Dec& b) {
    Dec nb = b; nb.negative = !b.negative;
    return decAdd(a, nb);
}

inline Dec decMul(const Dec& a, const Dec& b) {
    if (a.isZero() || b.isZero()) return Dec{};
    std::map<int, int> raw;
    for (auto& [ea, da] : a.digits)
        for (auto& [eb, db] : b.digits)
            raw[ea + eb] += da * db;
    Dec result;
    int carry = 0;
    int lo = raw.begin()->first;
    int hi = raw.rbegin()->first;
    for (int e = lo; e <= hi + 5; e++) {
        int val = (raw.count(e) ? raw[e] : 0) + carry;
        carry = val / 10;
        if (val % 10) result.digits[e] = val % 10;
        if (carry == 0 && e > hi) break;
    }
    result.negative = a.negative != b.negative;
    result.prune();
    return result;
}

inline Dec decDiv(const Dec& a, const Dec& b, int precision = 20) {
    if (b.isZero()) { std::cerr << "Preposterous: division by zero\n"; return Dec{}; }
    if (a.isZero()) return Dec{};

    // Long division: work with scaled integers
    // Scale both by enough to make b an integer
    int bMin = b.digits.begin()->first;
    int scale = (bMin < 0) ? -bMin : 0;

    // We'll do digit-by-digit long division
    Dec remainder = a; remainder.negative = false;
    Dec divisor   = b; divisor.negative   = false;

    Dec result;
    result.negative = a.negative != b.negative;

    int maxExp = a.isZero() ? 0 : a.digits.rbegin()->first;
    int startExp = maxExp;

    bool nonTerminating = false;
    int digitsEmitted = 0;
    int maxDigits = precision + (startExp >= 0 ? startExp + 1 : 0) + 5;

    for (int exp = startExp; exp >= -(precision) && digitsEmitted < maxDigits; exp--) {
        int d = 0;
        // Find how many times divisor fits into remainder at this position
        for (d = 9; d >= 0; d--) {
            Dec trial = decMul(divisor, Dec::fromInt(d));
            // Scale trial to current exponent
            Dec scaledTrial;
            for (auto& [te, td] : trial.digits) scaledTrial.digits[te + exp] = td;
            if (Dec::cmpAbs(remainder, scaledTrial) >= 0) break;
        }
        if (d > 0) {
            result.digits[exp] = d;
            Dec sub;
            Dec trial = decMul(divisor, Dec::fromInt(d));
            for (auto& [te, td] : trial.digits) sub.digits[te + exp] = td;
            remainder = decSub(remainder, sub);
            remainder.negative = false;
        }
        if (!remainder.isZero()) digitsEmitted++;
        if (digitsEmitted > precision && !remainder.isZero()) { nonTerminating = true; break; }
        if (remainder.isZero()) break;
    }

    if (nonTerminating)
        std::cerr << "Suggestion: non-terminating decimal, truncated at " << precision << " digits\n";

    result.prune();
    return result;
}

inline void decDisplay(const Dec& d) {
    std::cout << d.toString() << "\n";
}
