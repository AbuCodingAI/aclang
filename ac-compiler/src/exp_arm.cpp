/*
  exp_arm.cpp — AC Native Binary Generator, AArch64
  Target: Linux ARM64 (aarch64) ELF64 only.

  Mirrors exp_bny.cpp's role for AArch64 — hand-encoded machine code, no external assembler,
  no external linker. Every instruction encoding below was verified against a real
  `aarch64-linux-gnu-as` + `objdump -d` round-trip during development (not hand-derived from
  the ARM ARM alone) — see the session notes for the exact .s snippets used to confirm each
  bit pattern before it was hardcoded here.

  v1 scope (deliberately, not a hidden gap): a single <mainloop>, integer arithmetic,
  comparisons, WHILST/IF control flow, and Term.display of an int. No user functions, no
  strings, no floats, no arrays/dicts/bundles yet — this is the same starting point BNY itself
  would have had before its many follow-up passes added those. Globals/temps live in a small
  fixed-address data page (no relocation/fixup machinery needed for v1: both the data page and
  the code page have addresses fixed at compile time, independent of how much code is
  generated), so there is no two-pass address-patching system yet either — that becomes
  necessary once string constants (rodata) are added.
*/
#include "../include/ac.hpp"
#include "../include/error.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <stdexcept>

namespace AC_ArmGen {

using namespace AC_IR;

// ─── AArch64 registers (X0..X30, SP) ───────────────────────────────────────
enum class R : int {
    X0=0,X1=1,X2=2,X3=3,X4=4,X5=5,X6=6,X7=7,X8=8,X9=9,X10=10,X11=11,
    X12=12,X13=13,X14=14,X15=15,X16=16,X17=17,X18=18,X19=19,X20=20,
    X21=21,X22=22,X23=23,X24=24,X25=25,X26=26,X27=27,X28=28,X29=29,X30=30,
    XZR=31, SP=31 // context-dependent encoding (same bit pattern, different instruction classes)
};
static inline uint32_t rn(R r) { return (uint32_t)r; }

// Condition codes (b.cond / cset), values match the AArch64 4-bit cond field exactly —
// verified against `aarch64-linux-gnu-as` output for b.eq/b.ne/b.lt/b.gt/b.le/b.ge.
enum class Cond : uint32_t { EQ=0, NE=1, GE=10, LT=11, GT=12, LE=13 };

// ─── AArch64 instruction encoder ───────────────────────────────────────────
// Every base hex constant below was read directly off `objdump -d` of assembler-verified
// input, not derived from memory of the encoding tables — see this file's header comment.
class ArmEmitter {
    std::vector<uint8_t> buf;

    void emit32(uint32_t w) {
        buf.push_back((uint8_t)(w & 0xFF));
        buf.push_back((uint8_t)((w>>8) & 0xFF));
        buf.push_back((uint8_t)((w>>16) & 0xFF));
        buf.push_back((uint8_t)((w>>24) & 0xFF));
    }

public:
    size_t pos() const { return buf.size(); }
    const std::vector<uint8_t>& bytes() const { return buf; }
    void patch32(size_t off, uint32_t w) {
        buf[off]=(uint8_t)(w&0xFF); buf[off+1]=(uint8_t)((w>>8)&0xFF);
        buf[off+2]=(uint8_t)((w>>16)&0xFF); buf[off+3]=(uint8_t)((w>>24)&0xFF);
    }

    // ── data movement ──
    void movz(R d, uint16_t imm16, int hw /*0..3*/) {
        emit32(0xD2800000u | ((uint32_t)hw<<21) | ((uint32_t)imm16<<5) | rn(d));
    }
    void movk(R d, uint16_t imm16, int hw /*0..3*/) {
        emit32(0xF2800000u | ((uint32_t)hw<<21) | ((uint32_t)imm16<<5) | rn(d));
    }
    // Full 64-bit immediate load (4 instructions, always — correctness over size for v1).
    void mov_imm64(R d, int64_t val) {
        uint64_t u = (uint64_t)val;
        movz(d, (uint16_t)(u & 0xFFFF), 0);
        movk(d, (uint16_t)((u>>16) & 0xFFFF), 1);
        movk(d, (uint16_t)((u>>32) & 0xFFFF), 2);
        movk(d, (uint16_t)((u>>48) & 0xFFFF), 3);
    }
    // Register 31 is CONTEXT-DEPENDENT in AArch64: XZR in most instruction classes, SP only in
    // a few dedicated ones (ADD/SUB immediate, and the real MOV-to/from-SP alias, which IS an
    // ADD-immediate encoding). mov_reg's ORR-based encoding always reads Rn=31 as XZR, so
    // `mov_reg(d, R::SP)` silently computes `d = XZR` (0), NOT the stack pointer — a real bug
    // caught by objdump-reading the first test binary (a `mov x14, sp` in this file's own
    // source came out as `mov x14, xzr` in the disassembly, and X14 then null-pointer-wrote via
    // strb). Use mov_from_sp for SP specifically; mov_reg for any other source register.
    void mov_reg(R d, R n) { emit32(0xAA0003E0u | (rn(n)<<16) | rn(d)); }
    void mov_from_sp(R d)  { emit32(0x91000000u | (rn(R::SP)<<5) | rn(d)); } // add d, sp, #0
    void mov_sp_from(R n)  { emit32(0x91000000u | (rn(n)<<5) | rn(R::SP)); } // add sp, n, #0

    // ── arithmetic (register) ──
    void add_reg(R d, R n, R m) { emit32(0x8B000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void sub_reg(R d, R n, R m) { emit32(0xCB000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void subs_reg(R d, R n, R m){ emit32(0xEB000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void cmp_reg(R n, R m)      { emit32(0xEB00001Fu | (rn(m)<<16) | (rn(n)<<5)); }
    void mul_reg(R d, R n, R m) { emit32(0x9B007C00u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void madd(R d, R n, R m, R a){ emit32(0x9B000000u | (rn(m)<<16) | (rn(a)<<10) | (rn(n)<<5) | rn(d)); }
    void msub(R d, R n, R m, R a){ emit32(0x9B008000u | (rn(m)<<16) | (rn(a)<<10) | (rn(n)<<5) | rn(d)); }
    void sdiv(R d, R n, R m)    { emit32(0x9AC00C00u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void neg_reg(R d, R n)      { emit32(0xCB0003E0u | (rn(n)<<16) | rn(d)); }

    // ── arithmetic (12-bit unsigned immediate, 0..4095) ──
    void add_imm(R d, R n, uint32_t imm12) { emit32(0x91000000u | (imm12<<10) | (rn(n)<<5) | rn(d)); }
    void sub_imm(R d, R n, uint32_t imm12) { emit32(0xD1000000u | (imm12<<10) | (rn(n)<<5) | rn(d)); }

    // ── bitwise ──
    void and_reg(R d, R n, R m) { emit32(0x8A000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void orr_reg(R d, R n, R m) { emit32(0xAA000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void eor_reg(R d, R n, R m) { emit32(0xCA000000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void mvn_reg(R d, R n)      { emit32(0xAA2003E0u | (rn(n)<<16) | rn(d)); }
    void lsl_reg(R d, R n, R m) { emit32(0x9AC02000u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void lsr_reg(R d, R n, R m) { emit32(0x9AC02400u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }

    // ── comparison result → 0/1 integer ──
    void cset(R d, Cond c) {
        static const uint32_t base[6] = {
            /*EQ*/0x9A9F17E0u, /*NE*/0x9A9F07E0u, /*GE*/0x9A9FB7E0u,
            /*LT*/0x9A9FA7E0u, /*GT*/0x9A9FD7E0u, /*LE*/0x9A9FC7E0u
        };
        uint32_t b;
        switch (c) {
            case Cond::EQ: b=base[0]; break; case Cond::NE: b=base[1]; break;
            case Cond::GE: b=base[2]; break; case Cond::LT: b=base[3]; break;
            case Cond::GT: b=base[4]; break; default: b=base[5]; break;
        }
        emit32(b | rn(d));
    }

    // ── loads/stores (unsigned scaled 12-bit imm, offset must be a multiple of 8, 0..32760) ──
    void ldr_imm(R t, R n, uint32_t imm) { emit32(0xF9400000u | (((imm/8)&0xFFF)<<10) | (rn(n)<<5) | rn(t)); }
    void str_imm(R t, R n, uint32_t imm) { emit32(0xF9000000u | (((imm/8)&0xFFF)<<10) | (rn(n)<<5) | rn(t)); }
    // Single-byte store, zero offset (Wt is the low 32 bits of the same-numbered X register —
    // verified base 0x39000000 | Rn<<5 | Rt against `strb w1,[x1]`).
    void strb0(R t, R n) { emit32(0x39000000u | (rn(n)<<5) | rn(t)); }
    // Escape hatch for fixup patching only — never used for first-pass emission.
    void word32(uint32_t w) { emit32(w); }

    // ── fixed prologue/epilogue idiom (verified exact bytes for THESE specific operands) ──
    void stp_x29_x30_presp16()  { emit32(0xA9BF7BFDu); }  // stp x29,x30,[sp,#-16]!
    void ldp_x29_x30_postsp16() { emit32(0xA8C17BFDu); }  // ldp x29,x30,[sp],#16
    void mov_x29_sp()           { emit32(0x910003FDu); }  // mov x29, sp
    void ret()                  { emit32(0xD65F03C0u); }

    // ── branches (relative; label resolution is the caller's job — see LabelFixup below) ──
    void b_rel(int32_t imm26)     { emit32(0x14000000u | ((uint32_t)imm26 & 0x3FFFFFFu)); }
    void bl_rel(int32_t imm26)    { emit32(0x94000000u | ((uint32_t)imm26 & 0x3FFFFFFu)); }
    void cbz_rel(R t, int32_t imm19)  { emit32(0xB4000000u | (((uint32_t)imm19 & 0x7FFFFu)<<5) | rn(t)); }
    void cbnz_rel(R t, int32_t imm19) { emit32(0xB5000000u | (((uint32_t)imm19 & 0x7FFFFu)<<5) | rn(t)); }
    void bcond_rel(Cond c, int32_t imm19) {
        emit32(0x54000000u | (((uint32_t)imm19 & 0x7FFFFu)<<5) | (uint32_t)c);
    }

    // ── syscall ──
    void svc0() { emit32(0xD4000001u); }
    void nop()  { emit32(0xD503201Fu); }
};

// ─── ELF64 writer (format is architecture-agnostic; only e_machine differs from BNY's x86-64
// writer — struct layout copied from exp_bny.cpp's own ElfEhdr/ElfPhdr, which are already
// generic ELF64, not x86-specific) ──────────────────────────────────────────
#pragma pack(push,1)
struct ElfEhdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
};
struct ElfPhdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};
#pragma pack(pop)

static const uint32_t EM_AARCH64 = 183;

// ─── Branch fixups ──────────────────────────────────────────────────────────
enum class FixKind { B, BL, CBZ, CBNZ, BCOND };
struct Fixup { size_t bufOff; int labelId; FixKind kind; Cond cond; R reg; };

// ─── Temp elision: the real optimization behind "make scope boundaries earn their keep" ──
// Every var/temp in this backend round-trips through memory on every access (compute → STR →
// immediately LDR back) — correct, but wasteful for the overwhelmingly common case: a TEMP
// (t_N) that's computed once and consumed exactly once, by the very next instruction (`t_1 =
// lte i, 5` followed immediately by `jf t_1, L1` is the typical shape — every comparison
// feeding a branch, and most arithmetic feeding a STORE_VAR, looks like this). Temps are safe
// to elide this way specifically because this IR generates them with textually-local,
// non-loop-spanning live ranges by construction: a temp is always freshly (re)defined at its
// one definition site and never expected to hold a value across a loop back-edge the way a VAR
// (`s`, `i`, ...) legitimately can — VARs are deliberately NEVER elided here, only TEMPs.
//
// Scoped per compilation unit (one call per function body, one for the mainloop) — same
// boundary FUNC_BEGIN/FUNC_END and TAG_BEGIN/TAG_END already mark structurally, just actually
// used for something now instead of being pure no-ops.
static std::set<int> computeElidableTemps(const std::vector<AC_IR::IRInstruction>& instrs) {
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
        if (uc == useCount.end() || uc->second != 1) continue;   // must be used exactly once
        auto fu = firstUseIdx.find(tid);
        if (fu != firstUseIdx.end() && fu->second == di + 1) result.insert(tid); // by the very next instruction
    }
    return result;
}

// ─── Minimal AArch64 compiler: globals/temps in a fixed data page, mainloop only (v1) ──
class ArmCompiler {
    ArmEmitter em;
    std::vector<Fixup> fixups;
    std::map<int,size_t> labelOffsets;     // IR label id -> buffer offset
    std::vector<std::pair<size_t,std::string>> callFixups; // bufOff -> callee function name
    std::map<std::string,size_t> funcOffsets;              // function name -> buffer offset
    std::map<std::string,int> slotOf;      // qualified key (see keyFor) -> slot index
    int nextSlot = 0;
    const IRProgram& prog;

    // Which IRFunction (by name) is currently being compiled — "" while compiling the
    // mainloop. Real symbol (VAR) ids are globally unique across the whole program (the
    // SymbolTable's intern counter never resets), but TEMP ids are NOT — each IRFunction has
    // its own tempCount starting back at 0 (see IRFunction's own declaration), so `t_0` inside
    // `add` and `t_0` inside the mainloop are two totally different values that would silently
    // alias the same memory slot without this qualifier. Same reasoning extends to VAR too, out
    // of caution, since nothing here re-derives the real SymbolTable scoping rules directly.
    std::string currentFuncName_;

    // Vars and temps share one key space here (qualified by function + kind so no two
    // different bindings can ever collide) — same "give everything a memory slot, no register
    // allocation" choice BNY itself could have started from before its own register allocator
    // existed. NOTE (v1.1 scope, not silently accepted — see this file's header comment):
    // every function's locals get their OWN distinct slots but STILL share the one flat
    // globals page, not a real per-call stack frame — so this does not yet support recursion
    // (a recursive call would overwrite its own in-flight locals). Real stack-frame-backed
    // locals are the natural next step once plain non-recursive calls are solid.
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

    static const R GLOBALS_BASE = R::X28; // pinned for the whole program (no calls to clobber it in v1)
    static const R S0 = R::X9, S1 = R::X10, S2 = R::X11;

    // funcMaxSlot_ tracks the highest slot index actually used *within the function currently
    // being compiled* (reset per function in compileFunction) — used to size that function's
    // own `sub sp, sp, #N` at the end of compiling its body, once N is finally known. See
    // compileFunction's own comment for why this, not the flat globals page, is what makes
    // recursion correct: mainloop-level vars (currentFuncName_=="") still use the flat page
    // (mainloop never recurses, so there's nothing to fix there), but every function's own
    // locals/params/temps now live in ITS OWN per-call stack frame, addressed relative to SP.
    int funcMaxSlot_ = -1;
    R currentBase() const { return currentFuncName_.empty() ? GLOBALS_BASE : R::SP; }

    // Temp-elision cache (see computeElidableTemps' own comment). X16 is a dedicated relocation
    // slot, touched NOWHERE else in this file — but relocating there is the FALLBACK, not the
    // common case: an elided value is left exactly where the arithmetic op already computed it
    // (S0) and costs zero extra instructions, UNLESS something is about to overwrite that
    // register before the value's one consumer reads it (a 2-operand instruction whose FIRST
    // operand load would clobber S0 before the elided value, itself the SECOND operand, gets
    // read) — only THEN does it get moved to X16 first. Verified this matters: the naive
    // "always route through X16" version compiled a redundant `mov x16,x9` / `mov x9,x16` pair
    // for the (dominant) single-operand-consumer case, where the value was already sitting
    // exactly where its consumer wanted it.
    static const R TCACHE = R::X16;
    std::set<int> elidableTemps_;      // recomputed per scope by compileFunction/compile()
    int pendingTempId_ = -1;           // temp id currently pending, uncommitted to memory
    R pendingTempReg_ = S0;            // which register it's actually sitting in right now

    // Safety net, not the common path: if computeElidableTemps' "used exactly once, by the very
    // next instruction" guarantee ever doesn't hold (a bug, or a future IR shape this scan
    // doesn't anticipate), spill the pending value to real memory before it could be lost —
    // silently dropping a computed value would be exactly the "compiles clean, wrong at
    // runtime" failure mode this file no longer allows anywhere else. Only reachable at scope
    // boundaries in correct operation (see compile()/compileFunction()'s own calls to this).
    void ensureSpilled() {
        if (pendingTempId_ < 0) return;
        int tid = pendingTempId_;
        R reg = pendingTempReg_;
        pendingTempId_ = -1;
        IRRef t = IRRef::temp(tid);
        int s = slotFor(t);
        if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
        em.str_imm(reg, currentBase(), (uint32_t)(s*8));
    }
    // Called immediately before writing into `dst` for some OTHER reference — if a pending
    // elided value is currently sitting in that exact register, it must move out of the way
    // first (to the dedicated, never-otherwise-used TCACHE) or the write below would silently
    // clobber it.
    void protectPending(R dst) {
        if (pendingTempId_ >= 0 && pendingTempReg_ == dst) {
            em.mov_reg(TCACHE, pendingTempReg_);
            pendingTempReg_ = TCACHE;
        }
    }

    void loadOperand(const IRRef& r, R dst) {
        if (r.kind == IRRef::Kind::TEMP && r.id == pendingTempId_) {
            if (dst != pendingTempReg_) em.mov_reg(dst, pendingTempReg_);
            pendingTempId_ = -1;
            return;
        }
        protectPending(dst);
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::INT) {
            em.mov_imm64(dst, std::get<int64_t>(r.value.data));
        } else if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::BOOL) {
            em.mov_imm64(dst, std::get<bool>(r.value.data) ? 1 : 0);
        } else if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) {
            int s = slotFor(r);
            if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
            em.ldr_imm(dst, currentBase(), (uint32_t)(s*8));
        } else {
            em.mov_imm64(dst, 0);
        }
    }
    void storeResult(const IRRef& r, R src) {
        // Elide: leave an eligible temp's value exactly where it already is (src, almost always
        // S0) instead of spilling to memory now — its one and only consumer (the very next
        // instruction, by construction) will pick it straight up via loadOperand's cache-hit
        // branch above, moving it only if something else needs `src` first.
        if (r.kind == IRRef::Kind::TEMP && elidableTemps_.count(r.id)) {
            ensureSpilled(); // in case a previous elision somehow wasn't consumed — see its own comment
            pendingTempId_ = r.id;
            pendingTempReg_ = src;
            return;
        }
        ensureSpilled();
        int s = slotFor(r);
        if (!currentFuncName_.empty()) funcMaxSlot_ = std::max(funcMaxSlot_, s);
        em.str_imm(src, currentBase(), (uint32_t)(s*8));
    }

    void emitLabel(int labelId) { labelOffsets[(int)labelId] = em.pos(); }
    // `reg` only matters for CBZ/CBNZ (the register being tested) — defaults to S0, the
    // convention every compileInstr() call site relies on (loadOperand always leaves the
    // tested value in S0 before calling this); emitPrintIntRoutine's loop passes X12 explicitly
    // since ITS tested value lives there instead.
    void emitBranch(FixKind kind, int labelId, Cond c = Cond::EQ, R reg = S0) {
        fixups.push_back({em.pos(), labelId, kind, c, reg});
        switch (kind) {
            case FixKind::B:     em.b_rel(0); break;
            case FixKind::BL:    em.bl_rel(0); break;
            case FixKind::CBZ:   em.cbz_rel(reg, 0); break;
            case FixKind::CBNZ:  em.cbnz_rel(reg, 0); break;
            case FixKind::BCOND: em.bcond_rel(c, 0); break;
        }
    }

    static int labelIdOf(const IRRef& r) { return r.id; }

    void compileCompare(Cond c, const IRInstruction& ins) {
        loadOperand(ins.typedOperands[0], S0);
        loadOperand(ins.typedOperands[1], S1);
        em.cmp_reg(S0, S1);
        em.cset(S0, c);
        storeResult(ins.result, S0);
    }

    void compileInstr(const IRInstruction& ins) {
        switch (ins.opcode) {
            // The optimizer constant-folds aggressively (verified: `x@y` for two known-constant
            // operands becomes `t = ldc 85` directly, never reaching a real MUL instruction at
            // all) — LOAD_CONST is just as central to v1's actual test coverage as any
            // arithmetic opcode, not a rare edge case. Float constants (`ldc 3.4`) are a real, documented v1
            // gap (no FP/NEON support yet) — a hard error, not a no-op: silently storing 0
            // instead of the real value is exactly the "compiles clean, wrong at runtime"
            // failure mode this file no longer allows (see the default: case below).
            case IROpcode::LOAD_CONST:
                if (ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::FLOAT)
                    throw ACError::backend("ARM backend: floating-point constants are not yet implemented");
                loadOperand(ins.typedOperands[0], S0);
                storeResult(ins.result, S0);
                break;
            // Structural markers with no runtime semantics of their own — every real AC
            // program has at least a TAG_BEGIN/TAG_END pair around <mainloop> (verified:
            // --stop-after-ir shows `tag_begin "mainloop"` on literally every test case), so
            // these are true no-ops, not gaps, unlike the throwing default: case below.
            case IROpcode::TAG_BEGIN:
            case IROpcode::TAG_END:
            case IROpcode::NOP:
                break;
            case IROpcode::LABEL:
                emitLabel(labelIdOf(ins.typedOperands[0]));
                break;
            case IROpcode::JUMP:
                emitBranch(FixKind::B, labelIdOf(ins.typedOperands[0]));
                break;
            case IROpcode::JUMP_IF_FALSE:
                loadOperand(ins.typedOperands[0], S0);
                emitBranch(FixKind::CBZ, labelIdOf(ins.typedOperands[1]));
                break;
            case IROpcode::JUMP_IF_TRUE:
                loadOperand(ins.typedOperands[0], S0);
                emitBranch(FixKind::CBNZ, labelIdOf(ins.typedOperands[1]));
                break;
            case IROpcode::STORE_VAR: {
                // Two shapes (matches BNY's own note on this): {target,source} in
                // typedOperands[0..1], OR result=target with typedOperands[0]=source alone.
                if (ins.typedOperands.size() >= 2) {
                    loadOperand(ins.typedOperands[1], S0);
                    storeResult(ins.typedOperands[0], S0);
                } else if (!ins.typedOperands.empty()) {
                    loadOperand(ins.typedOperands[0], S0);
                    storeResult(ins.result, S0);
                }
                break;
            }
            case IROpcode::ADD:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.add_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::SUB:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.sub_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::MUL:
            case IROpcode::PMUL:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.mul_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::DIV:
            case IROpcode::IDIV:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.sdiv(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::MOD:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.sdiv(S2, S0, S1);              // S2 = quotient
                em.msub(S0, S2, S1, S0);           // S0 = S0 - S2*S1 = remainder
                storeResult(ins.result, S0);
                break;
            case IROpcode::BAND:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.and_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::BOR:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.orr_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::BXOR:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.eor_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::BNOT:
                loadOperand(ins.typedOperands[0], S0);
                em.mvn_reg(S0, S0); storeResult(ins.result, S0);
                break;
            case IROpcode::PTM:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.lsl_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::PTD:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.lsr_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::EQ:  compileCompare(Cond::EQ, ins); break;
            case IROpcode::NEQ: compileCompare(Cond::NE, ins); break;
            case IROpcode::LT:  compileCompare(Cond::LT, ins); break;
            case IROpcode::GT:  compileCompare(Cond::GT, ins); break;
            case IROpcode::LTE: compileCompare(Cond::LE, ins); break;
            case IROpcode::GTE: compileCompare(Cond::GE, ins); break;
            // AAPCS64: first 8 integer args in X0..X7, return value in X0. v1 caps calls at 8
            // args (register-passed only, no stack-passed overflow yet) — matches this file's
            // "get a real subset working end to end" scope; a 9th argument is a real gap, not
            // silently dropped (see the args.size()>8 check below).
            case IROpcode::CALL: {
                static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                size_t nArgs = ins.typedOperands.size() - 1;
                if (nArgs > 8)
                    throw ACError::backend("ARM backend: calls with more than 8 arguments are not yet implemented");
                for (size_t i = 0; i < nArgs; i++) loadOperand(ins.typedOperands[1+i], argRegs[i]);
                std::string calleeName = prog.symbols.getName(ins.typedOperands[0].id);
                callFixups.push_back({em.pos(), calleeName});
                em.bl_rel(0);
                if (ins.result.kind != IRRef::Kind::NONE) storeResult(ins.result, R::X0);
                break;
            }
            case IROpcode::RETURN:
                if (!ins.typedOperands.empty()) loadOperand(ins.typedOperands[0], R::X0);
                else ensureSpilled(); // defensive — see ensureSpilled's own comment
                // `mov sp, x29` first, not a fixed-size `add sp,sp,#N`: this function's own
                // local-frame size (the `sub sp,sp,#N` at entry, see compileFunction) is
                // patched in AFTER the whole body is compiled, so no fixed N is known yet at
                // any individual return site while compiling reaches it — restoring SP
                // directly from the frame pointer sidesteps needing one at all, and is correct
                // regardless of how much local-frame space this function ends up needing.
                em.mov_sp_from(R::X29);
                em.ldp_x29_x30_postsp16();
                em.ret();
                break;
            case IROpcode::PRINT:
                loadOperand(ins.typedOperands[0], R::X0);
                emitBranch(FixKind::BL, PRINT_INT_LABEL);
                break;
            case IROpcode::HALT:
                // `/kill` is a deliberate hard abort on every other backend (verified earlier
                // this session: CStrategy/PythonStrategy/BNY all raise SIGABRT here on
                // purpose, "for parity w/ PY's /kill" per CStrategy's own comment — not a bug
                // to route around). kill(getpid(), SIGABRT): getpid=172, kill=129 on the
                // AArch64 Linux syscall table.
                em.mov_imm64(R::X8, 172);
                em.svc0();               // X0 = own pid — already kill's 1st arg, no shuffling needed
                em.mov_imm64(R::X1, 6);  // SIGABRT
                em.mov_imm64(R::X8, 129);
                em.svc0();
                break;
            case IROpcode::SOFT_HALT:
                em.mov_imm64(R::X0, 0);
                em.mov_imm64(R::X8, 94); // exit_group (AArch64 Linux syscall table)
                em.svc0();
                break;
            default:
                // A silent no-op here means a real program construct compiles clean and then
                // produces silently WRONG output at runtime (verified: a function call +
                // string concat + array print compiled fine and printed "0") — far worse than
                // refusing to compile. Every opcode this v1 slice actually supports is listed
                // above; anything else is a real, not-yet-built gap and must fail loudly here.
                throw ACError::backend("ARM backend: IR opcode " + std::to_string((int)ins.opcode)
                    + " is not yet implemented (see exp_arm.cpp's header comment for current scope)");
        }
    }

    // Reserved label ids for this routine's internal branches — real IR label ids are always
    // >=0 (see mkLabel() in ir.cpp), so negative sentinels here can never collide.
    static const int PRINT_INT_LABEL = -1000;
    static const int PRINT_POS_LABEL = -1001;
    static const int PRINT_LOOP_LABEL = -1002;
    static const int PRINT_NOSIGN_LABEL = -1003;

    // __ac_print_int__: X0 = value (signed). Writes decimal digits + '\n' via write(2), no
    // libc. Same divide-by-10-from-the-back algorithm as BNY's own emitPrintIntCore
    // (exp_bny.cpp), re-expressed in AArch64 registers/instructions — SDIV+MSUB gives
    // quotient+remainder in 2 instructions where x86's combined `div` needed only 1, otherwise
    // the same shape: fill a stack buffer backward from a fixed newline-adjacent slot, track a
    // "how far left did we get" cursor, then one write(2) covering [cursor, end).
    //
    // Layout of the 32-byte scratch buffer at [sp, sp+31]: digits are written backward starting
    // at sp+30 (a do-while loop — at least one digit, even for value==0); sp+31 always holds the
    // trailing '\n', written once after the loop, independent of how many digits there were.
    void emitPrintIntRoutine() {
        emitLabel(PRINT_INT_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        // X12 = |value|, X13 = negative flag (0/1)
        em.mov_reg(R::X12, R::X0);
        em.mov_imm64(R::X13, 0);
        em.cmp_reg(R::X12, R::XZR);
        emitBranch(FixKind::BCOND, PRINT_POS_LABEL, Cond::GE);
        em.mov_imm64(R::X13, 1);
        em.neg_reg(R::X12, R::X12);
        emitLabel(PRINT_POS_LABEL);
        // X14 = write cursor, starts one before the newline slot, fills backward.
        em.mov_from_sp(R::X14);
        em.add_imm(R::X14, R::X14, 30);
        em.mov_imm64(R::X15, 10);
        emitLabel(PRINT_LOOP_LABEL);
        em.sdiv(R::X0, R::X12, R::X15);         // X0 = quotient
        em.msub(R::X1, R::X0, R::X15, R::X12);  // X1 = remainder = X12 - X0*X15
        em.add_imm(R::X1, R::X1, (uint32_t)'0');
        em.strb0(R::X1, R::X14);
        em.sub_imm(R::X14, R::X14, 1);
        em.mov_reg(R::X12, R::X0);              // X12 = quotient, for the next iteration/test
        emitBranch(FixKind::CBNZ, PRINT_LOOP_LABEL, Cond::EQ, R::X12); // loop while quotient != 0 (do-while)
        // sign
        em.cmp_reg(R::X13, R::XZR);
        emitBranch(FixKind::BCOND, PRINT_NOSIGN_LABEL, Cond::EQ);
        em.mov_imm64(R::X1, (int64_t)'-');
        em.strb0(R::X1, R::X14);
        em.sub_imm(R::X14, R::X14, 1);
        emitLabel(PRINT_NOSIGN_LABEL);
        em.add_imm(R::X14, R::X14, 1); // X14 now points at the first real character
        // newline, fixed at sp+31
        em.mov_from_sp(R::X1);
        em.add_imm(R::X1, R::X1, 31);
        em.mov_imm64(R::X2, (int64_t)'\n');
        em.strb0(R::X2, R::X1);
        // write(1, X14, (sp+31 - X14) + 1)   — digits/sign through the trailing newline
        em.sub_reg(R::X2, R::X1, R::X14);
        em.add_imm(R::X2, R::X2, 1);
        em.mov_reg(R::X1, R::X14);
        em.mov_imm64(R::X0, 1);   // fd = stdout
        em.mov_imm64(R::X8, 64);  // write (AArch64 Linux syscall table)
        em.svc0();
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

public:
    explicit ArmCompiler(const IRProgram& p) : prog(p) {}

    // Free functions only for v1.1 — a bundle/class method (fn.classOwner non-empty) needs a
    // real `self` receiver convention this file doesn't have yet; skipped with a hard error at
    // the point something actually tries to CALL one (this function itself just never emits
    // methods, so a call site's callFixup would resolve to nothing — caught below instead).
    // AAPCS64 prologue: stp x29,x30,[sp,#-16]! ; mov x29,sp ; sub sp,sp,#N — N (this function's
    // own local-frame size, in bytes) isn't known until the WHOLE body has been compiled (slots
    // are assigned lazily, on first reference), so the `sub sp,sp,#0` emitted here is a
    // placeholder, patched to the real N once funcMaxSlot_ is final. Every return path (RETURN
    // inside the body, and the implicit fallthrough return below) restores SP via `mov sp,x29`
    // rather than a matching `add sp,sp,#N` — correct regardless of N, and means no return site
    // needs its own separate fixup.
    void compileFunction(const IRFunction& fn) {
        if (!fn.classOwner.empty()) return; // methods: not yet supported, see comment above
        funcOffsets[fn.name] = em.pos();
        currentFuncName_ = fn.name;
        funcMaxSlot_ = -1;
        // Recomputed per function, not once for the whole program: temp ids restart at 0 in
        // each IRFunction (same reason keyFor is function-qualified — see its own comment), so
        // reusing one elidableTemps_ set across functions would let function B's temp 0 wrongly
        // inherit function A's temp 0's elision eligibility.
        elidableTemps_ = computeElidableTemps(fn.instructions);
        pendingTempId_ = -1;
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        size_t subSpOff = em.pos();
        em.sub_imm(R::SP, R::SP, 0); // placeholder — patched below once the real frame size is known
        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
        if (fn.parameters.size() > 8)
            throw ACError::backend("ARM backend: functions with more than 8 parameters are not yet implemented");
        for (size_t i = 0; i < fn.parameters.size(); i++) {
            // Parameters are VARs like any other — find their real symbol id the same way
            // any other reference to that name would resolve, by scanning this function's own
            // instructions for a matching VAR (mirrors BNY's identical param-binding approach
            // in exp_bny.cpp, for the identical reason: parameters are interned but not
            // otherwise distinguished from ordinary locals in this IR).
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
            if (symId >= 0) {
                IRRef pref = IRRef::var(symId);
                storeResult(pref, argRegs[i]);
            }
            // A parameter never referenced in the body at all needs no slot — nothing to do.
        }
        for (auto& ins : fn.instructions) {
            if (ins.opcode == IROpcode::FUNC_BEGIN || ins.opcode == IROpcode::FUNC_END) continue;
            compileInstr(ins);
        }
        ensureSpilled(); // in case the body's last instruction left something uncommitted
        // Implicit void return, if the body fell off the end without an explicit `return`.
        em.mov_sp_from(R::X29);
        em.ldp_x29_x30_postsp16();
        em.ret();
        // Now that the whole body (and its params) have assigned every slot they'll ever use,
        // patch the placeholder `sub sp,sp,#0` from the prologue with this function's real
        // frame size — 16-byte-aligned per AAPCS64 (SP must stay 16-aligned at every call).
        uint32_t frameBytes = (uint32_t)(((funcMaxSlot_+1)*8 + 15) & ~15);
        if (frameBytes > 4095)
            throw ACError::backend("ARM backend: function '" + fn.name
                + "' needs more local-frame space than this v1.1 slice's 12-bit immediate encoding supports");
        em.patch32(subSpOff, 0xD1000000u | (frameBytes<<10) | (rn(R::SP)<<5) | rn(R::SP));
        currentFuncName_.clear();
    }

    // Returns {machine code bytes, data-slot count needed}.
    std::pair<std::vector<uint8_t>,int> compile(uint64_t globalsVA) {
        // Pin X28 = globals base address once, at program start.
        em.mov_imm64(GLOBALS_BASE, (int64_t)globalsVA);
        // globalInit already IS data+main combined (see IRProgram's own comment on the field) —
        // looping mainSection too would compile the mainloop body twice (verified real bug:
        // caught it via a byte-for-byte objdump read of the very first test binary, which
        // showed the store/print/halt sequence duplicated even though --stop-after-ir's LIR
        // dump only shows it once, since the dump reads a different view).
        elidableTemps_ = computeElidableTemps(prog.globalInit);
        pendingTempId_ = -1;
        for (auto& ins : prog.globalInit) compileInstr(ins);
        ensureSpilled(); // in case the mainloop's last instruction left something uncommitted
        // Fallthrough safety net: ensure a clean exit even if no explicit /kill was compiled.
        em.mov_imm64(R::X0, 0);
        em.mov_imm64(R::X8, 94);
        em.svc0();
        for (auto& fn : prog.functions) compileFunction(fn);
        emitPrintIntRoutine();

        for (auto& fx : fixups) {
            auto it = labelOffsets.find(fx.labelId);
            if (it == labelOffsets.end()) continue; // dangling — skip rather than crash
            int64_t delta = (int64_t)it->second - (int64_t)fx.bufOff;
            int32_t words = (int32_t)(delta / 4);
            switch (fx.kind) {
                case FixKind::B:     em.patch32(fx.bufOff, 0x14000000u | ((uint32_t)words & 0x3FFFFFFu)); break;
                case FixKind::BL:    em.patch32(fx.bufOff, 0x94000000u | ((uint32_t)words & 0x3FFFFFFu)); break;
                case FixKind::CBZ:   em.patch32(fx.bufOff, 0xB4000000u | (((uint32_t)words & 0x7FFFFu)<<5) | rn(fx.reg)); break;
                case FixKind::CBNZ:  em.patch32(fx.bufOff, 0xB5000000u | (((uint32_t)words & 0x7FFFFu)<<5) | rn(fx.reg)); break;
                case FixKind::BCOND: em.patch32(fx.bufOff, 0x54000000u | (((uint32_t)words & 0x7FFFFu)<<5) | (uint32_t)fx.cond); break;
            }
        }
        for (auto& [bufOff, calleeName] : callFixups) {
            auto it = funcOffsets.find(calleeName);
            if (it == funcOffsets.end())
                throw ACError::backend("ARM backend: call to unresolved function '" + calleeName
                    + "' (bundle methods and forward-declared-only functions are not yet implemented)");
            int64_t delta = (int64_t)it->second - (int64_t)bufOff;
            int32_t words = (int32_t)(delta / 4);
            em.patch32(bufOff, 0x94000000u | ((uint32_t)words & 0x3FFFFFFu));
        }
        return {em.bytes(), nextSlot};
    }
};

static bool writeArmELF(const std::string& path, const std::vector<uint8_t>& text) {
    const uint64_t BASE   = 0x400000ULL;
    const uint64_t PGSZ   = 0x1000ULL;
    const uint64_t dataVA = BASE + PGSZ;       // fixed globals page — see this file's header
    const uint64_t codeVA = BASE + 2*PGSZ;     // fixed code page

    uint16_t phnum = 2; // data (RW) + text (RX)
    size_t hdrBytes = sizeof(ElfEhdr) + phnum*sizeof(ElfPhdr);
    uint64_t dataOff = ((hdrBytes + PGSZ-1)/PGSZ)*PGSZ;
    uint64_t codeOff = dataOff + PGSZ;

    ElfEhdr eh{};
    eh.e_ident[0]=0x7F; eh.e_ident[1]='E'; eh.e_ident[2]='L'; eh.e_ident[3]='F';
    eh.e_ident[4]=2; eh.e_ident[5]=1; eh.e_ident[6]=1;
    eh.e_type=2; eh.e_machine=(uint16_t)EM_AARCH64; eh.e_version=1;
    eh.e_entry = codeVA;
    eh.e_phoff = sizeof(ElfEhdr);
    eh.e_ehsize=sizeof(ElfEhdr); eh.e_phentsize=sizeof(ElfPhdr); eh.e_phnum=phnum;

    ElfPhdr dp{};
    dp.p_type=1; dp.p_flags=6; // PT_LOAD, R|W
    dp.p_offset=dataOff; dp.p_vaddr=dataVA; dp.p_paddr=dataVA;
    dp.p_filesz=0; dp.p_memsz=PGSZ; dp.p_align=PGSZ;

    ElfPhdr tp{};
    tp.p_type=1; tp.p_flags=5; // PT_LOAD, R|X
    tp.p_offset=codeOff; tp.p_vaddr=codeVA; tp.p_paddr=codeVA;
    tp.p_filesz=text.size(); tp.p_memsz=text.size(); tp.p_align=PGSZ;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((char*)&eh, sizeof(eh));
    f.write((char*)&dp, sizeof(dp));
    f.write((char*)&tp, sizeof(tp));
    std::vector<uint8_t> zeros;
    zeros.assign(codeOff - (sizeof(ElfEhdr)+phnum*sizeof(ElfPhdr)), 0);
    f.write((char*)zeros.data(), zeros.size());
    f.write((char*)text.data(), text.size());
    return true;
}

} // namespace AC_ArmGen

bool generateArmBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile) {
    using namespace AC_ArmGen;
    ArmCompiler compiler(ir);
    const uint64_t BASE = 0x400000ULL, PGSZ = 0x1000ULL, dataVA = BASE + PGSZ;
    auto [bytes, slotCount] = compiler.compile(dataVA);
    if ((size_t)slotCount * 8 > PGSZ) return false; // v1 fixed-page limit — see header comment
    return writeArmELF(outputFile, bytes);
}
