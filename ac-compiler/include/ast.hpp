#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include "type.hpp"

// Forward declarations
struct ASTNode;
using NodePtr = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

enum class NodeType {
    Program,
    Block,          // generic block (tag body)
    BackendDecl,    // AC->PY etc
    UseStmt,        // use X
    SaveStmt,       // save as name
    AssignStmt,     // x = y
    PlusEqualStmt,  // x += y
    MinusEqualStmt, // x -= y
    MultiplyEqualStmt, // x *= y  
    DivideEqualStmt, // x /= y
    AtEqualStmt,     // x @= y (compound multiplication since * is comment)
    IfStmt,         // IF condition body [OTHER body]
    ElseIfStmt,     // ELSEIF condition body
    ForLoop,        // FOR item in list
    WhilstLoop,     // WHILST condition
    ReturnStmt,     // return expr
    ListLiteral,    // [$a, b$]
    TupleLiteral,   // {$a, b$}
    IndexExpr,      // list[1] (1-based)
    DisplayStmt,    // display $string$ (screen output)
    ObjDecl,        // Obj.Name
    PropAssign,     // Name.prop = value
    MethodCall,     // Name.method(args)
    ConfigCall,     // Name.config key=value ...
    FuncDef,        // Make funcname func(arg)
    CustomTagDef,   // def tag <name>
    TagBlock,       // <tagname> ... <tagname>
    RaiseStmt,      // raise ERR(msg)
    KillStmt,       // /kill
    StringLit,
    NumberLit,
    Identifier,
    ForeignBlock,   // <Foreign> raw passthrough 
    EventListener,  // configure event-listener
    WhenBlock,      // when many hitbox overlap
    SpawnStmt,      // SpawnTerrain etc
    BinaryExpr,     // fn a*(b-c) — multiply/arithmetic
    UseLibStmt,     // use ilib <libname>
    RangeExpr,      // range N  → [0..N], N must be Numeral Pos
    SequenceExpr,   // sequence(x,y) → [x..y], breaks if x > y
    PassStmt,       // pass → no-op placeholder
    SkipStmt,       // skip → stop rest of if/elseif/other chain
    BreakStmt,      // break → exit loop
    ContinueStmt,   // continue → next loop iteration
    DestroyStmt,    // destroy x → remove variable from existence
};

struct ASTNode {
    NodeType type;
    std::string value;          // name, operator, literal value
    NodeList children;
    std::vector<std::string> attrs; // extra attributes
    TypePtr inferredType;       // type information for this node

    ASTNode(NodeType t, std::string v = "")
        : type(t), value(std::move(v)), inferredType(nullptr) {}
};
