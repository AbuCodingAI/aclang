#include "../include/token.hpp"
#include "../include/ast.hpp"
#include "../include/error.hpp"
#include "../include/backend_registry.hpp"
#include "acc_cache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>

// Forward declarations
std::vector<Token> lex(const std::string& source);
NodePtr parse(const std::vector<Token>& tokens);
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

        NodePtr ast;
        if (!forceCompile && cacheIsValid(inputFile, accFile)) {
            ast = loadCache(accFile);
        }
        if (!ast) {
            // Cache miss or stale — full lex + parse
            auto tokens = lex(source);
            ast = parse(tokens);
            saveCache(accFile, *ast);
        }

        // Use backend registry to generate code
        std::string outFile;
        if (backend == "PY") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".py";
            writeFile(outFile, generatePython(*ast));
        } else if (backend == "JS") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".js";
            writeFile(outFile, generateJS(*ast));
        } else if (backend == "HTML") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".html";
            writeFile(outFile, generateHTML(*ast));
        } else if (backend == "Java") {
            outFile = "Main.java";
            writeFile(outFile, generateJava(*ast));
        } else if (backend == "C++") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".cpp";
            writeFile(outFile, generateCpp(*ast));
        } else if (backend == "C") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".c";
            writeFile(outFile, generateC(*ast));
        } else if (backend == "ASM") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".s";
            writeFile(outFile, generateAsm(*ast));
        } else if (backend == "RS") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".rs";
            writeFile(outFile, generateRs(*ast));
        } else if (backend == "GO") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".go";
            writeFile(outFile, generateGo(*ast));
        } else if (backend == "V") {
            outFile = inputFile.substr(0, inputFile.rfind('.')) + ".v";
            writeFile(outFile, generateV(*ast));
        } else {
            std::cerr << "Backend '" << backend << "' not yet implemented.\n";
            return 1;
        }

        if (compileOnly) {
            std::cout << "Compiled " << outFile << " (compile-only mode)\n";
            return 0;
        }

        std::string runner;
        if (backend == "PY")   runner = "python3 " + outFile;
        else if (backend == "JS")   runner = "node " + outFile;
        else if (backend == "HTML") runner = "xdg-open " + outFile;
        else if (backend == "Java") runner = "javac " + outFile + " && java Main";
        else if (backend == "C++")  runner = "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out";
        else if (backend == "C")    runner = "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out";
        else if (backend == "ASM")  runner = "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out";
        else if (backend == "RS")   runner = "rustc " + outFile + " -o /tmp/ac_out && /tmp/ac_out";
        else if (backend == "GO")   runner = "go run " + outFile;
        else if (backend == "V")    runner = "cd /tmp/v && /usr/local/bin/v run " + outFile;

        if (runner.empty()) {
            std::cerr << "No runner defined for backend: " << backend << "\n";
            return 1;
        }

        return std::system(runner.c_str());

    } catch (const std::exception& e) {
                std::cerr << e.what() << "\n";
        return 1;
    }
}
