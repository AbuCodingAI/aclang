#include "cache.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sapl_lower.hpp"
#include "ir.hpp"
#include "typecheck.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write file: " + path);
    out.write(data.data(), (std::streamsize)data.size());
}

static std::string dirOf(const std::string& path) {
    auto p = path.find_last_of("/\\");
    return (p == std::string::npos) ? "." : path.substr(0, p);
}

static std::string baseNameNoExt(const std::string& path) {
    auto p = path.find_last_of("/\\");
    std::string name = (p == std::string::npos) ? path : path.substr(p + 1);
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

static void ensureDir(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: aca <file.aca>\n";
            return 2;
        }
        std::string input = argv[1];
        std::string src = readFile(input);

        // Cache dir: aca-cache next to source
        std::string cacheDir = dirOf(input) + "/aca-cache";
        ensureDir(cacheDir);
        std::string base = baseNameNoExt(input);

        // Always emit .sapl + .ir and cache .irc; cache key is (source + backend + ir-version).
        // Cache invalidation: bump the suffix when SAPL lowering changes.
        uint64_t h = aca::fnv64(src + "\0" + "IR" + "\0" + "aca-irc-v3-ircore");
        std::string saplPath = cacheDir + "/" + base + ".sapl";
        std::string irPath   = cacheDir + "/" + base + ".ir";
        std::string ircPath = cacheDir + "/" + base + ".irc";

        if (auto hit = aca::loadIrc(ircPath, h)) {
            writeFile(saplPath, hit->sapl);
            std::cout << "Cache hit: " << ircPath << "\n";
            // NOTE: cached entry currently stores SAPL only; IR text is regenerated on misses.
            std::cout << "Emitted: " << saplPath << "\n";
            if (std::ifstream(irPath).good()) std::cout << "Emitted: " << irPath << "\n";
            return 0;
        }

        aca::Lexer lexer(std::move(src));
        auto toks = lexer.lex();
        aca::Parser parser(std::move(toks));
        auto prog = parser.parse();

        aca::TypeChecker tc;
        tc.check(prog);

        aca::IRProgram ir = aca::lowerToIR(prog);
        std::string irText = aca::irToText(ir);
        writeFile(irPath, irText);

        std::string sapl = aca::lowerToSapl(prog);
        writeFile(saplPath, sapl);
        aca::saveIrc(ircPath, h, sapl);

        std::cout << "Emitted: " << irPath << "\n";
        std::cout << "Emitted: " << saplPath << "\n";
        std::cout << "Cached: " << ircPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
