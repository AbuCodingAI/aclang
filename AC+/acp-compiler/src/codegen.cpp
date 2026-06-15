#include "../include/ast.hpp"
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdexcept>

// ── Register alias tables ─────────────────────────────────────────────────────

static const std::unordered_map<std::string, std::string> REG_MAP = {
    {"64box1",      "%rdi"}, {"64box2",      "%rsi"}, {"64box3",      "%rdx"},
    {"64box4",      "%rcx"}, {"64box5",      "%r8"},  {"64box6",      "%r9"},
    {"64vault",     "%rax"}, {"64base",      "%rbx"},
    {"64stack_pin", "%rsp"}, {"64frame_pin", "%rbp"},
    {"64scratch1",  "%r10"}, {"64scratch2",  "%r11"},
    {"64saved1",    "%r12"}, {"64saved2",    "%r13"},
    {"64saved3",    "%r14"}, {"64saved4",    "%r15"},
    {"32box1",      "%edi"}, {"32box2",      "%esi"}, {"32box3",      "%edx"},
    {"32box4",      "%ecx"}, {"32box5",      "%r8d"}, {"32box6",      "%r9d"},
    {"32vault",     "%eax"}, {"32base",      "%ebx"},
    {"32stack_pin", "%esp"}, {"32frame_pin", "%ebp"},
    {"32scratch1",  "%r10d"},{"32scratch2",  "%r11d"},
    {"32saved1",    "%r12d"},{"32saved2",    "%r13d"},
    {"32saved3",    "%r14d"},{"32saved4",    "%r15d"},
};

// Map any register to its 8-bit low byte
static const std::unordered_map<std::string, std::string> BYTE_REG = {
    {"%rdi","%dil"},{"%rsi","%sil"},{"%rdx","%dl"}, {"%rcx","%cl"},
    {"%r8", "%r8b"},{"%r9", "%r9b"},{"%rax","%al"}, {"%rbx","%bl"},
    {"%r10","%r10b"},{"%r11","%r11b"},{"%r12","%r12b"},{"%r13","%r13b"},
    {"%r14","%r14b"},{"%r15","%r15b"},{"%rsp","%spl"},{"%rbp","%bpl"},
    // 32-bit aliases → same byte regs
    {"%edi","%dil"},{"%esi","%sil"},{"%edx","%dl"}, {"%ecx","%cl"},
    {"%r8d","%r8b"},{"%r9d","%r9b"},{"%eax","%al"}, {"%ebx","%bl"},
    {"%r10d","%r10b"},{"%r11d","%r11b"},
};

static std::string byteOf(const std::string& reg) {
    auto it = BYTE_REG.find(reg);
    return (it != BYTE_REG.end()) ? it->second : "%r10b";
}

// "q" for 64-bit alias, "l" for 32-bit
static std::string sizeSuf(const std::string& acpName) {
    if (acpName.size() >= 2 && acpName[0] == '3') return "l";
    return "q";
}

// ── Codegen struct ────────────────────────────────────────────────────────────

struct Codegen {
    std::ostringstream out;
    int labelN = 0;

    // Per-function state
    std::unordered_map<std::string, int> stackVars; // name → rbp offset (negative)
    int nextOff = 0;

    static bool isReg(const std::string& name) { return REG_MAP.count(name) > 0; }
    static std::string mapReg(const std::string& name) {
        auto it = REG_MAP.find(name);
        return (it != REG_MAP.end()) ? it->second : "";
    }

    std::string lbl(const std::string& pfx = ".Lacp") {
        return pfx + std::to_string(labelN++);
    }

    // ── genExpr: loads expr into `dst` register ───────────────────────────────
    // Uses %r10 for lhs of binary exprs, %r11 for rhs.
    // NOTE: binary expression depth > 1 is unsupported (fine for AOS).
    void genExpr(const ASTNode& n, const std::string& dst) {
        switch (n.type) {
        case NodeType::NumberLit:
            out << "    movq $" << n.value << ", " << dst << "\n";
            break;
        case NodeType::NilLit:
            out << "    xorq " << dst << ", " << dst << "\n";
            break;
        case NodeType::IdentExpr: {
            std::string reg = mapReg(n.value);
            if (!reg.empty()) {
                if (reg != dst) out << "    movq " << reg << ", " << dst << "\n";
            } else {
                auto it = stackVars.find(n.value);
                if (it != stackVars.end())
                    out << "    movq " << it->second << "(%rbp), " << dst << "\n";
                else
                    out << "    xorq " << dst << ", " << dst << "  /* unknown: " << n.value << " */\n";
            }
            break;
        }
        case NodeType::LocateExpr: {
            auto it = stackVars.find(n.value);
            if (it != stackVars.end())
                out << "    leaq " << it->second << "(%rbp), " << dst << "\n";
            else
                out << "    xorq " << dst << ", " << dst << "  /* locate: unknown " << n.value << " */\n";
            break;
        }
        case NodeType::BinaryExpr: {
            genExpr(*n.children[0], "%r10");
            genExpr(*n.children[1], "%r11");
            const std::string& op = n.value;
            if      (op == "+") out << "    addq  %r11, %r10\n";
            else if (op == "-") out << "    subq  %r11, %r10\n";
            else if (op == "*" || op == "@") out << "    imulq %r11, %r10\n";
            if (dst != "%r10") out << "    movq %r10, " << dst << "\n";
            break;
        }
        case NodeType::CallExpr:
            genCall(n);
            if (dst != "%rax") out << "    movq %rax, " << dst << "\n";
            break;
        default:
            out << "    xorq " << dst << ", " << dst << "\n";
            break;
        }
    }

    // ── genCond: emit cmp + false-jump ────────────────────────────────────────
    // Jumps to `falseLabel` when condition evaluates to FALSE.
    // #> = not-greater-than = <= (LESS_EQUAL semantics)
    // #< = not-less-than   = >= (GREATER_EQUAL semantics)
    void genCond(const ASTNode& cond, const std::string& falseLabel) {
        if (cond.type != NodeType::BinaryExpr) {
            genExpr(cond, "%r10");
            out << "    testq %r10, %r10\n";
            out << "    jz    " << falseLabel << "\n";
            return;
        }
        const std::string& op = cond.value;
        genExpr(*cond.children[0], "%r10");
        genExpr(*cond.children[1], "%r11");
        out << "    cmpq  %r11, %r10\n";
        if      (op == "is") out << "    jne   " << falseLabel << "\n";
        else if (op == "#=") out << "    je    " << falseLabel << "\n";
        else if (op == "<")  out << "    jge   " << falseLabel << "\n";
        else if (op == ">")  out << "    jle   " << falseLabel << "\n";
        else if (op == "#>") out << "    jg    " << falseLabel << "\n"; // #> = <=, exit if >
        else if (op == "#<") out << "    jl    " << falseLabel << "\n"; // #< = >=, exit if <
        else                 out << "    jne   " << falseLabel << "\n";
    }

    // ── genCall: push args to calling-convention registers, call ─────────────
    void genCall(const ASTNode& n) {
        static const char* argRegs[] = {"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
        int argc = (int)n.children.size();
        for (int i = 0; i < argc && i < 6; i++)
            genExpr(*n.children[i], argRegs[i]);
        out << "    call  " << n.value << "\n";
    }

    // ── genBlock / genStmt ────────────────────────────────────────────────────

    void genBlock(const ASTNode& block) {
        for (auto& ch : block.children)
            if (ch) genStmt(*ch);
    }

    void genStmt(const ASTNode& n) {
        switch (n.type) {
        case NodeType::Block:
            genBlock(n);
            break;

        // reg 64box1 = expr  →  movq $val, %rdi
        case NodeType::RegDecl: {
            std::string reg = mapReg(n.value);
            if (reg.empty()) break;
            std::string suf = sizeSuf(n.value);
            if (!n.children.empty()) {
                if (n.children[0]->type == NodeType::NumberLit)
                    out << "    mov" << suf << " $" << n.children[0]->value << ", " << reg << "\n";
                else {
                    genExpr(*n.children[0], "%r10");
                    out << "    mov" << suf << " %r10" << (suf=="l"?"d":"") << ", " << reg << "\n";
                }
            } else {
                out << "    xor" << suf << " " << reg << ", " << reg << "\n";
            }
            break;
        }

        // stack varname type  — space already reserved in prologue, no runtime work
        case NodeType::StackDecl:
            break;

        // 64box1 << value   or   64box1 + offset << value
        case NodeType::MemWrite: {
            std::string addrReg = mapReg(n.value);
            if (addrReg.empty()) break;
            bool hasOffset = (!n.attrs.empty() && n.attrs[0] == "offset");

            if (hasOffset) {
                // children[0]=offset, children[1]=value
                // If offset is a constant, fold it into the address expression
                const ASTNode& offNode = *n.children[0];
                const ASTNode& valNode = *n.children[1];

                if (offNode.type == NodeType::NumberLit) {
                    // Constant offset: movb %valReg, N(addrReg)
                    std::string valReg = mapReg(valNode.type == NodeType::IdentExpr ? valNode.value : "");
                    if (!valReg.empty()) {
                        out << "    movb  " << byteOf(valReg) << ", " << offNode.value << "(" << addrReg << ")\n";
                    } else {
                        genExpr(valNode, "%r10");
                        out << "    movb  %r10b, " << offNode.value << "(" << addrReg << ")\n";
                    }
                } else {
                    // Variable offset: use indexed addressing
                    genExpr(offNode, "%r11");
                    genExpr(valNode, "%r10");
                    out << "    movb  %r10b, (" << addrReg << ",%r11)\n";
                }
            } else {
                // No offset: children[0] = value
                const ASTNode& valNode = *n.children[0];
                std::string valReg = mapReg(valNode.type == NodeType::IdentExpr ? valNode.value : "");
                if (!valReg.empty()) {
                    out << "    movb  " << byteOf(valReg) << ", (" << addrReg << ")\n";
                } else {
                    genExpr(valNode, "%r10");
                    out << "    movb  %r10b, (" << addrReg << ")\n";
                }
            }
            break;
        }

        // reg += expr  or  plain_var += expr
        case NodeType::CompoundAssign: {
            std::string op  = n.attrs.empty() ? "+=" : n.attrs[0];
            std::string dst = mapReg(n.value);
            std::string suf = sizeSuf(n.value);

            if (!dst.empty()) {
                genExpr(*n.children[0], "%r10");
                std::string r10sz = (suf == "l") ? "%r10d" : "%r10";
                if      (op == "+=") out << "    add" << suf << " " << r10sz << ", " << dst << "\n";
                else if (op == "-=") out << "    sub" << suf << " " << r10sz << ", " << dst << "\n";
                else if (op == "*=" || op == "@=") out << "    imul" << suf << " " << r10sz << ", " << dst << "\n";
            } else {
                auto it = stackVars.find(n.value);
                if (it != stackVars.end()) {
                    genExpr(*n.children[0], "%r11");
                    out << "    movq  " << it->second << "(%rbp), %r10\n";
                    if      (op == "+=") out << "    addq  %r11, %r10\n";
                    else if (op == "-=") out << "    subq  %r11, %r10\n";
                    else if (op == "*=" || op == "@=") out << "    imulq %r11, %r10\n";
                    out << "    movq  %r10, " << it->second << "(%rbp)\n";
                }
            }
            break;
        }

        // var = expr
        case NodeType::AssignStmt: {
            std::string dst = mapReg(n.value);
            if (!dst.empty()) {
                std::string suf = sizeSuf(n.value);
                if (n.children[0]->type == NodeType::NumberLit)
                    out << "    mov" << suf << " $" << n.children[0]->value << ", " << dst << "\n";
                else {
                    genExpr(*n.children[0], "%r10");
                    out << "    mov" << suf << " %r10" << (suf=="l"?"d":"") << ", " << dst << "\n";
                }
            } else {
                auto it = stackVars.find(n.value);
                if (it != stackVars.end()) {
                    genExpr(*n.children[0], "%r10");
                    out << "    movq  %r10, " << it->second << "(%rbp)\n";
                }
            }
            break;
        }

        // IF cond block [ELSEIF cond block]* [OTHER block]
        case NodeType::IfStmt: {
            std::string endLbl  = lbl(".Lif_end");
            std::string elseLbl = lbl(".Lif_else");

            genCond(*n.children[0], elseLbl);
            genBlock(*n.children[1]);
            out << "    jmp   " << endLbl << "\n";
            out << elseLbl << ":\n";

            for (size_t i = 2; i < n.children.size(); i++) {
                const ASTNode& br = *n.children[i];
                if (br.value == "other") {
                    genBlock(*br.children[0]);
                } else {
                    // ELSEIF
                    std::string eiEnd = lbl(".Lif_else");
                    genCond(*br.children[0], eiEnd);
                    genBlock(*br.children[1]);
                    out << "    jmp   " << endLbl << "\n";
                    out << eiEnd << ":\n";
                }
            }
            out << endLbl << ":\n";
            break;
        }

        // WHILST cond body
        case NodeType::WhilstStmt: {
            std::string loopLbl = lbl(".Lwhile");
            std::string endLbl  = lbl(".Lwhile_end");
            out << loopLbl << ":\n";
            genCond(*n.children[0], endLbl);
            genBlock(*n.children[1]);
            out << "    jmp   " << loopLbl << "\n";
            out << endLbl << ":\n";
            break;
        }

        // FOR var in range N
        case NodeType::ForStmt: {
            std::string loopLbl = lbl(".Lfor");
            std::string endLbl  = lbl(".Lfor_end");
            auto it = stackVars.find(n.value);
            if (it == stackVars.end()) break;
            int coff = it->second;

            out << "    movq  $0, " << coff << "(%rbp)\n";
            genExpr(*n.children[0], "%r11");
            out << loopLbl << ":\n";
            out << "    movq  " << coff << "(%rbp), %r10\n";
            out << "    cmpq  %r11, %r10\n";
            out << "    jge   " << endLbl << "\n";
            genBlock(*n.children[1]);
            out << "    incq  " << coff << "(%rbp)\n";
            out << "    jmp   " << loopLbl << "\n";
            out << endLbl << ":\n";
            break;
        }

        // return expr  or  return nil
        case NodeType::ReturnStmt:
            if (!n.children.empty() && n.children[0]->type != NodeType::NilLit)
                genExpr(*n.children[0], "%rax");
            out << "    movq  %rbp, %rsp\n";
            out << "    popq  %rbp\n";
            out << "    ret\n";
            break;

        // /kill /stop → hlt   /end → ret
        case NodeType::HaltStmt:
            if (n.value == "end")
                out << "    ret\n";
            else
                out << "    hlt\n";
            break;

        case NodeType::CallExpr:
            genCall(n);
            break;

        default:
            break;
        }
    }

    // ── Pre-scan: count stack slots needed in a function body ─────────────────
    int preScan(const ASTNode& body, const std::vector<std::string>& params) {
        std::unordered_set<std::string> seen(params.begin(), params.end());
        int count = (int)params.size();
        std::function<void(const ASTNode&)> walk = [&](const ASTNode& nd) {
            if (nd.type == NodeType::StackDecl && !seen.count(nd.value)) {
                seen.insert(nd.value); count++;
            }
            if (nd.type == NodeType::AssignStmt && !isReg(nd.value) && !seen.count(nd.value)) {
                seen.insert(nd.value); count++;
            }
            if (nd.type == NodeType::ForStmt && !seen.count(nd.value)) {
                seen.insert(nd.value); count++;
            }
            for (auto& c : nd.children) if (c) walk(*c);
        };
        walk(body);
        return count;
    }

    // ── Pre-allocate all local / stack variables before codegen ──────────────
    void preAlloc(const ASTNode& body, const std::vector<std::string>& params) {
        static const char* pRegs[] = {"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
        for (int i = 0; i < (int)params.size() && i < 6; i++) {
            nextOff -= 8;
            stackVars[params[i]] = nextOff;
            out << "    movq  " << pRegs[i] << ", " << nextOff << "(%rbp)\n";
        }
        std::function<void(const ASTNode&)> walk = [&](const ASTNode& nd) {
            bool alloc = false;
            if (nd.type == NodeType::StackDecl) alloc = true;
            if (nd.type == NodeType::AssignStmt && !isReg(nd.value)) alloc = true;
            if (nd.type == NodeType::ForStmt) alloc = true;
            if (alloc && !stackVars.count(nd.value)) {
                nextOff -= 8;
                stackVars[nd.value] = nextOff;
            }
            for (auto& c : nd.children) if (c) walk(*c);
        };
        walk(body);
    }

    // ── Emit function prologue/body/epilogue ──────────────────────────────────
    void genFunc(const ASTNode& n, bool global) {
        stackVars.clear();
        nextOff = 0;
        const ASTNode& body = *n.children[0];

        int slots = preScan(body, n.attrs);
        int frame = (((slots * 8) + 15) / 16) * 16;
        if (frame < 16) frame = 16;

        if (global) out << "    .global " << n.value << "\n";
        out << n.value << ":\n";
        out << "    pushq %rbp\n";
        out << "    movq  %rsp, %rbp\n";
        out << "    subq  $" << frame << ", %rsp\n";

        preAlloc(body, n.attrs);
        genBlock(body);

        out << "    movq  %rbp, %rsp\n";
        out << "    popq  %rbp\n";
        out << "    ret\n";
    }

    // ── Emit __ai_main__ (mainloop entry point) ───────────────────────────────
    void genMain(const ASTNode& n) {
        stackVars.clear();
        nextOff = 0;
        const ASTNode& body = *n.children[0];

        int slots = preScan(body, {});
        int frame = (((slots * 8) + 15) / 16) * 16;
        if (frame < 16) frame = 16;

        out << "    .global __ai_main__\n";
        out << "__ai_main__:\n";
        out << "    pushq %rbp\n";
        out << "    movq  %rsp, %rbp\n";
        out << "    subq  $" << frame << ", %rsp\n";

        preAlloc(body, {});
        genBlock(body);

        out << "    movq  %rbp, %rsp\n";
        out << "    popq  %rbp\n";
        out << "    ret\n";
    }

    // ── Top-level entry ───────────────────────────────────────────────────────
    // __ai_main__ is emitted FIRST so its offset in the flat binary is 0x0.
    // All function calls (call boot_print etc.) use PC-relative addressing
    // resolved by 'as', so order in the .s file does not affect correctness.
    std::string generate(const ASTNode& root) {
        bool isLib = false;
        for (auto& ch : root.children)
            if (ch && ch->type == NodeType::BackendDecl && ch->value == "LIB")
                isLib = true;

        out << "    .text\n";

        // Pass 1: mainloop first → entry at offset 0
        for (auto& ch : root.children) {
            if (ch && ch->type == NodeType::MainLoop)
                genMain(*ch);
        }
        // Pass 2: functions
        for (auto& ch : root.children) {
            if (ch && ch->type == NodeType::FuncDef)
                genFunc(*ch, isLib);
        }
        return out.str();
    }
};

// ── Public API ────────────────────────────────────────────────────────────────

std::string generateASM(const ASTNode& root) {
    Codegen cg;
    return cg.generate(root);
}
