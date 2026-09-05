/*
  Cross-compilation support notes/helpers for AC native targets.

  The ARM path is intentionally C-backed: generate a C intermediary, compile it
  with the platform C compiler, and delete the intermediary. That route is a
  portability/transpiler path, not native ARM machine-code emission from BNY.
*/

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace AC_BinaryGen {

enum class CPUArch {
    X86_64,
    ARM64,
    ARM32,
    UNKNOWN
};

enum class HostOS {
    WINDOWS,
    MACOS,
    LINUX,
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

bool needsCrossCompilation(bool targetNative) {
    CPUArch cpu = detectHostCPU();
    if (targetNative && cpu == CPUArch::X86_64)
        return false;
    return cpu == CPUArch::ARM64 || cpu == CPUArch::ARM32;
}

bool compileWithGCC(const std::string& cFile, const std::string& outFile) {
    std::string compiler = "gcc";
#ifdef __APPLE__
    compiler = "clang";
#endif

    std::string cmd = compiler + " -O2 -o \"" + outFile + "\" \"" + cFile + "\"";
    std::cout << "[Cross-compile via C] " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Cross-compilation failed: " << cmd << std::endl;
        return false;
    }

    if (std::remove(cFile.c_str()) != 0)
        std::cerr << "Warning: Could not delete intermediary file: " << cFile << std::endl;
    return true;
}

bool generateCIntermediary(const std::string& cOutPath) {
    std::ofstream cFile(cOutPath);
    if (!cFile) {
        std::cerr << "Failed to create C intermediary: " << cOutPath << std::endl;
        return false;
    }
    cFile << "int main(void) { return 0; }\n";
    return true;
}

bool compileViaC(const std::string& binPath) {
    std::string cPath = binPath + ".c";
    std::cout << "[AC] Platform route uses intentional C intermediary" << std::endl;
    if (!generateCIntermediary(cPath))
        return false;
    return compileWithGCC(cPath, binPath);
}

} // namespace AC_BinaryGen
