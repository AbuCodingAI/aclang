#pragma once
#include "ast.hpp"
#include "types.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace aca {

// Low-level, backend-agnostic IR.
// Design goals:
// - Small set of ops: const/load/store/binop/call/jump/branch/return/label
// - Structured enough for one-time lowering passes (streaming, sugar removal)
// - Backend codegen can target Python/C/Rust from the same normalized core IR

enum class IRValueKind { Temp, Var, Const, Label, None };

struct IRConst {
    std::variant<int64_t, DecMap, std::string, bool> v;
    Type type = Type::Unknown();
};

struct IRValue {
    IRValueKind kind = IRValueKind::None;
    int id = -1;               // Temp/Label id
    std::string name;          // Var name
    IRConst c;                 // Const payload
    Type type = Type::Unknown();

    static IRValue none() { return IRValue{}; }
    static IRValue temp(int id, Type t = Type::Unknown()) {
        IRValue v;
        v.kind = IRValueKind::Temp;
        v.id = id;
        v.type = t;
        return v;
    }
    static IRValue label(int id) {
        IRValue v;
        v.kind = IRValueKind::Label;
        v.id = id;
        return v;
    }
    static IRValue var(std::string n, Type t = Type::Unknown()) {
        IRValue v;
        v.kind = IRValueKind::Var;
        v.name = std::move(n);
        v.type = t;
        return v;
    }
    static IRValue constant(IRConst c) {
        IRValue v;
        v.kind = IRValueKind::Const;
        v.c = std::move(c);
        v.type = v.c.type;
        return v;
    }
};

enum class IROp {
    LABEL,
    JUMP,
    BR,      // conditional branch: ops[0]=cond, ops[1]=trueLabel, ops[2]=falseLabel
    RETURN,  // ops[0]=value (optional)

    CONST,   // dst = const
    LOAD,    // dst = load var
    STORE,   // store var = ops[0]

    BINOP,   // dst = ops[0] <opStr> ops[1]
    CALL,    // dst? = call name(args...)
};

struct IRInst {
    IROp op;
    IRValue dst;                 // Temp for producing ops; Var for STORE name; None otherwise
    std::string aux;             // BINOP op string or CALL callee name
    std::vector<IRValue> ops;    // operands
    int line = 0;
    int col = 0;
};

struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<IRInst> insts;
    int tempCount = 0;
    int labelCount = 0;
};

struct IRProgram {
    std::string header;
    std::vector<IRFunction> functions;
    std::vector<IRInst> main;
};

// Core lowering: AST -> low-level IR (still includes some sugar via CALL names like "__stream__").
IRProgram lowerToIR(const Program& prog);

// Backend legalization hook: rewrite/normalize IR for a specific backend family.
// For now, returns the input unchanged; filled in as codegen grows.
IRProgram lowerIRForBackend(const IRProgram& ir, const std::string& backend);

std::string irToText(const IRProgram& ir);

} // namespace aca

