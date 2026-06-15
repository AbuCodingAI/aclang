#pragma once
#include <string>
#include <vector>
#include <memory>

enum class NodeType {
    Program,

    // Top-level declarations
    BackendDecl,    // AC+ LIB header
    UseFlibStmt,    // use flib file.acp
    FuncDef,        // Make name func(params) body
    MainLoop,       // <mainloop> body <mainloop>

    // Statements
    Block,
    AssignStmt,         // var = expr
    RegDecl,            // reg 64box1 = expr
    StackDecl,          // stack varname type
    MemWrite,           // addr << value  (or addr + offset << value)
    CompoundAssign,     // var += expr  etc.
    IfStmt,             // IF / ELSEIF / OTHER
    WhilstStmt,         // WHILST cond body
    ForStmt,            // FOR var in range N
    ReturnStmt,         // return expr
    HaltStmt,           // /kill /stop

    // Expressions
    BinaryExpr,         // lhs op rhs
    UnaryExpr,          // # expr
    CallExpr,           // name(args...)
    NumberLit,          // 42 / 0xB8000
    StringLit,          // $...$
    IdentExpr,          // variable name (may include reg name like 64box1)
    LocateExpr,         // locate varname
    NilLit,             // nil
    OffsetExpr,         // addr + offset  (used as lhs of <<)
};

struct ASTNode;
using NodePtr  = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

struct ASTNode {
    NodeType           type;
    std::string        value;       // identifier name, op char, etc.
    NodeList           children;
    std::vector<std::string> attrs; // parameter names, condition text, etc.
    int                line = 0;

    explicit ASTNode(NodeType t, std::string v = "", int l = 0)
        : type(t), value(std::move(v)), line(l) {}
};
