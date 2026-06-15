#include "../include/aibc.hpp"
#include "../include/ast.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

// Native function names exposed by each ilib
static const std::set<std::string> MATH_NATIVE_FUNS = {
    "math.sin","math.cos","math.tan","math.csc","math.sec","math.cot",
    "math.asin","math.acos","math.atan","math.atan2",
    "math.deg2rad","math.rad2deg",
    "math.sqrt","math.cbrt","math.pow","math.abs",
    "math.floor","math.ceil","math.round",
    "math.hypot","math.ln","math.log","math.log2","math.log10",
    "math.mod","math.clamp","math.gcd","math.lcm","math.is_prime",
    "math.pi","math.e","math.tau","math.phi","math.inf","math.eval",
};
static const std::set<std::string> CAMERA_NATIVE_FUNS = {
    "camera.init","camera.capture","camera.capture_latest",
    "camera.capture_first","camera.release",
    "camera.sidebar_config","camera.sidebar_setregion",
    "camera.sidebar_setinteractive","camera.sidebar_display",
    "camera.sidebar_ask","camera.sidebar_getinput",
    "camera.screen_setmode","camera.screen_update",
};

// ── AI Bytecode Codegen ────────────────────────────────────────────────────
// Walks the AC/AI AST and emits AIBC instructions.
// dec literals → DEC_NEW + dec arithmetic opcodes
// int/string   → integer registers
// All registers are per-function locals.

struct AICodeGen {
    AiBcModule& mod;

    // Per-function state
    std::map<std::string, uint8_t> locals;   // var name → register index
    uint8_t  nextReg    = 0;
    uint32_t funcStart  = 0;

    // Function name → final index in mod.funcs (after main is inserted at 0)
    // main=0, user funcs start at 1 in declaration order
    std::map<std::string, uint8_t> funcIndex;

    // Native function names (populated from ilib imports)
    std::set<std::string> nativeFuncs;

    // Control flow label stacks
    std::vector<uint32_t> breakPatchList;   // instructions to back-patch for break
    std::vector<uint32_t> contPatchList;    // instructions to back-patch for continue
    std::vector<uint32_t> breakTargets;     // pc targets for break
    std::vector<uint32_t> contTargets;      // pc targets for continue

    AICodeGen(AiBcModule& m) : mod(m) {}

    // Allocate a register for a variable
    uint8_t allocReg(const std::string& name) {
        if (!locals.count(name)) locals[name] = nextReg++;
        return locals[name];
    }
    uint8_t tempReg() { return nextReg++; }

    // Emit a CALL_NATIVE for a known native function; returns the dst register holding the result.
    uint8_t emitNativeCall(const std::string& name,
                           const std::vector<const ASTNode*>& argNodes, bool isDec = false) {
        uint8_t argc = (uint8_t)std::min(argNodes.size(), (size_t)6);
        std::vector<uint8_t> argRegs(argc);
        for (uint8_t i = 0; i < argc; i++)
            argRegs[i] = compileExpr(*argNodes[i], isDec);
        for (uint8_t i = 0; i < argc; i++)
            if (argRegs[i] != i) emit({ AIOp::STORE_VAR, i, argRegs[i], 0 });
        uint16_t pidx = mod.addStr(name);
        emit({ AIOp::CALL_NATIVE, argc, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
        uint8_t dst = tempReg();
        emit({ AIOp::LOAD_VAR, dst, 0, 0 });
        return dst;
    }

    // Emit and return instruction index
    uint32_t emit(AiBcInstr i) { return mod.emit(i); }
    uint32_t pc() const { return (uint32_t)mod.bytecode.size(); }

    // ── EXPRESSION ─────────────────────────────────────────────────────────
    // Returns the register holding the result.
    uint8_t compileExpr(const ASTNode& node, bool isDec = false) {
        switch (node.type) {

        case NodeType::LiteralExpr: {
            uint8_t dst = tempReg();
            std::string typ = node.attrs.empty() ? "INT" : node.attrs[0];
            if (typ == "DEC" || typ == "FLOAT" || (isDec && (typ == "INT" || typ == "BOOL"))) {
                // FLOAT and explicit DEC always → exact Dec; INT under dec cast → Dec too
                uint16_t pidx = mod.addDec(node.value);
                emit({ AIOp::DEC_NEW, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            } else if (typ == "INT" || typ == "BOOL") {
                int64_t v = std::stoll(node.value);
                uint16_t pidx = mod.addInt(v);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            } else if (typ == "STRING") {
                uint16_t pidx = mod.addStr(node.value);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            } else if (typ == "NULL") {
                uint16_t pidx = mod.addInt(0);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            } else {
                uint16_t pidx = mod.addInt(0);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            }
            return dst;
        }

        case NodeType::Identifier: {
            uint8_t dst = tempReg();
            if (locals.count(node.value))
                emit({ AIOp::LOAD_VAR, dst, locals[node.value], 0 });
            else {
                uint16_t pidx = mod.addInt(0);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx & 0xFF), (uint8_t)(pidx >> 8) });
            }
            return dst;
        }

        case NodeType::UnaryExpr: {
            uint8_t src = compileExpr(*node.children[0], isDec);
            uint8_t dst = tempReg();
            if (node.value == "-") emit({ AIOp::NEG, dst, src, 0 });
            else                   emit({ AIOp::NOT, dst, src, 0 });
            return dst;
        }

        case NodeType::BinaryExpr: {
            if (node.children.size() < 2) {
                // Legacy string form — load as int 0 (unsupported path)
                uint8_t dst = tempReg();
                uint16_t pidx = mod.addInt(0);
                emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
                return dst;
            }
            std::string op = node.value;
            bool decOp = isDec;

            uint8_t lhs = compileExpr(*node.children[0], decOp);
            uint8_t rhs = compileExpr(*node.children[1], decOp);
            uint8_t dst = tempReg();

            if (op == "@") {
                // @ is a universal multiply: string×int repetition or any numeric×numeric
                emit({ AIOp::AT_MUL, dst, lhs, rhs });
            } else if (decOp) {
                if      (op == "+")              emit({ AIOp::DEC_ADD, dst, lhs, rhs });
                else if (op == "*")              emit({ AIOp::DEC_MUL, dst, lhs, rhs });
                else if (op == "-")              emit({ AIOp::DEC_SUB, dst, lhs, rhs });
                else if (op == "/")              emit({ AIOp::DEC_DIV, dst, lhs, rhs });
                else if (op == "==" || op == "is") emit({ AIOp::DEC_CMP_EQ, dst, lhs, rhs });
                else if (op == "<")              emit({ AIOp::DEC_CMP_LT, dst, lhs, rhs });
                else if (op == ">")              emit({ AIOp::DEC_CMP_GT, dst, lhs, rhs });
                else                             emit({ AIOp::ADD, dst, lhs, rhs }); // fallback
            } else {
                if      (op == "+")              emit({ AIOp::ADD,    dst, lhs, rhs });
                else if (op == "-")              emit({ AIOp::SUB,    dst, lhs, rhs });
                else if (op == "*")              emit({ AIOp::MUL,    dst, lhs, rhs });
                else if (op == "/")              emit({ AIOp::DIV,    dst, lhs, rhs });
                else if (op == "%")              emit({ AIOp::MOD,    dst, lhs, rhs });
                else if (op == "|")              emit({ AIOp::XOR,    dst, lhs, rhs });
                else if (op == "xsub")           emit({ AIOp::XSUB,   dst, lhs, rhs });
                else if (op == "==" || op == "is") emit({ AIOp::CMP_EQ, dst, lhs, rhs });
                else if (op == "!=" || op == "#=") emit({ AIOp::CMP_NE, dst, lhs, rhs });
                else if (op == "<")              emit({ AIOp::CMP_LT, dst, lhs, rhs });
                else if (op == ">")              emit({ AIOp::CMP_GT, dst, lhs, rhs });
                else if (op == "<="  || op == "#>") emit({ AIOp::CMP_LE, dst, lhs, rhs });
                else if (op == ">="  || op == "#<") emit({ AIOp::CMP_GE, dst, lhs, rhs });
                else if (op == "AND")            emit({ AIOp::AND,    dst, lhs, rhs });
                else if (op == "OR")             emit({ AIOp::OR,     dst, lhs, rhs });
                else emit({ AIOp::ADD, dst, lhs, rhs }); // unknown operator → no-op add
            }
            return dst;
        }

        case NodeType::MethodCall: {
            // Native library call (math.sin, camera.capture, ...)
            if (nativeFuncs.count(node.value)) {
                std::vector<const ASTNode*> argPtrs;
                for (auto& c : node.children) argPtrs.push_back(c.get());
                return emitNativeCall(node.value, argPtrs, isDec);
            }
            // Term.ask in expression context: display prompt then read input
            if (node.value == "Term.ask") {
                if (!node.children.empty()) {
                    uint8_t prompt = compileExpr(*node.children[0]);
                    emit({ AIOp::DISPLAY, prompt, 0, 0 });
                }
                uint8_t dst = tempReg();
                emit({ AIOp::INPUT, dst, 0, 0 });
                return dst;
            }
            // Bundle/user method call: BundleName.method(args) or func.alias(args)
            {
                auto it = funcIndex.find(node.value);
                if (it != funcIndex.end()) {
                    uint8_t fnIdx = it->second;
                    uint8_t argc = (uint8_t)std::min(node.children.size(), (size_t)6);
                    std::vector<uint8_t> argRegs(argc);
                    for (uint8_t i = 0; i < argc; i++)
                        argRegs[i] = compileExpr(*node.children[i], isDec);
                    for (uint8_t i = 0; i < argc; i++)
                        if (argRegs[i] != i) emit({ AIOp::STORE_VAR, i, argRegs[i], 0 });
                    emit({ AIOp::CALL, fnIdx, argc, 0 });
                    uint8_t dst = tempReg();
                    emit({ AIOp::LOAD_VAR, dst, 0, 0 });
                    return dst;
                }
            }
            // Unknown method call in expression context: return 0
            uint8_t dst = tempReg();
            uint16_t pidx = mod.addInt(0);
            emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
            return dst;
        }

        case NodeType::CallExpr: {
            // Native library call
            if (nativeFuncs.count(node.value)) {
                std::vector<const ASTNode*> argPtrs;
                for (auto& c : node.children) argPtrs.push_back(c.get());
                return emitNativeCall(node.value, argPtrs, isDec);
            }
            // Built-in type conversions: to_dec, to_int, to_string, to_bool
            if (node.value == "to_dec" || node.value == "to_int" ||
                node.value == "to_string" || node.value == "to_bool") {
                uint8_t src;
                if (node.children.empty()) {
                    src = tempReg();
                    uint16_t p = mod.addInt(0);
                    emit({ AIOp::LOAD_CONST, src, (uint8_t)(p&0xFF), (uint8_t)(p>>8) });
                } else {
                    src = compileExpr(*node.children[0]);
                }
                uint8_t dst = tempReg();
                AIOp op = (node.value == "to_dec")    ? AIOp::CAST_DEC :
                          (node.value == "to_int")    ? AIOp::CAST_INT :
                          (node.value == "to_string") ? AIOp::CAST_STR :
                                                        AIOp::CAST_BOOL;
                emit({ op, dst, src, 0 });
                return dst;
            }

            // Resolve function index from the pre-built map (handles recursion + forward refs)
            uint8_t fnIdx = 0;
            auto it = funcIndex.find(node.value);
            if (it != funcIndex.end()) fnIdx = it->second;
            uint8_t argc = (uint8_t)std::min(node.children.size(), (size_t)6);
            // Compute ALL args into fresh temps FIRST, then move to regs 0..n-1.
            // This avoids clobbering caller locals (e.g. param reg 0) before all args are ready.
            std::vector<uint8_t> argRegs(argc);
            for (uint8_t i = 0; i < argc; i++)
                argRegs[i] = compileExpr(*node.children[i], isDec);
            for (uint8_t i = 0; i < argc; i++)
                if (argRegs[i] != i) emit({ AIOp::STORE_VAR, i, argRegs[i], 0 });
            emit({ AIOp::CALL, fnIdx, argc, 0 });
            uint8_t dst = tempReg();
            emit({ AIOp::LOAD_VAR, dst, 0, 0 }); // result in reg 0
            return dst;
        }

        case NodeType::IndexExpr: {
            // list[idx] → LIST_GET
            // children[0]=list (Identifier), children[1]=index expr
            // node.value = list var name; attrs[0] = legacy string index
            uint8_t listReg = allocReg(node.value);
            uint8_t idxReg;
            if (node.children.size() >= 2) {
                idxReg = compileExpr(*node.children[1]);
            } else if (!node.attrs.empty()) {
                // legacy: index in attrs[0]
                ASTNode fakeIdent(NodeType::Identifier, node.attrs[0]);
                idxReg = compileExpr(fakeIdent);
            } else {
                idxReg = tempReg();
                uint16_t z = mod.addInt(0);
                emit({ AIOp::LOAD_CONST, idxReg, (uint8_t)(z&0xFF), (uint8_t)(z>>8) });
            }
            uint8_t dst = tempReg();
            emit({ AIOp::LIST_GET, dst, listReg, idxReg });
            return dst;
        }

        default: {
            uint8_t dst = tempReg();
            uint16_t pidx = mod.addInt(0);
            emit({ AIOp::LOAD_CONST, dst, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
            return dst;
        }
        }
    }

    // ── STATEMENT ──────────────────────────────────────────────────────────
    void compileStmt(const ASTNode& node) {
        switch (node.type) {

        case NodeType::AssignStmt: {
            uint8_t dst = allocReg(node.value);
            if (!node.children.empty()) {
                uint8_t src = compileExpr(*node.children[0]);
                emit({ AIOp::STORE_VAR, dst, src, 0 });
            } else if (!node.attrs.empty()) {
                const std::string& raw = node.attrs[0];
                if (raw.rfind("__ailist__", 0) == 0) {
                    // Column list from datac import: "__ailist__TYPE:val,TYPE:val,..."
                    std::string items = raw.substr(10);
                    uint8_t startReg = nextReg;
                    uint8_t count = 0;
                    size_t pos = 0;
                    while (pos < items.size() && count < 255) {
                        auto comma = items.find(',', pos);
                        std::string item = (comma == std::string::npos)
                            ? items.substr(pos) : items.substr(pos, comma - pos);
                        pos = (comma == std::string::npos) ? items.size() : comma + 1;
                        auto colon = item.find(':');
                        if (colon == std::string::npos) continue;
                        std::string tp  = item.substr(0, colon);
                        std::string val = item.substr(colon + 1);
                        uint8_t reg = nextReg++;
                        if (tp == "INT") {
                            int64_t v = 0; try { v = std::stoll(val); } catch(...) {}
                            uint16_t p = mod.addInt(v);
                            emit({ AIOp::LOAD_CONST, reg, (uint8_t)(p&0xFF), (uint8_t)(p>>8) });
                        } else if (tp == "STR") {
                            uint16_t p = mod.addStr(val);
                            emit({ AIOp::LOAD_CONST, reg, (uint8_t)(p&0xFF), (uint8_t)(p>>8) });
                        } else if (tp == "DEC") {
                            uint16_t p = mod.addDec(val);
                            emit({ AIOp::DEC_NEW, reg, (uint8_t)(p&0xFF), (uint8_t)(p>>8) });
                        }
                        count++;
                    }
                    emit({ AIOp::MAKE_LIST, dst, startReg, count });
                } else {
                    // Legacy string-based: emit as int constant
                    int64_t ival = 0;
                    try { ival = std::stoll(raw); } catch (...) {}
                    uint16_t pidx = mod.addInt(ival);
                    uint8_t tmp = tempReg();
                    emit({ AIOp::LOAD_CONST, tmp, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
                    emit({ AIOp::STORE_VAR, dst, tmp, 0 });
                }
            }
            break;
        }

        case NodeType::TypeCoerceStmt: {
            // x = dec expr / dec x [= expr] — cast and assign
            std::string kind = node.attrs.empty() ? "INT" : node.attrs[0];
            uint8_t reg = allocReg(node.value);
            if (!node.children.empty()) {
                // FLOAT literals compile to DEC_NEW (exact Dec), INT literals compile to LOAD_CONST
                uint8_t src = compileExpr(*node.children[0], kind == "DEC");
                emit({ AIOp::STORE_VAR, reg, src, 0 });
            } else {
                // No initial expression: coerce whatever is already in the variable
                uint8_t tmp = tempReg();
                if (kind == "DEC")
                    emit({ AIOp::INT_TO_DEC, tmp, reg, 0 });
                else if (kind == "INT")
                    emit({ AIOp::DEC_TO_INT, tmp, reg, 0 });
                else
                    tmp = reg;
                if (tmp != reg) emit({ AIOp::STORE_VAR, reg, tmp, 0 });
            }
            break;
        }

        case NodeType::PlusEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::ADD, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }
        case NodeType::MinusEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::SUB, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }
        case NodeType::MultiplyEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::AT_MUL, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }
        case NodeType::AtEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::AT_MUL, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }
        case NodeType::DivideEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::DIV, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }
        case NodeType::XorEqualStmt: {
            uint8_t dst = allocReg(node.value);
            uint8_t rhs = compileExpr(*node.children[0]);
            uint8_t tmp = tempReg();
            emit({ AIOp::XOR, tmp, dst, rhs });
            emit({ AIOp::STORE_VAR, dst, tmp, 0 });
            break;
        }

        case NodeType::DisplayStmt: {
            if (node.attrs.empty() && node.children.empty()) {
                // string literal in value
                uint16_t pidx = mod.addStr(node.value);
                uint8_t  tmp  = tempReg();
                emit({ AIOp::LOAD_CONST, tmp, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
                emit({ AIOp::DISPLAY, tmp, 0, 0 });
            } else if (!node.children.empty()) {
                uint8_t r = compileExpr(*node.children[0]);
                emit({ AIOp::DISPLAY, r, 0, 0 });
            }
            break;
        }

        case NodeType::MethodCall: {
            // Native library call as statement (result discarded)
            if (nativeFuncs.count(node.value)) {
                std::vector<const ASTNode*> argPtrs;
                for (auto& c : node.children) argPtrs.push_back(c.get());
                emitNativeCall(node.value, argPtrs);
                break;
            }
            if (node.value == "Term.display") {
                std::string args = node.attrs.empty() ? "" : node.attrs[0];
                if (!node.children.empty()) {
                    uint8_t r = compileExpr(*node.children[0]);
                    emit({ AIOp::DISPLAY, r, 0, 0 });
                } else if (!args.empty()) {
                    uint16_t pidx = mod.addStr(args);
                    uint8_t  tmp  = tempReg();
                    emit({ AIOp::LOAD_CONST, tmp, (uint8_t)(pidx&0xFF), (uint8_t)(pidx>>8) });
                    emit({ AIOp::DISPLAY, tmp, 0, 0 });
                }
            } else if (node.value == "Term.halt") {
                emit({ AIOp::HALT, 0, 0, 0 });
            } else {
                // Bundle/user method call as statement (result discarded)
                auto it = funcIndex.find(node.value);
                if (it != funcIndex.end()) {
                    uint8_t fnIdx = it->second;
                    uint8_t argc = (uint8_t)std::min(node.children.size(), (size_t)6);
                    std::vector<uint8_t> argRegs(argc);
                    for (uint8_t i = 0; i < argc; i++)
                        argRegs[i] = compileExpr(*node.children[i]);
                    for (uint8_t i = 0; i < argc; i++)
                        if (argRegs[i] != i) emit({ AIOp::STORE_VAR, i, argRegs[i], 0 });
                    emit({ AIOp::CALL, fnIdx, argc, 0 });
                }
            }
            break;
        }

        case NodeType::ReturnStmt: {
            uint8_t src = 0;
            if (!node.children.empty()) src = compileExpr(*node.children[0]);
            emit({ AIOp::RETURN, src, 0, 0 });
            break;
        }

        case NodeType::KillStmt:
            emit({ AIOp::HALT, 0, 0, 0 });
            break;

        case NodeType::PassStmt:
            emit({ AIOp::NOP, 0, 0, 0 });
            break;

        case NodeType::BreakStmt:
        case NodeType::SkipStmt: {
            uint32_t idx = emit({ AIOp::JMP, 0, 0, 0 });
            breakPatchList.push_back(idx);
            break;
        }

        case NodeType::ContinueStmt: {
            uint32_t idx = emit({ AIOp::JMP, 0, 0, 0 });
            contPatchList.push_back(idx);
            break;
        }

        case NodeType::IfStmt: {
            // Structure: children[0]=cond, children[1]=body, children[2..]=ElseIf/Other nodes
            std::vector<uint32_t> endJumps;

            auto emitBody = [&](const ASTNode& block) {
                for (auto& s : block.children) compileStmt(*s);
            };

            auto compileIfClause = [&](const ASTNode& clause) {
                // IfStmt("OTHER") has only children[0]=body
                if (clause.value == "OTHER" && clause.children.size() >= 1) {
                    emitBody(*clause.children[0]);
                    return;
                }
                // IfStmt("") or ElseIfStmt: children[0]=cond, children[1]=body
                if (clause.children.size() < 2) return;
                uint8_t condReg = compileExpr(*clause.children[0]);
                uint32_t jzIdx = emit({ AIOp::JZ, condReg, 0, 0 });
                emitBody(*clause.children[1]);
                uint32_t jmpEnd = emit({ AIOp::JMP, 0, 0, 0 });
                endJumps.push_back(jmpEnd);
                int32_t off = (int32_t)pc() - (int32_t)jzIdx;
                mod.patch(jzIdx, { AIOp::JZ, condReg, (uint8_t)(off & 0xFF), (uint8_t)((off >> 8) & 0xFF) });
            };

            // Main IF clause (children[0]=cond, children[1]=body)
            compileIfClause(node);

            // ELSEIF/OTHER stored as children[2..] of the main IF node
            for (size_t i = 2; i < node.children.size(); i++)
                compileIfClause(*node.children[i]);

            // Patch all end-jumps to current pc
            for (uint32_t idx : endJumps) {
                int32_t off = (int32_t)pc() - (int32_t)idx;
                mod.patch(idx, { AIOp::JMP, (uint8_t)(off & 0xFF), (uint8_t)((off >> 8) & 0xFF), (uint8_t)((off >> 16) & 0xFF) });
            }
            break;
        }

        case NodeType::WhilstLoop: {
            // children[0] = condition expr, children[1] = body (Block)
            uint32_t loopStart = pc();
            contTargets.push_back(loopStart);

            // Evaluate condition
            uint8_t condReg = node.children.size() >= 1
                ? compileExpr(*node.children[0])
                : [&]{ uint8_t r = tempReg(); uint16_t p = mod.addInt(1);
                       emit({AIOp::LOAD_CONST,r,(uint8_t)(p&0xFF),(uint8_t)(p>>8)}); return r; }();
            uint32_t jzIdx = emit({ AIOp::JZ, condReg, 0, 0 });

            // Body (children[1] is a Block)
            size_t bpSize = breakPatchList.size();
            size_t cpSize = contPatchList.size();
            if (node.children.size() >= 2 && node.children[1])
                for (auto& c : node.children[1]->children) compileStmt(*c);

            // Jump back to loop start: offset = loopStart - pc() (before emitting JMP)
            int32_t backOff = (int32_t)loopStart - (int32_t)pc();
            emit({ AIOp::JMP, (uint8_t)(backOff & 0xFF), (uint8_t)((backOff >> 8) & 0xFF), (uint8_t)((backOff >> 16) & 0xFF) });

            uint32_t loopEnd = pc();
            // Patch exit JZ: offset = loopEnd - jzIdx
            int32_t exitOff = (int32_t)loopEnd - (int32_t)jzIdx;
            mod.patch(jzIdx, { AIOp::JZ, condReg, (uint8_t)(exitOff & 0xFF), (uint8_t)((exitOff >> 8) & 0xFF) });

            // Patch break jumps
            for (size_t i = bpSize; i < breakPatchList.size(); i++) {
                int32_t off = (int32_t)loopEnd - (int32_t)breakPatchList[i];
                mod.patch(breakPatchList[i], { AIOp::JMP, (uint8_t)(off & 0xFF), (uint8_t)((off >> 8) & 0xFF), (uint8_t)((off >> 16) & 0xFF) });
            }
            breakPatchList.resize(bpSize);

            // Patch continue jumps
            for (size_t i = cpSize; i < contPatchList.size(); i++) {
                int32_t off = (int32_t)loopStart - (int32_t)contPatchList[i];
                mod.patch(contPatchList[i], { AIOp::JMP, (uint8_t)(off & 0xFF), (uint8_t)((off >> 8) & 0xFF), (uint8_t)((off >> 16) & 0xFF) });
            }
            contPatchList.resize(cpSize);
            contTargets.pop_back();
            break;
        }

        case NodeType::ForLoop: {
            // FOR item in list — emit as: counter = 0; whilst counter < len
            std::string loopVar  = node.value;
            std::string collName = node.attrs.empty() ? "" : node.attrs[0];

            uint8_t counterReg = tempReg();
            uint8_t itemReg    = allocReg(loopVar);
            uint8_t listReg    = locals.count(collName) ? locals[collName] : tempReg();

            uint16_t zero = mod.addInt(0), one = mod.addInt(1);
            emit({ AIOp::LOAD_CONST, counterReg, (uint8_t)(zero&0xFF), (uint8_t)(zero>>8) });

            uint32_t loopStart = pc();
            contTargets.push_back(loopStart);

            // len = list[0] — for now just use length heuristic
            uint8_t condReg = tempReg();
            emit({ AIOp::LOAD_CONST, condReg, (uint8_t)(zero&0xFF), (uint8_t)(zero>>8) });
            uint32_t jzIdx = emit({ AIOp::JZ, condReg, 0, 0 });

            // item = list[counter]
            emit({ AIOp::LIST_GET, itemReg, listReg, counterReg });

            // body is children[1] (children[0] is the collection expr)
            size_t bpSize = breakPatchList.size();
            size_t cpSize = contPatchList.size();
            if (node.children.size() >= 2 && node.children[1])
                for (auto& c : node.children[1]->children) compileStmt(*c);

            // counter++
            uint8_t tmpInc = tempReg();
            uint16_t onePidx = mod.addInt(1);
            emit({ AIOp::LOAD_CONST, tmpInc, (uint8_t)(onePidx&0xFF), (uint8_t)(onePidx>>8) });
            emit({ AIOp::ADD, counterReg, counterReg, tmpInc });

            int32_t backOff = (int32_t)loopStart - (int32_t)pc();
            emit({ AIOp::JMP, (uint8_t)(backOff&0xFF), (uint8_t)((backOff>>8)&0xFF), (uint8_t)((backOff>>16)&0xFF) });

            uint32_t loopEnd = pc();
            int32_t exitOff  = (int32_t)loopEnd - (int32_t)jzIdx;
            mod.patch(jzIdx, { AIOp::JZ, condReg, (uint8_t)(exitOff&0xFF), (uint8_t)((exitOff>>8)&0xFF) });

            for (size_t i = bpSize; i < breakPatchList.size(); i++) {
                int32_t off = (int32_t)loopEnd - (int32_t)breakPatchList[i];
                mod.patch(breakPatchList[i], { AIOp::JMP, (uint8_t)(off&0xFF), (uint8_t)((off>>8)&0xFF), (uint8_t)((off>>16)&0xFF) });
            }
            breakPatchList.resize(bpSize);
            for (size_t i = cpSize; i < contPatchList.size(); i++) {
                int32_t off = (int32_t)loopStart - (int32_t)contPatchList[i];
                mod.patch(contPatchList[i], { AIOp::JMP, (uint8_t)(off&0xFF), (uint8_t)((off>>8)&0xFF), (uint8_t)((off>>16)&0xFF) });
            }
            contPatchList.resize(cpSize);
            contTargets.pop_back();
            break;
        }

        case NodeType::Block:
            for (auto& c : node.children) compileStmt(*c);
            break;

        case NodeType::TagBlock:
            if (node.value == "mainloop") {
                uint32_t loopStart = pc();
                for (auto& c : node.children) compileStmt(*c);
                int32_t backOff = (int32_t)loopStart - (int32_t)pc();
                emit({ AIOp::JMP, (uint8_t)(backOff&0xFF), (uint8_t)((backOff>>8)&0xFF), (uint8_t)((backOff>>16)&0xFF) });
            } else {
                for (auto& c : node.children) compileStmt(*c);
            }
            break;

        case NodeType::FuncDef:
        case NodeType::BundleDef:
            break;  // handled by top-level pass

        default: break;
        }
    }

    // ── TOP-LEVEL COMPILE ──────────────────────────────────────────────────
    void compileFunction(const ASTNode& funcNode, const std::string& name) {
        locals.clear();
        nextReg = 0;
        funcStart = pc();

        uint16_t nameRef = mod.addStr(name);

        // Params arrive in regs 0..argc-1 but CALL clobbers reg 0 with the return value.
        // Copy each param to a "safe" register at argc+i so recursive calls don't corrupt them.
        // Reserve reg[0] as the implicit call return slot; named vars start at reg[1] minimum.
        uint8_t argc = (uint8_t)funcNode.attrs.size();
        for (uint8_t i = 0; i < argc; i++)
            locals[funcNode.attrs[i]] = argc + i;
        nextReg = argc == 0 ? 1 : argc * 2;
        for (uint8_t i = 0; i < argc; i++)
            emit({ AIOp::STORE_VAR, (uint8_t)(argc + i), i, 0 });

        // Emit body
        if (!funcNode.children.empty())
            for (auto& c : funcNode.children[0]->children) compileStmt(*c);

        // Implicit return 0
        uint16_t zero = mod.addInt(0);
        uint8_t  tmp  = tempReg();
        emit({ AIOp::LOAD_CONST, tmp, (uint8_t)(zero&0xFF), (uint8_t)(zero>>8) });
        emit({ AIOp::RETURN, tmp, 0, 0 });

        uint32_t funcEnd = pc();
        AiBcFunc fn;
        fn.name_ref    = nameRef;
        fn.bc_offset   = funcStart;
        fn.bc_len      = funcEnd - funcStart;
        fn.local_count = nextReg;
        mod.funcs.push_back(fn);
    }

    void compileProgram(const ASTNode& root) {
        // Pre-scan: register native functions for each ilib import
        for (auto& c : root.children) {
            if (!c || c->type != NodeType::UseLibStmt) continue;
            if (c->value == "ilib:math")
                for (auto& s : MATH_NATIVE_FUNS)   nativeFuncs.insert(s);
            if (c->value == "ilib:camera")
                for (auto& s : CAMERA_NATIVE_FUNS) nativeFuncs.insert(s);
        }

        // First pass: collect function names and assign indices
        // main will be inserted at index 0, so user funcs start at 1
        uint8_t nextFnIdx = 1;
        funcIndex["main"] = 0;

        // Collect bundle methods first (as BundleName.methodName), skip recurse into bundles
        std::function<void(const ASTNode&)> collectBundles = [&](const ASTNode& n) {
            if (n.type == NodeType::BundleDef) {
                for (auto& block : n.children)           // one Block child
                    for (auto& method : block->children) // FuncDef nodes
                        if (method->type == NodeType::FuncDef) {
                            std::string fullName = n.value + "." + method->value;
                            mod.addStr(fullName);
                            if (!funcIndex.count(fullName))
                                funcIndex[fullName] = nextFnIdx++;
                        }
                return; // don't recurse into bundle
            }
            for (auto& c : n.children) collectBundles(*c);
        };
        collectBundles(root);

        // Collect standalone function names (skip inside bundles)
        std::function<void(const ASTNode&)> collectNames = [&](const ASTNode& n) {
            if (n.type == NodeType::BundleDef) return;
            if (n.type == NodeType::FuncDef) {
                mod.addStr(n.value);
                if (!funcIndex.count(n.value))
                    funcIndex[n.value] = nextFnIdx++;
            }
            for (auto& c : n.children) collectNames(*c);
        };
        collectNames(root);

        // Second pass: compile bundle methods then standalone functions
        std::function<void(const ASTNode&)> compileFuncs = [&](const ASTNode& n) {
            if (n.type == NodeType::BundleDef) {
                for (auto& block : n.children)
                    for (auto& method : block->children)
                        if (method->type == NodeType::FuncDef)
                            compileFunction(*method, n.value + "." + method->value);
                return;
            }
            if (n.type == NodeType::FuncDef) compileFunction(n, n.value);
            for (auto& c : n.children) compileFuncs(*c);
        };
        compileFuncs(root);

        // Third pass: compile top-level / mainloop as "main" function
        // Reserve reg[0] for the CALL return slot; named variables start at reg[1].
        locals.clear(); nextReg = 1;
        funcStart = pc();
        uint16_t nameRef = mod.addStr("main");

        for (auto& c : root.children) {
            if (c->type != NodeType::FuncDef && c->type != NodeType::BundleDef)
                compileStmt(*c);
        }

        uint16_t zero = mod.addInt(0);
        uint8_t  tmp  = tempReg();
        emit({ AIOp::LOAD_CONST, tmp, (uint8_t)(zero&0xFF), (uint8_t)(zero>>8) });
        emit({ AIOp::RETURN, tmp, 0, 0 });

        AiBcFunc mainFn;
        mainFn.name_ref    = nameRef;
        mainFn.bc_offset   = funcStart;
        mainFn.bc_len      = pc() - funcStart;
        mainFn.local_count = nextReg;
        mod.funcs.insert(mod.funcs.begin(), mainFn);  // main is always index 0
    }
};

// ── SERIALIZER / DESERIALIZER ─────────────────────────────────────────────

static void writeU8(std::string& out, uint8_t v)  { out += (char)v; }
static void writeU16(std::string& out, uint16_t v) { out += (char)(v & 0xFF); out += (char)(v >> 8); }
static void writeU32(std::string& out, uint32_t v) {
    out += (char)(v & 0xFF); out += (char)((v >> 8) & 0xFF);
    out += (char)((v >> 16) & 0xFF); out += (char)((v >> 24) & 0xFF);
}
static void writeI64(std::string& out, int64_t v) {
    for (int i = 0; i < 8; i++) { out += (char)(v & 0xFF); v >>= 8; }
}
static void writeStr(std::string& out, const std::string& s) {
    writeU16(out, (uint16_t)s.size());
    out += s;
}

static uint8_t  readU8(const std::string& d, size_t& p)  { return (uint8_t)d[p++]; }
static uint16_t readU16(const std::string& d, size_t& p) { uint16_t v = (uint8_t)d[p] | ((uint8_t)d[p+1]<<8); p+=2; return v; }
static uint32_t readU32(const std::string& d, size_t& p) { uint32_t v=(uint8_t)d[p]|((uint8_t)d[p+1]<<8)|((uint8_t)d[p+2]<<16)|((uint8_t)d[p+3]<<24); p+=4; return v; }
static int64_t  readI64(const std::string& d, size_t& p) { int64_t v=0; for(int i=0;i<8;i++) v|=((int64_t)(uint8_t)d[p+i]<<(i*8)); p+=8; return v; }
static std::string readStr(const std::string& d, size_t& p) { uint16_t n=readU16(d,p); std::string s=d.substr(p,n); p+=n; return s; }

std::string AiBcModule::serialize() const {
    std::string out;
    // Magic + version + flags
    out += (char)0x41; out += (char)0x49; out += (char)0x42; out += (char)0x43;
    writeU16(out, AIBC_VERSION);
    writeU16(out, 0);  // flags

    // Constant pool
    writeU16(out, (uint16_t)pool.size());
    for (auto& c : pool) {
        writeU8(out, c.type);
        if      (c.type == AIBC_CONST_INT) writeI64(out, c.ival);
        else if (c.type == AIBC_CONST_STR || c.type == AIBC_CONST_DEC) writeStr(out, c.sval);
    }

    // Function table
    writeU16(out, (uint16_t)funcs.size());
    for (auto& f : funcs) {
        writeU16(out, f.name_ref);
        writeU32(out, f.bc_offset);
        writeU32(out, f.bc_len);
        writeU16(out, f.local_count);
    }

    // Bytecode
    writeU32(out, (uint32_t)bytecode.size());
    for (uint32_t instr : bytecode) writeU32(out, instr);

    return out;
}

AiBcModule AiBcModule::deserialize(const std::string& data) {
    AiBcModule mod;
    size_t p = 0;
    if (data.size() < 8) return mod;

    // Magic check
    if (data[0]!=(char)0x41||data[1]!=(char)0x49||data[2]!=(char)0x42||data[3]!=(char)0x43) {
        std::cerr << "Preposterous: not a valid .aibc file\n"; return mod;
    }
    p = 4;
    readU16(data, p); // version
    readU16(data, p); // flags

    uint16_t poolCount = readU16(data, p);
    for (uint16_t i = 0; i < poolCount; i++) {
        AiBcConst c;
        c.type = readU8(data, p);
        if      (c.type == AIBC_CONST_INT) c.ival = readI64(data, p);
        else if (c.type == AIBC_CONST_STR || c.type == AIBC_CONST_DEC) c.sval = readStr(data, p);
        mod.pool.push_back(c);
    }

    uint16_t fnCount = readU16(data, p);
    for (uint16_t i = 0; i < fnCount; i++) {
        AiBcFunc f;
        f.name_ref    = readU16(data, p);
        f.bc_offset   = readU32(data, p);
        f.bc_len      = readU32(data, p);
        f.local_count = readU16(data, p);
        mod.funcs.push_back(f);
    }

    uint32_t bcCount = readU32(data, p);
    for (uint32_t i = 0; i < bcCount; i++) mod.bytecode.push_back(readU32(data, p));

    return mod;
}

// ── PUBLIC ENTRY POINT (AIBC) ──────────────────────────────────────────────
AiBcModule compileToAibc(const ASTNode& ast) {
    AiBcModule mod;
    AICodeGen  gen(mod);
    gen.compileProgram(ast);
    return mod;
}

