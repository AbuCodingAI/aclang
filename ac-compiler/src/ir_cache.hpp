// ir_cache.hpp — Binary cache for AC_IR::IRProgram
// Invalidation: FNV-64 hash of (source_content + '\0' + backend_name)
// Format: magic(4) + version(1) + hash(8) + IRProgram binary data
#pragma once

#include "../include/ir.hpp"
#include <fstream>
#include <string>
#include <cstdint>
#include <memory>

static const char IRC_MAGIC[4] = {'A','C','I','R'};
static const uint8_t IRC_VERSION = 2;

// ── FNV-1a 64-bit hash ────────────────────────────────────────────────────────
inline uint64_t fnv64(const std::string& data) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : data) { h ^= c; h *= 0x00000100000001B3ULL; }
    return h;
}

inline uint64_t hashForCache(const std::string& source, const std::string& backend) {
    return fnv64(source + '\0' + backend);
}

// ── Serialization primitives ──────────────────────────────────────────────────
namespace irc {

static void wU8 (std::ofstream& f, uint8_t  v) { f.write((char*)&v,1); }
static void wI32(std::ofstream& f, int32_t  v) { f.write((char*)&v,4); }
static void wU32(std::ofstream& f, uint32_t v) { f.write((char*)&v,4); }
static void wU64(std::ofstream& f, uint64_t v) { f.write((char*)&v,8); }
static void wF64(std::ofstream& f, double   v) { f.write((char*)&v,8); }
static void wStr(std::ofstream& f, const std::string& s) {
    uint16_t len=(uint16_t)s.size(); f.write((char*)&len,2); f.write(s.data(),len);
}

static void wValue(std::ofstream& f, const AC_IR::IRValue& v) {
    wU8(f,(uint8_t)v.type);
    using namespace AC_IR;
    switch (v.type) {
    case IRType::INT:    wI32(f, std::get<int>(v.data)); break;
    case IRType::FLOAT:  wF64(f, std::get<double>(v.data)); break;
    case IRType::STRING: wStr(f, std::get<std::string>(v.data)); break;
    case IRType::BOOL:   wU8(f,  std::get<bool>(v.data) ? 1 : 0); break;
    default: break;
    }
}

static void wRef(std::ofstream& f, const AC_IR::IRRef& r) {
    wU8(f,(uint8_t)r.kind); wI32(f,r.id); wValue(f,r.value);
}

static void wInstr(std::ofstream& f, const AC_IR::IRInstruction& ins) {
    wU8(f,(uint8_t)ins.opcode);
    wRef(f, ins.result);
    uint8_t nc = (uint8_t)std::min((size_t)255, ins.typedOperands.size());
    wU8(f, nc);
    for (size_t i = 0; i < nc; i++) wRef(f, ins.typedOperands[i]);
}

// ── Deserialization primitives ────────────────────────────────────────────────

static uint8_t  rU8 (std::ifstream& f) { uint8_t  v; f.read((char*)&v,1); return v; }
static int32_t  rI32(std::ifstream& f) { int32_t  v; f.read((char*)&v,4); return v; }
static uint32_t rU32(std::ifstream& f) { uint32_t v; f.read((char*)&v,4); return v; }
static uint64_t rU64(std::ifstream& f) { uint64_t v; f.read((char*)&v,8); return v; }
static double   rF64(std::ifstream& f) { double   v; f.read((char*)&v,8); return v; }
static std::string rStr(std::ifstream& f) {
    uint16_t len; f.read((char*)&len,2);
    std::string s(len,'\0'); f.read(&s[0],len); return s;
}

static AC_IR::IRValue rValue(std::ifstream& f) {
    auto type = (AC_IR::IRType)rU8(f);
    using namespace AC_IR;
    switch (type) {
    case IRType::INT:    return IRValue(rI32(f));
    case IRType::FLOAT:  return IRValue(rF64(f));
    case IRType::STRING: return IRValue(rStr(f));
    case IRType::BOOL:   return IRValue(rU8(f) != 0);
    default: return IRValue();
    }
}

static AC_IR::IRRef rRef(std::ifstream& f) {
    AC_IR::IRRef r;
    r.kind  = (AC_IR::IRRef::Kind)rU8(f);
    r.id    = rI32(f);
    r.value = rValue(f);
    return r;
}

static AC_IR::IRInstruction rInstr(std::ifstream& f) {
    auto op  = (AC_IR::IROpcode)rU8(f);
    auto res = rRef(f);
    uint8_t nc = rU8(f);
    std::vector<AC_IR::IRRef> ops; ops.reserve(nc);
    for (int i = 0; i < nc; i++) ops.push_back(rRef(f));
    return AC_IR::IRInstruction(op, res, std::move(ops));
}

} // namespace irc

// ── Public API ────────────────────────────────────────────────────────────────

inline void saveIRCache(const std::string& ircFile, uint64_t hash,
                        const AC_IR::IRProgram& prog) {
    std::ofstream f(ircFile, std::ios::binary);
    if (!f) return;
    using namespace irc;

    f.write(IRC_MAGIC, 4);
    wU8(f, IRC_VERSION);
    wU64(f, hash);
    wStr(f, prog.backend);
    wU8(f, prog.useHighLevelIR ? 1 : 0);

    // Symbol table
    uint32_t sc = (uint32_t)prog.symbols.size();
    wU32(f, sc);
    for (uint32_t i = 0; i < sc; i++) {
        const auto& sym = prog.symbols.getSymbol((int)i);
        wStr(f, sym.name);
        wU8(f, (uint8_t)sym.type);
        wI32(f, sym.scope);
    }

    // Functions
    wU32(f, (uint32_t)prog.functions.size());
    for (auto& fn : prog.functions) {
        wStr(f, fn.name);
        wU8(f, (uint8_t)fn.returnType);
        wI32(f, fn.tempCount);
        wI32(f, fn.labelCount);
        uint8_t np = (uint8_t)std::min((size_t)255, fn.parameters.size());
        wU8(f, np);
        for (size_t i = 0; i < np; i++) wStr(f, fn.parameters[i]);
        wU32(f, (uint32_t)fn.instructions.size());
        for (auto& ins : fn.instructions) wInstr(f, ins);
    }

    // Global init
    wU32(f, (uint32_t)prog.globalInit.size());
    for (auto& ins : prog.globalInit) wInstr(f, ins);

    wI32(f, prog.globalTempCount);
    wI32(f, prog.globalLabelCount);
}

// Returns nullptr on cache miss or corrupt data
inline std::unique_ptr<AC_IR::IRProgram> loadIRCache(const std::string& ircFile,
                                                      uint64_t expectedHash) {
    std::ifstream f(ircFile, std::ios::binary);
    if (!f) return nullptr;
    using namespace irc;

    char magic[4]; f.read(magic, 4);
    if (std::string(magic,4) != std::string(IRC_MAGIC,4)) return nullptr;
    if (rU8(f) != IRC_VERSION) return nullptr;
    if (rU64(f) != expectedHash) return nullptr;

    auto prog = std::make_unique<AC_IR::IRProgram>();
    prog->backend        = rStr(f);
    prog->useHighLevelIR = rU8(f) != 0;
    prog->target         = prog->backend;

    // Symbol table
    uint32_t sc = rU32(f);
    if (sc > 100000) return nullptr;
    for (uint32_t i = 0; i < sc; i++) {
        std::string name = rStr(f);
        auto type  = (AC_IR::IRType)rU8(f);
        rI32(f); // scope (intern rebuilds incrementally)
        prog->symbols.intern(name, type);
    }

    // Functions
    uint32_t fc = rU32(f);
    if (fc > 10000) return nullptr;
    for (uint32_t fi = 0; fi < fc; fi++) {
        AC_IR::IRFunction fn(rStr(f));
        fn.returnType  = (AC_IR::IRType)rU8(f);
        fn.tempCount   = rI32(f);
        fn.labelCount  = rI32(f);
        uint8_t np = rU8(f);
        for (int i = 0; i < np; i++) fn.parameters.push_back(rStr(f));
        uint32_t ic = rU32(f);
        if (ic > 1000000) return nullptr;
        fn.instructions.reserve(ic);
        for (uint32_t i = 0; i < ic; i++) fn.instructions.push_back(rInstr(f));
        prog->functions.push_back(std::move(fn));
    }

    // Global init
    uint32_t gc = rU32(f);
    if (gc > 1000000) return nullptr;
    prog->globalInit.reserve(gc);
    for (uint32_t i = 0; i < gc; i++) prog->globalInit.push_back(rInstr(f));

    prog->globalTempCount  = rI32(f);
    prog->globalLabelCount = rI32(f);

    if (!f) return nullptr; // truncated file
    return prog;
}
