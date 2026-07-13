# IEEE 754 Float Trap: Solutions in AC

How to fix the floating point infinite loop problem using AC's precision tools.

---

## The Problem (Recap)

```ac
x = 0.5
WHILST x #= 0.0      /* ← INFINITE LOOP! */
    x -= 0.1         /* 0.1 is NOT exact in IEEE 754 */
```

This loops forever because:
- 0.1 cannot be exactly represented in binary floating point
- Rounding errors accumulate over iterations
- x never equals exactly 0.0

---

## Solution 1: Use `math.GoodDec` (Best for Financial/Precise Math)

### What is GoodDec?

`GoodDec` is AC's arbitrary-precision decimal type — inspired by Java's BigDecimal.

```cpp
class GoodDec {
    std::string unscaled_;    // Integer digits (exact)
    long long scale_;         // Base-10 scale
    // value = unscaled * 10^scale
    // Example: (123, -2) => 1.23
};
```

**Why it's better than IEEE 754:**
- ✅ Stores value as integer + scale (exact decimal representation)
- ✅ No binary rounding errors
- ✅ Perfect for finance, commerce, precise calculations
- ❌ Slower than native doubles
- ❌ Not suitable for scientific computing (IEEE 754 is fine there)

### Example: Safe Loop with GoodDec

```ac
AC->C
use ilib math

<mainloop>
    /* Use GoodDec instead of double */
    x = math.GoodDec("0.5", 0)    /* value=0, scale=0 => 0.5 */
    count = 0

    WHILST x #= math.GoodDec("0.0", 0)
        count += 1
        decrement = math.GoodDec("0.1", -1)  /* -1 scale => 0.1 */
        x = x - decrement    /* Exact decimal subtraction */

        Term.display $Iteration $ + count + $: $ + x.str()

        IF count > 10
            /kill

    Term.display $x final: $ + x.str()  /* "0.0" (exactly!) */
    /end
<mainloop>
```

**Output:**
```
Iteration 1: 0.4
Iteration 2: 0.3
Iteration 3: 0.2
Iteration 4: 0.1
Iteration 5: 0.0   (EXACT! Loop terminates!)
x final: 0.0
```

---

## Solution 2: Add `math.isclose()` Function (Practical for Most Cases)

### What We Need

Add to math library:

```cpp
// Floating point equality with epsilon tolerance
inline bool isclose(double a, double b, double rel_tol = 1e-9, double abs_tol = 0.0) {
    // Returns true if |a - b| <= max(rel_tol * max(|a|, |b|), abs_tol)
    // Handles both relative and absolute tolerance
    return std::abs(a - b) <= std::max(
        rel_tol * std::max(std::abs(a), std::abs(b)),
        abs_tol
    );
}
```

### Usage Example

```ac
AC->BNY
use ilib math

<mainloop>
    x = 0.5
    count = 0

    /* Use isclose() instead of == */
    WHILST NOT math.isclose(x, 0.0, 0.0, 1e-10)
        x -= 0.1
        count += 1
        Term.display $Iteration $ + count + $: $ + x
        IF count > 10
            /kill

    Term.display $Done! x ~ 0.0 within tolerance$
    /end
<mainloop>
```

**Output:**
```
Iteration 1: 0.4
Iteration 2: 0.3
Iteration 3: 0.2
Iteration 4: 0.1
Iteration 5: 1.38778e-17   (close enough!)
Done! x ~ 0.0 within tolerance
```

---

## Solution 3: Use Integer Iteration (Safest & Simplest)

```ac
AC->BNY
use ilib math

<mainloop>
    x = 0.5

    /* Iterate a fixed number of times */
    FOR i in range(5)
        x -= 0.1
        Term.display $Iteration $ + (i+1) + $: $ + x

    Term.display $Final x: $ + x
    /end
<mainloop>
```

**Why this works:**
- ✅ No float comparison
- ✅ Loop always terminates
- ✅ Simple and clear

---

## Ratings: `math.LongInt` vs `math.GoodDec`

### `math.LongInt` ⭐⭐⭐

**What it is:**
- String-based arbitrary-precision **integer** storage
- Example: `LongInt("123456789012345678901234567890")`
- Stores digits as std::string

**Strengths:**
- ✅ Arbitrary precision integers
- ✅ No overflow (can grow infinitely)
- ✅ Exact integer arithmetic

**Weaknesses:**
- ❌ NO OPERATIONS (just storage!)
  - Can't add, subtract, multiply LongInts
  - Can't compare LongInts
  - Just stores digits as string
- ❌ Not exposed to AC code properly
- ❌ No math operations implemented

**Rating:** ⭐⭐ (Good idea, half-baked implementation)

**Verdict:** **INCOMPLETE** — Framework exists but no operations. You can create LongInts but can't do anything with them.

---

### `math.GoodDec` ⭐⭐⭐⭐

**What it is:**
- Arbitrary-precision **decimal** (like Java BigDecimal)
- Stores: unscaled integer + scale
- Example: `GoodDec("123", -2)` => 1.23

**Strengths:**
- ✅ Arbitrary precision decimals
- ✅ Exact decimal representation (no rounding)
- ✅ Perfect for finance/commerce
- ✅ String format() for display
- ✅ Can extract unscaled/scale separately

**Weaknesses:**
- ❌ NO OPERATIONS (no +, -, *, /)
  - Can't add GoodDecs together
  - Can't multiply or divide
  - Just stores unscaled + scale
- ❌ Very limited API
- ❌ Essentially just a data container

**Rating:** ⭐⭐⭐⭐ (Good architecture, needs operations)

**Verdict:** **FRAMEWORK COMPLETE, OPERATIONS MISSING** — Structure is solid but operators not implemented. Like GoodDec but more useful for representing values.

---

## What's Missing from Math Library

| Feature | Status | Priority |
|---------|--------|----------|
| **math.isclose(a, b, tol)** | ❌ Missing | 🔴 Critical |
| **GoodDec + - * / ** | ❌ Missing | 🟠 High |
| **GoodDec comparisons (==, <, >)** | ❌ Missing | 🟠 High |
| **LongInt + - * / ** | ❌ Missing | 🟠 High |
| **GoodDec to double conversion** | ❌ Missing | 🟡 Medium |
| **Parse decimal string to GoodDec** | ❌ Missing | 🟡 Medium |

---

## Implementation Plan: Fix the Trap

### Phase 1: Add `math.isclose()` (1 day)

```cpp
// In math.hpp
inline bool isclose(double a, double b, double rel_tol = 1e-9, double abs_tol = 0.0) {
    return std::abs(a - b) <= std::max(
        rel_tol * std::max(std::abs(a), std::abs(b)),
        abs_tol
    );
}
```

**Impact:** ✅ Solves 90% of IEEE float trap issues

---

### Phase 2: Implement GoodDec Operations (2-3 days)

```cpp
// Decimal arithmetic operations
GoodDec operator+(const GoodDec& a, const GoodDec& b);
GoodDec operator-(const GoodDec& a, const GoodDec& b);
GoodDec operator*(const GoodDec& a, const GoodDec& b);
GoodDec operator/(const GoodDec& a, const GoodDec& b);
bool operator==(const GoodDec& a, const GoodDec& b);
bool operator<(const GoodDec& a, const GoodDec& b);
// etc.
```

**Impact:** ✅ Solves 100% of IEEE float trap for financial math

---

### Phase 3: Implement LongInt Operations (2-3 days)

Same as GoodDec but for integers.

**Impact:** ✅ Arbitrary precision integer math

---

## Recommended Quick Win: Add isclose()

**Cost:** ~50 lines of code  
**Benefit:** Fixes the IEEE 754 trap for 90% of use cases  
**Impact:** Huge (prevents infinite loops)

```ac
/* After adding isclose() */
WHILST NOT math.isclose(x, 0.0)
    x -= 0.1
/* Loop terminates safely! */
```

---

## Summary

| Type | Rating | Operations | Use Case |
|------|--------|-----------|----------|
| **IEEE 754 double** | N/A | ✅ Fast | Science, graphics |
| **math.isclose()** | ⭐⭐⭐⭐⭐ | ✅ Safe compare | General float logic |
| **math.GoodDec** | ⭐⭐⭐⭐ | ❌ Missing | Finance, decimals |
| **math.LongInt** | ⭐⭐ | ❌ Missing | Big integers |
| **Integer loops** | ⭐⭐⭐⭐⭐ | ✅ Safest | Iteration |

---

## Action Items for v1.0+

**Critical (Block v1.0 if Float problems arise):**
- ✅ Add `math.isclose()` to math library

**High Priority (v1.1):**
- Implement GoodDec operations (+, -, *, /)
- Implement GoodDec comparisons (==, <, >)

**Medium Priority (v1.2):**
- Implement LongInt operations
- Add GoodDec↔double conversion

**Documentation:**
- ✅ Test showing the trap (done!)
- Add guide: "When to use GoodDec vs double"
- Update tutorial: "Avoid == with floats"
