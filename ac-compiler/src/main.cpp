#include "../include/token.hpp"
#include "../include/ast.hpp"
#include "../include/error.hpp"
#include "../include/backend_registry.hpp"
#include "../include/ir.hpp"
#include "acc_cache.hpp"
#include "ir_cache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <sys/stat.h>

// Forward declarations
std::vector<Token> lex(const std::string& source);
NodePtr parse(const std::vector<Token>& tokens);

// IR-based compilation (defined in ir.cpp inside AC_IR namespace)
namespace AC_IR { IRProgram generateIR(const ASTNode& ast, const std::string& backend); }

// Legacy direct codegens (for fallback)
std::string generatePython(const ASTNode& ast);
std::string generateJS(const ASTNode& ast);
std::string generateHTML(const ASTNode& ast);
std::string generateJava(const ASTNode& ast);
std::string generateCpp(const ASTNode& ast);
std::string generateC(const ASTNode& ast);
std::string generateAsm(const ASTNode& ast);
std::string generateRs(const ASTNode& ast);
std::string generateGo(const ASTNode& ast);
std::string generateV(const ASTNode& ast);
std::string generateBny(const ASTNode& ast);

// IR JSON generator and codegen implementation

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
        
        // Try to load from LIR cache first (fastest path)
        if (!forceCompile && lirCacheIsValid(inputFile, lirFile, backend)) {
            irProgram = loadLIRCache(lirFile);
        }
        
        // If LIR cache miss, try AST cache
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
            
            // Save LIR cache
            saveLIRCache(lirFile, irProgram, backend);
        }
        
        // Generate target code from AST using backend
        if (!BackendRegistry::hasBackend(backend)) {
            throw BACKEND_ERROR("Unknown backend: " + backend);
        }
        
        const auto& backendInfo = BackendRegistry::getBackend(backend);
        std::string code = backendInfo.generator(*ast);
        
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
