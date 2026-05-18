#include "../include/ast.hpp"
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Forward declaration - ASM backend is compiled separately
extern std::string generateAsm(const ASTNode& ast);

// BNY Backend: Compiles AC -> ASM -> Binary (.acb)
// This backend generates x86-64 assembly and then assembles/links it into a native executable

// OS detection
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
#elif defined(__linux__)
    #define OS_LINUX
#elif defined(__APPLE__)
    #define OS_MACOS
#else
    #define OS_UNKNOWN
#endif

std::string generateBny(const ASTNode& ast) {
    // Step 1: Generate ASM code using the ASM backend
    std::string asmCode = generateAsm(ast);
    
    // Step 2: Write ASM to temporary file (OS-specific paths)
    std::string tempAsm;
    std::string tempBinary;
    std::string compileCmd;
    
    #ifdef OS_WINDOWS
        // Windows: Use %TEMP% or fallback to current directory
        const char* tempDir = getenv("TEMP");
        if (!tempDir) tempDir = getenv("TMP");
        if (!tempDir) tempDir = ".";
        
        tempAsm = std::string(tempDir) + "\\ac_temp.s";
        tempBinary = std::string(tempDir) + "\\ac_temp.exe";
        
        // Try MinGW-w64 GCC first, then MSVC ml64
        compileCmd = "x86_64-w64-mingw32-gcc -no-pie " + tempAsm + " -o " + tempBinary + " 2>&1";
    #elif defined(OS_LINUX)
        // Linux: Use /tmp
        tempAsm = "/tmp/ac_temp.s";
        tempBinary = "/tmp/ac_temp.acb";
        compileCmd = "gcc -no-pie " + tempAsm + " -o " + tempBinary + " 2>&1";
    #elif defined(OS_MACOS)
        // macOS: Use /tmp with clang
        tempAsm = "/tmp/ac_temp.s";
        tempBinary = "/tmp/ac_temp.acb";
        compileCmd = "clang -no-pie " + tempAsm + " -o " + tempBinary + " 2>&1";
    #else
        // Unknown OS: Try current directory
        tempAsm = "./ac_temp.s";
        tempBinary = "./ac_temp.acb";
        compileCmd = "gcc -no-pie " + tempAsm + " -o " + tempBinary + " 2>&1";
    #endif
    
    std::ofstream asmFile(tempAsm);
    if (!asmFile) {
        throw std::runtime_error("Failed to create temporary ASM file at: " + tempAsm);
    }
    asmFile << asmCode;
    asmFile.close();
    
    // Step 3: Assemble and link to create binary
    int result = system(compileCmd.c_str());
    if (result != 0) {
        #ifdef OS_WINDOWS
            // On Windows, try fallback to regular gcc if MinGW-w64 not found
            compileCmd = "gcc -no-pie " + tempAsm + " -o " + tempBinary + " 2>&1";
            result = system(compileCmd.c_str());
            if (result != 0) {
                throw std::runtime_error("Failed to assemble/link binary. Install MinGW-w64 or GCC.");
            }
        #else
            throw std::runtime_error("Failed to assemble/link binary. Check that GCC/Clang is installed.");
        #endif
    }
    
    // Step 4: Read the binary file
    std::ifstream binFile(tempBinary, std::ios::binary);
    if (!binFile) {
        throw std::runtime_error("Failed to read compiled binary from: " + tempBinary);
    }
    
    // Read entire binary into string
    std::string binaryData((std::istreambuf_iterator<char>(binFile)),
                           std::istreambuf_iterator<char>());
    binFile.close();
    
    // Clean up temp files
    remove(tempAsm.c_str());
    remove(tempBinary.c_str());
    
    return binaryData;
}
