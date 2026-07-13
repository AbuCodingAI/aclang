#pragma once

#include "ir.hpp"
#include <string>

// Experimental binary generator - direct machine code emission
// Skips ASM, .o files, and external linker
// debugInfo: emit DWARF sections (-g flag); srcPath: original source file path for debug info
// runpath: absolute ilib dirs to embed as DT_RUNPATH so the binary finds its .so deps WITHOUT
//          LD_LIBRARY_PATH (proper dynamic linking). Empty = old behavior (no DT_RUNPATH).
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                          bool debugInfo = false, const std::string& srcPath = "",
                          const std::string& runpath = "");
