#include "../include/ir.hpp"
#include "../include/ast.hpp"
#include <sstream>
#include <stack>

namespace AC_IR {

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
    static IRRef mkVar(const std::string& name) {
        return IRRef::var(name);
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
            IRFunction fn(n.value);
            fn.returnType = IRType::VOID;
            for (auto& p : n.attrs) fn.parameters.push_back(p);
            prog.functions.push_back(std::move(fn));
            cur = &prog.functions.back();

            // function entry label
            IRInstruction entry(IROpcode::FUNC_BEGIN);
            entry.typedOperands = {mkVar(n.value)};
            emit(std::move(entry));

            if (!n.children.empty()) gen(*n.children[0]);

            // implicit void return if no explicit return
            IRInstruction ret(IROpcode::RETURN);
            ret.typedOperands = {};
            emit(std::move(ret));

            IRInstruction end(IROpcode::FUNC_END);
            end.typedOperands = {mkVar(n.value)};
            emit(std::move(end));

            cur = nullptr;
            break;
        }

        // ── assignment ──────────────────────────────────────────────────────
        case NodeType::AssignStmt: {
            if (n.attrs.empty()) break;
            const std::string& raw = n.attrs[0];
            IRRef dst = mkVar(n.value);

            if (raw.substr(0, 8) == "__list__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("list"), mkConst(raw.substr(8))});
                emit(std::move(i));
            } else if (raw.substr(0, 9) == "__tuple__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("tuple"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw.substr(0, 8) == "__dict__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("dict"), mkConst(raw.substr(8))});
                emit(std::move(i));
            } else if (raw.substr(0, 9) == "__range__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("range"), mkConst(raw.substr(9))});
                emit(std::move(i));
            } else if (raw.substr(0, 12) == "__sequence__") {
                IRInstruction i(IROpcode::ALLOC, dst, {mkConst("sequence"), mkConst(raw.substr(12))});
                emit(std::move(i));
            } else if (raw.substr(0, 6) == "__fn__") {
                IRRef src = lowerExpr(raw.substr(6));
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            } else if (raw.substr(0, 11) == "__funcall__") {
                // function call result stored in variable
                if (!n.children.empty() && n.children[0]->type == NodeType::FunctionCall) {
                    auto& fc = *n.children[0];
                    std::vector<IRRef> ops = {mkVar(fc.value)};
                    for (auto& a : fc.attrs) ops.push_back(mkConst(a));
                    IRInstruction i(IROpcode::CALL, dst, ops);
                    emit(std::move(i));
                }
            } else {
                IRRef src = lowerExpr(raw);
                IRInstruction i(IROpcode::STORE_VAR, dst, {src});
                emit(std::move(i));
            }
            break;
        }

        case NodeType::PlusEqualStmt:
            if (!n.attrs.empty()) emitCompound(IROpcode::ADD, n.value, n.attrs[0]);
            break;
        case NodeType::MinusEqualStmt:
            if (!n.attrs.empty()) emitCompound(IROpcode::SUB, n.value, n.attrs[0]);
            break;
        case NodeType::MultiplyEqualStmt:
        case NodeType::AtEqualStmt:
            if (!n.attrs.empty()) emitCompound(IROpcode::MUL, n.value, n.attrs[0]);
            break;
        case NodeType::DivideEqualStmt:
            if (!n.attrs.empty()) emitCompound(IROpcode::DIV, n.value, n.attrs[0]);
            break;

        // ── display / print ─────────────────────────────────────────────────
        case NodeType::DisplayStmt: {
            IRRef val = lowerExpr(n.value);
            IRInstruction i(IROpcode::PRINT);
            i.typedOperands = {val};
            emit(std::move(i));
            break;
        }

        // ── method / function calls ─────────────────────────────────────────
        case NodeType::MethodCall: {
            std::vector<IRRef> ops = {mkVar(n.value)};
            for (auto& a : n.attrs) ops.push_back(mkConst(a));
            IRInstruction i(IROpcode::LIB_CALL);
            i.typedOperands = ops;
            emit(std::move(i));
            break;
        }

        case NodeType::FunctionCall: {
            std::vector<IRRef> ops = {mkVar(n.value)};
            for (auto& a : n.attrs) ops.push_back(mkConst(a));
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
            for (auto& a : n.attrs) ops.push_back(mkConst(a));
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
                if (!n.children.empty()) gen(*n.children[0]);
                break;
            }

            IRRef condRef = lowerExpr(n.value);
            IRRef elseL   = mkLabel();
            IRRef endL    = mkLabel();

            emitJF(condRef, elseL);
            if (!n.children.empty()) gen(*n.children[0]);

            bool hasElse = n.children.size() > 1;
            if (hasElse) emitJump(endL);
            emitLabel(elseL);

            for (size_t i = 1; i < n.children.size(); i++) gen(*n.children[i]);
            if (hasElse) emitLabel(endL);
            break;
        }

        case NodeType::ElseIfStmt: {
            IRRef condRef = lowerExpr(n.value);
            IRRef skipL   = mkLabel();

            emitJF(condRef, skipL);
            if (!n.children.empty()) gen(*n.children[0]);
            emitLabel(skipL);
            break;
        }

        case NodeType::WhilstLoop: {
            IRRef startL = mkLabel();
            IRRef endL   = mkLabel();

            loopStart.push(startL);
            loopEnd.push(endL);

            emitLabel(startL);
            IRRef condRef = lowerExpr(n.value);
            emitJF(condRef, endL);

            if (!n.children.empty()) gen(*n.children[0]);

            // elseif/other chains after the loop body
            for (size_t i = 1; i < n.children.size(); i++) gen(*n.children[i]);

            emitJump(startL);
            emitLabel(endL);

            loopStart.pop();
            loopEnd.pop();
            break;
        }

        case NodeType::ForLoop: {
            std::string iterVar  = n.value;
            std::string coll     = n.attrs.empty() ? "" : n.attrs[0];

            IRRef startL = mkLabel();
            IRRef endL   = mkLabel();
            IRRef iterT  = mkTemp();  // iterator temp
            IRRef itemT  = mkTemp();  // current item temp

            loopStart.push(startL);
            loopEnd.push(endL);

            // iter_init: iterT = iter(coll)
            IRInstruction init(IROpcode::LOAD_VAR, iterT, {mkConst(coll)});
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

            if (!n.children.empty()) gen(*n.children[0]);

            emitJump(startL);
            emitLabel(endL);

            loopStart.pop();
            loopEnd.pop();
            break;
        }

        case NodeType::ReturnStmt: {
            IRRef val = lowerExpr(n.value);
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

static std::string refStr(const IRRef& r) {
    return r.toString();
}

static std::string instrToLIR(const IRInstruction& i) {
    std::ostringstream o;

    if (i.result.isValid())
        o << refStr(i.result) << " = ";

    o << opcodeStr(i.opcode);

    if (!i.typedOperands.empty()) {
        o << ' ';
        for (size_t k = 0; k < i.typedOperands.size(); k++) {
            if (k) o << ", ";
            o << refStr(i.typedOperands[k]);
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
    o << "; AC LIR  backend=" << program.backend << "\n\n";

    if (!program.globalInit.empty()) {
        o << "section .global:\n";
        for (auto& i : program.globalInit)
            o << "  " << instrToLIR(i) << '\n';
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
            o << "  " << instrToLIR(i) << '\n';
        o << "}\n\n";
    }

    return o.str();
}

} // namespace AC_IR
