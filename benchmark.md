# AC Language - Hello World Benchmark

Benchmarks for compiling and running a simple Hello World program across all supported backends.

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

| Backend | Declaration | Time (compile + run) | Output |
|---------|-------------|----------------------|--------|
| Python  | `AC->PY`    | 47ms                 | Hello, World! |
| C       | `AC->C`     | 96ms                 | Hello, World! |
| ASM     | `AC->ASM`   | 58ms                 | Hello, World! |
| Go      | `AC->GO`    | 199ms                | Hello, World! |
| JavaScript | `AC->JS` | 365ms               | Hello, World! |
| Rust    | `AC->RS`    | 367ms                | Hello, World! |
| C++     | `AC->C++`   | 656ms                | Hello, World! |
| Java    | `AC->Java`  | 1287ms               | Hello, World! |

## Notes

- Times include both compilation and execution
- Java is slowest due to JVM startup overhead
- ASM is among the fastest since gcc compiles to native code with minimal overhead
- Python is fast here because it's interpreted with no compile step
- C++ and Rust are slower than C/ASM due to more complex compilation pipelines
- V backend code generation is implemented but excluded from benchmark pending environment setup

## Machine

- OS: Linux
- Compiler: AC Compiler (ac-compiler)
- Date: 2026-03-17
