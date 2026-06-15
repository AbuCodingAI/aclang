#pragma once
#include "types.hpp"
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace aca {

enum class ExprKind { Literal, Var, Binary, Call, List, Range };

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    ExprKind kind;
    int line = 0;
    int col = 0;
    Type inferred = Type::Unknown();

    std::variant<
        LitValue,                                  // Literal
        std::string,                               // Var name (may be qualified: Term.display)
        std::tuple<std::string, ExprPtr, ExprPtr>, // Binary
        std::tuple<std::string, std::vector<ExprPtr>>, // Call
        std::vector<ExprPtr>,                      // List
        std::pair<ExprPtr, ExprPtr>                // Range
    >
        data;

    static ExprPtr lit(LitValue v, int line, int col) {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Literal;
        e->line = line;
        e->col = col;
        e->inferred = v.t;
        e->data = std::move(v);
        return e;
    }
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

struct AssignPayload {
    std::string name;
    std::string op; // "=", "+=", "-=", "@="
    ExprPtr expr;
};

struct IfPayload {
    ExprPtr cond;
    std::vector<StmtPtr> thenBody;
    std::vector<StmtPtr> elseBody;
};

struct ReturnPayload {
    ExprPtr expr;
};

enum class StmtKind { Assign, ExprOnly, Return, If };

struct Stmt {
    StmtKind kind;
    int line = 0;
    int col = 0;
    std::variant<AssignPayload, ExprPtr, ReturnPayload, IfPayload> data;
};

struct FuncDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    int line = 0;
    int col = 0;
};

struct Program {
    std::string header;
    std::vector<FuncDef> funcs;
    std::vector<StmtPtr> mainloop;
};

} // namespace aca

