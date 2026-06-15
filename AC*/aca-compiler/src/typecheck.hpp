#pragma once
#include "ast.hpp"
#include "types.hpp"
#include <map>
#include <stdexcept>
#include <string>

namespace aca {

struct TypeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class TypeChecker {
public:
    void check(Program& prog) {
        funcs.clear();
        // Predeclare functions
        for (auto& fn : prog.funcs) funcs[fn.name] = Type::Unknown();

        // Infer each function return type
        for (auto& fn : prog.funcs) {
            env.clear();
            for (auto& p : fn.params) env[p] = Type::Unknown();
            Type rt = Type::Unknown();
            for (auto& st : fn.body) {
                Type got = checkStmt(*st);
                rt = unify(rt, got);
            }
            funcs[fn.name] = rt;
        }

        env.clear();
        for (auto& st : prog.mainloop) (void)checkStmt(*st);
    }

private:
    std::map<std::string, Type> env;
    std::map<std::string, Type> funcs;

    [[noreturn]] void terr(int line, int col, const std::string& msg) const {
        throw TypeError("TypeError at " + std::to_string(line) + ":" + std::to_string(col) + " " + msg);
    }

    static Type unify(const Type& a, const Type& b) {
        if (a.kind == TypeKind::Unknown) return b;
        if (b.kind == TypeKind::Unknown) return a;
        if (a.kind != b.kind) return Type::Unknown();
        if (a.kind == TypeKind::Numeral) {
            auto as = std::get<NumeralSubtype>(a.sub);
            auto bs = std::get<NumeralSubtype>(b.sub);
            if (as == NumeralSubtype::NumDec || bs == NumeralSubtype::NumDec) return Type::Numeral(NumeralSubtype::NumDec);
            return Type::Numeral(NumeralSubtype::NumInt);
        }
        if (a.kind == TypeKind::Boolean) return Type::Boolean(BooleanSubtype::True);
        return a;
    }

    Type inferExpr(Expr& e) {
        switch (e.kind) {
            case ExprKind::Literal: {
                const auto& lit = std::get<LitValue>(e.data);
                e.inferred = lit.t;
                return e.inferred;
            }
            case ExprKind::Var: {
                const auto& name = std::get<std::string>(e.data);
                if (auto it = env.find(name); it != env.end()) return e.inferred = it->second;
                return e.inferred = Type::Unknown();
            }
            case ExprKind::Binary: {
                auto& tup = std::get<std::tuple<std::string, ExprPtr, ExprPtr>>(e.data);
                const std::string& op = std::get<0>(tup);
                Expr& le = *std::get<1>(tup);
                Expr& re = *std::get<2>(tup);
                Type lt = inferExpr(le);
                Type rt = inferExpr(re);

                auto forceNumeral = [&](Expr& ex, Type& tx) {
                    if (!isUnknown(tx)) return;
                    if (ex.kind == ExprKind::Var) {
                        const auto& n = std::get<std::string>(ex.data);
                        env[n] = Type::Numeral(NumeralSubtype::NumInt);
                        tx = env[n];
                        return;
                    }
                    tx = Type::Numeral(NumeralSubtype::NumInt);
                };

                if (op == "+" || op == "-" || op == "*" || op == "/" || op == "@" || op == "mod") {
                    if (isUnknown(lt)) forceNumeral(le, lt);
                    if (isUnknown(rt)) forceNumeral(re, rt);
                    if (!isNumeral(lt) || !isNumeral(rt)) terr(e.line, e.col, "Numeral operation '" + op + "' requires Numeral operands");
                    auto lsub = std::get<NumeralSubtype>(lt.sub);
                    auto rsub = std::get<NumeralSubtype>(rt.sub);
                    NumeralSubtype out = NumeralSubtype::NumInt;
                    if (op == "/" ) out = NumeralSubtype::NumDec;
                    else if (lsub == NumeralSubtype::NumDec || rsub == NumeralSubtype::NumDec) out = NumeralSubtype::NumDec;
                    return e.inferred = Type::Numeral(out);
                }

                if (op == "is" || op == "<" || op == ">" || op == "#>" || op == "#<") {
                    if (isUnknown(lt)) forceNumeral(le, lt);
                    if (isUnknown(rt)) forceNumeral(re, rt);
                    return e.inferred = Type::Boolean(BooleanSubtype::True);
                }

                return e.inferred = Type::Unknown();
            }
            case ExprKind::Call: {
                auto& tup = std::get<std::tuple<std::string, std::vector<ExprPtr>>>(e.data);
                const std::string& name = std::get<0>(tup);
                auto& args = std::get<1>(tup);
                for (auto& a : args) (void)inferExpr(*a);

                if (name == "count") return e.inferred = Type::Numeral(NumeralSubtype::NumInt);
                if (name == "sqrt") return e.inferred = Type::Numeral(NumeralSubtype::NumDec);
                if (name == "Term.display") return e.inferred = Type::Unknown();
                if (name == "__lambda__") return e.inferred = Type::Unknown();
                if (name == "__stream__") return e.inferred = Type::Unknown();

                if (auto it = funcs.find(name); it != funcs.end()) return e.inferred = it->second;
                return e.inferred = Type::Unknown();
            }
            case ExprKind::List: {
                auto& items = std::get<std::vector<ExprPtr>>(e.data);
                for (auto& it : items) (void)inferExpr(*it);
                return e.inferred = Type::Unknown();
            }
            case ExprKind::Range: {
                auto& pr = std::get<std::pair<ExprPtr, ExprPtr>>(e.data);
                Expr& ae = *pr.first;
                Expr& be = *pr.second;
                Type a = inferExpr(ae);
                Type b = inferExpr(be);
                auto forceNumeral = [&](Expr& ex, Type& tx) {
                    if (!isUnknown(tx)) return;
                    if (ex.kind == ExprKind::Var) {
                        const auto& n = std::get<std::string>(ex.data);
                        env[n] = Type::Numeral(NumeralSubtype::NumInt);
                        tx = env[n];
                        return;
                    }
                    tx = Type::Numeral(NumeralSubtype::NumInt);
                };
                if (isUnknown(a)) forceNumeral(ae, a);
                if (isUnknown(b)) forceNumeral(be, b);
                if (!isNumeral(a) || !isNumeral(b)) terr(e.line, e.col, "Range [a=>b] requires Numeral endpoints");
                return e.inferred = Type::Unknown();
            }
        }
        return e.inferred = Type::Unknown();
    }

    Type checkStmt(Stmt& s) {
        switch (s.kind) {
            case StmtKind::Assign: {
                auto& a = std::get<AssignPayload>(s.data);
                Type rhsT = inferExpr(*a.expr);
                if (a.op == "=") {
                    env[a.name] = rhsT;
                    return Type::Unknown();
                }
                Type lhsT = env.count(a.name) ? env[a.name] : Type::Unknown();
                if (!isNumeral(rhsT) && !isUnknown(rhsT)) terr(s.line, s.col, "Augmented assignment requires Numeral RHS");
                if (!isUnknown(lhsT) && !isNumeral(lhsT)) terr(s.line, s.col, "Augmented assignment requires Numeral LHS");
                NumeralSubtype out = NumeralSubtype::NumInt;
                if ((lhsT.kind == TypeKind::Numeral && std::get<NumeralSubtype>(lhsT.sub) == NumeralSubtype::NumDec) ||
                    (rhsT.kind == TypeKind::Numeral && std::get<NumeralSubtype>(rhsT.sub) == NumeralSubtype::NumDec))
                    out = NumeralSubtype::NumDec;
                env[a.name] = Type::Numeral(out);
                return Type::Unknown();
            }
            case StmtKind::ExprOnly: {
                auto& e = std::get<ExprPtr>(s.data);
                (void)inferExpr(*e);
                return Type::Unknown();
            }
            case StmtKind::Return: {
                auto& r = std::get<ReturnPayload>(s.data);
                return inferExpr(*r.expr);
            }
            case StmtKind::If: {
                auto& iff = std::get<IfPayload>(s.data);
                Type ct = inferExpr(*iff.cond);
                if (!isBoolean(ct) && !isUnknown(ct)) terr(s.line, s.col, "IF condition must be Boolean");
                for (auto& st : iff.thenBody) (void)checkStmt(*st);
                for (auto& st : iff.elseBody) (void)checkStmt(*st);
                return Type::Unknown();
            }
        }
        return Type::Unknown();
    }
};

} // namespace aca

