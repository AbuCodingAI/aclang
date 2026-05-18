#pragma once

#include "ir.hpp"
#include <string>

// Experimental binary generator - direct machine code emission
// Skips ASM, .o files, and external linker
// debugInfo: emit DWARF sections (-g flag); srcPath: original source file path for debug info
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                          bool debugInfo = false, const std::string& srcPath = "");
