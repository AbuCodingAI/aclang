#include "../include/ac.hpp"
#include <sstream>
#include <stack>
#include <set>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

// Declared in main.cpp; set by --allow-foreign CLI flag
extern bool g_allow_foreign;

namespace AC_IR {

// ─── IRRef legacy helpers ───────────────────────────────────────────────────

// Legacy: Create VAR from string name (for backward compatibility)
// This should be replaced with symbol table lookups
IRRef IRRef::varLegacy(const std::string& varName) {
    IRRef ref;
    ref.kind = Kind::VAR;
    ref.id = -1;  // Mark as legacy (needs symbol table lookup)
    // Store name temporarily in value field as string
    ref.value = IRValue(varName);
    return ref;
}

// Legacy: Create FUNCTION from string name (for backward compatibility)
IRRef IRRef::funcLegacy(const std::string& funcName) {
    IRRef ref;
    ref.kind = Kind::FUNCTION;
    ref.id = -1;  // Mark as legacy (needs symbol table lookup)
    // Store name temporarily in value field as string
    ref.value = IRValue(funcName);
    return ref;
}

// Helper to get string representation with symbol table (for better debugging)
std::string IRRef::toStringWithSymbols(SymbolTable* symbols) const {
    if (!symbols) {
        return toString();
    }
    
    switch (kind) {
        case Kind::VAR:
            if (id >= 0) {
                return symbols->getName(id);
            }
            // Legacy: extract name from value field
            if (value.type == IRType::STRING) {
                return std::get<std::string>(value.data);
            }
            return "v" + std::to_string(id);
            
        case Kind::FUNCTION:
            if (id >= 0) {
                return symbols->getName(id);
            }
            // Legacy: extract name from value field
            if (value.type == IRType::STRING) {
                return std::get<std::string>(value.data);
            }
            return "f" + std::to_string(id);
            
        default:
            return toString();
    }
}

// ─── helpers ────────────────────────────────────────────────────────────────

static std::string typeStr(IRType t) {
    switch (t) {
        case IRType::VOID:     return "void";
        case IRType::INT:      return "int";
        case IRType::FLOAT:    return "float";
        case IRType::STRING:   return "string";
        case IRType::BOOL:     return "bool";
        case IRType::LIST:     return "list";
        case IRType::TUPLE:    return "tuple";
        case IRType::OBJECT:   return "object";
        case IRType::FUNCTION: return "function";
        case IRType::POINTER:  return "ptr";
        default:               return "unknown";
    }
}

static std::string opcodeStr(IROpcode op) {
    switch (op) {
        case IROpcode::LABEL:         return "label";
        case IROpcode::JUMP:          return "jmp";
        case IROpcode::JUMP_IF_TRUE:  return "jt";
        case IROpcode::JUMP_IF_FALSE: return "jf";
        case IROpcode::CALL:          return "call";
        case IROpcode::RETURN:        return "ret";
        case IROpcode::LOAD_CONST:    return "ldc";
        case IROpcode::LOAD_VAR:      return "ldv";
        case IROpcode::STORE_VAR:     return "stv";
        case IROpcode::ADD:           return "add";
        case IROpcode::SUB:           return "sub";
        case IROpcode::MUL:           return "mul";
        case IROpcode::PMUL:          return "pmul";
        case IROpcode::DIV:           return "div";
        case IROpcode::IDIV:          return "idiv";
        case IROpcode::MOD:           return "mod";
        case IROpcode::EQ:            return "eq";
        case IROpcode::NEQ:           return "neq";
        case IROpcode::LT:            return "lt";
        case IROpcode::GT:            return "gt";
        case IROpcode::LTE:           return "lte";
        case IROpcode::GTE:           return "gte";
        case IROpcode::AND:           return "and";
        case IROpcode::OR:            return "or";
        case IROpcode::NOT:           return "not";
        case IROpcode::XOR:           return "xor";
        case IROpcode::XNOR:          return "xnor";
        case IROpcode::XSUB:          return "xsub";
        case IROpcode::ALLOC:         return "alloc";
        case IROpcode::FREE:          return "free";
        case IROpcode::FREE_DECL:     return "free_decl";
        case IROpcode::ALIAS_DECL:    return "alias_decl";
        case IROpcode::CONST_DECL:    return "const_decl";
        case IROpcode::RAISE_CLAUSE:  return "raise_clause";
        case IROpcode::LAZY_EVAL:     return "lazy_eval";
        case IROpcode::LOAD_INDEX:    return "ldi";
        case IROpcode::STORE_INDEX:   return "sti";
        case IROpcode::PRINT:         return "print";
        case IROpcode::INPUT:         return "input";
        case IROpcode::NOP:           return "nop";
        case IROpcode::HALT:          return "halt";
        case IROpcode::FUNC_BEGIN:    return "func_begin";
        case IROpcode::FUNC_END:      return "func_end";
        case IROpcode::IF_BEGIN:      return "if_begin";
        case IROpcode::IF_ELSE:       return "if_else";
        case IROpcode::IF_END:        return "if_end";
        case IROpcode::WHILE_BEGIN:   return "while_begin";
        case IROpcode::WHILE_END:     return "while_end";
        case IROpcode::FOR_BEGIN:     return "for_begin";
        case IROpcode::FOR_END:       return "for_end";
        case IROpcode::EVENT_BIND:    return "ev_bind";
        case IROpcode::EVENT_TRIGGER: return "ev_trigger";
        case IROpcode::LIB_CALL:      return "lib_call";
        case IROpcode::EVAL:          return "eval";
        case IROpcode::CLASS_BEGIN:   return "class_begin";
        case IROpcode::CLASS_END:     return "class_end";
        case IROpcode::TRY_BEGIN:     return "try_begin";
        case IROpcode::CATCH_BEGIN:   return "catch_begin";
        case IROpcode::AFTER_BEGIN:   return "after_begin";
        case IROpcode::TRY_END:       return "try_end";
        case IROpcode::TAG_BEGIN:     return "tag_begin";
        case IROpcode::TAG_END:       return "tag_end";
        default:                      return "???";
    }
}

// ─── generator ──────────────────────────────────────────────────────────────

class IRGenerator {
    IRProgram        prog;
    IRFunction*      cur  = nullptr;   // current function (null = global)
    int              tc   = 0;         // global temp counter
    int              lc   = 0;         // global label counter
    bool             inMainSection = false; // true once we enter <mainloop>
    bool             inFreeScope   = false; // true inside a <free> block
    std::set<int>    freeDeclaredIds;       // var IDs already FREE_DECL'd in current free scope
    std::string      currentClass_;         // non-empty while inside a bundle body
    std::string      currentCustomTag_;     // non-empty while inside a def tag body
    std::vector<std::string> currentTagObjects_; // ObjDecl names declared inside current custom tag
    std::set<std::string>    glObjects_;    // variable names declared as GL objects via ObjDecl

    // loop label stacks for break/continue
    std::stack<IRRef> loopStart;
    std::stack<IRRef> loopEnd;

    // type tracking: temp id → IRType (propagated from literals and operations)
    std::unordered_map<int,IRType> tempTypes_;

    // Infer IRType from any ref (CONST → embedded type, VAR/TEMP → tracked type)
    IRType typeOfRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST)  return r.value.type;
        if (r.kind == IRRef::Kind::VAR)    return prog.symbols.getType(r.id);
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = tempTypes_.find(r.id);
            return it != tempTypes_.end() ? it->second : IRType::VOID;
        }
        return IRType::VOID;
    }

    // Record the type of a temp or var ref
    void setRefType(const IRRef& r, IRType t) {
        if (r.kind == IRRef::Kind::TEMP) tempTypes_[r.id] = t;
        if (r.kind == IRRef::Kind::VAR)  prog.symbols.setType(r.id, t);
    }

    // ── allocation helpers ──────────────────────────────────────────────────

    IRRef mkTemp() {
        IRRef r; r.kind = IRRef::Kind::TEMP; r.id = tc++;
        return r;
    }
    IRRef mkLabel() {
        IRRef r; r.kind = IRRef::Kind::LABEL; r.id = lc++;
        return r;
    }
    
    // NEW: Use symbol table for variable references
    IRRef mkVar(const std::string& name) {
        int id = prog.symbols.lookup(name);
        if (id < 0) {
            // Variable not in symbol table yet, intern it
            id = prog.symbols.intern(name);
        }
        return IRRef::var(id);
    }
    
    static IRRef mkConst(const std::string& s) {
        return IRRef::constant(IRValue(s));
    }
    static IRRef mkConstInt(int64_t v) {
        return IRRef::constant(IRValue(v));
    }

    // Convert 1-based index (AC) to 0-based (target languages)
    IRRef adjustIndex(const IRRef& idx) {
        if (idx.kind == IRRef::Kind::CONST && idx.value.type == IRType::INT) {
            int64_t n = std::get<int64_t>(idx.value.data);
            return mkConstInt(n - 1);
        }
        // Variable/temp index: emit SUB(idx, 1)
        IRRef one = mkConstInt(1);
        IRRef adj = mkTemp();
        IRInstruction i(IROpcode::SUB, adj, {idx, one});
        emit(std::move(i));
        return adj;
    }

    // ── RAII scope guard ────────────────────────────────────────────────────
    struct ScopeGuard {
        SymbolTable& sym;
        ScopeGuard(SymbolTable& s) : sym(s) { sym.enterScope(); }
        ~ScopeGuard() { sym.exitScope(); }
    };

    // ── emit helpers ────────────────────────────────────────────────────────

    void rawEmit(IRInstruction i) {
        if (cur) {
            cur->instructions.push_back(std::move(i));
        } else {
            if (inMainSection) prog.mainSection.push_back(i);
            else               prog.dataSection.push_back(i);
            prog.globalInit.push_back(std::move(i));
        }
    }

    void emit(IRInstruction i) {
        // Inside a <free> block: auto-emit FREE_DECL before the first assignment to each var
        if (inFreeScope && i.opcode == IROpcode::STORE_VAR
                        && i.result.kind == IRRef::Kind::VAR) {
            int id = i.result.id;
            if (!freeDeclaredIds.count(id)) {
                freeDeclaredIds.insert(id);
                IRInstruction fd(IROpcode::FREE_DECL);
                fd.typedOperands = {i.result};
                rawEmit(std::move(fd));
            }
        }
        rawEmit(std::move(i));
    }

    // emit a LABEL definition
    void emitTag(IROpcode op, const std::string& name) {
        IRInstruction i(op);
        i.typedOperands = {IRRef::constant(IRValue(name))};
        rawEmit(std::move(i)); // bypass free-scope interceptor for structural markers
    }

    void emitLabel(const IRRef& lbl) {
        IRInstruction i(IROpcode::LABEL);
        i.typedOperands = {lbl};
        emit(std::move(i));
    }

    // emit an unconditional jump
    void emitJump(const IRRef& lbl) {
        IRInstruction i(IROpcode::JUMP);
        i.typedOperands = {lbl};
        emit(std::move(i));
    }

    // emit conditional jump (jf cond, label)
    void emitJF(const IRRef& cond, const IRRef& lbl) {
        IRInstruction i(IROpcode::JUMP_IF_FALSE);
        i.typedOperands = {cond, lbl};
        emit(std::move(i));
    }

    // ── expression lowering ─────────────────────────────────────────────────
    // New method: Lower structured AST expression nodes to IR
    IRRef lowerExprNode(const ASTNode& expr) {
        switch (expr.type) {
            case NodeType::LiteralExpr: {
                // Literal values: int, float, string, bool
                if (!expr.attrs.empty()) {
                    const std::string& typeStr = expr.attrs[0];
                    if (typeStr == "INT") {
                        try {
                            return mkConstInt(std::stoll(expr.value));
                        } catch (...) {
                            return mkConst(expr.value);
                        }
                    } else if (typeStr == "FLOAT") {
                        try {
                            return IRRef::constant(IRValue(std::stod(expr.value)));
                        } catch (...) {
                            return mkConst(expr.value);
                        }
                    } else if (typeStr == "STRING") {
                        return mkConst(expr.value);
                    } else if (typeStr == "BOOL") {
                        bool val = (expr.value == "true");
                        return IRRef::constant(IRValue(val));
                    } else if (typeStr == "NULL") {
                        return mkConst("null");
                    } else if (typeStr == "NIL") {
                        return mkConst("nil");
                    }
                }
                // null/nil literal as bare string
                if (expr.value == "null") return mkConst("null");
                if (expr.value == "nil")  return mkConst("nil");
                return mkConst(expr.value);
            }
            
            case NodeType::Identifier: {
                // Remap AC direction constants to string literals
                static const std::unordered_map<std::string,std::string> dirConsts = {
                    {"RightDir","right"}, {"LeftDir","left"},
                    {"UpDir","up"},       {"DownDir","down"},
                };
                auto dc = dirConsts.find(expr.value);
                if (dc != dirConsts.end()) return mkConst(dc->second);
                return mkVar(expr.value);
            }
            
            case NodeType::BinaryExpr: {
                // Binary operation: left op right
                if (expr.children.size() >= 2 && expr.children[0] && expr.children[1]) {
                    IRRef lRef = lowerExprNode(*expr.children[0]);
                    IRRef rRef = lowerExprNode(*expr.children[1]);
                    IRRef dst = mkTemp();

                    // Map operator string to IROpcode
                    IROpcode opcode = IROpcode::NOP;
                    const std::string& op = expr.value;
                    if (op == "+") opcode = IROpcode::ADD;
                    else if (op == "-") opcode = IROpcode::SUB;
                    else if (op == "*") opcode = IROpcode::MUL;
                    else if (op == "@") opcode = IROpcode::PMUL;
                    else if (op == "/") opcode = IROpcode::DIV;
                    else if (op == "//") opcode = IROpcode::IDIV;
                    else if (op == "is") opcode = IROpcode::EQ;
                    else if (op == "#=") opcode = IROpcode::NEQ;
                    else if (op == "<") opcode = IROpcode::LT;
                    else if (op == ">") opcode = IROpcode::GT;
                    else if (op == "#>") opcode = IROpcode::LTE;   // NOT greater = ≤
                    else if (op == "#<") opcode = IROpcode::GTE;   // NOT less = ≥
                    else if (op == "AND" || op == "and" || op == "&") opcode = IROpcode::AND;
                    else if (op == "OR" || op == "or") opcode = IROpcode::OR;
                    else if (op == "|")    opcode = IROpcode::XOR;
                    else if (op == "#|")   opcode = IROpcode::XNOR;
                    else if (op == "xsub") opcode = IROpcode::XSUB;

                    if (op == "overlap") {
                        // Extract base object name (before first dot) from each operand
                        auto extractBase = [this](const IRRef& ref) -> std::string {
                            std::string n;
                            if (ref.kind == IRRef::Kind::VAR) n = prog.symbols.getName(ref.id);
                            else if (ref.kind == IRRef::Kind::CONST && ref.value.type == IRType::STRING)
                                n = std::get<std::string>(ref.value.data);
                            auto dot = n.find('.');
                            return (dot != std::string::npos) ? n.substr(0, dot) : n;
                        };
                        IRRef dst = mkTemp();
                        IRInstruction libcall(IROpcode::LIB_CALL, dst, {});
                        libcall.typedOperands.push_back(mkConst("gl:hitbox.overlap"));
                        libcall.typedOperands.push_back(mkConst(extractBase(lRef)));
                        libcall.typedOperands.push_back(mkConst(extractBase(rRef)));
                        emit(std::move(libcall));
                        return dst;
                    }

                    if (op == "^") {
                        // Exponentiation: compile to math.pow(base, exp)
                        IRRef dst = mkTemp();
                        IRInstruction libcall(IROpcode::LIB_CALL, dst, {});
                        libcall.typedOperands.push_back(mkConst("math:pow"));
                        libcall.typedOperands.push_back(lRef);
                        libcall.typedOperands.push_back(rRef);
                        emit(std::move(libcall));
                        return dst;
                    }

                    if (opcode != IROpcode::NOP) {
                        // Constant folding: both operands are compile-time constants
                        if (lRef.kind == IRRef::Kind::CONST && rRef.kind == IRRef::Kind::CONST) {
                            auto& L = lRef.value;
                            auto& R = rRef.value;
                            bool lFloat = L.type == IRType::FLOAT;
                            bool rFloat = R.type == IRType::FLOAT;
                            bool anyFloat = lFloat || rFloat;
                            auto asDouble = [](const IRValue& v) -> double {
                                if (v.type == IRType::FLOAT) return std::get<double>(v.data);
                                if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
                                return 0.0;
                            };
                            auto asInt = [](const IRValue& v) -> int64_t {
                                if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
                                if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
                                if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
                                return 0;
                            };
                            auto asBool = [&](const IRValue& v) -> bool { return asInt(v) != 0; };
                            IRRef folded;
                            bool did_fold = true;
                            switch (opcode) {
                                case IROpcode::ADD:
                                    if (L.type == IRType::STRING || R.type == IRType::STRING) {
                                        auto toStr = [](const IRValue& v) -> std::string {
                                            if (v.type == IRType::STRING) return std::get<std::string>(v.data);
                                            if (v.type == IRType::INT)    return std::to_string(std::get<int64_t>(v.data));
                                            if (v.type == IRType::FLOAT)  return std::to_string(std::get<double>(v.data));
                                            if (v.type == IRType::BOOL)   return std::get<bool>(v.data) ? "true" : "false";
                                            return "";
                                        };
                                        folded = IRRef::constant(IRValue(toStr(L) + toStr(R)));
                                    } else {
                                        folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)+asDouble(R))) : IRRef::constant(IRValue(asInt(L)+asInt(R)));
                                    }
                                    break;
                                case IROpcode::SUB:  folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)-asDouble(R))) : IRRef::constant(IRValue(asInt(L)-asInt(R))); break;
                                case IROpcode::MUL:
                                case IROpcode::PMUL: folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)*asDouble(R))) : IRRef::constant(IRValue(asInt(L)*asInt(R))); break;
                                case IROpcode::DIV: {
                                    double dR = asDouble(R);
                                    if (dR == 0.0) { did_fold = false; break; }
                                    double q = asDouble(L) / dR;
                                    int64_t qi = (int64_t)q;
                                    folded = (q == (double)qi)
                                             ? IRRef::constant(IRValue(qi))
                                             : IRRef::constant(IRValue(q));
                                    break;
                                }
                                case IROpcode::IDIV: {
                                    int64_t iR = asInt(R);
                                    if (iR == 0) { did_fold = false; break; }
                                    folded = IRRef::constant(IRValue(asInt(L) / iR));
                                    break;
                                }
                                case IROpcode::MOD:
                                    if (asInt(R) != 0) folded = IRRef::constant(IRValue(asInt(L) % asInt(R)));
                                    else did_fold = false;
                                    break;
                                case IROpcode::EQ:   folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)==asDouble(R)) : (asInt(L)==asInt(R)))); break;
                                case IROpcode::NEQ:  folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)!=asDouble(R)) : (asInt(L)!=asInt(R)))); break;
                                case IROpcode::LT:   folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)< asDouble(R)) : (asInt(L)< asInt(R)))); break;
                                case IROpcode::GT:   folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)> asDouble(R)) : (asInt(L)> asInt(R)))); break;
                                case IROpcode::LTE:  folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)<=asDouble(R)) : (asInt(L)<=asInt(R)))); break;
                                case IROpcode::GTE:  folded = IRRef::constant(IRValue(anyFloat ? (asDouble(L)>=asDouble(R)) : (asInt(L)>=asInt(R)))); break;
                                case IROpcode::AND:  folded = IRRef::constant(IRValue(asBool(L) && asBool(R))); break;
                                case IROpcode::OR:   folded = IRRef::constant(IRValue(asBool(L) || asBool(R))); break;
                                case IROpcode::XOR:  folded = IRRef::constant(IRValue(asBool(L) != asBool(R))); break;
                                case IROpcode::XNOR: folded = IRRef::constant(IRValue(asBool(L) == asBool(R))); break;
                                case IROpcode::XSUB: { int64_t d = asInt(L)-asInt(R); folded = IRRef::constant(IRValue((d<0?-d:d)+1)); break; }
                                default: did_fold = false; break;
                            }
                            if (did_fold) return folded;
                        }
                        // Propagate float type: / always produces float; // always produces int; others propagate
                        IRType lt = typeOfRef(lRef), rt = typeOfRef(rRef);
                        IRType resType = (opcode == IROpcode::DIV)
                                         ? IRType::FLOAT
                                         : (opcode == IROpcode::IDIV)
                                           ? IRType::INT
                                           : ((lt == IRType::FLOAT || rt == IRType::FLOAT)
                                              ? IRType::FLOAT : (lt != IRType::VOID ? lt : rt));
                        IRInstruction i(opcode, dst, {lRef, rRef});
                        i.resultType = resType;
                        setRefType(dst, resType);
                        emit(std::move(i));
                        return dst;
                    }
                    // Structured children present but operator not recognised — surface it
                    throw ACError::backend("Preposterous: unknown binary operator '" + expr.value + "'");
                }
                // No structured children: legacy string-based expression from older AST cache
                return lowerExpr(expr.value);
            }
            
            case NodeType::UnaryExpr: {
                // Unary operation: op operand
                if (!expr.children.empty() && expr.children[0]) {
                    IRRef operand = lowerExprNode(*expr.children[0]);
                    IRRef dst = mkTemp();

                    if (expr.value == "-") {
                        // Unary minus: 0 - operand
                        IRRef zero = mkConstInt(0);
                        IRInstruction i(IROpcode::SUB, dst, {zero, operand});
                        emit(std::move(i));
                        return dst;
                    } else if (expr.value == "NOT" || expr.value == "#") {
                        IRInstruction i(IROpcode::NOT, dst, {operand});
                        emit(std::move(i));
                        return dst;
                    }
                    throw ACError::backend("Preposterous: unknown unary operator '" + expr.value + "'");
                }
                return mkConst("");
            }
            
            case NodeType::CallExpr:
            case NodeType::FunctionCall: {
                std::string fname = expr.value;

                // Built-in length function - compile as a direct call (each backend has ac_length)
                if (fname == "length") {
                    if (expr.children.empty() || !expr.children[0]) {
                        throw ACError::backend("length() requires an argument");
                    }
                    IRRef arg = lowerExprNode(*expr.children[0]);
                    IRRef dst = mkTemp();
                    std::vector<IRRef> ops = {mkVar("ac_length"), arg};
                    IRInstruction i(IROpcode::CALL, dst, ops);
                    emit(std::move(i));
                    return dst;
                }

                // Remap GL helper functions to high-level gl:* names (lowering pass handles the rest)
                static const std::unordered_map<std::string,std::string> glFuncMapExpr = {
                    {"is_obj",                 "gl:obj.is"},
                    {"is_draw",                "gl:obj.is_draw"},
                    {"gl_hitbox_many_overlap", "gl:hitbox.many_overlap"},
                    {"gl_hitbox_coords_overlap","gl:hitbox.overlap"},
                    {"gl_frame_begin",         "gl:frame.begin"},
                    {"gl_frame_render",        "gl:frame.render"},
                    {"gl_frame_end",           "gl:frame.end"},
                    {"gl_frame_update",        "gl:frame.update"},
                    {"gl_obj_animate",         "gl:obj.animate"},
                    {"gl_obj_regen",           "gl:obj.regen"},
                    {"gl_screen_init_from_ac", "gl:screen.init"},
                };
                auto fit = glFuncMapExpr.find(fname);
                if (fit != glFuncMapExpr.end()) {
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::LIB_CALL, dst, {});
                    i.typedOperands.push_back(mkConst(fit->second));
                    for (auto& arg : expr.children) i.typedOperands.push_back(lowerExprNode(*arg));
                    emit(std::move(i));
                    return dst;
                }
                // Regular function call
                IRRef dst = mkTemp();
                std::vector<IRRef> ops = {mkVar(fname)};
                for (auto& arg : expr.children) ops.push_back(lowerExprNode(*arg));
                IRInstruction i(IROpcode::CALL, dst, ops);
                emit(std::move(i));
                return dst;
            }
            
            case NodeType::IndexExpr: {
                // Array indexing: array[index]  (AC uses 1-based, targets use 0-based)
                if (expr.children.size() >= 2 && expr.children[0] && expr.children[1]) {
                    IRRef arr = lowerExprNode(*expr.children[0]);
                    IRRef rawIdx = lowerExprNode(*expr.children[1]);
                    IRRef idx = adjustIndex(rawIdx);
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::LOAD_INDEX, dst, {arr, idx});
                    emit(std::move(i));
                    return dst;
                }
                // Fallback: use variable name
                return mkVar(expr.value);
            }

            case NodeType::MethodCall: {
                // Handle Term.ask as an expression (input with prompt)
                const std::string& mname = expr.value;
                auto endsWith = [](const std::string& s, const std::string& suf) {
                    return s.size() >= suf.size() && s.substr(s.size()-suf.size()) == suf;
                };
                if (endsWith(mname, "ask")) {
                    IRRef dst = mkTemp();
                    IRRef prompt = expr.children.empty() ? mkConst("$$") : lowerExprNode(*expr.children[0]);
                    IRInstruction i(IROpcode::INPUT, dst, {prompt});
                    emit(std::move(i));
                    return dst;
                }
                // sure $msg$ as expression: result = sure $Delete?$  → confirm("Delete?")
                if (mname == "sure") {
                    IRRef dst = mkTemp();
                    IRRef arg = expr.children.empty() ? mkConst("$Confirm?$") : lowerExprNode(*expr.children[0]);
                    IRInstruction i(IROpcode::LIB_CALL, dst, {});
                    i.typedOperands = {mkVar("sure"), arg};
                    emit(std::move(i));
                    return dst;
                }
                // GL object method calls used as expressions: receiver.METHOD(args) → gl:obj.METHOD
                {
                    static const std::unordered_map<std::string,std::string> glMethodsExpr = {
                        {"move_y","gl:obj.move_y"},   {"move_x","gl:obj.move_x"},
                        {"vertex","gl:obj.vertex"},   {"curveshape","gl:obj.curveshape"},
                        {"CircleFall","gl:obj.circle_fall"}, {"CircleFell","gl:obj.circle_fell"},
                        {"regen","gl:obj.regen"},     {"set_spawn","gl:obj.set_spawn"},
                        {"animate","gl:obj.animate"}, {"velocity","gl:obj.velocity"},
                        {"set_speed","gl:obj.set_speed"}, {"set_direction","gl:obj.set_direction"},
                        {"overlap_boundary","gl:hitbox.overlap_boundary"},
                    };
                    auto dotPos = mname.find('.');
                    if (dotPos != std::string::npos) {
                        std::string receiver = mname.substr(0, dotPos);
                        std::string method   = mname.substr(dotPos + 1);
                        auto it = glMethodsExpr.find(method);
                        if (it != glMethodsExpr.end()) {
                            IRRef dst = mkTemp();
                            IRInstruction i(IROpcode::LIB_CALL, dst, {});
                            i.typedOperands.push_back(mkConst(it->second));
                            // known GL objects → string literal; parameters/variables → variable ref
                            if (glObjects_.count(receiver) > 0)
                                i.typedOperands.push_back(mkConst(receiver));
                            else
                                i.typedOperands.push_back(mkVar(receiver));
                            for (auto& child : expr.children)
                                i.typedOperands.push_back(lowerExprNode(*child));
                            emit(std::move(i));
                            return dst;
                        }
                    }
                }
                // No-arg property/constant reference or explicit empty-arg call
                if (expr.children.empty()) {
                    bool explicitCall = !expr.attrs.empty() && expr.attrs[0] == "__called__";
                    if (explicitCall) {
                        // ball.x() — empty-arg method call, not property access
                        IRRef dst = mkTemp();
                        std::vector<IRRef> ops = {mkVar(mname)};
                        IRInstruction i(IROpcode::CALL, dst, ops);
                        emit(std::move(i));
                        return dst;
                    }
                    // receiver.method with no args — check for string-cheese methods
                    // Only fires when receiver is NOT a known library namespace
                    auto dotPos2 = mname.rfind('.');
                    if (dotPos2 != std::string::npos && dotPos2 > 0) {
                        std::string recv2 = mname.substr(0, dotPos2);
                        std::string meth2 = mname.substr(dotPos2 + 1);
                        if (recv2.find('.') == std::string::npos) {
                            static const std::unordered_set<std::string> strMethods0 = {
                                "lower","upper","LOWER","UPPER","strip","STRIP",
                                "trim","TRIM","length","len","LEN","format","FORMAT"
                            };
                            static const std::unordered_set<std::string> libNS0 = {
                                "math","os","regex","gl","maudio","camera","widgets",
                                "stringm","term","Term","sidebar","screen"
                            };
                            if (strMethods0.count(meth2) && !libNS0.count(recv2)
                                && !prog.importedLibs.count(recv2)) {
                                IRRef dst = mkTemp();
                                IRInstruction call(IROpcode::LIB_CALL);
                                call.result = dst;
                                call.typedOperands = {mkConst("stringm." + meth2), mkVar(recv2)};
                                emit(std::move(call));
                                return dst;
                            }
                        }
                    }
                    return mkVar(mname);
                }
                // receiver.method(args) — check for string-cheese methods with arguments
                // Only fires when receiver is NOT a known library namespace
                {
                    auto dotPos3 = mname.rfind('.');
                    if (dotPos3 != std::string::npos && dotPos3 > 0) {
                        std::string recv3 = mname.substr(0, dotPos3);
                        std::string meth3 = mname.substr(dotPos3 + 1);
                        if (recv3.find('.') == std::string::npos) {
                            static const std::unordered_set<std::string> strMethodsN = {
                                "find","FIND","strip","STRIP","replace","REPLACE",
                                "startswith","endswith","count","COUNT","split","SPLIT"
                            };
                            // Skip if receiver is a known library namespace
                            static const std::unordered_set<std::string> libNamespaces = {
                                "math","os","regex","gl","maudio","camera","widgets",
                                "stringm","term","Term","sidebar","screen"
                            };
                            if (strMethodsN.count(meth3) && !libNamespaces.count(recv3)
                                && !prog.importedLibs.count(recv3)) {
                                IRRef dst = mkTemp();
                                IRInstruction call(IROpcode::LIB_CALL);
                                call.result = dst;
                                call.typedOperands = {mkConst("stringm." + meth3), mkVar(recv3)};
                                for (auto& arg : expr.children)
                                    call.typedOperands.push_back(lowerExprNode(*arg));
                                emit(std::move(call));
                                return dst;
                            }
                        }
                    }
                }
                // Function call: lib.func(args) → translated to lib_func in codegen
                IRRef dst = mkTemp();
                std::vector<IRRef> ops = {mkVar(mname)};
                for (auto& arg : expr.children)
                    ops.push_back(lowerExprNode(*arg));
                IRInstruction i(IROpcode::CALL, dst, ops);
                emit(std::move(i));
                return dst;
            }

            case NodeType::EvalExpr: {
                IRRef dst = mkTemp();
                IRRef arg = expr.children.empty()
                    ? mkConst("")
                    : lowerExprNode(*expr.children[0]);
                IRInstruction i(IROpcode::EVAL, dst, {arg});
                emit(std::move(i));
                return dst;
            }

            case NodeType::LazyEvalExpr: {
                // lazy_eval(expr) — evaluate expr in a safe try/catch wrapper
                IRRef dst = mkTemp();
                IRRef arg = expr.children.empty()
                    ? mkConst("")
                    : lowerExprNode(*expr.children[0]);
                IRInstruction i(IROpcode::LAZY_EVAL, dst, {arg});
                emit(std::move(i));
                return dst;
            }

            case NodeType::RangeExpr: {
                // range N used as expression (e.g., FOR i in range 5)
                IRRef dst = mkTemp();
                // New parser stores bound as child; legacy uses value string
                IRRef bound = !expr.children.empty()
                    ? lowerExprNode(*expr.children[0])
                    : lowerExpr(expr.value);
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), bound});
                emit(std::move(i));
                return dst;
            }
            case NodeType::SequenceExpr: {
                // sequence(a,b) used as expression (e.g., FOR n in sequence(3,7))
                IRRef dst = mkTemp();
                IRRef aRef, bRef;
                if (expr.children.size() >= 2) {
                    aRef = lowerExprNode(*expr.children[0]);
                    bRef = lowerExprNode(*expr.children[1]);
                } else {
                    // Legacy: value = "a,b"
                    size_t comma = expr.value.find(',');
                    aRef = lowerExpr(comma != std::string::npos ? expr.value.substr(0, comma) : "0");
                    bRef = lowerExpr(comma != std::string::npos ? expr.value.substr(comma + 1) : expr.value);
                }
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), aRef, bRef});
                emit(std::move(i));
                return dst;
            }

            case NodeType::ListLiteral: {
                IRRef dst = mkTemp();
                if (!expr.children.empty()) {
                    std::string content;
                    for (size_t ci = 0; ci < expr.children.size(); ci++) {
                        if (ci > 0) content += ", ";
                        IRRef er = lowerExprNode(*expr.children[ci]);
                        if (er.kind == IRRef::Kind::VAR && er.id >= 0)
                            content += prog.symbols.getName(er.id);
                        else if (er.kind == IRRef::Kind::CONST) {
                            const auto& cv = er.value;
                            if (cv.type == IRType::INT)   content += std::to_string(std::get<int64_t>(cv.data));
                            else if (cv.type == IRType::FLOAT) content += std::to_string(std::get<double>(cv.data));
                            else if (cv.type == IRType::STRING) content += "\"" + std::get<std::string>(cv.data) + "\"";
                            else content += "0";
                        } else content += "_";
                    }
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(content)});
                    emit(std::move(i));
                } else {
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(expr.value)});
                    emit(std::move(i));
                }
                return dst;
            }

            default:
                // Fallback: treat as variable or constant
                if (!expr.value.empty()) {
                    return mkVar(expr.value);
                }
                return mkConst("");
        }
    }

    static bool longIntFromExpr(const ASTNode& expr, std::string& out) {
        struct BigNat {
            enum : uint32_t { BASE = 1000000000u };
            std::vector<uint32_t> d;

            BigNat(uint64_t v = 0) {
                while (v) { d.push_back((uint32_t)(v % BASE)); v /= BASE; }
            }

            bool isZero() const { return d.empty(); }
            void norm() { while (!d.empty() && d.back() == 0) d.pop_back(); }

            static bool parse(const std::string& s, BigNat& out) {
                if (s.empty()) return false;
                BigNat v;
                for (char c : s) {
                    if (!std::isdigit((unsigned char)c)) return false;
                    v = mulSmall(v, 10);
                    v = add(v, BigNat((uint64_t)(c - '0')));
                }
                out = v;
                return true;
            }

            std::string str() const {
                if (d.empty()) return "0";
                std::string s = std::to_string(d.back());
                for (int i = (int)d.size() - 2; i >= 0; --i) {
                    std::string part = std::to_string(d[i]);
                    s += std::string(9 - part.size(), '0') + part;
                }
                return s;
            }

            bool toU64(uint64_t& out) const {
                uint64_t v = 0;
                for (int i = (int)d.size() - 1; i >= 0; --i) {
                    if (v > (std::numeric_limits<uint64_t>::max() - d[i]) / BASE) return false;
                    v = v * BASE + d[i];
                }
                out = v;
                return true;
            }

            static int cmp(const BigNat& a, const BigNat& b) {
                if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
                for (int i = (int)a.d.size() - 1; i >= 0; --i)
                    if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
                return 0;
            }

            static BigNat add(const BigNat& a, const BigNat& b) {
                BigNat r;
                uint64_t carry = 0;
                size_t n = std::max(a.d.size(), b.d.size());
                r.d.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = carry;
                    if (i < a.d.size()) cur += a.d[i];
                    if (i < b.d.size()) cur += b.d[i];
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigNat sub(const BigNat& a, const BigNat& b) {
                BigNat r;
                r.d.resize(a.d.size());
                int64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    int64_t cur = (int64_t)a.d[i] - carry - (i < b.d.size() ? b.d[i] : 0);
                    if (cur < 0) { cur += BASE; carry = 1; }
                    else carry = 0;
                    r.d[i] = (uint32_t)cur;
                }
                r.norm();
                return r;
            }

            static BigNat mulSmall(const BigNat& a, uint32_t m) {
                if (a.isZero() || m == 0) return BigNat();
                BigNat r;
                r.d.resize(a.d.size());
                uint64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t cur = carry + (uint64_t)a.d[i] * m;
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigNat mul(const BigNat& a, const BigNat& b) {
                if (a.isZero() || b.isZero()) return BigNat();
                BigNat r;
                r.d.assign(a.d.size() + b.d.size(), 0);
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < b.d.size() || carry; ++j) {
                        uint64_t cur = r.d[i + j] + carry;
                        if (j < b.d.size()) cur += (uint64_t)a.d[i] * b.d[j];
                        r.d[i + j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                }
                r.norm();
                return r;
            }

            static bool divmod(const BigNat& a, const BigNat& b, BigNat& q, BigNat& rem) {
                if (b.isZero()) return false;
                q.d.assign(a.d.size(), 0);
                rem = BigNat();
                for (int i = (int)a.d.size() - 1; i >= 0; --i) {
                    if (!rem.isZero() || a.d[i] != 0) rem.d.insert(rem.d.begin(), a.d[i]);
                    rem.norm();
                    uint32_t lo = 0, hi = BASE - 1, best = 0;
                    while (lo <= hi) {
                        uint32_t mid = lo + (hi - lo) / 2;
                        BigNat prod = mulSmall(b, mid);
                        if (cmp(prod, rem) <= 0) {
                            best = mid;
                            if (mid == BASE - 1) break;
                            lo = mid + 1;
                        } else {
                            if (mid == 0) break;
                            hi = mid - 1;
                        }
                    }
                    q.d[i] = best;
                    if (best) rem = sub(rem, mulSmall(b, best));
                }
                q.norm();
                rem.norm();
                return true;
            }
        };

        // LongInt bounds (signed 96-bit): min = -(2^95), max = 2^95 - 1
        static const BigNat LONGINT_MAX_MAG = []{
            BigNat v;
            BigNat::parse("39614081257132168796771975167", v); // 2^95 - 1
            return v;
        }();
        static const BigNat LONGINT_MIN_MAG = []{
            BigNat v;
            BigNat::parse("39614081257132168796771975168", v); // 2^95
            return v;
        }();

        struct BigInt {
            bool neg = false;
            BigNat mag;
            void norm() { if (mag.isZero()) neg = false; }
        };

        auto parseSignedDecimal = [](const std::string& s, BigInt& out) -> bool {
            if (s.empty()) return false;
            std::string t = s;
            bool neg = false;
            if (!t.empty() && t[0] == '-') { neg = true; t.erase(0, 1); }
            if (t.empty()) return false;
            BigNat m;
            if (!BigNat::parse(t, m)) return false;
            out.neg = neg;
            out.mag = m;
            out.norm();
            return true;
        };

        auto inSigned96 = [&](const BigInt& v) -> bool {
            if (!v.neg) return BigNat::cmp(v.mag, LONGINT_MAX_MAG) <= 0;
            return BigNat::cmp(v.mag, LONGINT_MIN_MAG) <= 0;
        };

        auto addSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt r;
            if (a.neg == b.neg) {
                r.neg = a.neg;
                r.mag = BigNat::add(a.mag, b.mag);
                r.norm();
                return r;
            }
            int c = BigNat::cmp(a.mag, b.mag);
            if (c == 0) return BigInt{};
            if (c > 0) { r.neg = a.neg; r.mag = BigNat::sub(a.mag, b.mag); }
            else       { r.neg = b.neg; r.mag = BigNat::sub(b.mag, a.mag); }
            r.norm();
            return r;
        };

        auto subSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt nb = b;
            nb.neg = !nb.neg;
            nb.norm();
            return addSigned(a, nb);
        };

        auto mulSigned = [&](const BigInt& a, const BigInt& b) -> BigInt {
            BigInt r;
            r.neg = (a.neg != b.neg);
            r.mag = BigNat::mul(a.mag, b.mag);
            r.norm();
            return r;
        };

        auto divmodSigned = [&](const BigInt& a, const BigInt& b, BigInt& q, BigInt& rem) -> bool {
            if (b.mag.isZero()) return false;
            BigNat qmag, rmag;
            if (!BigNat::divmod(a.mag, b.mag, qmag, rmag)) return false;
            q.neg = (a.neg != b.neg);
            q.mag = qmag;
            q.norm();
            rem.neg = a.neg; // C-like remainder sign = dividend sign
            rem.mag = rmag;
            rem.norm();
            return true;
        };

        std::function<bool(const ASTNode&, BigInt&)> eval = [&](const ASTNode& n, BigInt& outv) -> bool {
            if (n.type == NodeType::LiteralExpr && !n.attrs.empty() && n.attrs[0] == "INT")
                return parseSignedDecimal(n.value, outv);
            if (n.type == NodeType::NumberLit)
                return parseSignedDecimal(n.value, outv);

            if (n.type == NodeType::UnaryExpr) {
                if (n.children.empty()) return false;
                if (!eval(*n.children[0], outv)) return false;
                if (n.value == "-") { outv.neg = !outv.neg; outv.norm(); return true; }
                return false;
            }

            if ((n.type == NodeType::CallExpr || n.type == NodeType::MethodCall) && n.children.size() == 2) {
                BigInt a, b;
                if (!eval(*n.children[0], a) || !eval(*n.children[1], b)) return false;
                if (n.value == "pow" || n.value == "math.pow") {
                    if (b.neg) return false;
                    uint64_t exp = 0;
                    if (!b.mag.toU64(exp) || exp > 4096) return false;
                    BigInt result;
                    result.mag = BigNat(1);
                    BigInt base = a;
                    while (exp > 0) {
                        if (exp & 1) result = mulSigned(result, base);
                        exp >>= 1;
                        if (exp) base = mulSigned(base, base);
                    }
                    outv = result;
                    return true;
                }
                if (n.value == "mod" || n.value == "math.mod" || n.value == "math.mod_int") {
                    BigInt q, r;
                    if (!divmodSigned(a, b, q, r)) return false;
                    outv = r;
                    return true;
                }
                return false;
            }

            if (n.type == NodeType::BinaryExpr && n.children.size() >= 2) {
                BigInt l, r;
                if (!eval(*n.children[0], l) || !eval(*n.children[1], r)) return false;
                const std::string& op = n.value;
                if (op == "+") outv = addSigned(l, r);
                else if (op == "-") outv = subSigned(l, r);
                else if (op == "*" || op == "@") outv = mulSigned(l, r);
                else if (op == "/") {
                    BigInt q, rem;
                    if (!divmodSigned(l, r, q, rem)) return false;
                    outv = q;
                } else return false;
                return true;
            }

            return false;
        };

        BigInt v;
        if (!eval(expr, v)) return false;
        if (!inSigned96(v)) return false;
        out = (v.neg ? "-" : "") + v.mag.str();
        return true;
    }

    static void normalizeGoodDecPair(std::string& unscaledSigned, int64_t& scale) {
        // Canonicalize: strip leading zeros from magnitude, and if scale < 0
        // strip trailing zeros from magnitude while increasing scale.
        bool neg = false;
        std::string mag = unscaledSigned;
        if (!mag.empty() && mag[0] == '-') { neg = true; mag.erase(0, 1); }
        if (mag.empty()) mag = "0";

        size_t nz = mag.find_first_not_of('0');
        if (nz == std::string::npos) {
            mag = "0";
            neg = false;
            scale = 0;
        } else if (nz > 0) {
            mag.erase(0, nz);
        }

        while (scale < 0 && mag.size() > 1 && mag.back() == '0') {
            mag.pop_back();
            scale += 1;
        }

        unscaledSigned = (neg ? "-" : "") + mag;
        if (unscaledSigned == "-0") unscaledSigned = "0";
    }

    static bool goodDecPairFromExpr(const ASTNode& expr, std::string& unscaledOut, int64_t& scaleOut) {
        struct BigInt {
            enum : uint32_t { BASE = 1000000000u };
            bool neg = false;
            std::vector<uint32_t> d; // little-endian base 1e9

            BigInt(int64_t v = 0) {
                if (v < 0) { neg = true; v = -v; }
                uint64_t u = (uint64_t)v;
                while (u) { d.push_back((uint32_t)(u % BASE)); u /= BASE; }
            }

            bool isZero() const { return d.empty(); }
            void norm() {
                while (!d.empty() && d.back() == 0) d.pop_back();
                if (d.empty()) neg = false;
            }

            static int cmpAbs(const BigInt& a, const BigInt& b) {
                if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
                for (int i = (int)a.d.size() - 1; i >= 0; --i)
                    if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
                return 0;
            }

            static BigInt addAbs(const BigInt& a, const BigInt& b) {
                BigInt r;
                uint64_t carry = 0;
                size_t n = std::max(a.d.size(), b.d.size());
                r.d.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = carry;
                    if (i < a.d.size()) cur += a.d[i];
                    if (i < b.d.size()) cur += b.d[i];
                    r.d[i] = (uint32_t)(cur % BASE);
                    carry = cur / BASE;
                }
                if (carry) r.d.push_back((uint32_t)carry);
                return r;
            }

            static BigInt subAbs(const BigInt& a, const BigInt& b) {
                // |a| - |b|, assuming |a| >= |b|
                BigInt r;
                r.d.resize(a.d.size());
                int64_t carry = 0;
                for (size_t i = 0; i < a.d.size(); ++i) {
                    int64_t cur = (int64_t)a.d[i] - carry - (i < b.d.size() ? b.d[i] : 0);
                    if (cur < 0) { cur += BASE; carry = 1; }
                    else carry = 0;
                    r.d[i] = (uint32_t)cur;
                }
                r.norm();
                return r;
            }

            static BigInt add(const BigInt& a, const BigInt& b) {
                if (a.neg == b.neg) {
                    BigInt r = addAbs(a, b);
                    r.neg = a.neg;
                    r.norm();
                    return r;
                }
                int c = cmpAbs(a, b);
                if (c == 0) return BigInt();
                if (c > 0) {
                    BigInt r = subAbs(a, b);
                    r.neg = a.neg;
                    r.norm();
                    return r;
                }
                BigInt r = subAbs(b, a);
                r.neg = b.neg;
                r.norm();
                return r;
            }

            static BigInt sub(const BigInt& a, const BigInt& b) {
                BigInt nb = b;
                if (!nb.isZero()) nb.neg = !nb.neg;
                return add(a, nb);
            }

            static BigInt mul(const BigInt& a, const BigInt& b) {
                if (a.isZero() || b.isZero()) return BigInt();
                BigInt r;
                r.neg = (a.neg != b.neg);
                r.d.assign(a.d.size() + b.d.size(), 0);
                for (size_t i = 0; i < a.d.size(); ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < b.d.size() || carry; ++j) {
                        uint64_t cur = r.d[i + j] + carry;
                        if (j < b.d.size()) cur += (uint64_t)a.d[i] * b.d[j];
                        r.d[i + j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                }
                r.norm();
                return r;
            }

            static BigInt mulPow10(const BigInt& a, int64_t p) {
                if (a.isZero()) return BigInt();
                if (p <= 0) return a;
                BigInt r = a;
                for (int64_t i = 0; i < p; ++i) {
                    uint64_t carry = 0;
                    for (size_t j = 0; j < r.d.size(); ++j) {
                        uint64_t cur = carry + (uint64_t)r.d[j] * 10u;
                        r.d[j] = (uint32_t)(cur % BASE);
                        carry = cur / BASE;
                    }
                    if (carry) r.d.push_back((uint32_t)carry);
                }
                r.norm();
                return r;
            }

            std::string absStr() const {
                if (d.empty()) return "0";
                std::string s = std::to_string(d.back());
                for (int i = (int)d.size() - 2; i >= 0; --i) {
                    std::string part = std::to_string(d[i]);
                    s += std::string(9 - part.size(), '0') + part;
                }
                return s;
            }
        };

        struct BigDec {
            BigInt unscaled;
            int64_t scale = 0; // base-10 exponent: value = unscaled * 10^scale
        };

        auto parseDecimalLiteral = [&](const std::string& s, BigDec& outDec) -> bool {
            if (s.empty()) return false;
            std::string t = s;
            bool neg = false;
            if (!t.empty() && t[0] == '-') { neg = true; t.erase(0, 1); }
            if (t.empty()) return false;
            if (t.find_first_of("eE") != std::string::npos) return false;
            size_t dot = t.find('.');
            std::string intPart = (dot == std::string::npos) ? t : t.substr(0, dot);
            std::string fracPart = (dot == std::string::npos) ? "" : t.substr(dot + 1);
            if (intPart.empty()) intPart = "0";
            if (intPart.empty() && fracPart.empty()) return false;
            for (char c : intPart) if (!std::isdigit((unsigned char)c)) return false;
            for (char c : fracPart) if (!std::isdigit((unsigned char)c)) return false;

            // Remove leading zeros from integer part only; keep fractional zeros for scale.
            size_t nz = intPart.find_first_not_of('0');
            if (nz == std::string::npos) intPart = "0";
            else if (nz > 0) intPart.erase(0, nz);

            std::string digits = intPart + fracPart;
            // Strip leading zeros in combined digits to keep unscaled minimal.
            size_t dnz = digits.find_first_not_of('0');
            if (dnz == std::string::npos) digits = "0";
            else if (dnz > 0) digits.erase(0, dnz);

            BigInt u;
            u.neg = neg;
            // parse digits into BigInt (base 1e9)
            for (char c : digits) {
                if (!std::isdigit((unsigned char)c)) return false;
                // u = u*10 + (c-'0')
                uint64_t carry = (uint64_t)(c - '0');
                uint64_t ccarry = 0;
                for (size_t i = 0; i < u.d.size(); ++i) {
                    uint64_t cur = (uint64_t)u.d[i] * 10u + ccarry;
                    u.d[i] = (uint32_t)(cur % BigInt::BASE);
                    ccarry = cur / BigInt::BASE;
                }
                if (ccarry) u.d.push_back((uint32_t)ccarry);
                // add carry to u (least limb)
                size_t idx = 0;
                while (carry) {
                    if (idx >= u.d.size()) u.d.push_back(0);
                    uint64_t cur = (uint64_t)u.d[idx] + carry;
                    u.d[idx] = (uint32_t)(cur % BigInt::BASE);
                    carry = cur / BigInt::BASE;
                    idx++;
                }
            }
            u.norm();

            outDec.unscaled = u;
            outDec.scale = -(int64_t)fracPart.size();
            return true;
        };

        std::function<bool(const ASTNode&, BigDec&)> eval = [&](const ASTNode& n, BigDec& outDec) -> bool {
            if (n.type == NodeType::LiteralExpr && !n.attrs.empty()) {
                if (n.attrs[0] == "INT" || n.attrs[0] == "FLOAT") {
                    return parseDecimalLiteral(n.value, outDec);
                }
            }

            if (n.type == NodeType::UnaryExpr && n.value == "-" && !n.children.empty()) {
                BigDec inner;
                if (!eval(*n.children[0], inner)) return false;
                if (!inner.unscaled.isZero()) inner.unscaled.neg = !inner.unscaled.neg;
                outDec = inner;
                return true;
            }

            if (n.type == NodeType::BinaryExpr && n.children.size() >= 2) {
                BigDec a, b;
                if (!eval(*n.children[0], a) || !eval(*n.children[1], b)) return false;
                const std::string& op = n.value;

                if (op == "+" || op == "-") {
                    int64_t targetScale = std::min(a.scale, b.scale);
                    BigInt au = BigInt::mulPow10(a.unscaled, a.scale - targetScale);
                    BigInt bu = BigInt::mulPow10(b.unscaled, b.scale - targetScale);
                    BigInt ru = (op == "+") ? BigInt::add(au, bu) : BigInt::sub(au, bu);
                    outDec.unscaled = ru;
                    outDec.scale = targetScale;
                    return true;
                }

                if (op == "*") {
                    outDec.unscaled = BigInt::mul(a.unscaled, b.unscaled);
                    outDec.scale = a.scale + b.scale;
                    return true;
                }
            }

            return false;
        };

        BigDec v;
        if (!eval(expr, v)) return false;
        std::string signedUnscaled = (v.unscaled.neg ? "-" : "") + v.unscaled.absStr();
        int64_t scale = v.scale;
        normalizeGoodDecPair(signedUnscaled, scale);
        unscaledOut = signedUnscaled;
        scaleOut = scale;
        return true;
    }
    
    // Returns the IRRef holding the result of the expression string.
    // For simple names/literals we just return a VAR or CONST ref.
    // For compound expressions we emit arithmetic and return a temp.

    IRRef lowerExpr(const std::string& expr) {
        if (expr.empty()) return mkConst("");

        // strip outer $…$ string literal
        if (expr.size() >= 2 && expr.front() == '$' && expr.back() == '$') {
            return mkConst(expr.substr(1, expr.size() - 2));
        }

        // integer literal
        bool isInt = !expr.empty();
        for (char c : expr) if (!std::isdigit((unsigned char)c) && c != '-') { isInt = false; break; }
        if (isInt) {
            try { return mkConstInt(std::stoll(expr)); } catch (...) {}
        }

        // boolean / null / nil literals
        if (expr == "true")  return IRRef::constant(IRValue(true));
        if (expr == "false") return IRRef::constant(IRValue(false));
        if (expr == "null")  return mkConst("null");
        if (expr == "nil")   return mkConst("nil");

        // receiver.method in expression context (no parens) — promote to call
        {
            auto dotPos = expr.rfind('.');
            if (dotPos != std::string::npos && dotPos > 0) {
                std::string receiver = expr.substr(0, dotPos);
                std::string method   = expr.substr(dotPos + 1);
                // Only when receiver is a plain variable (no dots — not a library namespace)
                if (receiver.find('.') == std::string::npos) {
                    // String-cheese methods: lower(s), upper(s), strip(s), find(s,p), trim(s), len(s)
                    static const std::unordered_set<std::string> strMethods = {
                        "lower","upper","LOWER","UPPER","strip","STRIP",
                        "trim","TRIM","length","len","LEN","format","FORMAT"
                    };
                    // Widget / object no-arg methods: get(), mainloop(), update()
                    static const std::unordered_set<std::string> noArgMethods = {
                        "get","mainloop","update","destroy","pack","clear"
                    };
                    static const std::unordered_set<std::string> libNS2 = {
                        "math","os","regex","gl","maudio","camera","widgets",
                        "stringm","term","Term","sidebar","screen"
                    };
                    if (strMethods.count(method) && !libNS2.count(receiver)
                        && !prog.importedLibs.count(receiver)) {
                        // Emit: stringm.method(receiver)
                        IRRef t = mkTemp();
                        IRInstruction call(IROpcode::LIB_CALL);
                        call.result = t;
                        call.typedOperands = {mkConst("stringm." + method), mkVar(receiver)};
                        emit(std::move(call));
                        return t;
                    } else if (noArgMethods.count(method)) {
                        // Emit: receiver.method() — widget/object no-arg call
                        IRRef t = mkTemp();
                        IRInstruction call(IROpcode::LIB_CALL);
                        call.result = t;
                        call.typedOperands = {mkVar(expr)};  // codegen emits expr() with no args
                        emit(std::move(call));
                        return t;
                    }
                }
            }
        }

        // compound arithmetic: detect binary operator (+, -, *, /)
        // simple single-pass scan (handles "a + b", "a - b", etc.)
        // We look for the last +/- or first */ outside parens
        int depth = 0;
        int opPos = -1;
        IROpcode opcode = IROpcode::NOP;

        // scan right-to-left for +/- (lowest precedence)
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            char c = expr[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && (c == '+' || c == '-') && i > 0) {
                opPos = i;
                opcode = (c == '+') ? IROpcode::ADD : IROpcode::SUB;
                break;
            }
        }
        // if no +/-, scan for */@ (higher precedence)
        if (opPos == -1) {
            depth = 0;
            for (int i = (int)expr.size() - 1; i >= 0; --i) {
                char c = expr[i];
                if (c == ')') depth++;
                else if (c == '(') depth--;
                else if (depth == 0 && (c == '*' || c == '/' || c == '@') && i > 0) {
                    opPos = i;
                    opcode = (c == '*') ? IROpcode::MUL : (c == '@') ? IROpcode::PMUL : IROpcode::DIV;
                    break;
                }
            }
        }

        if (opPos > 0) {
            std::string lhs = expr.substr(0, opPos);
            std::string rhs = expr.substr(opPos + 1);
            // trim spaces
            while (!lhs.empty() && lhs.back() == ' ') lhs.pop_back();
            while (!rhs.empty() && rhs.front() == ' ') rhs.erase(rhs.begin());

            IRRef lRef = lowerExpr(lhs);
            IRRef rRef = lowerExpr(rhs);
            IRRef dst  = mkTemp();

            IRInstruction i(opcode, dst, {lRef, rRef});
            emit(std::move(i));
            return dst;
        }

        // function call inside expression: name(args)
        auto paren = expr.find('(');
        if (paren != std::string::npos && expr.back() == ')') {
            std::string fname = expr.substr(0, paren);
            std::string argsStr = expr.substr(paren + 1, expr.size() - paren - 2);
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::CALL, dst, {mkVar(fname), mkConst(argsStr)});
            emit(std::move(i));
            return dst;
        }

        // plain variable name
        return mkVar(expr);
    }

    // ── compound assignment helper ──────────────────────────────────────────

    void emitCompound(IROpcode op, const std::string& varName, const std::string& rhs) {
        IRRef lRef = mkVar(varName);
        IRRef rRef = lowerExpr(rhs);
        IRRef tmp  = mkTemp();
        emit(IRInstruction(op, tmp, {lRef, rRef}));
        IRInstruction st(IROpcode::STORE_VAR);
        st.typedOperands = {mkVar(varName), tmp};
        emit(std::move(st));
    }

    // ── else-chain helper (high-level IR only) ──────────────────────────────
    // Processes ElseIfStmt/OTHER children as nested if-else-endif blocks so
    // Python/JS backends get proper `else: if:` nesting instead of a flat list.
    void genElseChain(const std::vector<std::unique_ptr<ASTNode>>& children, size_t idx) {
        if (idx >= children.size()) return;
        const ASTNode& node = *children[idx];

        if (node.type == NodeType::ElseIfStmt) {
            if (node.children.empty() || node.children[0]->type == NodeType::Block)
                return; // malformed elseif — no condition
            IRRef condRef = lowerExprNode(*node.children[0]);
            size_t bodyIdx = 1;

            IRInstruction ifBegin(IROpcode::IF_BEGIN);
            ifBegin.typedOperands = {condRef};
            emit(std::move(ifBegin));

            if (bodyIdx < node.children.size()) gen(*node.children[bodyIdx]);

            if (idx + 1 < children.size()) {
                emit(IRInstruction(IROpcode::IF_ELSE));
                genElseChain(children, idx + 1);
            }

            emit(IRInstruction(IROpcode::IF_END));
        } else if (node.type == NodeType::IfStmt && node.value == "OTHER") {
            if (!node.children.empty()) gen(*node.children[0]);
        }
    }

    // ── node visitor ────────────────────────────────────────────────────────

    void gen(const ASTNode& n) {
        switch (n.type) {

        case NodeType::Program:
        case NodeType::Block:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::TagBlock: {
            const std::string& tag = n.value;

            // Local: truly transparent — no IR markers
            if (tag == "Local") { for (auto& c : n.children) gen(*c); break; }

            // mainloop: entry point — switch to main section
            if (tag == "mainloop") {
                prog.hadExplicitMainloop = true;
                inMainSection = true;
                emitTag(IROpcode::TAG_BEGIN, tag);
                for (auto& c : n.children) gen(*c);
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // StartHere: entry point — GL game loop
            // Python: while True: t=gl_frame_begin(); if not t: break; ...body...; render(); end()
            if (tag == "StartHere") {
                prog.hadExplicitMainloop = true;
                inMainSection = true;
                emitTag(IROpcode::TAG_BEGIN, tag);
                if (prog.useHighLevelIR) {
                    IRRef breakSentinel = mkConst("__break__");
                    loopEnd.push(breakSentinel);
                    loopStart.push(mkConst("__continue__"));

                    emit(IRInstruction(IROpcode::WHILE_BEGIN));

                    // t = gl:frame.begin(); if not t: break
                    IRRef dst = mkTemp();
                    IRInstruction callBegin(IROpcode::LIB_CALL, dst, {});
                    callBegin.typedOperands.push_back(mkConst("gl:frame.begin"));
                    emit(std::move(callBegin));
                    IRInstruction jfb(IROpcode::JUMP_IF_FALSE);
                    jfb.typedOperands = {dst, breakSentinel};
                    emit(std::move(jfb));

                    for (auto& c : n.children) gen(*c);

                    // gl:frame.render(); gl:frame.end() at bottom of loop
                    IRInstruction callRender(IROpcode::LIB_CALL);
                    callRender.typedOperands.push_back(mkConst("gl:frame.render"));
                    emit(std::move(callRender));
                    IRInstruction callEnd(IROpcode::LIB_CALL);
                    callEnd.typedOperands.push_back(mkConst("gl:frame.end"));
                    emit(std::move(callEnd));

                    emit(IRInstruction(IROpcode::WHILE_END));
                    loopEnd.pop(); loopStart.pop();
                } else {
                    IRRef loopL = mkLabel();
                    emitLabel(loopL);
                    for (auto& c : n.children) gen(*c);
                    emitJump(loopL);
                }
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // bound: scoped block — codegen emits { } or if True: as appropriate
            if (tag == "bound") {
                emitTag(IROpcode::TAG_BEGIN, tag);
                for (auto& c : n.children) gen(*c);
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // free: variables assigned inside become globally accessible
            if (tag == "free") {
                emitTag(IROpcode::TAG_BEGIN, tag);
                bool prevFree = inFreeScope;
                std::set<int> prevFreeIds;
                if (!prevFree) {
                    inFreeScope = true;
                    freeDeclaredIds.clear();
                } else {
                    prevFreeIds = freeDeclaredIds;
                }
                for (auto& c : n.children) gen(*c);
                if (!prevFree) {
                    inFreeScope = false;
                    freeDeclaredIds.clear();
                } else {
                    freeDeclaredIds = prevFreeIds;
                }
                emitTag(IROpcode::TAG_END, tag);
                break;
            }

            // shutoff: user-defined cleanup — compiled as __ac_shutoff__ function
            if (tag == "shutoff") {
                prog.functions.push_back(IRFunction("__ac_shutoff__"));
                IRFunction* prevCur = cur;
                cur = &prog.functions.back();
                for (auto& c : n.children) gen(*c);
                emit(IRInstruction(IROpcode::RETURN));
                cur = prevCur;
                prog.hasShutoff = true;
                break;
            }

            // Custom tag invocation: <terrain> inside a block → call spawn_terrain()
            {
                std::string spawnName = "spawn_" + tag;
                bool isCustom = false;
                for (auto& f : prog.functions)
                    if (f.name == spawnName) { isCustom = true; break; }
                if (isCustom) {
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::CALL, dst, {mkVar(spawnName)});
                    emit(std::move(i));
                    break;
                }
            }

            // All other tags (gui, SCREEN, LOGIC, OBJECT, standard): emit TAG markers + walk children
            emitTag(IROpcode::TAG_BEGIN, tag);
            for (auto& c : n.children) gen(*c);
            emitTag(IROpcode::TAG_END, tag);
            break;
        }

        case NodeType::BackendDecl:
        case NodeType::SaveStmt:
            break; // metadata only

        // ── function definition ─────────────────────────────────────────────
        case NodeType::FuncDef: {
            // Intern function name in symbol table
            int funcId = prog.symbols.intern(
                currentClass_.empty() ? n.value : currentClass_ + "." + n.value,
                IRType::FUNCTION);

            IRFunction fn(n.value, currentClass_);
            fn.returnType = IRType::VOID;

            // Scope guard: guarantees exitScope even if gen() throws
            ScopeGuard scope(prog.symbols);

            // Methods receive 'self' as first parameter.
            // If the user explicitly wrote func(self), do not duplicate it.
            if (!currentClass_.empty()) {
                bool userHasSelf = (!n.attrs.empty() && n.attrs[0] == "self");
                if (!userHasSelf) {
                    prog.symbols.intern("self");
                    fn.parameters.push_back("self");
                }
            }

            // Intern parameters in symbol table
            for (auto& p : n.attrs) {
                prog.symbols.intern(p);
                fn.parameters.push_back(p);
            }

            prog.functions.push_back(std::move(fn));
            cur = &prog.functions.back();

            // function entry label
            IRInstruction entry(IROpcode::FUNC_BEGIN);
            entry.typedOperands = {IRRef::func(funcId)};
            emit(std::move(entry));

            if (!n.children.empty()) gen(*n.children[0]);

            // implicit void return if no explicit return
            IRInstruction ret(IROpcode::RETURN);
            ret.typedOperands = {};
            emit(std::move(ret));

            IRInstruction end(IROpcode::FUNC_END);
            end.typedOperands = {IRRef::func(funcId)};
            emit(std::move(end));

            cur = nullptr;
            break;
        }

        // ── bundle (class) definition ────────────────────────────────────────
        case NodeType::BundleDef: {
            const std::string& className = n.value;

            // CLASS_BEGIN marker in globalInit
            IRInstruction cb(IROpcode::CLASS_BEGIN);
            cb.typedOperands = {mkConst(className)};
            emit(std::move(cb));

            // Lower body inside class context.
            // Bundle field defaults should become instance fields (set in init), not class/static vars.
            currentClass_ = className;
            std::vector<std::pair<std::string, const ASTNode*>> fieldDefaults;
            bool hasUserInit = false;
            // Body is either: old style — one Block child; new style — BundleMember children.
            // Normalize to a flat list of (access, stmt*) pairs.
            std::vector<std::pair<std::string, const ASTNode*>> bodyStmts;
            if (!n.children.empty() && n.children[0] && n.children[0]->type == NodeType::Block) {
                for (const auto& sp : n.children[0]->children)
                    if (sp) bodyStmts.push_back({"public", sp.get()});
            } else {
                for (const auto& mp : n.children) {
                    if (!mp) continue;
                    const std::string& acc = (mp->type == NodeType::BundleMember) ? mp->value : "public";
                    const ASTNode* stmt = (mp->type == NodeType::BundleMember && !mp->children.empty())
                                          ? mp->children[0].get() : mp.get();
                    if (stmt) bodyStmts.push_back({acc, stmt});
                }
            }

            if (!bodyStmts.empty()) {
                // Collect field defaults and detect init method
                for (const auto& [acc, stmtRaw] : bodyStmts) {
                    (void)acc;
                    const auto& stmt = *stmtRaw;
                    if (stmt.type == NodeType::AssignStmt) {
                        // Only treat bare identifiers as fields (no dots).
                        if (stmt.value.find('.') == std::string::npos) {
                            if (!stmt.children.empty())
                                fieldDefaults.push_back({stmt.value, stmt.children[0].get()});
                            else if (!stmt.attrs.empty()) {
                                // Legacy string-based assignment: lowerExpr() later
                                // Store nullptr to signal legacy; handled below.
                                fieldDefaults.push_back({stmt.value, nullptr});
                            }
                        }
                    } else if (stmt.type == NodeType::FuncDef && stmt.value == "init") {
                        hasUserInit = true;
                    }
                }

                // Emit a synthetic init if user didn't define one but there are fields.
                if (!hasUserInit && !fieldDefaults.empty()) {
                    int funcId = prog.symbols.intern(className + ".init", IRType::FUNCTION);
                    IRFunction fn("init", className);
                    fn.returnType = IRType::VOID;
                    prog.symbols.intern("self");
                    fn.parameters.push_back("self");
                    prog.functions.push_back(std::move(fn));
                    cur = &prog.functions.back();
                    IRInstruction entry(IROpcode::FUNC_BEGIN);
                    entry.typedOperands = {IRRef::func(funcId)};
                    emit(std::move(entry));

                    // Field initializers
                    for (const auto& [field, exprNode] : fieldDefaults) {
                        IRRef dst = mkVar("self." + field);
                        if (exprNode) {
                            IRRef src = lowerExprNode(*exprNode);
                            IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                            i.resultType = typeOfRef(src);
                            emit(std::move(i));
                        } else {
                            // Legacy: value stored in attrs[0] on the AssignStmt. Re-lower from string.
                            // (We don't have the original string here; default to 0.)
                            IRInstruction i(IROpcode::STORE_VAR, dst, {mkConstInt(0)});
                            i.resultType = IRType::INT;
                            emit(std::move(i));
                        }
                    }

                    IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                    IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                    cur = nullptr;
                }

                // Lower remaining statements:
                // - Skip bare field-default AssignStmt (handled via init)
                // - For user-defined init, we'll prepend initializers by emitting them right after FUNC_BEGIN.
                for (const auto& [acc2, stmtRaw2] : bodyStmts) {
                    (void)acc2;
                    if (!stmtRaw2) continue;
                    const auto& stmt = *stmtRaw2;
                    if (stmt.type == NodeType::AssignStmt && stmt.value.find('.') == std::string::npos)
                        continue; // field default

                    if (stmt.type == NodeType::FuncDef && stmt.value == "init" && !fieldDefaults.empty()) {
                        // Manual lowering of init so we can inject field initializers at the beginning.
                        int funcId = prog.symbols.intern(className + ".init", IRType::FUNCTION);
                        IRFunction fn("init", className);
                        fn.returnType = IRType::VOID;

                        ScopeGuard scope(prog.symbols);

                        // Parameters: include user args; ensure self exists once.
                        prog.symbols.intern("self");
                        fn.parameters.push_back("self");
                        for (auto& p : stmt.attrs) {
                            if (p == "self") continue;
                            prog.symbols.intern(p);
                            fn.parameters.push_back(p);
                        }
                        prog.functions.push_back(std::move(fn));
                        cur = &prog.functions.back();
                        IRInstruction entry(IROpcode::FUNC_BEGIN);
                        entry.typedOperands = {IRRef::func(funcId)};
                        emit(std::move(entry));

                        // Inject field initializers before user body.
                        for (const auto& [field, exprNode] : fieldDefaults) {
                            IRRef dst = mkVar("self." + field);
                            if (exprNode) {
                                IRRef src = lowerExprNode(*exprNode);
                                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                                i.resultType = typeOfRef(src);
                                emit(std::move(i));
                            } else {
                                IRInstruction i(IROpcode::STORE_VAR, dst, {mkConstInt(0)});
                                i.resultType = IRType::INT;
                                emit(std::move(i));
                            }
                        }

                        if (!stmt.children.empty()) gen(*stmt.children[0]);
                        IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                        IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                        cur = nullptr;
                        continue;
                    }

                    // Default: lower normally.
                    gen(stmt);
                }
            }

            currentClass_ = "";

            // CLASS_END marker
            IRInstruction ce(IROpcode::CLASS_END);
            ce.typedOperands = {mkConst(className)};
            emit(std::move(ce));
            break;
        }

        // ── assignment ──────────────────────────────────────────────────────
        case NodeType::AssignStmt: {
            // Const reassignment guard
            if (prog.constVars.count(n.value)) {
                std::cerr << "Preposterous: Cannot reassign const variable '" << n.value << "'\n";
                break;
            }
            IRRef dst = mkVar(n.value);

            // Check for special assignment types first (before checking children)
            if (!n.attrs.empty()) {
                const std::string& raw = n.attrs[0];
                
                // Function call result stored in variable
                if (raw.substr(0, 11) == "__funcall__") {
                    if (!n.children.empty() && n.children[0]->type == NodeType::FunctionCall) {
                        auto& fc = *n.children[0];
                        std::vector<IRRef> ops = {mkVar(fc.value)};
                        for (auto& a : fc.attrs) {
                            // Check if attr is a variable name or a constant
                            // If it's a number, make it a const int
                            bool isInt = !a.empty();
                            for (char c : a) if (!std::isdigit((unsigned char)c) && c != '-') { isInt = false; break; }
                            if (isInt) {
                                try { 
                                    ops.push_back(mkConstInt(std::stoi(a))); 
                                } catch (...) {
                                    ops.push_back(mkConst(a));
                                }
                            } else if (a.size() >= 2 && a.front() == '$' && a.back() == '$') {
                                // String literal
                                ops.push_back(mkConst(a));
                            } else {
                                // Variable name
                                ops.push_back(mkVar(a));
                            }
                        }
                        IRInstruction i(IROpcode::CALL, dst, ops);
                        emit(std::move(i));
                        break;
                    }
                }
            }
            
            // Check if we have a structured expression as child
            if (!n.children.empty()) {
                IRRef src = lowerExprNode(*n.children[0]);
                IRType t  = typeOfRef(src);
                setRefType(dst, t);          // propagate type to destination var
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                i.resultType = t;
                emit(std::move(i));
                break;
            }
            
            // Legacy string-based handling (fallback)
            if (n.attrs.empty()) break;
            const std::string& raw = n.attrs[0];

            if (raw.substr(0, 8) == "__list__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(raw.substr(8))});
                emit(std::move(i));
            } else if (raw.substr(0, 9) == "__tuple__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("tuple"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw.substr(0, 8) == "__dict__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("dict"), mkConst(raw.substr(8))});
                emit(std::move(i));
            } else if (raw == "__range__") {
                // Range with structured expression
                if (n.children.size() > 0) {
                    IRRef rangeExpr = lowerExprNode(*n.children[0]);
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), rangeExpr});
                    emit(std::move(i));
                }
            } else if (raw.substr(0, 9) == "__range__") {
                // Legacy range with string
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw == "__sequence__") {
                // Sequence with structured expressions
                if (n.children.size() >= 2) {
                    IRRef xExpr = lowerExprNode(*n.children[0]);
                    IRRef yExpr = lowerExprNode(*n.children[1]);
                    // Create a temporary string representation for now
                    // TODO: Improve sequence handling in IR
                    IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), xExpr, yExpr});
                    emit(std::move(i));
                }
            } else if (raw.substr(0, 12) == "__sequence__") {
                // Legacy sequence with string
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConst(raw.substr(12))});
                emit(std::move(i));
            } else if (raw.substr(0, 6) == "__fn__") {
                IRRef src = lowerExpr(raw.substr(6));
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            } else {
                IRRef src = lowerExpr(raw);
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            }
            break;
        }

        case NodeType::PlusEqualStmt:
            if (!n.children.empty()) {
                // Structured expression as child
                IRRef lRef = mkVar(n.value);
                IRRef rRef = lowerExprNode(*n.children[0]);
                IRRef tmp = mkTemp();
                emit(IRInstruction(IROpcode::ADD, tmp, {lRef, rRef}));
                IRInstruction st(IROpcode::STORE_VAR);
                st.typedOperands = {mkVar(n.value), tmp};
                emit(std::move(st));
            } else if (!n.attrs.empty()) {
                // Legacy string-based (fallback)
                emitCompound(IROpcode::ADD, n.value, n.attrs[0]);
            }
            break;
        case NodeType::MinusEqualStmt:
            if (!n.children.empty()) {
                // Structured expression as child
                IRRef lRef = mkVar(n.value);
                IRRef rRef = lowerExprNode(*n.children[0]);
                IRRef tmp = mkTemp();
                emit(IRInstruction(IROpcode::SUB, tmp, {lRef, rRef}));
                IRInstruction st(IROpcode::STORE_VAR);
                st.typedOperands = {mkVar(n.value), tmp};
                emit(std::move(st));
            } else if (!n.attrs.empty()) {
                // Legacy string-based (fallback)
                emitCompound(IROpcode::SUB, n.value, n.attrs[0]);
            }
            break;
        case NodeType::MultiplyEqualStmt:
        case NodeType::AtEqualStmt:
            if (!n.children.empty()) {
                // Structured expression as child
                IRRef lRef = mkVar(n.value);
                IRRef rRef = lowerExprNode(*n.children[0]);
                IRRef tmp = mkTemp();
                emit(IRInstruction(IROpcode::MUL, tmp, {lRef, rRef}));
                IRInstruction st(IROpcode::STORE_VAR);
                st.typedOperands = {mkVar(n.value), tmp};
                emit(std::move(st));
            } else if (!n.attrs.empty()) {
                // Legacy string-based (fallback)
                emitCompound(IROpcode::MUL, n.value, n.attrs[0]);
            }
            break;
        case NodeType::DivideEqualStmt:
            if (!n.children.empty()) {
                // Structured expression as child
                IRRef lRef = mkVar(n.value);
                IRRef rRef = lowerExprNode(*n.children[0]);
                IRRef tmp = mkTemp();
                emit(IRInstruction(IROpcode::DIV, tmp, {lRef, rRef}));
                IRInstruction st(IROpcode::STORE_VAR);
                st.typedOperands = {mkVar(n.value), tmp};
                emit(std::move(st));
            } else if (!n.attrs.empty()) {
                // Legacy string-based (fallback)
                emitCompound(IROpcode::DIV, n.value, n.attrs[0]);
            }
            break;
        case NodeType::XorEqualStmt:
            if (!n.children.empty()) {
                // Structured expression as child
                IRRef lRef = mkVar(n.value);
                IRRef rRef = lowerExprNode(*n.children[0]);
                IRRef tmp = mkTemp();
                emit(IRInstruction(IROpcode::XOR, tmp, {lRef, rRef}));
                IRInstruction st(IROpcode::STORE_VAR);
                st.typedOperands = {mkVar(n.value), tmp};
                emit(std::move(st));
            } else if (!n.attrs.empty()) {
                // Legacy string-based (fallback)
                emitCompound(IROpcode::XOR, n.value, n.attrs[0]);
            }
            break;

        // ── display / print ─────────────────────────────────────────────────
        case NodeType::DisplayStmt: {
            IRRef val;
            if (!n.children.empty()) {
                // Structured expression as child
                val = lowerExprNode(*n.children[0]);
            } else if (!n.value.empty()) {
                // Legacy string-based expression (fallback)
                val = lowerExpr(n.value);
            } else {
                val = mkConst("");
            }
            
            IRInstruction i(IROpcode::PRINT);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        // ── method / function calls ─────────────────────────────────────────
        case NodeType::MethodCall: {
            // Self-referential tag call: terrain.generation() inside spawn_terrain → skip
            if (!currentCustomTag_.empty()) {
                std::string selfCall = currentCustomTag_ + ".generation";
                if (n.value == selfCall || n.value == currentCustomTag_ + ".generate") break;
            }
            // Check for display-style methods where arg is a structured child expression
            const std::string& mname = n.value;
            // Term.log / Term.write / Term.print do not exist in AC; keep only Term.display (+ styled variants).
            if (mname == "Term.log" || mname == "Term.write" || mname == "Term.print") {
                throw ACError::backend("Preposterous: " + mname + " does not exist (use Term.display)");
            }
            auto endsWith = [](const std::string& s, const std::string& suf) {
                return s.size() >= suf.size() && s.substr(s.size()-suf.size()) == suf;
            };
            if (!n.children.empty() && (endsWith(mname, "display") || mname == "alert" || mname == "sure")) {
                IRRef val = lowerExprNode(*n.children[0]);
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar(n.value), val};
                emit(std::move(i));
                break;
            }
            // print_page — no argument
            if (mname == "print_page") {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar("print_page")};
                emit(std::move(i));
                break;
            }
            if (mname == "Screen.OBJECT") break;

            // background.config("color=X") → gl:screen.set_bg_by_name("X")
            if (mname == "background.config" && !n.attrs.empty()) {
                std::string spec = n.attrs[0];
                std::string color;
                if (spec.rfind("color=", 0) == 0) color = spec.substr(6);
                else color = spec;
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:screen.set_bg_by_name"), mkConst(color)};
                emit(std::move(i));
                break;
            }

            // animate("60fps") → gl:screen.animate("60fps")
            if (mname == "animate" && !n.attrs.empty()) {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:screen.animate"), mkConst(n.attrs[0])};
                emit(std::move(i));
                break;
            }

            // Terrain.ANIMATE inside a custom tag → gl:obj.animate(obj, "left", 300)
            if (mname == "Terrain.ANIMATE" && !currentTagObjects_.empty()) {
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("gl:obj.animate"),
                                   mkConst(currentTagObjects_[0]),
                                   mkConst("left"),
                                   IRRef::constant(IRValue(300.0))};
                emit(std::move(i));
                break;
            }

            // GL object method calls: receiver.METHOD(args) → gl:obj.METHOD
            {
                static const std::unordered_map<std::string,std::string> glMethods = {
                    {"move_y","gl:obj.move_y"},   {"move_x","gl:obj.move_x"},
                    {"vertex","gl:obj.vertex"},   {"curveshape","gl:obj.curveshape"},
                    {"CircleFall","gl:obj.circle_fall"}, {"CircleFell","gl:obj.circle_fell"},
                    {"regen","gl:obj.regen"},     {"set_spawn","gl:obj.set_spawn"},
                    {"animate","gl:obj.animate"}, {"velocity","gl:obj.velocity"},
                    {"set_speed","gl:obj.set_speed"}, {"set_direction","gl:obj.set_direction"},
                };
                // Helper: parse compound "1/4RightDir" attr into fraction + direction operands
                auto addCircleArgs = [&](IRInstruction& i, const std::string& a) -> bool {
                    for (auto& [dirKey, dirVal] : std::vector<std::pair<std::string,std::string>>{
                            {"RightDir","right"},{"LeftDir","left"},{"UpDir","up"},{"DownDir","down"}}) {
                        auto dpos = a.find(dirKey);
                        if (dpos != std::string::npos) {
                            std::string fracStr = a.substr(0, dpos);
                            while (!fracStr.empty() && (fracStr.back()==' '||fracStr.back()=='\t')) fracStr.pop_back();
                            double fracVal = 0.25;
                            auto sl = fracStr.find('/');
                            if (sl != std::string::npos) {
                                try { fracVal = std::stod(fracStr.substr(0,sl)) / std::stod(fracStr.substr(sl+1)); } catch(...) {}
                            } else if (!fracStr.empty()) {
                                try { fracVal = std::stod(fracStr); } catch(...) {}
                            }
                            i.typedOperands.push_back(IRRef::constant(IRValue(fracVal)));
                            i.typedOperands.push_back(mkConst(dirVal));
                            return true;
                        }
                    }
                    return false;
                };
                auto dotPos = mname.find('.');
                if (dotPos != std::string::npos) {
                    std::string receiver = mname.substr(0, dotPos);
                    std::string method   = mname.substr(dotPos + 1);
                    bool isGlObj = glObjects_.count(receiver) > 0;
                    auto it = glMethods.find(method);
                    // Route if: known GL method, regardless of whether receiver is statically in glObjects_
                    // (receiver might be a function parameter holding an object name)
                    if (it != glMethods.end()) {
                        IRInstruction i(IROpcode::LIB_CALL);
                        i.typedOperands.push_back(mkConst(it->second));
                        // Known GL object → string literal; variable/param → variable ref
                        if (isGlObj)
                            i.typedOperands.push_back(mkConst(receiver));
                        else
                            i.typedOperands.push_back(mkVar(receiver));
                        // Add attrs as arguments
                        for (auto& a : n.attrs) {
                            if (a == "RightDir") i.typedOperands.push_back(mkConst("right"));
                            else if (a == "LeftDir")  i.typedOperands.push_back(mkConst("left"));
                            else if (a == "UpDir")    i.typedOperands.push_back(mkConst("up"));
                            else if (a == "DownDir")  i.typedOperands.push_back(mkConst("down"));
                            else if (!addCircleArgs(i, a)) {
                                // Normalize: trim whitespace, collapse "- N" → "-N"
                                std::string na = a;
                                while (!na.empty() && na.front() == ' ') na.erase(na.begin());
                                while (!na.empty() && na.back()  == ' ') na.pop_back();
                                if (na.size() >= 3 && na[0] == '-' && na[1] == ' ') na = "-" + na.substr(2);
                                // numeric, string literal, or bare constant
                                bool isNum = !na.empty();
                                for (char c : na) if (!std::isdigit((unsigned char)c) && c!='-' && c!='.') { isNum=false; break; }
                                if (isNum) { try { i.typedOperands.push_back(IRRef::constant(IRValue(std::stod(na)))); } catch(...) { i.typedOperands.push_back(mkConst(na)); } }
                                else if (na.size()>=2 && na.front()=='$' && na.back()=='$') i.typedOperands.push_back(mkConst(na.substr(1,na.size()-2)));
                                else i.typedOperands.push_back(mkConst(na));
                            }
                        }
                        if (it->second == "ac_gl_obj_circle_fell") i.result = mkTemp();
                        emit(std::move(i));
                        break;
                    }
                    // Receiver is a GL object but method not in map — forward with gl:obj. prefix
                    if (isGlObj && method.rfind("gl:", 0) != 0) {
                        IRInstruction i(IROpcode::LIB_CALL);
                        i.typedOperands.push_back(mkConst("gl:obj." + method));
                        i.typedOperands.push_back(mkConst(receiver));
                        for (auto& a : n.attrs) i.typedOperands.push_back(mkConst(a));
                        emit(std::move(i));
                        break;
                    }
                }
            }

            // ── string method normalization ─────────────────────────────────────
            // Map AC string method names to cross-backend equivalents.
            // \ws in args → backend-native whitespace pattern string.
            {
                size_t dot = n.value.rfind('.');
                if (dot != std::string::npos) {
                    static const std::unordered_map<std::string,std::string> strMethods = {
                        {"UPPER","upper"},{"LOWER","lower"},{"lower","lower"},{"upper","upper"},
                        {"STRIP","strip"},{"FIND","find"},{"REPLACE","replace"},
                        {"SPLIT","split"},{"JOIN","join"},{"LEN","__len__"},
                        {"STARTSWITH","startswith"},{"ENDSWITH","endswith"},
                        {"COUNT","count"},{"FORMAT","format"},
                    };
                    std::string meth = n.value.substr(dot + 1);
                    auto it = strMethods.find(meth);
                    if (it != strMethods.end())
                        const_cast<std::string&>(n.value) = n.value.substr(0, dot + 1) + it->second;
                }
                // Replace \ws token in attrs with whitespace pattern sentinel
                for (auto& a : const_cast<std::vector<std::string>&>(n.attrs)) {
                    if (a == "\\ws" || a == "\\\\ws") a = "__WS__";
                }
            }

            std::vector<IRRef> ops = {mkVar(n.value)};
            for (auto& a : n.attrs) {
                // Trim whitespace
                std::string trimmed = a;
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
                
                // Check if attr is a variable name or a constant
                // If it's a number, make it a const int
                bool isInt = !trimmed.empty();
                for (char c : trimmed) if (!std::isdigit((unsigned char)c) && c != '-') { isInt = false; break; }
                if (isInt) {
                    try { 
                        ops.push_back(mkConstInt(std::stoi(trimmed))); 
                    } catch (...) {
                        ops.push_back(mkConst(trimmed));
                    }
                } else if (trimmed.size() >= 2 && trimmed.front() == '$' && trimmed.back() == '$') {
                    // String literal
                    ops.push_back(mkConst(trimmed));
                } else if (!trimmed.empty() && std::isdigit((unsigned char)trimmed.front())) {
                    // Mixed digit-alpha token like "60fps" — treat as string constant
                    ops.push_back(mkConst(trimmed));
                } else if (!trimmed.empty()) {
                    // Variable name
                    ops.push_back(mkVar(trimmed));
                } else {
                    // Empty - skip
                }
            }
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::FunctionCall: {
            // Remap GL helpers to high-level gl:* names (lowering pass handles the rest)
            static const std::unordered_map<std::string,std::string> glFuncMap = {
                {"is_obj",                  "gl:obj.is"},
                {"is_draw",                 "gl:obj.is_draw"},
                {"gl_hitbox_many_overlap",  "gl:hitbox.many_overlap"},
                {"gl_hitbox_coords_overlap","gl:hitbox.overlap"},
                {"gl_frame_begin",          "gl:frame.begin"},
                {"gl_frame_render",         "gl:frame.render"},
                {"gl_frame_end",            "gl:frame.end"},
                {"gl_frame_update",         "gl:frame.update"},
                {"gl_obj_animate",          "gl:obj.animate"},
                {"gl_obj_regen",            "gl:obj.regen"},
                {"gl_screen_init_from_ac",  "gl:screen.init"},
            };
            std::string fname = n.value;
            {
                auto fit = glFuncMap.find(fname);
                if (fit != glFuncMap.end()) fname = fit->second;
            }
            std::vector<IRRef> ops = {mkVar(fname)};
            for (auto& a : n.attrs) {
                // Check if attr is a variable name or a constant
                // If it's a number, make it a const int
                bool isInt = !a.empty();
                for (char c : a) if (!std::isdigit((unsigned char)c) && c != '-') { isInt = false; break; }
                if (isInt) {
                    try { 
                        ops.push_back(mkConstInt(std::stoi(a))); 
                    } catch (...) {
                        ops.push_back(mkConst(a));
                    }
                } else if (a.size() >= 2 && a.front() == '$' && a.back() == '$') {
                    // String literal
                    ops.push_back(mkConst(a));
                } else {
                    // Variable name
                    ops.push_back(mkVar(a));
                }
            }
            IRInstruction i(IROpcode::CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::PropAssign: {
            // name.prop = value  OR  name.prop /= value (compound, attrs[0] = "/=")
            IRRef dst = mkVar(n.value);
            // Compound assignment: attr[0] is the op ("/=", "*=", etc.), rhs in children[0]
            if (!n.attrs.empty() && !n.children.empty() &&
                (n.attrs[0] == "/=" || n.attrs[0] == "*=" || n.attrs[0] == "+=" || n.attrs[0] == "-=")) {
                // Skip compound assigns on function-attribute style (e.g. jump.vertex /= 2)
                // where the receiver is not a GL object — such "closure attributes" have no
                // cross-backend representation and would produce undeclared-variable errors.
                auto dotPos = n.value.find('.');
                if (dotPos != std::string::npos) {
                    std::string receiver = n.value.substr(0, dotPos);
                    if (glObjects_.count(receiver) == 0) break; // skip
                }
                IRRef rhs = lowerExprNode(*n.children[0]);
                // emit as LIB_CALL("__compound_assign__", dst, op, rhs) so codegen can render X op= Y
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkConst("__compound_assign__"), dst, mkConst(n.attrs[0]), rhs};
                emit(std::move(i));
            } else {
                IRRef src = n.attrs.empty() ? mkConst("") : lowerExpr(n.attrs[0]);
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            }
            break;
        }

        case NodeType::ConfigCall: {
            // Emit a single gl:obj.config call; the GL lowering pass expands it into
            // ac_gl_obj_config_item / ac_gl_obj_pos_from_spec / ac_gl_obj_color_by_name calls.
            IRInstruction cfg(IROpcode::LIB_CALL);
            cfg.typedOperands.push_back(mkConst("gl:obj.config"));
            cfg.typedOperands.push_back(mkConst(n.value));
            for (auto& a : n.attrs)
                cfg.typedOperands.push_back(mkConst(a));
            emit(std::move(cfg));
            break;
        }

        case NodeType::ObjDecl: {
            std::string full = n.value;
            std::string objName = (full.size() > 4 && full.substr(0,4) == "Obj.") ? full.substr(4) : full;

            if (objName == "Screen") {
                // Parse WxH from "resize:WxH" attr
                int w = 1720, h = 1080;
                for (auto& a : n.attrs) {
                    if (a.rfind("resize:", 0) == 0) {
                        std::string dims = a.substr(7);
                        auto xp = dims.find('x');
                        if (xp != std::string::npos) {
                            try { w = std::stoi(dims.substr(0, xp)); h = std::stoi(dims.substr(xp+1)); } catch(...) {}
                        }
                    }
                }
                // gl:screen.init — lowering pass maps to ac_gl_screen_init
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands.push_back(mkConst("gl:screen.init"));
                i.typedOperands.push_back(mkConstInt(w));
                i.typedOperands.push_back(mkConstInt(h));
                i.typedOperands.push_back(mkConst("Geodeo"));
                emit(std::move(i));
                break;
            }

            glObjects_.insert(objName);
            if (!currentCustomTag_.empty())
                currentTagObjects_.push_back(objName);

            // gl:obj.create — lowering pass maps to ac_gl_obj_create
            {
                IRInstruction ic(IROpcode::LIB_CALL);
                ic.typedOperands = {mkConst("gl:obj.create"), mkConst(objName)};
                emit(std::move(ic));
            }
            // Declare variable as string "Name" so it can be passed to functions
            {
                IRRef dst = mkVar(objName);
                IRInstruction id(IROpcode::ALLOC, dst, {mkConst("string"), mkConst(objName)});
                emit(std::move(id));
            }
            break;
        }

        case NodeType::MethodChain: {
            for (auto& c : n.children) gen(*c);
            break;
        }

        // ── control flow ────────────────────────────────────────────────────
        case NodeType::IfStmt: {
            if (n.value == "OTHER") {
                // OTHER (else) block - just generate the body
                // The IF_ELSE opcode was already emitted by the parent IF
                if (!n.children.empty()) gen(*n.children[0]);
                break;
            }

            // First child is condition expression, second is body
            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            IRRef condRef = lowerExprNode(*n.children[0]);
            size_t bodyIndex = 1;

            if (prog.useHighLevelIR) {
                // High-level IR: emit IF_BEGIN with condition
                IRInstruction ifBegin(IROpcode::IF_BEGIN);
                ifBegin.typedOperands = {condRef};
                emit(std::move(ifBegin));
                
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                
                if (n.children.size() > bodyIndex + 1) {
                    emit(IRInstruction(IROpcode::IF_ELSE));
                    genElseChain(n.children, bodyIndex + 1);
                }
                
                // Emit IF_END
                IRInstruction ifEnd(IROpcode::IF_END);
                emit(std::move(ifEnd));
            } else {
                // Low-level IR: use jumps and labels
                IRRef elseL   = mkLabel();
                IRRef endL    = mkLabel();

                emitJF(condRef, elseL);
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                bool hasElse = n.children.size() > bodyIndex + 1;
                if (hasElse) emitJump(endL);
                emitLabel(elseL);

                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);
                if (hasElse) emitLabel(endL);
            }
            break;
        }

        case NodeType::CondStmt: {
            // cond expr
            //   is v1: block
            //   is v2: block
            //   OTHER: block
            // Lowers to a nested IF/ELSE chain with expr evaluated once.
            if (n.children.empty()) break;

            // Evaluate the scrutinee once into a temp var
            static int condCounter = 0;
            std::string tmpName = "_ac_cond_" + std::to_string(condCounter++);
            IRRef tmpVar = mkVar(tmpName);
            IRRef scrut = lowerExprNode(*n.children[0]);
            emit(IRInstruction(IROpcode::STORE_VAR, tmpVar, {scrut}));

            struct Case { const ASTNode* expr; const ASTNode* block; };
            std::vector<Case> cases;
            const ASTNode* otherBlock = nullptr;

            for (size_t i = 1; i < n.children.size(); i++) {
                const auto& c = *n.children[i];
                if (c.type == NodeType::CondCase) {
                    const ASTNode* ex = (c.children.size() > 0) ? c.children[0].get() : nullptr;
                    const ASTNode* bl = (c.children.size() > 1) ? c.children[1].get() : nullptr;
                    if (ex && bl) cases.push_back({ex, bl});
                } else if (c.type == NodeType::CondOther) {
                    otherBlock = (!c.children.empty()) ? c.children[0].get() : nullptr;
                }
            }
            if (cases.empty()) break;

            auto genNested = [&](auto&& self, size_t idx) -> void {
                // Build cond = (tmpVar == caseExpr)
                IRRef rhs = lowerExprNode(*cases[idx].expr);
                IRRef condRef = mkTemp();
                IRInstruction cmp(IROpcode::EQ, condRef, {tmpVar, rhs});
                emit(std::move(cmp));

                IRInstruction ifBegin(IROpcode::IF_BEGIN);
                ifBegin.typedOperands = {condRef};
                emit(std::move(ifBegin));
                gen(*cases[idx].block);

                bool hasMore = (idx + 1 < cases.size()) || (otherBlock != nullptr);
                if (hasMore) {
                    emit(IRInstruction(IROpcode::IF_ELSE));
                    if (idx + 1 < cases.size()) self(self, idx + 1);
                    else if (otherBlock) gen(*otherBlock);
                }
                emit(IRInstruction(IROpcode::IF_END));
            };

            if (prog.useHighLevelIR) {
                genNested(genNested, 0);
            } else {
                // Low-level backends: fall back to high-level IF markers; codegen supports them.
                bool prev = prog.useHighLevelIR;
                prog.useHighLevelIR = true;
                genNested(genNested, 0);
                prog.useHighLevelIR = prev;
            }
            break;
        }

        case NodeType::ElseIfStmt: {
            // First child is condition expression, second is body
            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            IRRef condRef = lowerExprNode(*n.children[0]);
            size_t bodyIndex = 1;

            IRRef skipL   = mkLabel();

            emitJF(condRef, skipL);
            if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
            emitLabel(skipL);
            break;
        }

        case NodeType::WhilstLoop: {
            if (prog.useHighLevelIR) {
                // High-level backends: emit "while True:" with break-at-top-on-condition.
                // Push sentinels so BreakStmt/ContinueStmt emit the correct keywords.
                IRRef breakSentinel = mkConst("__break__");
                IRRef contSentinel  = mkConst("__continue__");
                loopEnd.push(breakSentinel);
                loopStart.push(contSentinel);

                emit(IRInstruction(IROpcode::WHILE_BEGIN));

                // Condition is computed INSIDE the loop body, re-evaluated each iteration
                if (n.children.empty() || n.children[0]->type == NodeType::Block) {
                    emit(IRInstruction(IROpcode::WHILE_END));
                    loopEnd.pop(); loopStart.pop();
                    break;
                }
                IRRef condRef = lowerExprNode(*n.children[0]);
                size_t bodyIndex = 1;

                // if not cond: break
                {
                    IRInstruction jfb(IROpcode::JUMP_IF_FALSE);
                    jfb.typedOperands = {condRef, breakSentinel};
                    emit(std::move(jfb));
                }

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);

                emit(IRInstruction(IROpcode::WHILE_END));

                loopEnd.pop();
                loopStart.pop();
            } else {
                // Low-level backends: labels + jumps, condition re-evaluated each iteration
                IRRef startL = mkLabel();
                IRRef endL   = mkLabel();

                loopStart.push(startL);
                loopEnd.push(endL);

                emitLabel(startL);  // label FIRST so condition is re-evaluated each loop

                if (n.children.empty() || n.children[0]->type == NodeType::Block) {
                    loopStart.pop(); loopEnd.pop();
                    break;
                }
                IRRef condRef = lowerExprNode(*n.children[0]);
                size_t bodyIndex = 1;
                emitJF(condRef, endL);

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);

                emitJump(startL);
                emitLabel(endL);

                loopStart.pop();
                loopEnd.pop();
            }
            break;
        }

        case NodeType::ForLoop: {
            std::string iterVar = n.value;

            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            size_t bodyIndex = 1;
            ASTNode& collNode = *n.children[0];

            // Low-level (BNY/ASM): range/sequence → compact counted WHILST loop
            if (!prog.useHighLevelIR &&
                (collNode.type == NodeType::RangeExpr || collNode.type == NodeType::SequenceExpr)) {

                IRRef startL = mkLabel();
                IRRef endL   = mkLabel();
                IRRef iter   = mkVar(iterVar);

                loopStart.push(startL);
                loopEnd.push(endL);

                IRRef aRef, bRef;
                if (collNode.type == NodeType::RangeExpr) {
                    aRef = mkConstInt(0);
                    // New parser: bound as child[0]; legacy: value string
                    bRef = !collNode.children.empty()
                        ? lowerExprNode(*collNode.children[0])
                        : lowerExpr(collNode.value);
                } else {
                    // SequenceExpr: new parser stores as children, legacy as "a,b" in value
                    if (collNode.children.size() >= 2) {
                        aRef = lowerExprNode(*collNode.children[0]);
                        bRef = lowerExprNode(*collNode.children[1]);
                    } else {
                        std::string val = collNode.value;
                        size_t comma = val.find(',');
                        aRef = lowerExpr(comma != std::string::npos ? val.substr(0, comma) : "0");
                        bRef = lowerExpr(comma != std::string::npos ? val.substr(comma + 1) : val);
                    }
                }

                emit(IRInstruction(IROpcode::STORE_VAR, iter, {aRef}));
                emitLabel(startL);

                IRRef cmpT = mkTemp();
                emit(IRInstruction(IROpcode::LT, cmpT, {iter, bRef}));
                emitJF(cmpT, endL);

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                IRRef incT = mkTemp();
                emit(IRInstruction(IROpcode::ADD, incT, {iter, mkConstInt(1)}));
                emit(IRInstruction(IROpcode::STORE_VAR, iter, {incT}));

                emitJump(startL);
                emitLabel(endL);
                loopStart.pop();
                loopEnd.pop();
                break;
            }

            IRRef collRef = lowerExprNode(collNode);

            if (prog.useHighLevelIR) {
                // High-level backends: FOR_BEGIN {iterVar, collection} … FOR_END
                IRRef breakSentinel = mkConst("__break__");
                IRRef contSentinel  = mkConst("__continue__");
                loopEnd.push(breakSentinel);
                loopStart.push(contSentinel);

                IRInstruction fb(IROpcode::FOR_BEGIN);
                fb.typedOperands = {mkVar(iterVar), collRef};
                emit(std::move(fb));

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                emit(IRInstruction(IROpcode::FOR_END));

                loopEnd.pop();
                loopStart.pop();
            } else {
                // Low-level (list iterator): labels + jumps + LOAD_INDEX __next__
                IRRef startL = mkLabel();
                IRRef endL   = mkLabel();
                IRRef iterT  = mkTemp();
                IRRef itemT  = mkTemp();

                loopStart.push(startL);
                loopEnd.push(endL);

                IRInstruction init(IROpcode::LOAD_VAR, iterT, {collRef});
                emit(std::move(init));

                emitLabel(startL);
                emitJF(iterT, endL);

                IRInstruction next(IROpcode::LOAD_INDEX, itemT, {iterT, mkConst("__next__")});
                emit(std::move(next));

                IRInstruction stv(IROpcode::STORE_VAR, mkVar(iterVar), {itemT});
                emit(std::move(stv));

                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

                emitJump(startL);
                emitLabel(endL);

                loopStart.pop();
                loopEnd.pop();
            }
            break;
        }

        case NodeType::ReturnStmt: {
            IRRef val;
            if (!n.children.empty()) {
                // Structured expression as child
                val = lowerExprNode(*n.children[0]);
            } else if (!n.value.empty()) {
                // Legacy string-based expression (fallback)
                val = lowerExpr(n.value);
            } else {
                // No return value (void return)
                val = mkConst("");
            }
            
            IRInstruction i(IROpcode::RETURN);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        case NodeType::BreakStmt:
        case NodeType::SkipStmt:
            if (!loopEnd.empty()) emitJump(loopEnd.top());
            break;

        case NodeType::ContinueStmt:
            if (!loopStart.empty()) emitJump(loopStart.top());
            break;

        case NodeType::KillStmt: {
            emit(IRInstruction(IROpcode::HALT));
            break;
        }

        case NodeType::StopStmt: {
            emit(IRInstruction(IROpcode::SOFT_HALT));
            break;
        }

        case NodeType::RestartStmt: {
            prog.hasRestart = true;
            emit(IRInstruction(IROpcode::RESTART_PROGRAM));
            break;
        }

        case NodeType::HaltStmt: {
            // /halt n — sleep for n seconds; /halt math.inf is already StopStmt in parser
            IRInstruction sleep(IROpcode::SLEEP);
            sleep.typedOperands.push_back(mkConst(n.value));
            emit(std::move(sleep));
            break;
        }

        case NodeType::EndStmt: {
            // Break out of the enclosing loop regardless of its condition.
            // Falls back to HALT if used outside any loop.
            if (!loopEnd.empty())
                emitJump(loopEnd.top());
            else
                emit(IRInstruction(IROpcode::HALT));
            break;
        }

        case NodeType::PassStmt: {
            IRInstruction i(IROpcode::NOP);
            emit(std::move(i));
            break;
        }

        case NodeType::AliasDecl: {
            // alias x = y — register bidirectional live binding
            IRInstruction i(IROpcode::ALIAS_DECL);
            i.typedOperands = {mkVar(n.value), mkVar(n.attrs.empty() ? n.value : n.attrs[0])};
            emit(std::move(i));
            // Initialise x = y so they start equal
            IRInstruction init(IROpcode::STORE_VAR, mkVar(n.value), {mkVar(n.attrs.empty() ? n.value : n.attrs[0])});
            emit(std::move(init));
            break;
        }

        case NodeType::FreeDecl: {
            // free x, y, z — emit one FREE_DECL per named variable
            for (const auto& varName : n.attrs) {
                IRInstruction i(IROpcode::FREE_DECL);
                i.typedOperands = {mkVar(varName)};
                emit(std::move(i));
            }
            break;
        }

        case NodeType::ConstDecl: {
            // const x = expr — immutable binding; track name so reassignment is an error
            IRRef varRef = mkVar(n.value);
            IRRef src = n.children.empty() ? mkConstInt(0) : lowerExprNode(*n.children[0]);
            prog.constVars.insert(n.value);
            IRInstruction i(IROpcode::CONST_DECL, varRef, {src});
            emit(std::move(i));
            break;
        }

        case NodeType::CopyStmt: {
            // cp x = y — deep copy of rhs into lhs
            IRRef dst = mkVar(n.value);
            IRRef src = n.children.empty() ? mkConstInt(0) : lowerExprNode(*n.children[0]);
            IRInstruction i(IROpcode::STORE_VAR, dst, {src});
            i.attrs.push_back("copy");   // marks this as an explicit deep copy
            emit(std::move(i));
            break;
        }

        case NodeType::TypeCoerceStmt: {
            // dec/int/string/bool x [= expr] — coerce x to target type
            const std::string& varName = n.value;
            // Determine target IRType from attrs
            IRType targetType = IRType::VOID;
            if (!n.attrs.empty()) {
                if      (n.attrs[0] == "DEC")    targetType = IRType::FLOAT;
                else if (n.attrs[0] == "INT")    targetType = IRType::INT;
                else if (n.attrs[0] == "STRING") targetType = IRType::STRING;
                else if (n.attrs[0] == "BOOL")   targetType = IRType::BOOL;
                else if (n.attrs[0] == "LONGINT") targetType = IRType::STRING;
                else if (n.attrs[0] == "GOODDEC") targetType = IRType::STRING;
            }

            if (!n.attrs.empty() && n.attrs[0] == "LONGINT") {
                std::string decimal;
                if (!n.children.empty() && longIntFromExpr(*n.children[0], decimal)) {
                    IRRef varRef = mkVar(varName);
                    // Fit check: if within int64, store as INT so all backends work natively
                    bool fits64 = false;
                    int64_t intVal = 0;
                    if (!decimal.empty() && decimal.size() <= 20) {
                        try {
                            size_t pos = 0;
                            long long v = std::stoll(decimal, &pos);
                            if (pos == decimal.size()) {
                                intVal = (int64_t)v;
                                fits64 = true;
                            }
                        } catch (...) {}
                    }
                    if (fits64) {
                        setRefType(varRef, IRType::INT);
                        IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConstInt(intVal)});
                        i.resultType = IRType::INT;
                        emit(std::move(i));
                    } else {
                        setRefType(varRef, IRType::STRING);
                        IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConst(decimal)});
                        i.resultType = IRType::STRING;
                        emit(std::move(i));
                    }
                } else {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::STRING);
                    IRInstruction i(IROpcode::STORE_VAR, varRef, {mkConst(std::string("inf"))});
                    i.resultType = IRType::STRING;
                    emit(std::move(i));
                }
                break;
            }

            if (!n.attrs.empty() && n.attrs[0] == "GOODDEC") {
                std::string unscaled;
                int64_t scale = 0;
                if (!n.children.empty() &&
                    goodDecPairFromExpr(*n.children[0], unscaled, scale)) {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::TUPLE);
                    std::string content = unscaled + "," + std::to_string(scale);
                    IRInstruction i(IROpcode::ALLOC, varRef, {mkConst("tuple"), mkConst(content)});
                    i.resultType = IRType::TUPLE;
                    emit(std::move(i));
                } else {
                    IRRef varRef = mkVar(varName);
                    setRefType(varRef, IRType::TUPLE);
                    IRInstruction i(IROpcode::ALLOC, varRef, {mkConst("tuple"), mkConst(std::string("0,0"))});
                    i.resultType = IRType::TUPLE;
                    emit(std::move(i));
                }
                break;
            }

            // Source: either provided expr, or the variable itself (coerce in-place)
            IRRef src = n.children.empty() ? mkVar(varName) : lowerExprNode(*n.children[0]);
            IRRef varRef = mkVar(varName);
            setRefType(varRef, targetType);
            IRInstruction i(IROpcode::TYPE_CAST);
            i.typedOperands = {src};
            i.result        = varRef;
            i.resultType    = targetType;
            emit(std::move(i));
            break;
        }

        case NodeType::DestroyStmt: {
            IRInstruction i(IROpcode::FREE);
            i.typedOperands = {mkVar(n.value)};
            emit(std::move(i));
            break;
        }

        // ── index expressions ───────────────────────────────────────────────
        case NodeType::IndexExpr: {
            if (n.attrs.size() >= 2) {
                // store: arr[idx] = val  (AC 1-based → 0-based)
                IRRef arr = mkVar(n.value);
                IRRef rawIdx = lowerExpr(n.attrs[0]);
                IRRef idx = adjustIndex(rawIdx);
                IRRef val = lowerExpr(n.attrs[1]);
                IRInstruction i(IROpcode::STORE_INDEX);
                i.typedOperands = {arr, idx, val};
                emit(std::move(i));
            } else if (n.attrs.size() == 1) {
                // load: tmp = arr[idx]  (AC 1-based → 0-based)
                IRRef arr = mkVar(n.value);
                IRRef rawIdx = lowerExpr(n.attrs[0]);
                IRRef idx = adjustIndex(rawIdx);
                IRRef dst = mkTemp();
                IRInstruction i(IROpcode::LOAD_INDEX, dst, {arr, idx});
                emit(std::move(i));
            }
            break;
        }

        // ── literals ────────────────────────────────────────────────────────
        case NodeType::ListLiteral: {
            IRRef dst = mkTemp();
            // If children are present (e.g. from `return x, y`), lower each and join names
            if (!n.children.empty()) {
                std::string content;
                for (size_t ci = 0; ci < n.children.size(); ci++) {
                    if (ci > 0) content += ", ";
                    IRRef er = lowerExprNode(*n.children[ci]);
                    if (er.kind == IRRef::Kind::VAR && er.id >= 0)
                        content += prog.symbols.getName(er.id);
                    else if (er.kind == IRRef::Kind::CONST) {
                        const auto& cv = er.value;
                        if (cv.type == IRType::INT)   content += std::to_string(std::get<int64_t>(cv.data));
                        else if (cv.type == IRType::FLOAT) content += std::to_string(std::get<double>(cv.data));
                        else if (cv.type == IRType::STRING) content += "\"" + std::get<std::string>(cv.data) + "\"";
                        else content += "0";
                    }
                    else
                        content += "_";
                }
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(content)});
                emit(std::move(i));
            } else {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(n.value)});
                emit(std::move(i));
            }
            break;
        }
        case NodeType::TupleLiteral: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("tuple"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::DictLiteral: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("dict"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::RangeExpr: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::SequenceExpr: {
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConst(n.value)});
            emit(std::move(i));
            break;
        }
        case NodeType::BinaryExpr: {
            lowerExprNode(n);  // evaluate via structured path; result discarded (expression-as-statement)
            break;
        }

        // ── events ──────────────────────────────────────────────────────────
        case NodeType::EventListener:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::KeyBinding: {
            // Generate a named callback function __keycb_<key> for the binding body
            std::string key = n.value;
            // Sanitize key name for use as identifier
            std::string safeName = key;
            for (char& c : safeName) if (!std::isalnum((unsigned char)c)) c = '_';
            std::string cbName = "__keycb_" + safeName;

            if (!n.children.empty()) {
                // Lower children as a function body
                int funcId = prog.symbols.intern(cbName, IRType::FUNCTION);
                IRFunction fn(cbName);
                fn.returnType = IRType::VOID;
                prog.functions.push_back(std::move(fn));
                cur = &prog.functions.back();
                IRInstruction entry(IROpcode::FUNC_BEGIN);
                entry.typedOperands = {IRRef::func(funcId)};
                emit(std::move(entry));
                for (auto& c : n.children) gen(*c);
                IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
                IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
                cur = nullptr;
            }

            // Emit EVENT_BIND: {key_string, callback_name}
            IRInstruction i(IROpcode::EVENT_BIND);
            i.typedOperands = {mkConst(key), mkVar(cbName)};
            emit(std::move(i));
            break;
        }

        case NodeType::InputStmt: {
            IRInstruction i(IROpcode::EVENT_TRIGGER);
            i.typedOperands = {mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        // ── library / misc ──────────────────────────────────────────────────
        case NodeType::UseStmt:
        case NodeType::UseLibStmt: {
            // track imported lib name (strip "ilib:", "elib:", etc.)
            {
                std::string libName = n.value;
                auto colon = libName.find(':');
                if (colon != std::string::npos) libName = libName.substr(colon + 1);
                prog.importedLibs.insert(libName);
            }
            IRInstruction i(IROpcode::LIB_CALL);
            // operands: "import", "ilib:math"[, "sin,cos,sqrt" if selective]
            i.typedOperands = {mkConst("import"), mkConst(n.value)};
            if (!n.attrs.empty()) {
                // Build comma-separated symbol list from attrs
                std::string symbols;
                for (size_t k = 0; k < n.attrs.size(); k++) {
                    if (k) symbols += ',';
                    symbols += n.attrs[k];
                }
                i.typedOperands.push_back(mkConst(symbols));
            }
            emit(std::move(i));
            break;
        }

        case NodeType::RaiseStmt: {
            // raise ERR or raise ERR($msg$) → Preposterous: + abort
            IRInstruction i(IROpcode::LIB_CALL);
            std::string msg = n.value.empty() ? "Fatality occurred" : n.value;
            i.typedOperands = {mkConst("raise"), mkConst(msg)};
            emit(std::move(i));
            break;
        }

        case NodeType::RaiseClauseStmt: {
            // raise Clause($msg$) — emit "Clause: msg" to stderr
            // Special cases: "hint" → Suggestion:, "toxic" → Toxic:
            // All others (including Hint, Err, Toxic, Praise...) → "Clause: msg"
            IRInstruction i(IROpcode::RAISE_CLAUSE);
            std::string clauseName = n.value;
            std::string msg = n.attrs.empty() ? "" : n.attrs[0];
            i.typedOperands = {mkConst(clauseName), mkConst(msg)};
            emit(std::move(i));
            break;
        }

        case NodeType::TryCatchStmt: {
            // children[0] = tryBody
            // children[1..] = CatchClause(typeName, attrs[0]=exVar, children[0]=Block) and optional AfterClause(children[0]=Block)
            emit(IRInstruction(IROpcode::TRY_BEGIN));

            if (!n.children.empty())
                gen(*n.children[0]); // try body

            bool sawCatch = false;
            for (size_t ci = 1; ci < n.children.size(); ci++) {
                const auto& child = *n.children[ci];
                if (child.type == NodeType::CatchClause) {
                    sawCatch = true;
                    std::string typeName = child.value; // may be empty for catch-all
                    std::string exVar = (!child.attrs.empty() && !child.attrs[0].empty()) ? child.attrs[0] : "_exc";
                    IRInstruction cb(IROpcode::CATCH_BEGIN);
                    cb.typedOperands = {mkConst(exVar), mkConst(typeName)};
                    emit(std::move(cb));
                    if (!child.children.empty())
                        gen(*child.children[0]); // catch body
                } else if (child.type == NodeType::AfterClause) {
                    emit(IRInstruction(IROpcode::AFTER_BEGIN));
                    if (!child.children.empty())
                        gen(*child.children[0]);
                } else {
                    // Unknown node inside TryCatchStmt: ignore
                }
            }

            if (!sawCatch) {
                throw std::runtime_error("Preposterous: try block missing catch clause");
            }

            emit(IRInstruction(IROpcode::TRY_END));
            break;
        }

        case NodeType::ForeignBlock: {
            if (prog.backend == "BNY") {
                throw std::runtime_error("Toxic: User attempts fluency in CPU");
            }
            if (!g_allow_foreign) {
                throw std::runtime_error(
                    "Foreign blocks are disabled. Recompile with --allow-foreign to enable raw code passthrough.");
            }
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = {mkConst("foreign"), mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        case NodeType::CustomTagDef: {
            // Lower as a function named spawn_<tag> so all backends get proper function wrapping
            std::string spawnName = "spawn_" + n.value;
            int funcId = prog.symbols.intern(spawnName, IRType::FUNCTION);
            IRFunction fn(spawnName);
            fn.returnType = IRType::VOID;
            prog.functions.push_back(std::move(fn));
            cur = &prog.functions.back();
            std::string prevTag = currentCustomTag_;
            std::vector<std::string> prevTagObjs = currentTagObjects_;
            currentTagObjects_.clear();
            currentCustomTag_ = n.value;
            IRInstruction entry(IROpcode::FUNC_BEGIN); entry.typedOperands = {IRRef::func(funcId)}; emit(std::move(entry));
            if (!n.children.empty()) gen(*n.children[0]);
            IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
            IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
            cur = nullptr;
            currentCustomTag_ = prevTag;
            currentTagObjects_ = prevTagObjs;
            break;
        }

        case NodeType::SpawnStmt: {
            std::string spawnName = "spawn_" + n.value;
            IRRef dst = mkTemp();
            IRInstruction i(IROpcode::CALL, dst, {mkVar(spawnName)});
            emit(std::move(i));
            break;
        }

        default:
            for (auto& c : n.children) gen(*c);
            break;
        }
    }

public:
    IRProgram generate(const ASTNode& ast, const std::string& backend) {
        prog         = IRProgram();
        prog.backend = backend;
        prog.target  = backend;
        // Use high-level IR for all backends that support structured while loops.
        // High-level IR: proper if/else blocks + while-true/break (no goto).
        // ASM stays low-level (uses real label/jump emitters for actual assembly).
        prog.useHighLevelIR = (backend == "PY" || backend == "JS" || backend == "HTML" ||
                               backend == "Java" || backend == "C++" || backend == "CPP" ||
                               backend == "RS" || backend == "V" || backend == "GO" ||
                               backend == "C");
        tc = lc = 0;
        inMainSection = false;
        gen(ast);

        // If no explicit <mainloop> tag was encountered, all non-import instructions
        // in dataSection are actually main body — reclassify them.
        if (prog.mainSection.empty() && !prog.dataSection.empty()) {
            for (auto& ins : prog.dataSection) {
                bool isImport = (ins.opcode == IROpcode::LIB_CALL &&
                                 !ins.typedOperands.empty() &&
                                 ins.typedOperands[0].kind == IRRef::Kind::CONST &&
                                 ins.typedOperands[0].value.type == IRType::STRING &&
                                 std::get<std::string>(ins.typedOperands[0].value.data) == "import");
                if (!isImport) prog.mainSection.push_back(ins);
            }
            // Keep only imports in dataSection
            auto& ds = prog.dataSection;
            ds.erase(std::remove_if(ds.begin(), ds.end(), [](const IRInstruction& ins) {
                if (ins.opcode != IROpcode::LIB_CALL) return true;
                if (ins.typedOperands.empty()) return true;
                if (ins.typedOperands[0].kind != IRRef::Kind::CONST) return true;
                if (ins.typedOperands[0].value.type != IRType::STRING) return true;
                return std::get<std::string>(ins.typedOperands[0].value.data) != "import";
            }), ds.end());
        }

        prog.globalTempCount  = tc;
        prog.globalLabelCount = lc;
        return std::move(prog);
    }
};

// ─── Optimization passes ────────────────────────────────────────────────────

// Copy propagation: when a TEMP is defined by a single STORE_VAR-like assignment
// and used exactly once, propagate its source to eliminate the temp.
// Specifically handles: STORE_VAR var {temp} where temp is used only here →
// rewrite the instruction that defines temp to write directly to var.
static void runCopyProp(std::vector<IRInstruction>& instrs) {
    // Count uses of each TEMP id
    std::unordered_map<int, int> useCount;
    // Track definition index for each TEMP id
    std::unordered_map<int, int> defIdx;

    auto countUse = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::TEMP) useCount[r.id]++;
    };

    for (int i = 0; i < (int)instrs.size(); i++) {
        const auto& ins = instrs[i];
        if (ins.result.kind == IRRef::Kind::TEMP) defIdx[ins.result.id] = i;
        for (const auto& op : ins.typedOperands) countUse(op);
    }

    // Iterate STORE_VAR instructions — two patterns:
    //   A) result=VAR, typedOperands={src_temp}          (from assignments)
    //   B) result=NONE, typedOperands={var, src_temp}    (from compound assigns)
    for (int i = 0; i < (int)instrs.size(); i++) {
        auto& ins = instrs[i];
        if (ins.opcode != IROpcode::STORE_VAR) continue;

        IRRef destVar;
        IRRef srcRef;
        if (ins.typedOperands.size() == 1 && ins.result.kind == IRRef::Kind::VAR) {
            // Pattern A
            destVar = ins.result;
            srcRef  = ins.typedOperands[0];
        } else if (ins.typedOperands.size() == 2 &&
                   ins.typedOperands[0].kind == IRRef::Kind::VAR) {
            // Pattern B
            destVar = ins.typedOperands[0];
            srcRef  = ins.typedOperands[1];
        } else {
            continue;
        }

        if (srcRef.kind != IRRef::Kind::TEMP) continue;
        if (useCount[srcRef.id] != 1) continue;
        auto dit = defIdx.find(srcRef.id);
        if (dit == defIdx.end()) continue;

        IRInstruction& def = instrs[dit->second];
        if (def.result.kind != IRRef::Kind::TEMP) continue;
        def.result = destVar;
        ins.opcode = IROpcode::NOP;
        ins.typedOperands.clear();
        ins.result = IRRef(); // clear to match NOP removal condition
    }

    // Remove NOPs
    instrs.erase(
        std::remove_if(instrs.begin(), instrs.end(),
                       [](const IRInstruction& i) { return i.opcode == IROpcode::NOP && i.typedOperands.empty() && !i.result.isValid(); }),
        instrs.end());
}

// Dead code elimination: remove TEMP-producing instructions whose result is never used
// (safe: does NOT remove side-effect ops PRINT, HALT, CALL, STORE_VAR, RETURN, etc.)
static void runDCE(std::vector<IRInstruction>& instrs) {
    // Side-effect opcodes — always kept
    auto hasSideEffect = [](IROpcode op) {
        switch (op) {
            case IROpcode::PRINT:
            case IROpcode::INPUT:        // Term.ask reads user input (side effect!)
            case IROpcode::HALT:
            case IROpcode::CALL:
            case IROpcode::LIB_CALL:
            case IROpcode::STORE_VAR:
            case IROpcode::RETURN:
            case IROpcode::JUMP:
            case IROpcode::JUMP_IF_FALSE:
            case IROpcode::JUMP_IF_TRUE:
            case IROpcode::LABEL:
            case IROpcode::ALLOC:
            case IROpcode::STORE_INDEX:
            case IROpcode::IF_BEGIN:
            case IROpcode::IF_ELSE:
            case IROpcode::IF_END:
            case IROpcode::WHILE_BEGIN:
            case IROpcode::WHILE_END:
            case IROpcode::FOR_BEGIN:
            case IROpcode::FOR_END:
            case IROpcode::TRY_BEGIN:
            case IROpcode::CATCH_BEGIN:
            case IROpcode::AFTER_BEGIN:
            case IROpcode::TRY_END:
            case IROpcode::TAG_BEGIN:
            case IROpcode::TAG_END:
                return true;
            default:
                return false;
        }
    };

    // Collect used TEMP ids
    std::unordered_set<int> usedTemps;
    for (const auto& ins : instrs) {
        for (const auto& op : ins.typedOperands)
            if (op.kind == IRRef::Kind::TEMP) usedTemps.insert(op.id);
    }

    // Remove instructions that produce a TEMP that is never used and have no side effect
    instrs.erase(
        std::remove_if(instrs.begin(), instrs.end(), [&](const IRInstruction& ins) {
            if (hasSideEffect(ins.opcode)) return false;
            if (ins.result.kind == IRRef::Kind::TEMP && !usedTemps.count(ins.result.id))
                return true;
            return false;
        }),
        instrs.end());
}

// ─── Constexpr function folding ─────────────────────────────────────────────
// Evaluates pure user-defined functions at compile time when all arguments are
// constants. Equivalent to C++ constexpr: if the result is known at compile
// time, the CALL is replaced with the folded constant.

struct ConstExprEnv {
    std::unordered_map<int, IRValue> temps;   // temp id → value
    std::unordered_map<std::string, IRValue> vars; // var name → value
    bool failed = false;

    void set(const IRRef& r, const IRValue& v) {
        if (r.kind == IRRef::Kind::TEMP) temps[r.id] = v;
        else if (r.kind == IRRef::Kind::VAR) {
            // We need the symbol table to resolve VAR names but don't have it here.
            // Store by id as fallback.
            temps[-r.id - 1] = v;
        }
    }
    bool get(const IRRef& r, IRValue& out) const {
        if (r.kind == IRRef::Kind::CONST) { out = r.value; return true; }
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = temps.find(r.id);
            if (it != temps.end()) { out = it->second; return true; }
        }
        if (r.kind == IRRef::Kind::VAR) {
            auto it = temps.find(-r.id - 1);
            if (it != temps.end()) { out = it->second; return true; }
        }
        return false;
    }
};

static IRValue applyBinOp(IROpcode op, const IRValue& L, const IRValue& R) {
    // String concatenation: any ADD where either side is a string
    if (op == IROpcode::ADD) {
        bool ls = L.type == IRType::STRING, rs = R.type == IRType::STRING;
        if (ls || rs) {
            auto toStr = [](const IRValue& v) -> std::string {
                if (v.type == IRType::STRING) return std::get<std::string>(v.data);
                if (v.type == IRType::INT)    return std::to_string(std::get<int64_t>(v.data));
                if (v.type == IRType::FLOAT)  return std::to_string(std::get<double>(v.data));
                if (v.type == IRType::BOOL)   return std::get<bool>(v.data) ? "true" : "false";
                return "";
            };
            return IRValue(toStr(L) + toStr(R));
        }
    }
    auto asDbl = [](const IRValue& v) -> double {
        if (v.type == IRType::FLOAT) return std::get<double>(v.data);
        if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
        if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1.0 : 0.0;
        return 0.0;
    };
    auto asInt = [](const IRValue& v) -> int64_t {
        if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
        if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
        if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
        return 0;
    };
    bool anyFloat = (L.type == IRType::FLOAT || R.type == IRType::FLOAT);
    switch (op) {
        case IROpcode::ADD:
            if (anyFloat) return IRValue(asDbl(L)+asDbl(R));
            {
                __int128 v = (__int128)asInt(L) + (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::SUB:
            if (anyFloat) return IRValue(asDbl(L)-asDbl(R));
            {
                __int128 v = (__int128)asInt(L) - (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::MUL:
        case IROpcode::PMUL:
            if (anyFloat) return IRValue(asDbl(L)*asDbl(R));
            {
                __int128 v = (__int128)asInt(L) * (__int128)asInt(R);
                if (v > std::numeric_limits<int64_t>::max() || v < std::numeric_limits<int64_t>::min())
                    return IRValue(std::numeric_limits<double>::infinity());
                return IRValue((int64_t)v);
            }
        case IROpcode::DIV:  {
            double d = asDbl(R);
            if (d == 0.0) return IRValue((int64_t)0);
            double q = asDbl(L) / d;
            int64_t qi = (int64_t)q;
            return (q == (double)qi) ? IRValue(qi) : IRValue(q);
        }
        case IROpcode::IDIV: { int64_t r = asInt(R); return r != 0 ? IRValue(asInt(L)/r) : IRValue((int64_t)0); }
        case IROpcode::MOD:  { int64_t r = asInt(R); return r != 0 ? IRValue(asInt(L)%r) : IRValue(0); }
        case IROpcode::EQ:   return IRValue(anyFloat ? (asDbl(L)==asDbl(R)) : (asInt(L)==asInt(R)));
        case IROpcode::NEQ:  return IRValue(anyFloat ? (asDbl(L)!=asDbl(R)) : (asInt(L)!=asInt(R)));
        case IROpcode::LT:   return IRValue(anyFloat ? (asDbl(L)< asDbl(R)) : (asInt(L)< asInt(R)));
        case IROpcode::GT:   return IRValue(anyFloat ? (asDbl(L)> asDbl(R)) : (asInt(L)> asInt(R)));
        case IROpcode::LTE:  return IRValue(anyFloat ? (asDbl(L)<=asDbl(R)) : (asInt(L)<=asInt(R)));
        case IROpcode::GTE:  return IRValue(anyFloat ? (asDbl(L)>=asDbl(R)) : (asInt(L)>=asInt(R)));
        case IROpcode::AND:  return IRValue(asInt(L) && asInt(R));
        case IROpcode::OR:   return IRValue(asInt(L) || asInt(R));
        case IROpcode::XOR:  return IRValue((bool)(asInt(L)) != (bool)(asInt(R)));
        case IROpcode::XNOR: return IRValue((bool)(asInt(L)) == (bool)(asInt(R)));
        case IROpcode::XSUB: { int64_t d = asInt(L)-asInt(R); return IRValue((d<0?-d:d)+1); }
        default: return IRValue(0);
    }
}

static double constexprAsDouble(const IRValue& v) {
    if (v.type == IRType::FLOAT) return std::get<double>(v.data);
    if (v.type == IRType::INT)   return (double)std::get<int64_t>(v.data);
    if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1.0 : 0.0;
    return 0.0;
}

static int64_t constexprAsInt(const IRValue& v) {
    if (v.type == IRType::INT)   return std::get<int64_t>(v.data);
    if (v.type == IRType::FLOAT) return (int64_t)std::get<double>(v.data);
    if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
    return 0;
}

static int64_t constexprGcd(int64_t a, int64_t b) {
    a = std::llabs(a);
    b = std::llabs(b);
    while (b != 0) {
        int64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static bool constexprIsPrime(int64_t n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    for (int64_t d = 3; d <= n / d; d += 2)
        if (n % d == 0) return false;
    return true;
}

static double constexprPiPrecision(int64_t digits, double value) {
    if (digits <= 0 || digits > 15) return value;
    double f = std::pow(10.0, (double)digits);
    return std::floor(value * f) / f;
}

static std::string normalizeMathName(std::string name, bool mathImported) {
    if (name.rfind("AcMath.", 0) == 0) name = name.substr(7);
    if (name.rfind("math_", 0) == 0) name = "math." + name.substr(5);
    for (char& c : name) {
        if (c == ':') c = '.';
    }
    if (name == "math.abs.int") name = "math.abs_int";
    if (name == "math.mod.int") name = "math.mod_int";
    if (name == "math.pi.prec") name = "math.pi";
    if (name == "math.e.prec") name = "math.e";
    if (name == "math.phi.prec") name = "math.phi";

    if (name.find('.') == std::string::npos && mathImported) {
        static const std::unordered_set<std::string> bareMath = {
            "pi","e","tau","em","phi","inf",
            "sin","cos","tan","csc","sec","cot",
            "asin","acos","atan","acsc","asec","acot","atan2",
            "deg2rad","rad2deg",
            "pow","sqrt","cbrt","hypot",
            "floor","ceil","round","abs","abs_int",
            "ln","log","log2","log10",
            "mod","mod_int",
            "to_int","to_dec",
            "gcd","lcm","is_prime","clamp"
        };
        if (bareMath.count(name)) name = "math." + name;
    }
    return name;
}

static IRValue constexprMathConstant(const std::string& normalizedName) {
    if (normalizedName == "math.pi")  return IRValue(3.141592653589793238462643383279502884197);
    if (normalizedName == "math.e")   return IRValue(2.718281828459045235360287471352662497757);
    if (normalizedName == "math.tau") return IRValue(6.283185307179586476925286766559005768394);
    if (normalizedName == "math.em")  return IRValue(0.5772156649015328606065120900824024310422);
    if (normalizedName == "math.phi") return IRValue(1.618033988749894848204586834365638117720);
    if (normalizedName == "math.inf") return IRValue(std::numeric_limits<double>::infinity());
    return IRValue();
}

static IRValue tryConstexprMathEval(const std::string& rawName,
                                    const std::vector<IRValue>& args,
                                    const IRProgram& prog)
{
    std::string name = normalizeMathName(rawName, prog.importedLibs.count("math") > 0);
    if (name.rfind("math.", 0) != 0) return IRValue();

    auto constant = constexprMathConstant(name);
    if (args.empty() && constant.type != IRType::VOID) return constant;

    auto arity = [&](size_t n) { return args.size() == n; };
    auto D = [&](size_t i) { return constexprAsDouble(args[i]); };
    auto I = [&](size_t i) { return constexprAsInt(args[i]); };

    if (name == "math.pi"  && arity(1)) return IRValue(constexprPiPrecision(I(0), 3.141592653589793238462643383279502884197));
    if (name == "math.e"   && arity(1)) return IRValue(constexprPiPrecision(I(0), 2.718281828459045235360287471352662497757));
    if (name == "math.phi" && arity(1)) return IRValue(constexprPiPrecision(I(0), 1.618033988749894848204586834365638117720));

    if (name == "math.sin"     && arity(1)) return IRValue(std::sin(D(0)));
    if (name == "math.cos"     && arity(1)) return IRValue(std::cos(D(0)));
    if (name == "math.tan"     && arity(1)) return IRValue(std::tan(D(0)));
    if (name == "math.csc"     && arity(1)) return IRValue(1.0 / std::sin(D(0)));
    if (name == "math.sec"     && arity(1)) return IRValue(1.0 / std::cos(D(0)));
    if (name == "math.cot"     && arity(1)) return IRValue(std::cos(D(0)) / std::sin(D(0)));
    if (name == "math.asin"    && arity(1)) return IRValue(std::asin(D(0)));
    if (name == "math.acos"    && arity(1)) return IRValue(std::acos(D(0)));
    if (name == "math.atan"    && arity(1)) return IRValue(std::atan(D(0)));
    if (name == "math.acsc"    && arity(1)) return IRValue(std::asin(1.0 / D(0)));
    if (name == "math.asec"    && arity(1)) return IRValue(std::acos(1.0 / D(0)));
    if (name == "math.acot"    && arity(1)) return IRValue((D(0) > 0 ? 1.0 : -1.0) * std::atan(1.0 / std::fabs(D(0))));
    if (name == "math.atan2"   && arity(2)) return IRValue(std::atan2(D(0), D(1)));
    if (name == "math.deg2rad" && arity(1)) return IRValue(D(0) * (3.141592653589793238462643383279502884197 / 180.0));
    if (name == "math.rad2deg" && arity(1)) return IRValue(D(0) * (180.0 / 3.141592653589793238462643383279502884197));

    if (name == "math.pow"   && arity(2)) {
        double v = std::pow(D(0), D(1));
        if (std::isfinite(v) && std::fabs(v) > (double)std::numeric_limits<int64_t>::max())
            return IRValue(std::numeric_limits<double>::infinity());
        return IRValue(v);
    }
    if (name == "math.sqrt"  && arity(1)) return IRValue(std::sqrt(D(0)));
    if (name == "math.cbrt"  && arity(1)) return IRValue(std::cbrt(D(0)));
    if (name == "math.hypot" && arity(2)) return IRValue(std::hypot(D(0), D(1)));
    if (name == "math.abs"   && arity(1)) return IRValue(std::fabs(D(0)));
    if (name == "math.abs_int" && arity(1)) return IRValue((int64_t)std::llabs(I(0)));
    if (name == "math.floor" && arity(1)) return IRValue(std::floor(D(0)));
    if (name == "math.ceil"  && arity(1)) return IRValue(std::ceil(D(0)));
    if (name == "math.round" && arity(1)) return IRValue(std::round(D(0)));

    if (name == "math.ln"    && arity(1)) return IRValue(std::log(D(0)));
    if (name == "math.log"   && arity(2)) return IRValue(std::log(D(1)) / std::log(D(0)));
    if (name == "math.log2"  && arity(1)) return IRValue(std::log2(D(0)));
    if (name == "math.log10" && arity(1)) return IRValue(std::log10(D(0)));

    if (name == "math.mod" && arity(2)) {
        double r = std::fmod(D(0), D(1));
        if (r != 0.0 && (r < 0) != (D(1) < 0)) r += D(1);
        return IRValue(r);
    }
    if (name == "math.mod_int" && arity(2)) {
        int64_t b = I(1);
        if (b == 0) return IRValue();
        int64_t r = I(0) % b;
        if (r != 0 && (r < 0) != (b < 0)) r += b;
        return IRValue(r);
    }

    if (name == "math.to_int" && arity(1)) return IRValue((int64_t)D(0));
    if (name == "math.to_dec" && arity(1)) return IRValue((double)I(0));
    if (name == "math.gcd" && arity(2)) return IRValue(constexprGcd(I(0), I(1)));
    if (name == "math.lcm" && arity(2)) {
        int64_t g = constexprGcd(I(0), I(1));
        if (g == 0) return IRValue((int64_t)0);
        return IRValue((int64_t)std::llabs((I(0) / g) * I(1)));
    }
    if (name == "math.is_prime" && arity(1)) return IRValue(constexprIsPrime(I(0)));
    if (name == "math.clamp" && arity(3)) return IRValue(std::min(std::max(D(0), D(1)), D(2)));

    return IRValue();
}

// Returns true if opcode has side effects (makes a function non-pure)
static bool isSideEffect(IROpcode op) {
    switch (op) {
        case IROpcode::PRINT: case IROpcode::INPUT: case IROpcode::LIB_CALL:
        case IROpcode::HALT: case IROpcode::SOFT_HALT: case IROpcode::SLEEP:
        case IROpcode::EVENT_BIND: case IROpcode::EVENT_TRIGGER:
        case IROpcode::EVAL: case IROpcode::RESTART_PROGRAM:
        case IROpcode::ALIAS_DECL:
            return true;
        default: return false;
    }
}

// Try to evaluate a user-defined function at compile time.
// Returns a valid IRValue on success, or IRValue(VOID) if the function is not pure
// or the result can't be determined.
static IRValue tryConstexprEval(const std::string& fname,
                                const std::vector<IRValue>& args,
                                const IRProgram& prog,
                                int depth = 0)
{
    static const int MAX_DEPTH = 64;       // matches default backend recursion limit
    static const int MAX_STEPS = 15000000; // 15M steps per constexpr evaluation
    if (depth > MAX_DEPTH) return IRValue();

    const IRFunction* fn = prog.findFunction(fname);
    if (!fn) return IRValue();

    // Check purity first
    for (const auto& ins : fn->instructions)
        if (isSideEffect(ins.opcode)) return IRValue();

    ConstExprEnv env;
    // Bind parameters from fn->parameters to args
    for (size_t k = 0; k < fn->parameters.size() && k < args.size(); ++k) {
        IRRef paramRef;
        paramRef.kind = IRRef::Kind::VAR;
        paramRef.id   = k; // placeholder — map by position
        // Store under a recognizable key: negative of param index
        env.temps[-(int)k - 10000] = args[k];
        // Also store by param name if we can find the symbol
        // Actually store by the param name via the vars map
        env.vars[fn->parameters[k]] = args[k];
    }

    // Pre-scan: label positions and WHILE_BEGIN/WHILE_END pairing
    std::unordered_map<int, int> labelPc;         // label-id → pc
    std::unordered_map<int, int> whileEndToBegin; // end_pc → begin_pc (for WHILE_END)
    std::unordered_map<int, int> whileBeginToEnd; // begin_pc → end_pc (for break sentinel)
    {
        std::stack<int> stk;
        for (int i = 0; i < (int)fn->instructions.size(); ++i) {
            const auto& ins2 = fn->instructions[i];
            if (ins2.opcode == IROpcode::LABEL && !ins2.typedOperands.empty()
                    && ins2.typedOperands[0].kind == IRRef::Kind::LABEL)
                labelPc[ins2.typedOperands[0].id] = i;
            else if (ins2.opcode == IROpcode::WHILE_BEGIN)
                stk.push(i);
            else if (ins2.opcode == IROpcode::WHILE_END && !stk.empty()) {
                int bpc = stk.top(); stk.pop();
                whileEndToBegin[i] = bpc;
                whileBeginToEnd[bpc] = i;
            }
        }
    }

    int steps = 0;
    int pc = 0;
    bool inSkippedIf = false;
    int skipDepth = 0;

    while (pc < (int)fn->instructions.size() && steps < MAX_STEPS) {
        ++steps;
        const auto& ins = fn->instructions[pc];

        // Skip branches of false conditions
        if (inSkippedIf) {
            if (ins.opcode == IROpcode::IF_BEGIN) { ++skipDepth; ++pc; continue; }
            if (ins.opcode == IROpcode::IF_ELSE && skipDepth == 0) { inSkippedIf = false; ++pc; continue; }
            if (ins.opcode == IROpcode::IF_END) {
                if (skipDepth == 0) inSkippedIf = false;
                else --skipDepth;
                ++pc; continue;
            }
            ++pc; continue;
        }

        auto getRef = [&](const IRRef& r) -> IRValue {
            if (r.kind == IRRef::Kind::CONST) return r.value;
            if (r.kind == IRRef::Kind::TEMP) {
                auto it = env.temps.find(r.id); if (it != env.temps.end()) return it->second;
            }
            if (r.kind == IRRef::Kind::VAR) {
                // Try symbol name lookup via prog.symbols
                std::string vname = r.id >= 0 ? prog.symbols.getName(r.id) : "";
                if (!vname.empty()) {
                    auto it = env.vars.find(vname);
                    if (it != env.vars.end()) return it->second;
                }
            }
            return IRValue(); // unknown
        };
        auto setRef = [&](const IRRef& r, const IRValue& v) {
            if (r.kind == IRRef::Kind::TEMP) env.temps[r.id] = v;
            else if (r.kind == IRRef::Kind::VAR) {
                std::string vname = r.id >= 0 ? prog.symbols.getName(r.id) : "";
                if (!vname.empty()) env.vars[vname] = v;
            }
        };

        switch (ins.opcode) {
        case IROpcode::NOP: case IROpcode::FREE_DECL: case IROpcode::FUNC_BEGIN:
        case IROpcode::FUNC_END: case IROpcode::TAG_BEGIN: case IROpcode::TAG_END:
            break;
        case IROpcode::STORE_VAR: {
            IRValue val;
            if (ins.typedOperands.size() >= 2) val = getRef(ins.typedOperands[1]);
            else if (!ins.typedOperands.empty()) val = getRef(ins.typedOperands[0]);
            if (val.type == IRType::VOID) return IRValue();
            if (ins.typedOperands.size() >= 2) setRef(ins.typedOperands[0], val);
            else if (ins.result.isValid()) setRef(ins.result, val);
            break;
        }
        case IROpcode::RETURN: {
            if (ins.typedOperands.empty()) return IRValue();
            IRValue v = getRef(ins.typedOperands[0]);
            return v; // success!
        }
        case IROpcode::IF_BEGIN: {
            if (ins.typedOperands.empty()) { ++pc; continue; }
            IRValue cond = getRef(ins.typedOperands[0]);
            if (cond.type == IRType::VOID) return IRValue();
            bool condTrue = false;
            if (cond.type == IRType::BOOL)  condTrue = std::get<bool>(cond.data);
            else if (cond.type == IRType::INT) condTrue = std::get<int64_t>(cond.data) != 0;
            else if (cond.type == IRType::FLOAT) condTrue = std::get<double>(cond.data) != 0.0;
            if (!condTrue) { inSkippedIf = true; skipDepth = 0; }
            break;
        }
        case IROpcode::IF_ELSE:
            // We're here because the if-branch was taken → skip else
            inSkippedIf = true; skipDepth = 0;
            break;
        case IROpcode::IF_END:
            break;
        case IROpcode::ADD: case IROpcode::SUB: case IROpcode::MUL: case IROpcode::PMUL:
        case IROpcode::DIV: case IROpcode::IDIV: case IROpcode::MOD: case IROpcode::EQ: case IROpcode::NEQ:
        case IROpcode::LT: case IROpcode::GT: case IROpcode::LTE: case IROpcode::GTE:
        case IROpcode::AND: case IROpcode::OR: case IROpcode::XOR: case IROpcode::XNOR:
        case IROpcode::XSUB:
            if (ins.result.isValid() && ins.typedOperands.size() >= 2) {
                IRValue L = getRef(ins.typedOperands[0]);
                IRValue R = getRef(ins.typedOperands[1]);
                if (L.type == IRType::VOID || R.type == IRType::VOID) return IRValue();
                setRef(ins.result, applyBinOp(ins.opcode, L, R));
            }
            break;
        case IROpcode::NOT:
            if (ins.result.isValid() && !ins.typedOperands.empty()) {
                IRValue v = getRef(ins.typedOperands[0]);
                if (v.type == IRType::VOID) return IRValue();
                bool b = (v.type == IRType::INT   ? std::get<int64_t>(v.data) != 0
                        : v.type == IRType::FLOAT ? std::get<double>(v.data) != 0
                        : v.type == IRType::BOOL  ? std::get<bool>(v.data)
                        : false);
                setRef(ins.result, IRValue(!b));
            }
            break;
        case IROpcode::CALL:
            if (ins.result.isValid() && !ins.typedOperands.empty()) {
                std::string callee;
                const auto& fn_ref = ins.typedOperands[0];
                if (fn_ref.kind == IRRef::Kind::VAR)
                    callee = fn_ref.id >= 0 ? prog.symbols.getName(fn_ref.id) : "";
                if (callee.empty()) return IRValue();
                std::vector<IRValue> cargs;
                for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
                    IRValue av = getRef(ins.typedOperands[j]);
                    if (av.type == IRType::VOID) return IRValue(); // unknown arg
                    cargs.push_back(av);
                }
                IRValue cv = tryConstexprMathEval(callee, cargs, prog);
                if (cv.type == IRType::VOID)
                    cv = tryConstexprEval(callee, cargs, prog, depth + 1);
                if (cv.type == IRType::VOID) return IRValue();
                setRef(ins.result, cv);
            }
            break;
        case IROpcode::LABEL:
            break; // position marker already in labelPc
        case IROpcode::JUMP: {
            if (ins.typedOperands.empty()) return IRValue();
            const auto& lbl = ins.typedOperands[0];
            if (lbl.kind != IRRef::Kind::LABEL) return IRValue();
            auto it = labelPc.find(lbl.id);
            if (it == labelPc.end()) return IRValue();
            pc = it->second;
            continue;
        }
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_TRUE: {
            if (ins.typedOperands.size() < 2) return IRValue();
            IRValue cond = getRef(ins.typedOperands[0]);
            if (cond.type == IRType::VOID) return IRValue();
            bool condTrue = (cond.type == IRType::BOOL  ? std::get<bool>(cond.data)
                           : cond.type == IRType::INT   ? std::get<int64_t>(cond.data) != 0
                           : cond.type == IRType::FLOAT ? std::get<double>(cond.data) != 0.0
                           : false);
            bool shouldJump = (ins.opcode == IROpcode::JUMP_IF_FALSE) ? !condTrue : condTrue;
            if (shouldJump) {
                const auto& lbl = ins.typedOperands[1];
                if (lbl.kind == IRRef::Kind::LABEL) {
                    // Low-level: direct label jump
                    auto it = labelPc.find(lbl.id);
                    if (it == labelPc.end()) return IRValue();
                    pc = it->second;
                    continue;
                } else {
                    // High-level __break__ sentinel: jump past the matching WHILE_END
                    int beginPc = -1, scanDepth = 0;
                    for (int i = pc - 1; i >= 0; --i) {
                        if (fn->instructions[i].opcode == IROpcode::WHILE_END) ++scanDepth;
                        else if (fn->instructions[i].opcode == IROpcode::WHILE_BEGIN) {
                            if (scanDepth == 0) { beginPc = i; break; }
                            --scanDepth;
                        }
                    }
                    if (beginPc < 0) return IRValue();
                    auto it = whileBeginToEnd.find(beginPc);
                    if (it == whileBeginToEnd.end()) return IRValue();
                    pc = it->second + 1; // after WHILE_END
                    continue;
                }
            }
            break;
        }
        case IROpcode::WHILE_BEGIN:
            break;
        case IROpcode::WHILE_END: {
            // High-level loop: jump back to WHILE_BEGIN for next iteration
            auto it = whileEndToBegin.find(pc);
            if (it == whileEndToBegin.end()) return IRValue();
            pc = it->second;
            continue;
        }
        case IROpcode::LOAD_CONST:
            if (ins.result.isValid() && !ins.typedOperands.empty())
                setRef(ins.result, getRef(ins.typedOperands[0]));
            break;
        default:
            // Unknown opcode — give up
            return IRValue();
        }
        ++pc;
    }
    return IRValue(); // no RETURN found or ran out of steps
}

static void runMathConstantRefs(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    auto foldRef = [&](IRRef& r) {
        if (r.kind != IRRef::Kind::VAR || r.id < 0) return;
        std::string name = normalizeMathName(prog.symbols.getName(r.id), false);
        IRValue constant = constexprMathConstant(name);
        if (constant.type != IRType::VOID)
            r = IRRef::constant(constant);
    };

    for (auto& ins : instrs) {
        for (auto& op : ins.typedOperands)
            foldRef(op);
    }
}

static void runLocalConstFolding(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    std::unordered_map<int, IRValue> tempConst;
    std::unordered_map<int, IRValue> varConst;

    auto clearOnControlFlow = [&](IROpcode op) {
        switch (op) {
            case IROpcode::LABEL:
            case IROpcode::JUMP:
            case IROpcode::JUMP_IF_TRUE:
            case IROpcode::JUMP_IF_FALSE:
            case IROpcode::IF_BEGIN:
            case IROpcode::IF_ELSE:
            case IROpcode::IF_END:
            case IROpcode::WHILE_BEGIN:
            case IROpcode::WHILE_END:
            case IROpcode::FOR_BEGIN:
            case IROpcode::FOR_END:
                return true;
            default:
                return false;
        }
    };

    auto known = [&](const IRRef& r, IRValue& out) -> bool {
        if (r.kind == IRRef::Kind::CONST && r.value.type != IRType::VOID) {
            out = r.value;
            return true;
        }
        if (r.kind == IRRef::Kind::TEMP) {
            auto it = tempConst.find(r.id);
            if (it != tempConst.end()) { out = it->second; return true; }
        }
        if (r.kind == IRRef::Kind::VAR) {
            auto it = varConst.find(r.id);
            if (it != varConst.end()) { out = it->second; return true; }
        }
        return false;
    };

    auto remember = [&](const IRRef& r, const IRValue& v) {
        if (r.kind == IRRef::Kind::TEMP) tempConst[r.id] = v;
        else if (r.kind == IRRef::Kind::VAR) varConst[r.id] = v;
    };

    auto forget = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::TEMP) tempConst.erase(r.id);
        else if (r.kind == IRRef::Kind::VAR) varConst.erase(r.id);
    };

    for (auto& ins : instrs) {
        if (clearOnControlFlow(ins.opcode)) {
            tempConst.clear();
            varConst.clear();
            continue;
        }

        switch (ins.opcode) {
            case IROpcode::LOAD_CONST: {
                if (ins.result.isValid() && !ins.typedOperands.empty()) {
                    IRValue v;
                    if (known(ins.typedOperands[0], v)) {
                        ins.typedOperands[0] = IRRef::constant(v);
                        remember(ins.result, v);
                    } else {
                        forget(ins.result);
                    }
                }
                break;
            }

            case IROpcode::STORE_VAR: {
                IRRef dest;
                IRRef src;
                if (ins.typedOperands.size() >= 2) {
                    dest = ins.typedOperands[0];
                    src = ins.typedOperands[1];
                } else if (ins.result.isValid() && !ins.typedOperands.empty()) {
                    dest = ins.result;
                    src = ins.typedOperands[0];
                }

                IRValue v;
                if (dest.isValid() && known(src, v)) {
                    if (ins.typedOperands.size() >= 2) ins.typedOperands[1] = IRRef::constant(v);
                    else ins.typedOperands[0] = IRRef::constant(v);
                    remember(dest, v);
                } else if (dest.isValid()) {
                    forget(dest);
                }
                break;
            }

            case IROpcode::ADD: case IROpcode::SUB: case IROpcode::MUL: case IROpcode::PMUL:
            case IROpcode::DIV: case IROpcode::IDIV: case IROpcode::MOD: case IROpcode::EQ: case IROpcode::NEQ:
            case IROpcode::LT: case IROpcode::GT: case IROpcode::LTE: case IROpcode::GTE:
            case IROpcode::AND: case IROpcode::OR: case IROpcode::XOR: case IROpcode::XNOR:
            case IROpcode::XSUB: {
                if (!ins.result.isValid() || ins.typedOperands.size() < 2) {
                    break;
                }
                IRValue L, R;
                if (known(ins.typedOperands[0], L) && known(ins.typedOperands[1], R)) {
                    IRValue folded = applyBinOp(ins.opcode, L, R);
                    ins.opcode = IROpcode::LOAD_CONST;
                    ins.typedOperands = {IRRef::constant(folded)};
                    ins.resultType = folded.type;
                    remember(ins.result, folded);
                } else {
                    forget(ins.result);
                }
                break;
            }

            case IROpcode::NOT: {
                if (!ins.result.isValid() || ins.typedOperands.empty()) break;
                IRValue v;
                if (known(ins.typedOperands[0], v)) {
                    bool b = (v.type == IRType::INT   ? std::get<int64_t>(v.data) != 0
                            : v.type == IRType::FLOAT ? std::get<double>(v.data) != 0
                            : v.type == IRType::BOOL  ? std::get<bool>(v.data)
                            : false);
                    IRValue folded(!b);
                    ins.opcode = IROpcode::LOAD_CONST;
                    ins.typedOperands = {IRRef::constant(folded)};
                    ins.resultType = folded.type;
                    remember(ins.result, folded);
                } else {
                    forget(ins.result);
                }
                break;
            }

            case IROpcode::CALL: {
                if (!ins.result.isValid() || ins.typedOperands.empty()) break;
                const auto& fnRef = ins.typedOperands[0];
                if (fnRef.kind != IRRef::Kind::VAR || fnRef.id < 0) {
                    forget(ins.result);
                    break;
                }
                std::vector<IRValue> args;
                bool allConst = true;
                for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
                    IRValue v;
                    if (known(ins.typedOperands[j], v)) args.push_back(v);
                    else { allConst = false; break; }
                }
                if (allConst) {
                    IRValue folded = tryConstexprMathEval(prog.symbols.getName(fnRef.id), args, prog);
                    if (folded.type != IRType::VOID) {
                        ins.opcode = IROpcode::LOAD_CONST;
                        ins.typedOperands = {IRRef::constant(folded)};
                        ins.resultType = folded.type;
                        remember(ins.result, folded);
                        break;
                    }
                }
                forget(ins.result);
                break;
            }

            default:
                if (ins.result.isValid()) forget(ins.result);
                break;
        }
    }

}

// Walk all instructions; replace CALL with constant result when possible
static void runConstexprFolding(std::vector<IRInstruction>& instrs, const IRProgram& prog) {
    for (auto& ins : instrs) {
        if (ins.opcode != IROpcode::CALL) continue;
        if (!ins.result.isValid() || ins.typedOperands.empty()) continue;
        // Get function name
        const auto& fn_ref = ins.typedOperands[0];
        if (fn_ref.kind != IRRef::Kind::VAR) continue;
        std::string callee = fn_ref.id >= 0 ? prog.symbols.getName(fn_ref.id) : "";
        if (callee.empty()) continue;
        // Check all args are constants
        std::vector<IRValue> args;
        bool allConst = true;
        for (size_t j = 1; j < ins.typedOperands.size(); ++j) {
            const auto& op = ins.typedOperands[j];
            if (op.kind == IRRef::Kind::CONST && op.value.type != IRType::VOID)
                args.push_back(op.value);
            else { allConst = false; break; }
        }
        if (!allConst) continue;
        // Try to evaluate built-in constexpr math first, then user-defined pure functions.
        IRValue result = tryConstexprMathEval(callee, args, prog);
        if (result.type == IRType::VOID)
            result = tryConstexprEval(callee, args, prog);
        if (result.type == IRType::VOID) continue;
        // Replace CALL with LOAD_CONST
        IRInstruction folded(IROpcode::LOAD_CONST, ins.result, {IRRef::constant(result)});
        folded.resultType = result.type;
        ins = std::move(folded);
    }
}

static void expandAliasWrites(std::vector<IRInstruction>& instrs) {
    std::unordered_map<int, std::set<int>> groups;

    auto merge = [&](int a, int b) {
        std::set<int> merged;
        auto ia = groups.find(a);
        auto ib = groups.find(b);
        if (ia != groups.end()) merged.insert(ia->second.begin(), ia->second.end());
        if (ib != groups.end()) merged.insert(ib->second.begin(), ib->second.end());
        merged.insert(a);
        merged.insert(b);
        for (int id : merged) groups[id] = merged;
    };

    auto aliasesFor = [&](int id) {
        std::vector<int> out;
        auto it = groups.find(id);
        if (it == groups.end()) return out;
        for (int alias : it->second)
            if (alias != id) out.push_back(alias);
        return out;
    };

    std::vector<IRInstruction> out;
    out.reserve(instrs.size());

    for (auto ins : instrs) {
        if (ins.opcode == IROpcode::ALIAS_DECL) {
            if (ins.typedOperands.size() >= 2 &&
                ins.typedOperands[0].kind == IRRef::Kind::VAR &&
                ins.typedOperands[1].kind == IRRef::Kind::VAR) {
                merge(ins.typedOperands[0].id, ins.typedOperands[1].id);
            }
            continue;
        }

        out.push_back(ins);

        if (ins.opcode != IROpcode::STORE_VAR) continue;

        IRRef dest;
        bool destInResult = false;
        if (ins.result.kind == IRRef::Kind::VAR && ins.typedOperands.size() == 1) {
            dest = ins.result;
            destInResult = true;
        } else if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR) {
            dest = ins.typedOperands[0];
        } else {
            continue;
        }

        for (int aliasId : aliasesFor(dest.id)) {
            IRInstruction dup = ins;
            if (destInResult) dup.result = IRRef::var(aliasId);
            else dup.typedOperands[0] = IRRef::var(aliasId);
            out.push_back(std::move(dup));
        }
    }

    instrs = std::move(out);
}

static void expandAliases(IRProgram& prog) {
    expandAliasWrites(prog.globalInit);
    for (auto& fn : prog.functions) expandAliasWrites(fn.instructions);
}

static void runOptPasses(IRProgram& prog) {
    // ── Collect usage info BEFORE optimization (constexpr removes CALLs) ──────
    std::set<std::string> calledFnsPre, writtenVarsPre, readVarsPre;
    if (prog.backend != "LIB") {
        auto scanPre = [&](const std::vector<IRInstruction>& instrs) {
            for (const auto& ins : instrs) {
                if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()) {
                    const auto& r = ins.typedOperands[0];
                    if (r.kind == IRRef::Kind::VAR && r.id >= 0)
                        calledFnsPre.insert(prog.symbols.getName(r.id));
                    // Arguments that are function names passed as values → mark as called
                    for (size_t ai = 1; ai < ins.typedOperands.size(); ++ai) {
                        const auto& arg = ins.typedOperands[ai];
                        if (arg.kind == IRRef::Kind::VAR && arg.id >= 0) {
                            std::string argName = prog.symbols.getName(arg.id);
                            if (prog.findFunction(argName))
                                calledFnsPre.insert(argName);
                        }
                    }
                }
                for (const auto& op : ins.typedOperands) {
                    if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                        readVarsPre.insert(prog.symbols.getName(op.id));
                }
                if (ins.opcode == IROpcode::STORE_VAR) {
                    std::string v;
                    if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                        v = prog.symbols.getName(ins.typedOperands[0].id);
                    else if (ins.result.isValid() && ins.result.kind == IRRef::Kind::VAR)
                        v = prog.symbols.getName(ins.result.id);
                    if (!v.empty() && v[0] != '_') writtenVarsPre.insert(v);
                }
            }
        };
        scanPre(prog.globalInit);
        for (const auto& fn : prog.functions) scanPre(fn.instructions);
        // Remove written vars that ARE read (they appear in both sets)
        for (auto it = writtenVarsPre.begin(); it != writtenVarsPre.end(); ) {
            if (readVarsPre.count(*it)) it = writtenVarsPre.erase(it);
            else ++it;
        }
        // But we only warn if the write-only var is NOT a function parameter
        // (function params appear in readVarsPre if they're used inside the function)
    }

    // Constexpr folding: run multiple passes so nested folds cascade
    // (inner call folds → copy-prop propagates → outer call folds)
    for (int pass = 0; pass < 3; ++pass) {
        for (auto& fn : prog.functions) {
            runMathConstantRefs(fn.instructions, prog);
            runLocalConstFolding(fn.instructions, prog);
            runConstexprFolding(fn.instructions, prog);
        }
        runMathConstantRefs(prog.globalInit, prog);
        runLocalConstFolding(prog.globalInit, prog);
        runConstexprFolding(prog.globalInit, prog);
        for (auto& fn : prog.functions) {
            runCopyProp(fn.instructions);
            runDCE(fn.instructions);
        }
        runCopyProp(prog.globalInit);
        runDCE(prog.globalInit);
    }

    // Toxic warnings: unused variables, functions (skip in AC->LIB)
    if (prog.backend != "LIB") {
        // Variables written but never read (collected pre-opt)
        for (const auto& v : writtenVarsPre)
            std::cerr << "Toxic: Compiler is slacking off — '" << v << "' assigned but never read\n";

        // Functions defined but never called (use pre-opt call set)
        for (const auto& fn : prog.functions) {
            if (fn.name.empty() || fn.name[0] == '_') continue;
            if (fn.name == "main" || fn.name == "mainloop") continue;
            if (!calledFnsPre.count(fn.name))
                std::cerr << "Toxic: Compiler is slacking off — '" << fn.name << "' defined but never called\n";
        }
    }
}

// ─── public API ─────────────────────────────────────────────────────────────

// Inject auto-cleanup calls into __ac_shutoff__ for known imported libraries.
// Also creates the function if it doesn't exist but auto-cleanup is needed.
static void injectAutoShutoff(IRProgram& prog) {
    bool needsMaudio = prog.importedLibs.count("machine-audio") ||
                       prog.importedLibs.count("maudio");
    if (!needsMaudio) return;

    // Find or create __ac_shutoff__
    IRFunction* fn = prog.findFunction("__ac_shutoff__");
    bool created = false;
    if (!fn) {
        prog.functions.push_back(IRFunction("__ac_shutoff__"));
        fn = &prog.functions.back();
        created = true;
        prog.hasShutoff = true;
    }

    // Find insertion point: before first RETURN, or at end
    auto& instrs = fn->instructions;
    size_t insertAt = instrs.size();
    for (size_t k = 0; k < instrs.size(); k++) {
        if (instrs[k].opcode == IROpcode::RETURN) { insertAt = k; break; }
    }

    // machine-audio → maudio.stop()  (LIB_CALL so each backend formats it natively)
    if (needsMaudio) {
        IRInstruction callStop(IROpcode::LIB_CALL);
        callStop.typedOperands.push_back(IRRef::constant(IRValue(std::string("maudio.stop"))));
        instrs.insert(instrs.begin() + (long)insertAt, std::move(callStop));
        insertAt++;
    }

    if (created)
        instrs.push_back(IRInstruction(IROpcode::RETURN));
}

// Wrap globalInit in a once-only restart loop.
// Semantics: on the first pass /restart sets the flag and continues from the top;
// on the second pass /restart is skipped (flag already set) and execution falls through.
static void wrapWithRestartLoop(IRProgram& prog) {
    int rdoneId = prog.symbols.intern("_ac_restart_done", IRType::BOOL);
    int& ntc    = prog.globalTempCount;
    int& nlc    = prog.globalLabelCount;

    // Separate library import instructions (they stay outside the loop)
    std::vector<IRInstruction> imports, body;
    for (auto& instr : prog.globalInit) {
        bool isImport = (instr.opcode == IROpcode::LIB_CALL &&
                         !instr.typedOperands.empty() &&
                         instr.typedOperands[0].kind == IRRef::Kind::CONST &&
                         instr.typedOperands[0].value.type == IRType::STRING &&
                         std::get<std::string>(instr.typedOperands[0].value.data) == "import");
        if (isImport) imports.push_back(instr);
        else          body.push_back(instr);
    }

    std::vector<IRInstruction> out;

    // Imports first — outside the restart loop
    for (auto& i : imports) out.push_back(i);

    // _ac_restart_done = false
    {
        IRInstruction st(IROpcode::STORE_VAR);
        st.result = IRRef::var(rdoneId);
        st.typedOperands.push_back(IRRef::constant(IRValue(false)));
        out.push_back(std::move(st));
    }

    // WHILE_BEGIN (infinite loop — exited by /kill, /stop, or the trailing break)
    out.push_back(IRInstruction(IROpcode::WHILE_BEGIN));

    for (auto& instr : body) {
        if (instr.opcode != IROpcode::RESTART_PROGRAM) {
            out.push_back(instr);
            continue;
        }

        // Expand /restart:
        //   t0 = _ac_restart_done
        //   t1 = not t0
        //   IF_BEGIN t1
        //       _ac_restart_done = true
        //       JUMP __continue__
        //   IF_END
        int t0 = ntc++;
        int t1 = ntc++;
        (void)nlc; // labels not needed here — IF_BEGIN/END handle the block structure

        // t0 = _ac_restart_done
        {
            IRInstruction ld(IROpcode::LOAD_VAR);
            ld.result = IRRef::temp(t0);
            ld.typedOperands.push_back(IRRef::var(rdoneId));
            out.push_back(std::move(ld));
        }
        // t1 = NOT t0
        {
            IRInstruction ni(IROpcode::NOT);
            ni.result = IRRef::temp(t1);
            ni.typedOperands.push_back(IRRef::temp(t0));
            out.push_back(std::move(ni));
        }
        // IF_BEGIN t1
        {
            IRInstruction ifB(IROpcode::IF_BEGIN);
            ifB.typedOperands.push_back(IRRef::temp(t1));
            out.push_back(std::move(ifB));
        }
        // _ac_restart_done = true
        {
            IRInstruction st(IROpcode::STORE_VAR);
            st.result = IRRef::var(rdoneId);
            st.typedOperands.push_back(IRRef::constant(IRValue(true)));
            out.push_back(std::move(st));
        }
        // JUMP __continue__
        {
            IRInstruction jc(IROpcode::JUMP);
            jc.typedOperands.push_back(IRRef::constant(IRValue(std::string("__continue__"))));
            out.push_back(std::move(jc));
        }
        // IF_END
        out.push_back(IRInstruction(IROpcode::IF_END));
    }

    // JUMP __break__ — natural end of program breaks out of the restart loop
    {
        IRInstruction jb(IROpcode::JUMP);
        jb.typedOperands.push_back(IRRef::constant(IRValue(std::string("__break__"))));
        out.push_back(std::move(jb));
    }

    // WHILE_END
    out.push_back(IRInstruction(IROpcode::WHILE_END));

    prog.globalInit = std::move(out);
}

IRProgram generateIR(const ASTNode& ast, const std::string& backend, bool runtimeMode) {
    IRGenerator g;
    auto prog = g.generate(ast, backend);
    if (!runtimeMode) runOptPasses(prog);
    else {
        // --runtime: skip constexpr but still run copy-prop/DCE and toxic warnings
        // Toxic warnings (pre-opt, same as normal)
        if (prog.backend != "LIB") {
            std::set<std::string> calledFnsPre;
            std::set<std::string> writtenVarsPre, readVarsPre;
            auto scan = [&](const std::vector<IRInstruction>& instrs) {
                for (const auto& ins : instrs) {
                    if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()) {
                        const auto& r = ins.typedOperands[0];
                        if (r.kind == IRRef::Kind::VAR && r.id >= 0)
                            calledFnsPre.insert(prog.symbols.getName(r.id));
                        // Function names passed as arguments → mark as used
                        for (size_t ai = 1; ai < ins.typedOperands.size(); ++ai) {
                            const auto& arg = ins.typedOperands[ai];
                            if (arg.kind == IRRef::Kind::VAR && arg.id >= 0) {
                                std::string argName = prog.symbols.getName(arg.id);
                                if (prog.findFunction(argName))
                                    calledFnsPre.insert(argName);
                            }
                        }
                    }
                    for (const auto& op : ins.typedOperands)
                        if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                            readVarsPre.insert(prog.symbols.getName(op.id));
                    if (ins.opcode == IROpcode::STORE_VAR) {
                        std::string v;
                        if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                            v = prog.symbols.getName(ins.typedOperands[0].id);
                        else if (ins.result.isValid() && ins.result.kind == IRRef::Kind::VAR)
                            v = prog.symbols.getName(ins.result.id);
                        if (!v.empty() && v[0] != '_') writtenVarsPre.insert(v);
                    }
                }
            };
            scan(prog.globalInit);
            for (const auto& fn : prog.functions) scan(fn.instructions);
            for (auto it = writtenVarsPre.begin(); it != writtenVarsPre.end(); )
                if (readVarsPre.count(*it)) it = writtenVarsPre.erase(it); else ++it;
            for (const auto& v : writtenVarsPre)
                std::cerr << "Toxic: Compiler is slacking off — '" << v << "' assigned but never read\n";
            for (const auto& fn : prog.functions) {
                if (fn.name.empty() || fn.name[0] == '_') continue;
                if (fn.name == "main" || fn.name == "mainloop") continue;
                if (!calledFnsPre.count(fn.name))
                    std::cerr << "Toxic: Compiler is slacking off — '" << fn.name << "' defined but never called\n";
            }
        }
        for (auto& fn : prog.functions) { runCopyProp(fn.instructions); runDCE(fn.instructions); }
        runCopyProp(prog.globalInit); runDCE(prog.globalInit);
    }
    injectAutoShutoff(prog);
    expandAliases(prog);
    if (prog.hasRestart)
        wrapWithRestartLoop(prog);
    return prog;
}

// ─── LIR text serialiser ────────────────────────────────────────────────────

static std::string refStr(const IRRef& r, SymbolTable* symbols = nullptr) {
    if (symbols) {
        return r.toStringWithSymbols(symbols);
    }
    return r.toString();
}

static std::string instrToLIR(const IRInstruction& i, SymbolTable* symbols = nullptr) {
    std::ostringstream o;

    if (i.result.isValid())
        o << refStr(i.result, symbols) << " = ";

    o << opcodeStr(i.opcode);

    if (!i.typedOperands.empty()) {
        o << ' ';
        for (size_t k = 0; k < i.typedOperands.size(); k++) {
            if (k) o << ", ";
            o << refStr(i.typedOperands[k], symbols);
        }
    } else if (!i.operands.empty()) {
        // legacy string operands fallback
        o << ' ';
        for (size_t k = 0; k < i.operands.size(); k++) {
            if (k) o << ", ";
            o << i.operands[k];
        }
    }

    if (!i.comment.empty()) o << "  ; " << i.comment;
    return o.str();
}

std::string generateIRText(const IRProgram& program) {
    std::ostringstream o;
    o << "; AC LIR  backend=" << program.backend << "\n";
    o << "; Symbol Table: " << program.symbols.size() << " symbols\n";
    o << "; Arena: " << program.arena.totalUsed() << " / " << program.arena.totalAllocated() 
      << " bytes used (" << program.arena.numBlocks() << " blocks)\n\n";

    if (!program.globalInit.empty()) {
        o << "section .global:\n";
        for (auto& i : program.globalInit)
            o << "  " << instrToLIR(i, const_cast<SymbolTable*>(&program.symbols)) << '\n';
        o << '\n';
    }

    for (auto& fn : program.functions) {
        o << "fn " << fn.name << '(';
        for (size_t k = 0; k < fn.parameters.size(); k++) {
            if (k) o << ", ";
            o << fn.parameters[k];
        }
        o << ") -> " << typeStr(fn.returnType) << " {\n";
        for (auto& i : fn.instructions)
            o << "  " << instrToLIR(i, const_cast<SymbolTable*>(&program.symbols)) << '\n';
        o << "}\n\n";
    }

    return o.str();
}

} // namespace AC_IR
