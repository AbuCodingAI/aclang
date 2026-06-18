# AC Language: Feature Completeness Matrix

## ✅ Backend-Specific Features (Fully Implemented)

### JS/HTML Backend Features
✅ **Browser Functions:**
- `alert $msg$` → `window.alert()`
- `sure $msg$` → `window.confirm()` (returns boolean)
- `print_page` → `window.print()`

✅ **HTML Display Types:**
- `bold` → `<b>text</b>`
- `italic` → `<i>text</i>`
- `header` → `<h2>text</h2>`
- `link` → `<a href="...">text</a>`
- `code` → `<code>text</code>`
- `para` → `<p>text</p>`
- `underline` → `<u>text</u>`
- `mark` → `<mark>text</mark>`
- `hr` → `<hr>` (horizontal rule)
- `title` → sets document title

✅ **JavaScript Interop:**
- Foreign blocks for raw JS
- DOM manipulation via `_printHTML()`
- Event handling ready for implementation

### Go Backend Features
⏳ **Goroutines:** Infrastructure exists, not yet tested
⏳ **Channels:** Framework in place, incomplete

### Other Backends
- **Python**: Full standard library access
- **C/C++**: Full C/C++ ecosystem access
- **Rust/Java/V**: Respective language features available

---

## Why AC Doesn't Need C++ Templates

```ac
// AC is dynamically typed, so ONE function works for ALL types:

Make add func(a, b)
    return a + b

add(5, 3)              // → 8 (numbers)
add($Hello, $ $)       // → "Hello " (strings)  
add([1,2], [3,4])      // → [1,2,3,4] (lists)

// No need for C++ style:  template<T> T add(T a, T b) { return a+b; }
```

**Template comparison:**

| Language | Type System | Templates Needed? | AC's Approach |
|----------|------------|-------------------|---------------|
| **C++** | Static | ✅ Yes | N/A (not applicable) |
| **Java** | Static | ✅ Yes | N/A (not applicable) |
| **Rust** | Static | ✅ Yes | N/A (not applicable) |
| **Go** | Static | ❌ No (interfaces) | N/A (not applicable) |
| **Python** | Dynamic | ❌ No | ✅ Single func for all types |
| **JavaScript** | Dynamic | ❌ No | ✅ Single func for all types |
| **AC** | Dynamic | ❌ No | ✅ Single func for all types |

---

## 🍷 Wine Testing: Windows PE Binaries on Linux

### Current State ✅
- Wine 9.0 installed
- AC->BNY has full PE writer implementation
- **Cannot currently cross-compile** (only targets native platform)

### How It Will Work

```bash
# On Windows (when compiled there):
ac program.ac  # → program.exe (PE format)

# Copy to Linux:
scp user@windows:program.exe .

# Test with Wine:
wine program.exe
# Output: [AC program runs via Wine]
```

### To Enable Cross-Compilation

Add `--target-os` flag to AC:

```bash
# Generate Windows PE on Linux (future feature)
ac --target-os windows program.ac  # → program.exe (PE)

# Test immediately:
wine program.exe
```

### Implementation Requirements

1. **Add flag** to force output format (Windows PE vs Linux ELF)
2. **Modify exp_bny.cpp** to select writePEDynamic() vs writeELFDynamic()
3. **Adjust linking** for Windows API vs Linux syscalls
4. **Update runtime** for Windows libc (if needed)

### Current Test ✅

```
$ ./test_win_hello.acb
Hello from AC on Windows
42
```

(Runs natively on Linux as ELF64)

---

## Summary: AC Feature Matrix

| Category | Implementation | Status |
|----------|---|---|
| **Logical Operators** | and, or, xor, not, # | ✅ Complete |
| **Bitwise Operators** | &, \|, bor, ~ | ✅ Complete |
| **Smart Folding** | compfold keyword | ✅ Complete |
| **Pointers** | ptr(), deref() | ✅ Framework |
| **JS/HTML Features** | Browser functions, display types | ✅ Complete |
| **Math Library** | Transcendental, statistics, constants | ✅ Complete |
| **Concurrency** | spawn, wait, channels | ⏳ Stubs only |
| **Error Handling** | try, catch, after | ⏳ Stubs only |
| **String I/O** | Term.ask returns strings | ✅ Complete |
| **Library Generation** | AC->LIB to .so/.dll | ✅ Complete |
| **Cross-Platform** | Smart architecture detection | ⏳ Infrastructure |
| **Wine Testing** | Windows PE on Linux | ⏳ Needs cross-compilation flag |

---

## Next Steps

1. **Cross-compilation flag** → Enable PE generation on Linux
2. **Wine testing pipeline** → Validate Windows binaries on Linux
3. **Platform detection** → Auto-select ELF vs PE vs Mach-O
4. **Native Windows build** → Test on actual Windows
5. **Concurrency completion** → Finish spawn/wait/channels
6. **Error handling** → Implement TRY/CATCH/AFTER
