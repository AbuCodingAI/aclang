#pragma once

#include "ir.hpp"
#include <string>

// AC->RISC: AArch64 GNU assembly text. The same codegen as AC->ARM (exp_arm.cpp), rendered as
// assembly instead of an ELF binary. Assemble: aarch64-linux-gnu-as out.s -o out.o
bool generateArmAsmFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile);
