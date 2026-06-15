// ir.cpp — AC* low-level IR lowering + printer
#include "ir.hpp"
#include <sstream>

namespace aca {

namespace {

struct IRBuilder {
    IRProgram out;
    std::vector<IRInst>* cur = nullptr;
    int tmp = 0;
    int lbl = 0;

    int newTemp() { return tmp++; }
    int newLabel() { return lbl++; }

    IRValue emitConst(LitValue lit, int line, int col) {
        IRConst c;
        c.type = lit.t;
        c.v = lit.v;
        int t = newTemp();
        IRInst i;
        i.op = IROp::CONST;
        i.dst = IRValue::temp(t, lit.t);
        i.ops = {IRValue::constant(std::move(c))};
        i.line = line;
        i.col = col;
        cur->push_back(std::move(i));
        return IRValue::temp(t, lit.t);
    }

    IRValue emitLoadVar(const std::string& name, Type t, int line, int col) {
        int id = newTemp();
        IRInst i;
        i.op = IROp::LOAD;
        i.dst = IRValue::temp(id, t);
        i.ops = {IRValue::var(name, t)};
        i.line = line;
        i.col = col;
        cur->push_back(std::move(i));
        return IRValue::temp(id, t);
    }

    void emitStoreVar(const std::string& name, IRValue v, int line, int col) {
        IRInst i;
        i.op = IROp::STORE;
        i.dst = IRValue::var(name, v.type);
        i.ops = {v};
        i.line = line;
        i.col = col;
        cur->push_back(std::move(i));
    }

    IRValue lowerExpr(const Expr& e) {
        switch (e.kind) {
            case ExprKind::Literal:
                return emitConst(std::get<LitValue>(e.data), e.line, e.col);
            case ExprKind::Var:
                return emitLoadVar(std::get<std::string>(e.data), e.inferred, e.line, e.col);
            case ExprKind::Binary: {
                const auto& t = std::get<std::tuple<std::string, ExprPtr, ExprPtr>>(e.data);
                IRValue a = lowerExpr(*std::get<1>(t));
                IRValue b = lowerExpr(*std::get<2>(t));
                int id = newTemp();
                IRInst i;
                i.op = IROp::BINOP;
                i.dst = IRValue::temp(id, e.inferred);
                i.aux = std::get<0>(t);
                i.ops = {a, b};
                i.line = e.line;
                i.col = e.col;
                cur->push_back(std::move(i));
                return IRValue::temp(id, e.inferred);
            }
            case ExprKind::Call: {
                const auto& t = std::get<std::tuple<std::string, std::vector<ExprPtr>>>(e.data);
                const std::string& name = std::get<0>(t);
                const auto& args = std::get<1>(t);
                std::vector<IRValue> lowered;
                lowered.reserve(args.size());
                for (auto& a : args) lowered.push_back(lowerExpr(*a));
                int id = newTemp();
                IRInst i;
                i.op = IROp::CALL;
                i.dst = IRValue::temp(id, e.inferred);
                i.aux = name;
                i.ops = std::move(lowered);
                i.line = e.line;
                i.col = e.col;
                cur->push_back(std::move(i));
                return IRValue::temp(id, e.inferred);
            }
            case ExprKind::List: {
                const auto& items = std::get<std::vector<ExprPtr>>(e.data);
                std::vector<IRValue> lowered;
                lowered.reserve(items.size());
                for (auto& it : items) lowered.push_back(lowerExpr(*it));
                int id = newTemp();
                IRInst i;
                i.op = IROp::CALL;
                i.dst = IRValue::temp(id, e.inferred);
                i.aux = "__list__";
                i.ops = std::move(lowered);
                i.line = e.line;
                i.col = e.col;
                cur->push_back(std::move(i));
                return IRValue::temp(id, e.inferred);
            }
            case ExprKind::Range: {
                const auto& pr = std::get<std::pair<ExprPtr, ExprPtr>>(e.data);
                IRValue a = lowerExpr(*pr.first);
                IRValue b = lowerExpr(*pr.second);
                int id = newTemp();
                IRInst i;
                i.op = IROp::CALL;
                i.dst = IRValue::temp(id, e.inferred);
                i.aux = "__range__";
                i.ops = {a, b};
                i.line = e.line;
                i.col = e.col;
                cur->push_back(std::move(i));
                return IRValue::temp(id, e.inferred);
            }
        }
        return IRValue::none();
    }

    void lowerStmt(const Stmt& s) {
        switch (s.kind) {
            case StmtKind::Assign: {
                const auto& a = std::get<AssignPayload>(s.data);
                IRValue rhs = lowerExpr(*a.expr);
                if (a.op == "=") {
                    emitStoreVar(a.name, rhs, s.line, s.col);
                    return;
                }
                std::string bop = "+";
                if (a.op == "+=") bop = "+";
                else if (a.op == "-=") bop = "-";
                else if (a.op == "@=") bop = "@";
                IRValue lhs = emitLoadVar(a.name, rhs.type, s.line, s.col);
                int id = newTemp();
                IRInst i;
                i.op = IROp::BINOP;
                i.dst = IRValue::temp(id, rhs.type);
                i.aux = bop;
                i.ops = {lhs, rhs};
                i.line = s.line;
                i.col = s.col;
                cur->push_back(std::move(i));
                emitStoreVar(a.name, IRValue::temp(id, rhs.type), s.line, s.col);
                return;
            }
            case StmtKind::ExprOnly: {
                const auto& e = std::get<ExprPtr>(s.data);
                (void)lowerExpr(*e);
                return;
            }
            case StmtKind::Return: {
                const auto& r = std::get<ReturnPayload>(s.data);
                IRValue v = lowerExpr(*r.expr);
                IRInst i;
                i.op = IROp::RETURN;
                i.ops = {v};
                i.line = s.line;
                i.col = s.col;
                cur->push_back(std::move(i));
                return;
            }
            case StmtKind::If: {
                const auto& iff = std::get<IfPayload>(s.data);
                IRValue cond = lowerExpr(*iff.cond);
                int lThen = newLabel();
                int lElse = newLabel();
                int lEnd = newLabel();

                IRInst br;
                br.op = IROp::BR;
                br.ops = {cond, IRValue::label(lThen), IRValue::label(lElse)};
                br.line = s.line;
                br.col = s.col;
                cur->push_back(std::move(br));

                IRInst labThen;
                labThen.op = IROp::LABEL;
                labThen.dst = IRValue::label(lThen);
                cur->push_back(std::move(labThen));
                for (auto& st : iff.thenBody) lowerStmt(*st);
                IRInst j1;
                j1.op = IROp::JUMP;
                j1.ops = {IRValue::label(lEnd)};
                cur->push_back(std::move(j1));

                IRInst labElse;
                labElse.op = IROp::LABEL;
                labElse.dst = IRValue::label(lElse);
                cur->push_back(std::move(labElse));
                for (auto& st : iff.elseBody) lowerStmt(*st);
                IRInst j2;
                j2.op = IROp::JUMP;
                j2.ops = {IRValue::label(lEnd)};
                cur->push_back(std::move(j2));

                IRInst labEnd;
                labEnd.op = IROp::LABEL;
                labEnd.dst = IRValue::label(lEnd);
                cur->push_back(std::move(labEnd));
                return;
            }
        }
    }
};

static std::string vToStr(const IRValue& v) {
    switch (v.kind) {
        case IRValueKind::Temp: return "%t" + std::to_string(v.id) + ":" + typeToString(v.type);
        case IRValueKind::Label: return "L" + std::to_string(v.id);
        case IRValueKind::Var: return v.name + ":" + typeToString(v.type);
        case IRValueKind::Const:
            if (std::holds_alternative<int64_t>(v.c.v)) return std::to_string(std::get<int64_t>(v.c.v));
            if (std::holds_alternative<DecMap>(v.c.v)) return "dec" + decMapToDebug(std::get<DecMap>(v.c.v));
            if (std::holds_alternative<std::string>(v.c.v)) return "\"" + std::get<std::string>(v.c.v) + "\"";
            if (std::holds_alternative<bool>(v.c.v)) return std::get<bool>(v.c.v) ? "true" : "false";
            return "<const>";
        case IRValueKind::None: return "_";
    }
    return "_";
}

static std::string opToStr(IROp op) {
    switch (op) {
        case IROp::LABEL: return "LABEL";
        case IROp::JUMP: return "JUMP";
        case IROp::BR: return "BR";
        case IROp::RETURN: return "RETURN";
        case IROp::CONST: return "CONST";
        case IROp::LOAD: return "LOAD";
        case IROp::STORE: return "STORE";
        case IROp::BINOP: return "BINOP";
        case IROp::CALL: return "CALL";
    }
    return "OP";
}

} // namespace

IRProgram lowerToIR(const Program& prog) {
    IRBuilder b;
    b.out.header = prog.header;

    for (auto& fn : prog.funcs) {
        IRFunction f;
        f.name = fn.name;
        f.params = fn.params;
        b.cur = &f.insts;
        b.tmp = 0;
        b.lbl = 0;
        for (auto& st : fn.body) b.lowerStmt(*st);
        f.tempCount = b.tmp;
        f.labelCount = b.lbl;
        b.out.functions.push_back(std::move(f));
    }

    b.cur = &b.out.main;
    b.tmp = 0;
    b.lbl = 0;
    for (auto& st : prog.mainloop) b.lowerStmt(*st);

    return b.out;
}

IRProgram lowerIRForBackend(const IRProgram& ir, const std::string& /*backend*/) {
    // TODO: legalization passes (stream lowering, list/range representation, decimal handling).
    return ir;
}

std::string irToText(const IRProgram& ir) {
    std::ostringstream ss;
    ss << "; AC* low-level IR\n";
    ss << "; header: " << (ir.header.empty() ? "<none>" : ir.header) << "\n\n";
    for (auto& fn : ir.functions) {
        ss << "func " << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); i++) {
            if (i) ss << ", ";
            ss << fn.params[i];
        }
        ss << ")\n";
        for (size_t i = 0; i < fn.insts.size(); i++) {
            const auto& ins = fn.insts[i];
            ss << "  " << i << ": " << opToStr(ins.op) << " ";
            if (ins.op == IROp::LABEL) ss << vToStr(ins.dst);
            else if (ins.op == IROp::CONST || ins.op == IROp::LOAD || ins.op == IROp::BINOP || ins.op == IROp::CALL)
                ss << vToStr(ins.dst) << " ";
            else if (ins.op == IROp::STORE) ss << vToStr(ins.dst) << " ";
            if (!ins.aux.empty()) ss << ins.aux << " ";
            for (auto& o : ins.ops) ss << vToStr(o) << " ";
            ss << "\n";
        }
        ss << "\n";
    }

    ss << "main\n";
    for (size_t i = 0; i < ir.main.size(); i++) {
        const auto& ins = ir.main[i];
        ss << "  " << i << ": " << opToStr(ins.op) << " ";
        if (ins.op == IROp::LABEL) ss << vToStr(ins.dst);
        else if (ins.op == IROp::CONST || ins.op == IROp::LOAD || ins.op == IROp::BINOP || ins.op == IROp::CALL)
            ss << vToStr(ins.dst) << " ";
        else if (ins.op == IROp::STORE) ss << vToStr(ins.dst) << " ";
        if (!ins.aux.empty()) ss << ins.aux << " ";
        for (auto& o : ins.ops) ss << vToStr(o) << " ";
        ss << "\n";
    }
    return ss.str();
}

} // namespace aca

