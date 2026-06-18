# Precision Types Guide: LongInt, GoodDec, and isclose()

Complete guide to AC's precision arithmetic types.

---

## 1. **math.LongInt** — 96-bit Integer (Perfect for Games!)

### Why 96-bit? (3 × 32-bit Components)

Game development uses 3 components constantly:
- **RGB color:** Red (32-bit) + Green (32-bit) + Blue (32-bit) = 96-bit
- **3D coordinates:** X (32-bit) + Y (32-bit) + Z (32-bit) = 96-bit
- **Game object ID:** Layer (32-bit) + Type (32-bit) + Unique (32-bit) = 96-bit

### Implementation

```cpp
class LongInt {
    int32_t high;  // bits 64-95 (most significant)
    int32_t mid;   // bits 32-63 (middle)
    int32_t low;   // bits 0-31  (least significant)
};
```

### Syntax in AC

```ac
AC->BNY
use ilib math

<mainloop>
    /* Create LongInt with actual numbers (not strings) */
    x = math.LongInt 123456789
    y = math.LongInt 987654321

    /* Arithmetic */
    sum = x + y              /* Add two LongInts */
    diff = x - y             /* Subtract */
    prod = x * y             /* Multiply */
    quot = x / y             /* Divide */

    /* Comparisons */
    IF x is y
        Term.display $Equal$
    IF x < y
        Term.display $x is less$

    /* Access components */
    high = x.high            /* Get high 32-bits */
    mid = x.mid              /* Get middle 32-bits */
    low = x.low              /* Get low 32-bits */

    /* RGB Color Example */
    red = math.LongInt 255
    green = math.LongInt 128
    blue = math.LongInt 64
    color = (red << 16) | (green << 8) | blue

    /end
<mainloop>
```

### Range

- **Min:** -(2^95 - 1) = -39,614,081,257,132,168,796,771,975,166,111,735,807
- **Max:** +(2^95 - 1) = +39,614,081,257,132,168,796,771,975,166,111,735,807
- **Compared to int64:** 2^95 ≈ 40 million times larger

---

## 2. **math.GoodDec** — Arbitrary-Precision Decimal

### What It Is

Like Java's `BigDecimal` — stores value as `unscaled * 10^scale`.

Example: `GoodDec("123", -2)` = 1.23 (exact!)

### Why Use It

- ✅ **EXACT** decimal arithmetic (no rounding)
- ✅ Perfect for finance, commerce, accounting
- ✅ No IEEE 754 floating point errors
- ❌ Slower than native doubles
- ❌ Not for scientific computing

### Syntax in AC

```ac
AC->BNY
use ilib math

<mainloop>
    /* Create GoodDec */
    price = math.GoodDec("19.99", 0)
    tax = math.GoodDec("0.08", 0)

    /* Arithmetic (EXACT!) */
    total = price * tax      /* Exact decimal multiplication */
    discount = price - total /* Exact decimal subtraction */

    /* Comparisons */
    IF price is tax
        Term.display $Same price$
    IF price < discount
        Term.display $Price lower$

    /* Conversions */
    as_double = price.to_double()      /* Convert to IEEE 754 double */
    from_double = math.GoodDec.from_double(3.14)

    /* Finance Example: No rounding errors! */
    account = math.GoodDec("1000.00", 0)
    FOR i in range(100)
        account = account * math.GoodDec("1.01", 0)  /* 1% interest */
    
    Term.display $Final: $ + account.str()  /* Exact! */

    /end
<mainloop>
```

### Operations (NEW!)

```ac
a + b        /* Add two GoodDecs (exact) */
a - b        /* Subtract (exact) */
a * b        /* Multiply (exact) */
a / b        /* Divide (exact, with rounding rules) */
a is b       /* Compare equality (exact) */
a < b        /* Less than */
a <= b       /* Less or equal */
a > b        /* Greater than */
a >= b       /* Greater or equal */
```

---

## 3. **math.isclose()** — Safe Float Comparison

### What It's For

**THE SAFE WAY to compare floating point numbers.**

### The Problem It Solves

```ac
x = 0.1 + 0.2
IF x is 0.3
    Term.display $Equal$  /* ❌ NEVER RUNS! x = 0.30000000000000004 */
```

Why? IEEE 754 can't exactly represent 0.1 or 0.2, so rounding errors accumulate.

### The Solution

```ac
AC->BNY
use ilib math

x = 0.1 + 0.2

/* WRONG: */
IF x is 0.3
    Term.display $Equal$  /* ❌ Never runs */

/* RIGHT: */
IF math.isclose(x, 0.3)
    Term.display $Approximately equal$  /* ✅ Runs! */
```

### How isclose() Works

```
Returns: true if |a - b| <= max(rel_tol * max(|a|, |b|), abs_tol)

Default tolerances:
  rel_tol = 1e-9  (relative: 0.000000001% difference allowed)
  abs_tol = 1e-99 (absolute: for tiny numbers near zero)
```

**Example:**
```ac
math.isclose(0.30000000000000004, 0.3)      /* ✅ true */
math.isclose(1.0000000001, 1.0)             /* ✅ true */
math.isclose(0.0, 1e-10)                    /* ✅ true (within abs_tol) */
math.isclose(100.1, 100.2)                  /* ❌ false (too different) */
```

### Common Use Cases

#### **Case 1: Loop Until Near Zero**

```ac
/* WRONG: infinite loop */
x = 0.5
WHILST x #= 0.0
    x -= 0.1  /* 0.1 can't be represented exactly */

/* RIGHT: terminates safely */
x = 0.5
WHILST NOT math.isclose(x, 0.0)
    x -= 0.1
```

#### **Case 2: Physics Simulations**

```ac
/* Check if velocity is essentially stopped */
velocity = 0.00000000001
IF math.isclose(velocity, 0.0)
    object.stop()
```

#### **Case 3: Financial Comparisons**

```ac
/* Compare floating point money values */
balance1 = 100.10
balance2 = 100.099999999
IF math.isclose(balance1, balance2)
    Term.display $Amounts match$
```

#### **Case 4: Custom Tolerance**

```ac
/* Allow 1% difference instead of default */
target = 100.0
actual = 100.5
IF math.isclose(target, actual, 0.01, 0.0)  /* 1% relative tolerance */
    Term.display $Within 1% acceptable$
```

---

## Quick Reference

| Type | Use Case | Range | Operations | Notes |
|------|----------|-------|-----------|-------|
| **int64** | Default integers | ±9.2×10^18 | ✅ All | Standard, fast |
| **math.LongInt** | Games, 96-bit needs | ±3.96×10^28 | ✅ All new | 3×32-bit components |
| **double** | General math | ±1.8×10^308 | ✅ All | IEEE 754 rounding |
| **math.GoodDec** | Finance, exact | Unlimited | ✅ All new | EXACT decimal arithmetic |
| **math.isclose()** | Safe float compare | Any | ✅ For doubles | Epsilon-tolerant |

---

## When to Use Which

### Use **math.LongInt** when:
- ✅ You need 96-bit integers
- ✅ Game dev (colors, coordinates, IDs)
- ✅ Working with 3 components

### Use **math.GoodDec** when:
- ✅ Exact decimal arithmetic needed
- ✅ Finance, commerce, accounting
- ✅ Can't afford rounding errors
- ❌ Science or graphics (use double)

### Use **math.isclose()** when:
- ✅ Comparing floating point numbers
- ✅ Loop conditions with floats
- ✅ Physics simulations
- ✅ Testing float values

### Use regular **double** when:
- ✅ Science, physics, graphics
- ✅ Fast computation
- ✅ Don't need exact decimals
- ✅ Using isclose() for comparisons

---

## Examples

### RGB Color (LongInt)

```ac
red = math.LongInt 255
green = math.LongInt 128
blue = math.LongInt 64
rgb_color = (red << 16) | (green << 8) | blue
Term.display rgb_color  /* Single 96-bit color value */
```

### Bank Account (GoodDec)

```ac
balance = math.GoodDec("1000.50", 0)
deposit = math.GoodDec("50.25", 0)
new_balance = balance + deposit  /* Exact: 1050.75 */
Term.display new_balance.str()
```

### Safe Loop (isclose)

```ac
x = 0.5
count = 0
WHILST NOT math.isclose(x, 0.0)
    x -= 0.1
    count += 1
    IF count > 1000
        /kill

Term.display $Loop terminated at x = $ + x  /* Safe! */
```

---

## Summary

- **math.LongInt**: 96-bit signed integers (game dev oriented)
- **math.GoodDec**: Exact arbitrary-precision decimals (finance oriented)
- **math.isclose()**: Safe floating point comparison (prevents IEEE 754 traps)

All three are now fully operational in AC! 🎉
