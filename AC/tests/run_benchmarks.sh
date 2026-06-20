#!/bin/bash

# Benchmark Suite for AC Compiler
# Tests: compilation speed, runtime speed, output size across all 11 backends

set -e

BENCH_FILE="$(dirname "$0")/benchmark_suite.ac"
RESULTS_DIR="$(dirname "$0")/benchmark_results"
COMPILER="$(dirname "$0")/../compiler/ac"  # Adjust path to actual compiler

mkdir -p "$RESULTS_DIR"

# Backends: compiled, interpreted, and special
BACKENDS=(
    "BNY:compiled"      # Binary Genius (native)
    "C:compiled"        # C
    "CPP:compiled"      # C++
    "PY:interpreted"    # Python
    "JS:interpreted"    # JavaScript
    "GO:compiled"       # Go
    "RS:compiled"       # Rust
    "JAVA:compiled"     # Java
    "V:compiled"        # V (modern compiled)
    "HTML:special"      # HTML (markup output)
    "ASM:compiled"      # Assembly (x86-64)
)

echo "=================================================="
echo "AC Compiler Benchmark Suite"
echo "=================================================="
echo ""

# Results CSV header
cat > "$RESULTS_DIR/compilation_speed.csv" << 'EOF'
Backend,CompileTime_ms,OutputSize_bytes,SourceSize_bytes,TotalSize_bytes,OptimizationLevel
EOF

cat > "$RESULTS_DIR/runtime_speed.csv" << 'EOF'
Backend,RuntimeTime_ms,Result_Output
EOF

cat > "$RESULTS_DIR/output_size.csv" << 'EOF'
Backend,SourceCode_bytes,OutputBinary_bytes,CacheData_bytes,TotalSize_bytes,SizeComparison_toNative_percent
EOF

for backend_info in "${BACKENDS[@]}"; do
    IFS=':' read -r BACKEND TYPE <<< "$backend_info"

    echo "Testing $BACKEND ($TYPE)..."

    OUTPUT_FILE="$RESULTS_DIR/benchmark_$BACKEND"

    # ========================================================================
    # TEST 1: COMPILATION SPEED
    # ========================================================================

    # Clean output
    rm -f "$OUTPUT_FILE"* "$OUTPUT_FILE.cache" "$OUTPUT_FILE.tmp"

    # Measure compilation time
    START_TIME=$(date +%s%N)

    if [ "$BACKEND" = "BNY" ]; then
        # Native binary
        "$COMPILER" "$BENCH_FILE" -target bny -o "$OUTPUT_FILE" 2>/dev/null || true
    elif [ "$BACKEND" = "C" ]; then
        "$COMPILER" "$BENCH_FILE" -target c -o "$OUTPUT_FILE.c" 2>/dev/null || true
        gcc -O2 "$OUTPUT_FILE.c" -o "$OUTPUT_FILE" 2>/dev/null || true
    elif [ "$BACKEND" = "CPP" ]; then
        "$COMPILER" "$BENCH_FILE" -target cpp -o "$OUTPUT_FILE.cpp" 2>/dev/null || true
        g++ -O2 "$OUTPUT_FILE.cpp" -o "$OUTPUT_FILE" 2>/dev/null || true
    elif [ "$BACKEND" = "PY" ]; then
        "$COMPILER" "$BENCH_FILE" -target py -o "$OUTPUT_FILE.py" 2>/dev/null || true
    elif [ "$BACKEND" = "JS" ]; then
        "$COMPILER" "$BENCH_FILE" -target js -o "$OUTPUT_FILE.js" 2>/dev/null || true
    elif [ "$BACKEND" = "GO" ]; then
        "$COMPILER" "$BENCH_FILE" -target go -o "$OUTPUT_FILE.go" 2>/dev/null || true
        go build -o "$OUTPUT_FILE" "$OUTPUT_FILE.go" 2>/dev/null || true
    elif [ "$BACKEND" = "RS" ]; then
        "$COMPILER" "$BENCH_FILE" -target rs -o "$OUTPUT_FILE.rs" 2>/dev/null || true
        rustc -O "$OUTPUT_FILE.rs" -o "$OUTPUT_FILE" 2>/dev/null || true
    elif [ "$BACKEND" = "JAVA" ]; then
        "$COMPILER" "$BENCH_FILE" -target java -o "$OUTPUT_FILE.java" 2>/dev/null || true
        javac "$OUTPUT_FILE.java" 2>/dev/null || true
    elif [ "$BACKEND" = "V" ]; then
        "$COMPILER" "$BENCH_FILE" -target v -o "$OUTPUT_FILE.v" 2>/dev/null || true
        v "$OUTPUT_FILE.v" 2>/dev/null || true
    elif [ "$BACKEND" = "HTML" ]; then
        "$COMPILER" "$BENCH_FILE" -target html -o "$OUTPUT_FILE.html" 2>/dev/null || true
    elif [ "$BACKEND" = "ASM" ]; then
        "$COMPILER" "$BENCH_FILE" -target asm -o "$OUTPUT_FILE.s" 2>/dev/null || true
        as "$OUTPUT_FILE.s" -o "$OUTPUT_FILE.o" 2>/dev/null || true
    fi

    END_TIME=$(date +%s%N)
    COMPILE_TIME=$(( (END_TIME - START_TIME) / 1000000 ))  # Convert to milliseconds

    # Measure output sizes
    SOURCE_SIZE=$(stat -f%z "$BENCH_FILE" 2>/dev/null || stat -c%s "$BENCH_FILE" 2>/dev/null || echo 0)

    OUTPUT_SIZE=0
    if [ -f "$OUTPUT_FILE" ]; then
        OUTPUT_SIZE=$(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo 0)
    elif [ -f "$OUTPUT_FILE.$BACKEND" ]; then
        OUTPUT_SIZE=$(stat -f%z "$OUTPUT_FILE.$BACKEND" 2>/dev/null || stat -c%s "$OUTPUT_FILE.$BACKEND" 2>/dev/null || echo 0)
    fi

    TOTAL_SIZE=$((SOURCE_SIZE + OUTPUT_SIZE))

    echo "  Compilation: ${COMPILE_TIME}ms"
    echo "  Output size: $OUTPUT_SIZE bytes"
    echo "  Source size: $SOURCE_SIZE bytes"
    echo "  Total: $TOTAL_SIZE bytes"

    # ========================================================================
    # TEST 2: RUNTIME SPEED (if executable)
    # ========================================================================

    if [ -f "$OUTPUT_FILE" ] && [ -x "$OUTPUT_FILE" ]; then
        START_TIME=$(date +%s%N)

        RUNTIME_OUTPUT=$("$OUTPUT_FILE" 2>/dev/null || echo "ERROR")

        END_TIME=$(date +%s%N)
        RUNTIME=$(( (END_TIME - START_TIME) / 1000000 ))  # milliseconds

        echo "  Runtime: ${RUNTIME}ms"
        echo "  Output: $RUNTIME_OUTPUT"

        echo "$BACKEND,$RUNTIME,\"$RUNTIME_OUTPUT\"" >> "$RESULTS_DIR/runtime_speed.csv"
    else
        echo "  Runtime: N/A (not executable)"
    fi

    # Record compilation metrics
    echo "$BACKEND,$COMPILE_TIME,$OUTPUT_SIZE,$SOURCE_SIZE,$TOTAL_SIZE,O2" >> "$RESULTS_DIR/compilation_speed.csv"
    echo "$BACKEND,$SOURCE_SIZE,$OUTPUT_SIZE,0,$TOTAL_SIZE,100" >> "$RESULTS_DIR/output_size.csv"

    echo ""
done

# ========================================================================
# GENERATE SUMMARY REPORT
# ========================================================================

echo "=================================================="
echo "RESULTS SUMMARY"
echo "=================================================="
echo ""

echo "Compilation Speed (fastest to slowest):"
sort -t',' -k2 -n "$RESULTS_DIR/compilation_speed.csv" | tail -n +2 | awk -F',' '{printf "  %-8s: %6d ms (%8d bytes)\n", $1, $2, $3}'

echo ""
echo "Output Sizes (smallest to largest):"
sort -t',' -k3 -n "$RESULTS_DIR/output_size.csv" | tail -n +2 | awk -F',' '{printf "  %-8s: %10d bytes\n", $1, $3}'

echo ""
echo "Full Results in: $RESULTS_DIR/"
echo "  - compilation_speed.csv"
echo "  - runtime_speed.csv"
echo "  - output_size.csv"
echo ""
