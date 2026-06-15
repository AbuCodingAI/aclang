#include "../include/interpreter.hpp"
#include "aibc.hpp"
#include <iostream>
#include <fstream>
#include <string>

// ── Minimal .aibc loader (mirrors AiBcModule::deserialize) ─────────────────
static uint8_t  ru8(const std::string& d, size_t& p)  { return (uint8_t)d[p++]; }
static uint16_t ru16(const std::string& d, size_t& p) { uint16_t v=(uint8_t)d[p]|((uint8_t)d[p+1]<<8); p+=2; return v; }
static uint32_t ru32(const std::string& d, size_t& p) { uint32_t v=(uint8_t)d[p]|((uint8_t)d[p+1]<<8)|((uint8_t)d[p+2]<<16)|((uint8_t)d[p+3]<<24); p+=4; return v; }
static int64_t  ri64(const std::string& d, size_t& p) { int64_t v=0; for(int i=0;i<8;i++) v|=((int64_t)(uint8_t)d[p+i]<<(i*8)); p+=8; return v; }
static std::string rstr(const std::string& d, size_t& p) { uint16_t n=ru16(d,p); std::string s=d.substr(p,n); p+=n; return s; }

static AiBcModule loadAibc(const std::string& data) {
    AiBcModule mod;
    if (data.size() < 8) return mod;
    if (data[0]!=(char)0x41||data[1]!=(char)0x49||data[2]!=(char)0x42||data[3]!=(char)0x43) {
        std::cerr << "Preposterous: not a valid .aibc file\n"; return mod;
    }
    size_t p = 4;
    ru16(data, p); ru16(data, p);  // version, flags

    uint16_t poolCount = ru16(data, p);
    for (uint16_t i = 0; i < poolCount; i++) {
        AiBcConst c; c.type = ru8(data, p);
        if      (c.type == AIBC_CONST_INT) c.ival = ri64(data, p);
        else if (c.type == AIBC_CONST_STR || c.type == AIBC_CONST_DEC) c.sval = rstr(data, p);
        mod.pool.push_back(c);
    }
    uint16_t fnCount = ru16(data, p);
    for (uint16_t i = 0; i < fnCount; i++) {
        AiBcFunc f;
        f.name_ref  = ru16(data, p);
        f.bc_offset = ru32(data, p);
        f.bc_len    = ru32(data, p);
        f.local_count = ru16(data, p);
        mod.funcs.push_back(f);
    }
    uint32_t bcCount = ru32(data, p);
    for (uint32_t i = 0; i < bcCount; i++) mod.bytecode.push_back(ru32(data, p));
    return mod;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "AIVM — AI Virtual Machine\n"
                  << "Usage: aivm <program.aibc>\n";
        return 1;
    }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::cerr << "Preposterous: cannot open " << argv[1] << "\n"; return 1; }
    std::string data((std::istreambuf_iterator<char>(f)), {});
    AiBcModule mod = loadAibc(data);
    aivm_run(mod);
    return 0;
}
