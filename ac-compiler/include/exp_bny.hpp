#pragma once

#include "ir.hpp"
#include <string>

// Experimental binary generator - direct machine code emission
// Skips ASM, .o files, and external linker
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile);
