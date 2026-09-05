#include "../include/backend_registry.hpp"
#include "../include/ast.hpp"
#include <string>
#include <algorithm>

// Dummy generator function - not used anymore (unified IR codegen handles all backends)
std::string dummyGenerator(const ASTNode& ast) {
    return "";
}

// Static member definition
std::unordered_map<std::string, BackendRegistry::BackendInfo> BackendRegistry::backends;

// Helper to resolve library paths
std::string resolveLibraryPath(const std::string& libName) {
    // Hardcoded camera library path for all backends
    if (libName == "camera") {
        return "../library/ilib/camera/camera.hpp";
    }

    // Default library paths
    std::string path = "../library/ilib/" + libName + "/" + libName;
    
    // Determine extension based on backend
    // For C/C++ backends: .hpp or .h
    // For Python: .py
    // For JS: .js
    // etc.
    
    return path;
}

void BackendRegistry::initializeStandardBackends() {
    if (!backends.empty()) return; // Already initialized
    
    // Register all standard backends (generator function not used - unified IR codegen handles all)
    registerBackend("PY", ".py", dummyGenerator, 
        [](const std::string& outFile) { return "python3 " + outFile; });
    
    registerBackend("JS", ".js", dummyGenerator, 
        [](const std::string& outFile) { return "node " + outFile; });
    
    registerBackend("HTML", ".html", dummyGenerator, 
        [](const std::string& outFile) { return "xdg-open " + outFile; });
    
    registerBackend("Java", ".java", dummyGenerator, 
        [](const std::string& outFile) { return "javac " + outFile + " && java Main"; });
    
    registerBackend("C++", ".cpp", dummyGenerator, 
        [](const std::string& outFile) { return "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("CPP", ".cpp", dummyGenerator, 
        [](const std::string& outFile) { return "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("C", ".c", dummyGenerator, 
        [](const std::string& outFile) { return "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("ASM", ".asm", dummyGenerator,
        [](const std::string& outFile) { return "nasm -f elf64 " + outFile + " -o /tmp/ac_out.o && gcc /tmp/ac_out.o -no-pie -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("RS", ".rs", dummyGenerator, 
        [](const std::string& outFile) { return "rustc " + outFile + " -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("GO", ".go", dummyGenerator, 
        [](const std::string& outFile) { return "go run " + outFile; });
    
    registerBackend("V", ".v", dummyGenerator,
        [](const std::string& outFile) { return "v run " + outFile; });
    
    registerBackend("BNY", ".acb", dummyGenerator,
        [](const std::string& outFile) { return outFile; });  // Direct execution of binary

    // ARM (AArch64) raw ELF64 — see exp_arm.cpp. Host is x86-64 during development, so the
    // default run step goes through qemu-aarch64 (user-mode emulation); on real ARM64
    // hardware this would just be `outFile` directly, same as BNY above. main.cpp's actual
    // dispatch (runArmBinary) also auto-detects the host at compile time and routes "BNY"
    // itself here when running on ARM, so BNY isn't hardcoded to x86-64 either.
    registerBackend("ARM", ".acb", dummyGenerator,
        [](const std::string& outFile) { return "qemu-aarch64 " + outFile; });

    // RISC (AArch64 GNU-assembler text) — see exp_arm_asm.cpp, Phase 2 built on exp_arm.cpp's
    // proven design; ARM's assembly-text counterpart to BNY's x86 "ASM" backend, named for the
    // RISC instruction set family (vs. x86's CISC) rather than reusing "ARMASM". Assemble+link
    // with the real cross toolchain, then run under qemu-aarch64 (same cross-host caveat as ARM
    // above). main.cpp's dispatch (runArmAsm) also auto-detects the host and routes plain "ASM"
    // here when running on ARM, instead of emitting x86 NASM syntax that wouldn't assemble there.
    registerBackend("RISC", ".s", dummyGenerator,
        [](const std::string& outFile) {
            return "aarch64-linux-gnu-as " + outFile + " -o /tmp/ac_arm_out.o && "
                   "aarch64-linux-gnu-ld /tmp/ac_arm_out.o -o /tmp/ac_arm_out && "
                   "qemu-aarch64 /tmp/ac_arm_out";
        });

    registerBackend("LIB", ".cpp", dummyGenerator,
        [](const std::string& outFile) { return ""; });  // No run step; compiled to .so/.dll
}
