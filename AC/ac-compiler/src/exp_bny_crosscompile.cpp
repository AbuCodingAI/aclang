/*
  Cross-compilation support for AC->BNY on non-native platforms
  
  When AC->BNY is invoked on:
  - ARM (any OS): Generate C intermediary → compile with GCC/Clang
  - Non-Linux x86: Generate C intermediary → compile with native compiler
  
  For AC->ASM on ARM:
  - Generate .s/.asm file
  - Optionally compile to binary
  - Keep assembly source
*/

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace AC_BinaryGen {

// Detect host CPU architecture
enum class CPUArch {
    X86_64,
    ARM64,
    ARM32,
    UNKNOWN
};

CPUArch detectHostCPU() {
#if defined(__x86_64__) || defined(_M_X64)
    return CPUArch::X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return CPUArch::ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    return CPUArch::ARM32;
#else
    return CPUArch::UNKNOWN;
#endif
}

// Detect host OS
enum class HostOS {
    WINDOWS,
    MACOS,
    LINUX,
    UNKNOWN
};

HostOS detectHostOS() {
#ifdef _WIN32
    return HostOS::WINDOWS;
#elif __APPLE__
    return HostOS::MACOS;
#elif __linux__
    return HostOS::LINUX;
#else
    return HostOS::UNKNOWN;
#endif
}

// Check if we need cross-compilation
bool needsCrossCompilation(bool targetNative) {
    CPUArch cpu = detectHostCPU();
    
    // If we're on x86-64 and target is native, no cross-compilation needed
    if (targetNative && cpu == CPUArch::X86_64) {
        return false;
    }
    
    // If we're on ARM, or target is ARM, we need cross-compilation
    if (cpu == CPUArch::ARM64 || cpu == CPUArch::ARM32) {
        return true;
    }
    
    return false;
}

// Compile C intermediary using system C compiler
bool compileWithGCC(const std::string& cFile, const std::string& outFile) {
    HostOS os = detectHostOS();
    std::string compiler = "gcc";
    std::string cmd;
    
#ifdef __APPLE__
    compiler = "clang";  // macOS ARM uses clang
#endif
    
    // Build compile command
    cmd = compiler + " -O2 -o " + outFile + " " + cFile;
    
    std::cout << "[Cross-compile] " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    
    if (result != 0) {
        std::cerr << "Cross-compilation failed: " << cmd << std::endl;
        return false;
    }
    
    // Delete intermediary C file
    if (std::remove(cFile.c_str()) != 0) {
        std::cerr << "Warning: Could not delete intermediary file: " << cFile << std::endl;
    }
    
    return true;
}

// Generate C code from BNY IR (intermediary)
bool generateCIntermediary(const std::string& cOutPath) {
    // This would generate C code from the IR
    // For now, placeholder - actual implementation depends on IR structure
    std::ofstream cFile(cOutPath);
    if (!cFile) {
        std::cerr << "Failed to create C intermediary: " << cOutPath << std::endl;
        return false;
    }
    
    // Write minimal C program (to be filled in with actual IR→C translation)
    cFile << "int main() { return 0; }\n";
    cFile.close();
    
    return true;
}

// Handle AC->BNY on non-native platforms
bool compileViaC(const std::string& binPath) {
    // Generate temporary C file
    std::string cPath = binPath + ".c";
    
    std::cout << "[AC->BNY] Platform is non-native, using C intermediary" << std::endl;
    
    if (!generateCIntermediary(cPath)) {
        return false;
    }
    
    if (!compileWithGCC(cPath, binPath)) {
        return false;
    }
    
    std::cout << "[AC->BNY] Cross-compilation complete: " << binPath << std::endl;
    return true;
}

}  // namespace AC_BinaryGen
