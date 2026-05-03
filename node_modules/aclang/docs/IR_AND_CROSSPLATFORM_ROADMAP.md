# IR Finalization & Cross-Platform Compilation Roadmap

**Goal:** Complete the IR system and enable compilation to multiple platforms/architectures

---

## 🎯 Overview

### Current State
- ✅ x86-64 Linux native binaries (AC->BNY)
- ✅ x86-64 assembly generation (AC->ASM)
- ✅ IR infrastructure exists but not active
- ❌ No cross-platform support
- ❌ No GPU compilation

### Target State
- ✅ IR-based compilation pipeline
- ✅ Multiple architectures (x86-64, ARM, RISC-V)
- ✅ Multiple platforms (Linux, Windows, macOS)
- ✅ GPU support (NVIDIA, AMD) - future

---

## 📋 Phase 1: IR Finalization (6-8 hours)

### Step 1: Complete IR Generator (2-3 hours)

**Update `ir_generator.cpp` to use typed operands:**

```cpp
// BEFORE (string-based):
IRInstruction instr(IROpcode::ADD);
instr.operands = {"x", "y"};

// AFTER (typed):
IRRef t0 = func.newTemp();
IRRef x = IRRef::var("x");
IRRef y = IRRef::var("y");
IRInstruction instr(IROpcode::ADD, t0, {x, y});
```

**Changes needed:**
1. Update expression generation to create temps
2. Update statement generation to use IRRef
3. Add temp allocation tracking
4. Generate explicit result targets

### Step 2: Pattern Recognition (2-3 hours)

**Recognize control flow patterns in IR:**

```cpp
// Detect IF pattern:
// JUMP_IF_FALSE cond, L0
// <then block>
// JUMP L1
// LABEL L0
// <else block>
// LABEL L1

// Generate high-level code:
if (cond) {
    // then block
} else {
    // else block
}
```

**Patterns to recognize:**
- IF/ELSE statements
- WHILE loops
- FOR loops
- Short-circuit evaluation

### Step 3: Basic Optimizations (2 hours)

**Implement simple optimization passes:**

1. **Constant Folding**
   ```
   t0 = ADD 2, 3  →  t0 = 5
   ```

2. **Dead Code Elimination**
   ```
   t0 = ADD x, y
   t1 = MUL a, b  (t0 never used)
   →
   t1 = MUL a, b
   ```

3. **Copy Propagation**
   ```
   t0 = x
   t1 = ADD t0, y
   →
   t1 = ADD x, y
   ```

---

## 📋 Phase 2: Cross-Platform Compilation (4-6 hours)

### Strategy: IR → C → Clang → Binary

**Why this approach:**
- Leverage mature C compilers (Clang, GCC)
- Automatic support for all architectures
- Built-in optimizations
- Cross-compilation support
- Less code to maintain

### Architecture Support Matrix

| Architecture | Compiler | Status | Notes |
|--------------|----------|--------|-------|
| **x86-64** | GCC/Clang | ✅ Working | Current BNY backend |
| **ARM64** | Clang | 🟡 Planned | Apple Silicon, Raspberry Pi |
| **ARM32** | Clang | 🟡 Planned | Embedded systems |
| **RISC-V** | Clang | 🟡 Planned | Emerging architecture |
| **WASM** | Clang | 🟡 Planned | Browser execution |
| **NVIDIA PTX** | Clang | 🔴 Future | GPU compute |
| **AMD GCN** | Clang | 🔴 Future | GPU compute |

### Platform Support Matrix

| Platform | Executable | Compiler | Status |
|----------|------------|----------|--------|
| **Linux x86-64** | ELF | GCC/Clang | ✅ Working |
| **Windows x86-64** | PE | MinGW/Clang | 🟡 Planned |
| **macOS x86-64** | Mach-O | Clang | 🟡 Planned |
| **macOS ARM64** | Mach-O | Clang | 🟡 Planned |
| **Linux ARM64** | ELF | Clang | 🟡 Planned |

### Implementation Plan

#### Step 1: Create Universal Binary Backend (2 hours)

```cpp
// ac-compiler/src/codegen_ubin.cpp
std::string generateUniversalBinary(const ASTNode& ast, 
                                    const std::string& target) {
    // 1. Generate C code
    std::string cCode = generateC(ast);
    
    // 2. Write to temp file
    std::string tempC = "/tmp/ac_temp.c";
    writeFile(tempC, cCode);
    
    // 3. Determine target triple
    std::string triple = getTargetTriple(target);
    // Examples:
    // "x86_64-pc-linux-gnu"      - Linux x86-64
    // "x86_64-w64-mingw32"       - Windows x86-64
    // "x86_64-apple-darwin"      - macOS x86-64
    // "aarch64-apple-darwin"     - macOS ARM64
    // "aarch64-linux-gnu"        - Linux ARM64
    // "riscv64-linux-gnu"        - RISC-V 64-bit
    // "wasm32-unknown-unknown"   - WebAssembly
    
    // 4. Compile with Clang
    std::string output = getOutputPath(target);
    std::string cmd = "clang -O2 -target " + triple + 
                      " " + tempC + " -o " + output;
    
    if (system(cmd.c_str()) != 0) {
        throw std::runtime_error("Compilation failed");
    }
    
    // 5. Read and return binary
    return readBinaryFile(output);
}
```

#### Step 2: Add Target Selection (1 hour)

```cpp
// New backend declarations
AC->BNY-LINUX-X64      // Linux x86-64 (current)
AC->BNY-LINUX-ARM64    // Linux ARM64
AC->BNY-WINDOWS-X64    // Windows x86-64
AC->BNY-MACOS-X64      // macOS Intel
AC->BNY-MACOS-ARM64    // macOS Apple Silicon
AC->BNY-RISCV64        // RISC-V 64-bit
AC->BNY-WASM           // WebAssembly
```

#### Step 3: Test Cross-Compilation (1-2 hours)

```bash
# Test on current platform
./ac benchmark.ac BNY-LINUX-X64
./benchmark.acb

# Cross-compile for other platforms
./ac benchmark.ac BNY-WINDOWS-X64
# Creates benchmark.exe (test on Windows)

./ac benchmark.ac BNY-MACOS-ARM64
# Creates benchmark.acb (test on Apple Silicon)

./ac benchmark.ac BNY-WASM
# Creates benchmark.wasm (test in browser)
```

---

## 📋 Phase 3: GPU Compilation (Future)

### NVIDIA GPU Support

**Pipeline:**
```
AC → IR → C/CUDA → nvcc → PTX → GPU Binary
```

**Alternative (Clang):**
```
AC → IR → C → Clang --cuda-gpu-arch=sm_75 → PTX
```

**Example AC Code:**
```ac
AC->GPU-NVIDIA

Make vectorAdd func(a, b, n)
    * Runs on GPU with parallel threads *
    FOR i in range(n)
        result[i] = a[i] + b[i]
    return result

<mainloop>
    a = [1, 2, 3, 4, 5]
    b = [10, 20, 30, 40, 50]
    result = vectorAdd(a, b, 5)
    Term.display result
    /kill
<mainloop>
```

**Generated CUDA:**
```cuda
__global__ void vectorAdd_kernel(int* a, int* b, int* result, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        result[i] = a[i] + b[i];
    }
}

void vectorAdd(int* a, int* b, int n) {
    int *d_a, *d_b, *d_result;
    
    // Allocate device memory
    cudaMalloc(&d_a, n * sizeof(int));
    cudaMalloc(&d_b, n * sizeof(int));
    cudaMalloc(&d_result, n * sizeof(int));
    
    // Copy to device
    cudaMemcpy(d_a, a, n * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, b, n * sizeof(int), cudaMemcpyHostToDevice);
    
    // Launch kernel
    int blockSize = 256;
    int numBlocks = (n + blockSize - 1) / blockSize;
    vectorAdd_kernel<<<numBlocks, blockSize>>>(d_a, d_b, d_result, n);
    
    // Copy back
    cudaMemcpy(result, d_result, n * sizeof(int), cudaMemcpyDeviceToHost);
    
    // Free
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_result);
}
```

**Challenges:**
- Memory management (host ↔ device transfers)
- Kernel launch configuration (blocks, threads)
- Synchronization
- Error handling
- **Cannot test without NVIDIA GPU**

### AMD GPU Support

**Pipeline:**
```
AC → IR → C/HIP → hipcc → GCN → GPU Binary
```

**Status:** Future work, needs AMD GPU for testing

---

## 🔧 Implementation Details

### Target Triple Format

```
<arch>-<vendor>-<os>-<abi>
```

**Examples:**
- `x86_64-pc-linux-gnu` - Linux x86-64
- `x86_64-w64-mingw32` - Windows x86-64 (MinGW)
- `x86_64-apple-darwin` - macOS Intel
- `aarch64-apple-darwin` - macOS ARM64
- `aarch64-linux-gnu` - Linux ARM64
- `riscv64-unknown-linux-gnu` - RISC-V Linux
- `wasm32-unknown-unknown` - WebAssembly

### Clang Cross-Compilation

```bash
# Install cross-compilation toolchains
sudo apt install clang lld

# Cross-compile for Windows
clang -target x86_64-w64-mingw32 -o program.exe program.c

# Cross-compile for ARM64
clang -target aarch64-linux-gnu -o program program.c

# Cross-compile for WASM
clang -target wasm32-unknown-unknown --no-standard-libraries \
      -Wl,--no-entry -Wl,--export-all -o program.wasm program.c
```

### Testing Strategy

1. **Native compilation** (x86-64 Linux) - Test locally
2. **Cross-compilation** - Generate binaries, test on target platforms
3. **Emulation** - Use QEMU for ARM/RISC-V testing
4. **CI/CD** - Automated testing on multiple platforms

---

## 📊 Timeline

### Week 1: IR Finalization
- Day 1-2: Update IR generator
- Day 3-4: Pattern recognition
- Day 5: Basic optimizations
- Day 6-7: Testing and debugging

### Week 2: Cross-Platform Support
- Day 1-2: Universal binary backend
- Day 3: Target selection system
- Day 4-5: Testing on multiple platforms
- Day 6-7: Documentation and examples

### Future: GPU Support
- Research CUDA/HIP integration
- Design GPU memory model
- Implement kernel generation
- **Blocked on hardware availability**

---

## 🎯 Success Criteria

### IR System
- ✅ All backends use IR
- ✅ Pattern recognition works
- ✅ Basic optimizations functional
- ✅ Code quality improved

### Cross-Platform
- ✅ Compiles for Linux x86-64
- ✅ Compiles for Windows x86-64
- ✅ Compiles for macOS x86-64
- ✅ Compiles for macOS ARM64
- ✅ Compiles for Linux ARM64
- ✅ Binaries run correctly on target platforms

### GPU (Future)
- ⏳ CUDA code generation
- ⏳ Memory management
- ⏳ Kernel launch
- ⏳ Performance benchmarks
- ❌ **Blocked: No GPU available for testing**

---

## 📝 Notes

### Why IR → C → Clang?

**Pros:**
- Leverage 40+ years of C compiler optimization
- Automatic support for all architectures
- Cross-compilation built-in
- Less code to maintain
- Proven reliability

**Cons:**
- Extra compilation step (slower)
- Less control over code generation
- Dependency on external compiler

**Alternative: Direct ASM for each architecture**
- More control
- Potentially faster compilation
- Much more work (need codegen_asm_arm.cpp, codegen_asm_riscv.cpp, etc.)
- Only feasible for x86-64 (already done)

**Alternative: LLVM Backend**
- Best of both worlds
- Full control + optimization
- Support for all architectures
- Complex to implement
- Best long-term solution

**Decision:** Start with IR → C → Clang, migrate to LLVM later

---

## 🚀 Getting Started

### Prerequisites
```bash
# Install Clang
sudo apt install clang lld

# Install cross-compilation support
sudo apt install gcc-mingw-w64  # For Windows
sudo apt install gcc-aarch64-linux-gnu  # For ARM64
```

### Quick Test
```bash
# Compile for current platform
./ac benchmark.ac BNY

# Cross-compile for Windows
./ac benchmark.ac BNY-WINDOWS-X64

# Cross-compile for ARM64
./ac benchmark.ac BNY-LINUX-ARM64
```

---

**Status:** Ready to implement  
**Priority:** High  
**Estimated Time:** 10-14 hours total  
**Blocked Items:** GPU support (no hardware)

