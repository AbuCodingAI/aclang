#pragma once
// .lir — AC IR Cache
// Serializes/deserializes the IR to skip lex+parse+IR generation on unchanged files.
// Invalidation: if .ac file is newer than .lir, regenerate.
// Format: binary, little-endian
//   Header: magic(4) + version(1) + backend_len(1) + backend + func_count(4)
//   Each function: name_len(2) + name + param_count(1) + params + instr_count(4) + instructions
//   Each instruction: opcode(1) + operand_count(1) + operands

#include "../include/ir.hpp"
#include "../include/ast.hpp"
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <cstdint>
#include <stdexcept>

// Reuse helpers from acc_cache.hpp
extern "C" {
#include "acc_cache.hpp"
}

static const char LIR_MAGIC[4] = {'A','C','L','1'};
static const uint8_t LIR_VERSION = 1;

// ── Timestamp check ──────────────────────────────────────────────────────────

inline bool lirCacheIsValid(const std::string& acFile, const std::string& lirFile, const std::string& backend) {
    time_t acMod  = fileModTime(acFile);
    time_t lirMod = fileModTime(lirFile);
    if (lirMod == 0 || lirMod < acMod) return false;
    
    // Read and verify LIR header
    std::ifstream f(lirFile, std::ios::binary);
    if (!f) return false;
    
    char magic[4];
    f.read(magic, 4);
    if (std::string(magic, 4) != std::string(LIR_MAGIC, 4)) return false;
    
    uint8_t ver = 0;
    f.read((char*)&ver, 1);
    if (ver != LIR_VERSION) return false;
    
    // Read backend length and verify
    uint8_t backendLen = 0;
    f.read((char*)&backendLen, 1);
    std::string storedBackend(backendLen, '\0');
    f.read(&storedBackend[0], backendLen);
    if (storedBackend != backend) return false;
    
    return true;
}

// ── Write helpers ─────────────────────────────────────────────────────────────

static void writeU32(std::ofstream& f, uint32_t v) { f.write((char*)&v, 4); }

static void serializeIRRef(std::ofstream& f, const AC_IR::IRRef& ref) {
    writeU8(f, (uint8_t)ref.kind);
    writeU32(f, ref.id);
    writeStr(f, ref.name);
    if (ref.value.type != AC_IR::IRType::VOID) {
        writeU8(f, (uint8_t)ref.value.type);
        switch (ref.value.type) {
            case AC_IR::IRType::INT:
                writeU32(f, (uint32_t)std::get<int>(ref.value.data));
                break;
            case AC_IR::IRType::FLOAT:
                f.write((char*)&std::get<double>(ref.value.data), 8);
                break;
            case AC_IR::IRType::STRING:
                writeStr(f, std::get<std::string>(ref.value.data));
                break;
            case AC_IR::IRType::BOOL:
                writeU8(f, std::get<bool>(ref.value.data) ? 1 : 0);
                break;
            default:
                break;
        }
    }
}

static void serializeIRInstruction(std::ofstream& f, const AC_IR::IRInstruction& instr) {
    writeU8(f, (uint8_t)instr.opcode);
    writeU8(f, (uint8_t)instr.typedOperands.size());
    for (const auto& op : instr.typedOperands) {
        serializeIRRef(f, op);
    }
    if (instr.result.isValid()) {
        writeU8(f, 1);
        serializeIRRef(f, instr.result);
    } else {
        writeU8(f, 0);
    }
    writeStr(f, instr.comment);
    writeU32(f, instr.lineNumber);
}

static void serializeIRFunction(std::ofstream& f, const AC_IR::IRFunction& func) {
    writeStr(f, func.name);
    writeU8(f, (uint8_t)func.parameters.size());
    for (const auto& p : func.parameters) {
        writeStr(f, p);
    }
    writeU8(f, (uint8_t)func.returnType);
    writeU32(f, (uint32_t)func.instructions.size());
    for (const auto& instr : func.instructions) {
        serializeIRInstruction(f, instr);
    }
}

inline void saveLIRCache(const std::string& lirFile, const AC_IR::IRProgram& program, const std::string& backend) {
    std::ofstream f(lirFile, std::ios::binary);
    if (!f) return;
    
    f.write(LIR_MAGIC, 4);
    writeU8(f, LIR_VERSION);
    writeU8(f, (uint8_t)backend.size());
    f.write(backend.data(), backend.size());
    writeU32(f, (uint32_t)program.functions.size());
    for (const auto& func : program.functions) {
        serializeIRFunction(f, func);
    }
    writeU32(f, (uint32_t)program.globalInit.size());
    for (const auto& instr : program.globalInit) {
        serializeIRInstruction(f, instr);
    }
}

// ── Read helpers ──────────────────────────────────────────────────────────────

static uint32_t readU32(std::ifstream& f) { uint32_t v; f.read((char*)&v, 4); return v; }

static AC_IR::IRRef deserializeIRRef(std::ifstream& f) {
    AC_IR::IRRef ref;
    ref.kind = (AC_IR::IRRef::Kind)readU8(f);
    ref.id = readU32(f);
    ref.name = readStr(f);
    if (readU8(f)) {
        ref.value.type = (AC_IR::IRType)readU8(f);
        switch (ref.value.type) {
            case AC_IR::IRType::INT:
                ref.value.data = (int)readU32(f);
                break;
            case AC_IR::IRType::FLOAT:
                f.read((char*)&std::get<double>(ref.value.data), 8);
                break;
            case AC_IR::IRType::STRING:
                ref.value.data = readStr(f);
                break;
            case AC_IR::IRType::BOOL:
                ref.value.data = (bool)readU8(f);
                break;
            default:
                break;
        }
    }
    return ref;
}

static AC_IR::IRInstruction deserializeIRInstruction(std::ifstream& f) {
    AC_IR::IROpcode opcode = (AC_IR::IROpcode)readU8(f);
    uint8_t opCount = readU8(f);
    std::vector<AC_IR::IRRef> operands;
    for (int i = 0; i < opCount; i++) {
        operands.push_back(deserializeIRRef(f));
    }
    AC_IR::IRRef result;
    if (readU8(f)) {
        result = deserializeIRRef(f);
    }
    std::string comment = readStr(f);
    uint32_t lineNumber = readU32(f);
    
    AC_IR::IRInstruction instr(opcode, operands);
    instr.result = result;
    instr.comment = comment;
    instr.lineNumber = lineNumber;
    return instr;
}

static AC_IR::IRFunction deserializeIRFunction(std::ifstream& f) {
    std::string name = readStr(f);
    AC_IR::IRFunction func(name);
    uint8_t paramCount = readU8(f);
    for (int i = 0; i < paramCount; i++) {
        func.parameters.push_back(readStr(f));
    }
    func.returnType = (AC_IR::IRType)readU8(f);
    uint32_t instrCount = readU32(f);
    for (int i = 0; i < instrCount; i++) {
        func.instructions.push_back(deserializeIRInstruction(f));
    }
    return func;
}

// Returns empty program if cache is invalid or corrupt
inline AC_IR::IRProgram loadLIRCache(const std::string& lirFile) {
    std::ifstream f(lirFile, std::ios::binary);
    if (!f) return AC_IR::IRProgram();
    
    char magic[4];
    f.read(magic, 4);
    if (std::string(magic, 4) != std::string(LIR_MAGIC, 4)) return AC_IR::IRProgram();
    
    uint8_t ver = readU8(f);
    if (ver != LIR_VERSION) return AC_IR::IRProgram();
    
    uint8_t backendLen = readU8(f);
    std::string backend(backendLen, '\0');
    f.read(&backend[0], backendLen);
    
    AC_IR::IRProgram program;
    program.backend = backend;
    
    uint32_t funcCount = readU32(f);
    for (int i = 0; i < funcCount; i++) {
        program.functions.push_back(deserializeIRFunction(f));
    }
    
    uint32_t globalCount = readU32(f);
    for (int i = 0; i < globalCount; i++) {
        program.globalInit.push_back(deserializeIRInstruction(f));
    }
    
    return program;
}
