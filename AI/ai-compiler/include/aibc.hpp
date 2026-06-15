#pragma once
#include <cstdint>
#include <string>
#include <vector>

// .aibc — AI Bytecode format
//
// Header:
//   magic    4 bytes   0x41 0x49 0x42 0x43  ("AIBC")
//   version  2 bytes   currently 1
//   flags    2 bytes   reserved, must be 0
//
// Constant pool:
//   count    2 bytes
//   entries  variable  type(1) + data
//     type 0x01 = integer  (8 bytes, little-endian int64)
//     type 0x02 = string   (2-byte len + chars, no null)
//     type 0x03 = decimal  (string representation, 2-byte len + chars)
//
// Function table:
//   count      2 bytes
//   entries:
//     name_ref   2 bytes  (index into constant pool, type string)
//     bc_offset  4 bytes  (byte offset into bytecode section)
//     bc_len     4 bytes  (byte length of this function's bytecode)
//     local_count 2 bytes (number of local registers)
//
// Bytecode section:
//   raw opcode stream — each instruction is 4 bytes
//   [opcode:1][A:1][B:1][C:1]

constexpr uint8_t  AIBC_MAGIC[4] = { 0x41, 0x49, 0x42, 0x43 };
constexpr uint16_t AIBC_VERSION  = 1;

// Constant pool entry types
constexpr uint8_t AIBC_CONST_INT = 0x01;
constexpr uint8_t AIBC_CONST_STR = 0x02;
constexpr uint8_t AIBC_CONST_DEC = 0x03;

// Opcodes (1 byte)
enum class AIOp : uint8_t {
    LOAD_CONST  = 0x01,  // dst=A, pool_idx=BC (16-bit)  reg[A] = pool[BC]
    LOAD_VAR    = 0x02,  // dst=A, var_idx=B              reg[A] = locals[B]
    STORE_VAR   = 0x03,  // var_idx=A, src=B              locals[A] = reg[B]
    ADD         = 0x10,  // dst=A, lhs=B, rhs=C
    SUB         = 0x11,
    MUL         = 0x12,
    DIV         = 0x13,
    MOD         = 0x14,
    XSUB        = 0x15,  // |lhs - rhs| + 1
    XOR         = 0x16,  // bitwise XOR
    NEG         = 0x17,  // dst=A, src=B    reg[A] = -reg[B]
    NOT         = 0x18,  // dst=A, src=B    reg[A] = !reg[B]
    AT_MUL      = 0x19,  // @ multiply: string×int repetition or numeric×numeric
    CMP_EQ      = 0x20,  // dst=A, lhs=B, rhs=C  → 0 or 1
    CMP_NE      = 0x21,
    CMP_LT      = 0x22,
    CMP_GT      = 0x23,
    CMP_LE      = 0x24,
    CMP_GE      = 0x25,
    AND         = 0x26,
    OR          = 0x27,
    JMP         = 0x30,  // offset=ABC (signed 24-bit)  pc += offset
    JZ          = 0x31,  // src=A, offset=BC (signed 16-bit)  if reg[A]==0 jump
    JNZ         = 0x32,
    CALL        = 0x40,  // fn_idx=A, argc=B  (args in reg[0..argc-1])
    RETURN      = 0x41,  // src=A
    DISPLAY     = 0x50,  // src=A
    INPUT       = 0x51,  // dst=A
    HALT        = 0x52,
    NOP         = 0x53,
    DEC_NEW     = 0x60,  // dst=A, pool_idx=BC  parse decimal from pool
    DEC_ADD     = 0x61,  // dst=A, lhs=B, rhs=C
    DEC_SUB     = 0x62,
    DEC_MUL     = 0x63,
    DEC_DIV     = 0x64,
    DEC_DISPLAY = 0x65,  // src=A
    DEC_CMP_EQ  = 0x66,  // dst=A, lhs=B, rhs=C → 0 or 1
    DEC_CMP_LT  = 0x67,
    DEC_CMP_GT  = 0x68,
    INT_TO_DEC  = 0x69,  // dst=A, src=B   convert int reg to dec
    DEC_TO_INT  = 0x6A,  // dst=A, src=B   truncate dec to int
    CAST_DEC    = 0x6B,  // dst=A, src=B   any val → Dec  (runtime cast)
    CAST_INT    = 0x6C,  // dst=A, src=B   any val → int
    CAST_STR    = 0x6D,  // dst=A, src=B   any val → string
    CAST_BOOL   = 0x6E,  // dst=A, src=B   any val → bool (0 or 1)
    MAKE_LIST   = 0x70,  // dst=A, start=B, count=C  build list from regs[B..B+C-1]
    LIST_GET    = 0x71,  // dst=A, list=B, idx=C
    LIST_SET    = 0x72,  // list=A, idx=B, val=C
    LIST_SIZE   = 0x73,  // dst=A, list=B  → reg[A] = len(list)
    CALL_NATIVE = 0x80,  // argc=A, pool_idx_lo=B, pool_idx_hi=C → dispatch to native .so fn; result in reg 0
};

// Instruction encoding helpers
struct AiBcInstr {
    AIOp    op;
    uint8_t A = 0, B = 0, C = 0;
};

inline uint32_t encodeInstr(AiBcInstr i) {
    return ((uint32_t)i.op) | ((uint32_t)i.A << 8) | ((uint32_t)i.B << 16) | ((uint32_t)i.C << 24);
}

inline AiBcInstr decodeInstr(uint32_t raw) {
    return { (AIOp)(raw & 0xFF), (uint8_t)((raw>>8)&0xFF),
             (uint8_t)((raw>>16)&0xFF), (uint8_t)((raw>>24)&0xFF) };
}

// Constant pool entry
struct AiBcConst {
    uint8_t     type;   // AIBC_CONST_*
    int64_t     ival;   // for INT
    std::string sval;   // for STR / DEC
};

// Function table entry
struct AiBcFunc {
    uint16_t name_ref;
    uint32_t bc_offset;
    uint32_t bc_len;
    uint16_t local_count;
};

// In-memory representation of a .aibc file
struct AiBcModule {
    std::vector<AiBcConst>   pool;
    std::vector<AiBcFunc>    funcs;
    std::vector<uint32_t>    bytecode;  // encoded instructions

    // Add to constant pool, return index
    uint16_t addInt(int64_t v) {
        for (uint16_t i = 0; i < pool.size(); i++)
            if (pool[i].type == AIBC_CONST_INT && pool[i].ival == v) return i;
        pool.push_back({ AIBC_CONST_INT, v, "" });
        return (uint16_t)(pool.size() - 1);
    }
    uint16_t addStr(const std::string& s) {
        for (uint16_t i = 0; i < pool.size(); i++)
            if (pool[i].type == AIBC_CONST_STR && pool[i].sval == s) return i;
        pool.push_back({ AIBC_CONST_STR, 0, s });
        return (uint16_t)(pool.size() - 1);
    }
    uint16_t addDec(const std::string& s) {
        for (uint16_t i = 0; i < pool.size(); i++)
            if (pool[i].type == AIBC_CONST_DEC && pool[i].sval == s) return i;
        pool.push_back({ AIBC_CONST_DEC, 0, s });
        return (uint16_t)(pool.size() - 1);
    }

    // Emit one instruction, return its index
    uint32_t emit(AiBcInstr i) {
        bytecode.push_back(encodeInstr(i));
        return (uint32_t)(bytecode.size() - 1);
    }
    // Patch instruction at index
    void patch(uint32_t idx, AiBcInstr i) { bytecode[idx] = encodeInstr(i); }

    // Serialize to binary
    std::string serialize() const;
    // Deserialize from binary
    static AiBcModule deserialize(const std::string& data);
};
