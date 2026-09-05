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
    XorEqualStmt,    // x |= y (compound XOR)
    IfStmt,         // IF condition body [OTHER body]
    ElseIfStmt,     // ELSEIF condition body
    ForLoop,        // FOR item in list
    WhilstLoop,     // WHILST condition
    ReturnStmt,     // return expr
    YieldStmt,      // yield expr — marks the enclosing Make func as a generator
    ListLiteral,    // [$a, b$]
    TupleLiteral,   // (a, b, ...) — fixed-shape tuple literal; children = element exprs
    DictLiteral,    // {key: value, key2: value2}
    IndexExpr,      // list[1] (1-based)
    DisplayStmt,    // should be Term.display $string$ (screen output)
    ObjDecl,        // Obj.Name
    PropAssign,     // Name.prop = value
    MethodCall,     // Name.method(args)
    MethodChain,    // method1 & method2 (same arg) or method1 && method2 (different args)
    ConfigCall,     // Name.config key=value ...
    FuncDef,        // Make funcname func(arg)
    CustomTagDef,   // def tag <name>
    TagBlock,       // <tagname> ... <tagname>
    RaiseStmt,      // raise ERR(msg)
    RaiseClauseStmt, // raise Clause(msg) — generic clause output "Clause: msg"
    LazyEvalExpr,    // lazy_eval(expr) — safe deferred evaluation
    KillStmt,       // /kill    — hard terminate
    StopStmt,       // /stop    — graceful stop (clean exit)
    EndStmt,        // /end     — break out of enclosing loop; exit if at top level
    RestartStmt,    // /restart — rerun program body once from top, skip on second pass
    HaltStmt,       // /halt n  — pause execution for n seconds (/halt math.inf → /stop)
    StringLit,
    NumberLit,
    Identifier,
    ForeignBlock,   // <Foreign> raw passthrough 
    EventListener,  // configure event-listener
    SpawnStmt,      // SpawnTerrain etc
    BinaryExpr,     // fn a*(b-c) — multiply/arithmetic (legacy string-based)
    UnaryExpr,      // Structured unary expression: op operand
    TernaryExpr,    // condition | true_expr, # false_expr — children: [cond, true, false]
    CallExpr,       // Structured function call: func(args)
    LiteralExpr,    // Structured literal: int, float, string, bool, null
    UseLibStmt,     // use ilib <libname>
    RangeExpr,      // range N  → [0..N], N must be Numeral Pos
    SequenceExpr,   // sequence(x,y) → [x..y], breaks if x > y
    IotaExpr,       // iota N  → lazy 0..N-1, generates numbers on the spot
    StreamExpr,     // stream(x,y[,step]) → lazy sequence, generates numbers on the spot
    XRangeExpr,     // xrange N → [1..N], 1-indexed range
    XIotaExpr,      // xiota N  → lazy 1..N, 1-indexed iota
    PassStmt,       // pass → no-op placeholder
    SkipStmt,       // skip → stop rest of if/elseif/other chain
    BreakStmt,      // break → exit loop
    ContinueStmt,   // continue → next loop iteration
    DestroyStmt,    // destroy x → remove variable from existence
    FunctionCall,   // func arg1 arg2 arg3 (multiple arguments)
    KeyBinding,     // on value=key function_call
    InputStmt,      // input <keybind> → send ghost/simulated input
    BindStmt,       // bind <key> to <function> → standalone key→callback registration (value=key, attrs[0]=function name)
    EvalExpr,       // eval(expr) → evaluate string as AC expression, returns value
    BundleDef,      // bundle X — class/struct definition (body has BundleMember nodes)
    BundleMember,   // bundle member: value=access("public"/"private"), child=actual member node
    TryCatchStmt,   // try body / catch report-var body / after body
    CatchClause,    // catch [Type] [report var] body
    AfterClause,    // after body
    FreeDecl,       // free x, y  — declare vars as globally scoped (like Python's global)
    AliasDecl,      // alias x = y — bidirectional live binding between two variables
    TypeCoerceStmt, // to_dec/to_int/to_string/to_bool/short/mini/atomic x [= expr] — coerce/
                    // declare x as that type; also math.GoodDec x [= expr] / math.LongInt x
                    // [= expr] (attrs[0]="GOODDEC"/"LONGINT"). NOT bare "dec x = expr" —
                    // that's not real syntax, verified (see KW_DEC's own comment in token.hpp).
    ConstDecl,      // const x = expr — immutable binding
    CompfoldStmt,   // compfold x = expr — request compile-time constant folding
    CopyStmt,       // cp x = y — explicit value copy
    CondStmt,       // cond expr { is value: block } ... OTHER block
    CondCase,       // is <expr> ... (child[0]=expr, child[1]=block)
    CondOther,      // OTHER ... (child[0]=block)
    ExportStmt,     // export name[, name...] — marks names visible to importing files
    DestructureAssignStmt, // a, b = expr — attrs = ordered LHS names, children[0] = RHS expr
};

struct ASTNode {
    NodeType type;
    std::string value;          // name, operator, literal value
    NodeList children;
    std::vector<std::string> attrs; // extra attributes
    TypePtr inferredType;       // type information for this node
    bool exported = false;      // marked by `export` — visible to files importing this one
    int angleUnit = 0;          // marked by `DEG`(1)/`RAD`(2) prefix — math ilib angle-unit sugar

    ASTNode(NodeType t, std::string v = "")
        : type(t), value(std::move(v)), inferredType(nullptr) {}
};

/* Structured expression nodes for Pratt parser

 Binary expression: left op right
 For BinaryExpr nodes:
 - value: operator as string (+, -, *, /, %, @, <, >, #>, #<, is, #=, not, AND, OR)
 - children[0]: left operand
 - children[1]: right operand
 Note: BinaryExpr is used for both legacy string-based and new structured expressions

 Unary expression: op operand
 For UnaryExpr nodes:
 - value: operator as string (-, NOT)
 - children[0]: operand

 Function call: func(arg1, arg2, ...)
 For CallExpr nodes:
 - value: function name
 - children: argument expressions

 Literal expression: int, float, string, bool, null
 For LiteralExpr nodes:
 - value: literal value as string
 - attrs[0]: type ("INT", "FLOAT", "STRING", "BOOL", "NULL")
*/