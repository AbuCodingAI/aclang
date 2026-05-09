#include "../include/token.hpp"
#include "../include/ast.hpp"
#include "../include/error.hpp"
#include "../include/backend_registry.hpp"
#include "../include/ir.hpp"
#include "../include/exp_bny.hpp"
#include "acc_cache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>

// Forward declarations
std::vector<Token> lex(const std::string& source);
NodePtr parse(const std::vector<Token>& tokens);

// IR-based compilation (defined in ir.cpp inside AC_IR namespace)
namespace AC_IR { 
    IRProgram generateIR(const ASTNode& ast, const std::string& backend);
    std::string generateIRText(const IRProgram& program);
}

// Unified IR-based code generator (defined in ir_codegen.cpp)
std::string generateFromIR(const AC_IR::IRProgram& ir);

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw FILE_ERROR("open", path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw FILE_ERROR("write", path);
    f << content;
}

static std::string detectBackend(const std::string& source) {
    size_t p = source.find("AC->");
    if (p == std::string::npos) return "";
    p += 4;
    std::string target;
    while (p < source.size() && (std::isalnum(source[p]) || source[p] == '+'))
        target += source[p++];
    return target;
}

int main(int argc, char* argv[]) {
    // Check for version flag first
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "AC Compiler v0.1.7\n";
            return 0;
        }
    }
    
    // Initialize backend registry
    BackendRegistry::initializeStandardBackends();
    
    if (argc < 2) {
        std::cerr << "Usage: ac <file.ac>\n";
        return 1;
    }

    std::string inputFile;
    std::string backend;
    bool forceCompile = false;
    bool compileOnly = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--target" || arg == "-t") {
            if (i + 1 < argc) backend = argv[++i];
            else throw RUNTIME_ERROR("--target requires an argument");
        } else if (arg == "--backend") {
            if (i + 1 < argc) backend = argv[++i];
            else throw RUNTIME_ERROR("--backend requires an argument");
        } else if (arg == "--force") {
            forceCompile = true;
        } else if (arg == "-compile" || arg == "--compile") {
            compileOnly = true;
        } else if (arg.rfind("--", 0) == 0 || arg.rfind("-", 0) == 0) {
            // unknown option
            throw BACKEND_ERROR("Unknown option: " + arg);
        } else if (inputFile.empty()) {
            inputFile = arg;
        } else {
            backend = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Usage: ac <file.ac> [--target language] [-compile] [--force]\n";
        return 1;
    }

    try {
        std::string source = readFile(inputFile);

        if (backend.empty()) {
            backend = detectBackend(source);
            if (backend.empty()) {
                std::cerr << "No backend found. Add 'AC->PY' (or other target) to your file.\n";
                return 1;
            }
        }

        std::string accFile = inputFile.substr(0, inputFile.rfind('.')) + ".acc";
        std::string lirFile = inputFile.substr(0, inputFile.rfind('.')) + ".lir";

        NodePtr ast;
        AC_IR::IRProgram irProgram;
        
        // LIR cache disabled - causes segfaults with binary serialization
        // Generate IR from AST directly
        if (!irProgram.functions.size()) {
            if (!forceCompile && cacheIsValid(inputFile, accFile)) {
                ast = loadCache(accFile);
            }
            
            // If AST cache miss, parse from source
            if (!ast) {
                auto tokens = lex(source);
                ast = parse(tokens);
                saveCache(accFile, *ast);
            }
            
            // Generate IR from AST
            irProgram = AC_IR::generateIR(*ast, backend);
            
            // Save text-based LIR (not binary cache)
            std::string lirContent = AC_IR::generateIRText(irProgram);
            writeFile(lirFile, lirContent);
        }
        
        // Generate target code from IR using unified codegen
        if (!BackendRegistry::hasBackend(backend)) {
            throw BACKEND_ERROR("Unknown backend: " + backend);
        }
        
        const auto& backendInfo = BackendRegistry::getBackend(backend);
        
        // Generate code from IR
        std::string code;
        
        if (backend == "BNY") {
            // BNY: Direct binary generation (experimental)
            // Skip ASM, .o files, and external linker entirely
            std::string outFile = inputFile.substr(0, inputFile.rfind('.')) + backendInfo.extension;
            
            if (!generateBinaryFromIR(irProgram, outFile)) {
                throw std::runtime_error("Failed to generate binary. exp_bny only supports Linux x86-64 currently.");
            }
            
            std::cout << "Generated: " << outFile << " (IR: " << lirFile << ") [exp_bny]\n";
            return 0;
        } else {
            // All other backends: use unified IR codegen
            code = generateFromIR(irProgram);
        }
        
        // Output target file
        std::string outFile = inputFile.substr(0, inputFile.rfind('.')) + backendInfo.extension;
        writeFile(outFile, code);
        
        std::cout << "Generated: " << outFile << " (IR: " << lirFile << ")\n";
        return 0;

    } catch (const std::exception& e) {
                std::cerr << e.what() << "\n";
        return 1;
    }
}
