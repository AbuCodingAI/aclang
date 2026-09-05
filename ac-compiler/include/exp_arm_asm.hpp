#pragma once

#include "ir.hpp"
#include <string>

// AArch64 GNU-assembler text generator — Phase 2 of the ARM backend, built ON TOP OF exp_arm.cpp
// (AC->ARM, the raw-ELF binary backend): same instruction selection, same register/slot
// allocation, same calling convention, same temp-elision optimization — all already proven
// correct there. The only real difference is HOW an instruction is emitted (text mnemonic vs.
// raw encoded bytes) and how branches resolve (symbolic labels the real assembler links, vs.
// this project's own hand-computed relative-offset fixups) — text assembly needs none of
// exp_arm.cpp's fixup/patching machinery at all, since `aarch64-linux-gnu-as`/`ld` do that job.
// Assemble: aarch64-linux-gnu-as out.s -o out.o && aarch64-linux-gnu-ld out.o -o out
bool generateArmAsmFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile);
