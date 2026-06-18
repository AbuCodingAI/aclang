# Bitwise Operators in AC

Complete guide to AC's bitwise operations.

---

## AC's Bitwise Operators

| Operator | Name | What It Does | Example |
|----------|------|------|---------|
| `&` or `band` | Bitwise AND | Both bits must be 1 | `12 & 10 = 8` |
| `\|` | Bitwise XOR | Bits must be different | `12 \| 10 = 6` |
| `bor` | Bitwise OR | At least one bit is 1 | `12 bor 10 = 14` |
| `~` | Bitwise NOT | Flip all bits | `~5 = -6` |
| `<<` | LEFT SHIFT | Shift bits left | ⏳ Not yet |
| `>>` | RIGHT SHIFT | Shift bits right | ⏳ Not yet |

---

## 1. BITWISE AND (`&` or `band`)

**What it does:** Result has 1 only where BOTH inputs have 1.

```
    12 = 1100
&   10 = 1010
──────────────
     8 = 1000
```

**Example in AC:**
```ac
a = 12
b = 10
c = a & b       /* = 8 (binary: 1000) */
d = a band b    /* Same as above */
```

**Use Case: Mask bits**
```ac
value = 12345
lower_8_bits = value & 0xFF     /* Keep only bits 0-7 */
```

---

## 2. BITWISE XOR (`|`)

**What it does:** Result has 1 where bits are DIFFERENT.

```
    12 = 1100
|   10 = 1010
──────────────
     6 = 0110
```

**Example in AC:**
```ac
a = 12
b = 10
c = a | b       /* = 6 (binary: 0110) */
```

**Use Case: Toggle bits / Encryption**
```ac
message = 42
key = 15
encrypted = message | key      /* XOR encryption */
decrypted = encrypted | key    /* XOR again to decrypt */
```

---

## 3. BITWISE OR (`bor`)

**What it does:** Result has 1 where AT LEAST ONE input has 1.

```
    12 = 1100
bor 10 = 1010
──────────────
    14 = 1110
```

**Example in AC:**
```ac
a = 12
b = 10
c = a bor b     /* = 14 (binary: 1110) */
```

**Use Case: Set specific bits**
```ac
value = 12      /* binary: 1100 */
bits_to_set = 3 /* binary: 0011 */
value = value bor bits_to_set  /* = 15 (binary: 1111) */
```

---

## 4. BITWISE NOT (`~`)

**What it does:** Flip every bit (1→0, 0→1).

```
~5:
5  = 00000101
~5 = -6 (in two's complement)
```

**Example in AC:**
```ac
x = 5
y = ~x          /* = -6 */
```

**Use Case: Create inverted masks**
```ac
all_ones = ~0           /* All bits set to 1 */
keep_upper_bits = ~0xFF /* Everything except lower 8 bits */
```

---

## Practical Examples

### **Test if Bit is Set**
```ac
value = 12          /* binary: 1100 */
bit_to_test = 4     /* binary: 0100 */

IF (value & bit_to_test) != 0
    Term.display "Bit is SET"
ELSE
    Term.display "Bit is NOT set"
```

### **Set Specific Bits**
```ac
value = 12      /* binary: 1100 */
bits_to_set = 3 /* binary: 0011 */
value = value bor bits_to_set  /* = 15 (binary: 1111) */
```

### **Clear Specific Bits**
```ac
value = 12          /* binary: 1100 */
bits_to_clear = 3   /* binary: 0011 */
value = value & ~bits_to_clear  /* Clear those bits */
```

### **Simple Encryption**
```ac
message = 42
key = 15

encrypted = message | key       /* XOR encrypt */
decrypted = encrypted | key     /* XOR decrypt (get original) */

Term.display decrypted  /* = 42 */
```

---

## RGB Color Example (Current Limitations)

Without shift operators (`<<`, `>>`), packing RGB is verbose:

```ac
/* Pack RGB using multiplication instead of shift */
red = 255
green = 128
blue = 64

color = (red * 65536) bor (green * 256) bor blue
        /* red * 2^16 */  /* green * 2^8 */

/* Unpack RGB using division instead of shift */
extracted_red = (color / 65536) & 0xFF
extracted_green = (color / 256) & 0xFF
extracted_blue = color & 0xFF
```

---

## Future Enhancements

When shift operators are added to AC:

```ac
/* Will be available in future AC: */
color = (red << 16) bor (green << 8) bor blue
extracted_red = (color >> 16) & 0xFF
extracted_green = (color >> 8) & 0xFF
```

---

## Summary

AC's bitwise operators:
- ✅ `&` / `band` — Bitwise AND
- ✅ `|` — Bitwise XOR
- ✅ `bor` — Bitwise OR
- ✅ `~` — Bitwise NOT
- ⏳ `<<` — LEFT SHIFT (future)
- ⏳ `>>` — RIGHT SHIFT (future)

Use these for: bit manipulation, encryption, flag handling, color packing (with workarounds), and low-level data handling. 🎯
