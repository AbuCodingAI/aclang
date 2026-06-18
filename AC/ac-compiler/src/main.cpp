#include "../include/ac.hpp"
#include "acc_cache.hpp"
#include "ir_cache.hpp"
#include "lib_lower.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#ifndef _WIN32
  #include <unistd.h>
  #include <cstring>
#endif

#ifdef _WIN32
  #include <direct.h>
  #define ac_mkdir(path) _mkdir(path)
#else
  #define ac_mkdir(path) mkdir(path, 0755)
#endif

// Forward declarations
std::vector<Token> lex(const std::string& source);
NodePtr parse(const std::vector<Token>& tokens);

// Build a string of FFI file mtimes for any "use ilib X" imports in source.
// This makes the IR cache invalidate when a library's FFI file changes.
static std::string ffiMtimesSuffix(const std::string& source, const std::string& backend) {
    std::string result;
    // Quick scan: find "ilib " or "header " tokens followed by a library name
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        std::string libName;
        auto tryExtract = [&](const std::string& kw) {
            auto pos = line.find(kw);
            if (pos == std::string::npos) return;
            pos += kw.size();
            while (pos < line.size() && line[pos] == ' ') pos++;
            std::string name;
            while (pos < line.size() && line[pos] != ' ' && line[pos] != '\n' && line[pos] != '\r') {
                name += line[pos++];
            }
            if (!name.empty()) libName = name;
        };
        tryExtract("ilib ");
        if (libName.empty()) tryExtract("header ");
        if (libName.empty()) continue;
        // Get mtime of the FFI file for this backend
        std::string ext = backend;
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        std::string ffiPath = "./library/ilib/" + libName + "/ffi/" + libName + "_ffi." + ext;
        struct stat st{};
        if (stat(ffiPath.c_str(), &st) == 0) {
            result += ffiPath + ":" + std::to_string((long long)st.st_mtime) + ";";
        }
        // Also check the .acl file
        std::string aclPath = "./library/ilib/" + libName + "/" + libName + ".acl";
        if (stat(aclPath.c_str(), &st) == 0) {
            result += aclPath + ":" + std::to_string((long long)st.st_mtime) + ";";
        }
    }
    return result;
}

// Parse errors from the last parse() call (populated by parser.cpp)
struct ParseErrorRecord {
    int line, col;
    std::string message, context;
};
extern std::vector<ParseErrorRecord> g_parseErrors;

// IR-based compilation (defined in ir.cpp inside AC_IR namespace)
namespace AC_IR {
    IRProgram generateIR(const ASTNode& ast, const std::string& backend, bool runtimeMode);
    std::string generateIRText(const IRProgram& program);
}

// Unified IR-based code generator (defined in ir_codegen.cpp)
std::string generateFromIR(const AC_IR::IRProgram& ir, const std::string& stem = "Main",
                           const std::string& outputBase = "");

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

// Sentinel returned for "AC LIB" — source-only, cannot be compiled directly.
static constexpr const char* BACKEND_AC_LIB_NOCOMPILE = "__AC_LIB__";

static std::string detectBackend(const std::string& source) {
    size_t p = source.find("AC->");
    if (p != std::string::npos) {
        p += 4;
        std::string target;
        while (p < source.size() && (std::isalnum(source[p]) || source[p] == '+'))
            target += source[p++];
        return target;  // e.g. "LIB" for AC->LIB, "PY" for AC->PY, etc.
    }
    // "AC LIB" = source-only library; cannot be compiled directly (import via flib).
    if (source.find("AC LIB") != std::string::npos)
        return BACKEND_AC_LIB_NOCOMPILE;
    return "";
}

static void printUsage() {
    std::cerr << "Usage: ac <file.ac> [options]\n"
              << "\n"
              << "Options:\n"
              << "  --target <backend>    Specify backend (PY, JS, C, CPP, Java, RS, GO, V, ASM, BNY, LIB)\n"
              << "  --backend <backend>   Same as --target\n"
              << "  --all, -all           Compile to all registered backends at once\n"
              << "  --no-run              Compile only; do not run the output\n"
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
              << "  --time, -time         Time compilation and execution separately\n"
              << "  --version, -v         Print compiler version\n";
}

// Inject functions and bundles from flib .ac/.ai modules into the AST.
// .ac/.ai flib files share AC syntax, so they can be parsed directly.
// Non-.ac/.ai flib entries are left unchanged for backend-native import.
static void injectFlibModules(ASTNode& root, const std::string& srcDir) {
    NodeList toAppend;
    for (auto& child : root.children) {
        if (!child || child->type != NodeType::UseLibStmt) continue;
        const std::string& val = child->value;
        if (val.rfind("flib:", 0) != 0) continue;
        if (val.rfind("flib:__inlined__:", 0) == 0) continue;
        std::string libpath = val.substr(5);
        auto dot = libpath.rfind('.');
        if (dot == std::string::npos) continue;
        std::string ext = libpath.substr(dot);
        if (ext != ".ac" && ext != ".ai") continue;

        // Resolve path relative to srcDir (absolute paths kept as-is)
        std::string fullPath = (!libpath.empty() && libpath[0] == '/')
            ? libpath : (srcDir + "/" + libpath);

        std::ifstream ff(fullPath);
        if (!ff) {
            std::cerr << "Preposterous: FlibError: cannot open flib file: " << fullPath << "\n";
            continue;
        }
        std::ostringstream buf;
        buf << ff.rdbuf();

        auto flibTokens = lex(buf.str());
        auto flibAst    = parse(flibTokens);
        if (!flibAst) continue;

        // Resolve the flib file's own srcDir so nested flib imports are
        // relative to the flib file's location, not the original source file.
        std::string flibDir = fullPath;
        auto slash = flibDir.find_last_of("/\\");
        flibDir = (slash == std::string::npos) ? "." : flibDir.substr(0, slash);

        // Recursively inject any flib imports inside the flib file
        injectFlibModules(*flibAst, flibDir);

        // Collect FuncDef, BundleDef, and resolved UseLibStmt nodes
        for (auto& node : flibAst->children) {
            if (!node) continue;
            if (node->type == NodeType::FuncDef  ||
                node->type == NodeType::BundleDef ||
                node->type == NodeType::UseLibStmt)
                toAppend.push_back(std::move(node));
        }

        child->value = "flib:__inlined__:" + libpath;
    }
    for (auto& e : toAppend)
        root.children.push_back(std::move(e));
}

// ── .datac file parser ──────────────────────────────────────────────────────
// Parses a .datac file and returns rows as lists of (key, formatted-value) pairs.
// Formatted values: strings wrapped in $...$, numbers/booleans left bare.
struct DatacRow { std::vector<std::pair<std::string,std::string>> fields; };

static std::string datac_fmtval(const std::string& v) {
    if (v.empty()) return "$$";
    // already quoted?
    if (v.front() == '"' || v.front() == '\'') {
        std::string inner = v.substr(1, v.size() - (v.size() > 1 ? 2 : 1));
        return "$" + inner + "$";
    }
    // pure number?
    bool isNum = !v.empty();
    for (char c : v) if (!std::isdigit((unsigned char)c) && c != '.' && c != '-') { isNum = false; break; }
    if (isNum) return v;
    // bare word (true/false/null/variable)
    return "$" + v + "$";
}

static std::vector<DatacRow> parseDatacRows(const std::string& content) {
    std::vector<DatacRow> rows;
    std::istringstream ss(content);
    std::string line;
    bool inBlock = false;
    // accumulated partial-row buffer (rows may be multi-line)
    std::string rowBuf;

    auto flushRow = [&]() {
        if (rowBuf.empty()) return;
        DatacRow row;
        // split on commas, but track quote depth
        std::vector<std::string> parts;
        std::string cur;
        bool inQ = false;
        for (char c : rowBuf) {
            if (c == '"' || c == '\'') { inQ = !inQ; cur += c; }
            else if (c == ',' && !inQ) { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) parts.push_back(cur);
        for (auto& p : parts) {
            auto colon = p.find(':');
            if (colon == std::string::npos) continue;
            std::string k = p.substr(0, colon);
            std::string v = p.substr(colon + 1);
            // trim whitespace
            auto trim = [](std::string& s) {
                auto b = s.find_first_not_of(" \t\r\n");
                auto e = s.find_last_not_of(" \t\r\n");
                s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
            };
            trim(k); trim(v);
            if (!k.empty()) row.fields.push_back({k, datac_fmtval(v)});
        }
        if (!row.fields.empty()) rows.push_back(row);
        rowBuf.clear();
    };

    while (std::getline(ss, line)) {
        // trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\t'))
            line.pop_back();
        if (!inBlock) {
            // look for "tablename {" — skip schema lines (class/int/string sub. lines)
            auto lb = line.find('{');
            if (lb != std::string::npos) {
                inBlock = true;
                // check if there's content on the same line after {
                std::string rest = line.substr(lb + 1);
                while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(rest.begin());
                if (!rest.empty() && rest.front() != '}') rowBuf = rest;
            }
        } else {
            if (line == "}") { flushRow(); inBlock = false; continue; }
            if (line.empty()) { flushRow(); continue; }
            // append to current row buffer; detect row boundary by leading key: pattern
            // A new row starts when the line contains a key that matches the primary key field
            // Simplest heuristic: flush if rowBuf already has the primary key field
            if (!rowBuf.empty() && !line.empty()) {
                // Check if this line starts a new row (has a colon not preceded by quote)
                auto firstColon = line.find(':');
                if (firstColon != std::string::npos) {
                    std::string firstKey = line.substr(0, firstColon);
                    while (!firstKey.empty() && (firstKey.front() == ' ' || firstKey.front() == '\t'))
                        firstKey.erase(firstKey.begin());
                    // If rowBuf already contains this key, flush the old row
                    if (!rowBuf.empty() && rowBuf.find(firstKey + ":") != std::string::npos)
                        flushRow();
                }
                if (!rowBuf.empty()) rowBuf += ",";
            }
            rowBuf += line;
        }
    }
    flushRow();
    return rows;
}

static void injectDatacImports(ASTNode& root, const std::string& srcDir) {
    for (size_t i = 0; i < root.children.size(); i++) {
        auto& child = root.children[i];
        if (!child || child->type != NodeType::UseLibStmt) continue;
        const std::string& val = child->value;
        if (val.rfind("datac:", 0) != 0) continue;

        // val = "datac:<filepath>:<alias>"
        std::string rest = val.substr(6);
        auto sep = rest.rfind(':');
        if (sep == std::string::npos) continue;
        std::string filepath = rest.substr(0, sep);
        std::string alias    = rest.substr(sep + 1);

        std::string fullPath = (!filepath.empty() && filepath[0] == '/')
            ? filepath : (srcDir + "/" + filepath);

        std::ifstream ff(fullPath);
        if (!ff) {
            std::cerr << "Preposterous: DatacError: cannot open datac file: " << fullPath << "\n";
            continue;
        }
        std::ostringstream buf;
        buf << ff.rdbuf();

        auto rows = parseDatacRows(buf.str());

        // Build replacement nodes: N dict assignments + 1 list assignment
        NodeList replacements;
        std::string listContent;

        for (size_t r = 0; r < rows.size(); r++) {
            std::string rowVar = "_dc_" + alias + "_" + std::to_string(r);
            // Build __dict__ content string: key:$val$,key2:42,...
            std::string dictContent;
            for (size_t f = 0; f < rows[r].fields.size(); f++) {
                if (f) dictContent += ",";
                dictContent += rows[r].fields[f].first + ":" + rows[r].fields[f].second;
            }
            auto rowNode = std::make_unique<ASTNode>(NodeType::AssignStmt, rowVar);
            rowNode->attrs.push_back("__dict__" + dictContent);
            replacements.push_back(std::move(rowNode));

            if (r) listContent += ", ";
            listContent += rowVar;
        }

        // Final list assignment: alias = [_dc_alias_0, _dc_alias_1, ...]
        auto listNode = std::make_unique<ASTNode>(NodeType::AssignStmt, alias);
        listNode->attrs.push_back("__list__" + listContent);
        replacements.push_back(std::move(listNode));

        // Replace the datac UseLibStmt node with the expanded nodes
        root.children.erase(root.children.begin() + (long)i);
        for (size_t k = 0; k < replacements.size(); k++)
            root.children.insert(root.children.begin() + (long)(i + k), std::move(replacements[k]));
        i += replacements.size() - 1; // skip over newly inserted nodes
    }
}

int main(int argc, char* argv[]) {
    // Check for version flag first
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "AC Compiler v0.3.1\n";
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
    bool forceCompile    = false;
    bool debugInfo       = false;
    int  optLevel        = 2;    // -O2 default
    bool stopAfterIR     = false;
    bool stopAfterCFG    = false;
    bool stopAfterSSA    = false;
    bool stopAfterOpt    = false;
    bool compileAll      = false;
    bool noCache         = false;
    bool runAfterCompile = true;  // default: compile + run
    bool doTime          = false;
    bool runtimeMode     = false; // --runtime: disable constexpr folding

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
        } else if (arg == "--time" || arg == "-time") {
            doTime = true;
        } else if (arg == "--runtime") {
            runtimeMode = true; // disable constexpr folding — benchmark actual runtime
        } else if (arg == "--no-run") {
            runAfterCompile = false;
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
            if (backend == BACKEND_AC_LIB_NOCOMPILE) {
                std::cerr << "Preposterous: CompileError: 'AC LIB' files cannot be compiled directly.\n"
                          << "  'AC LIB' marks a source-only library — import it with:\n"
                          << "      use flib <path/to/file.ac>\n"
                          << "  To build a shared library that other languages can load, use 'AC->LIB' instead.\n";
                return 1;
            }
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
        std::string lirFile = noCache ? "" : (cacheDir + "/" + baseName + ".lir");
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
                    std::string errMsg = "Preposterous: ParseError at line " + std::to_string(err.line)
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
                (void)g_parseErrors.size();
            }

            if (!noCache && !accFile.empty() && g_parseErrors.empty()) saveCache(accFile, *ast);
        }

        // Inject .ac/.ai flib modules into the AST before IR generation
        injectFlibModules(*ast, srcDir);
        // Bake .datac files into the AST as list-of-dict variable assignments
        injectDatacImports(*ast, srcDir);

        // ── helper: compile AST to one backend ─────────────────────────────
        // Ensure a compiled binary path can be invoked (needs ./ on Linux for relative paths)
        auto execPath = [](const std::string& p) -> std::string {
#ifndef _WIN32
            if (p.find('/') == std::string::npos) return "./" + p;
#endif
            return p;
        };

        auto compileOne = [&](const std::string& tgt) -> bool {
            using Clock = std::chrono::steady_clock;
            auto tStart = Clock::now();
            Clock::time_point tRunStart = tStart, tRunEnd = tStart;
            bool ran = false;

            auto timedRun = [&](const std::string& cmd) {
                tRunStart = Clock::now();
                std::system(cmd.c_str());
                tRunEnd = Clock::now();
                ran = true;
            };

            auto printTiming = [&]() {
                if (!doTime) return;
                auto tEnd = Clock::now();
                double compSec = ran
                    ? std::chrono::duration<double>(tRunStart - tStart).count()
                    : std::chrono::duration<double>(tEnd    - tStart).count();
                double runSec = ran
                    ? std::chrono::duration<double>(tRunEnd - tRunStart).count()
                    : 0.0;
                std::cout << "===Compilation time: " << std::fixed << std::setprecision(2)
                          << compSec << "s, Run time: " << runSec << "s===\n";
            };

            // IR cache: hash(source + backend) → skip IR generation on hit
            AC_IR::IRProgram irProg;
            bool irFromCache = false;
            if (!noCache && !forceCompile && !ircFile.empty()) {
                std::string irHashSource = source + ffiMtimesSuffix(source, tgt)
                                         + "\nruntime=" + (runtimeMode ? "1" : "0");
                uint64_t h = hashForCache(irHashSource, tgt);
                // Per-backend IRC file
                std::string tgtIrc = cacheDir + "/" + baseName + "_" + tgt + ".irc";
                auto cached = loadIRCache(tgtIrc, h);
                if (cached) {
                    irProg      = std::move(*cached);
                    irFromCache = true;
                }
                if (!irFromCache) {
                    irProg = AC_IR::generateIR(*ast, tgt, runtimeMode);
                    saveIRCache(tgtIrc, h, irProg);
                }
            } else {
                irProg = AC_IR::generateIR(*ast, tgt, runtimeMode);
            }

            // Library lowering pass: rewrite lib:* IR calls to ac_* before codegen
            {
                std::string libRoot = "./library";
#ifndef _WIN32
                char exeBuf[4096] = {};
                ssize_t elen = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf)-1);
                if (elen > 0) {
                    exeBuf[elen] = '\0';
                    std::string bd(exeBuf);
                    auto sl = bd.rfind('/');
                    if (sl != std::string::npos) bd = bd.substr(0, sl);
                    libRoot = bd + "/../library";
                }
#endif
                AC_IR::LibLowering lowering;
                lowering.load(libRoot + "/ilib/gl/gl.acl");
                lowering.load(libRoot + "/ilib/math/math.acl");
                lowering.load(libRoot + "/ilib/camera/camera.acl");
                lowering.load(libRoot + "/ilib/machine-audio/machine-audio.acl");
                lowering.load(libRoot + "/ilib/widgets/widgets.acl");
                lowering.load(libRoot + "/ilib/regex/regex.acl");
                lowering.load(libRoot + "/ilib/os/os.acl");
                lowering.load(libRoot + "/ilib/string-cheese/string-cheese.acl");
                lowering.load(libRoot + "/ilib/pointers/pointers.acl");
                lowering.apply(irProg);
            }

            // Save human-readable LIR — only for low-level backends (BNY/ASM) where it aids debugging
            // Higher-level backends (PY, JS, C++, etc.) don't benefit from the LIR text dump
            bool saveLir = (tgt == "BNY" || tgt == "ASM");
            if (!lirFile.empty() && saveLir)
                writeFile(lirFile, AC_IR::generateIRText(irProg));

            // Enforce entry point: non-LIB programs must have <mainloop> or <StartHere>
            if (tgt != "LIB" && !irProg.hadExplicitMainloop) {
                // Check if there are executable statements outside function definitions
                bool hasExecCode = false;
                for (const auto& ins : irProg.globalInit) {
                    if (ins.opcode == AC_IR::IROpcode::LIB_CALL) continue; // imports OK
                    hasExecCode = true;
                    break;
                }
                if (hasExecCode) {
                    std::cerr << "Preposterous: EntryPointError: Executable code found without a <mainloop> entry point.\n"
                              << "  Add a <mainloop> block to make this an executable program, or\n"
                              << "  change the header to 'AC LIB' / 'AC->LIB' for a library file.\n";
                    return false;
                }
            }

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
#ifndef _WIN32
                chmod(outFile.c_str(), 0755);
#endif
                if (runAfterCompile && !compileAll) {
                    // Locate AC library root (same logic as ir_codegen FFI search)
                    std::string libRoot = "./library";
#ifndef _WIN32
                    char exeBuf[4096] = {};
                    ssize_t elen = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf)-1);
                    if (elen > 0) {
                        exeBuf[elen] = '\0';
                        std::string bd(exeBuf);
                        auto sl = bd.rfind('/');
                        if (sl != std::string::npos) bd = bd.substr(0, sl);
                        libRoot = bd + "/../library";
                    }
                    if (!doTime) {
                        // Replace the ac process with the compiled binary directly.
                        // This propagates the exit code and keeps the process tree clean.
                        std::string newLdPath = libRoot + "/ilib/math:" + libRoot + "/ilib/camera";
                        const char* existing = getenv("LD_LIBRARY_PATH");
                        if (existing && strlen(existing)) newLdPath += std::string(":") + existing;
                        setenv("LD_LIBRARY_PATH", newLdPath.c_str(), 1);
                        char* argv0 = const_cast<char*>(outFile.c_str());
                        char* const exec_argv[] = { argv0, nullptr };
                        execv(outFile.c_str(), exec_argv);
                        // execv only returns on error — fall through to system() below
                    }
#endif
                    std::string runCmd = "LD_LIBRARY_PATH=\"" + libRoot + "/math:" + libRoot + "/camera:${LD_LIBRARY_PATH}\" \"" + outFile + "\"";
                    timedRun(runCmd);
                }
                printTiming();
                return true;

            // ── AC->ASM: Generate ARM Assembly (C->GAS workflow) ──────────────────
            if (tgt == "ASM") {
                // Detect ARM at runtime
                bool isARM = false;
                #if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
                isARM = true;
                #endif
                
                if (isARM) {
                    std::cerr << "[AC->ASM] Detected ARM - generating assembly\n";
                    
                    std::string cFile = base + ".c";
                    std::string asmFile = base + ".s";
                    
                    // Generate C code
                    std::string cContent = generateFromIR(irProg, base, base);
                    writeFile(cFile, cContent);
                    std::cout << "Generated: " << cFile << " [C intermediary]\n";
                    
                    // Compile C to assembly
                    std::string compiler = "gcc";
                    #ifdef __APPLE__
                    compiler = "clang";
                    #endif
                    
                    std::string cmd = compiler + " -S -o " + asmFile + " " + cFile;
                    std::cerr << "[AC->ASM] " << cmd << "\n";
                    
                    if (std::system(cmd.c_str()) != 0) {
                        std::cerr << "Toxic: Assembly generation failed\n";
                        return false;
                    }
                    
                    std::remove(cFile.c_str());
                    std::cout << "Generated: " << asmFile << " [ARM assembly]\n";
                    std::cout << "Tip: gcc " << asmFile << " -o " << base << "\n";
                    printTiming();
                    return true;
                } else {
                    std::cerr << "Toxic: AC->ASM on x86 not implemented\n";
                    std::cerr << "Suggestion: Use AC->BNY for native binary\n";
                    return false;
                }
            }
            }

            std::string outFile = base + info.extension;
            size_t slash = base.find_last_of("/\\");
            std::string stem = (slash == std::string::npos) ? base : base.substr(slash + 1);
            std::string content = generateFromIR(irProg, stem, base);
            writeFile(outFile, content);
            std::cout << "Generated: " << outFile << "\n";

            bool doRun = runAfterCompile && !compileAll;
            if (doRun) std::cout << std::flush;

            // ── Interpreted / JIT backends: run directly ──────────────────────
            if (tgt == "PY") {
                if (doRun) timedRun("python3 \"" + outFile + "\"");
                printTiming();
                return true;
            }
            if (tgt == "JS") {
                if (doRun) timedRun("node \"" + outFile + "\"");
                printTiming();
                return true;
            }
            if (tgt == "GO") {
                if (doRun) timedRun("go run \"" + outFile + "\"");
                printTiming();
                return true;
            }
            if (tgt == "V") {
                if (doRun) timedRun("v run \"" + outFile + "\"");
                printTiming();
                return true;
            }

            // ── C: compile with gcc then run ──────────────────────────────────
            if (tgt == "C") {
                std::string linkFlags;
                {
                    std::istringstream ss(content);
                    std::string line;
                    while (std::getline(ss, line)) {
                        const std::string prefix = "// Link: gcc ";
                        if (line.rfind(prefix, 0) == 0) {
                            std::string rest = line.substr(prefix.length());
                            auto sp = rest.find(' ');
                            if (sp != std::string::npos) linkFlags = rest.substr(sp + 1);
                            break;
                        }
                    }
                }
                char cwdbuf[4096];
                std::string cwd;
#ifdef _WIN32
                if (_getcwd(cwdbuf, sizeof(cwdbuf))) cwd = cwdbuf;
#else
                if (getcwd(cwdbuf, sizeof(cwdbuf))) cwd = cwdbuf;
#endif
                std::string binFile = base;
                // linkFlags already contains absolute -L and -Wl,-rpath from the codegen
                std::string gccCmd = "gcc \"" + outFile + "\" -I. " + linkFlags
                                     + " -o \"" + binFile + "\"";
                int rc = std::system(gccCmd.c_str());
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [gcc]\n";
                    if (doRun) timedRun("\"" + execPath(binFile) + "\"");
                } else {
                    std::cerr << "Warning: gcc compilation failed (exit " << rc << ")\n";
                }
                printTiming();
                return true;
            }

            // ── C++: compile with g++ then run ───────────────────────────────
            if (tgt == "C++" || tgt == "CPP") {
                std::string binFile = base;
                // Parse FLIB_SO_LINK directives: link .so files directly by path
                std::string flibLinkFlags;
                {
                    std::istringstream ss(content);
                    std::string line;
                    while (std::getline(ss, line)) {
                        const std::string prefix = "// FLIB_SO_LINK: ";
                        if (line.rfind(prefix, 0) == 0) {
                            std::string soPath = line.substr(prefix.size());
                            // Resolve path relative to srcDir if not absolute
                            if (!soPath.empty() && soPath[0] != '/')
                                soPath = srcDir + "/" + soPath;
                            flibLinkFlags += " \"" + soPath + "\"";
                        }
                    }
                }
                // Parse "// Link: g++ " directives (ilib libraries like gl)
                std::string glinkFlags;
                {
                    std::istringstream ss(content);
                    std::string line;
                    while (std::getline(ss, line)) {
                        const std::string pfx = "// Link: g++ ";
                        if (line.rfind(pfx, 0) == 0) {
                            std::string rest = line.substr(pfx.size());
                            auto sp = rest.find(' ');
                            if (sp != std::string::npos) glinkFlags += " " + rest.substr(sp + 1);
                        }
                    }
                }
                char cwdbuf[4096]; std::string cwd;
#ifdef _WIN32
                if (_getcwd(cwdbuf, sizeof(cwdbuf))) cwd = cwdbuf;
#else
                if (getcwd(cwdbuf, sizeof(cwdbuf))) cwd = cwdbuf;
#endif
                std::string cmd = "g++ -std=c++17 -fpermissive -I. \"" + outFile + "\"" + flibLinkFlags + glinkFlags + " -o \"" + binFile + "\"";
                if (!cwd.empty()) {
                    std::istringstream lf(glinkFlags);
                    std::string tok;
                    while (lf >> tok) {
                        if (tok.rfind("-L", 0) == 0) {
                            std::string libpath = tok.substr(2);
                            if (libpath.rfind("./", 0) == 0) libpath = libpath.substr(2);
                            if (!libpath.empty() && libpath[0] != '/')
                                libpath = cwd + "/" + libpath;
                            cmd += " -Wl,-rpath,\"" + libpath + "\"";
                        }
                    }
                }
                int rc = std::system(cmd.c_str());
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [g++]\n";
                    if (doRun) timedRun("\"" + execPath(binFile) + "\"");
                } else {
                    std::cerr << "Warning: g++ compilation failed (exit " << rc << ")\n";
                }
                printTiming();
                return true;
            }

            // ── LIB: compile to shared library (.so / .dll) ───────────────────
            if (tgt == "LIB") {
                // Parse FLIB_SO_LINK directives from generated source
                std::string flibLinkFlags;
                {
                    std::istringstream ss(content);
                    std::string line;
                    while (std::getline(ss, line)) {
                        const std::string prefix = "// FLIB_SO_LINK: ";
                        if (line.rfind(prefix, 0) == 0) {
                            std::string soPath = line.substr(prefix.size());
                            if (!soPath.empty() && soPath[0] != '/')
                                soPath = srcDir + "/" + soPath;
                            flibLinkFlags += " \"" + soPath + "\"";
                        }
                    }
                }
                // Parse "// Link: g++ " directives (ilib libraries like gl)
                std::string glinkFlags;
                {
                    std::istringstream ss(content);
                    std::string line;
                    while (std::getline(ss, line)) {
                        const std::string pfx = "// Link: g++ ";
                        if (line.rfind(pfx, 0) == 0) {
                            std::string rest = line.substr(pfx.size());
                            auto sp = rest.find(' ');
                            if (sp != std::string::npos) glinkFlags += " " + rest.substr(sp + 1);
                        }
                    }
                }
#ifdef _WIN32
                std::string soFile = base + ".dll";
#else
                std::string soFile = base + ".so";
#endif
                std::string cmd = "g++ -std=c++17 -fpermissive -I. -shared -fPIC \"" + outFile + "\"" + flibLinkFlags + glinkFlags + " -o \"" + soFile + "\"";
                int rc = std::system(cmd.c_str());
                if (rc == 0) {
                    std::cout << "Compiled:  " << soFile << " [shared lib]\n";
                    // Generate companion .h header with extern "C" declarations
                    std::string hFile = base + ".h";
                    std::ostringstream hdr;
                    hdr << "#pragma once\n";
                    hdr << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
                    for (const auto& fn : irProg.functions) {
                        if (!fn.classOwner.empty()) continue;
                        hdr << "long long " << fn.name << "(";
                        for (size_t pi = 0; pi < fn.parameters.size(); pi++) {
                            if (pi) hdr << ", ";
                            hdr << "long long " << fn.parameters[pi];
                        }
                        hdr << ");\n";
                    }
                    hdr << "#ifdef __cplusplus\n}\n#endif\n";
                    writeFile(hFile, hdr.str());
                    std::cout << "Generated: " << hFile << " [lib header]\n";
                } else {
                    std::cerr << "Warning: LIB compilation failed (exit " << rc << ")\n";
                }
                printTiming();
                return true;
            }

            // ── Rust: compile with rustc then run ─────────────────────────────
            if (tgt == "RS") {
                std::string binFile = base;
                // Detect ilib libraries from generated source and add link paths
                std::string libFlags;
                std::string libRoot = "./library";
#ifndef _WIN32
                char rsExeBuf[4096] = {};
                ssize_t rsLen = readlink("/proc/self/exe", rsExeBuf, sizeof(rsExeBuf)-1);
                if (rsLen > 0) {
                    rsExeBuf[rsLen] = '\0';
                    std::string bd(rsExeBuf);
                    auto sl2 = bd.rfind('/');
                    if (sl2 != std::string::npos) bd = bd.substr(0, sl2);
                    libRoot = bd + "/../library";
                }
#endif
                if (content.find("#[link(name = \"acmath\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/math\" -l acmath";
                if (content.find("#[link(name = \"accamera\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/camera\" -l accamera";
                if (content.find("#[link(name = \"acwidgets\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/widgets\" -l acwidgets";
                if (content.find("#[link(name = \"acregex\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/regex\" -l acregex";
                // Parse FLIB_SO_LINK for user-provided .so files
                {
                    std::istringstream ss2(content);
                    std::string fline;
                    while (std::getline(ss2, fline)) {
                        const std::string pfx = "// FLIB_SO_LINK: ";
                        if (fline.rfind(pfx, 0) == 0) {
                            std::string soPath = fline.substr(pfx.size());
                            if (!soPath.empty() && soPath[0] != '/')
                                soPath = srcDir + "/" + soPath;
                            auto sl = soPath.rfind('/');
                            std::string ld = (sl == std::string::npos) ? "." : soPath.substr(0, sl);
                            std::string bn = (sl == std::string::npos) ? soPath : soPath.substr(sl + 1);
                            std::string lname = bn.substr(0, bn.rfind('.'));
                            if (lname.rfind("lib", 0) == 0) lname = lname.substr(3);
                            libFlags += " -L \"" + ld + "\" -l " + lname;
                        }
                    }
                }
                std::string cmd = "rustc \"" + outFile + "\" -o \"" + binFile + "\"" + libFlags;
                int rc = std::system(cmd.c_str());
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [rustc]\n";
                    if (doRun) {
                        std::string runCmd = "LD_LIBRARY_PATH=\"" + libRoot + "/ilib/math:" + libRoot + "/ilib/camera:" + libRoot + "/ilib/widgets:" + libRoot + "/ilib/regex:${LD_LIBRARY_PATH}\" \"" + execPath(binFile) + "\"";
                        timedRun(runCmd);
                    }
                } else {
                    std::cerr << "Warning: rustc compilation failed (exit " << rc << ")\n";
                }
                printTiming();
                return true;
            }

            // ── Java: compile with javac then run ─────────────────────────────
            if (tgt == "Java") {
                std::string javaDir = ".";
                size_t sl = outFile.rfind('/');
                if (sl != std::string::npos) javaDir = outFile.substr(0, sl);
                int rc = std::system(("javac --enable-preview --release 21 \"" + outFile + "\"").c_str());
                if (rc == 0) {
                    std::cout << "Compiled:  " << stem << ".class [javac]\n";
                    if (doRun)
                        timedRun("java --enable-preview -cp \"" + javaDir + "\" " + stem);
                } else {
                    std::cerr << "Warning: javac compilation failed (exit " << rc << ")\n";
                }
                printTiming();
                return true;
            }

            // ── HTML / ASM: generate only, no CLI runner ──────────────────────
            printTiming();
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
                try {
                    if (compileOne(tgt)) ok++; else fail++;
                } catch (const std::exception& ex) {
                    std::cerr << tgt << ": " << ex.what() << "\n";
                    fail++;
                }
            }
            std::cout << ok << " succeeded";
            if (fail) std::cout << ", " << fail << " failed";
            std::cout << "\n";
            return fail > 0 ? 1 : 0;
        }

        // ── single-backend path ─────────────────────────────────────────────
        if (stopAfterIR) {
            auto irProg = AC_IR::generateIR(*ast, backend, runtimeMode);
            std::string lirContent = AC_IR::generateIRText(irProg);
            if (!lirFile.empty()) writeFile(lirFile, lirContent);
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
