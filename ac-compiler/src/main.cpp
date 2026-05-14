#include "../include/ac.hpp"
#include "acc_cache.hpp"
#include "ir_cache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #define ac_mkdir(path) _mkdir(path)
#else
  #define ac_mkdir(path) mkdir(path, 0755)
#endif

// Forward declarations
std::vector<Token> lex(const std::string& source);
NodePtr parse(const std::vector<Token>& tokens);

// Parse errors from the last parse() call (populated by parser.cpp)
struct ParseErrorRecord {
    int line, col;
    std::string message, context;
};
extern std::vector<ParseErrorRecord> g_parseErrors;

// IR-based compilation (defined in ir.cpp inside AC_IR namespace)
namespace AC_IR {
    IRProgram generateIR(const ASTNode& ast, const std::string& backend);
    std::string generateIRText(const IRProgram& program);
}

// Unified IR-based code generator (defined in ir_codegen.cpp)
std::string generateFromIR(const AC_IR::IRProgram& ir, const std::string& stem = "Main");

// Gating flag for <Foreign> raw-passthrough blocks.
bool g_allow_foreign = false;

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

static void printUsage() {
    std::cerr << "Usage: ac <file.ac> [options]\n"
              << "\n"
              << "Options:\n"
              << "  --target <backend>    Specify backend (PY, JS, C, CPP, Java, RS, GO, V, ASM, BNY)\n"
              << "  --backend <backend>   Same as --target\n"
              << "  --all, -all           Compile to all registered backends at once\n"
              << "  --force               Force recompile (ignore ac-cache/)\n"
              << "  --no-cache            Disable cache read and write entirely\n"
              << "  --allow-foreign       Enable <Foreign> raw-passthrough blocks\n"
              << "  -O0                   No optimization (currently same as default)\n"
              << "  -O1                   Basic optimization (constant folding, DCE)\n"
              << "  -O2                   Aggressive optimization (default for BNY)\n"
              << "  -O3                   Maximum optimization\n"
              << "  -g                    Include debug information (BNY backend)\n"
              << "  --stop-after-ir       Stop after IR generation; print .lir and exit\n"
              << "  --stop-after-cfg      Stop after CFG building (BNY only)\n"
              << "  --stop-after-ssa      Stop after SSA conversion (BNY only)\n"
              << "  --stop-after-opt      Stop after optimization passes (BNY only)\n"
              << "  --save-ast            Save AST to .acc (enabled by default for caching)\n"
              << "  --save-ir             Save IR to .lir (enabled by default)\n"
              << "  --version, -v         Print compiler version\n";
}

int main(int argc, char* argv[]) {
    // Check for version flag first
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "AC Compiler v0.2.0\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    // Initialize backend registry
    BackendRegistry::initializeStandardBackends();

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string inputFile;
    std::string backend;
    bool forceCompile = false;
    bool debugInfo    = false;
    int  optLevel     = 2;    // -O2 default
    bool stopAfterIR  = false;
    bool stopAfterCFG = false;
    bool stopAfterSSA = false;
    bool stopAfterOpt = false;
    bool compileAll   = false;
    bool noCache      = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--target" || arg == "-t") {
            if (i + 1 < argc) backend = argv[++i];
            else { std::cerr << "--target requires an argument\n"; return 1; }
        } else if (arg == "--backend") {
            if (i + 1 < argc) backend = argv[++i];
            else { std::cerr << "--backend requires an argument\n"; return 1; }
        } else if (arg == "--all" || arg == "-all") {
            compileAll = true;
        } else if (arg == "--force") {
            forceCompile = true;
        } else if (arg == "--allow-foreign") {
            g_allow_foreign = true;
        } else if (arg == "-g") {
            debugInfo = true;
        } else if (arg == "-O0") {
            optLevel = 0;
        } else if (arg == "-O1") {
            optLevel = 1;
        } else if (arg == "-O2") {
            optLevel = 2;
        } else if (arg == "-O3") {
            optLevel = 3;
        } else if (arg == "--stop-after-ir") {
            stopAfterIR = true;
        } else if (arg == "--stop-after-cfg") {
            stopAfterCFG = true;
        } else if (arg == "--stop-after-ssa") {
            stopAfterSSA = true;
        } else if (arg == "--stop-after-opt") {
            stopAfterOpt = true;
        } else if (arg == "--no-cache") {
            noCache = true;
            forceCompile = true; // --no-cache implies skip reading cache too
        } else if (arg == "--save-ast" || arg == "--save-ir" ||
                   arg == "-compile"   || arg == "--compile") {
            // accepted but currently default behaviour
        } else if (arg.rfind("--", 0) == 0 || (arg.rfind("-", 0) == 0 && arg.size() > 1 && !std::isdigit(arg[1]))) {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage();
            return 1;
        } else if (inputFile.empty()) {
            inputFile = arg;
        } else {
            backend = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "No input file specified.\n";
        printUsage();
        return 1;
    }

    // Suppress unused-variable warnings for flags not yet fully wired
    (void)optLevel;
    (void)stopAfterCFG; (void)stopAfterSSA; (void)stopAfterOpt;

    try {
        std::string source = readFile(inputFile);

        if (!compileAll && backend.empty()) {
            backend = detectBackend(source);
            if (backend.empty()) {
                std::cerr << "No backend found. Add 'AC->PY' (or other target) to your file, or use --all.\n";
                return 1;
            }
        }

        std::string base = inputFile.substr(0, inputFile.rfind('.'));

        // Cache directory: ac-cache/ next to the source file
        std::string srcDir;
        {
            size_t lastSlash = inputFile.find_last_of("/\\");
            srcDir = (lastSlash == std::string::npos) ? "." : inputFile.substr(0, lastSlash);
        }
        std::string cacheDir = srcDir + "/ac-cache";
        std::string baseName = base.substr(base.find_last_of("/\\") == std::string::npos ? 0 : base.find_last_of("/\\") + 1);

        // Ensure cache directory exists when caching is active
        if (!noCache) {
            struct stat st;
            if (stat(cacheDir.c_str(), &st) != 0)
                ac_mkdir(cacheDir.c_str());
        }

        std::string accFile = noCache ? "" : (cacheDir + "/" + baseName + ".acc");
        std::string lirFile = cacheDir + "/" + baseName + ".lir";
        std::string ircFile = noCache ? "" : (cacheDir + "/" + baseName + ".irc");

        NodePtr ast;

        if (!noCache && !accFile.empty() && !forceCompile && cacheIsValid(inputFile, accFile)) {
            ast = loadCache(accFile);
        }

        if (!ast) {
            auto tokens = lex(source);
            ast = parse(tokens);

            // Report collected parse errors with source context
            if (!g_parseErrors.empty()) {
                // Split source into lines once for caret display
                std::vector<std::string> srcLines;
                {
                    std::istringstream ss(source);
                    std::string ln;
                    while (std::getline(ss, ln)) srcLines.push_back(ln);
                }
                for (const auto& err : g_parseErrors) {
                    std::string errMsg = "ParseError at line " + std::to_string(err.line)
                        + " col " + std::to_string(err.col)
                        + ": " + err.message;
                    if (!err.context.empty()) errMsg += " [" + err.context + "]";
                    std::cerr << errMsg << "\n";
                    // Emit source line + caret when in range
                    int ln0 = err.line - 1; // 0-indexed
                    if (ln0 >= 0 && ln0 < (int)srcLines.size()) {
                        const std::string& sl = srcLines[ln0];
                        std::string lineNum = std::to_string(err.line);
                        std::string pad(lineNum.size(), ' ');
                        std::cerr << pad << "  |\n";
                        std::cerr << lineNum << "  | " << sl << "\n";
                        int col0 = std::max(0, err.col - 1);
                        std::cerr << pad << "  | " << std::string(col0, ' ') << "^\n";
                    }
                }
                if ((int)g_parseErrors.size() >= 10) {
                    std::cerr << "Too many parse errors. Compilation aborted.\n";
                    return 1;
                }
                std::cerr << g_parseErrors.size()
                          << " parse error(s). Attempting partial compilation.\n";
            }

            if (!noCache && !accFile.empty()) saveCache(accFile, *ast);
        }

        // ── helper: compile AST to one backend ─────────────────────────────
        auto compileOne = [&](const std::string& tgt) -> bool {
            // IR cache: hash(source + backend) → skip IR generation on hit
            AC_IR::IRProgram irProg;
            bool irFromCache = false;
            if (!noCache && !forceCompile && !ircFile.empty()) {
                uint64_t h = hashForCache(source, tgt);
                // Per-backend IRC file
                std::string tgtIrc = cacheDir + "/" + baseName + "_" + tgt + ".irc";
                auto cached = loadIRCache(tgtIrc, h);
                if (cached) {
                    irProg      = std::move(*cached);
                    irFromCache = true;
                }
                if (!irFromCache) {
                    irProg = AC_IR::generateIR(*ast, tgt);
                    saveIRCache(tgtIrc, h, irProg);
                }
            } else {
                irProg = AC_IR::generateIR(*ast, tgt);
            }

            // Save human-readable LIR (overwrite per-backend to avoid stale files)
            writeFile(lirFile, AC_IR::generateIRText(irProg));

            if (!BackendRegistry::hasBackend(tgt)) {
                std::cerr << "Preposterous: BackendError: Unknown backend: " << tgt << "\n";
                return false;
            }

            const auto& info = BackendRegistry::getBackend(tgt);

            if (tgt == "BNY") {
                std::string outFile = base + info.extension;
                if (!generateBinaryFromIR(irProg, outFile, debugInfo, inputFile)) {
                    std::cerr << "Preposterous: BackendError: Binary generation failed for BNY (Linux x86-64 only)\n";
                    return false;
                }
                std::cout << "Generated: " << outFile << " [exp_bny]\n";
                return true;
            }

            std::string outFile = base + info.extension;
            size_t slash = base.find_last_of("/\\");
            std::string stem = (slash == std::string::npos) ? base : base.substr(slash + 1);
            writeFile(outFile, generateFromIR(irProg, stem));
            std::cout << "Generated: " << outFile << "\n";
            return true;
        };

        // ── --all: compile to every registered backend ──────────────────────
        if (compileAll) {
            auto allBackends = BackendRegistry::getBackendNames();
            // Sort for deterministic output order
            std::sort(allBackends.begin(), allBackends.end());
            std::cout << "Compiling " << inputFile << " to " << allBackends.size() << " backends...\n";
            int ok = 0, fail = 0;
            for (const auto& tgt : allBackends) {
                if (compileOne(tgt)) ok++; else fail++;
            }
            std::cout << ok << " succeeded";
            if (fail) std::cout << ", " << fail << " failed";
            std::cout << "\n";
            return fail > 0 ? 1 : 0;
        }

        // ── single-backend path ─────────────────────────────────────────────
        if (stopAfterIR) {
            auto irProg = AC_IR::generateIR(*ast, backend);
            std::string lirContent = AC_IR::generateIRText(irProg);
            writeFile(lirFile, lirContent);
            std::cout << lirContent;
            return 0;
        }
        (void)ircFile; // used inside compileOne lambda

        compileOne(backend);
        return 0;

    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
