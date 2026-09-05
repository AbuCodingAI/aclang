/*
  exp_arm_asm.cpp — AC->ARM ASM: AArch64 GNU-assembler text output.

  Phase 2 of the ARM backend (see this file's header comment / exp_arm.hpp): same instruction
  selection, slot allocation, calling convention, and temp-elision optimization as exp_arm.cpp's
  raw-ELF binary backend — ported to text mnemonics instead of hand-encoded bytes. Every
  instruction shape here was cross-checked against exp_arm.cpp's own verified encodings (both
  ultimately trace back to the same aarch64-linux-gnu-as-verified session notes), and the
  generated output is assembled with the REAL toolchain, not hand-rolled — branches use
  symbolic labels the assembler/linker resolve, so none of exp_arm.cpp's relative-offset
  fixup/patching machinery is needed here at all.

  v1 scope: identical to exp_arm.cpp's — integers, arithmetic, comparisons, control flow,
  non-method function calls (including recursion), Term.display of an int. No strings, floats,
  arrays/dicts/bundles/classes, generators, try/catch, or ilib calls yet. Unsupported opcodes
  are a hard compile error, not a silent no-op (same reasoning as exp_arm.cpp).
*/
#include "../include/ac.hpp"
#include "../include/error.hpp"
#include <vector>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdint>

namespace AC_ArmAsmGen {

using namespace AC_IR;

enum class R : int {
    X0=0,X1=1,X2=2,X3=3,X4=4,X5=5,X6=6,X7=7,X8=8,X9=9,X10=10,X11=11,
    X12=12,X13=13,X14=14,X15=15,X16=16,X17=17,X18=18,X19=19,X20=20,
    X21=21,X22=22,X23=23,X24=24,X25=25,X26=26,X27=27,X28=28,X29=29,X30=30,
    SP=31
};
static std::string rn(R r) {
    if (r == R::SP) return "sp";
    return "x" + std::to_string((int)r);
}

enum class Cond { EQ, NE, GE, LT, GT, LE };
static const char* condStr(Cond c) {
    switch (c) {
        case Cond::EQ: return "eq"; case Cond::NE: return "ne";
        case Cond::GE: return "ge"; case Cond::LT: return "lt";
        case Cond::GT: return "gt"; default: return "le";
    }
}

// Same algorithm as exp_arm.cpp's computeElidableTemps — intentionally duplicated (small, pure,
// no shared header between the two backends yet) rather than forcing a shared-code dependency
// between two otherwise-independent files, matching this codebase's own established precedent
// for small verified helpers duplicated across sibling backend files (e.g. JS/HTML's ac_fmtg).
static std::set<int> computeElidableTemps(const std::vector<IRInstruction>& instrs) {
    std::map<int,int> useCount, defIdx, firstUseIdx;
    for (size_t i = 0; i < instrs.size(); i++) {
        const auto& ins = instrs[i];
        if (ins.result.kind == IRRef::Kind::TEMP) defIdx[ins.result.id] = (int)i;
        for (auto& op : ins.typedOperands) {
            if (op.kind == IRRef::Kind::TEMP) {
                useCount[op.id]++;
                if (!firstUseIdx.count(op.id)) firstUseIdx[op.id] = (int)i;
            }
        }
    }
    std::set<int> result;
    for (auto& [tid, di] : defIdx) {
        auto uc = useCount.find(tid);
        if (uc == useCount.end() || uc->second != 1) continue;
        auto fu = firstUseIdx.find(tid);
        if (fu != firstUseIdx.end() && fu->second == di + 1) result.insert(tid);
    }
    return result;
}

class ArmAsmCompiler {
    std::ostringstream out;
    const IRProgram& prog;
    std::string currentFuncName_;
    std::map<std::string,int> slotOf;
    int nextSlot = 0;
    int funcMaxSlot_ = -1;

    static const R GLOBALS_BASE = R::X28;
    static const R S0 = R::X9, S1 = R::X10, S2 = R::X11;
    static const R TCACHE = R::X16;
    std::set<int> elidableTemps_;
    int pendingTempId_ = -1;
    R pendingTempReg_ = S0;
    std::set<std::string> knownFuncs_; // real, compiled AC free functions — see CALL's own comment

    std::string keyFor(const IRRef& r) {
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return currentFuncName_ + "#" + kind + std::to_string(r.id);
    }
    int slotFor(const IRRef& r) {
        std::string k = keyFor(r);
        auto it = slotOf.find(k);
        if (it != slotOf.end()) return it->second;
        int s = nextSlot++;
        slotOf[k] = s;
        return s;
    }
    R currentBase() const { return currentFuncName_.empty() ? GLOBALS_BASE : R::SP; }
    std::string baseAddr(int slot) {
        return "[" + rn(currentBase()) + ", #" + std::to_string(slot*8) + "]";
    }

    // Real assembly labels: IR label ids restart per function (same collision class as temp
    // ids — see keyFor's own comment) and mainloop's own ids could collide with a function's,
    // so every label is qualified by its scope ("main" for the mainloop).
    std::string labelName(int id) const {
        return ".L" + (currentFuncName_.empty() ? std::string("main") : currentFuncName_)
             + "_" + std::to_string(id);
    }

    void ensureSpilled() {
        if (pendingTempId_ < 0) return;
        int tid = pendingTempId_;
        R reg = pendingTempReg_;
        pendingTempId_ = -1;
        IRRef t = IRRef::temp(tid);
        int s = slotFor(t);
        if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
        out << "    str " << rn(reg) << ", " << baseAddr(s) << "\n";
    }
    void protectPending(R dst) {
        if (pendingTempId_ >= 0 && pendingTempReg_ == dst) {
            out << "    mov " << rn(TCACHE) << ", " << rn(pendingTempReg_) << "\n";
            pendingTempReg_ = TCACHE;
        }
    }

    void movImm64(R dst, int64_t val) {
        uint64_t u = (uint64_t)val;
        out << "    movz " << rn(dst) << ", #" << (u & 0xFFFF) << "\n";
        if (((u>>16)&0xFFFF) != 0 || u > 0xFFFF)
            out << "    movk " << rn(dst) << ", #" << ((u>>16)&0xFFFF) << ", lsl #16\n";
        if (((u>>32)&0xFFFF) != 0)
            out << "    movk " << rn(dst) << ", #" << ((u>>32)&0xFFFF) << ", lsl #32\n";
        if (((u>>48)&0xFFFF) != 0)
            out << "    movk " << rn(dst) << ", #" << ((u>>48)&0xFFFF) << ", lsl #48\n";
    }

    void loadOperand(const IRRef& r, R dst) {
        if (r.kind == IRRef::Kind::TEMP && r.id == pendingTempId_) {
            if (dst != pendingTempReg_) out << "    mov " << rn(dst) << ", " << rn(pendingTempReg_) << "\n";
            pendingTempId_ = -1;
            return;
        }
        protectPending(dst);
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::INT) {
            movImm64(dst, std::get<int64_t>(r.value.data));
        } else if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::BOOL) {
            movImm64(dst, std::get<bool>(r.value.data) ? 1 : 0);
        } else if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) {
            int s = slotFor(r);
            if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
            out << "    ldr " << rn(dst) << ", " << baseAddr(s) << "\n";
        } else {
            movImm64(dst, 0);
        }
    }
    void storeResult(const IRRef& r, R src) {
        if (r.kind == IRRef::Kind::TEMP && elidableTemps_.count(r.id)) {
            ensureSpilled();
            pendingTempId_ = r.id;
            pendingTempReg_ = src;
            return;
        }
        ensureSpilled();
        int s = slotFor(r);
        if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
        out << "    str " << rn(src) << ", " << baseAddr(s) << "\n";
    }

    void compileCompare(Cond c, const IRInstruction& ins) {
        loadOperand(ins.typedOperands[0], S0);
        loadOperand(ins.typedOperands[1], S1);
        out << "    cmp " << rn(S0) << ", " << rn(S1) << "\n";
        out << "    cset " << rn(S0) << ", " << condStr(c) << "\n";
        storeResult(ins.result, S0);
    }

    void binOp(const char* mnem, const IRInstruction& ins) {
        loadOperand(ins.typedOperands[0], S0);
        loadOperand(ins.typedOperands[1], S1);
        out << "    " << mnem << " " << rn(S0) << ", " << rn(S0) << ", " << rn(S1) << "\n";
        storeResult(ins.result, S0);
    }

    void compileInstr(const IRInstruction& ins) {
        switch (ins.opcode) {
            case IROpcode::TAG_BEGIN: case IROpcode::TAG_END: case IROpcode::NOP: break;
            case IROpcode::LOAD_CONST:
                if (ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::FLOAT)
                    throw ACError::backend("ARM ASM backend: floating-point constants are not yet implemented");
                loadOperand(ins.typedOperands[0], S0);
                storeResult(ins.result, S0);
                break;
            case IROpcode::LABEL:
                out << labelName(ins.typedOperands[0].id) << ":\n";
                break;
            case IROpcode::JUMP:
                out << "    b " << labelName(ins.typedOperands[0].id) << "\n";
                break;
            case IROpcode::JUMP_IF_FALSE:
                loadOperand(ins.typedOperands[0], S0);
                out << "    cbz " << rn(S0) << ", " << labelName(ins.typedOperands[1].id) << "\n";
                break;
            case IROpcode::JUMP_IF_TRUE:
                loadOperand(ins.typedOperands[0], S0);
                out << "    cbnz " << rn(S0) << ", " << labelName(ins.typedOperands[1].id) << "\n";
                break;
            case IROpcode::STORE_VAR:
                if (ins.typedOperands.size() >= 2) {
                    loadOperand(ins.typedOperands[1], S0);
                    storeResult(ins.typedOperands[0], S0);
                } else if (!ins.typedOperands.empty()) {
                    loadOperand(ins.typedOperands[0], S0);
                    storeResult(ins.result, S0);
                }
                break;
            case IROpcode::ADD: binOp("add", ins); break;
            case IROpcode::SUB: binOp("sub", ins); break;
            case IROpcode::MUL: case IROpcode::PMUL:
                loadOperand(ins.typedOperands[0], S0);
                loadOperand(ins.typedOperands[1], S1);
                out << "    mul " << rn(S0) << ", " << rn(S0) << ", " << rn(S1) << "\n";
                storeResult(ins.result, S0);
                break;
            case IROpcode::DIV: case IROpcode::IDIV:
                loadOperand(ins.typedOperands[0], S0);
                loadOperand(ins.typedOperands[1], S1);
                out << "    sdiv " << rn(S0) << ", " << rn(S0) << ", " << rn(S1) << "\n";
                storeResult(ins.result, S0);
                break;
            case IROpcode::MOD:
                loadOperand(ins.typedOperands[0], S0);
                loadOperand(ins.typedOperands[1], S1);
                out << "    sdiv " << rn(S2) << ", " << rn(S0) << ", " << rn(S1) << "\n";
                out << "    msub " << rn(S0) << ", " << rn(S2) << ", " << rn(S1) << ", " << rn(S0) << "\n";
                storeResult(ins.result, S0);
                break;
            case IROpcode::BAND: binOp("and", ins); break;
            case IROpcode::BOR:  binOp("orr", ins); break;
            case IROpcode::BXOR: binOp("eor", ins); break;
            case IROpcode::BNOT:
                loadOperand(ins.typedOperands[0], S0);
                out << "    mvn " << rn(S0) << ", " << rn(S0) << "\n";
                storeResult(ins.result, S0);
                break;
            case IROpcode::PTM: binOp("lsl", ins); break;
            case IROpcode::PTD: binOp("lsr", ins); break;
            case IROpcode::EQ:  compileCompare(Cond::EQ, ins); break;
            case IROpcode::NEQ: compileCompare(Cond::NE, ins); break;
            case IROpcode::LT:  compileCompare(Cond::LT, ins); break;
            case IROpcode::GT:  compileCompare(Cond::GT, ins); break;
            case IROpcode::LTE: compileCompare(Cond::LE, ins); break;
            case IROpcode::GTE: compileCompare(Cond::GE, ins); break;
            case IROpcode::CALL: {
                static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                size_t nArgs = ins.typedOperands.size() - 1;
                if (nArgs > 8)
                    throw ACError::backend("ARM ASM backend: calls with more than 8 arguments are not yet implemented");
                for (size_t i = 0; i < nArgs; i++) loadOperand(ins.typedOperands[1+i], argRegs[i]);
                std::string calleeName = prog.symbols.getName(ins.typedOperands[0].id);
                // A real bug caught before it shipped: text output has no equivalent of
                // exp_arm.cpp's callFixups resolution pass (nothing here resolves branch
                // targets at all — the REAL assembler/linker does that job) — so emitting `bl`
                // to a name that isn't a real, compiled AC function used to "succeed" at
                // compile time and only fail at LINK time ("undefined reference to ac_ipow",
                // verified real: examples/power.ac's `^` operator lowers to a CALL of a
                // built-in `ac_ipow` helper this v1 slice doesn't implement on either ARM
                // backend). Check eagerly, matching exp_arm.cpp's own behavior, instead of
                // deferring to a toolchain step that isn't even always run (compile-only smoke
                // tests wouldn't have caught this).
                if (!knownFuncs_.count(calleeName))
                    throw ACError::backend("ARM ASM backend: call to unresolved function '" + calleeName
                        + "' (bundle methods, forward-declared-only functions, and built-in "
                          "runtime helpers like ac_ipow/ac_length are not yet implemented)");
                out << "    bl " << calleeName << "\n";
                if (ins.result.kind != IRRef::Kind::NONE) storeResult(ins.result, R::X0);
                break;
            }
            case IROpcode::RETURN:
                if (!ins.typedOperands.empty()) loadOperand(ins.typedOperands[0], R::X0);
                else ensureSpilled();
                out << "    mov sp, x29\n    ldp x29, x30, [sp], #16\n    ret\n";
                break;
            case IROpcode::PRINT:
                loadOperand(ins.typedOperands[0], R::X0);
                out << "    bl ac_print_int\n";
                break;
            case IROpcode::HALT:
                // Deliberate hard abort — matches every other backend's /kill (see exp_arm.cpp's
                // identical comment). getpid=172, kill=129 on the AArch64 Linux syscall table.
                out << "    mov x8, #172\n    svc #0\n";
                out << "    mov x1, #6\n    mov x8, #129\n    svc #0\n";
                break;
            case IROpcode::SOFT_HALT:
                out << "    mov x0, #0\n    mov x8, #94\n    svc #0\n";
                break;
            default:
                throw ACError::backend("ARM ASM backend: IR opcode " + std::to_string((int)ins.opcode)
                    + " is not yet implemented (see this file's header comment for current scope)");
        }
    }

    void emitPrintIntRoutine() {
        out << "ac_print_int:\n";
        out << "    stp x29, x30, [sp, #-16]!\n    mov x29, sp\n    sub sp, sp, #32\n";
        out << "    mov x12, x0\n    mov x13, #0\n";
        out << "    cmp x12, xzr\n    b.ge .Lpi_pos\n";
        out << "    mov x13, #1\n    neg x12, x12\n";
        out << ".Lpi_pos:\n";
        out << "    mov x14, sp\n    add x14, x14, #30\n    mov x15, #10\n";
        out << ".Lpi_loop:\n";
        out << "    sdiv x0, x12, x15\n    msub x1, x0, x15, x12\n    add x1, x1, #48\n";
        out << "    strb w1, [x14]\n    sub x14, x14, #1\n    mov x12, x0\n";
        out << "    cbnz x12, .Lpi_loop\n";
        out << "    cmp x13, xzr\n    b.eq .Lpi_nosign\n";
        out << "    mov x1, #45\n    strb w1, [x14]\n    sub x14, x14, #1\n";
        out << ".Lpi_nosign:\n";
        out << "    add x14, x14, #1\n";
        out << "    mov x1, sp\n    add x1, x1, #31\n    mov x2, #10\n    strb w2, [x1]\n";
        out << "    sub x2, x1, x14\n    add x2, x2, #1\n    mov x1, x14\n";
        out << "    mov x0, #1\n    mov x8, #64\n    svc #0\n";
        out << "    add sp, sp, #32\n    ldp x29, x30, [sp], #16\n    ret\n";
    }

    void compileFunction(const IRFunction& fn) {
        if (!fn.classOwner.empty()) return; // methods: not yet supported (matches exp_arm.cpp)
        currentFuncName_ = fn.name;
        funcMaxSlot_ = -1;
        elidableTemps_ = computeElidableTemps(fn.instructions);
        pendingTempId_ = -1;

        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
        if (fn.parameters.size() > 8)
            throw ACError::backend("ARM ASM backend: functions with more than 8 parameters are not yet implemented");

        // Body compiled into `out` first (temporarily emptied — see below), same reason as
        // exp_arm.cpp's placeholder-patch: the frame size (sub sp,sp,#N) isn't known until the
        // whole body (and its params) has assigned every slot it'll ever use. Text makes this
        // simpler than exp_arm.cpp's byte-patch: swap the accumulated program text out of `out`
        // into `saved`, let every existing emission helper (loadOperand/storeResult/
        // compileInstr, all of which just write through the `out` member directly) fill the
        // now-empty `out` with this function's body, capture that as a string, then restore
        // `saved` and splice prologue + body in the right order.
        std::ostringstream saved;
        saved.swap(out);

        for (size_t i = 0; i < fn.parameters.size(); i++) {
            int symId = -1;
            for (auto& ins : fn.instructions) {
                auto check = [&](const IRRef& r) {
                    if (r.kind == IRRef::Kind::VAR && r.id >= 0
                            && prog.symbols.getName(r.id) == fn.parameters[i]) symId = r.id;
                };
                check(ins.result);
                for (auto& op : ins.typedOperands) check(op);
                if (symId >= 0) break;
            }
            if (symId >= 0) storeResult(IRRef::var(symId), argRegs[i]);
        }
        for (auto& ins : fn.instructions) {
            if (ins.opcode == IROpcode::FUNC_BEGIN || ins.opcode == IROpcode::FUNC_END) continue;
            compileInstr(ins);
        }
        ensureSpilled();
        out << "    mov sp, x29\n    ldp x29, x30, [sp], #16\n    ret\n";

        std::string bodyText = out.str();
        out.str("");
        out.swap(saved); // `out` now holds the original accumulated text again

        uint32_t frameBytes = (uint32_t)(((funcMaxSlot_+1)*8 + 15) & ~15);
        out << fn.name << ":\n";
        out << "    stp x29, x30, [sp, #-16]!\n    mov x29, sp\n";
        if (frameBytes > 0) out << "    sub sp, sp, #" << frameBytes << "\n";
        out << bodyText;
        currentFuncName_.clear();
    }

public:
    explicit ArmAsmCompiler(const IRProgram& p) : prog(p) {}

    std::string compile() {
        for (auto& fn : prog.functions) if (fn.classOwner.empty()) knownFuncs_.insert(fn.name);
        out << ".text\n.global _start\n_start:\n";
        // Unlike exp_arm.cpp's binary backend (which picks its own fixed VA and controls the
        // whole ELF layout directly), text output is assembled and linked by the REAL
        // toolchain, which chooses .bss's actual address itself — there's no fixed number to
        // hardcode here. Symbolic PC-relative addressing (adrp+add :lo12:) is the standard
        // AArch64 idiom for this and is exactly what a real compiler emits for "take the
        // address of a global" (verified against a real assemble+link+qemu-aarch64 run before
        // wiring this in).
        out << "    adrp x28, ac_globals\n    add x28, x28, :lo12:ac_globals\n";
        elidableTemps_ = computeElidableTemps(prog.globalInit);
        pendingTempId_ = -1;
        for (auto& ins : prog.globalInit) compileInstr(ins);
        ensureSpilled();
        out << "    mov x0, #0\n    mov x8, #94\n    svc #0\n";
        for (auto& fn : prog.functions) compileFunction(fn);
        emitPrintIntRoutine();
        // Real bug caught before it shipped: this check was missing entirely at first — a
        // program needing more than 512 total slots would have silently written past
        // ac_globals' reserved 4096 bytes into whatever .bss puts next, instead of failing to
        // compile. Matches exp_arm.cpp's identical (conservative — this counts slots used by
        // per-function SP-relative frames too, not just the shared globals page, same as
        // there) bound.
        if ((size_t)nextSlot * 8 > 4096)
            throw ACError::backend("ARM ASM backend: program needs more than this v1 slice's 4096-byte globals page supports");
        out << ".bss\n.balign 16\nac_globals: .skip 4096\n";
        return out.str();
    }
};

} // namespace AC_ArmAsmGen

bool generateArmAsmFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile) {
    using namespace AC_ArmAsmGen;
    ArmAsmCompiler compiler(ir);
    std::string text = compiler.compile();
    std::ofstream f(outputFile);
    if (!f) return false;
    f << text;
    return true;
}
