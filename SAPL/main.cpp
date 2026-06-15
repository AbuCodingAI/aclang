/**
 * SAPL - Standard APL
 * Main entry point for the C++ interpreter
 */

#include "sapl.h"
#include <iostream>
#include <string>
#include <cstring>
#include <filesystem>

using namespace SAPL;

void printUsage(const char* programName) {
    std::cout << "SAPL Interpreter v1.0\n";
    std::cout << "Semantic Algebra Programming Language\n\n";
    std::cout << "Usage: " << programName << " [options] [file]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -r, --repl      Start interactive REPL\n";
    std::cout << "  -e, --eval      Evaluate expression from command line\n";
    std::cout << "  -h, --help      Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " program.sapl\n";
    std::cout << "  " << programName << " program        (auto-tries .sapl)\n";
    std::cout << "  " << programName << " -r\n";
    std::cout << "  " << programName << " -e \"E I 1 10\"\n";
    std::cout << "  " << programName << " -e \"factorial(x) = PI I 1 x\"\n";
    std::cout << "  " << programName << " -e \"0<- factorial(5)\"\n";
}

void printBanner() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  SAPL - Standard A Programming Language  ║\n";
    std::cout << "║        Language Interpreter v1.0         ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    SAPLInterpreter interpreter;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-r" || arg == "--repl") {
            printBanner();
            interpreter.runREPL();
            return 0;
        }
        else if (arg == "-e" || arg == "--eval") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -e requires an expression argument\n";
                return 1;
            }
            std::string expression = argv[++i];
            interpreter.run(expression);
            return 0;
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
        else {
            // Treat as filename
            std::string filename = arg;
            if (!std::filesystem::exists(filename)) {
                std::filesystem::path p(filename);
                const bool has_ext = p.has_extension();

                // If the user passed a bare name, try adding .sapl
                if (!has_ext) {
                    std::filesystem::path with_ext = p;
                    with_ext.replace_extension(".sapl");
                    if (std::filesystem::exists(with_ext)) {
                        filename = with_ext.string();
                    }
                }

                // If still missing, also try under ./SAPL (repo convenience)
                if (!std::filesystem::exists(filename)) {
                    std::filesystem::path under_sapl = std::filesystem::path("SAPL") / p;
                    if (std::filesystem::exists(under_sapl)) {
                        filename = under_sapl.string();
                    } else if (!has_ext) {
                        under_sapl.replace_extension(".sapl");
                        if (std::filesystem::exists(under_sapl)) {
                            filename = under_sapl.string();
                        }
                    }
                }
            }
            interpreter.runFile(filename);
            return 0;
        }
    }
    
    return 0;
}
