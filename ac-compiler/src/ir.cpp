#include "../include/ac.hpp"
#include <sstream>
#include <stack>
#include <cctype>
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
        case IROpcode::DIV:           return "div";
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
    std::string      currentClass_;    // non-empty while inside a bundle body

    // loop label stacks for break/continue
    std::stack<IRRef> loopStart;
    std::stack<IRRef> loopEnd;

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
    static IRRef mkConstInt(int v) {
        return IRRef::constant(IRValue(v));
    }

    // Convert 1-based index (AC) to 0-based (target languages)
    IRRef adjustIndex(const IRRef& idx) {
        if (idx.kind == IRRef::Kind::CONST && idx.value.type == IRType::INT) {
            int n = std::get<int>(idx.value.data);
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

    void emit(IRInstruction i) {
        if (cur) {
            cur->instructions.push_back(std::move(i));
        } else {
            // Route to the appropriate section (data vs main)
            if (inMainSection) {
                prog.mainSection.push_back(i);
            } else {
                prog.dataSection.push_back(i);
            }
            prog.globalInit.push_back(std::move(i));
        }
    }

    // emit a LABEL definition
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
                            return mkConstInt(std::stoi(expr.value));
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
                    }
                }
                // null literal as bare string
                if (expr.value == "null") return mkConst("null");
                return mkConst(expr.value);
            }
            
            case NodeType::Identifier: {
                // Variable reference
                return mkVar(expr.value);
            }
            
            case NodeType::BinaryExpr: {
                // Binary operation: left op right
                if (expr.children.size() >= 2) {
                    IRRef lRef = lowerExprNode(*expr.children[0]);
                    IRRef rRef = lowerExprNode(*expr.children[1]);
                    IRRef dst = mkTemp();

                    // Map operator string to IROpcode
                    IROpcode opcode = IROpcode::NOP;
                    const std::string& op = expr.value;
                    if (op == "+") opcode = IROpcode::ADD;
                    else if (op == "-") opcode = IROpcode::SUB;
                    else if (op == "*" || op == "@") opcode = IROpcode::MUL;
                    else if (op == "/") opcode = IROpcode::DIV;
                    else if (op == "%") opcode = IROpcode::MOD;
                    else if (op == "==" || op == "is") opcode = IROpcode::EQ;
                    else if (op == "#=" || op == "!=" || op == "is not") opcode = IROpcode::NEQ;
                    else if (op == "<") opcode = IROpcode::LT;
                    else if (op == ">") opcode = IROpcode::GT;
                    else if (op == "#>") opcode = IROpcode::LTE;   // NOT greater = ≤
                    else if (op == "#<") opcode = IROpcode::GTE;   // NOT less = ≥
                    else if (op == "AND" || op == "and" || op == "&") opcode = IROpcode::AND;
                    else if (op == "OR" || op == "or") opcode = IROpcode::OR;
                    else if (op == "|")    opcode = IROpcode::XOR;
                    else if (op == "#|")   opcode = IROpcode::XNOR;
                    else if (op == "xsub") opcode = IROpcode::XSUB;

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
                                if (v.type == IRType::INT)   return (double)std::get<int>(v.data);
                                return 0.0;
                            };
                            auto asInt = [](const IRValue& v) -> int {
                                if (v.type == IRType::INT)   return std::get<int>(v.data);
                                if (v.type == IRType::FLOAT) return (int)std::get<double>(v.data);
                                if (v.type == IRType::BOOL)  return std::get<bool>(v.data) ? 1 : 0;
                                return 0;
                            };
                            auto asBool = [&](const IRValue& v) -> bool { return asInt(v) != 0; };
                            IRRef folded;
                            bool did_fold = true;
                            switch (opcode) {
                                case IROpcode::ADD:  folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)+asDouble(R))) : IRRef::constant(IRValue(asInt(L)+asInt(R))); break;
                                case IROpcode::SUB:  folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)-asDouble(R))) : IRRef::constant(IRValue(asInt(L)-asInt(R))); break;
                                case IROpcode::MUL:  folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)*asDouble(R))) : IRRef::constant(IRValue(asInt(L)*asInt(R))); break;
                                case IROpcode::DIV:
                                    if (asInt(R) != 0 || asDouble(R) != 0.0)
                                        folded = anyFloat ? IRRef::constant(IRValue(asDouble(L)/asDouble(R))) : IRRef::constant(IRValue(asInt(L)/asInt(R)));
                                    else did_fold = false;
                                    break;
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
                                case IROpcode::XSUB: { int d = asInt(L)-asInt(R); folded = IRRef::constant(IRValue((d<0?-d:d)+1)); break; }
                                default: did_fold = false; break;
                            }
                            if (did_fold) return folded;
                        }
                        IRInstruction i(opcode, dst, {lRef, rRef});
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
                if (!expr.children.empty()) {
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
            
            case NodeType::CallExpr: {
                // Function call: func(args)
                IRRef dst = mkTemp();
                std::vector<IRRef> ops = {mkVar(expr.value)};
                
                // Add arguments
                for (auto& arg : expr.children) {
                    ops.push_back(lowerExprNode(*arg));
                }
                
                IRInstruction i(IROpcode::CALL, dst, ops);
                emit(std::move(i));
                return dst;
            }
            
            case NodeType::IndexExpr: {
                // Array indexing: array[index]  (AC uses 1-based, targets use 0-based)
                if (expr.children.size() >= 2) {
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
                // No-arg property/constant reference: self.prop or lib.CONST
                if (expr.children.empty()) {
                    auto dot = mname.find('.');
                    if (dot != std::string::npos) {
                        std::string obj = mname.substr(0, dot);
                        // self.X → property access, keep dot in symbol name
                        if (obj == "self") return mkVar(mname);
                        // lib.CONST → lib_CONST  (e.g. math.PI → math_PI)
                        std::string varName = mname;
                        for (char& c : varName) if (c == '.') c = '_';
                        return mkVar(varName);
                    }
                    return mkVar(mname);
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
                    ? mkConst("\"\"")
                    : lowerExprNode(*expr.children[0]);
                IRInstruction i(IROpcode::EVAL, dst, {arg});
                emit(std::move(i));
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
            try { return mkConstInt(std::stoi(expr)); } catch (...) {}
        }

        // boolean / null literals
        if (expr == "true")  return IRRef::constant(IRValue(true));
        if (expr == "false") return IRRef::constant(IRValue(false));
        if (expr == "null")  return mkConst("null");

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
        // if no +/-, scan for */ (higher precedence)
        if (opPos == -1) {
            depth = 0;
            for (int i = (int)expr.size() - 1; i >= 0; --i) {
                char c = expr[i];
                if (c == ')') depth++;
                else if (c == '(') depth--;
                else if (depth == 0 && (c == '*' || c == '/') && i > 0) {
                    opPos = i;
                    opcode = (c == '*') ? IROpcode::MUL : IROpcode::DIV;
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
            // Skip layout/logic-only tags that have no runtime meaning
            if (tag == "LOGIC" || tag == "gui" || tag == "OBJECT" || tag == "SCREEN") break;
            // Local: transparent pass-through
            if (tag == "Local") { for (auto& c : n.children) gen(*c); break; }
            // StartHere: wrap body in an infinite loop
            if (tag == "StartHere") {
                if (prog.useHighLevelIR) {
                    IRRef trueRef = IRRef::constant(IRValue(true));
                    IRInstruction wb(IROpcode::WHILE_BEGIN); wb.typedOperands = {trueRef}; emit(std::move(wb));
                    for (auto& c : n.children) gen(*c);
                    emit(IRInstruction(IROpcode::WHILE_END));
                } else {
                    IRRef loopL = mkLabel();
                    emitLabel(loopL);
                    for (auto& c : n.children) gen(*c);
                    emitJump(loopL);
                }
                break;
            }
            // mainloop: switch to main section and walk children
            if (tag == "mainloop") {
                inMainSection = true;
                for (auto& c : n.children) gen(*c);
                break;
            }
            // Default: walk children
            for (auto& c : n.children) gen(*c);
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

            // Methods implicitly receive 'self' as first parameter
            if (!currentClass_.empty()) {
                prog.symbols.intern("self");
                fn.parameters.push_back("self");
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

            // Lower body inside class context
            currentClass_ = className;
            if (!n.children.empty()) gen(*n.children[0]);
            currentClass_ = "";

            // CLASS_END marker
            IRInstruction ce(IROpcode::CLASS_END);
            ce.typedOperands = {mkConst(className)};
            emit(std::move(ce));
            break;
        }

        // ── assignment ──────────────────────────────────────────────────────
        case NodeType::AssignStmt: {
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
                // Structured expression as child
                IRRef src = lowerExprNode(*n.children[0]);
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
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
            // Check for display-style methods where arg is a structured child expression
            const std::string& mname = n.value;
            auto endsWith = [](const std::string& s, const std::string& suf) {
                return s.size() >= suf.size() && s.substr(s.size()-suf.size()) == suf;
            };
            if (!n.children.empty() && (endsWith(mname,"display") || endsWith(mname,"print") || endsWith(mname,"log"))) {
                IRRef val = lowerExprNode(*n.children[0]);
                IRInstruction i(IROpcode::LIB_CALL);
                i.typedOperands = {mkVar(n.value), val};
                emit(std::move(i));
                break;
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
            std::vector<IRRef> ops = {mkVar(n.value)};
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
            // name.prop = value
            IRRef dst = mkVar(n.value);
            IRRef src = n.attrs.empty() ? mkConst("") : lowerExpr(n.attrs[0]);
            IRInstruction i(IROpcode::STORE_VAR, dst, {src});
            emit(std::move(i));
            break;
        }

        case NodeType::ConfigCall: {
            std::vector<IRRef> ops = {mkVar(n.value)};
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
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::ObjDecl: {
            IRRef dst = mkVar(n.value);
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("object")});
            emit(std::move(i));
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

            // Resolve collection and body index
            if (n.children.empty() || n.children[0]->type == NodeType::Block) break;
            IRRef collRef = lowerExprNode(*n.children[0]);
            size_t bodyIndex = 1;

            if (prog.useHighLevelIR) {
                // High-level backends: emit FOR_BEGIN {iterVarName, collection} … FOR_END
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
                // Low-level backends (C, ASM, BNY): labels + jumps
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
            IRInstruction i(IROpcode::HALT);
            emit(std::move(i));
            break;
        }

        case NodeType::PassStmt: {
            IRInstruction i(IROpcode::NOP);
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
            IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(n.value)});
            emit(std::move(i));
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
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = {mkConst("raise"), mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        case NodeType::ForeignBlock: {
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
            IRInstruction entry(IROpcode::FUNC_BEGIN); entry.typedOperands = {IRRef::func(funcId)}; emit(std::move(entry));
            if (!n.children.empty()) gen(*n.children[0]);
            IRInstruction ret(IROpcode::RETURN); ret.typedOperands = {}; emit(std::move(ret));
            IRInstruction end(IROpcode::FUNC_END); end.typedOperands = {IRRef::func(funcId)}; emit(std::move(end));
            cur = nullptr;
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
        // Use high-level IR for languages without goto
        // High-level IR: proper if/else blocks + while-true/break (no goto)
        // Used for backends that lack goto: PY, JS, Java, C++, RS, V
        // High-level IR: proper if/else blocks + while-true/break (no goto)
        prog.useHighLevelIR = (backend == "PY" || backend == "JS" || backend == "HTML" ||
                               backend == "Java" || backend == "C++" || backend == "CPP" ||
                               backend == "RS" || backend == "V" || backend == "GO");
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

static void runOptPasses(IRProgram& prog) {
    for (auto& fn : prog.functions) {
        runCopyProp(fn.instructions);
        runDCE(fn.instructions);
    }
    runCopyProp(prog.globalInit);
    runDCE(prog.globalInit);
}

// ─── public API ─────────────────────────────────────────────────────────────

IRProgram generateIR(const ASTNode& ast, const std::string& backend) {
    IRGenerator g;
    auto prog = g.generate(ast, backend);
    runOptPasses(prog);
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
