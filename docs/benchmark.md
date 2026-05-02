# AC Language - Hello World Benchmark

Benchmarks for compiling and running a simple Hello World program across all supported backends.
Each backend was run 5 times. Times include both compilation and execution.

## Source

```ac
AC->PY *backend*

<mainloop>
     <LOGIC>
          Term.display $Hello, World!$
          /kill
     <LOGIC>
<mainloop>
```

## Results

| Backend    | Declaration | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Average |
|------------|-------------|-------|-------|-------|-------|-------|---------|
| ASM        | `AC->ASM`   | 74ms  | 66ms  | 46ms  | 50ms  | 63ms  | **59ms**  |
| C          | `AC->C`     | 84ms  | 85ms  | 77ms  | 84ms  | 79ms  | **81ms**  |
| Python     | `AC->PY`    | 52ms  | 46ms  | 54ms  | 53ms  | 46ms  | **50ms**  |
| Go         | `AC->GO`    | 201ms | 190ms | 215ms | 204ms | 194ms | **200ms** |
| JavaScript | `AC->JS`    | 269ms | 277ms | 360ms | 231ms | 262ms | **279ms** |
| Rust       | `AC->RS`    | 316ms | 319ms | 319ms | 326ms | 369ms | **329ms** |
| C++        | `AC->C++`   | 810ms | 706ms | 717ms | 809ms | 682ms | **744ms** |
| Java       | `AC->Java`  | 1246ms| 1216ms| 1318ms| 1139ms| 1151ms| **1214ms**|

## Sorted by Average Speed

1. Python  — 50ms  (interpreted, no compile step)
2. ASM     — 59ms  (native binary via gcc)
3. C       — 81ms  (native binary via gcc)
4. Go      — 200ms (fast compiler)
5. JS      — 279ms (Node.js startup)
6. Rust    — 329ms (rustc compile time)
7. C++     — 744ms (g++ compile time)
8. Java    — 1214ms (javac + JVM startup)

## Notes

- Python appears fastest because it skips compilation entirely — just runs the script
- ASM and C are the fastest compiled backends, producing lean native binaries
- Java is slowest due to two-step overhead: `javac` compile then JVM startup
- C++ and Rust are slower than C/ASM due to heavier compilation pipelines
- Go strikes a good balance — fast compiler, native output
- V backend code generation is implemented but excluded pending environment setup

---

## Round 2 — Execution Only (Precompiled)

Compiled backends (ASM, C, C++, Go, Rust) were precompiled ahead of time. Only execution time is measured here — no compilation overhead. Interpreted/VM backends (Python, JS, Java) include their runtime startup.

| Backend    | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Average |
|------------|-------|-------|-------|-------|-------|---------|
| ASM        | 5ms   | 4ms   | 4ms   | 4ms   | 3ms   | **4ms** |
| C          | 4ms   | 4ms   | 4ms   | 4ms   | 4ms   | **4ms** |
| C++        | 5ms   | 5ms   | 5ms   | 4ms   | 5ms   | **4ms** |
| Go         | 5ms   | 5ms   | 4ms   | 4ms   | 4ms   | **4ms** |
| Rust       | 4ms   | 4ms   | 3ms   | 4ms   | 4ms   | **3ms** |
| Python     | 30ms  | 29ms  | 28ms  | 28ms  | 28ms  | **28ms**|
| JavaScript | 28ms  | 27ms  | 26ms  | 26ms  | 28ms  | **27ms**|
| Java       | 107ms | 107ms | 107ms | 106ms | 107ms | **106ms**|

## Round 2 — Sorted by Average Speed

1. Rust       — 3ms  (native binary, no runtime)
2. ASM        — 4ms  (native binary)
3. C          — 4ms  (native binary)
4. C++        — 4ms  (native binary, same speed as C at runtime)
5. Go         — 4ms  (native binary, small runtime)
6. JavaScript — 27ms (Node.js startup)
7. Python     — 28ms (interpreter startup)
8. Java       — 106ms (JVM startup, even precompiled)

## Round 2 — Notes

- All compiled backends (ASM, C, C++, Go, Rust) are essentially tied at ~4ms — the difference is just OS scheduling noise
- C++ was slow in Round 1 purely because `g++` is a heavy compiler — at runtime it's identical to C
- Python and JS are close because both are bottlenecked by interpreter/engine startup, not the program itself
- Java still pays ~106ms even with precompiled `.class` files — that's pure JVM startup cost
- This round shows the true compiled vs interpreted gap: **~7x faster** for native binaries over Python/JS

## Machine

- OS: Linux
- Compiler: AC Compiler (ac-compiler)
- Date: 2026-03-17
