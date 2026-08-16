#pragma once

#include "ir.hpp"
#include <string>

// Experimental binary generator - direct machine code emission
// Skips ASM, .o files, and external linker
// debugInfo: emit DWARF sections (-g flag); srcPath: original source file path for debug info
// runpath: absolute ilib dirs to embed as DT_RUNPATH so the binary finds its .so deps WITHOUT
//          LD_LIBRARY_PATH (proper dynamic linking). Empty = old behavior (no DT_RUNPATH).
// bundleStatic: --static-link — splice freestanding ilib object code straight into the binary
//               (no gcc/ld, no DT_NEEDED) so it runs standalone. Falls back to dynamic if a needed
//               ilib symbol has no freestanding implementation yet.
// targetWindows: cross-compile to a PE32+ .exe instead of ELF (no host OS involved — this always
//                runs on the Linux-hosted `ac` binary). Very early: only the exit path is wired
//                (ExitProcess via a real Import Address Table); no print, no ilib calls yet.
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                          bool debugInfo = false, const std::string& srcPath = "",
                          const std::string& runpath = "", bool bundleStatic = false,
                          bool targetWindows = false);
