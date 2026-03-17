#include "../include/token.hpp"
#include "../include/ast.hpp"
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

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write file: " + path);
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

static std::string getRunner(const std::string& backend, const std::string& outFile) {
    if (backend == "PY")   return "python3 " + outFile;
    if (backend == "JS")   return "node " + outFile;
    if (backend == "HTML") return "xdg-open " + outFile;
    if (backend == "Java") return "javac " + outFile + " && java Main";
    if (backend == "C++")  return "g++ " + outFile + " -o /tmp/ac_out && /tmp/ac_out";
    if (backend == "C")    return "gcc " + outFile + " -o /tmp/ac_out && /tmp/ac_out";
    if (backend == "ASM")  return "gcc " + outFile + " -o /tmp/ac_out && /tmp/ac_out";
    if (backend == "GO")   return "go run " + outFile;
    if (backend == "RS")   return "rustc " + outFile + " -o /tmp/ac_out && /tmp/ac_out";
    return "";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ac <file.ac>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string backend;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc)
            backend = argv[++i];
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

        auto tokens = lex(source);
        auto ast    = parse(tokens);

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
        } else {
            std::cerr << "Backend '" << backend << "' not yet implemented.\n";
            return 1;
        }

        std::string runner = getRunner(backend, outFile);
        if (runner.empty()) {
            std::cerr << "No runner defined for backend: " << backend << "\n";
            return 1;
        }

        return std::system(runner.c_str());

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
