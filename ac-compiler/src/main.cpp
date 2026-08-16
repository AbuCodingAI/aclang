#include "../include/ac.hpp"
#include "acc_cache.hpp"
#include "ir_cache.hpp"
#include "lib_lower.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <sys/stat.h>
#ifndef _WIN32
  #include <unistd.h>
  #include <cstring>
  #include <sys/wait.h>
#endif

#ifdef _WIN32
  #include <direct.h>
  #include <windows.h>
  #include <process.h>
  #define ac_mkdir(path) _mkdir(path)
#else
  #define ac_mkdir(path) mkdir(path, 0755)
#endif
#include <utility>

// ── Shell-free process execution ────────────────────────────────────────────
// Run a program via an argv array — NO shell — so a path/flag containing
// $(...), backticks, ;, |, & etc. can never be interpreted as a command. This is
// the cross-platform, injection-proof way to invoke the toolchain: a malicious
// `.ac` doing `use flib "/x/$(rm -rf ~).so"` is just a (nonexistent) filename here,
// on Linux, macOS, AND Windows — no per-shell escaping rules to get wrong.
static int run_argv(const std::vector<std::string>& args,
                    const std::vector<std::pair<std::string,std::string>>& extraEnv = {}) {
    if (args.empty()) return -1;
#ifdef _WIN32
    for (const auto& e : extraEnv) _putenv_s(e.first.c_str(), e.second.c_str());
    std::vector<const char*> cargv;
    for (const auto& a : args) cargv.push_back(a.c_str());
    cargv.push_back(nullptr);
    return (int)_spawnvp(_P_WAIT, cargv[0], cargv.data());
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        for (const auto& e : extraEnv) setenv(e.first.c_str(), e.second.c_str(), 1);
        std::vector<char*> cargv;
        for (const auto& a : args) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);  // exec failed (e.g. program not on PATH)
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

// Split a codegen-produced flag string into argv tokens, honoring "..."/'...'
// quoting so a quoted path with spaces stays ONE token. Any $(...)/backtick
// inside a token is preserved literally (never executed) — the tokens go
// straight to execvp, not a shell.
static std::vector<std::string> shell_split(const std::string& s) {
    std::vector<std::string> out;
    std::string cur; bool inTok = false; char q = 0;
    for (char c : s) {
        if (q)                              { if (c == q) q = 0; else cur += c; inTok = true; }
        else if (c == '"' || c == '\'')     { q = c; inTok = true; }
        else if (c == ' ' || c == '\t')     { if (inTok) { out.push_back(cur); cur.clear(); inTok = false; } }
        else                                { cur += c; inTok = true; }
    }
    if (inTok) out.push_back(cur);
    return out;
}

// Forward declarations
std::vector<Token> lex(const std::string& source);
static std::string acLibRoot();
NodePtr parse(const std::vector<Token>& tokens, bool lenient = false);

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
        std::string ffiPath = acLibRoot() + "/ilib/" + libName + "/ffi/" + libName + "_ffi." + ext;
        struct stat st{};
        if (stat(ffiPath.c_str(), &st) == 0) {
            result += ffiPath + ":" + std::to_string((long long)st.st_mtime) + ";";
        }
        // Also check the .acl file
        std::string aclPath = acLibRoot() + "/ilib/" + libName + "/" + libName + ".acl";
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
    IRProgram generateIR(const ASTNode& ast, const std::string& backend, bool runtimeMode, int optLevel);
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
              << "  --output, -o <file>   Rename the generated output file\n"
              << "  --input <lt> <name>.. Inject imports from the CLI; <lt> is clib|ilib|elib|flib|header\n"
              << "                        e.g. --input clib /path/lexer.acb ilib math elib numac\n"
              << "  --no-run              Compile only; do not run the output\n"
              << "  --allow-infinite      Permit an unclosed-<mainloop> infinite loop (else it errors)\n"
              << "  --static-link         Static linking (C: gcc -static; BNY: omit DT_RUNPATH)\n"
              << "  --force               Force recompile (ignore ac-cache/)\n"
              << "  --no-cache            Disable cache read and write entirely\n"
              << "  --allow-foreign       Enable <Foreign> raw-passthrough blocks\n"
              << "  -O0..-O3              Optimize for COMPILE SPEED (TCC-style — all stay fast to build):\n"
              << "    -O0                   none — compile as written (fastest possible build)\n"
              << "    -O1                   local constant folding (2+3 -> 5)\n"
              << "    -O2                   + copy-prop + DCE (default: fast compile, tidy code)\n"
              << "    -O3                   same, more iterations — max opt while still compile-fast\n"
              << "  -O4                   Optimize for RUNTIME SPEED (GCC-style — slow compile, fast binary):\n"
              << "                        constexpr folding (evaluate whole constant calls at compile time)\n"
              << "                        + native runtime optimization. BNY-only; other backends clamp to\n"
              << "                        -O3 and hand runtime opt to gcc/rustc via the matching -O flag.\n"
              << "                        => low levels = TCC, -O4 = GCC, one compiler.\n"
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
// Resolve the AC library root (env AC_PATH, else relative to the compiler binary).
static std::string acLibRoot() {
    if (const char* acp = getenv("AC_PATH")) {
        std::string r = std::string(acp) + "/library";
        struct stat st{}; if (stat(r.c_str(), &st) == 0) return r;
    }
#ifndef _WIN32
    char exeBuf[4096] = {};
    ssize_t elen = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf)-1);
    if (elen > 0) {
        exeBuf[elen] = '\0';
        std::string bd(exeBuf);
        auto sl = bd.rfind('/');
        if (sl != std::string::npos) bd = bd.substr(0, sl);
        return bd + "/../library";
    }
#else
    char exeBuf[4096] = {};
    if (GetModuleFileNameA(NULL, exeBuf, sizeof(exeBuf)-1) > 0) {
        std::string bd(exeBuf);
        auto sl = bd.find_last_of("/\\");
        if (sl != std::string::npos) bd = bd.substr(0, sl);
        return bd + "/../library";
    }
#endif
    return "./library";
}

static void injectFlibModules(ASTNode& root, const std::string& srcDir) {
    NodeList toAppend;
    for (auto& child : root.children) {
        if (!child || child->type != NodeType::UseLibStmt) continue;
        // elib packages (installed by atar into library/elib/<name>/lib.ac) are AC-source
        // packages: rewrite to the equivalent flib import so the proven injection machinery
        // compiles them into the program on EVERY backend.
        if (child->value.rfind("elib:", 0) == 0) {
            std::string pkg = child->value.substr(5);
            std::string cand = acLibRoot() + "/elib/" + pkg + "/lib.ac";
            struct stat st{};
            if (stat(cand.c_str(), &st) == 0) {
                // `cand` is ALREADY a fully resolved path (via acLibRoot()/srcDir) — the
                // flib: resolver below re-prepends srcDir onto anything not starting with '/'
                // (its own, unrelated "resolve relative to srcDir" rule for a BARE flib path
                // from real AC source, e.g. `use flib helper.ac`). When srcDir itself is a
                // RELATIVE path (e.g. "examples", not "/home/.../examples" — normal whenever
                // `ac` is invoked with a relative source path), `cand` doesn't start with '/'
                // either, so it silently got srcDir prepended a SECOND time — "examples/
                // examples/elib/greet/lib.ac" — a real path that happens not to exist, so it
                // just failed as a plain "file not found", never surfacing as an obviously-
                // doubled path bug. realpath() guarantees an absolute result, which the flib:
                // resolver's `libpath[0]=='/'` check correctly recognizes as pre-resolved.
                char realBuf[4096] = {};
                if (realpath(cand.c_str(), realBuf)) cand = realBuf;
                child->value = "flib:" + cand;
            } else {
                std::cerr << "Preposterous: ElibError: package '" << pkg
                          << "' not installed (expected " << cand << ") — run: atar install <src> "
                          << pkg << "\n";
            }
        }
        // clib packages: how users test an elib-candidate locally before `atar install`
        // promotes it — same rewrite as elib, but rooted at the source dir (not the AC
        // install tree), since it's the developer's own in-progress package.
        if (child->value.rfind("clib:", 0) == 0) {
            std::string pkg = child->value.substr(5);
            std::string cand = srcDir + "/clib/" + pkg + "/lib.ac";
            struct stat st{};
            if (stat(cand.c_str(), &st) == 0) {
                // Same doubled-path fix as elib above — see its comment (verified here via
                // examples/keyword_catalog_modules.ac: "cannot open flib file: examples/
                // examples/clib/kwdemo/lib.ac", srcDir=="examples", a relative path).
                char realBuf[4096] = {};
                if (realpath(cand.c_str(), realBuf)) cand = realBuf;
                child->value = "flib:" + cand;
            } else {
                std::cerr << "Preposterous: ClibError: package '" << pkg
                          << "' not found (expected " << cand << ")\n";
            }
        }
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

        // Collect FuncDef, BundleDef, and resolved UseLibStmt nodes.
        // If the file uses `export`, only exported items are visible to importers
        // (UseLibStmt deps always come along). Files with no `export` share everything
        // (backward compatible with existing elib lib.ac packages).
        std::set<std::string> exportedNames;
        bool hasExports = false;
        for (auto& node : flibAst->children) {
            if (!node) continue;
            if (node->type == NodeType::ExportStmt) {
                hasExports = true;
                for (auto& n : node->attrs) exportedNames.insert(n);
            } else if (node->exported &&
                       (node->type == NodeType::FuncDef || node->type == NodeType::BundleDef)) {
                hasExports = true;
                exportedNames.insert(node->value);
            }
        }
        for (auto& node : flibAst->children) {
            if (!node) continue;
            bool isDef = node->type == NodeType::FuncDef || node->type == NodeType::BundleDef;
            if (node->type == NodeType::UseLibStmt ||
                (isDef && (!hasExports || exportedNames.count(node->value))))
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
            // "dc_", not "_dc_" — V specifically rejects any variable name with a leading
            // underscore ("cannot start with `_`"), and this synthetic per-row name is the
            // ONLY place AC's codegen ever generates one (every other synthetic name — `t_N`
            // temps, etc — already starts with a letter) — verified: examples/
            // keyword_catalog_modules.ac's datac import, V: "variable name `_dc_pets_0` cannot
            // start with `_`". No other backend cares either way, so dropping the leading
            // underscore uniformly (rather than V-specific renaming) is the simplest fix.
            std::string rowVar = "dc_" + alias + "_" + std::to_string(r);
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
            // Java-style product versioning: the compiler is "AC 1". The npm artifact rides a
            // compliant semver underneath (aclang 1.0.1 — 1.0.0 is a burned prototype release),
            // exactly like "Java 8" shipping as 1.8.0_xxx.
            std::cout << "AC 1 (aclang 1.0.1)\n";
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
    bool allowInfinite   = false; // --allow-infinite: permit an unclosed-<mainloop> infinite loop
    bool staticLink      = false; // --static-link: statically link (C: gcc -static; BNY: no DT_RUNPATH)
    bool targetWindows   = false; // --windows: BNY cross-compiles to a PE32+ .exe instead of ELF
    bool lenientParse    = false; // -supercalifragilisticexpialidocious: drop unparseable
                                   // lines instead of erroring out
    std::string outputOverride;          // --output/-o: rename the generated file
    std::vector<std::string> cmdlineImports; // --input: imports injected from the CLI

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
        } else if (arg == "--allow-infinite") {
            allowInfinite = true; // OK to run an unclosed-<mainloop> infinite loop
        } else if (arg == "--static-link") {
            staticLink = true; // static linking (C backend: gcc -static)
        } else if (arg == "--windows") {
            targetWindows = true; // BNY only: emit a PE32+ .exe instead of ELF
        } else if (arg == "-supercalifragilisticexpialidocious") {
            lenientParse = true; // a line the parser can't understand gets dropped, not fatal
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
        } else if (arg == "-O4") {
            optLevel = 4;   // heavy, GCC-style native optimization (BNY); other backends clamp to -O3
        } else if (arg == "--stop-after-ir") {
            stopAfterIR = true;
        } else if (arg == "--stop-after-cfg") {
            stopAfterCFG = true;
        } else if (arg == "--stop-after-ssa") {
            stopAfterSSA = true;
        } else if (arg == "--stop-after-opt") {
            stopAfterOpt = true;
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) outputOverride = argv[++i];
            else { std::cerr << "--output requires an argument (the output file name)\n"; return 1; }
        } else if (arg == "--input") {
            // --input <libtype> <name> [<libtype> <name> ...]  — inject imports from the CLI.
            // e.g.  --input clib /path/to/lexer.acb ilib math elib numac
            static const std::set<std::string> libtypes =
                {"clib", "ilib", "elib", "flib", "header"};
            if (!(i + 1 < argc && libtypes.count(argv[i + 1]))) {
                std::cerr << "--input requires <libtype> <name> "
                             "(libtype: clib|ilib|elib|flib|header)\n";
                return 1;
            }
            while (i + 1 < argc && libtypes.count(argv[i + 1])) {
                std::string lt = argv[++i];
                if (i + 1 >= argc) {
                    std::cerr << "--input " << lt << " requires a name/path\n";
                    return 1;
                }
                cmdlineImports.push_back(lt + " " + argv[++i]);
            }
        } else if (arg == "--no-cache") {
            noCache = true;
            forceCompile = true; // --no-cache implies skip reading cache too
        } else if (arg == "--save-ast" || arg == "--save-ir" ||
                   arg == "-compile"   || arg == "--compile") {
            // accepted but currently default behaviour
        } else if (arg.rfind("--", 0) == 0 || (arg.rfind("-", 0) == 0 && arg.size() > 1 && !std::isdigit((unsigned char)arg[1]))) {
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

    if (!outputOverride.empty() && compileAll)
        std::cerr << Toxic::outputIgnoredWithAll() << "\n";

    // Suppress unused-variable warnings for flags not yet fully wired
    // optLevel is now threaded into generateIR + the native-compiler flag
    (void)stopAfterCFG; (void)stopAfterSSA; (void)stopAfterOpt;

    try {
        std::string source = readFile(inputFile);

        // --input: splice command-line imports in as `use <libtype> <name>` lines, right
        // after the `AC->XXX` header so the parser handles them exactly like in-source `use`.
        // Disable caching when used so the injected imports never pollute the .acc/.irc.
        if (!cmdlineImports.empty()) {
            std::string inject;
            for (const auto& imp : cmdlineImports) inject += "use " + imp + "\n";
            size_t nl = source.find('\n');
            if (nl == std::string::npos) source += "\n" + inject;
            else source.insert(nl + 1, inject);
            noCache = true;
            forceCompile = true;
        }

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
        size_t bnSlash = base.find_last_of("/\\");
        std::string baseName = base.substr(bnSlash == std::string::npos ? 0 : bnSlash + 1);

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
            ast = parse(tokens, lenientParse);

            // Report collected parse errors with source context
            if (!g_parseErrors.empty() && lenientParse) {
                // Lenient mode: no Preposterous report, no caret, no abort — just one
                // roast per dropped line, and the (already-skipped) statement is simply
                // absent from the AST built by Parser::parse()'s synchronize()/skip path.
                for (size_t i = 0; i < g_parseErrors.size(); i++) {
                    std::cerr << Toxic::confusedToo() << "\n";
                }
            } else if (!g_parseErrors.empty()) {
                // Split source into lines once for caret display
                std::vector<std::string> srcLines;
                {
                    std::istringstream ss(source);
                    std::string ln;
                    while (std::getline(ss, ln)) srcLines.push_back(ln);
                }
                for (const auto& err : g_parseErrors) {
                    // err.message is often already a fully-formatted ACError ("Preposterous: …
                    // at line X char Y: …"). Don't stamp a second "Preposterous: ParseError …"
                    // header on top of it — just print it. Only bare messages get the header.
                    std::string errMsg;
                    if (err.message.rfind("Preposterous:", 0) == 0) {
                        errMsg = err.message;
                    } else {
                        errMsg = "Preposterous: SyntaxError (It's all Greek to me) at line "
                            + std::to_string(err.line) + " char " + std::to_string(err.col)
                            + ": " + err.message;
                    }
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
                // ANY parse error is fatal outside lenient mode — synchronize()'s error
                // recovery above exists purely so MULTIPLE errors can be collected and
                // reported in one pass (better diagnostics), not so compilation can proceed
                // on a partial/error-recovered AST. This used to only abort at >=10 errors,
                // meaning anything from 1-9 errors printed "Preposterous: SyntaxError..." to
                // stderr and then silently continued straight into codegen with statements
                // synchronize() had dropped, reporting "Generated: <file>" as if nothing were
                // wrong (verified: pong.ac, 2 real syntax errors, still produced pong.py/.c/
                // .asm/etc for every backend) — a broken-source file silently compiling into
                // broken output with no signal beyond scrollback noise above the false
                // "Generated:" success line.
                if ((int)g_parseErrors.size() >= 10)
                    std::cerr << "Too many parse errors. Compilation aborted.\n";
                return 1;
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

            auto timedRunArgv = [&](const std::vector<std::string>& argv,
                                    const std::vector<std::pair<std::string,std::string>>& env = {}) {
                tRunStart = Clock::now();
                run_argv(argv, env);
                tRunEnd = Clock::now();
                ran = true;
            };
            // LD_LIBRARY_PATH value = the given ilib dirs + any inherited value (for the child env).
            auto ldLibPath = [&](std::initializer_list<const char*> dirs) -> std::string {
                std::string lr = acLibRoot(), p;
                for (const char* d : dirs) { if (!p.empty()) p += ":"; p += lr + "/ilib/" + d; }
                const char* e = getenv("LD_LIBRARY_PATH");
                if (e && *e) p += std::string(":") + e;
                return p;
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
                    irProg = AC_IR::generateIR(*ast, tgt, runtimeMode, optLevel);
                    saveIRCache(tgtIrc, h, irProg);
                }
            } else {
                irProg = AC_IR::generateIR(*ast, tgt, runtimeMode, optLevel);
            }

            // Library lowering pass: rewrite lib:* IR calls to ac_* before codegen.
            // The .acl rules are identical for every backend, so parse them ONCE (A4): with --all
            // this was re-reading all 11 .acl files 13×. apply() is const → safe to reuse.
            {
                static const AC_IR::LibLowering& lowering = [] () -> const AC_IR::LibLowering& {
                    static AC_IR::LibLowering lw;
                    std::string libRoot = acLibRoot();
                    const char* acls[] = {"gl","math","camera","machine-audio","widgets","regex",
                                          "os","string-cheese","native-cpu","web","web-server","ml"};
                    for (const char* a : acls) lw.load(libRoot + "/ilib/" + a + "/" + a + ".acl");
                    return lw;
                }();
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

            // Unclosed <mainloop> means the body loops forever. Refuse without --allow-infinite
            // so it can't cook your RAM by accident.
            if (irProg.infiniteMainloop && !allowInfinite) {
                std::cerr << "Preposterous: InfiniteLoopError: <mainloop> has no closing <mainloop> tag,\n"
                          << "  so the program would loop FOREVER. If that's intended, pass --allow-infinite.\n"
                          << "  Otherwise add a closing <mainloop> line to end the block.\n";
                return false;
            }

            if (!BackendRegistry::hasBackend(tgt)) {
                std::cerr << ACError::unknownBackend(tgt).what() << "\n";
                return false;
            }

            const auto& info = BackendRegistry::getBackend(tgt);

            if (tgt == "BNY") {
                std::string outFile = (!outputOverride.empty() && !compileAll)
                                      ? outputOverride : base + info.extension;
                bool isARM = false;
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
                isARM = true;
#endif
                if (isARM) {
                    // Intentional portability path: BNY's direct emitter is x86-64. On ARM,
                    // route through AC->C and the platform C compiler, then delete the .c
                    // intermediary. This is a C-backed native binary route, not X64Emitter output.
                    AC_IR::IRProgram cIr = AC_IR::generateIR(*ast, "C", runtimeMode, optLevel);
                    {
                        static const AC_IR::LibLowering& lowering = [] () -> const AC_IR::LibLowering& {
                            static AC_IR::LibLowering lw;
                            std::string libRoot = acLibRoot();
                            const char* acls[] = {"gl","math","camera","machine-audio","widgets","regex",
                                                  "os","string-cheese","native-cpu","web","web-server","ml"};
                            for (const char* a : acls) lw.load(libRoot + "/ilib/" + a + "/" + a + ".acl");
                            return lw;
                        }();
                        lowering.apply(cIr);
                    }

                    size_t armSlash = base.find_last_of("/\\");
                    std::string armStem = (armSlash == std::string::npos) ? base : base.substr(armSlash + 1);
                    std::string cFile = base + ".c";
                    std::string cContent = generateFromIR(cIr, armStem, base);
                    writeFile(cFile, cContent);
                    std::cout << "Generated: " << cFile << " [C intermediary for BNY ARM]\n";

                    std::string linkFlags;
                    {
                        std::istringstream ss(cContent);
                        std::string line;
                        while (std::getline(ss, line)) {
                            const std::string prefix = "// Link: gcc ";
                            if (line.rfind(prefix, 0) == 0) {
                                std::string rest = line.substr(prefix.length());
                                auto sp = rest.find(' ');
                                if (sp != std::string::npos) linkFlags += " " + rest.substr(sp + 1);
                            }
                        }
                    }

                    std::string compiler = "gcc";
#ifdef __APPLE__
                    compiler = "clang";
#endif
                    std::vector<std::string> ccArgs = {compiler, "-O" + std::to_string(std::min(optLevel, 3))};
                    if (staticLink) ccArgs.push_back("-static");
                    ccArgs.push_back(cFile);
                    ccArgs.push_back("-I.");
                    for (auto& t : shell_split(linkFlags)) ccArgs.push_back(t);
                    ccArgs.push_back("-o"); ccArgs.push_back(outFile);
                    int rc = run_argv(ccArgs);
                    if (rc != 0) {
                        std::cerr << Toxic::gccChoked(rc) << "\n";
                        return false;
                    }
                    std::remove(cFile.c_str());
                    std::cout << "Generated: " << outFile << " [BNY ARM via C]\n";
                    if (runAfterCompile && !compileAll)
                        timedRunArgv({execPath(outFile)});
                    printTiming();
                    return true;
                }
                // Always pass the ilib dirs: the dynamic path embeds them as DT_RUNPATH so the binary
                // finds its .so deps standalone, and --static-link uses them to locate the freestanding
                // objects to splice. With --static-link the splice path emits NO DT_RUNPATH (it needs no
                // .so); if a used ilib fn has no freestanding impl yet, it falls back to a working
                // dynamic binary WITH the runpath (better than the old "drop runpath → broken binary").
                std::string bnyRunpath;
                {
                    std::string lr = acLibRoot();
                    const char* dirs[] = {"math","camera","os","regex","string-cheese","web",
                                          "machine-audio","widgets","native-cpu","ml","web-server"};
                    for (const char* d : dirs) {
                        if (!bnyRunpath.empty()) bnyRunpath += ":";
                        bnyRunpath += lr + "/ilib/" + d;
                    }
                }
                if (!generateBinaryFromIR(irProg, outFile, debugInfo, inputFile, bnyRunpath, staticLink, targetWindows)) {
                    std::cerr << "Preposterous: BackendError: Binary generation failed for BNY (Linux x86-64 only)\n";
                    return false;
                }
                std::cout << "Generated: " << outFile << " [exp_bny]\n";
#ifndef _WIN32
                chmod(outFile.c_str(), 0755);
#endif
                if (runAfterCompile && !compileAll) {
                    // Locate AC library root (shared resolver: $AC_PATH, else binary-relative)
                    std::string libRoot = acLibRoot();
#ifndef _WIN32
                    if (!doTime) {
                        // Replace the ac process with the compiled binary directly.
                        // This propagates the exit code and keeps the process tree clean.
                        std::string newLdPath = libRoot + "/ilib/math:" + libRoot + "/ilib/camera:" + libRoot + "/ilib/os:" + libRoot + "/ilib/regex:" + libRoot + "/ilib/string-cheese:" + libRoot + "/ilib/web";
                        const char* existing = getenv("LD_LIBRARY_PATH");
                        if (existing && strlen(existing)) newLdPath += std::string(":") + existing;
                        setenv("LD_LIBRARY_PATH", newLdPath.c_str(), 1);
                        char* argv0 = const_cast<char*>(outFile.c_str());
                        char* const exec_argv[] = { argv0, nullptr };
                        execv(outFile.c_str(), exec_argv);
                        // execv only returns on error — fall through to system() below
                    }
#endif
                    timedRunArgv({outFile},
                        {{"LD_LIBRARY_PATH", ldLibPath({"math","camera","os","regex","string-cheese","web","web-server"})}});
                }
                printTiming();
                return true;

            if (tgt == "ASM") {
                bool isARM = false;
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
                isARM = true;
#endif
                if (isARM) {
                    std::cerr << "Preposterous: ARM not supported yet\n";
                    printTiming();
                    return false;
                }
            }
            // Non-ARM AC->ASM falls through to generateFromIR below → AsmStrategy emits x86-64 NASM
            // (assemble with `nasm -f elf64`).
            }

            std::string outFile = (!outputOverride.empty() && !compileAll)
                                  ? outputOverride : base + info.extension;
            // --all runs backends in PARALLEL; C++/CPP aliases and LIB all emit ".cpp" and would
            // trample each other's file mid-compile. Give the aliases distinct names in that mode.
            if (compileAll && tgt == "C++") outFile = base + "_cxx.cpp";
            if (compileAll && tgt == "LIB") outFile = base + "_lib.cpp";
            size_t slash = base.find_last_of("/\\");
            std::string stem = (slash == std::string::npos) ? base : base.substr(slash + 1);
            // Java requires the .java FILE NAME (and the "java <class>" run invocation) to
            // exactly match its `public class` name — a hyphenated AC source filename
            // (`hello-world.ac`, entirely normal) isn't a valid Java identifier. Sanitize `stem`
            // itself here (before it feeds outFile, generateFromIR's className, AND the later
            // javac/java invocations below, which all read this same variable) rather than a
            // separate copy, so every one of those stays consistent (verified: examples/
            // hello-world.ac — javac rejected the unsanitized class/file name outright).
            if (tgt == "Java" && outputOverride.empty()) {
                for (char& c : stem)
                    if (!isalnum((unsigned char)c) && c != '_') c = '_';
                if (!stem.empty() && isdigit((unsigned char)stem[0])) stem = "_" + stem;
                // A source file named exactly after an ilib whose Java support is a lowercase
                // dispatcher CLASS (not FFI-bound, so no separate package to disambiguate it
                // from) collides with the program's own `public class <stem>` — TWO classes
                // named "math" in one file, "duplicate class: math" (verified: examples/math.ac,
                // `use ilib math` — Java's math shim is `class math {...}`, same name as the
                // file's own class). Reserved list mirrors every ilib with this shim shape.
                static const std::set<std::string> javaReservedShimNames = {
                    "math", "os", "regex", "stringm", "ncpu", "maudio",
                    "gl", "widgets", "camera", "server",
                };
                if (javaReservedShimNames.count(stem)) stem += "_ac";
                std::string dir = (slash == std::string::npos) ? "" : base.substr(0, slash + 1);
                outFile = dir + stem + info.extension;
            }
            std::string content = generateFromIR(irProg, stem, base);
            writeFile(outFile, content);
            std::cout << "Generated: " << outFile << "\n";

            bool doRun = runAfterCompile && !compileAll;
            if (doRun) std::cout << std::flush;

            // ── Interpreted / JIT backends: run directly ──────────────────────
            if (tgt == "PY") {
                if (doRun) timedRunArgv({"python3", outFile});
                printTiming();
                return true;
            }
            if (tgt == "JS") {
                if (doRun) timedRunArgv({"node", outFile});
                printTiming();
                return true;
            }
            if (tgt == "GO") {
                if (doRun) {
                    // Go's toolchain excludes ANY file ending in "_test.go" from normal builds
                    // entirely (treated as a test file, not a `package main` — "go run"/"go
                    // build" both fail with "cannot run *_test.go files" / "no packages to
                    // build") — a real, silent trap for any AC source named "*_test.ac"
                    // (verified: audio_test.ac, widgets_test.ac). Work around it by running a
                    // same-directory copy under a name that doesn't end in "_test.go".
                    std::string runFile = outFile;
                    bool isGoTestFile = outFile.size() > 8 &&
                        outFile.compare(outFile.size() - 8, 8, "_test.go") == 0;
                    if (isGoTestFile) {
                        runFile = outFile.substr(0, outFile.size() - 3) + "_run.go";
                        std::ifstream src(outFile, std::ios::binary);
                        std::ofstream dst(runFile, std::ios::binary);
                        dst << src.rdbuf();
                    }
                    timedRunArgv({"go", "run", runFile});
                    if (isGoTestFile) std::remove(runFile.c_str());
                }
                printTiming();
                return true;
            }
            if (tgt == "V") {
                // -enable-globals: web-server's V FFI shim needs module-level mutable
                // state (listener/connection/request) for its singleton server model.
                // -prod (real runtime speed, via a full C-compiler optimization pass under the
                // hood) is genuinely the "-O4: trade compile time for runtime speed" case —
                // measured ~6x SLOWER to compile (0.4s -> 2.7s on a trivial program) in exchange
                // for optimized native code, so it's only worth it at AC's own heaviest -O4
                // level; every lower level keeps V's normal fast dev-loop `run` untouched.
                std::vector<std::string> vArgs = {"v", "-enable-globals"};
                if (optLevel >= 4) vArgs.push_back("-prod");
                vArgs.push_back("run");
                vArgs.push_back(outFile);
                if (doRun) timedRunArgv(vArgs);
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
                                if (sp != std::string::npos) linkFlags += " " + rest.substr(sp + 1);
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
                if (compileAll) binFile = base + "_c";
                // linkFlags already contains absolute -L and -Wl,-rpath from the codegen.
                // -O2: AC leans on the native compiler for runtime speed (see /division -O3 note).
                // --static-link → -static (fully self-contained; needs static libs for any ilib).
                // Native optimization follows the user's -O level (gcc caps at -O3; AC's -O4 → -O3).
                std::vector<std::string> gccArgs = {"gcc", "-O" + std::to_string(std::min(optLevel, 3))};
                if (staticLink) gccArgs.push_back("-static");
                gccArgs.push_back(outFile);
                gccArgs.push_back("-I.");
                for (auto& t : shell_split(linkFlags)) gccArgs.push_back(t);
                gccArgs.push_back("-o"); gccArgs.push_back(binFile);
                int rc = run_argv(gccArgs);
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [gcc]\n";
                    if (doRun) timedRunArgv({execPath(binFile)});
                } else {
                    std::cerr << Toxic::gccChoked(rc) << "\n";
                    printTiming();
                    return false;
                }
                printTiming();
                return true;
            }

            // ── C++: compile with g++ then run ───────────────────────────────
            if (tgt == "C++" || tgt == "CPP") {
                std::string binFile = base;
                if (compileAll) binFile = base + (tgt == "C++" ? "_cxx" : "_cpp");
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
                std::vector<std::string> gxxArgs = {"g++", "-std=c++17", "-fpermissive",
                                                    "-O" + std::to_string(std::min(optLevel, 3)), "-I.", outFile};
                for (auto& t : shell_split(flibLinkFlags)) gxxArgs.push_back(t);
                for (auto& t : shell_split(glinkFlags))    gxxArgs.push_back(t);
                gxxArgs.push_back("-o"); gxxArgs.push_back(binFile);
                int rc = run_argv(gxxArgs);
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [g++]\n";
                    if (doRun) timedRunArgv({execPath(binFile)});
                } else {
                    std::cerr << Toxic::gxxChoked(rc) << "\n";
                    printTiming();
                    return false;
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
                // Same missing-optimization gap as the plain g++/gcc paths above (see rustc's
                // own comment) — the shared-library build never forwarded `-O` at all.
                std::vector<std::string> soArgs = {"g++","-std=c++17","-fpermissive",
                    "-O" + std::to_string(std::min(optLevel, 3)),"-I.","-shared","-fPIC",outFile};
                for (auto& t : shell_split(flibLinkFlags)) soArgs.push_back(t);
                for (auto& t : shell_split(glinkFlags))    soArgs.push_back(t);
                soArgs.push_back("-o"); soArgs.push_back(soFile);
                int rc = run_argv(soArgs);
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
                    std::cerr << Toxic::libBuildFellOver(rc) << "\n";
                    printTiming();
                    return false;
                }
                printTiming();
                return true;
            }

            // ── Rust: compile with rustc then run ─────────────────────────────
            if (tgt == "RS") {
                std::string binFile = base;
                // Detect ilib libraries from generated source and add link paths
                std::string libFlags;
                std::string libRoot = acLibRoot();
                if (content.find("#[link(name = \"acmath\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/math\" -l acmath -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/math\"";
                if (content.find("#[link(name = \"accamera\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/camera\" -l accamera -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/camera\"";
                if (content.find("#[link(name = \"acwidgets\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/widgets\" -l acwidgets -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/widgets\"";
                if (content.find("#[link(name = \"acregex\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/regex\" -l acregex -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/regex\"";
                if (content.find("#[link(name = \"acml\")]") != std::string::npos)
                    libFlags += " -L \"" + libRoot + "/ilib/ml\" -l acml -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/ml\"";
                // This list was originally only math/camera/widgets/regex/ml — every OTHER ilib
                // with a real Rust FFI (gl, machine-audio, os, string-cheese, native-cpu, web,
                // web-server, aczip) hit a hard "-lacX: No such file" LINKER error the moment
                // any .ac file actually used one on Rust (verified: gl_bounce.ac / "-lacgl"),
                // despite the .rs source itself compiling perfectly cleanly — the .so was simply
                // never told where to look. Same link-name convention already used by the
                // C/C++/Go/BNY sides of this same lookup (see libForSym/soLinkFlags).
                static const std::vector<std::pair<std::string,std::string>> otherIlibs = {
                    {"gl", "acgl"}, {"machine-audio", "acmachinaaudio"}, {"os", "acoos"},
                    {"string-cheese", "acstringcheese"}, {"native-cpu", "acncpu"},
                    {"web", "acweb"}, {"web-server", "acserver"}, {"aczip", "acaczip"},
                };
                for (auto& [dir, lname] : otherIlibs) {
                    if (content.find("#[link(name = \"" + lname + "\")]") != std::string::npos)
                        libFlags += " -L \"" + libRoot + "/ilib/" + dir + "\" -l " + lname
                                  + " -C link-arg=-Wl,-rpath,\"" + libRoot + "/ilib/" + dir + "\"";
                }
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
                // rustc got NO optimization flag at all before this — every Rust-compiled AC
                // program ran at rustc's unoptimized debug-build default (no inlining, no LLVM
                // opt passes), regardless of AC's own -O level, unlike gcc/g++ above which both
                // already forward `-O` + optLevel correctly. `-C opt-level=` takes the identical
                // 0-3 range (capped the same way AC's own -O4 already clamps to 3 elsewhere).
                std::vector<std::string> rustArgs = {"rustc", outFile, "-o", binFile,
                    "-C", "opt-level=" + std::to_string(std::min(optLevel, 3))};
                for (auto& t : shell_split(libFlags)) rustArgs.push_back(t);
                int rc = run_argv(rustArgs);
                if (rc == 0) {
                    std::cout << "Compiled:  " << binFile << " [rustc]\n";
                    if (doRun)
                        timedRunArgv({execPath(binFile)},
                            {{"LD_LIBRARY_PATH", ldLibPath({"math","camera","widgets","regex","web-server",
                                                             "gl","machine-audio","os","string-cheese",
                                                             "native-cpu","web","ml","aczip"})}});
                } else {
                    std::cerr << Toxic::rustcOpinions(rc) << "\n";
                    printTiming();
                    return false;
                }
                printTiming();
                return true;
            }

            // ── Java: compile with javac then run ─────────────────────────────
            if (tgt == "Java") {
                std::string javaDir = ".";
                size_t sl = outFile.rfind('/');
                if (sl != std::string::npos) javaDir = outFile.substr(0, sl);
                int rc = run_argv({"javac","--enable-preview","--release","21",outFile});
                if (rc == 0) {
                    std::cout << "Compiled:  " << stem << ".class [javac]\n";
                    if (doRun)
                        timedRunArgv({"java","--enable-preview","-cp",javaDir,stem});
                } else {
                    std::cerr << Toxic::javacNotHavingIt(rc) << "\n";
                    printTiming();
                    return false;
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
            // A3: parallelize across backends. compileOne's timing state is per-call, the IR-lowering
            // name counters are thread_local, each backend writes distinct files, and external
            // compilers (gcc/rustc/…) are separate processes — so backends are independent. The slow
            // part (rustc/javac) now overlaps instead of running serially.
            std::atomic<int> ok{0}, fail{0};
            std::atomic<size_t> next{0};
            unsigned hw = std::thread::hardware_concurrency(); if (!hw) hw = 4;
            size_t nthreads = std::min<size_t>(hw, allBackends.size());
            std::mutex logMtx;
            auto worker = [&]() {
                for (;;) {
                    size_t i = next.fetch_add(1);
                    if (i >= allBackends.size()) break;
                    const std::string& tgt = allBackends[i];
                    bool good = false;
                    try { good = compileOne(tgt); }
                    catch (const std::exception& ex) {
                        std::lock_guard<std::mutex> lk(logMtx);
                        std::cerr << tgt << ": " << ex.what() << "\n";
                    }
                    if (good) ok.fetch_add(1); else fail.fetch_add(1);
                }
            };
            std::vector<std::thread> pool;
            for (size_t t = 0; t < nthreads; t++) pool.emplace_back(worker);
            for (auto& th : pool) th.join();
            std::cout << ok.load() << " succeeded";
            if (fail.load()) std::cout << ", " << fail.load() << " failed";
            std::cout << "\n";
            return fail.load() > 0 ? 1 : 0;
        }

        // ── single-backend path ─────────────────────────────────────────────
        if (stopAfterIR) {
            auto irProg = AC_IR::generateIR(*ast, backend, runtimeMode, optLevel);
            std::string lirContent = AC_IR::generateIRText(irProg);
            if (!lirFile.empty()) writeFile(lirFile, lirContent);
            std::cout << lirContent;
            return 0;
        }
        (void)ircFile; // used inside compileOne lambda

        return compileOne(backend) ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
