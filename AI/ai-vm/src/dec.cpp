#include "dec_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

// ── Internal allocator ─────────────────────────────────────────────────────

static DecRef alloc_dec(bool negative, DecValue digits) {
    auto* o = new DecObject();
    o->negative = negative;
    o->digits   = std::move(digits);
    for (auto it = o->digits.begin(); it != o->digits.end(); )
        it->second == 0 ? it = o->digits.erase(it) : ++it;
    return o;
}

// ── Parsing ────────────────────────────────────────────────────────────────

DecRef dec_from_literal(const char* s) {
    std::string str(s ? s : "0");
    bool negative = false;
    if (!str.empty() && str[0] == '-') { negative = true; str = str.substr(1); }

    size_t dot = str.find('.');
    std::string ip = (dot == std::string::npos) ? str        : str.substr(0, dot);
    std::string fp = (dot == std::string::npos) ? std::string{} : str.substr(dot + 1);

    DecValue digits;
    // integer part: rightmost digit is at exponent 0
    for (int i = (int)ip.size() - 1, exp = 0; i >= 0; i--, exp++) {
        int d = ip[i] - '0';
        if (d) digits[exp] = d;
    }
    // fractional part: first digit after dot is at exponent -1
    for (int i = 0; i < (int)fp.size(); i++) {
        int d = fp[i] - '0';
        if (d) digits[-(i + 1)] = d;
    }
    return alloc_dec(negative, std::move(digits));
}

DecRef dec_from_int(int64_t n) {
    bool negative = (n < 0);
    if (negative) n = -n;
    DecValue digits;
    if (n == 0) return alloc_dec(false, {});
    for (int exp = 0; n > 0; n /= 10, exp++) {
        int d = (int)(n % 10);
        if (d) digits[exp] = d;
    }
    return alloc_dec(negative, std::move(digits));
}

// ── Output ─────────────────────────────────────────────────────────────────

const char* dec_to_cstr(DecRef r, char* buf, int bufsz) {
    if (!r || bufsz <= 0) { if (buf && bufsz > 0) buf[0] = 0; return buf; }
    const DecObject* o = deref(r);
    if (o->digits.empty()) {
        snprintf(buf, (size_t)bufsz, "0");
        return buf;
    }
    int minExp = o->digits.begin()->first;
    int maxExp = o->digits.rbegin()->first;

    std::string result;
    if (o->negative) result += '-';
    int intTop = std::max(maxExp, 0);
    for (int e = intTop; e >= 0; e--)
        result += (char)('0' + (o->digits.count(e) ? o->digits.at(e) : 0));
    if (minExp < 0) {
        result += '.';
        for (int e = -1; e >= minExp; e--)
            result += (char)('0' + (o->digits.count(e) ? o->digits.at(e) : 0));
    }
    snprintf(buf, (size_t)bufsz, "%s", result.c_str());
    return buf;
}

void dec_display(DecRef r) {
    char buf[1024];
    dec_to_cstr(r, buf, sizeof(buf));
    printf("%s\n", buf);
}

// ── Predicates ─────────────────────────────────────────────────────────────

int dec_is_zero(DecRef r) {
    return !r || deref(r)->digits.empty();
}

// ── Comparison ─────────────────────────────────────────────────────────────

static int cmp_abs(const DecObject* a, const DecObject* b) {
    int aMax = a->digits.empty() ? 0 : a->digits.rbegin()->first;
    int bMax = b->digits.empty() ? 0 : b->digits.rbegin()->first;
    int aMin = a->digits.empty() ? 0 : a->digits.begin()->first;
    int bMin = b->digits.empty() ? 0 : b->digits.begin()->first;
    int hi = std::max(aMax, bMax), lo = std::min(aMin, bMin);
    for (int e = hi; e >= lo; e--) {
        int da = a->digits.count(e) ? a->digits.at(e) : 0;
        int db = b->digits.count(e) ? b->digits.at(e) : 0;
        if (da != db) return da < db ? -1 : 1;
    }
    return 0;
}

int dec_cmp(DecRef ra, DecRef rb) {
    const DecObject* a = ra ? deref(ra) : nullptr;
    const DecObject* b = rb ? deref(rb) : nullptr;
    bool aNeg = a && a->negative;
    bool bNeg = b && b->negative;
    bool aZero = !a || a->digits.empty();
    bool bZero = !b || b->digits.empty();
    if (aZero && bZero) return 0;
    if (aZero) return bNeg ? 1 : -1;
    if (bZero) return aNeg ? -1 : 1;
    if (aNeg != bNeg) return aNeg ? -1 : 1;
    int c = cmp_abs(a, b);
    return aNeg ? -c : c;
}

// ── Arithmetic ─────────────────────────────────────────────────────────────

// Add absolute values (treat both as positive)
static DecValue add_abs(const DecObject* a, const DecObject* b) {
    DecValue result;
    int lo = 0, hi = 0;
    if (!a->digits.empty()) { lo = std::min(lo, a->digits.begin()->first);  hi = std::max(hi, a->digits.rbegin()->first); }
    if (!b->digits.empty()) { lo = std::min(lo, b->digits.begin()->first);  hi = std::max(hi, b->digits.rbegin()->first); }
    int carry = 0;
    for (int e = lo; e <= hi + 1; e++) {
        int da = a->digits.count(e) ? a->digits.at(e) : 0;
        int db = b->digits.count(e) ? b->digits.at(e) : 0;
        int sum = da + db + carry;
        carry = sum / 10;
        if (sum % 10) result[e] = sum % 10;
    }
    return result;
}

// Subtract abs(b) from abs(a), assuming abs(a) >= abs(b)
static DecValue sub_abs(const DecObject* a, const DecObject* b) {
    DecValue result;
    int lo = 0, hi = 0;
    if (!a->digits.empty()) { lo = std::min(lo, a->digits.begin()->first);  hi = std::max(hi, a->digits.rbegin()->first); }
    if (!b->digits.empty())   lo = std::min(lo, b->digits.begin()->first);
    int borrow = 0;
    for (int e = lo; e <= hi; e++) {
        int da = a->digits.count(e) ? a->digits.at(e) : 0;
        int db = b->digits.count(e) ? b->digits.at(e) : 0;
        int diff = da - db - borrow;
        borrow = 0;
        if (diff < 0) { diff += 10; borrow = 1; }
        if (diff) result[e] = diff;
    }
    return result;
}

DecRef dec_add(DecRef ra, DecRef rb) {
    static const DecObject zero{};
    const DecObject* a = ra ? deref(ra) : &zero;
    const DecObject* b = rb ? deref(rb) : &zero;

    if (a->negative == b->negative) {
        return alloc_dec(a->negative, add_abs(a, b));
    }
    int c = cmp_abs(a, b);
    if (c == 0) return alloc_dec(false, {});
    if (c > 0) return alloc_dec(a->negative, sub_abs(a, b));
    return          alloc_dec(b->negative, sub_abs(b, a));
}

DecRef dec_sub(DecRef ra, DecRef rb) {
    if (!rb) return dec_retain(ra);
    // sub(a,b) = add(a, -b): flip b's sign
    DecObject nb = *deref(rb);
    nb.negative = !nb.negative;
    DecRef tmpB = alloc_dec(nb.negative, nb.digits);
    DecRef result = dec_add(ra, tmpB);
    dec_release(tmpB);
    return result;
}

DecRef dec_mul(DecRef ra, DecRef rb) {
    static const DecObject zero{};
    const DecObject* a = ra ? deref(ra) : &zero;
    const DecObject* b = rb ? deref(rb) : &zero;
    if (a->digits.empty() || b->digits.empty()) return alloc_dec(false, {});

    std::map<int, int> raw;
    for (auto& [ea, da] : a->digits)
        for (auto& [eb, db] : b->digits)
            raw[ea + eb] += da * db;

    DecValue result;
    int carry = 0;
    int lo = raw.begin()->first, hi = raw.rbegin()->first;
    for (int e = lo; e <= hi + 5; e++) {
        int val = (raw.count(e) ? raw.at(e) : 0) + carry;
        carry = val / 10;
        if (val % 10) result[e] = val % 10;
        if (carry == 0 && e > hi) break;
    }
    return alloc_dec(a->negative != b->negative, std::move(result));
}

DecRef dec_div(DecRef ra, DecRef rb, int precision) {
    static const DecObject zero{};
    const DecObject* a = ra ? deref(ra) : &zero;
    const DecObject* b = rb ? deref(rb) : &zero;

    if (b->digits.empty()) {
        fprintf(stderr, "Preposterous: division by zero\n");
        return alloc_dec(false, {});
    }
    if (a->digits.empty()) return alloc_dec(false, {});

    // Long division digit by digit
    // Work with positive remainders; track sign separately
    DecRef rem = alloc_dec(false, a->digits);   // positive copy of a
    DecRef div = alloc_dec(false, b->digits);   // positive copy of b

    DecValue result_digits;
    bool result_neg = a->negative != b->negative;

    int startExp = a->digits.rbegin()->first;
    int dEmit = 0;
    int maxD = precision + (startExp >= 0 ? startExp + 1 : 0) + 5;
    bool nonTerminating = false;

    for (int exp = startExp; exp >= -precision && dEmit < maxD; exp--) {
        int d = 0;
        for (d = 9; d >= 0; d--) {
            // trial = div * d, shifted to current exponent
            DecRef trial = dec_mul(div, (DecRef)nullptr);  // 0
            dec_release(trial);
            // Build scaled trial: div_digits shifted by exp
            DecValue scaled;
            for (auto& [te, td] : deref(div)->digits) scaled[te + exp] = td;
            DecRef scaledD = alloc_dec(false, std::move(scaled));
            DecRef trialD  = dec_mul(div, dec_from_int(d));
            // Shift trialD by exp
            DecValue shiftedDigits;
            for (auto& [te, td] : deref(trialD)->digits) shiftedDigits[te + exp] = td;
            dec_release(trialD);
            DecRef shiftedRef = alloc_dec(false, std::move(shiftedDigits));
            int cmp = cmp_abs(deref(rem), deref(shiftedRef));
            dec_release(shiftedRef);
            dec_release(scaledD);
            if (cmp >= 0) break;
        }
        if (d > 0) {
            result_digits[exp] = d;
            // rem -= div * d, shifted
            DecRef trialD = dec_mul(div, dec_from_int(d));
            DecValue subDigits;
            for (auto& [te, td] : deref(trialD)->digits) subDigits[te + exp] = td;
            dec_release(trialD);
            DecRef subRef = alloc_dec(false, std::move(subDigits));
            DecRef newRem = dec_sub(rem, subRef);
            dec_release(subRef);
            dec_release(rem);
            rem = newRem;
            // ensure rem is positive
            if (rem && deref(rem)->negative) deref(rem)->negative = false;
        }
        if (!dec_is_zero(rem)) dEmit++;
        if (dEmit > precision && !dec_is_zero(rem)) { nonTerminating = true; break; }
        if (dec_is_zero(rem)) break;
    }
    dec_release(rem);
    dec_release(div);

    if (nonTerminating)
        fprintf(stderr, "Suggestion: non-terminating decimal, truncated at %d digits\n", precision);

    return alloc_dec(result_neg, std::move(result_digits));
}
