#include "../include/token.hpp"
#include "../include/ast.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <functional>

// Forward declarations (defined in other TUs)
std::vector<Token> lex(const std::string& src);
NodePtr parseACP(const std::vector<Token>& tokens);
std::string generateASM(const ASTNode& root);

// ── ACX format constants ──────────────────────────────────────────────────────

static constexpr uint8_t ACX_MAGIC[4] = { 0x41, 0x43, 0x58, 0x21 }; // "ACX!"
static constexpr uint8_t ARCH_X86_64  = 0x01;
static constexpr uint8_t ARCH_ARM     = 0x02;

// Import section bytes
static constexpr uint8_t IMP_SECTION  = 0x70;
static constexpr uint8_t IMP_ILIB     = 0x71; // ilib — AC built-in/standard libraries (ilib/ folder)
static constexpr uint8_t IMP_ELIB     = 0x72; // elib — packages installed via atar (AC's package manager)
static constexpr uint8_t IMP_CLIB     = 0x73; // clib — custom libraries you made, as subfolders in clib/ folder
static constexpr uint8_t IMP_FLIB     = 0x74; // flib — single .ac/.acp file by path (anywhere on disk)
static constexpr uint8_t IMP_END      = 0x00;

struct AcxImport {
    uint8_t     type;
    std::string name;
};

// ── File helpers ──────────────────────────────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << content;
}

// ── Write ACX binary ──────────────────────────────────────────────────────────

static void writeACX(const std::string& outPath, uint8_t arch, uint64_t entry,
                     const std::vector<AcxImport>& imports,
                     const std::string& payload) {
    std::ofstream f(outPath, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write: " + outPath);

    // 13-byte header
    f.write(reinterpret_cast<const char*>(ACX_MAGIC), 4);
    f.put(static_cast<char>(arch));
    for (int i = 0; i < 8; i++)
        f.put(static_cast<char>((entry >> (i * 8)) & 0xFF));

    // Import section (only if non-empty)
    if (!imports.empty()) {
        f.put(static_cast<char>(IMP_SECTION));
        for (auto& imp : imports) {
            f.put(static_cast<char>(imp.type));
            f.write(imp.name.c_str(), static_cast<std::streamsize>(imp.name.size() + 1));
        }
        f.put(static_cast<char>(IMP_END));
    }

    // Code payload
    f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!f) throw std::runtime_error("Write error: " + outPath);
}

// ── Collect imports from AST ──────────────────────────────────────────────────

static std::vector<AcxImport> collectImports(const ASTNode& root) {
    std::vector<AcxImport> result;
    for (auto& ch : root.children) {
        if (!ch || ch->type != NodeType::UseFlibStmt) continue;
        result.push_back({ IMP_FLIB, ch->value });
    }
    return result;
}

// ── Shell helper: run command, capture stdout ─────────────────────────────────

static int runCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

// ── flib injection ────────────────────────────────────────────────────────────

static void injectFlib(ASTNode& root, const std::string& libPath) {
    std::string src;
    try { src = readFile(libPath); }
    catch (...) {
        std::cerr << "acp: warning: flib not found: " << libPath << "\n";
        return;
    }
    auto tokens = lex(src);
    auto libAst  = parseACP(tokens);
    if (!libAst) return;
    for (auto& ch : libAst->children) {
        if (!ch) continue;
        if (ch->type == NodeType::FuncDef) {
            ch->attrs.push_back("__inlined__");
            root.children.insert(root.children.begin(), std::move(ch));
        }
    }
}

// ── Entry-point helpers ───────────────────────────────────────────────────────

static bool isLibHeader(const ASTNode& root) {
    for (auto& ch : root.children)
        if (ch && ch->type == NodeType::BackendDecl && ch->value == "LIB")
            return true;
    return false;
}

static bool hasMainLoop(const ASTNode& root) {
    for (auto& ch : root.children)
        if (ch && ch->type == NodeType::MainLoop)
            return true;
    return false;
}

// ── Usage ─────────────────────────────────────────────────────────────────────

static void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " <file.acp> [-o output.acx] [options]\n"
        << "  -o <file>      Output .acx file (default: <input>.acx)\n"
        << "  --emit-asm     Write the intermediate .s and stop (no assembly)\n"
        << "  --dump-ast     Print AST to stderr and exit\n"
        << "  --arm          Target ARM (arch byte 0x02)\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    std::string inPath;
    std::string outPath;
    bool emitAsm  = false;
    bool dumpAst  = false;
    uint8_t arch  = ARCH_X86_64;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "-o" && i + 1 < argc) outPath = argv[++i];
        else if (arg == "--emit-asm")          emitAsm = true;
        else if (arg == "--dump-ast")          dumpAst = true;
        else if (arg == "--arm")               arch    = ARCH_ARM;
        else if (arg[0] != '-')                inPath  = arg;
        else { std::cerr << "acp: unknown option: " << arg << "\n"; return 1; }
    }

    if (inPath.empty()) { usage(argv[0]); return 1; }

    // Default output: replace extension with .acx
    if (outPath.empty()) {
        outPath = inPath;
        auto dot = outPath.rfind('.');
        if (dot != std::string::npos) outPath = outPath.substr(0, dot);
        outPath += ".acx";
    }

    // Source directory for flib resolution
    std::string srcDir;
    {
        namespace fs = std::filesystem;
        auto p = fs::path(inPath).parent_path();
        srcDir = p.empty() ? "." : p.string();
    }

    // ── Lex + parse ───────────────────────────────────────────────────────────
    std::string src;
    try { src = readFile(inPath); }
    catch (const std::exception& e) { std::cerr << "acp: " << e.what() << "\n"; return 1; }

    std::vector<Token> tokens;
    NodePtr ast;
    try {
        tokens = lex(src);
        ast    = parseACP(tokens);
    } catch (const std::exception& e) {
        std::cerr << "acp: parse error: " << e.what() << "\n";
        return 1;
    }
    if (!ast) { std::cerr << "acp: empty AST\n"; return 1; }

    if (dumpAst) {
        std::function<void(const ASTNode&, int)> dump = [&](const ASTNode& n, int d) {
            std::cerr << std::string(d * 2, ' ')
                      << "[" << (int)n.type << "] \"" << n.value << "\"\n";
            for (auto& ch : n.children) if (ch) dump(*ch, d + 1);
        };
        dump(*ast, 0);
        return 0;
    }

    // ── Inject flib imports ───────────────────────────────────────────────────
    for (auto& ch : ast->children) {
        if (!ch || ch->type != NodeType::UseFlibStmt) continue;
        std::string libPath = ch->value;
        if (libPath[0] != '/') libPath = srcDir + "/" + libPath;
        injectFlib(*ast, libPath);
    }

    // ── Entry-point enforcement ───────────────────────────────────────────────
    bool lib = isLibHeader(*ast);
    if (lib) {
        std::cerr << "acp: " << inPath << " is an AC+ LIB — library files are not compiled directly.\n"
                  << "     Import it from another .acp program with:  use flib "
                  << std::filesystem::path(inPath).filename().string() << "\n";
        return 1;
    }
    if (!hasMainLoop(*ast)) {
        std::cerr << "acp: Preposterous: EntryPointError — "
                  << inPath << " has no <mainloop>\n";
        return 1;
    }

    // ── Code generation → .s text ─────────────────────────────────────────────
    std::string asmText;
    try { asmText = generateASM(*ast); }
    catch (const std::exception& e) {
        std::cerr << "acp: codegen error: " << e.what() << "\n";
        return 1;
    }

    // --emit-asm: write .s and stop (for debugging / inspection)
    if (emitAsm) {
        std::string asmPath = inPath;
        auto dot = asmPath.rfind('.');
        if (dot != std::string::npos) asmPath = asmPath.substr(0, dot);
        asmPath += ".s";
        try { writeFile(asmPath, asmText); }
        catch (const std::exception& e) {
            std::cerr << "acp: " << e.what() << "\n"; return 1;
        }
        std::cerr << "acp: wrote " << asmPath << "\n";
        return 0;
    }

    // ── Assemble + link → flat binary ─────────────────────────────────────────
    // Use a temp directory
    namespace fs = std::filesystem;
    std::string tmpDir = (fs::temp_directory_path() / "acp_XXXXXX").string();
    // mkdtemp-style: just use a predictable but unique name with PID
    {
        tmpDir = (fs::temp_directory_path() / ("acp_" + std::to_string(getpid()))).string();
        fs::create_directories(tmpDir);
    }

    std::string tmpS   = tmpDir + "/out.s";
    std::string tmpO   = tmpDir + "/out.o";
    std::string tmpBin = tmpDir + "/out.bin";

    // Write .s
    try { writeFile(tmpS, asmText); }
    catch (const std::exception& e) {
        std::cerr << "acp: " << e.what() << "\n"; return 1;
    }

    // as --64 -o out.o out.s
    std::string asCmd = "as --64 -o " + tmpO + " " + tmpS + " 2>&1";
    if (runCmd(asCmd) != 0) {
        std::cerr << "acp: assembler failed. Run with --emit-asm to inspect the .s\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    // ld -Ttext 0x0 --oformat binary -o out.bin out.o
    // Entry __ai_main__ is emitted first in the .s → offset 0x0 in binary
    std::string ldCmd = "ld -Ttext 0x0 --oformat binary -o " + tmpBin + " " + tmpO + " 2>&1";
    if (runCmd(ldCmd) != 0) {
        std::cerr << "acp: linker failed. Run with --emit-asm to inspect the .s\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    // Read flat binary
    std::string payload;
    try { payload = readFile(tmpBin); }
    catch (const std::exception& e) {
        std::cerr << "acp: " << e.what() << "\n";
        fs::remove_all(tmpDir);
        return 1;
    }

    fs::remove_all(tmpDir);

    // ── Write .acx ────────────────────────────────────────────────────────────
    // __ai_main__ is always at offset 0 (emitted first in .s; linked at -Ttext 0).
    // Import section records all use flib declarations as metadata.
    auto imports = collectImports(*ast);
    try { writeACX(outPath, arch, 0x0ULL, imports, payload); }
    catch (const std::exception& e) {
        std::cerr << "acp: " << e.what() << "\n"; return 1;
    }

    std::cerr << "acp: wrote " << outPath
              << "  (" << payload.size() << " bytes code"
              << (imports.empty() ? "" : ", " + std::to_string(imports.size()) + " import(s)")
              << ", entry=0x0)\n";
    return 0;
}
