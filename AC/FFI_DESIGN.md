# AC Foreign Function Interface (FFI)

Call C libraries from AC, and call AC from C/Python/Go/Rust/etc.

---

## Overview

**Two-way interoperability:**

1. **AC → C** (AC calls C libraries)
   ```ac
   FFI libc
       Make strlen func(s: str) -> int
       Make
   
   length = strlen("hello")  /* Calls C strlen */
   ```

2. **C/Python/Go → AC** (Other languages call AC)
   ```python
   from ac import my_module
   result = my_module.my_function(42)
   ```

---

## Part 1: AC Calling C Libraries (AC → C)

### **Basic FFI Declaration**

```ac
AC->C

/* Import C library function */
FFI libc
    Make strlen func(str: str) -> int
    Make

FFI libm
    Make sqrt func(x: dec) -> dec
    Make

FFI libcurl
    Make curl_easy_init func() -> *void
    Make
```

**What happens:**
1. Compiler recognizes `FFI` block
2. Generates C `extern` declarations
3. At link time: links against `-lc`, `-lm`, `-lcurl`

### **Under the Hood**

```c
/* Generated C code from AC FFI block */
extern int strlen(const char* str);
extern double sqrt(double x);
extern CURL* curl_easy_init(void);

/* AC code becomes: */
int ac_strlen(const char* str) {
    return strlen(str);
}
```

### **Type Mapping**

```
AC Type          → C Type
────────────────────────────
int              → int64_t / long
str              → char* (null-terminated)
dec              → double (for now) / custom struct (future)
list             → custom array struct
*void            → void*
```

### **Full Example: File I/O with libc**

```ac
AC->C
use ilib os

FFI libc
    Make open func(path: str, flags: int) -> int
    Make
    Make read func(fd: int, buf: str, count: int) -> int
    Make
    Make close func(fd: int) -> int
    Make

Make read_file func(path: str) -> str
    fd = open(path, 0)  /* O_RDONLY = 0 */
    IF fd < 0
        RETURN $error$
    
    buffer = $                                $  /* 1KB buffer */
    bytes_read = read(fd, buffer, 1024)
    close(fd)
    
    RETURN buffer
Make
```

### **Error Handling**

```ac
FFI libc
    Make fopen func(path: str, mode: str) -> *void
    Make

Make open_file func(path: str)
    TRY
        file_ptr = fopen(path, $r$)
        IF file_ptr is null
            THROW $file not found$
        RETURN file_ptr
    CATCH err
        Term.display $Error: $ + err
        RETURN null
    END
Make
```

---

## Part 2: Creating AC Libraries (AC->LIB)

**AC->LIB already works** and outputs shared libraries:
- `libmycode.so` (Linux)
- `mycode.dll` (Windows)
- `libmycode.dylib` (macOS)

### **For Library Development: Use clib/**

Store library code in `clib/` folder for testing:

```
my_project/
  clib/              /* Computer Libraries - internal testing */
    mylib/
      lib.ac
      atar.json
    another/
      lib.ac
      atar.json
  elib/              /* External libraries - installed via atar */
    downloaded_lib/
  src/
    main.ac
```

### **For External Libraries: Use elib/**

Downloaded packages go to `elib/`:

```bash
atar install math_lib              /* Downloads to elib/ */
atar install math_lib --local      /* Alternative: downloads to elib/ */
```

### **C Caller**

```c
#include "mycode.h"

int main() {
    int result = ac_multiply(6, 7);
    printf("Result: %d\n", result);  /* Output: 42 */
    
    const char* greeting = ac_greet("World");
    printf("%s\n", greeting);  /* Output: Hello, World */
    
    return 0;
}
```

### **Auto-Generated Header**

```c
/* mycode.h - AUTO-GENERATED */
#ifndef MYCODE_H
#define MYCODE_H

int ac_multiply(int a, int b);
char* ac_greet(const char* name);

#endif
```

### **Python Caller** (via ctypes)

```python
from ctypes import CDLL, c_int, c_char_p

lib = CDLL('./libmycode.so')

# Call multiply
result = lib.ac_multiply(6, 7)
print(f"Result: {result}")  # 42

# Call greet
lib.ac_greet.restype = c_char_p
greeting = lib.ac_greet(b"World")
print(greeting.decode())  # Hello, World
```

### **Go Caller** (via cgo)

```go
package main

/*
#cgo LDFLAGS: -L. -lmycode
#include "mycode.h"
*/
import "C"

func main() {
    result := C.ac_multiply(6, 7)
    println("Result:", result)  // 42
    
    greeting := C.GoString(C.ac_greet(C.CString("World")))
    println(greeting)  // Hello, World
}
```

### **Rust Caller**

```rust
extern "C" {
    fn ac_multiply(a: i64, b: i64) -> i64;
    fn ac_greet(name: *const u8) -> *const u8;
}

fn main() {
    unsafe {
        let result = ac_multiply(6, 7);
        println!("Result: {}", result);  // 42
    }
}
```

---

## Part 3: Memory Management Across FFI

### **Ownership Model**

**String ownership:**
```ac
AC->C

Make process_string func(s: str) -> str
    /* AC owns input (passed from caller) */
    /* AC owns output (caller takes ownership) */
    RETURN $processed: $ + s
Make
```

**When called from C:**
```c
char* result = ac_process_string("input");
printf("%s\n", result);
free(result);  /* Caller must free */
```

### **List/Array Ownership**

```ac
AC->LIB

Make create_array func() -> list
    /* AC allocates and owns list */
    /* Caller gets ownership (must free when done) */
    RETURN [1, 2, 3, 4, 5]
Make
```

**When called from Python:**
```python
import ctypes

lib = ctypes.CDLL('./libarray.so')

# AC allocates
arr = lib.create_array()

# Python manages (AC runtime handles cleanup)
print(arr)  # [1, 2, 3, 4, 5]
```

---

## Part 4: FFI Declaration Syntax

### **Simple Functions**

```ac
FFI libc
    Make printf func(format: str) -> int
    Make
```

### **With Multiple Arguments**

```ac
FFI libexample
    Make compute func(a: int, b: int, c: dec) -> dec
    Make
```

### **With Pointers**

```ac
FFI libc
    Make malloc func(size: int) -> *void
    Make
    Make free func(ptr: *void) -> void
    Make
```

### **With Varargs** (advanced)

```ac
FFI libc
    Make printf func(format: str, ...) -> int
    Make
```

### **Struct Types** (future)

```ac
FFI libexample
    struct Point
        x: int
        y: int

    Make get_point func() -> Point
    Make
```

---

## Part 5: FFI Compilation

### **AC→C with FFI**

```bash
ac mycode.ac --target C
```

**Generated C code includes:**
```c
#include <stdio.h>  /* Auto-included for FFI */

/* AC code ... */

int ac_strlen(const char* str) {
    return strlen(str);
}
```

### **AC→LIB with FFI**

```bash
ac mycode.ac --target LIB
```

**Build process:**
```
AC code
    ↓
Generate C
    ↓
Add FFI headers (auto-include libm, libc, etc.)
    ↓
GCC compile
    ↓
Link shared library
    ↓
Auto-generate .h header file
    ↓
Output: libmycode.so + mycode.h
```

### **Linking**

```bash
/* Option 1: Implicit linking (compiler detects FFI libraries) */
ac mycode.ac --target C
gcc mycode.c -lm -lc -lcurl  /* Auto-inferred */

/* Option 2: Explicit linking */
ac mycode.ac --target LIB --link "-lm -lcurl"

/* Option 3: Custom flags in code */
FFI libcustom LINK "-L/custom/lib -lcustom"
    Make custom_func func() -> int
    Make
```

---

## Part 6: ABI Stability

### **Version Tagging**

```ac
AC->LIB

VERSION 1.0.0

Make add func(a: int, b: int) -> int
    RETURN a + b
Make
```

**Auto-generates:**
```c
/* mycode.h */
const char* ac_version = "1.0.0";
```

### **Symbol Visibility**

```ac
AC->LIB

/* Public (exported) */
Make public_func func() -> int
    RETURN 42
Make

/* Private (internal use only) */
Make private_func func() -> int
    RETURN 0
Make
```

**Generated header:**
```c
int ac_public_func(void);  /* Exported */
/* private_func not in header */
```

---

## Implementation Plan

### **Phase 1: Basic FFI (AC calling C)**
- ✅ Parse FFI blocks
- ✅ Generate extern declarations
- ✅ Link against system libraries
- ✅ Type mapping (int, str, *void)

### **Phase 2: Reverse FFI (C calling AC)**
- ✅ AC->LIB backend generates shared library
- ✅ Auto-generate C headers
- ✅ Handle memory ownership

### **Phase 3: Binding Generators**
- ✅ Python ctypes bindings
- ✅ Go cgo bindings
- ✅ Rust extern declarations

### **Phase 4: Advanced Features**
- ✅ Struct types
- ✅ Varargs
- ✅ Custom link flags
- ✅ ABI versioning

---

## Files to Create

```
AC/ac-compiler/
  src/
    ffi_parser.cpp           /* Parse FFI blocks */
    ffi_codegen.cpp          /* Generate extern declarations */
  include/
    ffi.hpp

AC/ac-compiler/backends/
  libgen.cpp                 /* AC->LIB backend (updated) */
    - Generate .h headers
    - Handle memory ownership

AC/ac-ffi/
  binding_generators/
    python_ctypes.cpp        /* Generate Python bindings */
    go_cgo.cpp               /* Generate Go bindings */
    rust.cpp                 /* Generate Rust bindings */
```

---

## Example: Complete FFI Library

```ac
AC->LIB
VERSION 1.0.0

/* Math library wrapper */
FFI libm
    Make sin func(x: dec) -> dec
    Make
    Make cos func(x: dec) -> dec
    Make
    Make sqrt func(x: dec) -> dec
    Make

/* AC functions */
Make calculate func(angle: dec) -> dec
    /* Uses both FFI (libm) and native AC */
    value = sin(angle)
    RETURN sqrt(value * value + 1)
Make
```

**Usage in Python:**
```python
from ac import mathlib
result = mathlib.calculate(3.14159)
print(result)  # Uses C libm + AC logic
```

---

## Summary

**AC FFI enables seamless interop:**

✅ AC calling C libraries (native performance)  
✅ C/Python/Go/Rust calling AC (share AC code)  
✅ Type-safe mapping across boundaries  
✅ Auto-generated headers and bindings  
✅ ABI versioning and stability  
✅ Memory safety guarantees  

**Result:** AC becomes the glue language in polyglot systems. 🎯
