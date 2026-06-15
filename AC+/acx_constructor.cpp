/*
    acx_constructor — wraps a flat binary into the ACX format

    ACX layout:
        Bytes 0-3  : Magic  0x41 0x43 0x58 0x21  ("ACX!")
        Byte  4    : Arch   0x01 = x86-64 | 0x02 = ARM
        Bytes 5-12 : Entry  64-bit LE offset into code section (__ai_main__)

        Import section (optional, immediately after header):
          0x70               — import section start
          [0x7N][name\0]...  — each import: type byte + null-terminated name
                               0x71 = ilib  0x72 = elib  0x73 = clib  0x74 = flib
          0x00               — end of import section

        Code section:
          Flat binary; entry field is an offset into this section.

    Usage:
        Build:   acx_constructor <input.bin> <output.acx> <entry> [--arm] [--import TYPE:name ...]
        Inspect: acx_constructor --info <file.acx>

        --import TYPE:name    add an import (TYPE = ilib|elib|clib|flib)
        --arm                 set arch byte to 0x02 (ARM)

    Example:
        as   --64 -o kernel.o kernel.s
        ld   -Ttext 0x0 --oformat binary -o kernel.bin kernel.o
        acx_constructor kernel.bin kernel.acx 0x0 --import flib:logic.acp
*/

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

// ── Magic / arch constants ────────────────────────────────────────────────────

static constexpr uint8_t ACX_MAGIC[4] = { 0x41, 0x43, 0x58, 0x21 };
static constexpr uint8_t ARCH_X86_64  = 0x01;
static constexpr uint8_t ARCH_ARM     = 0x02;

// Import type bytes
static constexpr uint8_t IMP_SECTION = 0x70;
static constexpr uint8_t IMP_ILIB    = 0x71; // ilib — AC built-in/standard libraries (stored in the ilib/ folder)
static constexpr uint8_t IMP_ELIB    = 0x72; // elib — packages installed via atar (AC's package manager, uses .tar bundles)
static constexpr uint8_t IMP_CLIB    = 0x73; // clib — custom libraries you made, placed as a subfolder inside the clib/ folder
static constexpr uint8_t IMP_FLIB    = 0x74; // flib — a single .ac/.acp file imported by path (can live anywhere on disk)
static constexpr uint8_t IMP_END     = 0x00;

struct Import {
    uint8_t     type; // IMP_ILIB .. IMP_FLIB
    std::string name;
};

static const char* impTypeName(uint8_t t) {
    switch (t) {
    case IMP_ILIB: return "ilib";  // internal
    case IMP_ELIB: return "elib";  // external
    case IMP_CLIB: return "clib";  // computer (global system install)
    case IMP_FLIB: return "flib";  // file
    default:       return "unknown";
    }
}

static uint8_t impTypeFromStr(const std::string& s) {
    if (s == "ilib") return IMP_ILIB;
    if (s == "elib") return IMP_ELIB;
    if (s == "clib") return IMP_CLIB;
    if (s == "flib") return IMP_FLIB;
    throw std::runtime_error("Unknown import type: " + s);
}

// ── Write ACX ─────────────────────────────────────────────────────────────────

static void writeACX(const std::string& outPath,
                     uint8_t arch,
                     uint64_t entry,
                     const std::vector<Import>& imports,
                     const std::vector<uint8_t>& code)
{
    std::ofstream f(outPath, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open for writing: " + outPath);

    // Header
    f.write(reinterpret_cast<const char*>(ACX_MAGIC), 4);
    f.put(static_cast<char>(arch));
    for (int i = 0; i < 8; i++)
        f.put(static_cast<char>((entry >> (i * 8)) & 0xFF));

    // Import section (only if there are imports)
    if (!imports.empty()) {
        f.put(static_cast<char>(IMP_SECTION));
        for (auto& imp : imports) {
            f.put(static_cast<char>(imp.type));
            f.write(imp.name.c_str(), static_cast<std::streamsize>(imp.name.size() + 1)); // include \0
        }
        f.put(static_cast<char>(IMP_END));
    }

    // Code payload
    f.write(reinterpret_cast<const char*>(code.data()),
            static_cast<std::streamsize>(code.size()));

    if (!f) throw std::runtime_error("Write error: " + outPath);
}

// ── Read / inspect ACX ────────────────────────────────────────────────────────

static void readACX(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);

    // Header
    uint8_t magic[4];
    f.read(reinterpret_cast<char*>(magic), 4);
    if (memcmp(magic, ACX_MAGIC, 4) != 0)
        throw std::runtime_error("Not an ACX file: bad magic");

    uint8_t arch = static_cast<uint8_t>(f.get());
    uint64_t entry = 0;
    for (int i = 0; i < 8; i++)
        entry |= (static_cast<uint64_t>(static_cast<uint8_t>(f.get())) << (i * 8));

    // Import section?
    std::vector<Import> imports;
    auto pos = f.tellg();
    uint8_t probe = static_cast<uint8_t>(f.get());
    if (probe == IMP_SECTION) {
        while (true) {
            uint8_t typeByte = static_cast<uint8_t>(f.get());
            if (typeByte == IMP_END || f.eof()) break;
            std::string name;
            char ch;
            while (f.get(ch) && ch != '\0') name += ch;
            imports.push_back({ typeByte, name });
        }
    } else {
        // No import section — rewind the one byte we probed
        f.seekg(pos);
    }

    auto codeStart = f.tellg();
    f.seekg(0, std::ios::end);
    auto total      = static_cast<uint64_t>(f.tellg());
    auto codeBytes  = total - static_cast<uint64_t>(codeStart);

    printf("ACX file  : %s\n", path.c_str());
    printf("  Magic   : ACX!\n");
    printf("  Arch    : 0x%02X (%s)\n", arch,
           arch == ARCH_X86_64 ? "x86-64" : arch == ARCH_ARM ? "ARM" : "unknown");
    printf("  Entry   : 0x%016llX\n", (unsigned long long)entry);
    if (!imports.empty()) {
        printf("  Imports : %zu\n", imports.size());
        for (auto& imp : imports)
            printf("    [0x%02X] %-6s %s\n", imp.type, impTypeName(imp.type), imp.name.c_str());
    } else {
        printf("  Imports : (none)\n");
    }
    printf("  Code    : %llu bytes\n", (unsigned long long)codeBytes);
    printf("  Total   : %llu bytes\n", (unsigned long long)total);
}

// ── Main ──────────────────────────────────────────────────────────────────────

static void usage(const char* argv0) {
    fprintf(stderr,
        "Usage:\n"
        "  Build  : %s <input.bin> <output.acx> <entry> [--arm] [--import TYPE:name ...]\n"
        "  Inspect: %s --info <file.acx>\n"
        "\n"
        "  --import TYPE:name   add import  (TYPE = ilib | elib | clib | flib)\n"
        "  --arm                ARM arch byte (0x02) instead of x86-64\n",
        argv0, argv0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (argc == 3 && std::string(argv[1]) == "--info") {
        try { readACX(argv[2]); }
        catch (const std::exception& e) { fprintf(stderr, "Error: %s\n", e.what()); return 1; }
        return 0;
    }

    if (argc < 4) { usage(argv[0]); return 1; }

    std::string inPath   = argv[1];
    std::string outPath  = argv[2];
    uint64_t entry       = strtoull(argv[3], nullptr, 0);
    uint8_t  arch        = ARCH_X86_64;
    std::vector<Import>  imports;

    for (int i = 4; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--arm") {
            arch = ARCH_ARM;
        } else if (arg == "--import" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto colon = spec.find(':');
            if (colon == std::string::npos) {
                fprintf(stderr, "Error: --import expects TYPE:name\n"); return 1;
            }
            try {
                uint8_t t = impTypeFromStr(spec.substr(0, colon));
                imports.push_back({ t, spec.substr(colon + 1) });
            } catch (const std::exception& e) {
                fprintf(stderr, "Error: %s\n", e.what()); return 1;
            }
        }
    }

    std::ifstream inFile(inPath, std::ios::binary);
    if (!inFile) { fprintf(stderr, "Error: cannot open %s\n", inPath.c_str()); return 1; }
    std::vector<uint8_t> code(
        (std::istreambuf_iterator<char>(inFile)),
        std::istreambuf_iterator<char>());

    try { writeACX(outPath, arch, entry, imports, code); }
    catch (const std::exception& e) { fprintf(stderr, "Error: %s\n", e.what()); return 1; }

    printf("Written : %s\n", outPath.c_str());
    printf("  Arch   : %s\n",                  arch == ARCH_X86_64 ? "x86-64" : "ARM");
    printf("  Entry  : 0x%016llX\n",            (unsigned long long)entry);
    printf("  Imports: %zu\n",                  imports.size());
    for (auto& imp : imports)
        printf("    [0x%02X] %-6s %s\n", imp.type, impTypeName(imp.type), imp.name.c_str());
    printf("  Code   : %zu bytes\n",            code.size());
    printf("  Total  : %zu bytes\n",
           code.size() + 13 + (imports.empty() ? 0 : 2 + imports.size() /* type bytes */ +
               [&]{ size_t n=0; for(auto& i:imports) n+=i.name.size()+1; return n; }()));
    return 0;
}
