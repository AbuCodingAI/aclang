/*
  exp_arm_asm.cpp — AC->RISC: AArch64 GNU assembly text.

  RISC is the ARM backend's own code, written out as assembly. There is no second code generator:
  exp_arm.cpp compiles the program once (same instruction selection, slots, calling convention,
  temp elision, and runtime routines as the AC->ARM binary), and generateListingFromIR renders
  those exact bytes as text. The mnemonics come from objdump, the ISA decoder, so the listing
  cannot drift from the binary; branch and adr targets are labels; inline data is .byte lines.
  Assemble with: aarch64-linux-gnu-as out.s -o out.o && aarch64-linux-gnu-ld -Ttext=0x402000 --section-start=.bss=0x401000 out.o -o out
  (absolute addresses follow the ARM ELF layout, the same as the AC->ARM binary).
*/
#include "../include/exp_arm_asm.hpp"
#include <iostream>

// Defined in exp_arm.cpp next to the ARM binary writer (same codegen, text output).
bool generateArmListingFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile, std::string& error);

bool generateArmAsmFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile) {
    std::string error;
    if (!generateArmListingFromIR(ir, outputFile, error)) {
        std::cerr << "Preposterous: BackendError: RISC listing: " << error << "\n";
        return false;
    }
    return true;
}
