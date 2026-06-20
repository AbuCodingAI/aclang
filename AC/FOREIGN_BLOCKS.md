# Foreign Blocks: Raw Code Passthrough

How to embed raw native code in AC programs using `<Foreign>` blocks.

---

## What Are Foreign Blocks?

`<Foreign>` blocks allow you to **embed raw code** in AC programs that will be passed directly to the compiler without any AC-level checks.

**This is intentional escape hatch for edge cases where AC's abstractions don't fit.**

---

## Enabling Foreign Blocks

### Compile Flag

```bash
ac myprogram.ac --allow-foreign
```

Without the flag, Foreign blocks are **rejected at compile time**:

```
Error: Foreign blocks are disabled. 
Recompile with --allow-foreign to enable raw code passthrough.
```

### Why a Flag?

- Prevents accidental raw code in untrusted sources
- Makes intentional use explicit
- Aids code review ("grep for --allow-foreign in CI")
- Default is safe (no raw code unless explicitly enabled)

---

## Syntax

### Basic Foreign Block

```ac
<Foreign>
    /* Raw code here - passed directly to backend */
<Foreign>
```

### Multiline Foreign

```ac
<Foreign>
    line1
    line2
    line3
<Foreign>
```

---

## Examples by Backend

### AC→PY (Python)

```ac
AC->PY
use ilib web

<Foreign>
import requests
import json
<Foreign>

Make fetchData func()
    <Foreign>
        response = requests.get("https://api.example.com/data")
        data = response.json()
        return data
    <Foreign>
```

### AC→C (C)

```ac
AC->C
use ilib math

Make computeFFT func(array_ptr)
    <Foreign>
        fftw_plan plan = fftw_plan_dft_1d(1024, (fftw_complex*)array_ptr, ...);
        fftw_execute(plan);
        fftw_destroy_plan(plan);
    <Foreign>
```

### AC→RS (Rust)

```ac
AC->RS
use ilib web

Make parseJSON func(json_str)
    <Foreign>
        use serde_json;
        let parsed: serde_json::Value = serde_json::from_str(json_str)?;
        Ok(parsed)
    <Foreign>
```

### AC→GO (Go)

```ac
AC->GO
use ilib os

Make callHTTP func(url)
    <Foreign>
        resp, err := http.Get(url)
        if err != nil {
            return nil, err
        }
        defer resp.Body.Close()
        body, _ := io.ReadAll(resp.Body)
        return body, nil
    <Foreign>
```

### AC→JS (JavaScript)

```ac
AC->JS
use ilib web

Make fetchAPI func(endpoint)
    <Foreign>
        const response = await fetch(endpoint);
        const data = await response.json();
        return data;
    <Foreign>
```

---

## Use Cases

### 1. **Use Libraries Not in AC**

```ac
AC->PY

<Foreign>
import numpy as np
import scipy.optimize as opt
<Foreign>

Make optimize_function func(coefficients)
    <Foreign>
        result = opt.minimize(lambda x: sum(x**2), coefficients)
        return result.x.tolist()
    <Foreign>
```

### 2. **Performance-Critical Code**

```ac
AC->C

Make fast_sort func(array)
    <Foreign>
        qsort(array, length, sizeof(int), compare);
        return array;
    <Foreign>
```

### 3. **Platform-Specific Code**

```ac
AC->C

Make get_memory_info func()
    <Foreign>
        #ifdef _WIN32
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            return memInfo.ullTotalPhys;
        #else
            long pages = sysconf(_SC_PHYS_PAGES);
            long pageSize = sysconf(_SC_PAGE_SIZE);
            return pages * pageSize;
        #endif
    <Foreign>
```

### 4. **Interop with Native Libraries**

```ac
AC->GO

Make callCFunction func()
    <Foreign>
        import "C"
        
        // Call C library function
        result := C.my_c_function()
        return int64(result)
    <Foreign>
```

---

## Important: No Type Checking

Foreign code is **passed through as-is**. No AC type checking happens:

```ac
AC->PY

<Foreign>
# This is syntactically wrong Python
def broken(:
    return undefined_variable
<Foreign>

/* AC compiler: "Looks fine to me" */
/* Python interpreter at runtime: ERROR */
```

---

## Security Implications

### Foreign Blocks **BYPASS** All AC Protections

| Protection | Status in Foreign | Impact |
|-----------|-------------------|--------|
| Rate limiting | ❌ Bypassed | Can DDoS other servers |
| Memory limits | ❌ Bypassed | Can allocate unbounded |
| CPU limits | ❌ Bypassed | Can spin CPU at 100% |
| File I/O restrictions | ❌ Bypassed | Can read/write anything |
| Process restrictions | ❌ Bypassed | Can run arbitrary commands |


## Responsibility Model

```
┌─────────────────────────────────────────┐
│  AC Runtime Protections                 │
│  ✅ DDoS (rate-limited)                 │
│  ✅ Ping floods (detected)              │
│  ✅ Memory (500MB limit)                │
│  ✅ CPU (spinning detected)             │
│  ✅ Connections (100 max)               │
└─────────────────────────────────────────┘
                    ↓
         ┌──────────────────────┐
         │  AC Code             │
         │  ✅ Protected        │
         └──────────────────────┘
                    ↓
         ┌──────────────────────┐
         │  <Foreign> Code      │
         │  ❌ NO PROTECTION    │
         │  Requires trust      │
         └──────────────────────┘
```

**Usage Rule:**

```
✅ SAFE: Foreign blocks with code you trust (library wrappers, performance)
❌ UNSAFE: Foreign blocks with untrusted input (user code, external scripts)
```

---

## When to Use (and NOT Use)

### ✅ GOOD Uses

1. **Wrapping a native library**
   ```ac
   <Foreign>
       import requests
   <Foreign>
   ```

2. **Performance-critical computation**
   ```ac
   <Foreign>
       fftw_execute(plan);  // FFT, can't do in AC
   <Foreign>
   ```

3. **Platform-specific code**
   ```ac
   <Foreign>
       #ifdef _WIN32
           // Windows-only code
       #else
           // Linux code
       #endif
   <Foreign>
   ```

### ❌ BAD Uses

1. **Processing untrusted user input**
   ```ac
   /* DON'T DO THIS */
   Make process_user_code func(code)
       <Foreign>
           eval(code)  /* User code runs unprotected */
       <Foreign>
   Make
   ```

2. **Implementing AC features**
   ```ac
   /* DON'T DO THIS */
   Make do_loop func()
       <Foreign>
           while (true) { /* Bypasses loop detection */ }
       <Foreign>
   Make
   ```

3. **Avoiding AC semantics intentionally**
   ```ac
   /* DON'T DO THIS */
   <Foreign>
       # Just use Python then!
       import sys
       for x in range(1000000000):
           pass
   <Foreign>
   ```

---

## Code Review

Foreign blocks should be **explicitly audited** in code review:

```bash
# Find all Foreign blocks
grep -r "<Foreign>" .

# In CI/CD: fail if --allow-foreign was used without approval
if grep -q "allow-foreign" build.log; then
    echo "ERROR: Foreign blocks used without review"
    exit 1
fi
```

---

## Recommended Pattern

Instead of uncontrolled Foreign blocks, use **explicit wrappers**:

```ac
AC->PY
use ilib web

/* Wrapper module: carefully controls what's exposed */
Make http_get func(url)
    <Foreign>
        import requests
        # Validate URL before making request
        if not url.startswith(("http://", "https://")):
            raise ValueError("Invalid URL")
        response = requests.get(url, timeout=30)
        return response.text
    <Foreign>
Make
```

This way:
- Foreign code is **isolated** to specific functions
- **Intent is clear** (this function uses native library)
- **Easy to audit** (single file grep finds it)
- **Easy to test** (wrapper has contract)

---

## Error Messages

### Foreign blocks disabled

```
Error: Foreign blocks are disabled. 
Recompile with --allow-foreign to enable raw code passthrough.
```

**Fix:** Add `--allow-foreign` flag

```bash
ac program.ac --allow-foreign
```

### Syntax errors in Foreign code

```
Python Error: SyntaxError: invalid syntax
```

**Your problem now.** AC passed the code through; the backend compiler/interpreter caught it.

---

## Summary

| Aspect | Details |
|--------|---------|
| **Syntax** | `<Foreign>` ... `<Foreign>` |
| **Enable** | `ac file.ac --allow-foreign` |
| **Default** | Disabled (safe) |
| **Type checking** | None (raw code) |
| **Protections** | All bypassed |
| **Use** | Trusted code only |
| **Best practice** | Wrap in functions, audit, test |

**Foreign blocks are intentional escape hatches. Use responsibly.** 🔒

If you're using Foreign blocks for everyday logic, **you're using the wrong language.**
