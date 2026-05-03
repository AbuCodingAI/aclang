#pragma once
// .acc — AC Cache
// Serializes/deserializes the AST to skip lex+parse on unchanged files.
// Invalidation: if .ac file is newer than .acc, regenerate.
// Format: binary, little-endian
//   Header: magic(4) + version(1) + node_count(4)
//   Each node: type(1) + value_len(2) + value + attr_count(1) + attrs + child_count(2)
//   Children are written recursively (depth-first).

#include "../include/ast.hpp"
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <cstdint>
#include <stdexcept>

static const char ACC_MAGIC[4] = {'A','C','C','1'};
static const uint8_t ACC_VERSION = 2;

// ── Timestamp check ──────────────────────────────────────────────────────────

inline time_t fileModTime(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return st.st_mtime;
}

inline bool cacheIsValid(const std::string& acFile, const std::string& accFile) {
    time_t acMod  = fileModTime(acFile);
    time_t accMod = fileModTime(accFile);
    return accMod > 0 && accMod >= acMod;
}

// ── Write helpers ─────────────────────────────────────────────────────────────

static void writeU8 (std::ofstream& f, uint8_t  v) { f.write((char*)&v, 1); }
static void writeU16(std::ofstream& f, uint16_t v) { f.write((char*)&v, 2); }
static void writeStr(std::ofstream& f, const std::string& s) {
    uint16_t len = (uint16_t)s.size();
    writeU16(f, len);
    f.write(s.data(), len);
}

static void serializeNode(std::ofstream& f, const ASTNode& node) {
    writeU8(f,  (uint8_t)node.type);
    writeStr(f, node.value);
    writeU8(f,  (uint8_t)node.attrs.size());
    for (auto& a : node.attrs) writeStr(f, a);
    writeU16(f, (uint16_t)node.children.size());
    for (auto& c : node.children) serializeNode(f, *c);
}

inline void saveCache(const std::string& accFile, const ASTNode& root) {
    std::ofstream f(accFile, std::ios::binary);
    if (!f) return; // silently skip if can't write
    f.write(ACC_MAGIC, 4);
    writeU8(f, ACC_VERSION);
    serializeNode(f, root);
}

// ── Read helpers ──────────────────────────────────────────────────────────────

static uint8_t  readU8 (std::ifstream& f) { uint8_t  v; f.read((char*)&v, 1); return v; }
static uint16_t readU16(std::ifstream& f) { uint16_t v; f.read((char*)&v, 2); return v; }
static std::string readStr(std::ifstream& f) {
    uint16_t len = readU16(f);
    std::string s(len, '\0');
    f.read(&s[0], len);
    return s;
}

static NodePtr deserializeNode(std::ifstream& f) {
    auto type  = (NodeType)readU8(f);
    auto value = readStr(f);
    auto node  = std::make_unique<ASTNode>(type, value);
    uint8_t attrCount = readU8(f);
    for (int i = 0; i < attrCount; i++)
        node->attrs.push_back(readStr(f));
    uint16_t childCount = readU16(f);
    for (int i = 0; i < childCount; i++)
        node->children.push_back(deserializeNode(f));
    return node;
}

// Returns nullptr if cache is invalid or corrupt
inline NodePtr loadCache(const std::string& accFile) {
    std::ifstream f(accFile, std::ios::binary);
    if (!f) return nullptr;
    char magic[4];
    f.read(magic, 4);
    if (std::string(magic, 4) != std::string(ACC_MAGIC, 4)) return nullptr;
    uint8_t ver = readU8(f);
    if (ver != ACC_VERSION) return nullptr;
    try {
        return deserializeNode(f);
    } catch (...) {
        return nullptr;
    }
}
