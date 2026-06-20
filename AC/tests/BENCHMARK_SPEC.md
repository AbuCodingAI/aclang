# AC Compiler Benchmark Suite

## Overview

Comprehensive benchmark for AC compilation across all 11 backends:
- **Compiled**: BNY, C, C++, Go, Rust, Java, V, Assembly
- **Interpreted**: Python, JavaScript  
- **Special**: HTML (markup output)

## Test Design

### Test File: benchmark_suite.ac

Single AC source file compiled to all backends, measuring:

**Workload:**
- Fibonacci(30) - recursive computation
- String manipulation - concatenation
- List operations - iteration and summation
- Loops - 1000 iterations

Same code → all backends → compare metrics

## Test Metrics

### 1. COMPILATION SPEED

**What:** Time from AC source → executable/bytecode/output

**Measurements:**
- `CompileTime_ms` - wall-clock milliseconds
- Includes:
  - AC compiler parse/IR/codegen
  - Backend-specific compilation (gcc, go build, rustc, etc.)
  - Linking (for compiled backends)

**Baseline:** BNY (native) or C (C backend)

**Why it matters:**
- Developer iteration speed
- CI/CD pipeline efficiency
- Build server costs

### 2. RUNTIME SPEED

**What:** Time to execute compiled benchmark

**Measurements:**
- `RuntimeTime_ms` - wall-clock milliseconds
- Includes:
  - JIT startup (Java)
  - Interpreter overhead (Python, JS)
  - Native code execution (BNY, C, etc.)

**Baseline:** BNY (native) = 100%

**Why it matters:**
- Application performance
- User experience
- Server resource costs

### 3. SOURCE + OUTPUT SIZE

**What:** Total disk footprint of source + compiled output

**Measurements:**
- `SourceSize_bytes` - benchmark_suite.ac
- `OutputBinary_bytes` - compiled output (.bin, .exe, .pyc, .js, etc.)
- `CacheData_bytes` - IR cache, object files, etc.
- `TotalSize_bytes` - source + output + cache

**Baseline:** BNY = smallest binary

**Why it matters:**
- Distribution size (embedded, mobile, serverless)
- Caching efficiency
- Storage costs
- Download bandwidth

### 4. SIZE COMPARISON

**Metric:** `SizeComparison_toNative_percent`

Ratio of output size to BNY (native) baseline:
- 100% = same size as native
- 150% = 1.5x larger than native
- 50% = half size of native

Lower is better.

## Expected Results Pattern

| Backend | Compile Speed | Runtime | Output Size | Profile |
|---------|---------------|---------|-------------|---------|
| BNY | Fast | ⚡⚡⚡ | Baseline | Native binary - best runtime |
| C | Slow | ⚡⚡⚡ | +10-20% | Overhead from C codegen |
| C++ | Slow | ⚡⚡⚡ | +15-25% | C++ codegen overhead |
| Go | Medium | ⚡⚡ | +30-50% | Runtime + stdlib bloat |
| Rust | Slow | ⚡⚡⚡ | +25-40% | Optimization pass overhead |
| Java | Fast | ⚡ | +80-120% | JIT startup + class files |
| Python | Fast | 🐢 | +5-10% | Source/bytecode only |
| JavaScript | Fast | 🐢 | +8-15% | Source/bytecode only |
| V | Medium | ⚡⚡ | +20-35% | Still young, potential perf |
| HTML | Fast | N/A | Varies | Markup only, no runtime |
| Assembly | Medium | ⚡⚡⚡ | +5-15% | Raw object file size |

## Running the Benchmark

```bash
chmod +x AC/tests/run_benchmarks.sh
./AC/tests/run_benchmarks.sh
```

## Output

Results in `AC/tests/benchmark_results/`:

- `compilation_speed.csv` - Compilation metrics
- `runtime_speed.csv` - Runtime performance + output
- `output_size.csv` - Size breakdown

## Interpretation Guide

### Good Compilation Speed
- BNY: < 500ms
- C/C++: < 1000ms (includes gcc/g++)
- Go: < 1500ms (includes go build)
- Python/JS: < 200ms (just AC compile, no link)

### Good Runtime Performance
- BNY: baseline (1.0x)
- Compiled: 0.8x - 2.0x of BNY
- Interpreted: 5x - 50x slower (expected)

### Good Output Size
- BNY: baseline
- Compiled: 1.0x - 1.5x of BNY
- Interpreted: 0.1x - 1.5x (no linking)

## Red Flags

🚩 **Compilation 10x slower than expected**
→ Check IR optimization passes, codegen efficiency

🚩 **Runtime 100x slower than compiled**
→ Check interpreter implementation, JIT warmup

🚩 **Output 10x larger than baseline**
→ Check dead code elimination, linker flags

🚩 **Huge variance between runs**
→ System load, cache effects, measure multiple times

## Hypothesis: AC Compilation Quality

Based on architecture:
- **IR layer** enables optimization
- **Multi-backend** means some overhead vs single-target languages
- **Interpreted backends** will be faster to compile but slower to run
- **V backend** new, may underperform until mature
- **Native (BNY)** should be competitive with C

Expected: AC compiles as fast as Go, runs as fast as C, outputs smaller than Java.
