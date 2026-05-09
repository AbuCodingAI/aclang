#include "../include/ir.hpp"
#include "../include/ast.hpp"
#include <sstream>
#include <stack>

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
        default:                      return "???";
    }
}

// ─── generator ──────────────────────────────────────────────────────────────

class IRGenerator {
    IRProgram        prog;
    IRFunction*      cur  = nullptr;   // current function (null = global)
    int              tc   = 0;         // global temp counter
    int              lc   = 0;         // global label counter

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

    // ── emit helpers ────────────────────────────────────────────────────────

    void emit(IRInstruction i) {
        if (cur) cur->instructions.push_back(std::move(i));
        else     prog.globalInit.push_back(std::move(i));
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
                        return mkConst(expr.value);
                    } else if (typeStr == "STRING") {
                        return mkConst(expr.value);
                    } else if (typeStr == "BOOL") {
                        bool val = (expr.value == "true");
                        return IRRef::constant(IRValue(val));
                    }
                }
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
                    else if (op == "==") opcode = IROpcode::EQ;
                    else if (op == "#=" || op == "!=") opcode = IROpcode::NEQ;
                    else if (op == "<") opcode = IROpcode::LT;
                    else if (op == ">") opcode = IROpcode::GT;
                    else if (op == "<=") opcode = IROpcode::LTE;
                    else if (op == ">=") opcode = IROpcode::GTE;
                    else if (op == "AND" || op == "and") opcode = IROpcode::AND;
                    else if (op == "OR" || op == "or") opcode = IROpcode::OR;
                    
                    if (opcode != IROpcode::NOP) {
                        IRInstruction i(opcode, dst, {lRef, rRef});
                        emit(std::move(i));
                        return dst;
                    }
                }
                // Fallback: treat as string expression (legacy compatibility)
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
                    } else if (expr.value == "NOT") {
                        IRInstruction i(IROpcode::NOT, dst, {operand});
                        emit(std::move(i));
                        return dst;
                    }
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
                // Array indexing: array[index]
                if (expr.children.size() >= 2) {
                    IRRef arr = lowerExprNode(*expr.children[0]);
                    IRRef idx = lowerExprNode(*expr.children[1]);
                    IRRef dst = mkTemp();
                    IRInstruction i(IROpcode::LOAD_INDEX, dst, {arr, idx});
                    emit(std::move(i));
                    return dst;
                }
                // Fallback: use variable name
                return mkVar(expr.value);
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
        for (char c : expr) if (!std::isdigit(c) && c != '-') { isInt = false; break; }
        if (isInt) {
            try { return mkConstInt(std::stoi(expr)); } catch (...) {}
        }

        // boolean literals
        if (expr == "true")  return IRRef::constant(IRValue(true));
        if (expr == "false") return IRRef::constant(IRValue(false));

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

    // ── node visitor ────────────────────────────────────────────────────────

    void gen(const ASTNode& n) {
        switch (n.type) {

        case NodeType::Program:
        case NodeType::Block:
        case NodeType::TagBlock:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::BackendDecl:
        case NodeType::SaveStmt:
            break; // metadata only

        // ── function definition ─────────────────────────────────────────────
        case NodeType::FuncDef: {
            // Intern function name in symbol table
            int funcId = prog.symbols.intern(n.value, IRType::FUNCTION);
            
            IRFunction fn(n.value);
            fn.returnType = IRType::VOID;
            
            // Enter function scope
            prog.symbols.enterScope();
            
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

            // Exit function scope
            prog.symbols.exitScope();
            
            cur = nullptr;
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
                            for (char c : a) if (!std::isdigit(c) && c != '-') { isInt = false; break; }
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
            std::vector<IRRef> ops = {mkVar(n.value)};
            for (auto& a : n.attrs) {
                // Trim whitespace
                std::string trimmed = a;
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
                
                // Check if attr is a variable name or a constant
                // If it's a number, make it a const int
                bool isInt = !trimmed.empty();
                for (char c : trimmed) if (!std::isdigit(c) && c != '-') { isInt = false; break; }
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
                for (char c : a) if (!std::isdigit(c) && c != '-') { isInt = false; break; }
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
                for (char c : a) if (!std::isdigit(c) && c != '-') { isInt = false; break; }
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
            IRRef condRef;
            size_t bodyIndex = 0;
            if (!n.children.empty() && n.children[0]->type != NodeType::Block) {
                // Structured expression as first child
                condRef = lowerExprNode(*n.children[0]);
                bodyIndex = 1;
            } else {
                // Legacy string-based condition (fallback)
                condRef = lowerExpr(n.value);
                bodyIndex = 0;
            }
            
            if (prog.useHighLevelIR) {
                // High-level IR: emit IF_BEGIN with condition
                IRInstruction ifBegin(IROpcode::IF_BEGIN);
                ifBegin.typedOperands = {condRef};
                emit(std::move(ifBegin));
                
                if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
                
                bool hasElse = n.children.size() > bodyIndex + 1;
                if (hasElse) {
                    // Emit IF_ELSE
                    IRInstruction elseInstr(IROpcode::IF_ELSE);
                    emit(std::move(elseInstr));
                    for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);
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
            IRRef condRef;
            size_t bodyIndex = 0;
            if (!n.children.empty() && n.children[0]->type != NodeType::Block) {
                // Structured expression as first child
                condRef = lowerExprNode(*n.children[0]);
                bodyIndex = 1;
            } else {
                // Legacy string-based condition (fallback)
                condRef = lowerExpr(n.value);
                bodyIndex = 0;
            }
            
            IRRef skipL   = mkLabel();

            emitJF(condRef, skipL);
            if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);
            emitLabel(skipL);
            break;
        }

        case NodeType::WhilstLoop: {
            IRRef startL = mkLabel();
            IRRef endL   = mkLabel();

            loopStart.push(startL);
            loopEnd.push(endL);

            emitLabel(startL);
            
            // First child is condition expression, second is body
            IRRef condRef;
            size_t bodyIndex = 0;
            if (!n.children.empty() && n.children[0]->type != NodeType::Block) {
                // Structured expression as first child
                condRef = lowerExprNode(*n.children[0]);
                bodyIndex = 1;
            } else {
                // Legacy string-based condition (fallback)
                condRef = lowerExpr(n.value);
                bodyIndex = 0;
            }
            
            emitJF(condRef, endL);

            if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

            // elseif/other chains after the loop body
            for (size_t i = bodyIndex + 1; i < n.children.size(); i++) gen(*n.children[i]);

            emitJump(startL);
            emitLabel(endL);

            loopStart.pop();
            loopEnd.pop();
            break;
        }

        case NodeType::ForLoop: {
            std::string iterVar  = n.value;
            
            IRRef startL = mkLabel();
            IRRef endL   = mkLabel();
            IRRef iterT  = mkTemp();  // iterator temp
            IRRef itemT  = mkTemp();  // current item temp

            loopStart.push(startL);
            loopEnd.push(endL);

            // First child is collection expression, second is body
            IRRef collRef;
            size_t bodyIndex = 0;
            if (!n.children.empty() && n.children[0]->type != NodeType::Block) {
                // Structured expression as first child
                collRef = lowerExprNode(*n.children[0]);
                bodyIndex = 1;
            } else {
                // Legacy string-based collection (fallback)
                std::string coll = n.attrs.empty() ? "" : n.attrs[0];
                collRef = lowerExpr(coll);
                bodyIndex = 0;
            }

            // iter_init: iterT = iter(coll)
            IRInstruction init(IROpcode::LOAD_VAR, iterT, {collRef});
            emit(std::move(init));

            emitLabel(startL);

            // iter_has_next: jf iterT, endL
            emitJF(iterT, endL);

            // iter_next: itemT = next(iterT)
            IRInstruction next(IROpcode::LOAD_INDEX, itemT, {iterT, mkConst("__next__")});
            emit(std::move(next));

            // store into loop variable
            IRInstruction stv(IROpcode::STORE_VAR, mkVar(iterVar), {itemT});
            emit(std::move(stv));

            if (bodyIndex < n.children.size()) gen(*n.children[bodyIndex]);

            emitJump(startL);
            emitLabel(endL);

            loopStart.pop();
            loopEnd.pop();
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
                // store: arr[idx] = val
                IRRef arr = mkVar(n.value);
                IRRef idx = lowerExpr(n.attrs[0]);
                IRRef val = lowerExpr(n.attrs[1]);
                IRInstruction i(IROpcode::STORE_INDEX);
                i.typedOperands = {arr, idx, val};
                emit(std::move(i));
            } else if (n.attrs.size() == 1) {
                // load: tmp = arr[idx]
                IRRef arr = mkVar(n.value);
                IRRef idx = lowerExpr(n.attrs[0]);
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
            lowerExpr(n.value); // side-effect: emits arithmetic if compound
            break;
        }

        // ── events ──────────────────────────────────────────────────────────
        case NodeType::EventListener:
            for (auto& c : n.children) gen(*c);
            break;

        case NodeType::KeyBinding: {
            IRInstruction i(IROpcode::EVENT_BIND);
            i.typedOperands = {mkConst(n.value),
                               mkConst(n.attrs.empty() ? "" : n.attrs[0])};
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
            i.typedOperands = {mkConst("import"), mkConst(n.value)};
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
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = {mkConst("foreign"), mkConst(n.value)};
            emit(std::move(i));
            break;
        }

        case NodeType::CustomTagDef:
        case NodeType::WhenBlock:
        case NodeType::SpawnStmt: {
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = {mkConst(n.value)};
            emit(std::move(i));
            for (auto& c : n.children) gen(*c);
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
        prog.useHighLevelIR = (backend == "PY" || backend == "JS" || backend == "Java");
        tc = lc = 0;
        gen(ast);
        return std::move(prog);
    }
};

// ─── public API ─────────────────────────────────────────────────────────────

IRProgram generateIR(const ASTNode& ast, const std::string& backend) {
    IRGenerator g;
    return g.generate(ast, backend);
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
