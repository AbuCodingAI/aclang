#pragma once

#include "ir.hpp"
#include <string>

// AArch64 native binary generator — direct machine-code emission, Linux ELF64 only.
// Mirrors exp_bny.cpp's role (BNY = x86-64 raw ELF) for AArch64: no external assembler,
// no external linker, hand-encoded instructions verified against a real aarch64-linux-gnu-as
// during development. v1 scope: integer arithmetic/comparisons/control-flow/print in a
// single <mainloop> (matches BNY's own earliest capability before it grew function-call,
// string, array, and class support over many follow-up passes — this is the same starting
// point, not a shortcut).
bool generateArmBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile);
