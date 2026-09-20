#pragma once

#include "ir.hpp"
#include <string>

// AArch64 native binary generator — direct machine-code emission, Linux ELF64 only.
// Mirrors exp_bny.cpp's role (BNY = x86-64 raw ELF) for AArch64: no external assembler,
// no external linker, hand-encoded instructions verified against a real aarch64-linux-gnu-as
// during development. Current scope: integer arithmetic/comparisons/logical ops/control-flow,
// user functions/recursion, Term.display for ints and direct string literals, integer
// arrays/lists, range/sequence materialization, .append, length, array display, simple string
// values/literals/concat/strlen/printing, Term.ask string input, scalar-double FP arithmetic and
// numeric/string casts, dictionaries, structured IF/FOR markers, and small integer helpers such as ac_ipow/math.mod. Tuples, mixed dynamic
// returns, bundle methods, generators, and try/catch remain explicit hard errors; native ilib
// calls use the AArch64 dynamic ELF path by default, or the static cross-link path with
// --static-link when ARM libraries are available.
bool generateArmBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                             bool staticLink = false);
