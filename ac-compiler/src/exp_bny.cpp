/*
  exp_bny.cpp — AC Native Binary Generator
  Target: Linux x86-64 ELF64 Only
  Generates native Linux ELF64 binaries - Ubuntu, Fedora, Arch, Mint, Bazzite, etc.
*/
#include "../include/ac.hpp"
#include "../include/error.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <climits>
#include <sys/stat.h>

#ifdef _WIN32
#  define TARGET_WINDOWS 1
#elif __linux__
#  define TARGET_LINUX 1
#elif __APPLE__
#  define TARGET_MACOS 1
#endif

namespace AC_BinaryGen {

    // ─── Platform Detection ────────────────────────────────────────────────────
    enum class HostCPU { X86_64, ARM64, ARM32, UNKNOWN };
    
    static HostCPU detectCPU() {
    #if defined(__x86_64__) || defined(_M_X64)
        return HostCPU::X86_64;
    #elif defined(__aarch64__) || defined(_M_ARM64)
        return HostCPU::ARM64;
    #elif defined(__arm__) || defined(_M_ARM)
        return HostCPU::ARM32;
    #else
        return HostCPU::UNKNOWN;
    #endif
    }

    static bool needsCrossCompilation() {
        HostCPU cpu = detectCPU();
        return (cpu == HostCPU::ARM64 || cpu == HostCPU::ARM32);
    }
    
    


// ─── Register IDs ────────────────────────────────────────────────────────────
enum class R : int {
    RAX=0,RCX=1,RDX=2,RBX=3,RSP=4,RBP=5,RSI=6,RDI=7,
    R8=8,R9=9,R10=10,R11=11,R12=12,R13=13,R14=14,R15=15
};
// Use PhysReg alias from original code so exp_bny.hpp still works
using PhysReg = R;

// ─── x86-64 Code Emitter ─────────────────────────────────────────────────────
class X64Emitter {
    std::vector<uint8_t> buf;
    // These are pure key→value stores whose consumers patch DISJOINT byte ranges of `buf`, so the
    // emitted bytes are identical regardless of iteration order → unordered_map (O(1) vs O(log n)
    // over hundreds of emit sites). (NOTE: the live-interval map in the reg allocator is left as
    // std::map on purpose — its ordered iteration breaks sort ties deterministically.)
    std::unordered_map<std::string, size_t> labelDefs;   // label → offset
    std::unordered_map<size_t, std::string> rel32Fixups; // offset → label
    std::unordered_map<size_t, int>         strAddrFixups; // offset → string pool id
    std::unordered_map<size_t, int>         gvarAddrFixups; // offset → global var slot id (NA→free)
    // PLT→GOT fixups: buf_offset → sym_idx (>=0) or -1 (got[1]) or -2 (got[2])
    std::unordered_map<size_t, int>         gotPltFixups_;

    void rex(bool W, int R_, int B) {
        uint8_t b = 0x40;
        if (W) b |= 8;
        if (R_ >= 8) b |= 4;
        if (B  >= 8) b |= 1;
        if (b != 0x40) emit(b);
    }
    void rexFull(bool W, int R_, int X, int B) {
        uint8_t b = 0x40;
        if (W) b |= 8;
        if (R_ >= 8) b |= 4;
        if (X  >= 8) b |= 2;
        if (B  >= 8) b |= 1;
        if (b != 0x40) emit(b);
    }
    void modrm(int mod, int reg, int rm) {
        emit(((mod&3)<<6) | ((reg&7)<<3) | (rm&7));
    }

public:
    size_t pos() const { return buf.size(); }

    void emit(uint8_t b) { buf.push_back(b); }
    void emit16(uint16_t v) { emit(v); emit(v>>8); }
    void emit32(uint32_t v) { emit(v); emit(v>>8); emit(v>>16); emit(v>>24); }
    void emit64(uint64_t v) { emit32((uint32_t)v); emit32((uint32_t)(v>>32)); }

    // ── Labels & fixups ──────────────────────────────────────────────────────
    void label(const std::string& name) { labelDefs[name] = pos(); }

    // --static-link splicing: append a freestanding ilib's raw .text, and bind an ilib call label
    // directly to the spliced code (no PLT/GOT, no DT_NEEDED). The object is verified to have zero
    // undefined external symbols, so its .text is position-independent-enough to run as-is inline.
    void appendSplicedText(const std::vector<uint8_t>& t) { buf.insert(buf.end(), t.begin(), t.end()); }
    void defineLabelAt(const std::string& name, size_t off) { labelDefs[name] = off; }

    // Apply all relative fixups (must call before extracting bytes)
    void applyFixups() {
        for (auto& [off, name] : rel32Fixups) {
            auto it = labelDefs.find(name);
            if (it == labelDefs.end())
                throw ACError::undefinedLabel(name, (long long)off);
            int32_t rel = (int32_t)((int64_t)it->second - (int64_t)(off + 4));
            buf[off+0] = rel;  buf[off+1] = rel>>8;
            buf[off+2] = rel>>16; buf[off+3] = rel>>24;
        }
    }
    void applyStringFixups(uint64_t rodataBase, const std::vector<uint8_t>& rodata,
                            const std::vector<size_t>& strOffsets) {
        for (auto& [off, sid] : strAddrFixups) {
            if (sid < 0 || sid >= (int)strOffsets.size()) continue;
            uint64_t addr = rodataBase + strOffsets[sid];
            buf[off+0] = addr;       buf[off+1] = addr>>8;
            buf[off+2] = addr>>16;   buf[off+3] = addr>>24;
            buf[off+4] = addr>>32;   buf[off+5] = addr>>40;
            buf[off+6] = addr>>48;   buf[off+7] = addr>>56;
        }
    }
    // Patch each gvar imm64 with the absolute VA of its global slot (NA→free).
    void applyGVarFixups(uint64_t gvarBase, const std::vector<size_t>& slotOffsets) {
        for (auto& [off, slot] : gvarAddrFixups) {
            if (slot < 0 || slot >= (int)slotOffsets.size()) continue;
            uint64_t addr = gvarBase + slotOffsets[slot];
            for (int i = 0; i < 8; i++) buf[off+i] = (uint8_t)(addr >> (8*i));
        }
    }
    bool hasGVars() const { return !gvarAddrFixups.empty(); }
    const std::vector<uint8_t>& code() const { return buf; }

    size_t getLabelOffset(const std::string& name) const {
        auto it = labelDefs.find(name);
        return it != labelDefs.end() ? it->second : 0;
    }

    // ── MOV ──────────────────────────────────────────────────────────────────
    // mov r64, r64
    void mov_rr(R d, R s) {
        rex(true,(int)s,(int)d); emit(0x89); modrm(3,(int)s,(int)d);
    }
    // mov r64, imm32 (sign-extend)
    void mov_ri32(R d, int32_t imm) {
        rex(true,0,(int)d); emit(0xC7); modrm(3,0,(int)d); emit32(imm);
    }
    // mov r64, imm64
    void mov_ri64(R d, uint64_t imm) {
        rex(true,0,(int)d); emit(0xB8+((int)d&7)); emit64(imm);
    }
    // mov r64, imm64 with string pool fixup (placeholder, patched later)
    void mov_ri64_str(R d, int strId) {
        rex(true,0,(int)d); emit(0xB8+((int)d&7));
        strAddrFixups[pos()] = strId;
        emit64(0);
    }
    // mov r64, imm64 = absolute address of global var slot (NA→free); patched later
    void mov_ri64_gvar(R d, int slotId) {
        rex(true,0,(int)d); emit(0xB8+((int)d&7));
        gvarAddrFixups[pos()] = slotId;
        emit64(0);
    }
    // mov r64, [ptr]  — 64-bit load through a register pointer
    void mov_r_ptr(R d, R ptr) {
        rex(true,(int)d,(int)ptr); emit(0x8B);
        int rm = (int)ptr & 7;
        if (rm == 5) { modrm(1,(int)d,5); emit(0); }       // rbp/r13: need disp8
        else if (rm == 4) { modrm(0,(int)d,4); emit(0x24); } // rsp/r12: need SIB
        else { modrm(0,(int)d,rm); }
    }
    // mov [ptr], r64  — 64-bit store through a register pointer
    void mov_ptr_r(R ptr, R s) {
        rex(true,(int)s,(int)ptr); emit(0x89);
        int rm = (int)ptr & 7;
        if (rm == 5) { modrm(1,(int)s,5); emit(0); }
        else if (rm == 4) { modrm(0,(int)s,4); emit(0x24); }
        else { modrm(0,(int)s,rm); }
    }
    // xchg [ptr], r64 — atomically swaps [ptr] and r (XCHG with a memory operand carries an
    // IMPLICIT LOCK on x86 — no explicit `lock` prefix needed; Intel SDM Vol.2, XCHG). This is
    // the real primitive `atomic` uses on BNY: a spinlock's try-acquire (xchg-with-1, retry
    // while the old value read back is nonzero) needs no libc, no syscall, no external process —
    // fits BNY's zero-dependency, "own the CPU" design exactly.
    void xchg_ptr_r(R ptr, R s) {
        rex(true,(int)s,(int)ptr); emit(0x87);
        int rm = (int)ptr & 7;
        if (rm == 5) { modrm(1,(int)s,5); emit(0); }
        else if (rm == 4) { modrm(0,(int)s,4); emit(0x24); }
        else { modrm(0,(int)s,rm); }
    }
    // mov r64, [rbp+disp]
    void mov_r_rbp(R d, int32_t disp) {
        rex(true,(int)d,5);
        emit(0x8B);
        if (disp>=-128 && disp<=127) { modrm(1,(int)d,5); emit((uint8_t)(int8_t)disp); }
        else { modrm(2,(int)d,5); emit32(disp); }
    }
    // mov [rbp+disp], r64
    void mov_rbp_r(int32_t disp, R s) {
        rex(true,(int)s,5);
        emit(0x89);
        if (disp>=-128 && disp<=127) { modrm(1,(int)s,5); emit((uint8_t)(int8_t)disp); }
        else { modrm(2,(int)s,5); emit32(disp); }
    }
    // mov byte [rbp+disp8], imm8
    void mov_rbp8_imm8(int8_t disp, uint8_t val) {
        emit(0xC6); modrm(1,0,5); emit((uint8_t)disp); emit(val);
    }
    // mov [r64], r8_low  (store low byte of src to memory at ptr)
    void mov_ptr_r8(R ptr, R src8) {
        bool extPtr = (int)ptr>=8, extSrc = (int)src8>=8;
        uint8_t r = 0x40;
        if (extPtr) r |= 1;
        if (extSrc) r |= 4;
        bool needR = extPtr || extSrc || ((int)src8>=4 && (int)src8<=7);
        if (needR) emit(r);
        emit(0x88);
        int pl = (int)ptr&7, sl = (int)src8&7;
        if (pl==5) { modrm(1,sl,5); emit(0); }
        else if (pl==4) { modrm(0,sl,4); emit(0x24); }
        else { modrm(0,sl,pl); }
    }

    // mov r64, [base+disp32] — general register+offset load (`yield`/generators: state-block
    // field access). mov_r_rbp/mov_rbp_r above are the same encoding hardcoded to base=RBP;
    // this generalizes to any base register, always via the mod=10 (disp32) form for simplicity
    // (a few bytes larger than the disp8 form mov_r_rbp uses, but avoids that function's own
    // mod=00-means-RIP-relative special case for rbp/r13 at disp==0 — our field offset 0 (`done`)
    // would otherwise silently misencode as %rip-relative addressing instead of [base+0]).
    void mov_r_based(R d, R base, int32_t disp) {
        rex(true,(int)d,(int)base); emit(0x8B);
        int rm = (int)base & 7;
        if (rm == 4) { modrm(2,(int)d,4); emit(0x24); emit32(disp); } // rsp/r12 need SIB
        else { modrm(2,(int)d,rm); emit32(disp); }
    }
    // mov [base+disp32], r64
    void mov_based_r(R base, int32_t disp, R s) {
        rex(true,(int)s,(int)base); emit(0x89);
        int rm = (int)base & 7;
        if (rm == 4) { modrm(2,(int)s,4); emit(0x24); emit32(disp); }
        else { modrm(2,(int)s,rm); emit32(disp); }
    }

    // ── LEA ──────────────────────────────────────────────────────────────────
    void lea_r_rbp8(R d, int8_t disp) {
        rex(true,(int)d,5); emit(0x8D); modrm(1,(int)d,5); emit((uint8_t)disp);
    }
    void lea_r_rbp32(R d, int32_t disp) {
        rex(true,(int)d,5); emit(0x8D); modrm(2,(int)d,5); emit32(disp);
    }

    // ── Stack ─────────────────────────────────────────────────────────────────
    void push_r(R r) {
        if ((int)r>=8) emit(0x41);
        emit(0x50+((int)r&7));
    }
    void pop_r(R r) {
        if ((int)r>=8) emit(0x41);
        emit(0x58+((int)r&7));
    }
    void push_rbp() { emit(0x55); }
    void pop_rbp()  { emit(0x5D); }
    void mov_rbp_rsp() { emit(0x48); emit(0x89); emit(0xE5); }
    void sub_rsp_i32(int32_t n) {
        if (n>=0 && n<=127) { emit(0x48); emit(0x83); emit(0xEC); emit((uint8_t)n); }
        else { emit(0x48); emit(0x81); emit(0xEC); emit32(n); }
    }
    void add_rsp_i32(int32_t n) {
        if (n>=0 && n<=127) { emit(0x48); emit(0x83); emit(0xC4); emit((uint8_t)n); }
        else { emit(0x48); emit(0x81); emit(0xC4); emit32(n); }
    }

    // ── Arithmetic ───────────────────────────────────────────────────────────
    void add_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x01); modrm(3,(int)s,(int)d); }
    void sub_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x29); modrm(3,(int)s,(int)d); }
    void imul_rr(R d, R s) { rex(true,(int)d,(int)s); emit(0x0F); emit(0xAF); modrm(3,(int)d,(int)s); }
    void neg_r(R r) { rex(true,0,(int)r); emit(0xF7); modrm(3,3,(int)r); }
    void not_r(R r) { rex(true,0,(int)r); emit(0xF7); modrm(3,2,(int)r); }
    void xor_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x31); modrm(3,(int)s,(int)d); }
    void and_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x21); modrm(3,(int)s,(int)d); }
    // and rsp, -16 — force 16-byte stack alignment regardless of incoming parity.
    // Used once at a Windows process-exit call site (see emitHalt): unlike a normal
    // call site where the compiler tracks parity through the frame's push/sub count,
    // the exact rsp parity at an arbitrary emitHalt() call site isn't tracked, and
    // since control never returns here, clobbering the exact rsp value is free.
    void and_rsp_align16() { emit(0x48); emit(0x83); emit(0xE4); emit(0xF0); }
    void or_rr(R d, R s)  { rex(true,(int)s,(int)d); emit(0x09); modrm(3,(int)s,(int)d); }
    // cqo: sign-extend rax into rdx:rax
    void cqo() { emit(0x48); emit(0x99); }
    // idiv rcx: signed divide rdx:rax by rcx → rax=quotient, rdx=remainder
    void idiv_rcx() { emit(0x48); emit(0xF7); emit(0xF9); }
    void call_r(R r) { if ((int)r >= 8) emit(0x41); emit(0xFF); modrm(3, 2, (int)r); } // call reg
    // jmp reg (FF /4) — unlike call_r, does NOT push a return address. Needed for a real
    // longjmp-style non-local jump: by the time this fires, RSP has already been restored to
    // the try's own frame, so a `call` here would push onto (and corrupt) that frame's own
    // locals instead of transferring control cleanly.
    void jmp_r(R r) { if ((int)r >= 8) emit(0x41); emit(0xFF); modrm(3, 4, (int)r); }
    void shl_rax_cl() { emit(0x48); emit(0xD3); emit(0xE0); }  // shl rax, cl
    void sar_rax_cl() { emit(0x48); emit(0xD3); emit(0xF8); }  // sar rax, cl (arithmetic)
    void rdtsc() { emit(0x0F); emit(0x31); }                   // rdtsc → edx:eax
    void shl_r_i8(R r, uint8_t n) { rex(true,0,(int)r); emit(0xC1); modrm(3,4,(int)r); emit(n); } // shl r64, imm8
    void shr_r_i8(R r, uint8_t n) { rex(true,0,(int)r); emit(0xC1); modrm(3,5,(int)r); emit(n); } // shr r64, imm8
    // xor edx, edx (zero rdx for unsigned div)
    void xor_edx_edx() { emit(0x31); emit(0xD2); }
    // div rcx: unsigned divide rdx:rax by rcx
    void div_rcx() { emit(0x48); emit(0xF7); emit(0xF1); }
    // add dl, imm8
    void add_dl_i8(uint8_t v) { emit(0x80); emit(0xC2); emit(v); }
    // inc / dec
    void inc_r(R r) { rex(true,0,(int)r); emit(0xFF); modrm(3,0,(int)r); }
    void dec_r(R r) { rex(true,0,(int)r); emit(0xFF); modrm(3,1,(int)r); }
    // add r64, imm8
    void add_r_i8(R r, int8_t v) { rex(true,0,(int)r); emit(0x83); modrm(3,0,(int)r); emit((uint8_t)v); }
    // xor r64, imm8
    void xor_r_i8(R r, int8_t v) { rex(true,0,(int)r); emit(0x83); modrm(3,6,(int)r); emit((uint8_t)v); }
    // sub r64, imm32
    void sub_r_i32(R r, int32_t v) { rex(true,0,(int)r); emit(0x81); modrm(3,5,(int)r); emit32(v); }

    // ── Compare & Test ───────────────────────────────────────────────────────
    void cmp_rr(R a, R b) { rex(true,(int)b,(int)a); emit(0x39); modrm(3,(int)b,(int)a); }
    void cmp_r_i32(R r, int32_t v) { rex(true,0,(int)r); emit(0x81); modrm(3,7,(int)r); emit32(v); }
    void test_rr(R a, R b) { rex(true,(int)b,(int)a); emit(0x85); modrm(3,(int)b,(int)a); }
    // movzx r64, byte [ptr]  (load byte from memory, zero-extend to 64-bit)
    void movzx_r64_ptr8(R dst, R ptr) {
        uint8_t rxb = 0x48;
        if ((int)dst>=8) rxb |= 0x04; // REX.R
        if ((int)ptr>=8) rxb |= 0x01; // REX.B
        emit(rxb); emit(0x0F); emit(0xB6);
        int dl = (int)dst&7, pl = (int)ptr&7;
        if (pl==5) { modrm(1,dl,5); emit(0); }       // R13/RBP: disp8=0
        else if (pl==4) { modrm(0,dl,4); emit(0x24); } // R12/RSP: SIB
        else { modrm(0,dl,pl); }
    }
    // setcc: condition code → low byte of reg, then zero-extend
    void setcc_r(uint8_t cc, R r) {
        int ri = (int)r;
        if (ri>=4 && ri<=7) emit(0x40); // REX for spl/bpl/sil/dil
        else if (ri>=8) { emit(0x41); }
        emit(0x0F); emit(0x90+cc); modrm(3,0,ri);
        // movzx r64, r8
        rex(true,ri,ri); emit(0x0F); emit(0xB6); modrm(3,ri,ri);
    }

    // ── Jumps ────────────────────────────────────────────────────────────────
    void jmp(const std::string& lbl) {
        emit(0xE9); rel32Fixups[pos()] = lbl; emit32(0);
    }
    void jcc(uint8_t cc, const std::string& lbl) {
        emit(0x0F); emit(0x80+cc); rel32Fixups[pos()] = lbl; emit32(0);
    }
    void je(const std::string& l)   { jcc(0x04,l); }
    void jne(const std::string& l)  { jcc(0x05,l); }
    void jl(const std::string& l)   { jcc(0x0C,l); }
    void jg(const std::string& l)   { jcc(0x0F,l); }
    void jle(const std::string& l)  { jcc(0x0E,l); }
    void jge(const std::string& l)  { jcc(0x0D,l); }
    void jns(const std::string& l)  { jcc(0x09,l); }
    void jz(const std::string& l)   { je(l); }
    void jnz(const std::string& l)  { jne(l); }

    // ── Call & Ret ───────────────────────────────────────────────────────────
    void call(const std::string& lbl) {
        emit(0xE8); rel32Fixups[pos()] = lbl; emit32(0);
    }
    // lea d, [rip+label] — absolute address of a label (function pointers in lists)
    void lea_r_label(R d, const std::string& lbl) {
        rex(true, (int)d, 0); emit(0x8D); modrm(0, (int)d, 5);
        rel32Fixups[pos()] = lbl; emit32(0);
    }
    // call [mem64]: FF /2 with REX (indirect call through memory address in register)
    void call_rip_rel(const std::string& lbl) {
        // FF 15 rel32 = CALL [rip+rel32]
        emit(0xFF); emit(0x15); rel32Fixups[pos()] = lbl; emit32(0);
    }
    void ret() { emit(0xC3); }

    // ── Syscall ──────────────────────────────────────────────────────────────
    void syscall() { emit(0x0F); emit(0x05); }

    // ── SSE/XMM ──────────────────────────────────────────────────────────────
    // movq gpr, xmm0  — move 64-bit float bits from XMM0 to GPR (66 REX.W 0F 7E /r)
    void movq_gpr_from_xmm0(R gpr) {
        int g = (int)gpr;
        emit(0x66);
        emit(g >= 8 ? 0x49 : 0x48);  // REX.W (+ REX.B if r8..r15)
        emit(0x0F); emit(0x7E);
        emit((uint8_t)(0xC0 | (g & 7)));  // ModRM: mod=11, reg=xmm0=0, r/m=gpr
    }
    // movq xmm0, gpr  — move 64-bit float bits from GPR to XMM0 (66 REX.W 0F 6E /r)
    void movq_xmm0_from_gpr(R gpr) {
        int g = (int)gpr;
        emit(0x66);
        emit(g >= 8 ? 0x49 : 0x48);  // REX.W (+ REX.B if r8..r15)
        emit(0x0F); emit(0x6E);
        emit((uint8_t)(0xC0 | (g & 7)));  // ModRM: mod=11, reg=xmm0=0, r/m=gpr
    }
    // movq xmm1, gpr  — same but into XMM1 (66 REX.W 0F 6E /r, reg field=1)
    void movq_xmm1_from_gpr(R gpr) {
        int g = (int)gpr;
        emit(0x66);
        emit(g >= 8 ? 0x49 : 0x48);
        emit(0x0F); emit(0x6E);
        emit((uint8_t)(0xC0 | (1 << 3) | (g & 7)));  // ModRM: reg=xmm1=1, r/m=gpr
    }
    // Generic: movq xmmN, gpr (66 REX.W 0F 6E /r) — N in the ModRM reg field (#41: args ≥2)
    void movq_xmmN_from_gpr(int n, R gpr) {
        int g = (int)gpr;
        emit(0x66);
        emit(g >= 8 ? 0x49 : 0x48);
        emit(0x0F); emit(0x6E);
        emit((uint8_t)(0xC0 | ((n & 7) << 3) | (g & 7)));
    }
    // Generic: cvtsi2sd xmmN, gpr (F2 REX.W 0F 2A /r)
    void cvtsi2sd_xmmN_from_gpr(int n, R gpr) {
        int g = (int)gpr;
        emit(0xF2);
        emit(g >= 8 ? 0x49 : 0x48);
        emit(0x0F); emit(0x2A);
        emit((uint8_t)(0xC0 | ((n & 7) << 3) | (g & 7)));
    }
    // cvtsi2sd xmm0, gpr  — convert int64 GPR → double in XMM0 (F2 REX.W 0F 2A /r)
    void cvtsi2sd_xmm0_from_gpr(R gpr) {
        int g = (int)gpr;
        emit(0xF2);
        emit(g >= 8 ? 0x49 : 0x48);
        emit(0x0F); emit(0x2A);
        emit((uint8_t)(0xC0 | (g & 7)));  // ModRM: reg=xmm0=0, r/m=gpr
    }
    // cvtsi2sd xmm1, gpr  — convert int64 GPR → double in XMM1
    void cvtsi2sd_xmm1_from_gpr(R gpr) {
        int g = (int)gpr;
        emit(0xF2);
        emit(g >= 8 ? 0x49 : 0x48);
        emit(0x0F); emit(0x2A);
        emit((uint8_t)(0xC0 | (1 << 3) | (g & 7)));  // ModRM: reg=xmm1=1, r/m=gpr
    }

    // divsd xmm0, xmm1  — xmm0 /= xmm1 (double division)
    void divsd_xmm0_xmm1() { emit(0xF2); emit(0x0F); emit(0x5E); emit(0xC1); }
    // subsd xmm0, xmm1  — xmm0 -= xmm1
    void addsd_xmm0_xmm1() { emit(0xF2); emit(0x0F); emit(0x58); emit(0xC1); } // xmm0 += xmm1
    void subsd_xmm0_xmm1() { emit(0xF2); emit(0x0F); emit(0x5C); emit(0xC1); }
    // mulsd xmm0, xmm1  — xmm0 *= xmm1
    void mulsd_xmm0_xmm1() { emit(0xF2); emit(0x0F); emit(0x59); emit(0xC1); }
    // ucomisd xmm0, xmm1 — compare; sets ZF/PF/CF (no flags for NaN)
    void ucomisd_xmm0_xmm1() { emit(0x66); emit(0x0F); emit(0x2E); emit(0xC1); }
    // cvttsd2si rax, xmm0 — truncate double in xmm0 to int64 in rax
    void cvttsd2si_rax_xmm0() { emit(0xF2); emit(0x48); emit(0x0F); emit(0x2C); emit(0xC0); }
    // cvttsd2si r13, xmm0
    void cvttsd2si_r13_xmm0() { emit(0xF2); emit(0x4C); emit(0x0F); emit(0x2C); emit(0xE8); }
    // jns — jump if SF=0 (non-negative)
    // add r64, imm32: REX.W 81 /0 r imm32
    void add_ri32(R r, int32_t v) {
        if (v >= -128 && v <= 127) { rex(true,0,(int)r); emit(0x83); modrm(3,0,(int)r); emit((uint8_t)(int8_t)v); }
        else { rex(true,0,(int)r); emit(0x81); modrm(3,0,(int)r); emit32(v); }
    }

    // ── PLT / GOT ────────────────────────────────────────────────────────────
    // PLT[0]: lazy resolver stub (16 bytes)
    void emitPLT0() {
        label("__plt0__");
        // push QWORD [rip + got.plt[1]]
        emit(0xFF); emit(0x35); gotPltFixups_[pos()] = -1; emit32(0);
        // jmp  QWORD [rip + got.plt[2]]
        emit(0xFF); emit(0x25); gotPltFixups_[pos()] = -2; emit32(0);
        // nop pad to 16 bytes
        emit(0x0F); emit(0x1F); emit(0x40); emit(0x00);
    }
    // PLT[symIdx+1]: function stub (16 bytes). Label irName so existing CALL resolves here.
    void emitPLTStub(int symIdx, const std::string& irName) {
        label(irName);
        // jmp QWORD [rip + got.plt[3+symIdx]]
        emit(0xFF); emit(0x25); gotPltFixups_[pos()] = symIdx; emit32(0);
        // push symIdx (relocation index for lazy resolver)
        emit(0x68); emit32(symIdx);
        // jmp PLT[0]
        jmp("__plt0__");
    }
    // Patch all FF 25/FF 35 rel32 fields after final layout is known.
    void applyGOTPLTFixups(uint64_t textVA, uint64_t gotpltVA) {
        for (auto& [off, idx] : gotPltFixups_) {
            uint64_t gotEntry = (idx == -1) ? gotpltVA + 8
                              : (idx == -2) ? gotpltVA + 16
                              : gotpltVA + (uint64_t)(3 + idx) * 8;
            int64_t  rel   = (int64_t)gotEntry - (int64_t)(textVA + off + 4);
            int32_t  r32   = (int32_t)rel;
            buf[off+0] = (uint8_t)r32;        buf[off+1] = (uint8_t)(r32>>8);
            buf[off+2] = (uint8_t)(r32>>16);  buf[off+3] = (uint8_t)(r32>>24);
        }
    }
    bool hasPLT() const { return !gotPltFixups_.empty(); }
};

// ─── ABI Info ─────────────────────────────────────────────────────────────────
struct ABI {
    std::vector<R> argRegs;
    bool hasRedZone;
    int shadowSpace;
};

static ABI sysv_abi() {
    return { {R::RDI,R::RSI,R::RDX,R::RCX,R::R8,R::R9}, true, 0 };
}
static ABI win64_abi() {
    return { {R::RCX,R::RDX,R::R8,R::R9}, false, 32 };
}

// Real cross-compile target selector, set once at the top of BinaryCompiler::compile()
// from a CLI flag. NOT the same thing as the old `TARGET_WINDOWS`/`_WIN32` macros
// above, which detect the HOST the `ac` binary itself runs on — useless for producing
// a Windows .exe from a Linux-hosted compiler, which is the actual use case here.
static bool g_bnyTargetWindows = false;

static ABI host_abi() {
    return g_bnyTargetWindows ? win64_abi() : sysv_abi();
}

// ─── String Pool ─────────────────────────────────────────────────────────────
class StringPool {
    std::vector<std::string> strs;
    std::unordered_map<std::string,int> index;
public:
    int add(const std::string& s) {
        auto it = index.find(s);
        if (it != index.end()) return it->second;
        int id = (int)strs.size();
        strs.push_back(s);
        index[s] = id;
        return id;
    }
    // Build byte array: each string is null-terminated
    std::vector<uint8_t> build(std::vector<size_t>& offsets) const {
        std::vector<uint8_t> data;
        offsets.resize(strs.size());
        for (int i = 0; i < (int)strs.size(); i++) {
            offsets[i] = data.size();
            for (char c : strs[i]) data.push_back((uint8_t)c);
            data.push_back(0);
        }
        return data;
    }
    size_t size() const { return strs.size(); }
    const std::string& get(int i) const { return strs[i]; }
};

// ─── Function Frame ───────────────────────────────────────────────────────────
// Maps temp IDs and symbol IDs to rbp-relative offsets
class FuncFrame {
    // Offsets are assigned by the linear scan in scanInstrs (nextSlot++), not by map order; these
    // maps only store/retrieve the already-computed offset → unordered_map is byte-identical + O(1).
    std::unordered_map<int,int32_t> tempOff;  // temp_id → rbp offset (negative)
    std::unordered_map<int,int32_t> varOff;   // symbol_id → rbp offset (negative)
    int nextSlot = 0;   // slot counter (each = 8 bytes)
    int baseSlot = 0;   // first slot available for temps/vars (slots 0..base-1 are
                        // reserved for callee-save pushes which live at rbp-8..rbp-8*base)

    int32_t slotToOff(int slot) const { return -(slot+1)*8; }

public:
    // Call BEFORE scanInstrs when the prologue will push N callee-saved registers.
    // Those pushes land at rbp-8..rbp-8*n, so temp/var slots must start below them.
    void setCalleeSaveBase(int n) { baseSlot = n; nextSlot = n; }

    void scanInstrs(const std::vector<AC_IR::IRInstruction>& instrs) {
        for (auto& ins : instrs) {
            auto scanRef = [&](const AC_IR::IRRef& r) {
                if (r.kind == AC_IR::IRRef::Kind::TEMP && tempOff.find(r.id) == tempOff.end())
                    tempOff[r.id] = slotToOff(nextSlot++);
                else if (r.kind == AC_IR::IRRef::Kind::VAR && varOff.find(r.id) == varOff.end())
                    varOff[r.id] = slotToOff(nextSlot++);
            };
            scanRef(ins.result);
            for (auto& op : ins.typedOperands) scanRef(op);
        }
    }

    int32_t tempOffset(int id) {
        auto it = tempOff.find(id);
        if (it != tempOff.end()) return it->second;
        tempOff[id] = slotToOff(nextSlot++);
        return tempOff[id];
    }
    int32_t varOffset(int symId) {
        auto it = varOff.find(symId);
        if (it != varOff.end()) return it->second;
        varOff[symId] = slotToOff(nextSlot++);
        return varOff[symId];
    }

    // Frame size in bytes for sub_rsp: only counts slots BELOW the callee-save pushes.
    // The callee-save region (slots 0..baseSlot-1) is handled by push_r instructions
    // and must NOT be re-counted here.
    int32_t frameSize() const {
        int32_t s = (nextSlot - baseSlot) * 8;
        s = ((s + 15) / 16) * 16;
        return s ? s : 16;
    }
};

// ─── Live Interval Analysis ───────────────────────────────────────────────────
struct LiveInterval {
    int tempId = -1;
    int start  = INT_MAX;
    int end    = INT_MIN;
};

// Detect loop regions as [labelIndex, backJumpIndex] for every backward jump (a jump whose
// target label is defined earlier). Used to keep loop-carried values allocated for the whole
// loop body so a temp defined later in the body can't steal a register live across the back-edge.
static std::vector<std::pair<int,int>> computeLoopRegions(
    const std::vector<AC_IR::IRInstruction>& instrs)
{
    using namespace AC_IR;
    std::map<int,int> labelPos;
    for (int i = 0; i < (int)instrs.size(); i++) {
        const auto& ins = instrs[i];
        if (ins.opcode == IROpcode::LABEL && !ins.typedOperands.empty()
                && ins.typedOperands[0].kind == IRRef::Kind::LABEL)
            labelPos[ins.typedOperands[0].id] = i;
    }
    std::vector<std::pair<int,int>> loops;
    for (int i = 0; i < (int)instrs.size(); i++) {
        const auto& ins = instrs[i];
        int target = -1;
        if (ins.opcode == IROpcode::JUMP && !ins.typedOperands.empty()
                && ins.typedOperands[0].kind == IRRef::Kind::LABEL)
            target = ins.typedOperands[0].id;
        else if ((ins.opcode == IROpcode::JUMP_IF_FALSE || ins.opcode == IROpcode::JUMP_IF_TRUE)
                 && ins.typedOperands.size() >= 2 && ins.typedOperands[1].kind == IRRef::Kind::LABEL)
            target = ins.typedOperands[1].id;
        if (target >= 0) {
            auto it = labelPos.find(target);
            if (it != labelPos.end() && it->second < i) loops.push_back({it->second, i});
        }
    }
    return loops;
}

static std::vector<LiveInterval> computeLiveIntervals(
    const std::vector<AC_IR::IRInstruction>& instrs)
{
    std::map<int, LiveInterval> ivMap;
    for (int i = 0; i < (int)instrs.size(); i++) {
        auto& ins = instrs[i];
        auto touch = [&](const AC_IR::IRRef& r) {
            if (r.kind != AC_IR::IRRef::Kind::TEMP) return;
            auto& iv  = ivMap[r.id];
            iv.tempId = r.id;
            iv.start  = std::min(iv.start, i);
            iv.end    = std::max(iv.end,   i);
        };
        touch(ins.result);
        for (auto& op : ins.typedOperands) touch(op);
    }
    // Loop-carried liveness: any interval overlapping a loop must stay live through the
    // back-edge, else a temp defined later in the body reuses its register and clobbers it
    // on the next iteration. Extend the end to the loop's end; iterate to a fixpoint for
    // nested loops.
    auto loops = computeLoopRegions(instrs);
    if (!loops.empty()) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& [id, iv] : ivMap)
                for (auto& [ls, le] : loops)
                    if (iv.start <= le && iv.end >= ls && iv.end < le) { iv.end = le; changed = true; }
        }
    }
    std::vector<LiveInterval> result;
    result.reserve(ivMap.size());
    for (auto& [id, iv] : ivMap) result.push_back(iv);
    std::sort(result.begin(), result.end(),
        [](const LiveInterval& a, const LiveInterval& b){ return a.start < b.start; });
    return result;
}

// ─── Linear Scan Register Allocator ──────────────────────────────────────────
// Allocates TEMPs to callee-saved registers {RBX,R12,R13,R14,R15}.
// Callee-saved means these survive across CALL instructions without any extra
// spill logic — the called function preserves them for us.
class LinearScanAlloc {
public:
    // Allocation pool — callee-saved only, safe across all CALLs
    static const R     POOL[5];
    static const int   POOL_SIZE = 5;

    std::unordered_map<int, R> tempReg;        // temp_id → allocated register
    std::set<R>                usedCalleeSaved; // registers that need push/pop

    void run(const std::vector<AC_IR::IRInstruction>& instrs) {
        tempReg.clear();
        usedCalleeSaved.clear();

        auto intervals = computeLiveIntervals(instrs);
        if (intervals.empty()) return;

        std::set<int> freeIdx;
        for (int i = 0; i < POOL_SIZE; i++) freeIdx.insert(i);

        // active: end → tempId (multimap in case two intervals end on same instr)
        std::multimap<int,int> active;

        for (auto& iv : intervals) {
            // Expire intervals whose live range ended before this one starts
            for (auto it = active.begin();
                 it != active.end() && it->first < iv.start; ) {
                for (int i = 0; i < POOL_SIZE; i++)
                    if (POOL[i] == tempReg[it->second]) { freeIdx.insert(i); break; }
                it = active.erase(it);
            }

            if (freeIdx.empty()) {
                // Spill: evict the interval with the furthest end if it extends
                // beyond ours; otherwise skip (spill current).
                auto lastIt = std::prev(active.end());
                if (!active.empty() && lastIt->first > iv.end) {
                    int spillTid  = lastIt->second;
                    R   reg       = tempReg[spillTid];
                    tempReg.erase(spillTid);
                    active.erase(lastIt);
                    tempReg[iv.tempId] = reg;
                    usedCalleeSaved.insert(reg);
                    active.insert({iv.end, iv.tempId});
                }
                // else: iv stays in stack (no register assigned)
            } else {
                int idx = *freeIdx.begin();
                freeIdx.erase(freeIdx.begin());
                tempReg[iv.tempId] = POOL[idx];
                usedCalleeSaved.insert(POOL[idx]);
                active.insert({iv.end, iv.tempId});
            }
        }
    }

    bool hasReg(int tempId) const { return tempReg.count(tempId) > 0; }
    R    getReg(int tempId) const { return tempReg.at(tempId); }
};

const R LinearScanAlloc::POOL[5] = {R::RBX, R::R12, R::R13, R::R14, R::R15};

// ─── Function Compiler ───────────────────────────────────────────────────────
// Returns the no-arg C function that produces a math constant, or "" if not a constant
static std::string mathConstantFunc(const std::string& name) {
    static const std::map<std::string,std::string> tbl = {
        {"math.pi",  "ac_math_pi_const"},
        {"math.e",   "ac_math_e_const"},
        {"math.phi", "ac_math_phi_const"},
        {"math.tau", "ac_math_tau_const"},
        {"math.em",  "ac_math_em_const"},
        {"math.inf", "ac_math_inf"},
    };
    auto it = tbl.find(name);
    return it != tbl.end() ? it->second : "";
}

static bool isIntReturningMathCall(const std::string& irName) {
    static const std::set<std::string> intReturning = {
        "math.to_int", "math.abs_int", "math.mod_int",
        "math.gcd",    "math.lcm",     "math.is_prime",
    };
    return intReturning.find(irName) != intReturning.end();
}

// Returns true if an external call returns a double in XMM0 (System V ABI float return)
// ilib functions returning char* — their PRINT must go through __ac_print_cstr__,
// otherwise the pointer prints as an integer (the classic stack-address output).
static bool returnsCString(const std::string& irName) {
    static const std::set<std::string> cstr = {
        "os.cwd", "os.env", "os.bash", "os.sbash", "os.read_from",
        "regex.search", "regex.replace", "regex.escape",
        "stringm.upper", "stringm.lower", "stringm.strip", "stringm.trim",
        "stringm.replace", "stringm.b", "stringm.f", "stringm.t", "stringm.format", "stringm.getline",
        "web.page_get", "web.help",
        "server.db_run", "server.db_run_p", "server.db_import",
        "server.db_reset", "server.help",
        "server.req_method", "server.req_path", "server.req_query",
        "server.req_body", "server.req_header",
        "maudio.listen",
    };
    return cstr.count(irName) > 0;
}

static bool isFloatReturningCall(const std::string& irName) {
    return acCallReturnsFloat(irName);   // single authority in type.hpp (see acCallReturnsFloat)
}

static bool callArgTakesDouble(const std::string& irName, int argIndex) {
    if (irName == "ml.tensor" || irName == "ml_tensor")
        return argIndex == 0;
    if (irName == "ml.grid" || irName == "ml_grid")
        return argIndex == 2;
    if (irName == "ml.weights" || irName == "ml_weights")
        return argIndex == 0;
    if (irName == "ml.optimize" || irName == "ml_optimize")
        return argIndex == 0;
    if (irName.rfind("math.", 0) == 0)
        return !isIntReturningMathCall(irName);
    return false;
}

// Shared with BinaryCompiler::collectExternalSymbols() (a separate class further down this
// file, needs the same widget-kind classification to register the REAL `ac_widgets_*` PLT
// symbols — see widgetVarKind_'s comment in FuncCompiler for the full story). Free functions
// so both classes can see them regardless of declaration order.
static bool bnyIsWidgetCtorName(const std::string& func) {
    static const std::set<std::string> ctors = {
        "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
        "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch", "textbox"
    };
    return ctors.count(func) > 0;
}
static std::string bnyWidgetPackFn(const std::string& kind) {
    if (kind == "Screen") return "";
    if (kind == "radbtn") return "ac_widgets_ckbtn_pack";
    return "ac_widgets_" + kind + "_pack";
}
static std::string bnyWidgetGetFn(const std::string& kind) {
    if (kind == "radbtn") return "ac_widgets_ckbtn_get";
    return "ac_widgets_" + kind + "_get";
}
static std::string bnyWidgetSetFn(const std::string& kind) {
    if (kind == "radbtn") return "ac_widgets_ckbtn_set";
    return "ac_widgets_" + kind + "_set";
}
static std::string bnyWidgetNewFn(const std::string& func) {
    if (!bnyIsWidgetCtorName(func)) return "";
    if (func == "Screen") return "ac_widgets_screen_new";
    if (func == "ckbtn" || func == "radbtn") return "ac_widgets_ckbtn_new";
    return "ac_widgets_" + func + "_new";
}

// One logic base, driven by any emitter that speaks the X64Emitter instruction API.
// Instantiated as FuncCompiler<X64Emitter> for BNY (machine-code bytes); a future
// FuncCompiler<NasmEmitter> emits the same logic as NASM text. Duck-typed on `Em`:
// the body only ever calls em.<instruction>() — never touches byte-buffer machinery
// (pos/code/fixups/PLT live on X64Emitter and are used only by the ELF driver).
template<class Em>
class FuncCompiler {
public:
    std::set<std::string>*  floatFuncs_ = nullptr; // set by BinaryCompiler; shared across funcs
    std::set<std::string>*  stringFuncs_ = nullptr; // user fns returning char* (shared)
    std::set<std::string>*  arrayFuncs_ = nullptr;  // user fns returning list blocks (shared)
    std::set<std::string>   forcedStringParams_;
    std::set<std::string>   forcedFloatParams_;   // params a caller passes a float to (→ load as double)
    // NA→free: free-var names that live in shared global slots, and the name→slot map (shared).
    std::set<std::string>*       promotedGlobals_ = nullptr;
    std::map<std::string,int>*   gvarSlots_       = nullptr;
    bool                         usesSave_        = false; // program uses `save as` (set by orchestrator)
    bool                         usesTry_         = false; // program uses try/catch (set by orchestrator)
    bool                         usesGenerators_  = false; // program has a `yield` generator (set by orchestrator)
    bool                         curFnIsGenerator_ = false; // true while compiling a generator body

    // `yield`/generators: state-block field offsets (byte offsets into a heap-allocated block —
    // see compileGeneratorFn's own comment for the full design). Both halves (gen-side and
    // caller-side) save/restore the SAME register set — RSP/RBP/RBX/R12-R15 — because that's
    // BOTH the exact set a fiber switch needs to preserve AND, not by coincidence,
    // LinearScanAlloc::POOL's own allocation pool: any live temp the register allocator ever put
    // in one of these registers survives a swap for free, symmetrically, with no extra spill
    // logic anywhere (see emitFiberSwap's own comment).
    static const int GEN_OFF_DONE        = 0;
    static const int GEN_OFF_VALUE       = 8;
    static const int GEN_OFF_GEN_RSP     = 16;
    static const int GEN_OFF_GEN_RBP     = 24;
    static const int GEN_OFF_GEN_RBX     = 32;
    static const int GEN_OFF_GEN_R12     = 40;
    static const int GEN_OFF_GEN_R13     = 48;
    static const int GEN_OFF_GEN_R14     = 56;
    static const int GEN_OFF_GEN_R15     = 64;
    static const int GEN_OFF_GEN_RIP     = 72;
    static const int GEN_OFF_CALLER_RSP  = 80;
    static const int GEN_OFF_CALLER_RBP  = 88;
    static const int GEN_OFF_CALLER_RBX  = 96;
    static const int GEN_OFF_CALLER_R12  = 104;
    static const int GEN_OFF_CALLER_R13  = 112;
    static const int GEN_OFF_CALLER_R14  = 120;
    static const int GEN_OFF_CALLER_R15  = 128;
    static const int GEN_OFF_CALLER_RIP  = 136;
    static const int GEN_HEADER_BYTES    = 144;  // args[] start here
    static const int GEN_STACK_BYTES     = 65536;

    // Loads the currently-active generator's state-block pointer (the "am I resuming, and which
    // one" context a fiber-native design gets for free from its call stack — BNY has to thread it
    // through this one global slot instead, same as every other backend's ac_gen_cur/lastGen*).
    void loadGenCur(R dst) {
        int slot = (*gvarSlots_)["__ac_gen_cur"];
        em.mov_ri64_gvar(dst, slot);
        em.mov_r_ptr(dst, dst);
    }

    // The actual two-way context switch. `state` holds the state-block pointer (MUST NOT be one
    // of RSP/RBP/RBX/R12-R15 — those are exactly the registers being read/overwritten mid-
    // sequence, so the base pointer itself would be corrupted partway through; callers always
    // pass R11, which is caller-saved/volatile and never in LinearScanAlloc::POOL). Symmetric:
    // save the currently-live side's registers + a resume label into its own half of the state
    // block, then load the OTHER side's saved registers + jump to ITS resume point. Used by
    // GEN_NEXT (caller→gen) and by YIELD/a generator's RETURN (gen→caller).
    void emitFiberSwap(R state, bool intoGenerator) {
        int selfRSP  = intoGenerator ? GEN_OFF_CALLER_RSP : GEN_OFF_GEN_RSP;
        int selfRBP  = intoGenerator ? GEN_OFF_CALLER_RBP : GEN_OFF_GEN_RBP;
        int selfRBX  = intoGenerator ? GEN_OFF_CALLER_RBX : GEN_OFF_GEN_RBX;
        int selfR12  = intoGenerator ? GEN_OFF_CALLER_R12 : GEN_OFF_GEN_R12;
        int selfR13  = intoGenerator ? GEN_OFF_CALLER_R13 : GEN_OFF_GEN_R13;
        int selfR14  = intoGenerator ? GEN_OFF_CALLER_R14 : GEN_OFF_GEN_R14;
        int selfR15  = intoGenerator ? GEN_OFF_CALLER_R15 : GEN_OFF_GEN_R15;
        int selfRIP  = intoGenerator ? GEN_OFF_CALLER_RIP : GEN_OFF_GEN_RIP;
        int otherRSP = intoGenerator ? GEN_OFF_GEN_RSP : GEN_OFF_CALLER_RSP;
        int otherRBP = intoGenerator ? GEN_OFF_GEN_RBP : GEN_OFF_CALLER_RBP;
        int otherRBX = intoGenerator ? GEN_OFF_GEN_RBX : GEN_OFF_CALLER_RBX;
        int otherR12 = intoGenerator ? GEN_OFF_GEN_R12 : GEN_OFF_CALLER_R12;
        int otherR13 = intoGenerator ? GEN_OFF_GEN_R13 : GEN_OFF_CALLER_R13;
        int otherR14 = intoGenerator ? GEN_OFF_GEN_R14 : GEN_OFF_CALLER_R14;
        int otherR15 = intoGenerator ? GEN_OFF_GEN_R15 : GEN_OFF_CALLER_R15;
        int otherRIP = intoGenerator ? GEN_OFF_GEN_RIP : GEN_OFF_CALLER_RIP;

        em.mov_based_r(state, selfRSP, R::RSP);
        em.mov_based_r(state, selfRBP, R::RBP);
        em.mov_based_r(state, selfRBX, R::RBX);
        em.mov_based_r(state, selfR12, R::R12);
        em.mov_based_r(state, selfR13, R::R13);
        em.mov_based_r(state, selfR14, R::R14);
        em.mov_based_r(state, selfR15, R::R15);
        std::string resumeL = uniq("__ac_gen_resume_" + std::to_string(catchCounter_++) + "__");
        em.lea_r_label(R::RAX, resumeL);        // RAX: safe scratch, `state` is R11 (see above)
        em.mov_based_r(state, selfRIP, R::RAX);

        em.mov_r_based(R::RSP, state, otherRSP);
        em.mov_r_based(R::RBP, state, otherRBP);
        em.mov_r_based(R::RBX, state, otherRBX);
        em.mov_r_based(R::R12, state, otherR12);
        em.mov_r_based(R::R13, state, otherR13);
        em.mov_r_based(R::R14, state, otherR14);
        em.mov_r_based(R::R15, state, otherR15);
        em.mov_r_based(R::RAX, state, otherRIP);
        em.jmp_r(R::RAX);
        em.label(resumeL);
    }
    // `return` inside a generator body (explicit, or the implicit trailing one every function
    // body ends with): mark done, swap back to the caller. No resume label is ever jumped back
    // to afterward (GEN_NEXT's own done-check guards that), but emitFiberSwap emits one anyway —
    // harmless, and keeps this one code path shared instead of a near-duplicate without it.
    void emitGenReturnSwap() {
        loadGenCur(R::R11);
        em.mov_ri32(R::RAX, 1);
        em.mov_based_r(R::R11, GEN_OFF_DONE, R::RAX);
        emitFiberSwap(R::R11, false);
    }
    // Bundle/class — shared (pointer, like gvarSlots_/promotedGlobals_) since instanceClass_ is
    // mutated live as CONSTRUCT calls are compiled, visible across whichever FuncCompiler
    // instance (global-section or a specific function) processes a given statement.
    std::map<std::string, std::vector<std::string>>* classFields_ = nullptr;
    std::map<std::string, std::string>* instanceClass_ = nullptr;
    std::map<std::string, std::set<std::string>>* classStringFields_ = nullptr;
    std::map<std::string, std::string>* classReturnFuncs_ = nullptr;   // fn.name -> class name (see the prescan's comment)
    // widgets ilib var-kind map — pointer (like classFields_/instanceClass_ above) so a
    // WHOLE-PROGRAM prescan (a widget var built in one function, e.g. `<mainloop>`, is routinely
    // used from another — an on_click callback declared earlier in source, scanned first)
    // populates one shared map BEFORE any FuncCompiler starts codegen, instead of each
    // function's own fresh FuncCompiler instance only ever seeing ctors from its OWN body
    // (verified: applicant_form.ac's `pos_drop.get()`, called from a callback declared before
    // `<mainloop>`'s `pos_drop = dropdown(...)` runs, needs `pos_drop`'s kind known from
    // function-scan #1 while its ctor is only ever seen in function-scan #2). See
    // isWidgetCtorName's comment below for the "undefined label 'display'" bug this whole
    // module (bnyWidgetCtor/bnyWidgetMethod + collectExternalSymbols' matching prescan) fixes.
    std::map<std::string, std::string>* widgetVarKind_ = nullptr;
    std::string currentClass_;   // set in compileFn from fn.classOwner; empty outside a method
    int fieldOffset(const std::string &cls, const std::string &field) const {
        if (!classFields_) return -1;
        auto it = classFields_->find(cls);
        if (it == classFields_->end()) return -1;
        for (size_t i = 0; i < it->second.size(); i++)
            if (it->second[i] == field) return (int)(8 * i);
        return -1;
    }
    bool resolveFieldAccess(const std::string &name, std::string &base, int &offset) const {
        auto dot = name.find('.');
        if (dot == std::string::npos) return false;
        base = name.substr(0, dot);
        std::string field = name.substr(dot + 1);
        std::string cls = (base == "self") ? currentClass_
                         : (instanceClass_ && instanceClass_->count(base) ? instanceClass_->at(base) : std::string());
        if (cls.empty()) return false;
        offset = fieldOffset(cls, field);
        return offset >= 0;
    }
    // Synthetic symbol ID for a method's `self` parameter — see compileFn's comment on why "self"
    // can never be found via the normal name-matching scan (it's never a bare VAR in the IR,
    // only ever fused into compound names like "self.hp"). Must be a value no real symbol ID or
    // TEMP id will ever collide with; both are non-negative small integers in this compiler.
    static constexpr int kSelfSymId_ = -777001;
    // Loads a plain (non-field) variable NAME's value into `scratch` — used to get the object
    // pointer a field access needs to add its offset to.
    void loadNamedVar(const std::string &name, R scratch) {
        if (name == "self") { em.mov_r_rbp(scratch, frame.varOffset(kSelfSymId_)); return; }
        int gs = gvarSlotOf(name);
        if (gs >= 0) { em.mov_ri64_gvar(scratch, gs); em.mov_r_ptr(scratch, scratch); return; }
        auto it = localVarIds_.find(name);
        if (it != localVarIds_.end()) { em.mov_r_rbp(scratch, frame.varOffset(it->second)); return; }
        em.mov_ri32(scratch, 0);   // shouldn't happen — name wasn't a known var
    }
private:
    // var name → symbol id within the function currently being compiled (for `arr.append`,
    // whose receiver array is encoded in the LIB_CALL method name, not as an operand).
    std::map<std::string,int> localVarIds_;
    void buildLocalVarIds(const std::vector<AC_IR::IRInstruction>& instrs) {
        localVarIds_.clear();
        auto add = [&](const AC_IR::IRRef& r) {
            if (r.kind == AC_IR::IRRef::Kind::VAR && r.id >= 0) {
                std::string n = prog.symbols.getName(r.id);
                if (!n.empty()) localVarIds_.emplace(n, r.id);
            }
        };
        for (auto& ins : instrs) { add(ins.result); for (auto& op : ins.typedOperands) add(op); }
    }
    // Slot id for a VAR that is a promoted free var, or -1 if it's an ordinary stack local.
    int gvarSlotOf(const std::string& name) const {
        if (!promotedGlobals_ || !gvarSlots_ || name.empty()) return -1;
        if (!promotedGlobals_->count(name)) return -1;
        auto it = gvarSlots_->find(name);
        return it == gvarSlots_->end() ? -1 : it->second;
    }
    const AC_IR::IRProgram& prog;
    Em&                     em;
    StringPool&             sp;
    ABI                     abi;
    FuncFrame               frame;
    int32_t                 fsize = 0;
    bool                    isGlobal;
    LinearScanAlloc         regAlloc;
    std::vector<R>          calleeSaves; // push/pop order for this function
    std::set<int>           floatTempIds_;   // temp IDs from float-returning calls
    std::set<std::string>   floatVarNames_;  // var names assigned from float-returning calls
    std::set<std::string>   stringVarNames_; // var names holding string pointers (LongInt big values)
    std::set<int>           stringTempIds_;  // temp IDs holding char* (ilib string returns)
    std::set<std::string>   arrayVarNames_;  // var names holding list blocks ([len][e0]…)
    std::set<int>           arrayTempIds_;   // temp IDs holding list blocks
    std::set<std::string>   dictVarNames_;   // var names holding dict blocks ([n][k0][v0]…)
    std::set<int>           dictTempIds_;    // temp IDs holding dict blocks
    std::set<std::string>   listOfDictVarNames_; // list vars whose elements are dict-block pointers
    std::set<int>           listOfDictTempIds_;
    // Per-dict-var (and per-dict-TEMP) set of keys whose LITERAL value is a `$..$` string —
    // a dict is naturally heterogeneous at runtime (every value slot is just an int64), but a
    // LOAD_INDEX with a compile-time-constant string key ("name") needs to know statically
    // whether to print/consume the result as a string or an int64. Populated by a content-string
    // prescan over every ALLOC "dict" site (below) before codegen runs, same fixpoint-adjacent
    // spirit as the dict/list-of-dict var-kind propagation.
    std::map<std::string, std::set<std::string>> dictStrKeysByVar_;
    std::map<int, std::set<std::string>>         dictStrKeysByTemp_;
    std::set<std::string> dictStrKeysFor(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::TEMP) {
            auto it = dictStrKeysByTemp_.find(r.id);
            if (it != dictStrKeysByTemp_.end()) return it->second;
        } else if (r.kind == AC_IR::IRRef::Kind::VAR) {
            auto it = dictStrKeysByVar_.find(varName(r));
            if (it != dictStrKeysByVar_.end()) return it->second;
        }
        return {};
    }
    void mergeDictStrKeys(const AC_IR::IRRef& dst, const std::set<std::string>& keys) {
        if (keys.empty()) return;
        if (dst.kind == AC_IR::IRRef::Kind::TEMP)
            dictStrKeysByTemp_[dst.id].insert(keys.begin(), keys.end());
        else if (dst.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(dst);
            if (!vn.empty()) dictStrKeysByVar_[vn].insert(keys.begin(), keys.end());
        }
    }
    // Returns true if merging `keys` into dst actually grew its recorded set (fixpoint signal).
    bool mergeDictStrKeysChanged(const AC_IR::IRRef& dst, const std::set<std::string>& keys) {
        size_t before = dictStrKeysFor(dst).size();
        mergeDictStrKeys(dst, keys);
        return dictStrKeysFor(dst).size() != before;
    }
    std::set<std::string>   atomicVarNames_; // var names declared `atomic` — lock-guarded stores
    // widgets ilib: `use ilib widgets` exposes bare-name constructor calls (`display(root,...)`)
    // and dot-method calls on the returned handle (`lang_drop.add(...)`) — every other backend
    // (CStrategy's cWidgetCtor/cWidgetMethod is the reference this ports) expands these into the
    // real `ac_widgets_*_new`/`_pack`/`_set`/`_get`/`_add` calls. BNY had NONE of this: a bare
    // `display(...)` fell straight through the generic CALL path to `em.call("display")` — a
    // label that exists nowhere (not even in the .so, which only exports `ac_widgets_display_new`
    // etc), the exact "undefined label 'display'" this session's audit found.
    static bool isWidgetCtorName(const std::string& func) {
        static const std::set<std::string> ctors = {
            "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
            "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch", "textbox"
        };
        return ctors.count(func) > 0;
    }
    static std::string widgetPackFn(const std::string& kind) {
        if (kind == "Screen") return "";
        if (kind == "radbtn") return "ac_widgets_ckbtn_pack";
        return "ac_widgets_" + kind + "_pack";
    }
    static std::string widgetGetFn(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_get";
        return "ac_widgets_" + kind + "_get";
    }
    static std::string widgetSetFn(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_set";
        return "ac_widgets_" + kind + "_set";
    }
    int userFuncArity(const std::string& name) const {
        for (auto& fn : prog.functions) if (fn.name == name) return (int)fn.parameters.size();
        return 0;
    }
    void loadArgOrInt(const std::vector<AC_IR::IRRef>& args, size_t i, int64_t def, R reg) {
        if (i < args.size()) load(args[i], reg);
        else em.mov_ri64(reg, (uint64_t)def);
    }
    void loadArgOrStr(const std::vector<AC_IR::IRRef>& args, size_t i, const std::string& def, R reg) {
        if (i < args.size()) load(args[i], reg);
        else { int sid = sp.add(def); em.mov_ri64_str(reg, sid); }
    }
    // Constructor: `res = display(root, "text")` etc. Returns false (nothing emitted) if `func`
    // isn't a widget constructor name, so callers can fall through to the generic CALL path.
    bool bnyWidgetCtor(const AC_IR::IRInstruction& ins, const std::string& func,
                       const std::vector<AC_IR::IRRef>& args) {
        if (!isWidgetCtorName(func) || !ins.result.isValid()) return false;
        struct A { bool isStr; std::string def; };
        std::string newFn; std::vector<A> specs;
        // title is mandatory (enforced at the ir.cpp level) — no geometry arg; use
        // .dimensions(w, h) to size the window.
        if (func == "Screen") { newFn = "ac_widgets_screen_new"; specs = {{true,"AC App"}}; }
        else if (func == "display") { newFn = "ac_widgets_display_new"; specs = {{false,"0"},{true,""}}; }
        else if (func == "ask") { newFn = "ac_widgets_ask_new"; specs = {{false,"0"},{false,"20"}}; }
        else if (func == "btn") { newFn = "ac_widgets_btn_new"; specs = {{false,"0"},{true,"Button"}}; }
        else if (func == "ckbtn" || func == "radbtn") { newFn = "ac_widgets_ckbtn_new"; specs = {{false,"0"},{true,""}}; }
        else if (func == "dropdown") { newFn = "ac_widgets_dropdown_new"; specs = {{false,"0"}}; }
        else if (func == "advance") { newFn = "ac_widgets_advance_new"; specs = {{false,"0"},{false,"200"}}; }
        else if (func == "slider") { newFn = "ac_widgets_slider_new"; specs = {{false,"0"},{false,"0"},{false,"100"},{true,"horizontal"}}; }
        else if (func == "group") { newFn = "ac_widgets_group_new"; specs = {{false,"0"},{true,""}}; }
        else if (func == "tabs") { newFn = "ac_widgets_tabs_new"; specs = {{false,"0"}}; }
        else if (func == "scroller") { newFn = "ac_widgets_scroller_new"; specs = {{false,"0"},{true,"vertical"}}; }
        else if (func == "listbox") { newFn = "ac_widgets_listbox_new"; specs = {{false,"0"},{false,"20"},{false,"10"}}; }
        else if (func == "table") { newFn = "ac_widgets_table_new"; specs = {{false,"0"},{true,""},{false,"10"}}; }
        else if (func == "sketch") { newFn = "ac_widgets_sketch_new"; specs = {{false,"0"},{false,"300"},{false,"200"}}; }
        else if (func == "textbox") { newFn = "ac_widgets_textbox_new"; specs = {{false,"0"},{true,"black"},{true,"monospace"}}; }
        else return false;
        // A trailing `lazy` sentinel arg (`textbox(root, ..., lazy)`) defers auto-pack to a
        // manual caller-side `.pack(...)` later — mirrors CStrategy's hasLazyArg/withoutLazy
        // (see its comment) exactly; BNY never had this at all before (every ctor always
        // auto-packed unconditionally, silently ignoring any trailing `lazy` arg that happened
        // to overflow past a shorter ctor's own arg count with no effect either way).
        bool isLazy = !args.empty() && funcName(args.back()) == "lazy";
        std::vector<AC_IR::IRRef> realArgs = isLazy
            ? std::vector<AC_IR::IRRef>(args.begin(), args.end() - 1) : args;
        for (size_t i = 0; i < specs.size() && i < abi.argRegs.size(); i++) {
            if (specs[i].isStr) loadArgOrStr(realArgs, i, specs[i].def, abi.argRegs[i]);
            else {
                int64_t d = 0; try { d = std::stoll(specs[i].def); } catch (...) {}
                loadArgOrInt(realArgs, i, d, abi.argRegs[i]);
            }
        }
        em.call(newFn);
        store(ins.result, R::RAX);
        std::string resName = varName(ins.result);
        if (!resName.empty() && widgetVarKind_) (*widgetVarKind_)[resName] = func;
        if (isLazy) { load(ins.result, R::RDI); em.call("ac_widgets_set_lazy"); }
        else {
            std::string packFn = widgetPackFn(func);
            if (!packFn.empty()) { load(ins.result, abi.argRegs[0]); em.call(packFn); }
        }
        if (func == "btn" && args.size() > 2) {
            std::string cbName = funcName(args[2]);
            std::string adapter = userFuncArity(cbName) > 0 ? "_ac_widget_call1" : "_ac_widget_call0";
            load(ins.result, R::RDI);
            em.lea_r_label(R::RSI, adapter);
            em.lea_r_label(R::RDX, cbName);
            em.call("ac_widgets_btn_on_click");
        }
        return true;
    }
    // Method call: `recv.method(args)` where `recv` is a known widget var (LIB_CALL path).
    bool bnyWidgetMethod(const AC_IR::IRInstruction& ins, const std::string& recv,
                         const std::string& method, const std::vector<AC_IR::IRRef>& args) {
        if (!widgetVarKind_) return false;
        auto wit = widgetVarKind_->find(recv);
        if (wit == widgetVarKind_->end()) return false;
        const std::string& kind = wit->second;
        if (method == "pack") {
            if (args.size() >= 2) {
                loadNamedVar(recv, R::RDI);
                load(args[0], R::RSI);
                load(args[1], R::RDX);
                em.call("ac_widgets_pack_spaced");
            } else {
                loadNamedVar(recv, R::RDI);
                em.call(widgetPackFn(kind));
            }
            return true;
        }
        if (method == "mainloop" && kind == "Screen") { loadNamedVar(recv, R::RDI); em.call("ac_widgets_screen_mainloop"); return true; }
        if (method == "update"   && kind == "Screen") { loadNamedVar(recv, R::RDI); em.call("ac_widgets_screen_update");   return true; }
        if (method == "destroy"  && kind == "Screen") { loadNamedVar(recv, R::RDI); em.call("ac_widgets_screen_destroy");  return true; }
        if (method == "dimensions" && kind == "Screen" && args.size() >= 2) {
            loadNamedVar(recv, R::RDI);
            load(args[0], R::RSI);
            load(args[1], R::RDX);
            em.call("ac_widgets_screen_dimensions");
            return true;
        }
        if (method == "add") {
            std::string fn = (kind == "dropdown" ? "ac_widgets_dropdown_add" :
                              kind == "listbox"  ? "ac_widgets_listbox_add"  :
                              kind == "table"    ? "ac_widgets_table_add"    : "ac_widgets_add");
            loadNamedVar(recv, R::RDI);
            for (size_t i = 0; i < args.size() && i + 1 < abi.argRegs.size(); i++)
                load(args[i], abi.argRegs[i + 1]);
            em.call(fn);
            return true;
        }
        if (method == "set" || method == "config") {
            loadNamedVar(recv, R::RDI);
            if (!args.empty()) load(args[0], R::RSI);
            else { int sid = sp.add(""); em.mov_ri64_str(R::RSI, sid); }
            em.call(widgetSetFn(kind));
            return true;
        }
        if (method == "get") {
            loadNamedVar(recv, R::RDI);
            em.call(widgetGetFn(kind));
            if (ins.result.isValid()) {
                store(ins.result, R::RAX);
                if (kind == "ask" || kind == "display" || kind == "dropdown" || kind == "textbox")
                    markDstString(ins.result);
            }
            return true;
        }
        // textbox.write(text) — sets/overwrites its content. Own method name (not "set"): a
        // different real symbol (`ac_widgets_textbox_write`), matching the API Abu specified
        // directly for the ac-ide rebuild rather than reusing the generic scalar set/config path.
        if (method == "write" && kind == "textbox" && !args.empty()) {
            loadNamedVar(recv, R::RDI);
            load(args[0], R::RSI);
            em.call("ac_widgets_textbox_write");
            return true;
        }
        // textbox.find(needle) — returns the matched text (or "") — and textbox.fix(text) —
        // writes text and locks the box read-only. Composable: `tb.fix(tb.find($x$))`.
        if (method == "find" && kind == "textbox" && !args.empty()) {
            loadNamedVar(recv, R::RDI);
            load(args[0], R::RSI);
            em.call("ac_widgets_textbox_find");
            if (ins.result.isValid()) { store(ins.result, R::RAX); markDstString(ins.result); }
            return true;
        }
        if (method == "fix" && kind == "textbox" && !args.empty()) {
            loadNamedVar(recv, R::RDI);
            load(args[0], R::RSI);
            em.call("ac_widgets_textbox_fix");
            return true;
        }
        if (method == "on_click" && kind == "btn" && !args.empty()) {
            std::string cbName = funcName(args[0]);
            std::string adapter = userFuncArity(cbName) > 0 ? "_ac_widget_call1" : "_ac_widget_call0";
            loadNamedVar(recv, R::RDI);
            em.lea_r_label(R::RSI, adapter);
            em.lea_r_label(R::RDX, cbName);
            em.call("ac_widgets_btn_on_click");
            return true;
        }
        // tabs.add_tab / sketch's drawing methods — mirrors CStrategy's cWidgetMethod (see its
        // matching comment for the "undefined label" bug both this and the C fix close).
        if (method == "add_tab" && kind == "tabs" && !args.empty()) {
            loadNamedVar(recv, R::RDI);
            load(args[0], R::RSI);
            em.call("ac_widgets_tabs_add_tab");
            if (ins.result.isValid()) store(ins.result, R::RAX);
            return true;
        }
        if (method == "clear" && kind == "sketch") {
            loadNamedVar(recv, R::RDI);
            em.call("ac_widgets_sketch_clear");
            return true;
        }
        // ac_widgets_sketch_{line,rect}(h, x1,y1,x2,y2 [double], r,g,b [int]) — SysV assigns
        // float-class (XMM0..3) and int-class (RDI/RSI/RDX/RCX after RDI=h) args independently.
        if ((method == "line" || method == "rect") && kind == "sketch" && args.size() >= 7) {
            loadNamedVar(recv, R::RDI);
            for (int k = 0; k < 4; k++) {
                load(args[k], R::RAX);
                if (isFloatRef(args[k])) em.movq_xmmN_from_gpr(k, R::RAX);
                else em.cvtsi2sd_xmmN_from_gpr(k, R::RAX);
            }
            load(args[4], R::RSI);
            load(args[5], R::RDX);
            load(args[6], R::RCX);
            em.call(method == "line" ? "ac_widgets_sketch_line" : "ac_widgets_sketch_rect");
            return true;
        }
        if (method == "circle" && kind == "sketch" && args.size() >= 6) {
            loadNamedVar(recv, R::RDI);
            for (int k = 0; k < 3; k++) {
                load(args[k], R::RAX);
                if (isFloatRef(args[k])) em.movq_xmmN_from_gpr(k, R::RAX);
                else em.cvtsi2sd_xmmN_from_gpr(k, R::RAX);
            }
            load(args[3], R::RSI);
            load(args[4], R::RDX);
            load(args[5], R::RCX);
            em.call("ac_widgets_sketch_circle");
            return true;
        }
        if (method == "text_at" && kind == "sketch" && args.size() >= 6) {
            loadNamedVar(recv, R::RDI);
            for (int k = 0; k < 2; k++) {
                load(args[k], R::RAX);
                if (isFloatRef(args[k])) em.movq_xmmN_from_gpr(k, R::RAX);
                else em.cvtsi2sd_xmmN_from_gpr(k, R::RAX);
            }
            load(args[2], R::RSI);   // text
            load(args[3], R::RDX);   // r
            load(args[4], R::RCX);   // g
            load(args[5], R::R8);    // b
            em.call("ac_widgets_sketch_text");
            return true;
        }
        return false;
    }
    // Structured IF support (IF_BEGIN/IF_ELSE/IF_END markers from cond/LineUp lowering)
    struct IfCtx { std::string elseL, endL; bool sawElse; };
    std::vector<IfCtx> ifStack_;
    std::vector<std::string> catchSkip_;   // #29: pending labels that skip catch bodies
    std::vector<std::string> catchEntry_;  // pending catch-body ENTRY labels (longjmp landing pad)
    int catchCounter_ = 0;
    int ifCounter_ = 0;
    int rangeCounter_ = 0;   // range/sequence ALLOC's own inline fill-loop labels (per-occurrence)
    // Per-function prefix for internally-generated labels. FuncCompiler is re-instantiated per
    // function so catchCounter_/ifCounter_ reset to 0 each time; without a prefix, two functions
    // both emit "__ac_idiv_ok_0__"/"__if0_else__" into the SHARED emitter map → applyFixups binds
    // both functions' jumps to the last definition → function A jumps into function B. (FATAL)
    std::string labelPrefix_;
    std::string uniq(const std::string& base) const { return labelPrefix_ + base; }
    std::set<std::string>   usingHeaders_;  // namespaces from "using header X"

    // Shared by IDIV and MOD's zero-divisor guard: if inside a `try` (a call site reached from a
    // DIFFERENT function than the one containing that try — this is exactly why real cross-frame
    // unwinding is needed, not a same-function jump), restore RSP/RBP from the try-stack and
    // jmp_r straight to the catch body; otherwise print the error and exit(1) same as always.
    // Clobbers RAX/RCX/RDX/RBX — call only at a statement boundary, matching every other call site.
    void emitDivZeroTrap() {
        std::string fatalL = uniq("__ac_divzero_fatal_" + std::to_string(catchCounter_++) + "__");
        if (usesTry_) {
            int stackSlot = (*gvarSlots_)["__try_stack_ptr"];
            int depthSlot = (*gvarSlots_)["__try_depth"];
            em.mov_ri64_gvar(R::RCX, depthSlot);
            em.mov_r_ptr(R::RAX, R::RCX);
            em.test_rr(R::RAX, R::RAX);
            em.je(fatalL);                       // depth == 0 → no active try
            em.dec_r(R::RAX);
            em.mov_ptr_r(R::RCX, R::RAX);         // depth--
            em.mov_rr(R::RDX, R::RAX);            // rdx = slot index to use (post-decrement)
            em.mov_ri64_gvar(R::RCX, stackSlot);
            em.mov_r_ptr(R::R8, R::RCX);          // r8 = try-stack base (stable pointer, NOT
                                                   // one of the values being restored)
            em.mov_ri32(R::RAX, 64);
            em.imul_rr(R::RAX, R::RDX);
            em.add_rr(R::R8, R::RAX);             // r8 = slot address
            em.mov_r_ptr(R::RSP, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::RBP, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::RBX, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::R12, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::R13, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::R14, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::R15, R::R8); em.add_ri32(R::R8, 8);
            em.mov_r_ptr(R::RAX, R::R8);          // rax = saved catch-entry target
            em.jmp_r(R::RAX);
        }
        em.label(fatalL);
        std::string msg = "Preposterous: 3rd grade mathematics violated (ZeroDivisionError)\n";
        int sid = sp.add(msg);
        em.mov_ri64_str(R::RSI, sid);
        em.mov_ri32(R::RDI, 2);
        em.mov_ri32(R::RDX, (int32_t)msg.size());
        em.mov_ri32(R::RAX, 1); em.syscall();      // write(2, msg)
        em.mov_ri32(R::RAX, 231); em.mov_ri32(R::RDI, 1); em.syscall(); // exit(1)
    }
    // floatFuncs_ moved to public section above

    // Is this ref a known string (const literal, marked var, or marked temp)?
    bool isStrRef(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::CONST && r.value.type == AC_IR::IRType::STRING) return true;
        if (r.kind == AC_IR::IRRef::Kind::TEMP && stringTempIds_.count(r.id)) return true;
        if (r.kind == AC_IR::IRRef::Kind::VAR && stringVarNames_.count(varName(r))) return true;
        return false;
    }
    bool isArrRef(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::TEMP && arrayTempIds_.count(r.id)) return true;
        if (r.kind == AC_IR::IRRef::Kind::VAR && arrayVarNames_.count(varName(r))) return true;
        return false;
    }
    bool isDictRef(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::TEMP && dictTempIds_.count(r.id)) return true;
        if (r.kind == AC_IR::IRRef::Kind::VAR && dictVarNames_.count(varName(r))) return true;
        return false;
    }
    void markDstDict(const AC_IR::IRRef& r) {
        if (r.kind == AC_IR::IRRef::Kind::TEMP) dictTempIds_.insert(r.id);
        else if (r.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(r);
            if (!vn.empty()) dictVarNames_.insert(vn);
        }
    }
    bool isListOfDictRef(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::TEMP && listOfDictTempIds_.count(r.id)) return true;
        if (r.kind == AC_IR::IRRef::Kind::VAR && listOfDictVarNames_.count(varName(r))) return true;
        return false;
    }
    void markDstListOfDict(const AC_IR::IRRef& r) {
        if (r.kind == AC_IR::IRRef::Kind::TEMP) listOfDictTempIds_.insert(r.id);
        else if (r.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(r);
            if (!vn.empty()) listOfDictVarNames_.insert(vn);
        }
    }
    void markDstArray(const AC_IR::IRRef& r) {
        if (r.kind == AC_IR::IRRef::Kind::TEMP) arrayTempIds_.insert(r.id);
        else if (r.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(r);
            if (!vn.empty()) arrayVarNames_.insert(vn);
        }
    }
    void markDstString(const AC_IR::IRRef& r) {
        if (r.kind == AC_IR::IRRef::Kind::TEMP) stringTempIds_.insert(r.id);
        else if (r.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(r);
            if (!vn.empty()) stringVarNames_.insert(vn);
        }
    }

    // Get function name from a VAR or FUNCTION IRRef
    std::string funcName(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::FUNCTION || r.kind == AC_IR::IRRef::Kind::VAR) {
            if (r.id >= 0) {
                std::string n = prog.symbols.getName(r.id);
                if (!n.empty()) return n;
            }
            if (r.value.type == AC_IR::IRType::STRING)
                return std::get<std::string>(r.value.data);
        }
        return "";
    }

    // Resolve a bare function name via "using header X" if it has no dot prefix
    std::string resolveFunc(const std::string& fn) const {
        if (fn.empty() || fn.find('.') != std::string::npos) return fn;
        if (usingHeaders_.empty()) return fn;
        // Check if it's a user-defined function
        for (auto& f : prog.functions)
            if (f.name == fn) return fn;
        // Not user-defined — prefix with the first using-header namespace
        return *usingHeaders_.begin() + "." + fn;
    }

    // Get variable name from a VAR IRRef (id or inline string)
    std::string varName(const AC_IR::IRRef& r) const {
        if (r.kind != AC_IR::IRRef::Kind::VAR) return "";
        if (r.id >= 0) {
            std::string n = prog.symbols.getName(r.id);
            if (!n.empty()) return n;
        }
        if (r.value.type == AC_IR::IRType::STRING)
            return std::get<std::string>(r.value.data);
        return "";
    }

    bool isFloatRef(const AC_IR::IRRef& r) const {
        if (r.kind == AC_IR::IRRef::Kind::CONST && r.value.type == AC_IR::IRType::FLOAT) return true;
        if (r.kind == AC_IR::IRRef::Kind::TEMP && floatTempIds_.count(r.id)) return true;
        if (r.kind == AC_IR::IRRef::Kind::VAR) {
            std::string vn = varName(r);
            if (!vn.empty() && floatVarNames_.count(vn)) return true;
        }
        return false;
    }

    // Load an IRRef into a register; returns the register used.
    // For TEMPs with a register assignment: copies to scratch to avoid
    // aliasing issues in arithmetic ops (e.g. add_rr clobbers destination).
    R load(const AC_IR::IRRef& r, R scratch = R::RAX) {
        using namespace AC_IR;
        switch (r.kind) {
        case IRRef::Kind::CONST:
            if (r.value.type == IRType::INT) {
                int64_t v = std::get<int64_t>(r.value.data);
                if (v >= INT32_MIN && v <= INT32_MAX)
                    em.mov_ri32(scratch, (int32_t)v);
                else
                    em.mov_ri64(scratch, (uint64_t)v);
            } else if (r.value.type == IRType::FLOAT) {
                // Load raw 64-bit IEEE 754 bits so xmm can interpret them
                double dv = std::get<double>(r.value.data);
                uint64_t bits; std::memcpy(&bits, &dv, 8);
                em.mov_ri64(scratch, bits);
            } else if (r.value.type == IRType::BOOL)
                em.mov_ri32(scratch, std::get<bool>(r.value.data) ? 1 : 0);
            else if (r.value.type == IRType::STRING) {
                // Embed string into data section; load its address.
                // Method-arg consts may still carry the $...$ delimiters — strip them
                // (text backends strip at formatRef; BNY embeds the bytes verbatim).
                std::string sval = std::get<std::string>(r.value.data);
                if (sval.size() >= 2 && sval.front() == '$' && sval.back() == '$')
                    sval = sval.substr(1, sval.size() - 2);
                int sid = sp.add(sval);
                em.mov_ri64_str(scratch, sid);
            } else
                em.mov_ri32(scratch, 0);
            return scratch;
        case IRRef::Kind::TEMP:
            if (regAlloc.hasReg(r.id)) {
                R reg = regAlloc.getReg(r.id);
                if (reg != scratch) em.mov_rr(scratch, reg); // reg→reg (fast)
                return scratch;
            }
            em.mov_r_rbp(scratch, frame.tempOffset(r.id));
            return scratch;
        case IRRef::Kind::VAR: {
            std::string nm = varName(r);
            std::string fbase; int foff;
            if (resolveFieldAccess(nm, fbase, foff)) {
                // self.field / instance.field — real pointer+offset access (see the field-order
                // pre-scan's comment). Was completely disconnected before: this var just got ITS
                // OWN local stack slot via frame.varOffset(r.id) below, same as any unrelated
                // plain variable — a field WRITE never reached the actual object at all.
                loadNamedVar(fbase, scratch);
                if (foff != 0) em.add_ri32(scratch, foff);
                em.mov_r_ptr(scratch, scratch);
                return scratch;
            }
            int gs = gvarSlotOf(nm);
            if (gs >= 0) {                          // promoted free var: load from global slot
                em.mov_ri64_gvar(scratch, gs);      // scratch = &slot
                em.mov_r_ptr(scratch, scratch);     // scratch = *scratch
                return scratch;
            }
            em.mov_r_rbp(scratch, frame.varOffset(r.id));
            return scratch;
        }
        default:
            em.mov_ri32(scratch, 0);
            return scratch;
        }
    }

    void store(const AC_IR::IRRef& r, R src) {
        using namespace AC_IR;
        if (r.kind == IRRef::Kind::TEMP && regAlloc.hasReg(r.id)) {
            R reg = regAlloc.getReg(r.id);
            if (reg != src) em.mov_rr(reg, src);
            return;
        }
        if (r.kind == IRRef::Kind::TEMP)
            em.mov_rbp_r(frame.tempOffset(r.id), src);
        else if (r.kind == IRRef::Kind::VAR) {
            std::string nm = varName(r);
            std::string fbase; int foff;
            if (resolveFieldAccess(nm, fbase, foff)) {
                R addr = (src == R::R11) ? R::R10 : R::R11;
                loadNamedVar(fbase, addr);
                if (foff != 0) em.add_ri32(addr, foff);
                em.mov_ptr_r(addr, src);
                return;
            }
            int gs = gvarSlotOf(nm);
            if (gs >= 0) {                          // promoted free var: store to global slot
                R addr = (src == R::R11) ? R::R10 : R::R11;
                em.mov_ri64_gvar(addr, gs);         // addr = &slot
                em.mov_ptr_r(addr, src);            // *addr = src
                return;
            }
            em.mov_rbp_r(frame.varOffset(r.id), src);
        }
    }

    void emitEpilogue() {
        em.add_rsp_i32(fsize);
        // Pop callee-saved in reverse push order
        for (int i = (int)calleeSaves.size() - 1; i >= 0; i--)
            em.pop_r(calleeSaves[i]);
        em.pop_rbp();
        em.ret();
    }

    void emitHalt() {
        if (g_bnyTargetWindows) {
            em.and_rsp_align16();      // force alignment — parity here isn't tracked, control never returns
            em.sub_rsp_i32(32);        // Win64 shadow space
            em.mov_ri32(R::RCX, 0);
            em.call_rip_rel("ExitProcess"); // real IAT call, not a PLT-style direct call
        } else {
            // 231 = exit_group, NOT 60 = exit. Plain `exit` only terminates the CALLING
            // thread — harmless for every purely-BNY-code program (single-threaded), but
            // once a dynamically-linked ilib spawns its own background pthreads (verified:
            // camera_demo.ac — libaccamera.so's OpenCV/GStreamer capture thread), the main
            // thread's `exit(0)` left those threads running and the process never actually
            // terminated (hung until each one's own internal ~15s timeout independently
            // fired). exit_group kills every thread in the process and is identical to
            // plain exit for the single-threaded case, so this is a strict, safe upgrade.
            em.mov_ri32(R::RAX, 231);
            em.xor_rr(R::RDI, R::RDI);
            em.syscall();
        }
    }

    void emitLibCall(const AC_IR::IRInstruction& ins) {
        if (ins.typedOperands.empty()) return;
        using namespace AC_IR;

        // Determine method from operand[0]
        std::string method;
        auto& m = ins.typedOperands[0];
        if (m.kind == IRRef::Kind::CONST && m.value.type == IRType::STRING)
            method = std::get<std::string>(m.value.data);
        else
            method = funcName(m);

        // import — handled via PLT/GOT; nothing to emit at runtime
        if (method == "import") return;

        // widgets method call (`lang_drop.add(...)`, `bar.set(42)`, `root.mainloop`) — must run
        // before the generic dotted-method fallbacks below, same ordering CStrategy/AsmStrategy
        // use (see widgetVarKind_'s header comment).
        {
            auto dot = method.find('.');
            if (dot != std::string::npos) {
                std::string recv = method.substr(0, dot);
                std::string mname = method.substr(dot + 1);
                if (widgetVarKind_ && widgetVarKind_->count(recv)) {
                    std::vector<IRRef> args(ins.typedOperands.begin() + 1, ins.typedOperands.end());
                    if (bnyWidgetMethod(ins, recv, mname, args)) return;
                }
            }
        }

        // arr.append(value): grow the receiver array and store the new pointer back into it.
        // The receiver array name is the part of the method before ".append".
        if (method.size() > 7 && method.compare(method.size() - 7, 7, ".append") == 0
                && ins.typedOperands.size() >= 2) {
            std::string recv = method.substr(0, method.size() - 7);
            // #13: a promoted free var lives in a GLOBAL slot, not the stack frame —
            // appending via frame.varOffset read a stale slot → corruption/segfault.
            int gs = gvarSlotOf(recv);
            if (gs >= 0) {
                em.mov_ri64_gvar(R::RDI, gs);                 // rdi = &slot
                em.mov_r_ptr(R::RDI, R::RDI);                 // rdi = *slot (old array ptr)
                load(ins.typedOperands[1], R::RSI);           // rsi = value
                em.call("__ac_append__");                     // rax = new array ptr
                em.mov_ri64_gvar(R::RCX, gs);                 // rcx = &slot
                em.mov_ptr_r(R::RCX, R::RAX);                 // *slot = new ptr
                return;
            }
            auto it = localVarIds_.find(recv);
            if (it != localVarIds_.end()) {
                int symId = it->second;
                em.mov_r_rbp(R::RDI, frame.varOffset(symId)); // rdi = old array ptr
                load(ins.typedOperands[1], R::RSI);           // rsi = value
                em.call("__ac_append__");                     // rax = new array ptr
                em.mov_rbp_r(frame.varOffset(symId), R::RAX); // store new ptr back
                return;
            }
        }

        // `c.greet()` — instance method call on a known bundle var. Previously fell all the way
        // through this function (nothing matched "c.greet" against any `fn.name`, since methods
        // are compiled as bare "greet"/"init" — see compileFn) and did NOTHING — a pure,
        // silent no-op; `c.greet()` just vanished. Redirect to the real class-qualified label
        // (`Critter_greet`), passing the instance pointer as `self` (1st arg).
        {
            auto dot = method.find('.');
            if (dot != std::string::npos) {
                std::string recv = method.substr(0, dot);
                std::string methodName = method.substr(dot + 1);
                if (instanceClass_ && instanceClass_->count(recv)) {
                    std::string cls = instanceClass_->at(recv);
                    loadNamedVar(recv, R::RDI);
                    for (size_t ai = 1; ai < ins.typedOperands.size() && ai < abi.argRegs.size(); ai++)
                        load(ins.typedOperands[ai], abi.argRegs[ai]);
                    em.call(cls + "_" + methodName);
                    if (ins.result.isValid()) store(ins.result, R::RAX);
                    return;
                }
            }
        }

        // quickthread f(args): BNY has no lightweight threads — degrade to a plain call.
        if (method == "quickthread" && ins.typedOperands.size() >= 2) {
            std::string fname = funcName(ins.typedOperands[1]);
            int argc = (int)ins.typedOperands.size() - 2;
            for (int i = 0; i < argc && i < (int)abi.argRegs.size(); i++)
                load(ins.typedOperands[2 + i], abi.argRegs[i]);
            em.call(fname);
            return;
        }

        // User-defined function called as a statement (result unused) is lowered to LIB_CALL.
        // Emit a direct call so its side effects (e.g. NA→free mutations) actually run.
        for (auto& fn : prog.functions) {
            if (fn.name == method) {
                int argc = (int)ins.typedOperands.size() - 1;
                for (int i = 0; i < argc && i < (int)abi.argRegs.size(); i++)
                    load(ins.typedOperands[1 + i], abi.argRegs[i]);
                em.call(method);
                return;
            }
        }

        // NOTE: the old "pointers.*" dispatch block that used to live here was 100% dead —
        // bare `ptr_new(...)` etc. never reach codegen as a dotted "pointers.X" LIB_CALL method
        // (every other backend's generated source shows they're plain CALLs), and it called
        // `__ac_ptr_new__`/`__ac_ptr_deref__`/`__ac_ptr_update__` helper labels that were never
        // defined anywhere — so even the (unreachable) match would have failed to assemble.
        // native-cpu's bare functions (ptr_new, dha, arena_alloc, abort, ...) now go through the
        // same generic external-symbol / PLT-GOT path as os./regex./ml. — see
        // collectExternalSymbols()'s bare-name check and libForSym()'s "libacncpu.so" mapping.

        // General Term.X(args) — call math.X PLT stub, print integer result
        // Term.display and Term.ask fall through to the default print below
        if (method.rfind("Term.", 0) == 0 && method.size() > 5 &&
            method != "Term.display" && method != "Term.ask" &&
            ins.typedOperands.size() >= 2)
        {
            std::string fname = method.substr(5);
            // Load up to 2 args (RDI, RSI per SysV ABI)
            if (ins.typedOperands.size() >= 3) {
                load(ins.typedOperands[1], R::RDI);
                load(ins.typedOperands[2], R::RSI);
            } else {
                load(ins.typedOperands[1], R::RDI);
            }
            em.call("math." + fname);  // PLT stub label = irName
            em.mov_rr(R::RDI, R::RAX);
            em.call("__ac_print_int__");
            return;
        }

        // Namespaced ilib calls — ml.tensor(2), os.mkfile(p), regex.match(s,p),
        // stringm.upper(s), web.open(u), ncpu.dha(n) — all route through their .so PLT stubs.
        if (method.rfind("ml.", 0) == 0 || method.rfind("os.", 0) == 0 ||
            method.rfind("regex.", 0) == 0 || method.rfind("stringm.", 0) == 0 ||
            method.rfind("web.", 0) == 0 || method.rfind("ncpu.", 0) == 0 ||
            method.rfind("maudio.", 0) == 0 ||
            // camera/sidebar/screen/aczip/server were missing from this allowlist — the
            // REAL reason every one of their calls was broken, not just an unresolved-label
            // gap: falling through this whole block sent them into the "Term.display" LIB_CALL
            // fallback further down (which also matches on "method, arg" shape), so e.g.
            // `sidebar.config($mode$, $manual$)` printed "mode" as if it were `Term.display
            // $mode$` instead of calling ac_sidebar_config at all (verified: camera_demo.ac's
            // BNY output had stray "mode"/"0,0,320,240"/"0"/"Ready to capture." lines — its
            // own string args, printed — interleaved with the real output).
            method.rfind("camera.", 0) == 0 || method.rfind("sidebar.", 0) == 0 ||
            method.rfind("screen.", 0) == 0 || method.rfind("aczip.", 0) == 0 ||
            method.rfind("server.", 0) == 0) {
            if (method == "ml.weights" && ins.typedOperands.size() >= 3) {
                load(ins.typedOperands[1], R::RAX);
                if (isFloatRef(ins.typedOperands[1])) em.movq_xmm0_from_gpr(R::RAX);
                else                                  em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                load(ins.typedOperands[2], R::RDI);
                em.call(method);
                if (ins.result.isValid()) store(ins.result, R::RAX);
                return;
            }
            // SysV AMD64: integer and float args use INDEPENDENT register sequences —
            // ints fill RDI,RSI,RDX,RCX,R8,R9 in order; floats fill XMM0..XMM7 in order; each
            // arg consumes exactly one of its own sequence. The old code loaded EVERY arg into
            // an int reg AND placed floats into XMM[arg-index] → a mixed sig like
            // ml.grid(int,int,double) put the double in XMM2 (callee reads XMM0) → garbage. (FATAL)
            int argCount = (int)ins.typedOperands.size() - 1;
            int intIdx = 0, fltIdx = 0;
            for (int ai = 0; ai < argCount; ++ai) {
                const auto& argRef = ins.typedOperands[1 + ai];
                if (callArgTakesDouble(method, ai)) {
                    if (fltIdx < 8) {
                        load(argRef, R::RAX);                                   // scratch (not an arg reg)
                        if (isFloatRef(argRef)) em.movq_xmmN_from_gpr(fltIdx, R::RAX);
                        else                    em.cvtsi2sd_xmmN_from_gpr(fltIdx, R::RAX);
                    }
                    fltIdx++;
                } else {
                    if (intIdx < (int)abi.argRegs.size())
                        load(argRef, abi.argRegs[intIdx]);
                    intIdx++;
                }
            }
            em.call(method);
            if (isFloatReturningCall(method)) {
                em.movq_gpr_from_xmm0(R::RAX);
                if (ins.result.isValid()) {
                    if (ins.result.kind == AC_IR::IRRef::Kind::TEMP)
                        floatTempIds_.insert(ins.result.id);
                    else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                        std::string vn = varName(ins.result);
                        if (!vn.empty()) floatVarNames_.insert(vn);
                    }
                }
            }
            // char*-returning ilib calls: mark the result so PRINT uses __ac_print_cstr__
            if (returnsCString(method) && ins.result.isValid()) {
                if (ins.result.kind == AC_IR::IRRef::Kind::TEMP)
                    stringTempIds_.insert(ins.result.id);
                else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) stringVarNames_.insert(vn);
                }
            }
            if (ins.result.isValid()) store(ins.result, R::RAX);
            return;
        }

        // Term.display — print operand (LIB_CALL with "Term.display" method)
        if (ins.typedOperands.size() < 2) return;
        auto& val = ins.typedOperands[1];
        if (val.kind == IRRef::Kind::CONST && val.value.type == IRType::STRING) {
            std::string s = std::get<std::string>(val.value.data);
            int sid = sp.add(s);
            em.mov_ri64_str(abi.argRegs[0], sid);
            em.mov_ri32(abi.argRegs[1], (int32_t)s.size());
            em.call("__ac_print_str__");
        } else if (val.kind == IRRef::Kind::VAR
                   && stringVarNames_.count(varName(val))) {
            // String-typed variable (e.g. LongInt > int64 max) — pointer is in the slot
            load(val, abi.argRegs[0]);
            em.call("__ac_print_cstr__");
        } else if (isFloatRef(val)) {
            load(val, R::RDI);
            em.movq_xmm0_from_gpr(R::RDI);
            em.call("ac_print_double");
        } else {
            load(val, abi.argRegs[0]);
            em.call("__ac_print_int__");
        }
    }

    void compileInstr(const AC_IR::IRInstruction& ins) {
        using namespace AC_IR;
        auto& ops = ins.typedOperands;

        auto op0 = [&]() -> IRRef {
            return ops.empty() ? IRRef() : ops[0];
        };
        auto op1 = [&]() -> IRRef {
            return ops.size()<2 ? IRRef() : ops[1];
        };

        switch (ins.opcode) {
        case IROpcode::FUNC_BEGIN: {
            std::string name;
            if (!ops.empty()) name = funcName(ops[0]);
            if (!name.empty()) em.label(name);
            // Prologue
            em.push_rbp();
            em.mov_rbp_rsp();
            em.sub_rsp_i32(fsize);
            // For user functions (not global): save parameters from arg regs to var slots
            // We identify params by scanning: the function struct has .parameters names
            // We need to find which symbol ID each param name maps to
            // We do this by scanning the function's instructions for VAR refs
            // whose symbol name matches a parameter name.
            // Parameters are saved from argRegs[0..n-1] in declaration order.
            // We retrieve param symbol IDs by name lookup below (see compileFn).
            // This stub is filled in by compileFn before iterating instructions.
            break;
        }
        case IROpcode::FUNC_END:
            break;

        case IROpcode::STORE_VAR: {
            auto handleSrc = [&](const AC_IR::IRRef& src, const AC_IR::IRRef& dst) {
                // Check if src is a math constant reference (math.pi, math.e, etc.)
                std::string srcName, constFn;
                if (src.kind == AC_IR::IRRef::Kind::VAR) {
                    srcName = varName(src);
                    constFn = mathConstantFunc(srcName);
                }
                if (!constFn.empty()) {
                    em.call(srcName);                  // PLT stub labeled with irName (math.pi)
                    em.movq_gpr_from_xmm0(R::RAX);    // move bits to RAX
                    store(dst, R::RAX);
                    // Mark destination as float
                    if (dst.kind == AC_IR::IRRef::Kind::TEMP)
                        floatTempIds_.insert(dst.id);
                    else if (dst.kind == AC_IR::IRRef::Kind::VAR) {
                        std::string vn = varName(dst);
                        if (!vn.empty()) floatVarNames_.insert(vn);
                    }
                } else {
                    // Propagate float/string type from source to destination
                    bool srcIsFloat = isFloatRef(src);
                    bool srcIsString = isStrRef(src);
                    if (src.kind == AC_IR::IRRef::Kind::CONST) {
                        if (src.value.type == AC_IR::IRType::FLOAT) {
                            srcIsFloat = true;
                        }
                    }
                    if (srcIsString) markDstString(dst);
                    // A float source landing on an `atomic` destination needs a real
                    // truncating float->int conversion, not a raw bit copy — `atomic` is
                    // always an int variable (its own doc comment, token.hpp), and this is
                    // exactly the plain-reassignment path ir.cpp's sticky-keep routes
                    // through as a STORE_VAR (not TYPE_CAST) once a var's type doesn't
                    // change, e.g. `atomic x = 5; x = 5.5` stays ATOMIC end-to-end. Missing
                    // this let 5.5's raw IEEE754 bits land straight in x's int slot
                    // (verified: printed as the huge decimal those bits read as, not `5`)
                    // — including this float-tracking mark below, which ran unconditionally
                    // and made a LATER Term.display of x print via the float path even
                    // after the value itself got correctly truncated to an int.
                    bool dstIsAtomic = dst.kind == AC_IR::IRRef::Kind::VAR
                        && atomicVarNames_.count(varName(dst));
                    if (srcIsFloat && !dstIsAtomic) {
                        if (dst.kind == AC_IR::IRRef::Kind::TEMP) floatTempIds_.insert(dst.id);
                        else if (dst.kind == AC_IR::IRRef::Kind::VAR) {
                            std::string vn = varName(dst);
                            if (!vn.empty()) floatVarNames_.insert(vn);
                        }
                    }
                    if (srcIsFloat && dstIsAtomic) {
                        load(src, R::RAX);
                        em.movq_xmm0_from_gpr(R::RAX);
                        em.cvttsd2si_rax_xmm0();
                        store(dst, R::RAX);
                    } else {
                        load(src, R::RAX);
                        // Storing an int value into a float-typed destination: convert to double
                        // so the bit pattern is a valid IEEE-754 value, not a raw integer.
                        if (!srcIsFloat && isFloatRef(dst)) {
                            em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                            em.movq_gpr_from_xmm0(R::RAX);
                        }
                        store(dst, R::RAX);
                    }
                }
            };
            // `atomic` var reassignment: no inline spinlock wrap here anymore. The IR
            // lowering (ir.cpp's emitCompoundRef/AssignStmt) now brackets the WHOLE
            // read-modify-write span with real LOCK_BEGIN/LOCK_END instructions (see
            // the IROpcode::LOCK_BEGIN/LOCK_END cases below), closing the TOCTOU race
            // this old single-store wrap could never actually close — despite this
            // comment's own earlier claim, it only ever locked THIS instruction; the
            // read that computed `src` for a compound update (`x = x + 1`) was always a
            // separate, earlier, unlocked ADD instruction. Leaving this old wrap in
            // place too would double-acquire the spinlock and deadlock (it's not
            // reentrant — see xchg_ptr_r's own comment).
            if (ins.result.isValid()) {
                handleSrc(op0(), ins.result);
            } else if (ops.size() >= 2) {
                handleSrc(ops[1], ops[0]);
            }
            break;
        }

        case IROpcode::LOAD_VAR:
        case IROpcode::LOAD_CONST:
            load(op0(), R::RAX);
            store(ins.result, R::RAX);
            // Propagate float-ness: a folded float const (e.g. inlined nsqrt(2.0) → 1.414214)
            // must print via ac_print_double, not as its raw bit pattern.
            if (op0().kind == IRRef::Kind::CONST && op0().value.type == IRType::FLOAT) {
                if (ins.result.kind == IRRef::Kind::TEMP) floatTempIds_.insert(ins.result.id);
                else if (ins.result.kind == IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) floatVarNames_.insert(vn);
                }
            }
            break;

        case IROpcode::CONST_DECL:
            // const x = v — BNY treats it as a plain store (immutability enforced in IR)
            if (ins.result.isValid() && !ops.empty()) {
                load(ops[0], R::RAX);
                store(ins.result, R::RAX);
            }
            break;

        case IROpcode::TYPE_CAST: {
            // to_int / to_dec / to_string / to_bool — was silently unhandled ("??? " in LIR):
            // `to_int n = Term.ask ...` left n reading a stale slot (printed 0).
            if (ops.empty() || !ins.result.isValid()) break;
            bool srcIsStr =
                (ops[0].kind == IRRef::Kind::VAR  && stringVarNames_.count(varName(ops[0]))) ||
                (ops[0].kind == IRRef::Kind::TEMP && stringTempIds_.count(ops[0].id)) ||
                (ops[0].kind == IRRef::Kind::CONST && ops[0].value.type == IRType::STRING);
            switch (ins.resultType) {
            case IRType::SHORT:   // native BNY has no sub-word slots; short/mini use the 64-bit
            case IRType::MINI:    // integer path (width advisory here — no manual truncation)
            case IRType::INT:
                // Automatic-retype (ir.cpp's AssignStmt) can emit a `resultType==INT`
                // TYPE_CAST for a var that's ALREADY tracked float from an EARLIER cast in
                // this same var's lifetime (`x=5; x=5.5; x=10` — the third assignment is
                // its own INT-typed TYPE_CAST, since ITS OWN literal is an int, even though
                // `x` conceptually stays float for the rest of the program — every OTHER
                // backend's emitTypeCast already defends against this by checking `floatVars
                // .count(var) || t==FLOAT` before dispatching on `t` alone; BNY dispatched
                // purely on `ins.resultType` with no such check). Without this, `x`'s slot
                // got the RAW INT64 BITS of 10 while every later read still (correctly)
                // treated the slot as a double — printing the denormalized bit-reinterpretation
                // garbage of int64 10 as a float instead of 10.0.
                if (isFloatRef(ins.result)) {
                    load(ops[0], R::RDI);
                    if (srcIsStr) { em.call("__ac_atoi__"); em.cvtsi2sd_xmm0_from_gpr(R::RAX); }
                    else if (!isFloatRef(ops[0])) em.cvtsi2sd_xmm0_from_gpr(R::RDI);
                    else em.movq_xmm0_from_gpr(R::RDI);
                    em.movq_gpr_from_xmm0(R::RAX);
                    store(ins.result, R::RAX);
                    break;
                }
                load(ops[0], R::RDI);
                if (srcIsStr) {
                    em.call("__ac_atoi__");
                } else if (isFloatRef(ops[0])) {
                    em.movq_xmm0_from_gpr(R::RDI);
                    em.cvttsd2si_rax_xmm0();
                } else {
                    em.mov_rr(R::RAX, R::RDI);
                }
                store(ins.result, R::RAX);
                break;
            case IRType::ATOMIC: {
                // `atomic x = e` — the declaration is just the first write; wrap it in the same
                // real spinlock (xchg-based, see X64Emitter::xchg_ptr_r) every later `x = ...`
                // reassignment uses too, so it's a genuine critical section from the start.
                if (ins.result.kind == IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) atomicVarNames_.insert(vn);
                }
                load(ops[0], R::RDI);
                em.call("__ac_atomic_lock__");
                // `atomic` is always an int variable (see its own doc comment, token.hpp) —
                // a float source needs a real truncating conversion here, same as the
                // plain-INT case right above this one already does. Missing this let a
                // raw double bit-pattern get copied straight into an atomic var's slot
                // (verified: `atomic x = 5; x = 5.5` printed 5.5's IEEE754 bits reinterpreted
                // as a huge int64, not 5) — a plain register copy is only correct when the
                // source is already a genuine integer.
                if (isFloatRef(ops[0])) {
                    em.movq_xmm0_from_gpr(R::RDI);
                    em.cvttsd2si_rax_xmm0();
                } else {
                    em.mov_rr(R::RAX, R::RDI);
                }
                store(ins.result, R::RAX);
                em.call("__ac_atomic_unlock__");
                break;
            }
            case IRType::FLOAT:
                load(ops[0], R::RDI);
                if (srcIsStr) { em.call("__ac_atoi__"); em.cvtsi2sd_xmm0_from_gpr(R::RAX); }
                else if (!isFloatRef(ops[0])) em.cvtsi2sd_xmm0_from_gpr(R::RDI);
                else em.movq_xmm0_from_gpr(R::RDI);
                em.movq_gpr_from_xmm0(R::RAX);
                store(ins.result, R::RAX);
                if (ins.result.kind == IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) floatVarNames_.insert(vn);
                } else if (ins.result.kind == IRRef::Kind::TEMP)
                    floatTempIds_.insert(ins.result.id);
                break;
            case IRType::BOOL:
                load(ops[0], R::RAX);
                em.test_rr(R::RAX, R::RAX);
                em.setcc_r(0x05, R::RAX); // SETNE
                store(ins.result, R::RAX);
                break;
            case IRType::STRING:
            default:
                if (srcIsStr) {
                    load(ops[0], R::RAX);          // already a string — copy the pointer
                } else if (isFloatRef(ops[0])) {
                    // float→string: truncate to int, then itoa. NOT a bug — matches every
                    // other typed backend's own to_string(float) (C/C++/V all truncate the
                    // same way via an implicit double->int argument conversion into their
                    // own int-only ac_to_str helpers; see CStrategy's ac_to_str(ac_int n)).
                    // A real digit-formatting version (__ac_dtoa__, still defined below)
                    // was tried here first, but it broke dec_to_bin.ac — its algorithm
                    // builds a binary string by concatenating to_string(math.mod(n,2)) one
                    // digit at a time, relying on "1"/"0", not "1.0"/"0.0" — and made BNY
                    // diverge from C/C++/V's matching (if imperfect) convention. Keep the
                    // three consistent rather than "fixing" this one in isolation.
                    load(ops[0], R::RDI);          // float→string: truncate to int, then itoa
                    em.movq_xmm0_from_gpr(R::RDI);
                    em.cvttsd2si_rax_xmm0();
                    em.mov_rr(R::RDI, R::RAX);
                    em.call("__ac_itoa__");
                } else {
                    load(ops[0], R::RDI);          // int→heap decimal string (#7)
                    em.call("__ac_itoa__");
                }
                store(ins.result, R::RAX);
                if (ins.result.kind == IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) stringVarNames_.insert(vn);
                } else if (ins.result.kind == IRRef::Kind::TEMP)
                    stringTempIds_.insert(ins.result.id);
                break;
            }
            break;
        }

        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::PMUL:
        {
            // Runtime string concat (#6): ADD where either side is a string → __ac_concat__.
            if (ins.opcode == IROpcode::ADD && (isStrRef(op0()) || isStrRef(op1()))) {
                auto loadStr = [&](const AC_IR::IRRef& r, R dst) {
                    if (r.kind == AC_IR::IRRef::Kind::CONST && r.value.type == AC_IR::IRType::STRING) {
                        int sid = sp.add(std::get<std::string>(r.value.data));
                        em.mov_ri64_str(dst, sid);
                    } else load(r, dst);
                };
                loadStr(op0(), R::RDI);
                loadStr(op1(), R::RSI);
                em.call("__ac_concat__");
                if (ins.result.isValid()) { store(ins.result, R::RAX); markDstString(ins.result); }
                break;
            }
            bool anyFloat = isFloatRef(op0()) || isFloatRef(op1());
            if (anyFloat) {
                // SSE path: load operands as doubles (convert int→double if needed)
                load(op0(), R::RAX);
                if (isFloatRef(op0())) em.movq_xmm0_from_gpr(R::RAX);
                else                   em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                load(op1(), R::RCX);
                if (isFloatRef(op1())) em.movq_xmm1_from_gpr(R::RCX);
                else                   em.cvtsi2sd_xmm1_from_gpr(R::RCX);
                if (ins.opcode == IROpcode::ADD) em.addsd_xmm0_xmm1();
                else if (ins.opcode == IROpcode::SUB) em.subsd_xmm0_xmm1();
                else em.mulsd_xmm0_xmm1();
                em.movq_gpr_from_xmm0(R::RAX);
                store(ins.result, R::RAX);
                // Mark result as float
                if (ins.result.kind == AC_IR::IRRef::Kind::TEMP) floatTempIds_.insert(ins.result.id);
                else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) floatVarNames_.insert(vn);
                }
            } else {
                load(op0(), R::RAX); load(op1(), R::RCX);
                if (ins.opcode == IROpcode::ADD) em.add_rr(R::RAX, R::RCX);
                else if (ins.opcode == IROpcode::SUB) em.sub_rr(R::RAX, R::RCX);
                else em.imul_rr(R::RAX, R::RCX);
                // If the destination is float-typed (used elsewhere in a float context),
                // store the int result as a double, not raw integer bits.
                if (isFloatRef(ins.result)) {
                    em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                    em.movq_gpr_from_xmm0(R::RAX);
                }
                store(ins.result, R::RAX);
            }
            break;
        }
        case IROpcode::FDIV:
        case IROpcode::DIV:
            // True division — load operands respecting their float/int type
            load(op0(), R::RAX);
            if (isFloatRef(op0())) em.movq_xmm0_from_gpr(R::RAX);
            else                   em.cvtsi2sd_xmm0_from_gpr(R::RAX);
            load(op1(), R::RCX);
            if (isFloatRef(op1())) em.movq_xmm1_from_gpr(R::RCX);
            else                   em.cvtsi2sd_xmm1_from_gpr(R::RCX);
            em.divsd_xmm0_xmm1();
            em.movq_gpr_from_xmm0(R::RAX);
            store(ins.result, R::RAX);
            // Mark result as float
            if (ins.result.kind == AC_IR::IRRef::Kind::TEMP)
                floatTempIds_.insert(ins.result.id);
            else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                std::string vn = varName(ins.result);
                if (!vn.empty()) floatVarNames_.insert(vn);
            }
            break;
        case IROpcode::IDIV: {
            // Integer (floor) division. Zero divisor → clean ZeroDivisionError (idiv on 0
            // raises SIGFPE = core dump; match C's guarded behavior instead).
            load(op0(), R::RAX); load(op1(), R::RCX);
            std::string okL = uniq("__ac_idiv_ok_" + std::to_string(catchCounter_++) + "__");
            em.test_rr(R::RCX, R::RCX);
            em.jne(okL);
            emitDivZeroTrap();
            em.label(okL);
            em.cqo(); em.idiv_rcx();
            store(ins.result, R::RAX);
            break;
        }
        case IROpcode::PTM:   // a ptm b — literal shl (for speed)
        case IROpcode::PTD:   // a ptd b — literal sar
            load(op0(), R::RAX); load(op1(), R::RCX);
            if (ins.opcode == IROpcode::PTM) em.shl_rax_cl();
            else                             em.sar_rax_cl();
            store(ins.result, R::RAX);
            break;
        case IROpcode::MOD: {
            // Same zero-divisor guard as IDIV — bare idiv on a 0 divisor raises SIGFPE (core dump).
            load(op0(), R::RAX); load(op1(), R::RCX);
            std::string okL = uniq("__ac_mod_ok_" + std::to_string(catchCounter_++) + "__");
            em.test_rr(R::RCX, R::RCX);
            em.jne(okL);
            emitDivZeroTrap();
            em.label(okL);
            em.cqo(); em.idiv_rcx();
            store(ins.result, R::RDX);
            break;
        }

        case IROpcode::EQ:  case IROpcode::NEQ:
        case IROpcode::LT:  case IROpcode::GT:
        case IROpcode::LTE: case IROpcode::GTE: {
            // String equality is CONTENT compare, not pointer compare (#6: `c is $1$`).
            if ((ins.opcode == IROpcode::EQ || ins.opcode == IROpcode::NEQ)
                && (isStrRef(op0()) || isStrRef(op1()))) {
                auto loadS = [&](const AC_IR::IRRef& r, R dst) {
                    if (r.kind == IRRef::Kind::CONST && r.value.type == AC_IR::IRType::STRING) {
                        int sid = sp.add(std::get<std::string>(r.value.data));
                        em.mov_ri64_str(dst, sid);
                    } else load(r, dst);
                };
                loadS(op0(), R::RDI);
                loadS(op1(), R::RSI);
                em.call("__ac_streq__");             // rax = 1 if equal
                if (ins.opcode == IROpcode::NEQ) {   // invert
                    em.cmp_r_i32(R::RAX, 0);
                    em.setcc_r(0x04, R::RAX);        // sete: 0→1, 1→0
                }
                store(ins.result, R::RAX);
                break;
            }
            bool anyFloat = isFloatRef(op0()) || isFloatRef(op1());
            uint8_t cc;
            if (anyFloat) {
                // #12: float-aware compare. Raw-bit integer cmp on IEEE doubles was wrong
                // (3.14 > 100 came out true). Convert the int side, ucomisd, and use the
                // UNSIGNED condition codes (ucomisd sets CF/ZF like an unsigned compare).
                load(op0(), R::RAX);
                if (isFloatRef(op0())) em.movq_xmm0_from_gpr(R::RAX);
                else                   em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                load(op1(), R::RCX);
                if (isFloatRef(op1())) em.movq_xmm1_from_gpr(R::RCX);
                else                   em.cvtsi2sd_xmm1_from_gpr(R::RCX);
                em.ucomisd_xmm0_xmm1();
                switch (ins.opcode) {
                case IROpcode::EQ:  cc=0x04; break; // sete
                case IROpcode::NEQ: cc=0x05; break; // setne
                case IROpcode::LT:  cc=0x02; break; // setb  (CF)
                case IROpcode::GT:  cc=0x07; break; // seta  (!CF & !ZF)
                case IROpcode::LTE: cc=0x06; break; // setbe
                default:            cc=0x03; break; // setae (GTE)
                }
            } else {
                load(op0(), R::RAX); load(op1(), R::RCX);
                em.cmp_rr(R::RAX, R::RCX);
                switch (ins.opcode) {
                case IROpcode::EQ:  cc=0x04; break;
                case IROpcode::NEQ: cc=0x05; break;
                case IROpcode::LT:  cc=0x0C; break;
                case IROpcode::GT:  cc=0x0F; break;
                case IROpcode::LTE: cc=0x0E; break;
                default:            cc=0x0D; break; // GTE
                }
            }
            em.setcc_r(cc, R::RAX);
            store(ins.result, R::RAX);
            break;
        }
        case IROpcode::NOT:
            load(op0(), R::RAX);
            em.cmp_r_i32(R::RAX, 0);
            em.setcc_r(0x04, R::RAX); // SETE
            store(ins.result, R::RAX);
            break;
        case IROpcode::AND:
            load(op0(), R::RAX); em.cmp_r_i32(R::RAX,0); em.setcc_r(0x05,R::RAX);
            load(op1(), R::RCX); em.cmp_r_i32(R::RCX,0); em.setcc_r(0x05,R::RCX);
            em.and_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::OR:
            load(op0(), R::RAX); em.cmp_r_i32(R::RAX,0); em.setcc_r(0x05,R::RAX);
            load(op1(), R::RCX); em.cmp_r_i32(R::RCX,0); em.setcc_r(0x05,R::RCX);
            em.or_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::XOR:
            load(op0(), R::RAX); em.cmp_r_i32(R::RAX,0); em.setcc_r(0x05,R::RAX);
            load(op1(), R::RCX); em.cmp_r_i32(R::RCX,0); em.setcc_r(0x05,R::RCX);
            em.xor_rr(R::RAX, R::RCX); // xor of booleans = !=
            store(ins.result, R::RAX);
            break;
        case IROpcode::XNOR:
            load(op0(), R::RAX); em.cmp_r_i32(R::RAX,0); em.setcc_r(0x05,R::RAX);
            load(op1(), R::RCX); em.cmp_r_i32(R::RCX,0); em.setcc_r(0x05,R::RCX);
            em.xor_rr(R::RAX, R::RCX);   // 0 if same, 1 if different
            em.xor_r_i8(R::RAX, 1);       // invert: 1 if same (xnor), 0 if different
            store(ins.result, R::RAX);
            break;
        case IROpcode::XSUB:
            // |a - b| + 1 using two's-complement abs: cqo / xor / sub / inc
            load(op0(), R::RAX);
            load(op1(), R::RCX);
            em.sub_rr(R::RAX, R::RCX);    // rax = a - b
            em.cqo();                      // sign-extend rax into rdx (0 or -1)
            em.xor_rr(R::RAX, R::RDX);    // flip bits if negative
            em.sub_rr(R::RAX, R::RDX);    // add 1 if was negative (abs)
            em.inc_r(R::RAX);             // +1 for inclusive count
            store(ins.result, R::RAX);
            break;

        // Bitwise operations (on integer values, not boolean)
        case IROpcode::BAND:
            load(op0(), R::RAX);
            load(op1(), R::RCX);
            em.and_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::BOR:
            load(op0(), R::RAX);
            load(op1(), R::RCX);
            em.or_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::BXOR:
            load(op0(), R::RAX);
            load(op1(), R::RCX);
            em.xor_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::BNOT:
            load(op0(), R::RAX);
            em.not_r(R::RAX);
            store(ins.result, R::RAX);
            break;

        case IROpcode::IF_BEGIN: {
            // Structured IF markers (cond/LineUp lower through these even on BNY).
            // They were silently ignored — every branch body executed sequentially.
            IfCtx c;
            c.elseL = uniq("__if" + std::to_string(ifCounter_) + "_else__");
            c.endL  = uniq("__if" + std::to_string(ifCounter_) + "_end__");
            ifCounter_++;
            c.sawElse = false;
            if (!ops.empty()) load(ops[0], R::RAX);
            else em.mov_ri32(R::RAX, 0);
            em.test_rr(R::RAX, R::RAX);
            em.je(c.elseL);
            ifStack_.push_back(c);
            break;
        }
        case IROpcode::IF_ELSE:
            if (!ifStack_.empty()) {
                auto& c = ifStack_.back();
                em.jmp(c.endL);
                em.label(c.elseL);
                c.sawElse = true;
            }
            break;
        case IROpcode::IF_END:
            if (!ifStack_.empty()) {
                auto c = ifStack_.back(); ifStack_.pop_back();
                if (!c.sawElse) em.label(c.elseL);
                em.label(c.endL);
            }
            break;

        case IROpcode::LABEL:
            if (!ops.empty() && ops[0].kind == IRRef::Kind::LABEL)
                em.label("L" + std::to_string(ops[0].id));
            break;
        case IROpcode::JUMP:
            if (!ops.empty() && ops[0].kind == IRRef::Kind::LABEL)
                em.jmp("L" + std::to_string(ops[0].id));
            break;
        case IROpcode::JUMP_IF_TRUE:
            if (ops.size()>=2 && ops[1].kind==IRRef::Kind::LABEL) {
                load(ops[0], R::RAX);
                em.cmp_r_i32(R::RAX,0);
                em.jne("L"+std::to_string(ops[1].id));
            }
            break;
        case IROpcode::JUMP_IF_FALSE:
            if (ops.size()>=2 && ops[1].kind==IRRef::Kind::LABEL) {
                load(ops[0], R::RAX);
                em.cmp_r_i32(R::RAX,0);
                em.je("L"+std::to_string(ops[1].id));
            }
            break;

        case IROpcode::CALL: {
            if (ops.empty()) break;
            // Indirect call: callee is a computed value (funcs[i](x)) — call through R10.
            if (ops[0].kind == IRRef::Kind::TEMP) {
                for (int ai = 1; ai < (int)ops.size() && (ai-1) < (int)abi.argRegs.size(); ai++)
                    load(ops[ai], abi.argRegs[ai-1]);
                load(ops[0], R::R10);
                em.call_r(R::R10);
                if (ins.result.isValid()) store(ins.result, R::RAX);
                break;
            }
            std::string rawFn = funcName(ops[0]);
            // widgets ctor (`root = Screen(...)`, `lbl = display(root, ...)`) — must be checked
            // against the RAW name too, same reasoning as the bundle-construction check right
            // below: no function is ever literally named "display" (see widgetVarKind_'s header
            // comment for the exact failure this fixes).
            if (isWidgetCtorName(rawFn)) {
                std::vector<IRRef> args(ops.begin() + 1, ops.end());
                if (bnyWidgetCtor(ins, rawFn, args)) break;
            }
            // A zero-arg dotted widget method call WITH parens (`pos_drop.get()`) lowers through
            // this plain CALL opcode instead of LIB_CALL — LIB_CALL is only used when the dotted
            // call actually carries args (see the LIR: `lib_call pos_drop.add, "..."` vs
            // `call pos_drop.get`). Same "zero-arg dot-call-with-parens takes a different IR path
            // than a with-args one" bug class as jarvis.ac's `speech.lower()` fix earlier this
            // session — different symptom (widget dispatch instead of string-cheese), same root
            // shape, so it needs its own check here rather than relying on emitLibCall alone.
            {
                auto dot = rawFn.find('.');
                if (dot != std::string::npos && widgetVarKind_) {
                    std::string recv = rawFn.substr(0, dot);
                    std::string mname = rawFn.substr(dot + 1);
                    if (widgetVarKind_->count(recv)) {
                        std::vector<IRRef> args(ops.begin() + 1, ops.end());
                        if (bnyWidgetMethod(ins, recv, mname, args)) break;
                    }
                }
            }
            // Same "zero-arg dot-call-with-parens takes a different IR path than a with-args
            // one" gap, this time for a BUNDLE INSTANCE method call (`q.getx()`, no args) — the
            // WITH-args case (`p.setx(5)`) already has this exact check inside LIB_CALL's own
            // handling (see its `instanceClass_->count(recv)` block); a zero-arg call never
            // reaches LIB_CALL at all, so it needs the identical check here too. Verified real
            // bug: "BNY: undefined label 'q.getx' referenced" — fell all the way through to
            // treating the dotted text as a literal (nonexistent) function label.
            {
                auto dot = rawFn.find('.');
                if (dot != std::string::npos && instanceClass_ && instanceClass_->count(rawFn.substr(0, dot))) {
                    std::string recv = rawFn.substr(0, dot);
                    std::string mname = rawFn.substr(dot + 1);
                    std::string cls = instanceClass_->at(recv);
                    loadNamedVar(recv, R::RDI);
                    for (size_t ai = 1; ai < ops.size() && ai < abi.argRegs.size(); ai++)
                        load(ops[ai], abi.argRegs[ai]);
                    em.call(cls + "_" + mname);
                    if (ins.result.isValid()) store(ins.result, R::RAX);
                    break;
                }
            }
            // `c = Critter()` — bundle construction. Must be checked against the RAW function
            // name, BEFORE resolveFunc's using-namespace prefixing runs: no function is ever
            // literally named "Critter" (only class-qualified METHODS like "Critter_init" are,
            // via compileFn's label — a class name on its own never appears as an `fn.name`
            // anywhere), so `resolveFunc`'s "is this user-defined?" scan always fails for a bare
            // class name and falls through to prefixing it with the first `using` namespace —
            // e.g. a program with `using math.sqrt` turned "Critter" into "math.Critter" (verified:
            // the full kitchen-sink file, which has `using math.sqrt`, silently exited before ever
            // reaching the bundle code — an isolated repro without that `using` line worked fine,
            // which is what made this one non-obvious). Previously this whole thing fell straight
            // through to the generic `em.call(fn)` at the bottom, calling a label that never
            // existed (only "Critter_init" did) — a hard "undefined label" error, or (with the
            // namespace-prefix bug) an even more confusing SILENT wrong-symbol failure.
            if (classFields_ && classFields_->count(rawFn)) {
                const std::string& fn = rawFn;
                int n = (int)(*classFields_)[fn].size();
                em.mov_ri32(R::RDI, 8 * (n > 0 ? n : 1));
                em.call("__ac_alloc__");
                em.mov_rr(R::RBX, R::RAX);        // rbx = new object ptr (callee-saved, survives init call)
                em.mov_rr(R::RDI, R::RBX);
                for (int ai = 1; ai < (int)ops.size() && (ai - 1) < (int)abi.argRegs.size() - 1; ai++)
                    load(ops[ai], abi.argRegs[ai]);   // extra constructor args start at 2nd reg (self is 1st)
                em.call(fn + "_init");
                em.mov_rr(R::RAX, R::RBX);
                if (ins.result.isValid()) {
                    store(ins.result, R::RAX);
                    std::string resName = varName(ins.result);
                    if (!resName.empty() && instanceClass_) (*instanceClass_)[resName] = fn;
                }
                break;
            }
            std::string fn = resolveFunc(rawFn);
            if (fn.empty()) break;
            if (fn == "ac_length" && ops.size() >= 2) {
                auto& a = ops[1];
                // `length $hello$` — a raw string LITERAL argument, not a variable/temp — never
                // matched either branch here (both check VAR/TEMP tracking sets, neither
                // recognizes a bare CONST STRING), so a direct-literal `length` call fell to the
                // ARRAY-length helper, reading the string's pointer bytes as if they were an
                // array's length header — garbage (verified: printed a random ~19-digit number
                // instead of 5). Same shape of bug as CStrategy's own `length $literal$` fix
                // earlier this session (see [[ac_v_trycatch_and_asm_full_rebuild]]).
                bool isStr = (a.kind == IRRef::Kind::VAR && stringVarNames_.count(varName(a)))
                          || (a.kind == IRRef::Kind::TEMP && stringTempIds_.count(a.id))
                          || (a.kind == IRRef::Kind::CONST && a.value.type == IRType::STRING);
                load(a, R::RDI);
                em.call(isStr ? "__ac_strlen__" : "ac_length");
                if (ins.result.isValid()) store(ins.result, R::RAX);
                break;
            }
            if (fn == "random.number" && ops.size() >= 2) {
                load(ops[1], R::RDI);
                em.call("__ac_rand__");
                if (ins.result.isValid()) store(ins.result, R::RAX);
                break;
            }
            if (fn == "random.choice" && ops.size() >= 2) {
                load(ops[1], R::R11);            // block ptr (R11: not in alloc pool, survives helper)
                em.mov_r_ptr(R::RDI, R::R11);    // rdi = len
                em.call("__ac_rand__");          // rax = idx in [0,len)
                em.inc_r(R::RAX);                // +1 (skip len header)
                em.mov_ri32(R::RCX, 8); em.imul_rr(R::RAX, R::RCX);
                em.add_rr(R::RAX, R::R11);
                em.mov_r_ptr(R::RAX, R::RAX);    // element
                if (ins.result.isValid()) store(ins.result, R::RAX);
                break;
            }
            bool floatReturn = isFloatReturningCall(fn);
            if ((fn == "ml.weights" || fn == "ml_weights" || fn == "ml.optimize" || fn == "ml_optimize") && ops.size() >= 3) {
                load(ops[1], R::RAX);
                if (isFloatRef(ops[1])) em.movq_xmm0_from_gpr(R::RAX);
                else                    em.cvtsi2sd_xmm0_from_gpr(R::RAX);
                load(ops[2], R::RDI);
                em.call(fn);
                if (ins.result.isValid()) store(ins.result, R::RAX);
                break;
            }
            // Load args into integer registers first, then fix up for float-taking functions
            for (int i = 1; i < (int)ops.size() && (i-1) < (int)abi.argRegs.size(); i++) {
                load(ops[i], abi.argRegs[i-1]);
            }
            int argCount = (int)ops.size() - 1;
            for (int ai = 0; ai < argCount && ai < 6 && ai < (int)abi.argRegs.size(); ++ai) {
                if (!callArgTakesDouble(fn, ai)) continue;   // #41: was capped at 2 args
                R arg = abi.argRegs[ai];
                if (isFloatRef(ops[1 + ai])) em.movq_xmmN_from_gpr(ai, arg);
                else                         em.cvtsi2sd_xmmN_from_gpr(ai, arg);
            }
            em.call(fn);
            // Float-returning calls put result in XMM0 per System V ABI — move to GPR for storage
            if (floatReturn) {
                em.movq_gpr_from_xmm0(R::RAX);
                if (ins.result.isValid()) {
                    if (ins.result.kind == AC_IR::IRRef::Kind::TEMP)
                        floatTempIds_.insert(ins.result.id);
                    else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                        std::string vn = varName(ins.result);
                        if (!vn.empty()) floatVarNames_.insert(vn);
                    }
                }
            }
            // char*-returning ilib calls (os.cwd(), stringm.upper(s), ...)
            if (returnsCString(fn) && ins.result.isValid()) {
                if (ins.result.kind == AC_IR::IRRef::Kind::TEMP)
                    stringTempIds_.insert(ins.result.id);
                else if (ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                    std::string vn = varName(ins.result);
                    if (!vn.empty()) stringVarNames_.insert(vn);
                }
            }
            if (ins.result.isValid()) {
                store(ins.result, R::RAX);
                // `q = f()` where f always constructs+returns one bundle class
                // (classReturnFuncs_'s prescan) — same treatment as a direct construct-call
                // (see its own block above) for resolveFieldAccess's instanceClass_ lookup,
                // even though the instance arrived across a function-return boundary. Verified
                // real bug: without this, `q.x` after `q = f()` silently read a field OFFSET
                // from whatever garbage/zeroed memory `q`'s own (wrongly untracked) slot held,
                // printing 0 instead of the real value — not a crash, just silently wrong.
                if (classReturnFuncs_ && ins.result.kind == AC_IR::IRRef::Kind::VAR) {
                    auto crf = classReturnFuncs_->find(rawFn);
                    if (crf != classReturnFuncs_->end()) {
                        std::string resName = varName(ins.result);
                        if (!resName.empty() && instanceClass_) (*instanceClass_)[resName] = crf->second;
                    }
                }
            }
            break;
        }

        case IROpcode::RETURN:
            // A generator's `return` (explicit or implicit-trailing) ends iteration, discarding
            // any value — matches every other backend's documented non-goal (no StopIteration
            // value). It never falls through to the normal call/ret epilogue below: nothing
            // ever `call`s into a generator body in the first place (see compileGeneratorFn),
            // so there's no return address on this stack to `ret` to.
            if (curFnIsGenerator_) { emitGenReturnSwap(); break; }
            if (!ops.empty())
                load(ops[0], R::RAX);
            else
                em.mov_ri32(R::RAX, 0);
            emitEpilogue();
            break;

        case IROpcode::YIELD: {
            R val = ops.empty() ? R::RAX : load(ops[0], R::RAX);
            loadGenCur(R::R11);
            em.mov_based_r(R::R11, GEN_OFF_VALUE, val);
            emitFiberSwap(R::R11, false);   // gen -> caller; falls back through on next GEN_NEXT
            break;
        }

        case IROpcode::GEN_CREATE: {
            // ops[0] = mkVar(calleeName) (matches CALL's own callee-ref convention), ops[1..] =
            // the generator's arguments.
            std::string calleeName = varName(ops[0]);
            int nargs = (int)ops.size() - 1;
            int totalBytes = GEN_HEADER_BYTES + 8 * nargs;
            totalBytes = (totalBytes + 15) & ~15;   // keep __ac_alloc__'s bump cursor 16-aligned
                                                     // for the stack allocation right after
            em.mov_ri32(abi.argRegs[0], totalBytes);
            em.call("__ac_alloc__");                // rax = state block
            em.push_r(R::RAX);
            em.mov_ri32(abi.argRegs[0], GEN_STACK_BYTES);
            em.call("__ac_alloc__");                // rax = fiber stack base
            em.mov_rr(R::R10, R::RAX);
            em.pop_r(R::R11);                       // r11 = state block, r10 = stack base

            // Stack top = align_down_16(base + size) — stacks grow down; align defensively
            // rather than trust the bump allocator's own running alignment (see the 16-round
            // above, which only guarantees THIS call started aligned, not that every earlier
            // caller of __ac_alloc__ elsewhere in the program did the same).
            em.add_ri32(R::R10, GEN_STACK_BYTES);
            em.mov_ri32(R::RAX, -16);
            em.and_rr(R::R10, R::RAX);

            em.mov_ri32(R::RAX, 0);
            em.mov_based_r(R::R11, GEN_OFF_DONE, R::RAX);
            em.mov_based_r(R::R11, GEN_OFF_GEN_RSP, R::R10);
            // genRBP/RBX/R12-R15 are left uninitialized: the body's own prologue (push_rbp;
            // mov_rbp_rsp) overwrites RBP immediately, and RBX/R12-R15 are pure scratch until
            // the body's FIRST yield/return saves real values into them — nothing ever reads
            // these particular slots before that first save writes them for real (locals live
            // on the fiber's own stack, not in these slots — see compileGeneratorFn's comment).
            em.lea_r_label(R::RAX, calleeName);     // generator body is compiled under its own name
            em.mov_based_r(R::R11, GEN_OFF_GEN_RIP, R::RAX);

            for (int i = 0; i < nargs; i++) {
                R v = load(ops[1 + i], R::RAX);
                em.mov_based_r(R::R11, GEN_HEADER_BYTES + 8 * i, v);
            }
            store(ins.result, R::R11);
            break;
        }

        case IROpcode::GEN_NEXT: {
            load(ops[0], R::R11);                   // r11 = handle
            std::string skipL = uniq("__ac_gen_skip_" + std::to_string(catchCounter_++) + "__");
            em.mov_r_based(R::RAX, R::R11, GEN_OFF_DONE);
            em.test_rr(R::RAX, R::RAX);
            em.jne(skipL);                          // never resume an already-done fiber
            em.mov_ri64_gvar(R::RAX, (*gvarSlots_)["__ac_gen_cur"]);
            em.mov_ptr_r(R::RAX, R::R11);            // ac_gen_cur = handle, so the body's own
                                                       // prologue/YIELD/RETURN can find it
            emitFiberSwap(R::R11, true);              // caller -> gen (resumes here on next yield)
            em.label(skipL);
            em.mov_r_based(R::RAX, R::R11, GEN_OFF_VALUE);
            store(ins.result, R::RAX);
            break;
        }

        case IROpcode::GEN_DONE: {
            load(ops[0], R::R11);
            em.mov_r_based(R::RAX, R::R11, GEN_OFF_DONE);
            store(ins.result, R::RAX);
            break;
        }

        case IROpcode::LIB_CALL:
            emitLibCall(ins);
            break;

        case IROpcode::PRINT:
            if (!ops.empty()) {
                auto& v = ops[0];
                if (v.kind == IRRef::Kind::CONST && v.value.type == AC_IR::IRType::STRING) {
                    std::string s = std::get<std::string>(v.value.data);
                    // Most Term.display call sites arrive here already $-stripped (upstream
                    // lowering strips them), but the `fn X & Y` chained-call syntax lowers
                    // differently and left the raw `$...$` delimiters attached — verified:
                    // printed literally "$chained-one$" instead of "chained-one". `load()`'s own
                    // CONST-STRING branch already defensively strips these for the same reason
                    // (see its comment); mirror that here too rather than special-casing the
                    // chained-call lowering specifically.
                    if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
                        s = s.substr(1, s.size() - 2);
                    int sid = sp.add(s);
                    em.mov_ri64_str(abi.argRegs[0], sid);
                    em.mov_ri32(abi.argRegs[1], (int32_t)s.size());
                    em.call("__ac_print_str__");
                    if (usesSave_) { em.mov_ri64_str(R::RDI, sid); em.call("__ac_save_append_cstr__"); }
                } else if ((v.kind == IRRef::Kind::VAR
                            && stringVarNames_.count(varName(v)))
                           || (v.kind == IRRef::Kind::TEMP
                               && stringTempIds_.count(v.id))) {
                    // String-typed value — slot holds a char* (Term.ask / ilib string return)
                    load(v, abi.argRegs[0]);
                    em.call("__ac_print_cstr__");
                    if (usesSave_) { load(v, R::RDI); em.call("__ac_save_append_cstr__"); }
                } else if (isArrRef(v)) {
                    load(v, abi.argRegs[0]);
                    em.call("__ac_print_arr__");   // [e0, e1, …] — matches PY
                    // Array capture isn't wired up (same narrow, documented scope boundary every
                    // other backend's emitCapture currently has — Term.display of a plain
                    // scalar/string is the demonstrated, verified case).
                } else if (isFloatRef(v)) {
                    load(v, R::RDI);
                    em.movq_xmm0_from_gpr(R::RDI);
                    em.call("ac_print_double");
                    if (usesSave_) { load(v, R::RDI); em.movq_xmm0_from_gpr(R::RDI); em.call("__ac_save_append_double__"); }
                } else {
                    load(v, abi.argRegs[0]);
                    em.call("__ac_print_int__");
                    if (usesSave_) { load(v, R::RDI); em.call("__ac_save_append_int__"); }
                }
            }
            break;

        case IROpcode::INPUT: {
            // INPUT: read integer from stdin (Term.ask)
            // operand[0] = prompt string
            if (!ops.empty()) {
                auto& prompt = ops[0];
                // Print prompt first
                if (prompt.kind == IRRef::Kind::CONST && prompt.value.type == AC_IR::IRType::STRING) {
                    std::string s = std::get<std::string>(prompt.value.data);
                    int sid = sp.add(s);
                    em.mov_ri64_str(abi.argRegs[0], sid);
                    em.mov_ri32(abi.argRegs[1], (int32_t)s.size());
                    em.call("__ac_print_str__");
                } else if (prompt.kind == IRRef::Kind::VAR
                           && stringVarNames_.count(varName(prompt))) {
                    load(prompt, abi.argRegs[0]);
                    em.call("__ac_print_cstr__");
                }
            }
            // Call __ac_input_str__ to read string from stdin; result is in RAX (pointer to buffer)
            em.call("__ac_input_str__");
            // Store the result (in RAX) to the destination variable
            // Mark as a string variable so it gets treated as string pointer
            if (ins.result.isValid()) {
                // Store the pointer to the input buffer
                store(ins.result, R::RAX);
                // Mark the destination as holding a string pointer (VAR or TEMP —
                // `to_int n = Term.ask ...` routes through a TEMP first).
                if (ins.result.kind == IRRef::Kind::VAR) {
                    stringVarNames_.insert(varName(ins.result));
                } else if (ins.result.kind == IRRef::Kind::TEMP) {
                    stringTempIds_.insert(ins.result.id);
                }
            }
            break;
        }

        case IROpcode::SAVE_FILE:
            if (!ops.empty()) {
                auto& v = ops[0];
                if (v.kind == IRRef::Kind::CONST && v.value.type == AC_IR::IRType::STRING) {
                    int sid = sp.add(std::get<std::string>(v.value.data));
                    em.mov_ri64_str(R::RDI, sid);
                } else {
                    load(v, R::RDI);
                }
                em.call("__ac_save_file__");
            }
            break;

        case IROpcode::HALT:
            emitHalt();
            break;

        // ── #29: these were silently DROPPED (no case → try+catch both ran, /stop ignored) ──
        case IROpcode::SOFT_HALT:            // /stop — graceful exit(0)
            em.mov_ri32(R::RAX, 231);
            em.xor_rr(R::RDI, R::RDI);
            em.syscall();
            break;

        case IROpcode::SLEEP: {              // /halt n — nanosleep(n seconds)
            long long secs = 0;
            if (!ops.empty() && ops[0].kind == IRRef::Kind::CONST) {
                const auto& v = ops[0].value;
                if (v.type == AC_IR::IRType::INT)    secs = std::get<int64_t>(v.data);
                else if (v.type == AC_IR::IRType::FLOAT) secs = (long long)std::get<double>(v.data);
                else if (v.type == AC_IR::IRType::STRING) { try { secs = std::stoll(std::get<std::string>(v.data)); } catch (...) {} }
            }
            // timespec {tv_sec, tv_nsec} on the stack (16 bytes, kept 16-aligned)
            em.sub_rsp_i32(16);
            em.mov_ri64(R::RAX, secs);
            em.mov_ptr_r(R::RSP, R::RAX);            // tv_sec
            em.mov_rr(R::RDI, R::RSP);
            em.add_ri32(R::RDI, 8);
            em.xor_rr(R::RAX, R::RAX);
            em.mov_ptr_r(R::RDI, R::RAX);            // tv_nsec = 0
            em.mov_rr(R::RDI, R::RSP);               // rdi = &timespec
            em.xor_rr(R::RSI, R::RSI);               // rem = NULL
            em.mov_ri32(R::RAX, 35);                 // sys_nanosleep
            em.syscall();
            em.add_rsp_i32(16);
            break;
        }

        case IROpcode::RAISE_CLAUSE: {       // raise Clause($msg$) → "Clause: msg" on stderr + exit(1)
            std::string clause = (!ops.empty() && ops[0].kind == IRRef::Kind::CONST
                                  && ops[0].value.type == AC_IR::IRType::STRING)
                                 ? std::get<std::string>(ops[0].value.data) : "Preposterous";
            std::string msg = (ops.size() > 1 && ops[1].kind == IRRef::Kind::CONST
                               && ops[1].value.type == AC_IR::IRType::STRING)
                              ? std::get<std::string>(ops[1].value.data) : "";
            if (clause == "hint") clause = "Suggestion"; else if (clause == "toxic") clause = "Toxic";
            std::string line = clause + ": " + msg + "\n";
            int sid = sp.add(line);
            em.mov_ri64_str(R::RSI, sid);
            em.mov_ri32(R::RDI, 2);                  // stderr
            em.mov_ri32(R::RDX, (int32_t)line.size());
            em.mov_ri32(R::RAX, 1);                  // sys_write
            em.syscall();
            // `raise Clause(...)` is non-fatal on EVERY other backend (and PY, the reference) —
            // it prints "Clause: msg" to stderr and execution continues, regardless of whether
            // the clause is hint/toxic/praise or a custom name. This exit(1) for anything else
            // was a genuine BNY-only bug, not a deliberate design difference: verified via
            // `examples/keyword_catalog_core.ac` — `raise MyClause(...)` silently killed the
            // program before it ever reached the code that follows (a bundle construction, in
            // this case, but ANY code after a custom raise clause was equally unreachable).
            break;
        }

        case IROpcode::TRY_BEGIN: {
            // Real try/catch via a hand-rolled setjmp equivalent (see this class's `usesTry_`
            // comment for the full design). Saves RSP/RBP/RBX/R12-R15/catch-target into a
            // depth-indexed 64-byte slot — the ENTIRE callee-saved register set, not just
            // RSP/RBP: the div-by-zero is usually reached through a CALL into a different
            // function, and jmp_r bypasses that function's normal epilogue (which would
            // otherwise pop/restore any callee-saved reg it modified) — so all of them must be
            // captured here and restored in emitDivZeroTrap, or the catch body could see
            // corrupted values in whatever the register allocator assigned to RBX/R12-R15
            // before the try. R8 holds the slot pointer throughout (RBX is now a SAVED VALUE,
            // not scratch).
            std::string catchEntryL = uniq("__ac_catch_entry_" + std::to_string(catchCounter_++) + "__");
            catchEntry_.push_back(catchEntryL);
            int stackSlot = (*gvarSlots_)["__try_stack_ptr"];
            int depthSlot = (*gvarSlots_)["__try_depth"];

            std::string haveBufL = uniq("__ac_try_havebuf_" + std::to_string(catchCounter_++) + "__");
            em.mov_ri64_gvar(R::RCX, stackSlot);
            em.mov_r_ptr(R::RAX, R::RCX);
            em.test_rr(R::RAX, R::RAX);
            em.jne(haveBufL);
            em.push_r(R::RCX);
            em.mov_ri32(R::RDI, 32 * 64);      // 32 nesting slots * 64 bytes/slot
            em.call("__ac_alloc__");
            em.pop_r(R::RCX);
            em.mov_ptr_r(R::RCX, R::RAX);
            em.label(haveBufL);
            em.mov_r_ptr(R::R8, R::RCX);        // r8 = try-stack base

            em.mov_ri64_gvar(R::RCX, depthSlot);
            em.mov_r_ptr(R::RAX, R::RCX);
            em.mov_rr(R::RDX, R::RAX);
            em.mov_ri32(R::RAX, 64);
            em.imul_rr(R::RAX, R::RDX);         // rax = depth * 64
            em.add_rr(R::R8, R::RAX);           // r8 = slot address

            em.mov_ptr_r(R::R8, R::RSP); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::RBP); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::RBX); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::R12); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::R13); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::R14); em.add_ri32(R::R8, 8);
            em.mov_ptr_r(R::R8, R::R15); em.add_ri32(R::R8, 8);
            em.lea_r_label(R::RAX, catchEntryL);  // forward reference — defined in CATCH_BEGIN below
            em.mov_ptr_r(R::R8, R::RAX);

            em.mov_ri64_gvar(R::RCX, depthSlot);
            em.mov_r_ptr(R::RAX, R::RCX);
            em.inc_r(R::RAX);
            em.mov_ptr_r(R::RCX, R::RAX);
            break;
        }
        case IROpcode::CATCH_BEGIN: {
            // Reached two ways: (1) the try body completed normally and fell through here — must
            // decrement depth (TRY_BEGIN incremented it) and skip the catch body entirely; (2) the
            // div-by-zero guard jmp_r'd directly to catchEntry_'s label (below) after ALREADY
            // restoring RSP/RBP and decrementing depth itself — falls straight into the catch body.
            if (!(*gvarSlots_).count("__try_depth")) { // defensive: matches the pre-existing no-op if usesTry_ somehow wasn't set
                std::string skipL = uniq("__ac_catch_skip_" + std::to_string(catchCounter_++) + "__");
                em.jmp(skipL);
                catchSkip_.push_back(skipL);
                break;
            }
            int depthSlot = (*gvarSlots_)["__try_depth"];
            em.mov_ri64_gvar(R::RCX, depthSlot);
            em.mov_r_ptr(R::RAX, R::RCX);
            em.dec_r(R::RAX);
            em.mov_ptr_r(R::RCX, R::RAX);

            std::string skipL = uniq("__ac_catch_skip_" + std::to_string(catchCounter_++) + "__");
            em.jmp(skipL);
            catchSkip_.push_back(skipL);
            if (!catchEntry_.empty()) { em.label(catchEntry_.back()); catchEntry_.pop_back(); }
            break;
        }
        case IROpcode::AFTER_BEGIN:          // after-block always runs — close pending skip here
        case IROpcode::TRY_END:
            if (!catchSkip_.empty()) { em.label(catchSkip_.back()); catchSkip_.pop_back(); }
            break;

        // ─── Phase 3: Memory & Pointers (Stubs - pointers not in AC yet) ─────
        case IROpcode::ALLOC: {
            // dict = alloc "dict", "$k$:v,..." → heap block: [cap][n][k0][v0][k1][v1]...
            // Hidden capacity word at ptr[-8] (one slot before the n word every other
            // dict-reading path already knows about — length/get only ever read ptr[0]=n
            // and ptr[8+16i]/ptr[16+16i] for keys/values) — mirrors the array literal's
            // [cap][len][e0]... header. Lets __ac_dict_set__ grow in place (amortized O(1))
            // instead of allocating+copying on every single insert.
            if (!ops.empty() && ops[0].kind == AC_IR::IRRef::Kind::CONST
                && ops[0].value.type == AC_IR::IRType::STRING
                && std::get<std::string>(ops[0].value.data) == "dict"
                && ins.result.isValid()) {
                em.mov_ri32(R::RDI, 16 * (8 + 1));      // seed cap = 8 pairs
                em.call("__ac_alloc__");
                em.mov_ri32(R::RCX, 8);
                em.mov_ptr_r(R::RAX, R::RCX);          // raw[0] = cap
                em.add_ri32(R::RAX, 8);                // rax = ptr (skip cap word)
                em.mov_ri32(R::RCX, 0);
                em.mov_ptr_r(R::RAX, R::RCX);          // ptr[0] = n = 0
                store(ins.result, R::RAX);
                markDstDict(ins.result);
                // materialize literal pairs via __ac_dict_set__ (block may move on growth)
                std::string content = ops.size() >= 2 && ops[1].kind == AC_IR::IRRef::Kind::CONST
                                      && ops[1].value.type == AC_IR::IRType::STRING
                                      ? std::get<std::string>(ops[1].value.data) : "";
                std::string rest = content;
                while (!rest.empty()) {
                    auto comma = rest.find(',');
                    std::string pair = comma == std::string::npos ? rest : rest.substr(0, comma);
                    rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
                    auto colon = pair.find(':');
                    if (colon == std::string::npos) continue;
                    std::string k = pair.substr(0, colon), v = pair.substr(colon + 1);
                    auto trim = [](std::string& x){ size_t a=x.find_first_not_of(' '), b=x.find_last_not_of(' ');
                        x = (a==std::string::npos) ? "" : x.substr(a, b-a+1); };
                    trim(k); trim(v);
                    if (k.size() >= 2 && k.front() == '$' && k.back() == '$') k = k.substr(1, k.size()-2);
                    load(ins.result, R::RDI);          // current block
                    em.mov_ri64_str(R::RSI, sp.add(k));
                    // A `$..$`-wrapped value is a string literal — store its string-pool ID
                    // (same 8-byte-slot representation a plain string var uses elsewhere), not
                    // an attempted int parse. A dict is naturally heterogeneous per-slot; which
                    // reads should be treated as strings is decided at LOAD_INDEX call sites via
                    // dictStrKeysByVar_ (populated from this same $..$ scan during the prescan).
                    if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
                        em.mov_ri64_str(R::RDX, sp.add(v.substr(1, v.size()-2)));
                    else {
                        int64_t vi = 0; try { vi = std::stoll(v); } catch (...) {}
                        em.mov_ri64(R::RDX, (uint64_t)vi);
                    }
                    em.call("__ac_dict_set__");
                    store(ins.result, R::RAX);         // possibly-moved block
                }
                break;
            }
            // range N → [0..N-1]; sequence(a,b) → [a..b-1] (both exclusive upper bound, step 1;
            // xrange/xiota/stream/iota all desugar to one of these two — see ir.cpp). A REAL
            // materialized array, same [cap][len][elems] layout "list" uses below, computed via
            // a runtime fill loop since the bound(s) may be dynamic, not compile-time literals.
            // Verified real bug this closes: NEITHER kind was ever handled here at all before —
            // `ins.result` was simply never stored to, leaving it holding whatever was already
            // in that slot (stale stack/register value, non-deterministic under ASLR) —
            // `Term.display iota 5` printed a huge random-looking integer instead of the range.
            if (!ops.empty() && ops[0].kind == AC_IR::IRRef::Kind::CONST
                && ops[0].value.type == AC_IR::IRType::STRING
                && (std::get<std::string>(ops[0].value.data) == "range"
                 || std::get<std::string>(ops[0].value.data) == "sequence")
                && ins.result.isValid()) {
                // Unique per-occurrence labels — this block is INLINED at every call site (not
                // a shared subroutine like ac_print_double), so a function with more than one
                // range/sequence/xrange/xiota/stream in it needs each instance's labels to be
                // distinct. Verified real bug: without this, two such ALLOCs in one function
                // both defined "__acr_fill__" etc into the SAME shared label map — the SECOND
                // definition wins for BOTH occurrences (same class of collision the FATAL
                // cross-function fix above this class already closed, just within one function
                // instead of across two) — every EARLIER range/sequence in that function silently
                // printed nothing (its jumps landed in the LAST occurrence's fill loop instead).
                std::string cntokL = uniq("__acr" + std::to_string(rangeCounter_) + "_cntok__");
                std::string fillL  = uniq("__acr" + std::to_string(rangeCounter_) + "_fill__");
                std::string doneL  = uniq("__acr" + std::to_string(rangeCounter_) + "_filldone__");
                rangeCounter_++;
                bool isRange = std::get<std::string>(ops[0].value.data) == "range";
                // `sequence(a,b,step)`/`stream(a,b,step)` carry an explicit 3rd/step operand
                // (see ir.cpp's SequenceExpr/StreamExpr lowering, sops[3]). Verified real bug
                // this closes: this materialize-as-array path always assumed step==1 (count =
                // end-start, fill via start+i) — a step operand was computed by the caller but
                // never read here at all, silently ignored (only the far more common direct
                // `FOR v in sequence(a,b,step)` loop, a completely separate lowering path in
                // ir.cpp, honored it). `range`/`iota` have no step concept (always 0..N-1).
                bool hasStep = !isRange && ops.size() >= 4;
                if (isRange) {
                    em.mov_ri32(R::R13, 0);
                    R endR = load(ops[1], R::R14);
                    if (endR != R::R14) em.mov_rr(R::R14, endR);
                } else {
                    R startR = load(ops[1], R::R13);
                    if (startR != R::R13) em.mov_rr(R::R13, startR);
                    R endR = load(ops.size() >= 3 ? ops[2] : ops[1], R::R14);
                    if (endR != R::R14) em.mov_rr(R::R14, endR);
                }
                if (hasStep) {
                    R stepR = load(ops[3], R::R12);
                    if (stepR != R::R12) em.mov_rr(R::R12, stepR);
                }
                if (!hasStep) {
                    em.mov_rr(R::R15, R::R14);
                    em.sub_rr(R::R15, R::R13);          // r15 = count = end - start
                    em.cmp_r_i32(R::R15, 0);
                    em.jge(cntokL);
                    em.mov_ri32(R::R15, 0);              // clamp: descending/empty range → 0 elements
                    em.label(cntokL);
                } else {
                    // Pass 1: count iterations by walking start→end in `step` increments
                    // (R11: iterator copy, not in the allocator's pool, safe scratch here).
                    // Direction (ascending vs. descending) is decided at RUNTIME from step's
                    // actual sign in R12 — a literal `-3` step lowers to a SUB(0,3) temp, not
                    // a negative CONST IRRef (constant folding, if it runs, keeps this operand
                    // as a TEMP backed by a LOAD_CONST rather than rewriting the ALLOC's operand
                    // list itself — verified by testing), so a compile-time-only sign check
                    // (mirroring the FOR-loop lowering's own limited negStep heuristic, ir.cpp
                    // ~5107) silently misses every literal-negative-step case here. A real
                    // runtime branch handles literal, computed, and variable steps uniformly.
                    std::string descL   = uniq("__acr" + std::to_string(rangeCounter_) + "_desc__");
                    std::string cloopAL = uniq("__acr" + std::to_string(rangeCounter_) + "_cloopA__");
                    std::string cloopDL = uniq("__acr" + std::to_string(rangeCounter_) + "_cloopD__");
                    em.mov_ri32(R::R15, 0);              // count
                    em.mov_rr(R::R11, R::R13);           // iter = start
                    em.cmp_r_i32(R::R12, 0);
                    em.jl(descL);
                    em.label(cloopAL);
                    em.cmp_rr(R::R11, R::R14);
                    em.jge(cntokL);
                    em.inc_r(R::R15);
                    em.add_rr(R::R11, R::R12);
                    em.jmp(cloopAL);
                    em.label(descL);
                    em.label(cloopDL);
                    em.cmp_rr(R::R11, R::R14);
                    em.jle(cntokL);
                    em.inc_r(R::R15);
                    em.add_rr(R::R11, R::R12);
                    em.jmp(cloopDL);
                    em.label(cntokL);
                }
                em.mov_rr(R::RDI, R::R15);
                em.add_ri32(R::RDI, 2);
                em.shl_r_i8(R::RDI, 3);              // rdi = (count+2)*8 bytes
                em.call("__ac_alloc__");
                em.mov_ptr_r(R::RAX, R::R15);        // raw[0] = cap = count
                em.add_ri32(R::RAX, 8);              // rax = ptr (skip cap word)
                em.mov_rr(R::RBX, R::RAX);           // rbx = ptr, held through the fill loop
                em.mov_ptr_r(R::RBX, R::R15);        // ptr[0] = len = count
                em.mov_rr(R::RDX, R::RBX); em.add_ri32(R::RDX, 8); // rdx = &ptr[1], write cursor
                em.mov_ri32(R::RCX, 0);              // rcx = loop index i
                if (!hasStep) {
                    em.label(fillL);
                    em.cmp_rr(R::RCX, R::R15);
                    em.jge(doneL);
                    em.mov_rr(R::RAX, R::R13);
                    em.add_rr(R::RAX, R::RCX);           // rax = start + i
                    em.mov_ptr_r(R::RDX, R::RAX);
                    em.add_ri32(R::RDX, 8);
                    em.inc_r(R::RCX);
                    em.jmp(fillL);
                    em.label(doneL);
                } else {
                    em.mov_rr(R::R11, R::R13);           // iter = start (reset for pass 2)
                    em.label(fillL);
                    em.cmp_rr(R::RCX, R::R15);
                    em.jge(doneL);
                    em.mov_ptr_r(R::RDX, R::R11);
                    em.add_ri32(R::RDX, 8);
                    em.add_rr(R::R11, R::R12);           // iter += step
                    em.inc_r(R::RCX);
                    em.jmp(fillL);
                    em.label(doneL);
                }
                store(ins.result, R::RBX);
                break;
            }
            // arr = alloc "list", "e0,e1,..."  → heap block: [len][e0][e1]...
            // Integer-literal elements are materialized here; others get a 0 placeholder
            // and are typically overwritten via STORE_INDEX.
            // Elements: integer literals, user-function NAMES (funcs = [f1, f2] — store the
            // function's address via a rip-relative lea), or a known dict-var NAME (datac
            // multi-row import: pets = [dc_pets_0, dc_pets_1] — each token already names a
            // dict block var declared earlier in this same function; load its current value
            // at runtime rather than treating the name as an unparseable placeholder).
            struct Elem { int64_t val; std::string funcLabel; std::string varLoad; };
            std::vector<Elem> elems;
            if (ops.size() >= 2 && ops[1].kind == AC_IR::IRRef::Kind::CONST
                    && ops[1].value.type == AC_IR::IRType::STRING) {
                const std::string& s = std::get<std::string>(ops[1].value.data);
                size_t i = 0;
                while (i < s.size()) {
                    size_t j = s.find(',', i);
                    std::string tok = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
                    size_t a = tok.find_first_not_of(" \t");
                    size_t b = tok.find_last_not_of(" \t");
                    if (a != std::string::npos) {
                        tok = tok.substr(a, b - a + 1);
                        bool isFn = false;
                        for (auto& fn : prog.functions)
                            if (fn.name == tok) { isFn = true; break; }
                        if (isFn) elems.push_back({0, tok, ""});
                        else if (dictVarNames_.count(tok)) elems.push_back({0, "", tok});
                        else {
                            try { elems.push_back({std::stoll(tok), "", ""}); }
                            catch (...) { elems.push_back({0, "", ""}); }
                        }
                    }
                    if (j == std::string::npos) break;
                    i = j + 1;
                }
            }
            int64_t N = (int64_t)elems.size();
            // Layout: [cap][len][e0][e1]...  — a hidden capacity word lives ONE slot before
            // the length word that every other array-reading path already knows about
            // (indexing/iteration/print all only ever touch ptr[0]=len and ptr[8+8i]=elem_i,
            // completely unaware ptr[-8] exists). This lets __ac_append__ grow in place
            // (O(1) amortized, capacity-doubling) without any other code path changing —
            // "ptr" returned here still means exactly what it always meant. Every array must
            // be created here (the only ALLOC "list" site in BNY) so every array consistently
            // carries this header; seed cap >= 4 so small lists don't immediately re-grow.
            int64_t CAP0 = N > 4 ? N : 4;
            em.mov_ri32(R::RDI, (int32_t)((CAP0 + 2) * 8));
            em.call("__ac_alloc__");                  // RAX = raw block
            em.mov_ri32(R::RCX, (int32_t)CAP0);
            em.mov_ptr_r(R::RAX, R::RCX);             // raw[0] = cap
            em.add_ri32(R::RAX, 8);                   // RAX = ptr (skip cap word)
            em.mov_ri32(R::RCX, (int32_t)N);
            em.mov_ptr_r(R::RAX, R::RCX);             // ptr[0] = length
            em.mov_rr(R::RDX, R::RAX); em.add_ri32(R::RDX, 8); // RDX = &ptr[1]
            for (int64_t k = 0; k < N; k++) {
                if (!elems[k].funcLabel.empty())
                    em.lea_r_label(R::RCX, elems[k].funcLabel);
                else if (!elems[k].varLoad.empty())
                    loadNamedVar(elems[k].varLoad, R::RCX);
                else
                    em.mov_ri64(R::RCX, (uint64_t)elems[k].val);
                em.mov_ptr_r(R::RDX, R::RCX);
                em.add_ri32(R::RDX, 8);
            }
            if (ins.result.isValid()) store(ins.result, R::RAX);
            break;
        }

        case IROpcode::FREE: {
            // No-op: the bump allocator never frees.
            break;
        }

        case IROpcode::LOAD_INDEX: {
            // arr[idx] (0-based in IR) → block[1+idx]; special index "__len__" → block[0].
            if (ins.result.isValid() && !ops.empty()) {
                bool isLen = ops.size() >= 2 && ops[1].kind == AC_IR::IRRef::Kind::CONST
                          && ops[1].value.type == AC_IR::IRType::STRING
                          && std::get<std::string>(ops[1].value.data) == "__len__";
                // dict[key] → __ac_dict_get__ (string keys; content compare inside)
                if (!isLen && ops.size() >= 2 && isDictRef(ops[0])) {
                    load(ops[0], R::RDI);
                    if (ops[1].kind == AC_IR::IRRef::Kind::CONST
                        && ops[1].value.type == AC_IR::IRType::STRING) {
                        int sid = sp.add(std::get<std::string>(ops[1].value.data));
                        em.mov_ri64_str(R::RSI, sid);
                    } else load(ops[1], R::RSI);
                    em.call("__ac_dict_get__");
                    store(ins.result, R::RAX);
                    break;
                }
                // s[i] on a STRING → 1-char heap string (AC semantics).
                // NOTE: do NOT use RBX/R12 inline — the register allocator owns callee-saved
                // regs for loop vars; clobbering them corrupted FOR counters. Stack-save instead.
                if (!isLen && ops.size() >= 2 && isStrRef(ops[0])) {
                    load(ops[0], R::RAX);             // char*
                    em.push_r(R::RAX);
                    load(ops[1], R::RAX);             // index
                    em.push_r(R::RAX);
                    em.mov_ri32(R::RDI, 2);
                    em.call("__ac_alloc__");          // rax = 2-byte block (clobbers caller-saved)
                    em.pop_r(R::RCX);                 // idx
                    em.pop_r(R::RSI);                 // char*
                    em.add_rr(R::RSI, R::RCX);        // &s[idx]
                    em.movzx_r64_ptr8(R::RCX, R::RSI);
                    em.mov_ptr_r8(R::RAX, R::RCX);    // dst[0] = ch
                    em.mov_rr(R::RDI, R::RAX); em.inc_r(R::RDI);
                    em.mov_ri32(R::RCX, 0);
                    em.mov_ptr_r8(R::RDI, R::RCX);    // dst[1] = NUL
                    store(ins.result, R::RAX);
                    markDstString(ins.result);
                    break;
                }
                if (isLen && isStrRef(ops[0])) {      // FOR over a STRING: length = strlen
                    load(ops[0], R::RDI);
                    em.call("__ac_strlen__");
                    store(ins.result, R::RAX);
                    break;
                }
                load(ops[0], R::RAX);                 // block ptr
                if (isLen) {
                    em.mov_r_ptr(R::RAX, R::RAX);     // length = [block]
                } else if (ops.size() >= 2) {
                    load(ops[1], R::RCX);             // index
                    em.inc_r(R::RCX);                 // +1 (skip length header)
                    em.mov_ri32(R::RDX, 8); em.imul_rr(R::RCX, R::RDX); // (idx+1)*8
                    em.add_rr(R::RAX, R::RCX);        // &element
                    em.mov_r_ptr(R::RAX, R::RAX);     // element value
                }
                store(ins.result, R::RAX);
            }
            break;
        }

        case IROpcode::STORE_INDEX: {
            // dict[key] = val → __ac_dict_set__ (block may move; write it back to the var)
            if (ops.size() >= 3 && isDictRef(ops[0])) {
                load(ops[0], R::RDI);
                if (ops[1].kind == AC_IR::IRRef::Kind::CONST
                    && ops[1].value.type == AC_IR::IRType::STRING) {
                    int sid = sp.add(std::get<std::string>(ops[1].value.data));
                    em.mov_ri64_str(R::RSI, sid);
                } else load(ops[1], R::RSI);
                load(ops[2], R::RDX);
                em.call("__ac_dict_set__");
                store(ops[0], R::RAX);
                break;
            }
            // arr[idx] = val  (ops: block, index, value)
            if (ops.size() >= 3) {
                load(ops[0], R::RAX);                 // block ptr
                load(ops[1], R::RCX);                 // index
                em.inc_r(R::RCX);
                em.mov_ri32(R::RDX, 8); em.imul_rr(R::RCX, R::RDX);
                em.add_rr(R::RAX, R::RCX);            // &element
                load(ops[2], R::RDX);                 // value
                em.mov_ptr_r(R::RAX, R::RDX);
            }
            break;
        }

        // Event-listener (`configure event-listener`/`on value is X`/`bind KEY to FUNC`) —
        // see emitEventBindLinux/emitEventTriggerLinux's own header comment for why BNY needed
        // this ported specially (no fixed .bss array primitive). ops = {key_const, callback_var}
        // for EVENT_BIND (mirrors ir.cpp's `i.typedOperands = {mkConst(key), mkVar(cbName)}`),
        // {key_const} for EVENT_TRIGGER.
        case IROpcode::EVENT_BIND:
            if (ops.size() >= 2 && ops[0].kind == IRRef::Kind::CONST
                && ops[0].value.type == AC_IR::IRType::STRING) {
                std::string key = std::get<std::string>(ops[0].value.data);
                if (key.size() >= 2 && key.front() == '$' && key.back() == '$')
                    key = key.substr(1, key.size() - 2);
                int sid = sp.add(key);
                em.mov_ri64_str(R::RDI, sid);
                em.lea_r_label(R::RSI, funcName(ops[1]));
                em.call("__ac_bind__");
            }
            break;
        case IROpcode::EVENT_TRIGGER:
            if (!ops.empty() && ops[0].kind == IRRef::Kind::CONST
                && ops[0].value.type == AC_IR::IRType::STRING) {
                std::string key = std::get<std::string>(ops[0].value.data);
                if (key.size() >= 2 && key.front() == '$' && key.back() == '$')
                    key = key.substr(1, key.size() - 2);
                int sid = sp.add(key);
                em.mov_ri64_str(R::RDI, sid);
                em.call("__ac_trigger__");
            }
            break;

        // `atomic` read-modify-write brackets (see the STORE_VAR case's own comment
        // above, and ir.cpp's emitCompoundRef/AssignStmt) — the same real spinlock
        // every atomic var already uses, just held across the whole bracketed span
        // instead of a single instruction.
        case IROpcode::LOCK_BEGIN:
            em.call("__ac_atomic_lock__");
            break;
        case IROpcode::LOCK_END:
            em.call("__ac_atomic_unlock__");
            break;

        case IROpcode::NOP:
        default:
            break;
        }
    }

public:
    FuncCompiler(const AC_IR::IRProgram& p, Em& e, StringPool& s, ABI a,
                 bool glob=false, std::set<std::string> using_hdrs={})
        : prog(p), em(e), sp(s), abi(a), isGlobal(glob), usingHeaders_(std::move(using_hdrs)) {}

    // Pre-scan: determine which variables will ever hold float values.
    // Needed because a loop variable may be int on first write but float on subsequent
    // iterations — we need to emit the right SSE path for ALL uses, not just after first write.
    void preScanFloats(const std::vector<AC_IR::IRInstruction>& instrs) {
        using namespace AC_IR;
        bool changed = true;
        while (changed) {  // iterate to fixpoint for propagation through loops
            changed = false;
            for (const auto& ins : instrs) {
                auto markDstFloat = [&](const IRRef& dst) {
                    if (dst.kind == IRRef::Kind::TEMP) {
                        if (!floatTempIds_.count(dst.id)) { floatTempIds_.insert(dst.id); changed = true; }
                    } else if (dst.kind == IRRef::Kind::VAR) {
                        std::string vn = varName(dst);
                        if (!vn.empty() && !floatVarNames_.count(vn)) { floatVarNames_.insert(vn); changed = true; }
                    }
                };
                // STORE_VAR of a float constant or float-typed source
                if (ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL
                    || ins.opcode == IROpcode::LOAD_CONST) {   // folded call results
                    // The VALUE being stored is operands[0] in the result-form (STORE_VAR result=tgt,
                    // {value}) but operands[1] in the two-operand form (STORE_VAR {tgt, value}).
                    // Reading operands[0] unconditionally checked the TARGET's float-ness, not the
                    // value's → float never propagated through `total = t_2`, the final step of
                    // `total += math.mod(...)`, so the accumulator stayed int and summed raw bits.
                    bool hasResultForm = ins.result.isValid() && !ins.typedOperands.empty();
                    IRRef src = hasResultForm ? ins.typedOperands[0]
                              : (ins.typedOperands.size() >= 2 ? ins.typedOperands[1] : IRRef());
                    bool srcFloat = (src.kind == IRRef::Kind::CONST && src.value.type == IRType::FLOAT)
                                 || isFloatRef(src);
                    // A STORE_VAR whose OWN resultType is a non-float qualifier (ATOMIC/
                    // SHORT/MINI) means the value is being COERCED into that type at this
                    // exact store, not turning the var float — ir.cpp's sticky-keep emits
                    // a plain STORE_VAR (not TYPE_CAST) whenever a var's type doesn't
                    // actually change, e.g. `atomic x = 5; x = 5.5` stays ATOMIC
                    // end-to-end, so the literal source being float-typed doesn't mean x
                    // itself should be. Verified real bug this closes: without this guard
                    // x got marked float program-wide from that LATER store, corrupting
                    // the EARLIER `x += 3` (computed/stored assuming x was a genuine
                    // double, garbage result) even though it runs first.
                    bool coercedNonFloat = ins.resultType != IRType::VOID
                        && ins.resultType != IRType::FLOAT
                        && (ins.resultType == IRType::ATOMIC || irIntWidth(ins.resultType));
                    if (srcFloat && !coercedNonFloat) {
                        if (ins.result.isValid()) markDstFloat(ins.result);
                        else if (ins.typedOperands.size() >= 2) markDstFloat(ins.typedOperands[0]);
                    }
                }
                // TYPE_CAST to FLOAT — needed for automatic-retype (ir.cpp's AssignStmt: a
                // plain reassignment auto-inserts a TYPE_CAST the moment a var's value type
                // changes, e.g. `x=5; x=5.5;`). Without this, `x`'s FIRST assignment (a
                // plain STORE_VAR, emitted before the fixpoint has ever seen the LATER
                // TYPE_CAST) had no way to know `x` would eventually be float, so it stored
                // a real int64 that every subsequent float-typed read then reinterpreted as
                // raw double bits (verified: printed a denormalized garbage float instead
                // of the real integer value for the pre-retype print).
                if (ins.opcode == IROpcode::TYPE_CAST && ins.resultType == IRType::FLOAT
                    && ins.result.isValid())
                    markDstFloat(ins.result);
                // DIV always produces float
                if ((ins.opcode == IROpcode::DIV || ins.opcode == IROpcode::FDIV) && ins.result.isValid()) markDstFloat(ins.result);
                // Arithmetic with any float operand → float result
                if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                  || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::PMUL)
                    && ins.result.isValid() && ins.typedOperands.size() >= 2) {
                    if (isFloatRef(ins.typedOperands[0]) || isFloatRef(ins.typedOperands[1]))
                        markDstFloat(ins.result);
                }
                // CALL/LIB_CALL to a float-returning function → mark result as float. This must use
                // the SAME authority (acCallReturnsFloat / type.hpp) the codegen uses — checking only
                // user `floatFuncs_` missed ilib floats like math.mod, so `total += math.mod(...)`
                // never promoted `total` and summed a double's raw bits into an int.
                if ((ins.opcode == AC_IR::IROpcode::CALL || ins.opcode == AC_IR::IROpcode::LIB_CALL)
                        && ins.result.isValid() && !ins.typedOperands.empty()) {
                    std::string callee = funcName(ins.typedOperands[0]);
                    if ((floatFuncs_ && floatFuncs_->count(callee)) || isFloatReturningCall(callee))
                        markDstFloat(ins.result);
                }
            }
        }
    }

    // After compileFn, check if this function's return value is float and record it.
    void recordFloatReturn(const std::vector<AC_IR::IRInstruction>& instrs, const std::string& fname) {
        if (!floatFuncs_) return;
        for (const auto& ins : instrs) {
            if (ins.opcode == AC_IR::IROpcode::RETURN && !ins.typedOperands.empty()) {
                if (isFloatRef(ins.typedOperands[0])) { floatFuncs_->insert(fname); return; }
            }
        }
    }

    // Pre-scan (fixpoint): mark string-typed vars/temps BEFORE codegen so PRINT/RETURN/ADD in any
    // order (incl. loops) see them: string-const stores, concat results, string-returning calls.
    void preScanStrings(const std::vector<AC_IR::IRInstruction>& instrs) {
        bool changed = true;
        while (changed) {
            changed = false;
            auto mark = [&](const AC_IR::IRRef& r) {
                size_t v = stringVarNames_.size(), t = stringTempIds_.size();
                markDstString(r);
                if (stringVarNames_.size() != v || stringTempIds_.size() != t) changed = true;
            };
            for (const auto& ins : instrs) {
                using OP = AC_IR::IROpcode;
                if ((ins.opcode == OP::STORE_VAR || ins.opcode == OP::LOAD_CONST)
                    && ins.result.isValid() && !ins.typedOperands.empty()) {
                    if (isStrRef(ins.typedOperands[0])) mark(ins.result);
                } else if (ins.opcode == OP::STORE_VAR && ins.typedOperands.size() >= 2
                           && ins.typedOperands[0].isValid()) {
                    if (isStrRef(ins.typedOperands[1])) mark(ins.typedOperands[0]);
                } else if (ins.opcode == OP::ADD && ins.result.isValid() && ins.typedOperands.size() >= 2) {
                    if (isStrRef(ins.typedOperands[0]) || isStrRef(ins.typedOperands[1])) mark(ins.result);
                } else if (ins.opcode == OP::LOAD_INDEX && ins.result.isValid()
                           && !ins.typedOperands.empty() && isStrRef(ins.typedOperands[0])) {
                    // s[i] yields a 1-char string — EXCEPT the "__len__" pseudo-index (an int)
                    bool isLenIdx = ins.typedOperands.size() >= 2
                        && ins.typedOperands[1].kind == AC_IR::IRRef::Kind::CONST
                        && ins.typedOperands[1].value.type == AC_IR::IRType::STRING
                        && std::get<std::string>(ins.typedOperands[1].value.data) == "__len__";
                    if (!isLenIdx) mark(ins.result);
                } else if (ins.opcode == OP::LOAD_VAR && ins.result.isValid()
                           && !ins.typedOperands.empty() && isStrRef(ins.typedOperands[0])) {
                    mark(ins.result);   // copying a string pointer keeps it a string
                } else if (ins.opcode == OP::LOAD_INDEX && ins.result.isValid()
                           && ins.typedOperands.size() >= 2 && isDictRef(ins.typedOperands[0])
                           && ins.typedOperands[1].kind == AC_IR::IRRef::Kind::CONST
                           && ins.typedOperands[1].value.type == AC_IR::IRType::STRING) {
                    // dict[$key$] where $key$ is a statically-known string-valued field —
                    // see dictStrKeysByVar_'s comment (dicts are heterogeneous at runtime, so
                    // this is the only place codegen can know a given read is string-typed).
                    std::string key = std::get<std::string>(ins.typedOperands[1].value.data);
                    if (key.size() >= 2 && key.front()=='$' && key.back()=='$') key = key.substr(1, key.size()-2);
                    auto keys = dictStrKeysFor(ins.typedOperands[0]);
                    if (keys.count(key)) mark(ins.result);
                } else if (ins.opcode == OP::CALL && ins.result.isValid() && !ins.typedOperands.empty()) {
                    std::string callee = funcName(ins.typedOperands[0]);
                    if ((stringFuncs_ && stringFuncs_->count(callee)) || returnsCString(callee))
                        mark(ins.result);
                }
                // ── array kind (list-print dispatch) ──
                auto markA = [&](const AC_IR::IRRef& r) {
                    size_t v = arrayVarNames_.size(), t = arrayTempIds_.size();
                    markDstArray(r);
                    if (arrayVarNames_.size() != v || arrayTempIds_.size() != t) changed = true;
                };
                if (ins.opcode == OP::ALLOC && ins.result.isValid() && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == AC_IR::IRRef::Kind::CONST
                    && ins.typedOperands[0].value.type == AC_IR::IRType::STRING
                    && (std::get<std::string>(ins.typedOperands[0].value.data) == "list"
                     || std::get<std::string>(ins.typedOperands[0].value.data) == "range"
                     || std::get<std::string>(ins.typedOperands[0].value.data) == "sequence")) {
                    // `range N`/`iota N` (desugars to "range") and `sequence(a,b)`/`xrange`/
                    // `xiota`/`stream` (desugar to "sequence") are, on BNY, materialized as REAL
                    // arrays with the exact same [cap][len][elems] layout "list" uses (see the
                    // new ALLOC case below) — they need the same array-print/length/index
                    // dispatch as "list", so they must be recognized here too.
                    markA(ins.result);
                } else if (ins.opcode == OP::STORE_VAR && ins.result.isValid()
                           && !ins.typedOperands.empty() && isArrRef(ins.typedOperands[0])) {
                    markA(ins.result);
                } else if (ins.opcode == OP::LOAD_VAR && ins.result.isValid()
                           && !ins.typedOperands.empty() && isArrRef(ins.typedOperands[0])) {
                    markA(ins.result);
                }
                // dict propagation: ALLOC "dict" result + copies keep dict-ness
                if (ins.opcode == OP::ALLOC && ins.result.isValid() && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == AC_IR::IRRef::Kind::CONST
                    && ins.typedOperands[0].value.type == AC_IR::IRType::STRING
                    && std::get<std::string>(ins.typedOperands[0].value.data) == "dict") {
                    size_t v = dictVarNames_.size(), t = dictTempIds_.size();
                    markDstDict(ins.result);
                    if (dictVarNames_.size() != v || dictTempIds_.size() != t) changed = true;
                    // Record which keys carry a `$..$` string literal — see dictStrKeysByVar_.
                    std::string content = ins.typedOperands.size() >= 2
                        && ins.typedOperands[1].kind == AC_IR::IRRef::Kind::CONST
                        && ins.typedOperands[1].value.type == AC_IR::IRType::STRING
                        ? std::get<std::string>(ins.typedOperands[1].value.data) : "";
                    std::set<std::string> strKeys; std::string rest = content;
                    while (!rest.empty()) {
                        auto comma = rest.find(',');
                        std::string pair = comma == std::string::npos ? rest : rest.substr(0, comma);
                        rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
                        auto colon = pair.find(':');
                        if (colon == std::string::npos) continue;
                        std::string k = pair.substr(0, colon), v2 = pair.substr(colon + 1);
                        auto trim = [](std::string& x){ size_t a=x.find_first_not_of(' '), b=x.find_last_not_of(' ');
                            x = (a==std::string::npos) ? "" : x.substr(a, b-a+1); };
                        trim(k); trim(v2);
                        if (k.size() >= 2 && k.front()=='$' && k.back()=='$') k = k.substr(1, k.size()-2);
                        if (v2.size() >= 2 && v2.front()=='$' && v2.back()=='$') strKeys.insert(k);
                    }
                    if (mergeDictStrKeysChanged(ins.result, strKeys)) changed = true;
                } else if ((ins.opcode == OP::STORE_VAR || ins.opcode == OP::LOAD_VAR)
                           && ins.result.isValid() && !ins.typedOperands.empty()
                           && isDictRef(ins.typedOperands[0])) {
                    size_t v = dictVarNames_.size(), t = dictTempIds_.size();
                    markDstDict(ins.result);
                    if (dictVarNames_.size() != v || dictTempIds_.size() != t) changed = true;
                    if (mergeDictStrKeysChanged(ins.result, dictStrKeysFor(ins.typedOperands[0]))) changed = true;
                } else if (ins.opcode == OP::CALL && ins.result.isValid() && !ins.typedOperands.empty()
                           && arrayFuncs_ && arrayFuncs_->count(funcName(ins.typedOperands[0]))) {
                    markA(ins.result);
                }
                // list-of-dicts propagation: an ALLOC "list" whose every element token names an
                // already-known dict var (datac multi-row import: pets = [dc_pets_0, dc_pets_1])
                // is itself a list of dict-block pointers; LOAD_INDEX out of it yields a dict.
                if (ins.opcode == OP::ALLOC && ins.result.isValid() && ins.typedOperands.size() >= 2
                    && ins.typedOperands[0].kind == AC_IR::IRRef::Kind::CONST
                    && ins.typedOperands[0].value.type == AC_IR::IRType::STRING
                    && std::get<std::string>(ins.typedOperands[0].value.data) == "list"
                    && ins.typedOperands[1].kind == AC_IR::IRRef::Kind::CONST
                    && ins.typedOperands[1].value.type == AC_IR::IRType::STRING) {
                    const std::string& s = std::get<std::string>(ins.typedOperands[1].value.data);
                    std::vector<std::string> toks; size_t i = 0;
                    while (i < s.size()) {
                        size_t j = s.find(',', i);
                        std::string tok = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
                        size_t a = tok.find_first_not_of(" \t"), b = tok.find_last_not_of(" \t");
                        toks.push_back(a != std::string::npos ? tok.substr(a, b - a + 1) : "");
                        if (j == std::string::npos) break;
                        i = j + 1;
                    }
                    bool allDicts = !toks.empty();
                    for (auto& t : toks) if (!dictVarNames_.count(t)) { allDicts = false; break; }
                    if (allDicts) {
                        size_t v = listOfDictVarNames_.size(), t2 = listOfDictTempIds_.size();
                        markDstListOfDict(ins.result);
                        if (listOfDictVarNames_.size() != v || listOfDictTempIds_.size() != t2) changed = true;
                        // Every row shares the same schema — union each element dict's str-keys
                        // onto the LIST var itself, so a later `pets[1]` read can look them up
                        // via dictStrKeysFor(list-ref) same as any plain dict ref.
                        std::set<std::string> rowKeys;
                        for (auto& t : toks) {
                            auto it = dictStrKeysByVar_.find(t);
                            if (it != dictStrKeysByVar_.end()) rowKeys.insert(it->second.begin(), it->second.end());
                        }
                        if (mergeDictStrKeysChanged(ins.result, rowKeys)) changed = true;
                    }
                } else if (ins.opcode == OP::LOAD_INDEX && ins.result.isValid()
                           && !ins.typedOperands.empty() && isListOfDictRef(ins.typedOperands[0])) {
                    size_t v = dictVarNames_.size(), t = dictTempIds_.size();
                    markDstDict(ins.result);
                    if (dictVarNames_.size() != v || dictTempIds_.size() != t) changed = true;
                    if (mergeDictStrKeysChanged(ins.result, dictStrKeysFor(ins.typedOperands[0]))) changed = true;
                } else if ((ins.opcode == OP::STORE_VAR || ins.opcode == OP::LOAD_VAR)
                           && ins.result.isValid() && !ins.typedOperands.empty()
                           && isListOfDictRef(ins.typedOperands[0])) {
                    size_t v = listOfDictVarNames_.size(), t = listOfDictTempIds_.size();
                    markDstListOfDict(ins.result);
                    if (listOfDictVarNames_.size() != v || listOfDictTempIds_.size() != t) changed = true;
                }
            }
        }
    }

    // Record functions whose RETURN value is a string (so call sites type their results).
    void recordStringReturn(const std::vector<AC_IR::IRInstruction>& instrs, const std::string& fname) {
        if (!stringFuncs_) return;
        for (const auto& ins : instrs)
            if (ins.opcode == AC_IR::IROpcode::RETURN && !ins.typedOperands.empty()
                && isStrRef(ins.typedOperands[0])) { stringFuncs_->insert(fname); return; }
    }

    // Record functions whose RETURN value is a list block (so `Term.display f(x)` list-prints).
    void recordArrayReturn(const std::vector<AC_IR::IRInstruction>& instrs, const std::string& fname) {
        if (!arrayFuncs_) return;
        for (const auto& ins : instrs)
            if (ins.opcode == AC_IR::IROpcode::RETURN && !ins.typedOperands.empty()
                && isArrRef(ins.typedOperands[0])) { arrayFuncs_->insert(fname); return; }
    }

    void compileFn(const AC_IR::IRFunction& fn) {
        // Bundle methods need a class-qualified label — was just the bare method name (`greet`,
        // `init`), a collision risk between classes AND a naming mismatch with what any call
        // site actually needs (`Critter_greet`) — see the field-order pre-scan's comment.
        std::string label = fn.classOwner.empty() ? fn.name : fn.classOwner + "_" + fn.name;
        currentClass_ = fn.classOwner;
        labelPrefix_ = "__fn_" + label + "_";
        buildLocalVarIds(fn.instructions);
        // A bundle/tuple-instance parameter referenced ONLY via dotted field access
        // ("p.x"/"p.y", each its own separate symbol) never appears as a bare "p" VAR anywhere
        // in fn.instructions — buildLocalVarIds' scan (just above) can't find it, so
        // loadNamedVar("p", ...) (the field-access base-pointer load resolveFieldAccess routes
        // through) silently fell through to its "shouldn't happen" `mov $0, reg` fallback,
        // making every `p.field` read dereference a null pointer instead of the real struct
        // (verified real segfault without this — the frame-slot side of this exact gap is
        // fixed separately, just above the prologue's parameter-storing loop; this is the
        // read-side twin of that same fix).
        for (auto& pname : fn.parameters) {
            if (localVarIds_.count(pname) || !instanceClass_ || !instanceClass_->count(pname)) continue;
            int sid = prog.symbols.lookupAnyScope(pname);
            if (sid >= 0) localVarIds_.emplace(pname, sid);
        }
        // Pass 0: pre-scan to identify float-typed variables (needed for correct loop codegen)
        preScanFloats(fn.instructions);
        for (const auto& p : forcedStringParams_) stringVarNames_.insert(p);
        for (const auto& p : forcedFloatParams_)  floatVarNames_.insert(p);
        // Seed cross-method string-field knowledge BEFORE preScanStrings runs its fixpoint (see
        // classStringFields_'s comment) — a field this class assigns a string to in ANY method
        // (commonly `init`) must print as a string in every OTHER method too (`self.field`), not
        // just the one that assigns it. Must happen before the scan below, not after: a `Term.
        // display self.name` actually reads through a TEMP (`LOAD_VAR tX, self.name; PRINT tX`),
        // and preScanStrings only propagates "self.name is a string" onto tX via ITS OWN
        // fixpoint if "self.name" was already a known string going in — seeding stringVarNames_
        // after the scan already ran left the temp permanently unmarked (verified: printed the
        // raw pointer as a decimal integer, "4202497", not "unnamed").
        if (!fn.classOwner.empty() && classStringFields_ && classStringFields_->count(fn.classOwner))
            for (auto& f : classStringFields_->at(fn.classOwner)) stringVarNames_.insert("self." + f);
        preScanStrings(fn.instructions);
        recordFloatReturn(fn.instructions, fn.name);
        recordStringReturn(fn.instructions, fn.name);
        recordArrayReturn(fn.instructions, fn.name);
        // Pass 1: register allocation (must know N callee-saves before frame layout)
        regAlloc.run(fn.instructions);
        calleeSaves.clear();
        for (int i = 0; i < LinearScanAlloc::POOL_SIZE; i++)
            if (regAlloc.usedCalleeSaved.count(LinearScanAlloc::POOL[i]))
                calleeSaves.push_back(LinearScanAlloc::POOL[i]);
        int nSave = (int)calleeSaves.size();

        // Pass 2: frame layout — skip slots 0..nSave-1 (used by callee-save pushes)
        frame.setCalleeSaveBase(nSave);
        // A method's `self` parameter is NEVER a bare "self" VAR anywhere in the IR — it only
        // ever appears fused into compound names like "self.hp" (a totally separate symbol from
        // "self" itself) — so the parameter-storing loop below, which finds a param's slot by
        // scanning for a VAR whose NAME matches the param string, can never find "self" and
        // silently never stores its incoming pointer ANYWHERE (verified crash: `self.name = x`
        // read a slot containing raw zero, `mov (%r11)` on a null pointer). Pre-reserve a real
        // frame slot under a synthetic ID BEFORE scanInstrs so it's correctly counted in fsize
        // (reserving it AFTER `sub rsp` was already sized would silently write past the frame).
        if (!fn.classOwner.empty()) frame.varOffset(kSelfSymId_);
        // Same "self" bug, same fix, for a bundle/tuple-typed FREE-FUNCTION parameter (see the
        // comment just above): referenced in the body only via "p.x"/"p.y" (separate symbols),
        // never as a bare "p" VAR, so it needs the identical pre-scanInstrs reservation or its
        // incoming pointer has nowhere to land (verified real segfault without this).
        for (auto& pname : fn.parameters) {
            if (!instanceClass_ || !instanceClass_->count(pname)) continue;
            int sid = prog.symbols.lookupAnyScope(pname);
            if (sid >= 0) frame.varOffset(sid);
        }
        frame.scanInstrs(fn.instructions);
        fsize = frame.frameSize();

        // Stack alignment: after push_rbp (rsp≡0) + nSave callee-save pushes,
        // rsp ≡ 8*(nSave%2). For rsp≡0 before calls: fsize must absorb the remainder.
        if (nSave % 2 != 0) fsize += 8;

        // Emit prologue
        em.label(label);
        em.push_rbp();
        em.mov_rbp_rsp();
        for (R r : calleeSaves) em.push_r(r);
        em.sub_rsp_i32(fsize);

        // Save incoming parameters from arg registers to their VAR stack slots
        for (int i = 0; i < (int)fn.parameters.size() && i < (int)abi.argRegs.size(); i++) {
            const std::string& pname = fn.parameters[i];
            int symId = -1;
            for (auto& ins : fn.instructions) {
                auto checkRef = [&](const AC_IR::IRRef& r) {
                    if (r.kind == AC_IR::IRRef::Kind::VAR && r.id >= 0 &&
                        prog.symbols.getName(r.id) == pname)
                        symId = r.id;
                };
                checkRef(ins.result);
                for (auto& op : ins.typedOperands) checkRef(op);
                if (symId >= 0) break;
            }
            // A param proven (instanceClass_, see the classParamTypes discovery above) to hold
            // a bundle/tuple instance is referenced in the body ONLY via dotted field access
            // ("p.x"/"p.y", each its own separate symbol) — the plain bare name "p" the scan
            // above looks for never appears at all, so symId stayed -1 and the incoming pointer
            // was silently dropped (verified real crash: `show(t): Term.display p.x` segfaulted
            // — resolveFieldAccess correctly found "p" is a Point, but the slot it dereferenced
            // was never written). ir.cpp's FuncDef case unconditionally interns every parameter
            // name regardless of body usage (`prog.symbols.intern(p)`), so a plain name lookup
            // always finds the real symbol id here — just needs to be tried explicitly.
            if (symId < 0 && instanceClass_ && instanceClass_->count(pname)) symId = prog.symbols.lookupAnyScope(pname);
            if (symId >= 0)
                em.mov_rbp_r(frame.varOffset(symId), abi.argRegs[i]);
            else if (i == 0 && !fn.classOwner.empty() && pname == "self")
                em.mov_rbp_r(frame.varOffset(kSelfSymId_), abi.argRegs[i]);
        }

        // Emit instructions
        for (auto& ins : fn.instructions) {
            if (ins.opcode == AC_IR::IROpcode::FUNC_BEGIN) continue;
            if (ins.opcode == AC_IR::IROpcode::FUNC_END) continue;
            compileInstr(ins);
        }

        // Implicit void return
        em.mov_ri32(R::RAX, 0);
        emitEpilogue();
    }

    // A generator function body — compiled as a real fiber (see emitFiberSwap's comment for the
    // full save/restore design), NOT via BNY's normal call/ret convention: nothing ever `call`s
    // this label at all. GEN_CREATE just arms a fresh state block/stack pointing at it; GEN_NEXT
    // jumps straight in with RSP already pointing at that fresh stack.
    //
    // This mirrors compileFn's own structure closely (same register allocation, same frame
    // layout, same "push callee-saves then reserve locals" prologue shape — a generator's own
    // temps/locals get exactly the same treatment as any other function's, including landing in
    // the SAME POOL registers emitFiberSwap already saves/restores) and diverges at exactly two
    // points: where parameters come from (the state block's args area, via the shared
    // `__ac_gen_cur` slot — there's no real incoming `call` to read abi.argRegs from), and what
    // "returning" means (emitGenReturnSwap's mark-done-and-swap-back, not a normal `ret` — see
    // RETURN's own curFnIsGenerator_ branch in compileInstr).
    //
    // Bundle-method generators are an explicit scope cut for this pass, matching every other
    // backend (`self` has nowhere to come from without a real incoming call either).
    void compileGeneratorFn(const AC_IR::IRFunction& fn) {
        std::string label = fn.name;
        currentClass_.clear();
        curFnIsGenerator_ = true;
        labelPrefix_ = "__fn_" + label + "_";
        buildLocalVarIds(fn.instructions);
        preScanFloats(fn.instructions);
        for (const auto& p : forcedStringParams_) stringVarNames_.insert(p);
        for (const auto& p : forcedFloatParams_)  floatVarNames_.insert(p);
        preScanStrings(fn.instructions);
        recordFloatReturn(fn.instructions, fn.name);
        recordStringReturn(fn.instructions, fn.name);
        recordArrayReturn(fn.instructions, fn.name);

        regAlloc.run(fn.instructions);
        calleeSaves.clear();
        for (int i = 0; i < LinearScanAlloc::POOL_SIZE; i++)
            if (regAlloc.usedCalleeSaved.count(LinearScanAlloc::POOL[i]))
                calleeSaves.push_back(LinearScanAlloc::POOL[i]);
        int nSave = (int)calleeSaves.size();

        frame.setCalleeSaveBase(nSave);
        frame.scanInstrs(fn.instructions);
        fsize = frame.frameSize();
        if (nSave % 2 != 0) fsize += 8;

        em.label(label);
        em.push_rbp();
        em.mov_rbp_rsp();
        for (R r : calleeSaves) em.push_r(r);
        em.sub_rsp_i32(fsize);

        // Parameters: copied from the state block's args area (GEN_CREATE laid them out at
        // GEN_HEADER_BYTES, GEN_HEADER_BYTES+8, ...), not from abi.argRegs — same param→symbol
        // lookup compileFn uses, just a different source register.
        loadGenCur(R::RAX);
        for (int i = 0; i < (int)fn.parameters.size(); i++) {
            const std::string& pname = fn.parameters[i];
            int symId = -1;
            for (auto& ins : fn.instructions) {
                auto checkRef = [&](const AC_IR::IRRef& r) {
                    if (r.kind == AC_IR::IRRef::Kind::VAR && r.id >= 0 &&
                        prog.symbols.getName(r.id) == pname)
                        symId = r.id;
                };
                checkRef(ins.result);
                for (auto& op : ins.typedOperands) checkRef(op);
                if (symId >= 0) break;
            }
            if (symId < 0) continue;
            em.mov_r_based(R::RCX, R::RAX, GEN_HEADER_BYTES + 8 * i);
            em.mov_rbp_r(frame.varOffset(symId), R::RCX);
        }

        for (auto& ins : fn.instructions) {
            if (ins.opcode == AC_IR::IROpcode::FUNC_BEGIN) continue;
            if (ins.opcode == AC_IR::IROpcode::FUNC_END) continue;
            compileInstr(ins);
        }

        // Implicit trailing return — same "mark done, swap back" as an explicit `return` inside
        // a generator; see RETURN's curFnIsGenerator_ branch. Never a normal emitEpilogue(): this
        // body was never `call`d, so there's no return address on this stack to `ret` to.
        emitGenReturnSwap();
        curFnIsGenerator_ = false;
    }

    void compileGlobal(const std::vector<AC_IR::IRInstruction>& globalInit) {
        labelPrefix_ = "__start_";
        buildLocalVarIds(globalInit);
        preScanFloats(globalInit);
        preScanStrings(globalInit);
        regAlloc.run(globalInit);
        calleeSaves.clear();
        for (int i = 0; i < LinearScanAlloc::POOL_SIZE; i++)
            if (regAlloc.usedCalleeSaved.count(LinearScanAlloc::POOL[i]))
                calleeSaves.push_back(LinearScanAlloc::POOL[i]);
        int nSave = (int)calleeSaves.size();

        frame.setCalleeSaveBase(nSave);
        frame.scanInstrs(globalInit);
        fsize = frame.frameSize();

        // _start alignment: OS enters with rsp ≡ 0 (no return address pushed).
        // push_rbp → rsp ≡ 8. After N callee-save pushes: rsp ≡ 8 - 8N (mod 16).
        //   N even → rsp ≡ 8 → need fsize ≡ 8 → add 8.
        //   N odd  → rsp ≡ 0 → fsize ≡ 0 already correct.
        if (nSave % 2 == 0) fsize += 8;

        em.label("_start");
        em.push_rbp();
        em.mov_rbp_rsp();
        for (R r : calleeSaves) em.push_r(r);
        em.sub_rsp_i32(fsize);

        // `use ilib widgets` needs a one-time `ac_widgets_init()` (== gtk_init()) before ANY
        // widget constructor runs — every other backend's FFI shim already does this
        // automatically as part of its own module/class init (verified: the Python shim calls
        // `_lib.ac_widgets_init()` unconditionally at load time), but nothing here ever emitted
        // the equivalent call. Without it GTK's internal display/style-context state was never
        // set up, so the FIRST real GTK call (inside `Screen()`) crashed with "Can't create a
        // GtkStyleContext without a display connection" — this had nothing to do with sandbox/
        // display availability at all (confirmed: a plain hand-written `gtk_init()` C program
        // opens a real window fine in this same environment); every "reaches the same GTK
        // crash point" verification this session was actually hitting THIS bug, not an
        // environment limit.
        if (prog.importedLibs.count("widgets")) em.call("ac_widgets_init");

        for (auto& ins : globalInit) compileInstr(ins);

        emitHalt();
    }
};

// ─── Array Heap Allocator (Linux) ─────────────────────────────────────────────
// Bump allocator over a single lazily-mmap'd 16 MB region. The bump cursor lives in a
// global slot (__heap_cursor). No free — adequate for AC array/list programs.
//   __ac_alloc__(rdi = bytes) -> rax = pointer
static void emitAllocLinux(X64Emitter& em, int cursorSlot) {
    em.label("__ac_alloc__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.mov_ri64_gvar(R::RCX, cursorSlot);   // rcx = &cursor
    em.mov_r_ptr(R::RAX, R::RCX);           // rax = *cursor
    em.test_rr(R::RAX, R::RAX);
    em.jne("__ac_alloc_have__");
    // First call: mmap(NULL, 16MB, PROT_READ|WRITE, MAP_PRIVATE|ANON, -1, 0)
    em.push_r(R::RDI);                       // save requested bytes
    em.push_r(R::RCX);                       // save &cursor (syscall clobbers rcx)
    em.mov_ri32(R::RDI, 0);
    em.mov_ri32(R::RSI, 0x1000000);          // 16 MB
    em.mov_ri32(R::RDX, 3);                  // PROT_READ|PROT_WRITE
    em.mov_ri32(R::R10, 0x22);               // MAP_PRIVATE|MAP_ANONYMOUS
    em.mov_ri32(R::R8,  -1);                 // fd
    em.mov_ri32(R::R9,  0);                  // offset
    em.mov_ri32(R::RAX, 9);                  // sys_mmap
    em.syscall();                            // rax = base
    em.pop_r(R::RCX);                        // &cursor
    em.pop_r(R::RDI);                        // bytes
    em.mov_ptr_r(R::RCX, R::RAX);            // *cursor = base
    em.label("__ac_alloc_have__");
    // rax = current cursor (result), rcx = &cursor, rdi = bytes
    em.mov_rr(R::RDX, R::RAX);               // rdx = result (old cursor)
    em.add_rr(R::RAX, R::RDI);               // rax = old + bytes (new cursor)
    em.mov_ptr_r(R::RCX, R::RAX);            // *cursor = new
    em.mov_rr(R::RAX, R::RDX);               // rax = result
    em.pop_rbp(); em.ret();
}

// `atomic` support: a real spinlock, zero libc/syscalls — fits BNY's own "own the CPU"
// design exactly (the same reason the allocator above is a raw mmap syscall, not malloc).
// XCHG with a memory operand carries an implicit x86 lock (Intel SDM Vol.2), so try-acquire
// is a single instruction + a compare; no `lock cmpxchg` or explicit LOCK prefix needed.
static void emitAtomicLockLinux(X64Emitter& em, int lockSlot) {
    em.label("__ac_atomic_lock__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.mov_ri64_gvar(R::RCX, lockSlot);      // rcx = &lock
    em.label("__ac_atomic_spin__");
    em.mov_ri32(R::RAX, 1);                  // rax = 1 (the "locked" marker)
    em.xchg_ptr_r(R::RCX, R::RAX);           // atomically: old=[rcx]; [rcx]=1; rax=old
    em.test_rr(R::RAX, R::RAX);              // old == 0 (was unlocked)?
    em.jnz("__ac_atomic_spin__");            // old != 0 → someone else holds it, retry
    em.pop_rbp(); em.ret();                  // acquired
}
static void emitAtomicUnlockLinux(X64Emitter& em, int lockSlot) {
    em.label("__ac_atomic_unlock__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.mov_ri64_gvar(R::RCX, lockSlot);
    em.mov_ri32(R::RAX, 0);
    em.mov_ptr_r(R::RCX, R::RAX);            // plain store — fine, we uniquely hold the lock here
    em.pop_rbp(); em.ret();
}

// __ac_append__(rdi = ptr, rsi = value) -> rax = ptr (SAME pointer whenever capacity
// allows — O(1) amortized; only allocates+copies once every doubling interval, matching
// the ALLOC "list" site's [cap][len][e0][e1]... layout: ptr[-8]=cap, ptr[0]=len,
// ptr[8+8i]=elem_i. Previously this allocated a fresh (len+2)*8-byte block and copied
// every existing element on EVERY append — O(n^2) total memory traffic for an N-append
// loop. Every array is created via the one ALLOC "list" site, which always seeds cap>=4,
// so cap is never 0 here.
static void emitAppendLinux(X64Emitter& em) {
    em.label("__ac_append__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14);
    em.mov_rr(R::RBX, R::RDI);                // rbx = ptr
    em.mov_rr(R::R12, R::RSI);                // r12 = value
    em.mov_r_ptr(R::R13, R::RBX);             // r13 = len = ptr[0]
    em.mov_rr(R::RAX, R::RBX); em.add_ri32(R::RAX, -8);
    em.mov_r_ptr(R::R14, R::RAX);             // r14 = cap = ptr[-8]
    em.cmp_rr(R::R13, R::R14);
    em.jl("__ac_append_fast__");              // len < cap → room already available

    // ---- slow path: capacity exhausted — double and copy, O(len) but amortized O(1) ----
    em.add_rr(R::R14, R::R14);                // r14 = newcap = cap*2
    em.mov_rr(R::RDI, R::R14); em.add_ri32(R::RDI, 2);
    em.mov_ri32(R::RDX, 8); em.imul_rr(R::RDI, R::RDX);   // bytes = (newcap+2)*8
    em.call("__ac_alloc__");                  // rax = new raw block
    em.mov_ptr_r(R::RAX, R::R14);              // new_raw[0] = newcap
    em.add_ri32(R::RAX, 8);                    // rax = new ptr (skip cap word)
    em.mov_rr(R::RCX, R::R13); em.mov_ptr_r(R::RAX, R::RCX); // new_ptr[0] = len (for now)
    // copy old_ptr[1..len] -> new_ptr[1..len]  (word index; word i holds element i-1)
    em.mov_ri32(R::RCX, 1);
    em.label("__ac_append_copy__");
    em.cmp_rr(R::RCX, R::R13);
    em.jg("__ac_append_copydone__");
    em.mov_rr(R::RSI, R::RCX); em.mov_ri32(R::RDX, 8); em.imul_rr(R::RSI, R::RDX);
    em.mov_rr(R::R8, R::RBX); em.add_rr(R::R8, R::RSI); em.mov_r_ptr(R::R9, R::R8); // r9 = old[i]
    em.mov_rr(R::R8, R::RAX); em.add_rr(R::R8, R::RSI); em.mov_ptr_r(R::R8, R::R9); // new[i] = r9
    em.inc_r(R::RCX);
    em.jmp("__ac_append_copy__");
    em.label("__ac_append_copydone__");
    // new_ptr[len+1] = value ; new_ptr[0] = len+1
    em.mov_rr(R::RCX, R::R13); em.inc_r(R::RCX); em.mov_ri32(R::RDX, 8); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::R8, R::RAX); em.add_rr(R::R8, R::RCX); em.mov_ptr_r(R::R8, R::R12);
    em.mov_rr(R::RCX, R::R13); em.inc_r(R::RCX);
    em.mov_ptr_r(R::RAX, R::RCX);
    em.jmp("__ac_append_done__");

    // ---- fast path: len < cap — write in place, O(1), zero allocation/copy ----
    em.label("__ac_append_fast__");
    em.mov_rr(R::RCX, R::R13); em.inc_r(R::RCX); em.mov_ri32(R::RDX, 8); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::R8, R::RBX); em.add_rr(R::R8, R::RCX); em.mov_ptr_r(R::R8, R::R12); // ptr[len+1]=value
    em.mov_rr(R::RCX, R::R13); em.inc_r(R::RCX);
    em.mov_ptr_r(R::RBX, R::RCX);              // ptr[0] = len+1
    em.mov_rr(R::RAX, R::RBX);                 // return same ptr — nothing moved

    em.label("__ac_append_done__");
    em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_itoa__(rdi = signed int64) -> rax = fresh NUL-terminated decimal string (heap).
// Two-pass: count digits, then write them backwards into the buffer. Backs to_string(int) (#7).
static void emitItoaLinux(X64Emitter& em) {
    em.label("__ac_itoa__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14);
    em.mov_rr(R::R12, R::RDI);              // r12 = n
    em.mov_ri32(R::RDI, 24);
    em.call("__ac_alloc__");
    em.mov_rr(R::RBX, R::RAX);              // rbx = buffer base
    em.xor_rr(R::R14, R::R14);              // r14 = sign flag
    em.test_rr(R::R12, R::R12);
    em.jns("__itoa_pos__");
    em.mov_ri32(R::R14, 1);
    em.neg_r(R::R12);
    em.label("__itoa_pos__");
    // count digits (do-while so 0 → 1 digit): r13 = count
    em.mov_rr(R::RAX, R::R12);
    em.mov_ri32(R::R13, 0);
    em.label("__itoa_count__");
    em.inc_r(R::R13);
    em.xor_rr(R::RDX, R::RDX); em.mov_ri32(R::RCX, 10); em.div_rcx();
    em.test_rr(R::RAX, R::RAX);
    em.jnz("__itoa_count__");
    // end pointer = base + sign + count ; write NUL there
    em.mov_rr(R::RCX, R::R13); em.add_rr(R::RCX, R::R14);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX);
    em.mov_ri32(R::RCX, 0); em.mov_ptr_r8(R::RDI, R::RCX);   // [end] = NUL
    // sign char at base
    em.test_rr(R::R14, R::R14);
    em.je("__itoa_wr__");
    em.mov_ri32(R::RCX, '-'); em.mov_ptr_r8(R::RBX, R::RCX);
    em.label("__itoa_wr__");
    em.dec_r(R::RDI);                       // last digit slot
    em.mov_rr(R::RAX, R::R12);              // n (positive)
    em.label("__itoa_dig__");
    em.xor_rr(R::RDX, R::RDX); em.mov_ri32(R::RCX, 10); em.div_rcx();  // rax=q rdx=rem
    em.add_ri32(R::RDX, '0'); em.mov_ptr_r8(R::RDI, R::RDX);
    em.dec_r(R::RDI);
    em.test_rr(R::RAX, R::RAX);
    em.jnz("__itoa_dig__");
    em.mov_rr(R::RAX, R::RBX);
    em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_concat__(rdi = left char*, rsi = right char*) -> rax = fresh NUL-terminated char*
// Backs runtime string `+` on BNY (compile-time concat is folded). Uses __ac_strlen__ + __ac_alloc__.
static void emitConcatLinux(X64Emitter& em) {
    em.label("__ac_concat__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::R15);
    em.mov_rr(R::RBX, R::RDI);                 // left
    em.mov_rr(R::R12, R::RSI);                 // right
    em.call("__ac_strlen__");                  // rdi already = left
    em.mov_rr(R::R13, R::RAX);                 // llen
    em.mov_rr(R::RDI, R::R12);
    em.call("__ac_strlen__");
    em.mov_rr(R::R14, R::RAX);                 // rlen
    em.mov_rr(R::RDI, R::R13);
    em.add_rr(R::RDI, R::R14);
    em.inc_r(R::RDI);                          // llen + rlen + 1 (NUL)
    em.call("__ac_alloc__");                   // rax = dst
    em.mov_rr(R::R15, R::RAX);                 // save dst base
    em.mov_rr(R::RDI, R::R15);                 // write cursor
    em.label("__ac_ccat_l__");                 // copy left
    em.movzx_r64_ptr8(R::RCX, R::RBX);
    em.test_rr(R::RCX, R::RCX);
    em.je("__ac_ccat_ld__");
    em.mov_ptr_r8(R::RDI, R::RCX);
    em.inc_r(R::RBX); em.inc_r(R::RDI);
    em.jmp("__ac_ccat_l__");
    em.label("__ac_ccat_ld__");
    em.label("__ac_ccat_r__");                 // copy right
    em.movzx_r64_ptr8(R::RCX, R::R12);
    em.test_rr(R::RCX, R::RCX);
    em.je("__ac_ccat_rd__");
    em.mov_ptr_r8(R::RDI, R::RCX);
    em.inc_r(R::R12); em.inc_r(R::RDI);
    em.jmp("__ac_ccat_r__");
    em.label("__ac_ccat_rd__");
    em.mov_ri32(R::RCX, 0);                    // NUL terminator
    em.mov_ptr_r8(R::RDI, R::RCX);
    em.mov_rr(R::RAX, R::R15);                 // return dst
    em.pop_r(R::R15); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_streq__(rdi = a, rsi = b) -> rax = 1 if NUL-terminated strings match, else 0
static void emitStrEqLinux(X64Emitter& em) {
    em.label("__ac_streq__");
    em.label("__ac_streq_loop__");
    em.movzx_r64_ptr8(R::RAX, R::RDI);
    em.movzx_r64_ptr8(R::RCX, R::RSI);
    em.cmp_rr(R::RAX, R::RCX);
    em.jne("__ac_streq_no__");
    em.test_rr(R::RAX, R::RAX);          // matched NULs → equal
    em.je("__ac_streq_yes__");
    em.inc_r(R::RDI); em.inc_r(R::RSI);
    em.jmp("__ac_streq_loop__");
    em.label("__ac_streq_yes__");
    em.mov_ri32(R::RAX, 1);
    em.ret();
    em.label("__ac_streq_no__");
    em.mov_ri32(R::RAX, 0);
    em.ret();
}

// Event-listener bind/trigger table — same fixed 64-slot parallel-array design
// AsmStrategy/CStrategy already use (see their emitEventBind/emitEventTrigger +
// _ac_bind/_ac_trigger comments); ported here because BNY's opcode switch never had a case
// for EVENT_BIND/EVENT_TRIGGER at all (silent no-op — the one backend this session's
// event-listener fix left out). The two 64-entry arrays (keys, fns — 512 bytes each) are
// lazily __ac_alloc__'d on first bind rather than living in a fixed .bss-style region: BNY's
// gvar slots are individually-addressed 8-byte cells with no guaranteed contiguous layout
// (the static-link path lays them out in NAME-SORTED order), so an "array" here has to be a
// single heap block referenced by a pointer slot, same pattern as __save_buf_ptr/
// __try_stack_ptr right above.
//
// __ac_bind__(rdi = key ptr, rsi = fn ptr) — appends (key, fn) at index __ev_n, n++.
static void emitEventBindLinux(X64Emitter& em, int keysPtrSlot, int fnsPtrSlot, int nSlot) {
    em.label("__ac_bind__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::R15);
    em.mov_rr(R::R12, R::RDI);   // r12 = key ptr
    em.mov_rr(R::R13, R::RSI);   // r13 = fn ptr

    em.mov_ri64_gvar(R::RCX, keysPtrSlot);
    em.mov_r_ptr(R::R14, R::RCX);         // r14 = keys_ptr
    em.test_rr(R::R14, R::R14);
    em.jne("__acb_havekeys__");
    em.push_r(R::RCX); em.push_r(R::R12); em.push_r(R::R13);
    em.mov_ri32(R::RDI, 512);
    em.call("__ac_alloc__");
    em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RCX);
    em.mov_ptr_r(R::RCX, R::RAX);
    em.mov_rr(R::R14, R::RAX);
    em.label("__acb_havekeys__");

    em.mov_ri64_gvar(R::RCX, fnsPtrSlot);
    em.mov_r_ptr(R::R15, R::RCX);         // r15 = fns_ptr
    em.test_rr(R::R15, R::R15);
    em.jne("__acb_havefns__");
    em.push_r(R::RCX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14);
    em.mov_ri32(R::RDI, 512);
    em.call("__ac_alloc__");
    em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RCX);
    em.mov_ptr_r(R::RCX, R::RAX);
    em.mov_rr(R::R15, R::RAX);
    em.label("__acb_havefns__");

    em.mov_ri64_gvar(R::RBX, nSlot);
    em.mov_r_ptr(R::RAX, R::RBX);         // rax = n
    em.mov_rr(R::RCX, R::RAX);
    em.shl_r_i8(R::RCX, 3);
    em.add_rr(R::RCX, R::R14);
    em.mov_ptr_r(R::RCX, R::R12);         // keys[n] = key
    em.mov_rr(R::RCX, R::RAX);
    em.shl_r_i8(R::RCX, 3);
    em.add_rr(R::RCX, R::R15);
    em.mov_ptr_r(R::RCX, R::R13);         // fns[n] = fn
    em.inc_r(R::RAX);
    em.mov_ptr_r(R::RBX, R::RAX);         // n++

    em.pop_r(R::R15); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_trigger__(rdi = key ptr) — linear __ac_streq__ scan; calls the first matching fn, 0-arg
// (every EVENT_BIND callback is synthesized/referenced as a 0-arg function — see ir.cpp's
// KeyBinding/BindStmt lowering, same convention CStrategy's `_ac_trigger` assumes).
static void emitEventTriggerLinux(X64Emitter& em, int keysPtrSlot, int fnsPtrSlot, int nSlot) {
    em.label("__ac_trigger__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::R15);
    em.mov_rr(R::R12, R::RDI);            // r12 = key ptr to match

    em.mov_ri64_gvar(R::RCX, keysPtrSlot);
    em.mov_r_ptr(R::R13, R::RCX);         // r13 = keys_ptr
    em.mov_ri64_gvar(R::RCX, fnsPtrSlot);
    em.mov_r_ptr(R::R14, R::RCX);         // r14 = fns_ptr
    em.mov_ri64_gvar(R::RCX, nSlot);
    em.mov_r_ptr(R::R15, R::RCX);         // r15 = n

    em.xor_rr(R::RBX, R::RBX);            // i = 0
    em.label("__act_loop__");
    em.cmp_rr(R::RBX, R::R15);
    em.jge("__act_done__");
    em.mov_rr(R::RAX, R::RBX);
    em.shl_r_i8(R::RAX, 3);
    em.add_rr(R::RAX, R::R13);
    em.mov_r_ptr(R::RAX, R::RAX);         // rax = keys[i]
    em.mov_rr(R::RDI, R::RAX);
    em.mov_rr(R::RSI, R::R12);
    em.call("__ac_streq__");
    em.test_rr(R::RAX, R::RAX);
    em.je("__act_next__");
    em.mov_rr(R::RAX, R::RBX);
    em.shl_r_i8(R::RAX, 3);
    em.add_rr(R::RAX, R::R14);
    em.mov_r_ptr(R::RAX, R::RAX);         // rax = fns[i]
    em.call_r(R::RAX);
    em.jmp("__act_done__");
    em.label("__act_next__");
    em.inc_r(R::RBX);
    em.jmp("__act_loop__");
    em.label("__act_done__");

    em.pop_r(R::R15); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// widgets ilib callback trampolines (see widgetVarKind_'s comment for the ctor/method dispatch
// these back — `btn(root, text, OnClick)` / `.on_click(OnClick)`). GTK's C callback signature is
// `void (*)(void*)`; the AC user function being bridged to is either 0-arg or 1-arg (arity from
// userFuncArity()). Mirrors CStrategy's `_ac_widget_call0`/`_ac_widget_call1`
// (`((ac_int(*)(void))fn)()` / `((ac_int(*)(ac_int))fn)(0)`) as raw machine code.
static void emitWidgetTrampolinesLinux(X64Emitter& em) {
    em.label("_ac_widget_call0");        // rdi = fn ptr
    em.mov_rr(R::R10, R::RDI);
    em.call_r(R::R10);
    em.ret();
    em.label("_ac_widget_call1");        // rdi = fn ptr
    em.mov_rr(R::R10, R::RDI);
    em.mov_ri32(R::RDI, 0);
    em.call_r(R::R10);
    em.ret();
}

// __ac_dict_get__(rdi = block [n][k0][v0]…, rsi = key) -> rax = value; missing key → KeyError.
static void emitDictLinux(X64Emitter& em, StringPool& sp) {
    em.label("__ac_dict_get__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13);
    em.mov_rr(R::RBX, R::RDI);                  // block
    em.mov_rr(R::R12, R::RSI);                  // key
    em.mov_ri32(R::R13, 0);                     // i
    em.label("__ac_dg_loop__");
    em.mov_r_ptr(R::RAX, R::RBX);               // n
    em.cmp_rr(R::R13, R::RAX);
    em.jge("__ac_dg_miss__");
    em.mov_rr(R::RCX, R::R13);
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX); em.add_ri32(R::RDI, 8);
    em.mov_r_ptr(R::RDI, R::RDI);               // key_i
    em.mov_rr(R::RSI, R::R12);
    em.call("__ac_streq__");
    em.test_rr(R::RAX, R::RAX);
    em.je("__ac_dg_next__");
    em.mov_rr(R::RCX, R::R13);                  // hit: value at block+16+16i
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RAX, R::RBX); em.add_rr(R::RAX, R::RCX); em.add_ri32(R::RAX, 16);
    em.mov_r_ptr(R::RAX, R::RAX);
    em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
    em.label("__ac_dg_next__");
    em.inc_r(R::R13);
    em.jmp("__ac_dg_loop__");
    em.label("__ac_dg_miss__");
    {
        std::string msg = "Preposterous: KeyError: key not found\n";
        int sid = sp.add(msg);
        em.mov_ri64_str(R::RSI, sid);
        em.mov_ri32(R::RDI, 2);
        em.mov_ri32(R::RDX, (int32_t)msg.size());
        em.mov_ri32(R::RAX, 1); em.syscall();
        em.mov_ri32(R::RAX, 231); em.mov_ri32(R::RDI, 1); em.syscall();
    }
}

// __ac_dict_set__(rdi = ptr, rsi = key, rdx = val) -> rax = ptr (same pointer whenever
// capacity allows — O(1) amortized on top of the existing O(n) linear-scan-for-match; only
// allocates+copies once every doubling interval, mirroring __ac_append__'s design against
// the [cap][n][k0][v0]... layout ptr[-8]=cap seeded by the ALLOC "dict" site. Previously
// this allocated a fresh (2n+3)-word block and copied every existing pair on EVERY insert
// past the first match-scan — O(n^2) total memory traffic for an N-insert loop.
static void emitDictSetLinux(X64Emitter& em) {
    em.label("__ac_dict_set__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14);
    em.mov_rr(R::RBX, R::RDI);                  // rbx = ptr
    em.mov_rr(R::R12, R::RSI);                  // r12 = key
    em.mov_rr(R::R13, R::RDX);                  // r13 = val
    em.mov_ri32(R::R14, 0);                     // r14 = i (scan index)
    em.label("__ac_ds_loop__");
    em.mov_r_ptr(R::RAX, R::RBX);                // n
    em.cmp_rr(R::R14, R::RAX);
    em.jge("__ac_ds_grow__");                     // r14 == n → no match found, need to insert
    em.mov_rr(R::RCX, R::R14);
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX); em.add_ri32(R::RDI, 8);
    em.mov_r_ptr(R::RDI, R::RDI);
    em.mov_rr(R::RSI, R::R12);
    em.call("__ac_streq__");
    em.test_rr(R::RAX, R::RAX);
    em.je("__ac_ds_next__");
    em.mov_rr(R::RCX, R::R14);                  // overwrite existing value
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX); em.add_ri32(R::RDI, 16);
    em.mov_ptr_r(R::RDI, R::R13);
    em.mov_rr(R::RAX, R::RBX);
    em.jmp("__ac_ds_done__");
    em.label("__ac_ds_next__");
    em.inc_r(R::R14);
    em.jmp("__ac_ds_loop__");

    em.label("__ac_ds_grow__");                  // r14 == n; decide fast (room) vs slow (full)
    em.mov_rr(R::RCX, R::RBX); em.add_ri32(R::RCX, -8);
    em.mov_r_ptr(R::RCX, R::RCX);                 // rcx = cap = ptr[-8]
    em.cmp_rr(R::R14, R::RCX);
    em.jl("__ac_ds_fast__");                       // n < cap → room already available

    // ---- slow path: n == cap exactly (grow only ever triggers here, never n>cap) — r14
    // doubles as oldcap for free, no separate read needed. Double capacity and copy. ----
    em.mov_rr(R::RDI, R::R14); em.add_rr(R::RDI, R::RDI); em.inc_r(R::RDI);
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RDI, R::RDX);   // bytes = 16*(2*oldcap + 1)
    em.call("__ac_alloc__");                                // rax = new raw block
    em.mov_rr(R::RCX, R::R14); em.add_rr(R::RCX, R::RCX);    // rcx = newcap = 2*oldcap
    em.mov_ptr_r(R::RAX, R::RCX);                             // new_raw[0] = newcap
    em.add_ri32(R::RAX, 8);                                    // rax = new ptr
    // copy old_ptr[0..2*oldcap] (word0=n plus 2*oldcap pair-words) -> new_ptr[same range];
    // word0 gets overwritten with n+1 right after.
    em.mov_ri32(R::RCX, 0);
    em.label("__ac_ds_copy__");
    em.mov_rr(R::RDX, R::R14); em.add_rr(R::RDX, R::RDX); em.inc_r(R::RDX); // bound = 2*oldcap+1
    em.cmp_rr(R::RCX, R::RDX);
    em.jge("__ac_ds_copied__");
    em.mov_rr(R::RSI, R::RCX); em.mov_ri32(R::RDI, 8); em.imul_rr(R::RSI, R::RDI);
    em.mov_rr(R::R8, R::RBX); em.add_rr(R::R8, R::RSI); em.mov_r_ptr(R::R9, R::R8);
    em.mov_rr(R::R8, R::RAX); em.add_rr(R::R8, R::RSI); em.mov_ptr_r(R::R8, R::R9);
    em.inc_r(R::RCX);
    em.jmp("__ac_ds_copy__");
    em.label("__ac_ds_copied__");
    em.mov_rr(R::RCX, R::R14); em.inc_r(R::RCX);
    em.mov_ptr_r(R::RAX, R::RCX);                    // new_ptr[0] = n+1 (= oldcap+1)
    em.mov_rr(R::RCX, R::R14);
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RAX); em.add_rr(R::RDI, R::RCX); em.add_ri32(R::RDI, 8);
    em.mov_ptr_r(R::RDI, R::R12);                    // new key slot
    em.add_ri32(R::RDI, 8);
    em.mov_ptr_r(R::RDI, R::R13);                     // new value slot
    em.jmp("__ac_ds_done__");

    // ---- fast path: n < cap — write in place, O(1), zero allocation/copy ----
    em.label("__ac_ds_fast__");
    em.mov_rr(R::RCX, R::R14);
    em.mov_ri32(R::RDX, 16); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX); em.add_ri32(R::RDI, 8);
    em.mov_ptr_r(R::RDI, R::R12);                    // key slot at ptr + 8 + 16n
    em.add_ri32(R::RDI, 8);
    em.mov_ptr_r(R::RDI, R::R13);                     // value slot
    em.mov_rr(R::RCX, R::R14); em.inc_r(R::RCX);
    em.mov_ptr_r(R::RBX, R::RCX);                       // ptr[0] = n+1
    em.mov_rr(R::RAX, R::RBX);                           // return same ptr — nothing moved

    em.label("__ac_ds_done__");
    em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// ac_ipow(rdi = base, rsi = exp) -> rax = base^exp (integer; exp<=0 → 1)
// Backs the `^` operator on BNY for variable exponents (literals are folded at compile time).
static void emitIpowLinux(X64Emitter& em) {
    em.label("ac_ipow");
    em.push_rbp(); em.mov_rbp_rsp();
    em.mov_ri32(R::RAX, 1);              // result = 1
    em.label("__ac_ipow_loop__");
    em.test_rr(R::RSI, R::RSI);
    em.jle("__ac_ipow_done__");          // while exp > 0
    em.imul_rr(R::RAX, R::RDI);          // result *= base
    em.dec_r(R::RSI);
    em.jmp("__ac_ipow_loop__");
    em.label("__ac_ipow_done__");
    em.pop_rbp(); em.ret();
}

// __ac_atoi__(rdi = char*) -> rax = parsed int64 (skips leading spaces, handles '-')
// Backs `to_int` on string values (Term.ask results).
static void emitAtoiLinux(X64Emitter& em) {
    em.label("__ac_atoi__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12);
    em.mov_rr(R::RBX, R::RDI);           // rbx = cursor
    em.mov_ri32(R::RAX, 0);              // result = 0
    em.mov_ri32(R::R12, 0);              // negative flag
    em.label("__ac_atoi_sp__");          // skip leading spaces
    em.movzx_r64_ptr8(R::RCX, R::RBX);
    em.cmp_r_i32(R::RCX, ' ');
    em.jne("__ac_atoi_sign__");
    em.inc_r(R::RBX);
    em.jmp("__ac_atoi_sp__");
    em.label("__ac_atoi_sign__");
    em.cmp_r_i32(R::RCX, '-');
    em.jne("__ac_atoi_loop__");
    em.mov_ri32(R::R12, 1);
    em.inc_r(R::RBX);
    em.label("__ac_atoi_loop__");
    em.movzx_r64_ptr8(R::RCX, R::RBX);
    em.cmp_r_i32(R::RCX, '0');
    em.jl("__ac_atoi_done__");
    em.cmp_r_i32(R::RCX, '9');
    em.jg("__ac_atoi_done__");
    em.mov_ri32(R::RDX, 10);
    em.imul_rr(R::RAX, R::RDX);          // result *= 10
    em.sub_r_i32(R::RCX, '0');
    em.add_rr(R::RAX, R::RCX);           // result += digit
    em.inc_r(R::RBX);
    em.jmp("__ac_atoi_loop__");
    em.label("__ac_atoi_done__");
    em.test_rr(R::R12, R::R12);
    em.je("__ac_atoi_ret__");
    em.neg_r(R::RAX);
    em.label("__ac_atoi_ret__");
    em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_rand__(rdi = n) -> rax = pseudo-random in [0, n)  (rdtsc-seeded xorshift)
static void emitRandLinux(X64Emitter& em) {
    em.label("__ac_rand__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX);
    em.mov_rr(R::RBX, R::RDI);          // rbx = n
    em.rdtsc();                          // rdtsc → edx:eax
    em.shl_r_i8(R::RDX, 32);             // shl rdx, 32
    em.or_rr(R::RAX, R::RDX);            // or rax, rdx  → 64-bit seed
    // xorshift64
    em.mov_rr(R::RCX, R::RAX); em.shl_r_i8(R::RCX, 13); em.xor_rr(R::RAX, R::RCX); // x ^= x<<13
    em.mov_rr(R::RCX, R::RAX); em.shr_r_i8(R::RCX, 7);  em.xor_rr(R::RAX, R::RCX); // x ^= x>>7
    em.mov_rr(R::RCX, R::RAX); em.shl_r_i8(R::RCX, 17); em.xor_rr(R::RAX, R::RCX); // x ^= x<<17
    em.test_rr(R::RBX, R::RBX);
    em.jle("__ac_rand_zero__");
    em.mov_rr(R::RCX, R::RBX);
    em.xor_rr(R::RDX, R::RDX);          // unsigned div: rdx=0
    em.div_rcx();                        // div rcx → rdx = x % n
    em.mov_rr(R::RAX, R::RDX);
    em.jmp("__ac_rand_done__");
    em.label("__ac_rand_zero__");
    em.mov_ri32(R::RAX, 0);
    em.label("__ac_rand_done__");
    em.pop_r(R::RBX); em.pop_rbp(); em.ret();
}

// ac_length(rdi = array block) -> rax = element count (layout: [len][e0][e1]...)
static void emitLengthLinux(X64Emitter& em) {
    em.label("ac_length");
    em.mov_r_ptr(R::RAX, R::RDI);        // rax = block[0] = len
    em.ret();
    // __ac_strlen__(rdi = char*) -> rax — for `length` on STRING values
    em.label("__ac_strlen__");
    em.mov_ri32(R::RAX, 0);
    em.label("__ac_strlen_loop__");
    em.movzx_r64_ptr8(R::RCX, R::RDI);
    em.test_rr(R::RCX, R::RCX);
    em.je("__ac_strlen_done__");
    em.inc_r(R::RAX); em.inc_r(R::RDI);
    em.jmp("__ac_strlen_loop__");
    em.label("__ac_strlen_done__");
    em.ret();
}

// ─── Print Helpers (Linux) ────────────────────────────────────────────────────
static void emitPrintIntCore(X64Emitter& em, const std::string& name, bool newline) {
    // name(rdi = int64): print decimal, optionally followed by newline
    std::string P = "__" + name + "_";   // unique label prefix per instantiation
    em.label(name);
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.sub_rsp_i32(32); // 32-byte local buffer, rsp is 16-aligned after 2 pushes + rbp

    em.mov_rr(R::R12, R::RDI);   // r12 = argument value
    em.xor_rr(R::R13, R::R13);   // r13 = negative flag

    // R10 = write pointer (going backwards from rbp-18).
    // MUST stay below the saved r12/r13 slots at [rbp-8] and [rbp-16] — writing the
    // buffer at rbp-1/-2 overwrote the SAVED R12, corrupting the caller's register
    // (surfaced as FOR-in loops never terminating once loop-carried temps used r12).
    em.lea_r_rbp8(R::R10, -18);

    // Handle zero
    em.test_rr(R::R12, R::R12);
    em.jnz(P+"notzero");
    em.mov_ri32(R::RAX, '0');
    em.mov_ptr_r8(R::R10, R::RAX);  // [r10] = '0'
    em.dec_r(R::R10);
    em.jmp(P+"fin");

    em.label(P+"notzero");
    em.test_rr(R::R12, R::R12);
    em.jns(P+"pos");
    em.mov_ri32(R::R13, 1);     // set negative flag
    em.neg_r(R::R12);           // negate to positive

    em.label(P+"pos");
    em.mov_ri32(R::RCX, 10);    // divisor = 10

    em.label(P+"loop");
    em.mov_rr(R::RAX, R::R12);   // rax = value
    em.xor_edx_edx();             // rdx = 0
    em.div_rcx();                 // rax = quotient, rdx = remainder
    em.add_dl_i8('0');            // dl = remainder + '0'
    em.mov_ptr_r8(R::R10, R::RDX); // [r10] = digit
    em.dec_r(R::R10);
    em.mov_rr(R::R12, R::RAX);   // r12 = quotient
    em.test_rr(R::R12, R::R12);
    em.jnz(P+"loop");

    // Add sign
    em.test_rr(R::R13, R::R13);
    em.jz(P+"fin");
    em.mov_ri32(R::RAX, '-');
    em.mov_ptr_r8(R::R10, R::RAX);
    em.dec_r(R::R10);

    em.label(P+"fin");
    em.inc_r(R::R10);            // r10 = first character
    if (newline) {
        em.mov_rbp8_imm8(-17, '\n'); // [rbp-17] = newline (below saved r12/r13)
        em.lea_r_rbp8(R::RDX, -17);   // last char = newline at rbp-17
    } else {
        em.lea_r_rbp8(R::RDX, -18);   // last char = final digit at rbp-18
    }
    // length = lastChar - r10 + 1
    em.sub_rr(R::RDX, R::R10);
    em.inc_r(R::RDX);

    // sys_write(1, r10, rdx)
    em.mov_rr(R::RSI, R::R10);
    em.mov_ri32(R::RDI, 1);
    em.mov_ri32(R::RAX, 1);
    em.syscall();

    em.add_rsp_i32(32);
    em.pop_r(R::R13);
    em.pop_r(R::R12);
    em.pop_rbp();
    em.ret();
}

static void emitPrintIntLinux(X64Emitter& em) {
    emitPrintIntCore(em, "__ac_print_int__", true);
    emitPrintIntCore(em, "__ac_print_int_raw__", false);  // no newline — used by list print
}

// __ac_print_arr__(rdi = list block [len][e0][e1]…): print "[e0, e1, …]\n" (matches PY)
static void emitPrintArrLinux(X64Emitter& em) {
    em.label("__ac_print_arr__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13);
    em.sub_rsp_i32(24);                      // small char buffer below saved regs
    em.mov_rr(R::RBX, R::RDI);               // rbx = block
    em.mov_r_ptr(R::R12, R::RBX);            // r12 = len
    em.mov_ri32(R::R13, 1);                  // r13 = idx (1-based words)
    // emit '['
    em.mov_rbp8_imm8(-33, '[');
    em.lea_r_rbp8(R::RSI, -33); em.mov_ri32(R::RDI, 1); em.mov_ri32(R::RDX, 1);
    em.mov_ri32(R::RAX, 1); em.syscall();
    em.label("__ac_parr_loop__");
    em.cmp_rr(R::R13, R::R12);
    em.jg("__ac_parr_done__");
    // separator ", " for idx > 1
    em.cmp_r_i32(R::R13, 1);
    em.je("__ac_parr_elem__");
    em.mov_rbp8_imm8(-33, ','); em.mov_rbp8_imm8(-32, ' ');
    em.lea_r_rbp8(R::RSI, -33); em.mov_ri32(R::RDI, 1); em.mov_ri32(R::RDX, 2);
    em.mov_ri32(R::RAX, 1); em.syscall();
    em.label("__ac_parr_elem__");
    em.mov_rr(R::RCX, R::R13); em.mov_ri32(R::RDX, 8); em.imul_rr(R::RCX, R::RDX);
    em.mov_rr(R::RDI, R::RBX); em.add_rr(R::RDI, R::RCX);
    em.mov_r_ptr(R::RDI, R::RDI);            // rdi = block[idx]
    em.call("__ac_print_int_raw__");
    em.inc_r(R::R13);
    em.jmp("__ac_parr_loop__");
    em.label("__ac_parr_done__");
    em.mov_rbp8_imm8(-33, ']'); em.mov_rbp8_imm8(-32, '\n');
    em.lea_r_rbp8(R::RSI, -33); em.mov_ri32(R::RDI, 1); em.mov_ri32(R::RDX, 2);
    em.mov_ri32(R::RAX, 1); em.syscall();
    em.add_rsp_i32(24);
    em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

static void emitPrintStrLinux(X64Emitter& em) {
    // __ac_print_str__(rdi = ptr, rsi = len): print string + newline
    em.label("__ac_print_str__");
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.sub_rsp_i32(16); // 16-byte local (for newline byte)

    em.mov_rr(R::R12, R::RDI);   // save ptr
    em.mov_rr(R::R13, R::RSI);   // save len

    // sys_write(1, ptr, len)
    em.mov_rr(R::RSI, R::R12);
    em.mov_rr(R::RDX, R::R13);
    em.mov_ri32(R::RDI, 1);
    em.mov_ri32(R::RAX, 1);
    em.syscall();

    // Write newline: store '\n' at [rbp-17] (outside the two 8-byte saved regs)
    em.mov_rbp8_imm8(-17, '\n');
    em.lea_r_rbp8(R::RSI, -17);
    em.mov_ri32(R::RDX, 1);
    em.mov_ri32(R::RDI, 1);
    em.mov_ri32(R::RAX, 1);
    em.syscall();

    em.add_rsp_i32(16);
    em.pop_r(R::R13);
    em.pop_r(R::R12);
    em.pop_rbp();
    em.ret();
}

static void emitPrintCStrLinux(X64Emitter& em) {
    // __ac_print_cstr__(rdi = null-terminated ptr): compute strlen, print + newline
    em.label("__ac_print_cstr__");
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.sub_rsp_i32(16);

    em.mov_rr(R::R12, R::RDI);   // R12 = base ptr
    em.test_rr(R::R12, R::R12);
    em.jz("__ac_cstr_done__");
    em.mov_rr(R::R13, R::RDI);   // R13 = cursor

    em.label("__ac_cstr_scan__");
    em.movzx_r64_ptr8(R::RAX, R::R13); // rax = *(cursor)
    em.test_rr(R::RAX, R::RAX);        // test for NUL
    em.je("__ac_cstr_done__");
    em.inc_r(R::R13);
    em.jmp("__ac_cstr_scan__");

    em.label("__ac_cstr_done__");
    em.sub_rr(R::R13, R::R12);   // R13 = len

    // sys_write(1, ptr, len)
    em.mov_rr(R::RSI, R::R12);
    em.mov_rr(R::RDX, R::R13);
    em.mov_ri32(R::RDI, 1);
    em.mov_ri32(R::RAX, 1);
    em.syscall();

    // newline
    em.mov_rbp8_imm8(-17, '\n');
    em.lea_r_rbp8(R::RSI, -17);
    em.mov_ri32(R::RDX, 1);
    em.mov_ri32(R::RDI, 1);
    em.mov_ri32(R::RAX, 1);
    em.syscall();

    em.add_rsp_i32(16);
    em.pop_r(R::R13);
    em.pop_r(R::R12);
    em.pop_rbp();
    em.ret();
}

// Inline ac_print_double — no libc, no libacmath.so.
// Prints a double with up to 16 significant decimal digits, correctly rounded, trailing zeros
// stripped. 2.0→"2"  2.5→"2.5"  10/3→"3.333333333333333"
// Registers: xmm0=value in, r12=buf_ptr, r13=int_part→final int value, r14=scratch cursor
//            (reused across phases), r15=scaled fraction, rbx=scratch. Sign lives in a
//            dedicated stack byte (not a register) so it survives every later phase without
//            fighting r14/rbx for space — see #28's fix below for why this rewrite needed one.
static void emitPrintDoubleLinux(X64Emitter& em) {
    em.label("ac_print_double");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::R15); em.push_r(R::RBX);
    em.sub_rsp_i32(200);
    // Stack layout (200-byte local area, safe below saved regs at rbp-8..rbp-40):
    //   rbp-192..rbp-163 : main output buffer (30 bytes max)
    //   rbp-162..rbp-143 : reverse integer digit buffer (20 bytes)
    //   rbp-142..rbp-125 : fractional digit buffer, raw 0-9 values, MSB-first (18 bytes: index 0
    //                      at rbp-142 .. index 16 [guard] at rbp-126) — raw values, not ASCII,
    //                      since rounding needs to compare/increment digit VALUES.
    //   rbp-125          : sign flag byte (0 = non-negative, 1 = negative)
    //   rbp-124..rbp-49  : spare

    // r12 = write pointer into main output buffer at rbp-192
    em.lea_r_rbp32(R::R12, -192);

    // ── Integer part + sign ─────────────────────────────────────────────────
    em.cvttsd2si_r13_xmm0();            // r13 = trunc(value)   [xmm0 still = ORIGINAL value]

    // Determine the sign from the ORIGINAL value, once, HERE — not from r13's own sign, which
    // is unreliable whenever |value| < 1 (truncates to exactly integer 0, which has no sign
    // bit at all: a value like -0.25 would silently lose its minus sign entirely). Verified
    // real bug (two layers, both already fixed once): -2.5 printed "-2.+" (cvttsd2si on an
    // un-negated fraction, -0.5*10=-5.0, truncates to digit -5; `-5+'0'` is ASCII '+', not
    // '5'); -0.25 printed "0..+" (r13==0 has no sign of its own). Stash the flag in a stack
    // byte (rbp-125) — a register can't survive the several phases (fraction-digit extraction,
    // rounding/carry, integer-digit extraction) that all need scratch registers of their own.
    em.mov_ri32(R::RAX, 0);
    em.cvtsi2sd_xmm1_from_gpr(R::RAX); // xmm1 = 0.0 (temporary — repurposed for (double)r13 below)
    em.ucomisd_xmm0_xmm1();            // compare ORIGINAL value against 0.0
    em.mov_ri32(R::RAX, 0);
    em.jcc(0x03, "__acd_notneg__");    // JAE/JNB (CF=0): value >= 0.0 → not negative
    em.mov_ri32(R::RAX, 1);
    em.label("__acd_notneg__");
    em.lea_r_rbp32(R::RCX, -125);
    em.mov_ptr_r8(R::RCX, R::RAX);     // [rbp-125] = sign flag

    em.cvtsi2sd_xmm1_from_gpr(R::R13); // xmm1 = (double)r13
    em.subsd_xmm0_xmm1();              // xmm0 = fractional part (NEGATIVE when value < 0,
                                        // since e.g. -2.5 - (-2.0) = -0.5, not +0.5)

    em.test_rr(R::RAX, R::RAX);        // rax still holds the sign flag from just above
    em.je("__acd_fracpos__");
    em.mov_ri64(R::RCX, (uint64_t)0xBFF0000000000000ULL); // -1.0
    em.movq_xmm1_from_gpr(R::RCX);
    em.mulsd_xmm0_xmm1();              // xmm0 = -fractional part (now positive)
    em.label("__acd_fracpos__");

    em.mov_rr(R::RBX, R::R13);
    em.test_rr(R::RAX, R::RAX);
    em.je("__acd_absdone__");
    em.neg_r(R::RBX);                  // rbx = |r13| (neg(0) is still 0 — fine)
    em.label("__acd_absdone__");       // rbx = |integer part|; may still be bumped by rounding below

    // ── Significant-digit budget: count |intpart|'s own decimal digits (min 1, even for 0),
    // so the fractional part only gets 16-minus-that-many digits — matches %.16g's convention
    // (significant digits are counted starting at the integer part's first digit, not the
    // decimal point). Verified real bug in an earlier version of this fix: always keeping 16
    // FRACTIONAL digits regardless of the integer part gave 17 total significant digits for
    // 5.640000000000001 (int part "5" = 1 digit + 16 kept fractional = 17), printing
    // "5.6400000000000006" instead of the correct "5.640000000000001" (15 fractional digits).
    // Stored at [rbp-124] since later phases (guard-digit position, rounding-carry start,
    // strip-loop's initial count) all need it and none of them have a spare register free.
    { em.mov_rr(R::RAX, R::RBX);
      em.mov_ri32(R::R14, 0);
      em.label("__acd_cntloop__");
      em.mov_ri32(R::RCX, 10);
      em.cqo(); em.idiv_rcx();
      em.inc_r(R::R14);
      em.test_rr(R::RAX, R::RAX);
      em.jne("__acd_cntloop__");        // r14 = digit count (>=1; 0 itself counts as 1 digit)
      em.mov_ri32(R::RAX, 16);
      em.sub_rr(R::RAX, R::R14);        // rax = 16 - digitcount
      em.cmp_r_i32(R::RAX, 0);
      em.jge("__acd_fkok__");
      em.mov_ri32(R::RAX, 0);
      em.label("__acd_fkok__");
      em.lea_r_rbp32(R::RCX, -124);
      em.mov_ptr_r(R::RCX, R::RAX);     // [rbp-124] = fracKeep (0-15)
    }

    // ── #28: fractional digits via EXACT 64-bit integer arithmetic, correctly rounded ──────
    // The OLD algorithm repeatedly did `frac *= 10.0` in DOUBLE precision — each multiply/
    // subtract step rounds, and by the ~13th-15th digit the accumulated error was large enough
    // that `frac` prematurely computed as exactly 0.0, truncating real digits (verified: 3.14 +
    // 2.5's true nearest double is 5.6400000000000005684...; PY's shortest round-trip repr is
    // "5.640000000000001" — glibc's `%.16g`, ALREADY the convention CStrategy/CppStrategy use
    // elsewhere in this codebase, agrees exactly — but the old BNY algorithm printed "5.64",
    // stopping 13 digits early). Fix: `frac` has at most 52 significant mantissa bits, so
    // `frac * 2^52` is an EXACT integer (no rounding — multiplying a double by a power of 2
    // only shifts its exponent) representable in a 64-bit register. Extracting decimal digits
    // from THAT via repeated `*10` / shift-52 / mask-52 is exact 64-bit integer arithmetic —
    // no floating-point rounding anywhere in the loop, so no premature-zero termination.
    em.mov_ri64(R::RAX, (uint64_t)0x4330000000000000ULL); // 2^52 as a double
    em.movq_xmm1_from_gpr(R::RAX);
    em.mulsd_xmm0_xmm1();               // xmm0 = frac * 2^52 (exact, still < 2^52)
    em.cvttsd2si_rax_xmm0();            // rax = scaled fraction, exact 64-bit integer
    em.mov_rr(R::R15, R::RAX);          // r15 = scaled

    // Extract 17 raw digit values (16 to keep + 1 guard digit for rounding) into
    // [rbp-142..rbp-126], MSB-first, via: scaled*=10; digit=scaled>>52; scaled&=(2^52-1).
    em.lea_r_rbp32(R::R14, -142);       // r14 = write cursor into the fraction-digit buffer
    { // rcx is the loop counter (pushed/popped each iteration — also needed as scratch inside)
      em.mov_ri32(R::RCX, 17);
      em.label("__acd_fdig__");
      em.push_r(R::RCX);
      em.mov_ri32(R::RCX, 10);
      em.imul_rr(R::R15, R::RCX);       // r15 *= 10 (safe: <2^52 * 10 < 2^56, fits in 64 bits)
      em.mov_rr(R::RAX, R::R15);
      em.shr_r_i8(R::RAX, 52);          // rax = top digit (0-9)
      em.mov_ptr_r8(R::R14, R::RAX); em.inc_r(R::R14);
      em.mov_ri64(R::RCX, (uint64_t)0xFFFFFFFFFFFFFULL); // (1<<52)-1
      em.and_rr(R::R15, R::RCX);        // r15 &= mask
      em.pop_r(R::RCX);
      em.dec_r(R::RCX); em.test_rr(R::RCX, R::RCX); em.jne("__acd_fdig__");
    }

    // ── Round-half-up using the guard digit at index fracKeep, carrying leftward through the
    // fracKeep kept digits at [rbp-142..rbp-142+fracKeep-1]; a carry that escapes past digit 0
    // (or fracKeep==0, meaning there's no fractional digit to carry through at all) bumps rbx
    // (the integer part) by 1 BEFORE the integer-digit loop below runs, so e.g.
    // 8.99999999999999996 → "9", not "8" with a wrong fractional tail.
    em.lea_r_rbp32(R::RAX, -142);
    em.lea_r_rbp32(R::RCX, -124); em.mov_r_ptr(R::RCX, R::RCX); // rcx = fracKeep
    em.add_rr(R::RAX, R::RCX);          // rax = &fracbuf[fracKeep] (the guard digit)
    em.movzx_r64_ptr8(R::RAX, R::RAX);
    em.cmp_r_i32(R::RAX, 5);
    em.jl("__acd_noround__");
    em.lea_r_rbp32(R::RCX, -124); em.mov_r_ptr(R::RCX, R::RCX);
    em.test_rr(R::RCX, R::RCX);
    em.jne("__acd_havefrac__");
    em.inc_r(R::RBX);                   // fracKeep==0: nothing to carry through, bump directly
    em.jmp("__acd_noround__");
    em.label("__acd_havefrac__");
    em.lea_r_rbp32(R::R14, -142);
    em.lea_r_rbp32(R::RCX, -124); em.mov_r_ptr(R::RCX, R::RCX);
    em.add_rr(R::R14, R::RCX); em.dec_r(R::R14); // r14 = &fracbuf[fracKeep-1] (last kept digit)
    em.label("__acd_carry__");
    em.movzx_r64_ptr8(R::RAX, R::R14);
    em.inc_r(R::RAX);
    em.cmp_r_i32(R::RAX, 10);
    em.jl("__acd_nooverflow__");
    em.mov_ri32(R::RAX, 0);
    em.mov_ptr_r8(R::R14, R::RAX);
    em.dec_r(R::R14);
    { // Carry escaped past digit 0 (index -1, address rbp-143) → bump the integer part.
      std::string cont = "__acd_carrycont__";
      em.lea_r_rbp32(R::RAX, -143);
      em.cmp_rr(R::R14, R::RAX);
      em.jne(cont);
      em.inc_r(R::RBX);
      em.jmp("__acd_noround__");
      em.label(cont);
    }
    em.jmp("__acd_carry__");
    em.label("__acd_nooverflow__");
    em.mov_ptr_r8(R::R14, R::RAX);
    em.label("__acd_noround__");

    // ── Sign character (rbx now holds the FINAL, possibly rounding-bumped integer value) ──
    em.lea_r_rbp32(R::RAX, -125);
    em.movzx_r64_ptr8(R::RAX, R::RAX);
    em.test_rr(R::RAX, R::RAX);
    em.je("__acd_nosign__");
    em.mov_ri32(R::RAX, '-');
    em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.label("__acd_nosign__");

    // ── Integer digits: reverse-extract rbx into [rbp-162..rbp-143], copy forward ──────────
    em.lea_r_rbp32(R::R14, -143);       // r14 = one past end of reverse area
    em.label("__acd_idig__");
    em.mov_rr(R::RAX, R::RBX);
    em.mov_ri32(R::RCX, 10);
    em.cqo(); em.idiv_rcx();
    em.add_ri32(R::RDX, '0');
    em.dec_r(R::R14);
    em.mov_ptr_r8(R::R14, R::RDX);
    em.mov_rr(R::RBX, R::RAX);
    em.test_rr(R::RBX, R::RBX);
    em.jne("__acd_idig__");
    em.lea_r_rbp32(R::RBX, -143);
    em.label("__acd_icpy__");
    em.movzx_r64_ptr8(R::RAX, R::R14);
    em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12); em.inc_r(R::R14);
    em.cmp_rr(R::R14, R::RBX);
    em.jl("__acd_icpy__");

    // ── Fractional digits: strip trailing zeros from the (rounded, fracKeep-digit) buffer ──
    // rcx = count of digits still kept (starts at fracKeep, shrinks while the last is 0).
    em.lea_r_rbp32(R::RCX, -124); em.mov_r_ptr(R::RCX, R::RCX);
    em.label("__acd_striploop__");
    em.test_rr(R::RCX, R::RCX);
    em.je("__acd_stripdone__");         // stripped everything → whole-valued, print ".0"
    em.lea_r_rbp32(R::RAX, -142);
    em.add_rr(R::RAX, R::RCX);
    em.dec_r(R::RAX);                   // rax = &fracbuf[rcx-1] (the last currently-kept digit)
    em.movzx_r64_ptr8(R::RDX, R::RAX);
    em.test_rr(R::RDX, R::RDX);
    em.jne("__acd_stripdone__");        // nonzero digit found — rcx is the final kept count
    em.dec_r(R::RCX);
    em.jmp("__acd_striploop__");
    em.label("__acd_stripdone__");

    em.test_rr(R::RCX, R::RCX);
    em.je("__acd_dotzero__");           // whole-valued float still prints ".0" (12.0, not 12)

    // ── Print '.' + the rcx kept fractional digits (ASCII) ────────────────────────────────
    em.mov_ri32(R::RAX, '.'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.lea_r_rbp32(R::R14, -142);        // r14 = cursor into fraction buffer, index 0 first
    em.mov_rr(R::RBX, R::RCX);           // rbx free again (integer part already fully printed)
    em.label("__acd_fcpy__");
    em.movzx_r64_ptr8(R::RAX, R::R14);
    em.add_ri32(R::RAX, '0');
    em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12); em.inc_r(R::R14);
    em.dec_r(R::RBX); em.test_rr(R::RBX, R::RBX); em.jne("__acd_fcpy__");
    em.jmp("__acd_nl__");

    // ── Whole-valued float (after rounding) → append ".0" ─────────────────────────────────
    em.label("__acd_dotzero__");
    em.mov_ri32(R::RAX, '.'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.mov_ri32(R::RAX, '0'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    // fall through to newline + write

    // ── Newline + write ────────────────────────────────────────────────────────────────────
    em.label("__acd_nl__");
    em.mov_ri32(R::RAX, '\n'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.lea_r_rbp32(R::RSI, -192);        // buf start
    em.sub_rr(R::R12, R::RSI);
    em.mov_rr(R::RDX, R::R12);
    em.mov_ri32(R::RDI, 1); em.mov_ri32(R::RAX, 1); em.syscall();

    em.add_rsp_i32(200);
    em.pop_r(R::RBX); em.pop_r(R::R15); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12);
    em.pop_rbp(); em.ret();
}

// `save as <file>` — accumulates everything Term.display'd so far into a 64KB buffer (lazily
// mmap'd via the existing __ac_alloc__ the same way __heap_cursor lazily mmaps its own storage —
// see this file's `usesSave_` comment), written out via a raw `open`/`write`/`close` syscall
// sequence when SAVE_FILE fires. Was a complete no-op before this session (`ir.cpp`'s SaveStmt
// case did nothing at all, on every backend) despite being a real, documented feature.
//
// __ac_save_append_cstr__(rdi = null-terminated ptr): strlen-scan (mirrors __ac_print_cstr__),
// lazy-alloc the buffer on first use, byte-copy the string + a newline onto the end, update len.
static void emitSaveAppendCStrLinux(X64Emitter& em, int bufPtrSlot, int bufLenSlot) {
    em.label("__ac_save_append_cstr__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::R15);

    em.mov_rr(R::R12, R::RDI);          // r12 = src base ptr
    em.mov_rr(R::R13, R::RDI);          // r13 = scan cursor
    em.label("__acsa_scan__");
    em.movzx_r64_ptr8(R::RAX, R::R13);
    em.test_rr(R::RAX, R::RAX);
    em.je("__acsa_scandone__");
    em.inc_r(R::R13);
    em.jmp("__acsa_scan__");
    em.label("__acsa_scandone__");
    em.sub_rr(R::R13, R::R12);          // r13 = strlen(src)

    // Lazy-allocate the 64KB buffer (same "cursor == 0 means never allocated" check __ac_alloc__
    // itself uses for __heap_cursor — a proven pattern, not new machinery).
    em.mov_ri64_gvar(R::RCX, bufPtrSlot);  // rcx = &buf_ptr
    em.mov_r_ptr(R::R14, R::RCX);          // r14 = buf_ptr
    em.test_rr(R::R14, R::R14);
    em.jne("__acsa_havebuf__");
    em.push_r(R::RCX); em.push_r(R::R12); em.push_r(R::R13);
    em.mov_ri32(R::RDI, 65536);
    em.call("__ac_alloc__");
    em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RCX);
    em.mov_ptr_r(R::RCX, R::RAX);
    em.mov_rr(R::R14, R::RAX);
    em.label("__acsa_havebuf__");

    // r15 = write cursor = buf_ptr + buf_len
    em.mov_ri64_gvar(R::RBX, bufLenSlot);  // rbx = &buf_len
    em.mov_r_ptr(R::RAX, R::RBX);          // rax = buf_len
    em.mov_rr(R::R15, R::R14);
    em.add_rr(R::R15, R::RAX);

    // Byte-copy loop: r12=src cursor, r13=remaining count, r15=dst cursor
    em.label("__acsa_copy__");
    em.test_rr(R::R13, R::R13);
    em.je("__acsa_copydone__");
    em.movzx_r64_ptr8(R::RAX, R::R12);
    em.mov_ptr_r8(R::R15, R::RAX);
    em.inc_r(R::R12); em.inc_r(R::R15); em.dec_r(R::R13);
    em.jmp("__acsa_copy__");
    em.label("__acsa_copydone__");

    // Append newline, then buf_len += (copied bytes + 1)
    em.mov_ri32(R::RAX, '\n');
    em.mov_ptr_r8(R::R15, R::RAX);
    em.inc_r(R::R15);
    em.sub_rr(R::R15, R::R14);          // r15 = new total length
    em.mov_ptr_r(R::RBX, R::R15);

    em.pop_r(R::R15); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

// __ac_save_append_int__(rdi = signed int64): itoa then delegate to the cstr appender.
static void emitSaveAppendIntLinux(X64Emitter& em) {
    em.label("__ac_save_append_int__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.call("__ac_itoa__");             // rax = heap-allocated decimal string
    em.mov_rr(R::RDI, R::RAX);
    em.call("__ac_save_append_cstr__");
    em.pop_rbp(); em.ret();
}

// __ac_save_append_double__(xmm0 = value): same digit-formatting algorithm as ac_print_double
// (see its comment for the full walkthrough) but appends the formatted text into the save
// buffer instead of writing it to stdout — kept as a full separate copy rather than trying to
// share code with ac_print_double at the machine-code level (no clean way to parameterize
// "where do the bytes go" without restructuring that already-correct, delicate function).
static void emitSaveAppendDoubleLinux(X64Emitter& em, int bufPtrSlot, int bufLenSlot) {
    em.label("__ac_save_append_double__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::R12); em.push_r(R::R13); em.push_r(R::R14); em.push_r(R::RBX);
    em.sub_rsp_i32(128);

    em.lea_r_rbp32(R::R12, -160);       // r12 = write pointer into local buffer

    em.cvttsd2si_r13_xmm0();            // r13 = trunc(value)  [xmm0 still = ORIGINAL value]

    // Same fix as ac_print_double (see its own comment for the full walkthrough): the sign
    // must come from the ORIGINAL value, checked once here, not from r13 (unreliable for
    // |value| < 1, which truncates to integer 0 — no sign bit). Verified real bug here too:
    // `save as` output for -2.5 wrote "-2.+"; for -0.25, "0..+".
    em.mov_ri32(R::RAX, 0);
    em.cvtsi2sd_xmm1_from_gpr(R::RAX); // xmm1 = 0.0 (temporary)
    em.ucomisd_xmm0_xmm1();
    em.mov_ri32(R::R14, 0);
    em.jcc(0x03, "__acsd_notneg__");   // JAE/JNB (CF=0): value >= 0.0
    em.mov_ri32(R::R14, 1);
    em.label("__acsd_notneg__");

    em.cvtsi2sd_xmm1_from_gpr(R::R13);
    em.subsd_xmm0_xmm1();

    em.test_rr(R::R14, R::R14);
    em.je("__acsd_fracpos__");
    em.mov_ri64(R::RAX, (uint64_t)0xBFF0000000000000ULL); // -1.0
    em.movq_xmm1_from_gpr(R::RAX);
    em.mulsd_xmm0_xmm1();
    em.label("__acsd_fracpos__");

    em.mov_rr(R::RBX, R::R13);
    em.test_rr(R::R14, R::R14);
    em.je("__acsd_pos__");
    em.mov_ri32(R::RAX, '-');
    em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.neg_r(R::RBX);
    em.label("__acsd_pos__");

    em.lea_r_rbp32(R::R14, -110);
    em.label("__acsd_idig__");
    em.mov_rr(R::RAX, R::RBX);
    em.mov_ri32(R::RCX, 10);
    em.cqo(); em.idiv_rcx();
    em.add_ri32(R::RDX, '0');
    em.dec_r(R::R14);
    em.mov_ptr_r8(R::R14, R::RDX);
    em.mov_rr(R::RBX, R::RAX);
    em.test_rr(R::RBX, R::RBX);
    em.jne("__acsd_idig__");
    em.lea_r_rbp32(R::RBX, -110);
    em.label("__acsd_icpy__");
    em.movzx_r64_ptr8(R::RAX, R::R14);
    em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12); em.inc_r(R::R14);
    em.cmp_rr(R::R14, R::RBX);
    em.jl("__acsd_icpy__");

    em.mov_ri32(R::RAX, 0);
    em.cvtsi2sd_xmm1_from_gpr(R::RAX);
    em.ucomisd_xmm0_xmm1();
    em.je("__acsd_dotzero__");

    em.mov_ri32(R::RAX, '.'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.mov_rr(R::R13, R::R12);

    em.mov_ri32(R::R14, 15);
    em.label("__acsd_fdig__");
    em.mov_ri64(R::RAX, (uint64_t)0x4024000000000000ULL); // 10.0
    em.movq_xmm1_from_gpr(R::RAX);
    em.mulsd_xmm0_xmm1();
    em.cvttsd2si_rax_xmm0();
    em.mov_rr(R::RBX, R::RAX);
    em.cvtsi2sd_xmm1_from_gpr(R::RAX);
    em.subsd_xmm0_xmm1();
    em.add_ri32(R::RBX, '0');
    em.mov_ptr_r8(R::R12, R::RBX); em.inc_r(R::R12);
    em.dec_r(R::R14); em.test_rr(R::R14, R::R14); em.je("__acsd_strip__");
    em.mov_ri32(R::RAX, 0);
    em.cvtsi2sd_xmm1_from_gpr(R::RAX);
    em.ucomisd_xmm0_xmm1();
    em.jne("__acsd_fdig__");

    em.label("__acsd_strip__");
    em.dec_r(R::R12);
    em.movzx_r64_ptr8(R::RAX, R::R12);
    em.cmp_r_i32(R::RAX, '0'); em.je("__acsd_strip__");
    em.inc_r(R::R12);
    em.jmp("__acsd_done__");

    em.label("__acsd_dotzero__");
    em.mov_ri32(R::RAX, '.'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);
    em.mov_ri32(R::RAX, '0'); em.mov_ptr_r8(R::R12, R::RAX); em.inc_r(R::R12);

    em.label("__acsd_done__");
    // NUL-terminate the local buffer, then hand it to the cstr appender (which strlen-scans it
    // and does the lazy-alloc + copy against the real global save buffer).
    em.mov_ri32(R::RAX, 0);
    em.mov_ptr_r8(R::R12, R::RAX);
    em.lea_r_rbp32(R::RDI, -160);
    (void)bufPtrSlot; (void)bufLenSlot;  // reached only via __ac_save_append_cstr__ below
    em.call("__ac_save_append_cstr__");

    em.add_rsp_i32(128);
    em.pop_r(R::RBX); em.pop_r(R::R14); em.pop_r(R::R13); em.pop_r(R::R12);
    em.pop_rbp(); em.ret();
}

// SAVE_FILE: raw open/write/close syscalls — writes the accumulated buffer to the named file.
static void emitSaveFileLinux(X64Emitter& em, int bufPtrSlot, int bufLenSlot) {
    em.label("__ac_save_file__");        // rdi = NUL-terminated path ptr
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX); em.push_r(R::R12);
    em.mov_rr(R::R12, R::RDI);           // save path ptr across syscalls

    // open(path, O_WRONLY|O_CREAT|O_TRUNC = 0x241, 0644)
    em.mov_rr(R::RDI, R::R12);
    em.mov_ri32(R::RSI, 0x241);
    em.mov_ri32(R::RDX, 0644);
    em.mov_ri32(R::RAX, 2);              // sys_open
    em.syscall();
    em.mov_rr(R::RBX, R::RAX);           // rbx = fd (or negative errno)
    em.test_rr(R::RBX, R::RBX);
    em.jl("__acsf_done__");              // couldn't open — silently skip, matches other backends'
                                          // "if (_f) {...}" guard rather than crashing the program

    em.mov_ri64_gvar(R::RCX, bufPtrSlot);
    em.mov_r_ptr(R::RSI, R::RCX);        // rsi = buf ptr
    em.mov_ri64_gvar(R::RCX, bufLenSlot);
    em.mov_r_ptr(R::RDX, R::RCX);        // rdx = buf len
    em.mov_rr(R::RDI, R::RBX);
    em.mov_ri32(R::RAX, 1);              // sys_write
    em.syscall();

    em.mov_rr(R::RDI, R::RBX);
    em.mov_ri32(R::RAX, 3);              // sys_close
    em.syscall();

    em.label("__acsf_done__");
    em.pop_r(R::R12); em.pop_r(R::RBX);
    em.pop_rbp(); em.ret();
}

static void emitInputIntLinux(X64Emitter& em) {
    // __ac_input_int__(): read integer from stdin via syscall, parse, return in RAX
    em.label("__ac_input_int__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::R12); em.push_r(R::R13);
    em.sub_rsp_i32(64);  // buffer for input (at [rbp-64])

    // Syscall: read(0, buffer, 64) - read up to 64 bytes from stdin
    em.lea_r_rbp32(R::RSI, -64);  // buffer address
    em.mov_ri32(R::RDI, 0);        // fd = 0 (stdin)
    em.mov_ri32(R::RDX, 64);       // count = 64
    em.mov_ri32(R::RAX, 0);        // syscall 0 = read
    em.syscall();

    // RAX now has bytes read; save to R13
    em.mov_rr(R::R13, R::RAX);

    // Parse integer from buffer [rbp-64]
    // Start with result = 0 in RAX
    em.xor_rr(R::RAX, R::RAX);
    em.xor_rr(R::R12, R::R12);     // R12 = sign flag (0=pos, 1=neg)
    em.lea_r_rbp32(R::RDI, -64);   // RDI = buffer pointer

    // R12 = 0 (collecting digits), 1 (after first newline)
    em.xor_rr(R::R12, R::R12);     // reset to 0 for digit collection

    // Loop: for each byte in buffer
    em.label("__read_loop__");
    em.test_rr(R::R13, R::R13);    // check bytes remaining
    em.jz("__read_done__");

    em.movzx_r64_ptr8(R::RCX, R::RDI);  // load byte

    // Check for newline (stop parsing after first line)
    em.cmp_r_i32(R::RCX, '\n');
    em.je("__read_done__");

    // Check if digit (0-9)
    em.cmp_r_i32(R::RCX, '0');
    em.jl("__read_skip__");
    em.cmp_r_i32(R::RCX, '9');
    em.jg("__read_skip__");

    // Digit found: result = result * 10 + digit
    em.mov_ri32(R::RDX, 10);
    em.imul_rr(R::RAX, R::RDX);    // rax *= 10
    em.sub_r_i32(R::RCX, '0');     // digit = byte - '0'
    em.add_rr(R::RAX, R::RCX);     // result += digit

    em.label("__read_skip__");
    em.inc_r(R::RDI);              // next byte
    em.dec_r(R::R13);              // decrement counter
    em.jmp("__read_loop__");

    em.label("__read_done__");
    // RAX now contains the parsed integer

    em.add_rsp_i32(64);
    em.pop_r(R::R13); em.pop_r(R::R12);
    em.pop_rbp();
    em.ret();
}

// __ac_input_str__(): read string line from stdin, return buffer pointer in RAX
static void emitInputStrLinux(X64Emitter& em, StringPool& sp) {
    em.label("__ac_input_str__");
    em.push_rbp(); em.mov_rbp_rsp();
    em.push_r(R::RBX);
    em.push_r(R::R12);
    em.push_r(R::R13);
    int emptySid = sp.add("");

    // Allocate stable storage for the returned string. Returning a stack pointer here
    // corrupts Term.ask as soon as this helper returns.
    em.mov_ri32(R::RDI, 0);         // addr = NULL
    em.mov_ri32(R::RSI, 4096);      // length
    em.mov_ri32(R::RDX, 3);         // PROT_READ | PROT_WRITE
    em.mov_ri32(R::R10, 0x22);      // MAP_PRIVATE | MAP_ANONYMOUS
    em.mov_ri32(R::R8, -1);         // fd
    em.mov_ri32(R::R9, 0);          // offset
    em.mov_ri32(R::RAX, 9);         // sys_mmap
    em.syscall();
    em.cmp_r_i32(R::RAX, 0);
    em.jl("__ac_input_str_fail__");
    em.mov_rr(R::RBX, R::RAX);      // RBX = stable input buffer

    em.mov_rr(R::R13, R::RBX);      // R13 = cursor
    em.mov_ri32(R::R12, 4095);      // remaining capacity before NUL

    em.label("__ac_input_str_scan__");
    em.test_rr(R::R12, R::R12);
    em.je("__ac_input_str_term__");

    // Read one byte at a time so one Term.ask consumes exactly one line.
    em.mov_ri32(R::RDI, 0);
    em.mov_rr(R::RSI, R::R13);
    em.mov_ri32(R::RDX, 1);
    em.mov_ri32(R::RAX, 0);
    em.syscall();
    em.cmp_r_i32(R::RAX, 1);
    em.jne("__ac_input_str_term__");

    em.movzx_r64_ptr8(R::RCX, R::R13);
    em.cmp_r_i32(R::RCX, '\n');
    em.je("__ac_input_str_term__");
    em.cmp_r_i32(R::RCX, '\r');
    em.je("__ac_input_str_term__");
    em.inc_r(R::R13);
    em.dec_r(R::R12);
    em.jmp("__ac_input_str_scan__");

    em.label("__ac_input_str_term__");
    em.mov_ri32(R::RAX, 0);
    em.mov_ptr_r8(R::R13, R::RAX);
    em.mov_rr(R::RAX, R::RBX);
    em.jmp("__ac_input_str_return__");

    em.label("__ac_input_str_fail__");
    em.mov_ri64_str(R::RAX, emptySid);

    em.label("__ac_input_str_return__");
    em.pop_r(R::R13);
    em.pop_r(R::R12);
    em.pop_r(R::RBX);
    em.pop_rbp();
    em.ret();
}

// ─── DWARF Debug Info Builder ────────────────────────────────────────────────
class DWARFBuilder {
    struct FuncInfo { std::string name; uint64_t startVA, endVA; };
    std::vector<FuncInfo> funcs;
    std::string srcFile;
    std::string compDir;

    static void uleb128(std::vector<uint8_t>& v, uint64_t n) {
        do {
            uint8_t b = n & 0x7F; n >>= 7;
            if (n) b |= 0x80;
            v.push_back(b);
        } while (n);
    }
    static void a16(std::vector<uint8_t>& v, uint16_t n) {
        v.push_back(n & 0xFF); v.push_back(n >> 8);
    }
    static void a32(std::vector<uint8_t>& v, uint32_t n) {
        v.push_back(n); v.push_back(n>>8); v.push_back(n>>16); v.push_back(n>>24);
    }
    static void a64(std::vector<uint8_t>& v, uint64_t n) {
        a32(v, (uint32_t)n); a32(v, (uint32_t)(n>>32));
    }
    static void aStr(std::vector<uint8_t>& v, const std::string& s) {
        for (char c : s) v.push_back((uint8_t)c);
        v.push_back(0);
    }

public:
    void setSourceFile(const std::string& f, const std::string& d = ".") {
        srcFile = f; compDir = d;
    }
    void addFunction(const std::string& name, uint64_t startVA, uint64_t endVA) {
        funcs.push_back({name, startVA, endVA});
    }

    // .debug_abbrev: describe the DIE shapes we'll emit
    std::vector<uint8_t> buildAbbrev() {
        std::vector<uint8_t> v;

        // Abbrev 1: DW_TAG_compile_unit (0x11), has children
        uleb128(v,1); uleb128(v,0x11); v.push_back(1);
        uleb128(v,0x25); v.push_back(0x08); // DW_AT_producer,  DW_FORM_string
        uleb128(v,0x13); v.push_back(0x05); // DW_AT_language,  DW_FORM_data2
        uleb128(v,0x03); v.push_back(0x08); // DW_AT_name,      DW_FORM_string
        uleb128(v,0x1B); v.push_back(0x08); // DW_AT_comp_dir,  DW_FORM_string
        uleb128(v,0x10); v.push_back(0x17); // DW_AT_stmt_list, DW_FORM_sec_offset
        v.push_back(0); v.push_back(0);

        // Abbrev 2: DW_TAG_subprogram (0x2E), no children
        uleb128(v,2); uleb128(v,0x2E); v.push_back(0);
        uleb128(v,0x03); v.push_back(0x08); // DW_AT_name,       DW_FORM_string
        uleb128(v,0x11); v.push_back(0x01); // DW_AT_low_pc,     DW_FORM_addr
        uleb128(v,0x12); v.push_back(0x01); // DW_AT_high_pc,    DW_FORM_addr
        uleb128(v,0x40); v.push_back(0x18); // DW_AT_frame_base, DW_FORM_exprloc
        v.push_back(0); v.push_back(0);

        v.push_back(0); // end of abbreviation table
        return v;
    }

    // .debug_info: compilation unit + subprogram DIEs
    std::vector<uint8_t> buildInfo() {
        std::vector<uint8_t> body;

        // Compile-unit DIE (abbrev 1)
        uleb128(body, 1);
        aStr(body, "AC 1");   // DWARF producer string (was the stale v0.2.0)
        a16(body, 0x0001);  // DW_LANG_C
        aStr(body, srcFile);
        aStr(body, compDir);
        a32(body, 0);       // DW_AT_stmt_list = offset 0 in .debug_line

        // Subprogram DIEs (abbrev 2)
        for (auto& fn : funcs) {
            uleb128(body, 2);
            aStr(body, fn.name);
            a64(body, fn.startVA);
            a64(body, fn.endVA);
            // DW_AT_frame_base: DW_FORM_exprloc (1 byte expr: DW_OP_call_frame_cfa)
            uleb128(body, 1); body.push_back(0x9C);
        }
        body.push_back(0); // end-of-children

        // Prepend CU header: unit_length(4) version(2) abbrev_offset(4) addr_size(1)
        std::vector<uint8_t> v;
        a32(v, 2 + 4 + 1 + (uint32_t)body.size()); // unit_length (excludes itself)
        a16(v, 4);   // DWARF version 4
        a32(v, 0);   // debug_abbrev_offset
        v.push_back(8); // address_size = 8
        v.insert(v.end(), body.begin(), body.end());
        return v;
    }

    // .debug_line: minimal line-number program mapping each function start to line 1
    std::vector<uint8_t> buildLine() {
        // extract base filename
        std::string baseName = srcFile;
        size_t sl = baseName.find_last_of("/\\");
        if (sl != std::string::npos) baseName = baseName.substr(sl+1);

        // standard opcode lengths for opcodes 1..12
        const uint8_t std_len[12] = {0,1,1,1,1,0,0,0,1,0,0,1};

        // Line program header body
        std::vector<uint8_t> hdr;
        hdr.push_back(1);  // minimum_instruction_length
        hdr.push_back(1);  // maximum_ops_per_instruction (DWARF 4)
        hdr.push_back(1);  // default_is_stmt
        hdr.push_back((uint8_t)(int8_t)(-5)); // line_base
        hdr.push_back(14); // line_range
        hdr.push_back(13); // opcode_base (first special = 13)
        for (int i = 0; i < 12; i++) hdr.push_back(std_len[i]);
        hdr.push_back(0); // include_directories: empty
        // file_names: one entry
        aStr(hdr, baseName);
        uleb128(hdr, 0); uleb128(hdr, 0); uleb128(hdr, 0); // dir_idx, mtime, fsize
        hdr.push_back(0); // end of file names

        // Line program opcodes
        std::vector<uint8_t> prog;
        for (auto& fn : funcs) {
            // DW_LNE_set_address
            prog.push_back(0x00); uleb128(prog, 9); prog.push_back(0x02);
            a64(prog, fn.startVA);
            // DW_LNS_copy (emit a row)
            prog.push_back(0x01);
        }
        // DW_LNE_end_sequence
        prog.push_back(0x00); uleb128(prog, 1); prog.push_back(0x01);

        // Assemble: unit_length(4) version(2) header_length(4) hdr prog
        std::vector<uint8_t> v;
        uint32_t unitLen = 2 + 4 + (uint32_t)hdr.size() + (uint32_t)prog.size();
        a32(v, unitLen);
        a16(v, 4);                         // version = 4
        a32(v, (uint32_t)hdr.size());      // header_length
        v.insert(v.end(), hdr.begin(), hdr.end());
        v.insert(v.end(), prog.begin(), prog.end());
        return v;
    }
};

struct DebugSections {
    std::vector<uint8_t> abbrev, info, line;
    bool empty() const { return abbrev.empty(); }
};

// ─── ELF64 Writer ─────────────────────────────────────────────────────────────
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
struct ElfShdr {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
};
struct Elf64Sym {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};
struct Elf64Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};
struct Elf64Dyn {
    int64_t  d_tag;
    uint64_t d_val;
};
#pragma pack(pop)

// ─── PE32+ (Windows x86-64) Writer ────────────────────────────────────────────
// Windows has no stable raw-syscall ABI (unlike Linux) — every PE binary needs at
// least an Import Address Table into kernel32.dll, even for the simplest program
// (ExitProcess). Proven standalone against Wine + cross-checked structurally
// against a real MinGW-produced binary via objdump before being wired in here.
#pragma pack(push,1)
struct PEDosHeader {
    uint16_t e_magic = 0x5A4D; // "MZ"
    uint16_t e_cblp=0,e_cp=0,e_crlc=0,e_cparhdr=0,e_minalloc=0,e_maxalloc=0;
    uint16_t e_ss=0,e_sp=0,e_csum=0,e_ip=0,e_cs=0,e_lfarlc=0,e_ovno=0;
    uint16_t e_res[4]={0,0,0,0};
    uint16_t e_oemid=0,e_oeminfo=0;
    uint16_t e_res2[10]={0,0,0,0,0,0,0,0,0,0};
    int32_t  e_lfanew;
};
struct PECoffHeader {
    uint32_t Signature = 0x00004550; // "PE\0\0"
    uint16_t Machine = 0x8664;       // AMD64
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp = 0;
    uint32_t PointerToSymbolTable = 0;
    uint32_t NumberOfSymbols = 0;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics = 0x0022; // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
};
struct PEDataDirectory { uint32_t VirtualAddress=0; uint32_t Size=0; };
struct PEOptionalHeader64 {
    uint16_t Magic = 0x20b; // PE32+
    uint8_t  MajorLinkerVersion=1, MinorLinkerVersion=0;
    uint32_t SizeOfCode=0, SizeOfInitializedData=0, SizeOfUninitializedData=0;
    uint32_t AddressOfEntryPoint=0;
    uint32_t BaseOfCode=0;
    uint64_t ImageBase = 0x140000000ULL;
    uint32_t SectionAlignment = 0x1000;
    uint32_t FileAlignment = 0x200;
    uint16_t MajorOSVersion=6, MinorOSVersion=0;
    uint16_t MajorImageVersion=0, MinorImageVersion=0;
    uint16_t MajorSubsystemVersion=6, MinorSubsystemVersion=0;
    uint32_t Win32VersionValue=0;
    uint32_t SizeOfImage=0, SizeOfHeaders=0;
    uint32_t CheckSum=0;
    uint16_t Subsystem = 3; // IMAGE_SUBSYSTEM_WINDOWS_CUI (console)
    uint16_t DllCharacteristics = 0x8140; // NX_COMPAT|TERMINAL_SERVER_AWARE|HIGH_ENTROPY_VA
    uint64_t SizeOfStackReserve=0x100000, SizeOfStackCommit=0x1000;
    uint64_t SizeOfHeapReserve=0x100000, SizeOfHeapCommit=0x1000;
    uint32_t LoaderFlags=0;
    uint32_t NumberOfRvaAndSizes=16;
    PEDataDirectory DataDir[16];
};
struct PESectionHeader {
    char     Name[8]={0,0,0,0,0,0,0,0};
    uint32_t VirtualSize=0;
    uint32_t VirtualAddress=0;
    uint32_t SizeOfRawData=0;
    uint32_t PointerToRawData=0;
    uint32_t PointerToRelocations=0;
    uint32_t PointerToLinenumbers=0;
    uint16_t NumberOfRelocations=0;
    uint16_t NumberOfLinenumbers=0;
    uint32_t Characteristics=0;
};
struct PEImportDescriptor {
    uint32_t OriginalFirstThunk=0; // RVA -> Import Lookup Table
    uint32_t TimeDateStamp=0;
    uint32_t ForwarderChain=0;
    uint32_t Name=0;               // RVA -> DLL name string
    uint32_t FirstThunk=0;         // RVA -> Import Address Table (patched by loader)
};
#pragma pack(pop)

static uint32_t peAlignUp(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

// One imported function, grouped by DLL for the import table builder below.
struct PEImport { std::string dll; std::string func; };

// PE always lays out as .text at RVA SECT_ALIGN, .idata at RVA 2*SECT_ALIGN (single
// code section + single import section — sufficient for what BNY emits for Windows
// so far). Fixed and public so callers can compute label offsets before layout.
static constexpr uint32_t PE_SECT_ALIGN = 0x1000;
static constexpr uint32_t PE_TEXT_RVA   = PE_SECT_ALIGN;
static constexpr uint32_t PE_IDATA_RVA  = 2 * PE_SECT_ALIGN;

// Shared layout for the .idata section: [descriptors][ILT per dll][IAT per dll]
// [hint/name per import][dll name strings]. Computed once, consumed both to bind
// call-site fixup labels (before em.applyFixups()) and to actually build the bytes
// (in writePE) — keeping both in sync by construction instead of by hand.
struct PEImportLayout {
    std::vector<std::string> dlls;
    std::map<std::string, std::vector<size_t>> byDll;      // dll -> indices into `imports`
    std::map<std::string, size_t> dllIltOff, dllIatOff, dllNameOff;
    std::map<size_t, size_t> hintNameOff;                   // import idx -> offset
    size_t descrBytes = 0;
    size_t totalSize = 0;

    // RVA of the IAT slot for a given import index — what call sites fix up against.
    uint32_t iatSlotRVA(size_t importIdx, const std::vector<PEImport>& imports) const {
        auto& dll = imports[importIdx].dll;
        auto& v = byDll.at(dll);
        size_t k = std::find(v.begin(), v.end(), importIdx) - v.begin();
        return PE_IDATA_RVA + (uint32_t)dllIatOff.at(dll) + (uint32_t)k * 8;
    }
};

static PEImportLayout layoutPEImports(const std::vector<PEImport>& imports) {
    PEImportLayout L;
    for (size_t i = 0; i < imports.size(); i++) {
        auto& dll = imports[i].dll;
        if (L.byDll.find(dll) == L.byDll.end()) L.dlls.push_back(dll);
        L.byDll[dll].push_back(i);
    }
    L.descrBytes = (L.dlls.size() + 1) * sizeof(PEImportDescriptor);
    size_t cursor = L.descrBytes;
    for (auto& dll : L.dlls) { L.dllIltOff[dll] = cursor; cursor += (L.byDll[dll].size() + 1) * 8; }
    for (auto& dll : L.dlls) { L.dllIatOff[dll] = cursor; cursor += (L.byDll[dll].size() + 1) * 8; }
    for (auto& dll : L.dlls) {
        for (size_t idx : L.byDll[dll]) {
            L.hintNameOff[idx] = cursor;
            size_t len = 2 + imports[idx].func.size() + 1;
            if (len % 2) len++;
            cursor += len;
        }
    }
    for (auto& dll : L.dlls) { L.dllNameOff[dll] = cursor; cursor += dll.size() + 1; }
    L.totalSize = cursor;
    return L;
}

// Lay out and write a minimal static PE32+ executable: one .text (code) section and
// (if `imports` is non-empty) one .idata section. `text` must already have its
// `call qword ptr [rip+disp32]` sites patched — the caller binds those via
// em.defineLabelAt(importName, layoutPEImports(imports).iatSlotRVA(idx,imports) -
// PE_TEXT_RVA) before em.applyFixups(), reusing the existing generic fixup engine
// instead of a parallel one here.
static bool writePE(const std::string& path,
                     std::vector<uint8_t> text,
                     const std::vector<PEImport>& imports,
                     uint32_t entryOffset) {
    const uint32_t SECT_ALIGN = PE_SECT_ALIGN;
    const uint32_t FILE_ALIGN = 0x200;

    PEImportLayout L = layoutPEImports(imports);
    std::vector<uint8_t> idata(L.totalSize, 0);
    auto put64 = [&](size_t off, uint64_t v){ std::memcpy(&idata[off], &v, 8); };

    uint32_t textRVA  = PE_TEXT_RVA;
    uint32_t idataRVA = PE_IDATA_RVA;

    for (auto& dll : L.dlls) {
        for (size_t idx : L.byDll[dll]) {
            size_t off = L.hintNameOff[idx];
            idata[off] = 0; idata[off+1] = 0; // hint = 0 (name-based lookup)
            std::memcpy(&idata[off+2], imports[idx].func.c_str(), imports[idx].func.size()+1);
        }
        std::memcpy(&idata[L.dllNameOff[dll]], dll.c_str(), dll.size()+1);

        size_t n = L.byDll[dll].size();
        for (size_t k = 0; k < n; k++) {
            uint64_t hnRVA = idataRVA + (uint32_t)L.hintNameOff[L.byDll[dll][k]];
            put64(L.dllIltOff[dll] + k*8, hnRVA);
            put64(L.dllIatOff[dll] + k*8, hnRVA);
        }
        put64(L.dllIltOff[dll] + n*8, 0); // ILT terminator
        put64(L.dllIatOff[dll] + n*8, 0); // IAT terminator
    }
    for (size_t d = 0; d < L.dlls.size(); d++) {
        PEImportDescriptor id{};
        id.OriginalFirstThunk = idataRVA + (uint32_t)L.dllIltOff[L.dlls[d]];
        id.Name               = idataRVA + (uint32_t)L.dllNameOff[L.dlls[d]];
        id.FirstThunk         = idataRVA + (uint32_t)L.dllIatOff[L.dlls[d]];
        std::memcpy(&idata[d*sizeof(PEImportDescriptor)], &id, sizeof(id));
    }
    // Final descriptor slot stays zeroed (terminator).

    uint32_t numSections = imports.empty() ? 1 : 2;
    uint32_t hdrSize = sizeof(PEDosHeader) + sizeof(PECoffHeader) + sizeof(PEOptionalHeader64)
                       + numSections * sizeof(PESectionHeader);
    uint32_t sizeOfHeaders = peAlignUp(hdrSize, FILE_ALIGN);

    uint32_t textRawOff  = sizeOfHeaders;
    uint32_t textRawSize = peAlignUp((uint32_t)text.size(), FILE_ALIGN);
    uint32_t idataRawOff  = textRawOff + textRawSize;
    uint32_t idataRawSize = peAlignUp((uint32_t)idata.size(), FILE_ALIGN);

    uint32_t sizeOfImage = imports.empty()
        ? peAlignUp(textRVA + (uint32_t)text.size(), SECT_ALIGN)
        : peAlignUp(idataRVA + (uint32_t)idata.size(), SECT_ALIGN);

    PEDosHeader dos{};
    dos.e_lfanew = sizeof(PEDosHeader);

    PECoffHeader coff{};
    coff.NumberOfSections = numSections;
    coff.SizeOfOptionalHeader = sizeof(PEOptionalHeader64);

    PEOptionalHeader64 opt{};
    opt.AddressOfEntryPoint = textRVA + entryOffset;
    opt.BaseOfCode = textRVA;
    opt.SizeOfCode = textRawSize;
    opt.SizeOfInitializedData = idataRawSize;
    opt.SizeOfImage = sizeOfImage;
    opt.SizeOfHeaders = sizeOfHeaders;
    if (!imports.empty()) {
        opt.DataDir[1].VirtualAddress = idataRVA; // Import Table
        opt.DataDir[1].Size = (uint32_t)L.descrBytes;
    }

    PESectionHeader shText{};
    std::memcpy(shText.Name, ".text", 5);
    shText.VirtualSize = (uint32_t)text.size();
    shText.VirtualAddress = textRVA;
    shText.SizeOfRawData = textRawSize;
    shText.PointerToRawData = textRawOff;
    shText.Characteristics = 0x60000020; // CODE|EXECUTE|READ

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((char*)&dos, sizeof(dos));
    f.write((char*)&coff, sizeof(coff));
    f.write((char*)&opt, sizeof(opt));
    f.write((char*)&shText, sizeof(shText));

    if (!imports.empty()) {
        PESectionHeader shIdata{};
        std::memcpy(shIdata.Name, ".idata", 6);
        shIdata.VirtualSize = (uint32_t)idata.size();
        shIdata.VirtualAddress = idataRVA;
        shIdata.SizeOfRawData = idataRawSize;
        shIdata.PointerToRawData = idataRawOff;
        shIdata.Characteristics = 0xC0000040; // INITIALIZED_DATA|READ|WRITE (loader patches the IAT)
        f.write((char*)&shIdata, sizeof(shIdata));
    }

    std::vector<uint8_t> pad(sizeOfHeaders - hdrSize, 0);
    f.write((char*)pad.data(), pad.size());

    text.resize(textRawSize, 0);
    f.write((char*)text.data(), text.size());

    if (!imports.empty()) {
        idata.resize(idataRawSize, 0);
        f.write((char*)idata.data(), idata.size());
    }
    f.close();
    return true;
}

// ── Minimal relocatable-object (.o) reader for --static-link splicing ─────────────
// Reads a freestanding ilib object (ET_REL, verified zero undefined externals) and returns its
// .text bytes + the offset of each defined .text symbol. BNY appends the .text into its own code
// and points each ilib call at the spliced offset → one standalone binary, no gcc/ld, no DT_NEEDED.
// Pilot objects have no relocations (pure self-contained code); `hadRelocs` flags any that would.
struct SplicedObject {
    std::vector<uint8_t>            text;
    std::map<std::string, uint64_t> symOffset;  // defined .text symbol → offset within .text
    bool ok = false;
    bool hadRelocs = false;
};

static SplicedObject readFreestandingObject(const std::string& path) {
    SplicedObject obj;
    std::ifstream f(path, std::ios::binary);
    if (!f) return obj;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < sizeof(ElfEhdr)) return obj;
    const ElfEhdr* eh = reinterpret_cast<const ElfEhdr*>(b.data());
    if (!(eh->e_ident[0]==0x7f && eh->e_ident[1]=='E' && eh->e_ident[2]=='L' && eh->e_ident[3]=='F')) return obj;
    if (eh->e_type != 1 /*ET_REL*/ || eh->e_machine != 62 /*x86-64*/) return obj;
    if (eh->e_shoff == 0 || eh->e_shentsize != sizeof(ElfShdr) || eh->e_shnum == 0) return obj;
    if ((uint64_t)eh->e_shoff + (uint64_t)eh->e_shnum * sizeof(ElfShdr) > b.size()) return obj;
    const ElfShdr* sh = reinterpret_cast<const ElfShdr*>(b.data() + eh->e_shoff);
    int nsh = eh->e_shnum;
    if (eh->e_shstrndx >= (uint16_t)nsh) return obj;
    const char* shstr = reinterpret_cast<const char*>(b.data() + sh[eh->e_shstrndx].sh_offset);
    int textIdx = -1, symtabIdx = -1;
    for (int i = 0; i < nsh; i++) {
        std::string nm = shstr + sh[i].sh_name;
        if (nm == ".text") textIdx = i;
        else if (sh[i].sh_type == 2 /*SHT_SYMTAB*/) symtabIdx = i;
        if (sh[i].sh_type == 4 /*SHT_RELA*/ || sh[i].sh_type == 9 /*SHT_REL*/) obj.hadRelocs = true;
    }
    if (textIdx < 0 || symtabIdx < 0) return obj;
    int strtabIdx = (int)sh[symtabIdx].sh_link;
    if (strtabIdx < 0 || strtabIdx >= nsh) return obj;
    if (sh[textIdx].sh_offset + sh[textIdx].sh_size > b.size()) return obj;
    obj.text.assign(b.begin() + sh[textIdx].sh_offset,
                    b.begin() + sh[textIdx].sh_offset + sh[textIdx].sh_size);
    const Elf64Sym* syms = reinterpret_cast<const Elf64Sym*>(b.data() + sh[symtabIdx].sh_offset);
    int nsyms = (int)(sh[symtabIdx].sh_size / sizeof(Elf64Sym));
    const char* strtab = reinterpret_cast<const char*>(b.data() + sh[strtabIdx].sh_offset);
    for (int i = 0; i < nsyms; i++) {
        if (syms[i].st_shndx == (uint16_t)textIdx && syms[i].st_name != 0)
            obj.symOffset[strtab + syms[i].st_name] = syms[i].st_value;
    }
    obj.ok = true;
    return obj;
}

static bool writeELF(const std::string& path,
                     const std::vector<uint8_t>& text,
                     const std::vector<uint8_t>& rodata,
                     uint64_t entryOffset = 0,
                     const DebugSections* dbg = nullptr,
                     bool rodataWritable = false) {
    const uint64_t BASE   = 0x400000ULL;
    const uint64_t PGSZ   = 0x1000ULL;

    size_t hdrBytes = sizeof(ElfEhdr) + 2*sizeof(ElfPhdr);
    uint64_t textOff   = ((hdrBytes + PGSZ-1) / PGSZ) * PGSZ;
    uint64_t rodataOff = textOff + ((text.size() + PGSZ-1) / PGSZ) * PGSZ;

    uint64_t textVA   = BASE + textOff;
    uint64_t rodataVA = BASE + rodataOff;
    uint64_t entry    = textVA + entryOffset;

    bool hasDebug = dbg && !dbg->empty();

    // Build section name string table when we have debug sections
    std::vector<uint8_t> shstrtab;
    std::map<std::string,uint32_t> shstrIdx;
    uint64_t dbgAbbrevOff=0, dbgInfoOff=0, dbgLineOff=0, shstrtabOff=0, shdrsOff=0;
    int shnum=0, shstrndx=0;

    if (hasDebug) {
        auto addSN = [&](const std::string& s) -> uint32_t {
            uint32_t idx = (uint32_t)shstrtab.size();
            shstrIdx[s] = idx;
            for (char c : s) shstrtab.push_back((uint8_t)c);
            shstrtab.push_back(0);
            return idx;
        };
        addSN("");           // index 0: empty (required)
        addSN(".text");
        addSN(".rodata");
        addSN(".debug_abbrev");
        addSN(".debug_info");
        addSN(".debug_line");
        addSN(".shstrtab");

        uint64_t rodataEnd = rodataOff + rodata.size();
        dbgAbbrevOff = rodataEnd;
        dbgInfoOff   = dbgAbbrevOff + dbg->abbrev.size();
        dbgLineOff   = dbgInfoOff   + dbg->info.size();
        shstrtabOff  = dbgLineOff   + dbg->line.size();
        shdrsOff     = (shstrtabOff + shstrtab.size() + 7) & ~7ULL; // 8-byte align

        shnum    = 7;  // NULL + .text + .rodata + 3 debug + .shstrtab
        shstrndx = 6;  // .shstrtab is section 6
    }

    bool emitRodata = !rodata.empty();
    uint16_t phnum  = emitRodata ? 2 : 1;

    ElfEhdr eh = {};
    eh.e_ident[0]=0x7F; eh.e_ident[1]='E'; eh.e_ident[2]='L'; eh.e_ident[3]='F';
    eh.e_ident[4]=2; eh.e_ident[5]=1; eh.e_ident[6]=1;
    eh.e_type=2; eh.e_machine=0x3E; eh.e_version=1;
    eh.e_entry=entry; eh.e_phoff=sizeof(ElfEhdr);
    eh.e_ehsize=sizeof(ElfEhdr); eh.e_phentsize=sizeof(ElfPhdr); eh.e_phnum=phnum;
    if (hasDebug) {
        eh.e_shoff    = shdrsOff;
        eh.e_shentsize = sizeof(ElfShdr);
        eh.e_shnum    = (uint16_t)shnum;
        eh.e_shstrndx = (uint16_t)shstrndx;
    }

    ElfPhdr tp = {};
    tp.p_type=1; tp.p_flags=5;
    tp.p_offset=textOff; tp.p_vaddr=textVA; tp.p_paddr=textVA;
    tp.p_filesz=text.size(); tp.p_memsz=text.size(); tp.p_align=PGSZ;

    ElfPhdr rp = {};
    rp.p_type=1; rp.p_flags = rodataWritable ? 6 : 4; // 6=R|W for NA→free global slots, else R
    rp.p_offset=rodataOff; rp.p_vaddr=rodataVA; rp.p_paddr=rodataVA;
    rp.p_filesz=rodata.size(); rp.p_memsz=rodata.size(); rp.p_align=PGSZ;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write((char*)&eh, sizeof(eh));
    f.write((char*)&tp, sizeof(tp));
    if (emitRodata) f.write((char*)&rp, sizeof(rp));

    // Pad to textOff (account for phnum * sizeof(ElfPhdr))
    size_t pad = textOff - (sizeof(ElfEhdr) + phnum * sizeof(ElfPhdr));
    std::vector<uint8_t> z(pad, 0);
    f.write((char*)z.data(), z.size());
    f.write((char*)text.data(), text.size());

    // Pad to rodataOff
    size_t pad2 = rodataOff - (textOff + text.size());
    z.assign(pad2, 0);
    f.write((char*)z.data(), z.size());
    if (!rodata.empty())
        f.write((char*)rodata.data(), rodata.size());

    if (hasDebug) {
        // Append debug sections immediately after rodata (no page alignment)
        f.write((char*)dbg->abbrev.data(), dbg->abbrev.size());
        f.write((char*)dbg->info.data(),   dbg->info.size());
        f.write((char*)dbg->line.data(),   dbg->line.size());
        f.write((char*)shstrtab.data(),    shstrtab.size());

        // Align to 8 bytes before section headers
        uint64_t curOff = shstrtabOff + shstrtab.size();
        if (curOff < shdrsOff) {
            z.assign((size_t)(shdrsOff - curOff), 0);
            f.write((char*)z.data(), z.size());
        }

        // Section header table
        auto writeSh = [&](uint32_t name, uint32_t type, uint64_t flags,
                           uint64_t addr, uint64_t offset, uint64_t size,
                           uint32_t lnk=0, uint32_t inf=0, uint64_t align=1) {
            ElfShdr sh = {};
            sh.sh_name=name; sh.sh_type=type; sh.sh_flags=flags;
            sh.sh_addr=addr; sh.sh_offset=offset; sh.sh_size=size;
            sh.sh_link=lnk; sh.sh_info=inf; sh.sh_addralign=align;
            f.write((char*)&sh, sizeof(sh));
        };

        writeSh(0,0,0,0,0,0);                                              // NULL
        writeSh(shstrIdx[".text"],   1, 6, textVA,   textOff,   text.size(),   0,0,16); // SHT_PROGBITS, ALLOC|EXEC
        writeSh(shstrIdx[".rodata"], 1, 2, rodataVA, rodataOff, rodata.size(), 0,0,1);  // SHT_PROGBITS, ALLOC
        writeSh(shstrIdx[".debug_abbrev"], 1,0,0, dbgAbbrevOff, dbg->abbrev.size(), 0,0,1);
        writeSh(shstrIdx[".debug_info"],   1,0,0, dbgInfoOff,   dbg->info.size(),   0,0,1);
        writeSh(shstrIdx[".debug_line"],   1,0,0, dbgLineOff,   dbg->line.size(),   0,0,1);
        writeSh(shstrIdx[".shstrtab"],     3,0,0, shstrtabOff,  shstrtab.size(),    0,0,1); // SHT_STRTAB
    }

    f.close();
    chmod(path.c_str(), 0755);
    return true;
}

// ─── Dynamic linking helpers ──────────────────────────────────────────────────

struct ExtSym {
    std::string irName;     // e.g. "math.sin"
    std::string exportName; // e.g. "ac_sin"
    std::string lib;        // e.g. "libacmath.so"
};

static bool isNativeCpuPtrSym(const std::string& name) {
    static const std::set<std::string> bareCarriedOver = {
        "ptr_new", "ptr_deref", "ptr_null", "ptr_is_null",
        "ptr_eq", "ptr_copy", "ptr_update"
    };
    return bareCarriedOver.count(name) > 0;
}

static std::string normalizeExtSym(const std::string& irName) {
    static const std::map<std::string,std::string> tbl = {
        // ── Trig ──────────────────────────────────────────────────────────────
        {"math.sin",    "ac_sin"},    {"math.cos",    "ac_cos"},
        {"math.tan",    "ac_tan"},    {"math.csc",    "ac_csc"},
        {"math.sec",    "ac_sec"},    {"math.cot",    "ac_cot"},
        {"math.asin",   "ac_asin"},   {"math.acos",   "ac_acos"},
        {"math.atan",   "ac_atan"},   {"math.acsc",   "ac_acsc"},
        {"math.asec",   "ac_asec"},   {"math.acot",   "ac_acot"},
        {"math.atan2",  "ac_atan2"},
        {"math.deg2rad","ac_deg2rad"},{"math.rad2deg","ac_rad2deg"},
        // ── Arithmetic ────────────────────────────────────────────────────────
        {"math.sqrt",   "ac_sqrt"},   {"math.pow",    "ac_pow"},
        {"math.cbrt",   "ac_cbrt"},   {"math.abs",    "ac_abs"},
        {"math.abs_int","ac_abs_int"},{"math.floor",  "ac_floor"},
        {"math.ceil",   "ac_ceil"},   {"math.round",  "ac_round"},
        {"math.hypot",  "ac_hypot"},  {"math.clamp",  "ac_clamp"},
        {"math.ln",     "ac_ln"},     {"math.log",    "ac_log_base"},
        {"math.log2",   "ac_log2"},   {"math.log10",  "ac_log10"},
        {"math.mod",    "ac_mod"},    {"math.mod_int","ac_mod_int"},
        {"math.to_int", "ac_to_int"},  {"math.to_dec", "ac_to_dec"},
        {"math.gcd",    "ac_gcd"},    {"math.lcm",    "ac_lcm"},
        {"math.is_prime","ac_is_prime"},
        // ── Constants ─────────────────────────────────────────────────────────
        {"math.pi",     "ac_math_pi_const"},
        {"math.e",      "ac_math_e_const"},
        {"math.phi",    "ac_math_phi_const"},
        {"math.tau",    "ac_math_tau_const"},
        {"math.em",     "ac_math_em_const"},
        {"math.inf",    "ac_math_inf"},
        // ── Print helper ──────────────────────────────────────────────────────
        {"ac_print_double", "ac_print_double"},
        // ── Camera library ────────────────────────────────────────────────────
        {"camera.init",             "ac_camera_init"},
        {"camera.capture",          "ac_camera_capture"},
        {"camera.capture_latest",   "ac_camera_capture_latest"},
        {"camera.capture_first",    "ac_camera_capture_first"},
        {"camera.release",          "ac_camera_release"},
        {"sidebar.config",          "ac_sidebar_config"},
        {"sidebar.setregion",       "ac_sidebar_setregion"},
        {"sidebar.setinteractive",  "ac_sidebar_setinteractive"},
        {"sidebar.display",         "ac_sidebar_display"},
        {"sidebar.ask",             "ac_sidebar_ask"},
        {"sidebar.getinput",        "ac_sidebar_getinput"},
        {"screen.setmode",          "ac_screen_setmode"},
        {"screen.update",           "ac_screen_update"},
        // ── aczip library ─────────────────────────────────────────────────────
        // AC has no raw byte-buffer type, so these route through the file-to-file
        // convenience functions in aczip_c.h/.cpp (shared with every other backend's
        // FFI — see that file's own comment) instead of the raw ACZipByteArray API.
        {"aczip.compress",          "ac_zip_compress_to_file"},
        {"aczip.decompress",        "ac_zip_decompress_from_file"},
        {"aczip.get_ratio",         "ac_get_compression_ratio"},
        // ── Web library ──────────────────────────────────────────────────────
        {"web.open",                "ac_web_open"},
        {"web.file_open",           "ac_web_file_open"},
        {"web.popen",               "ac_web_popen"},
        {"web.ropen",               "ac_web_ropen"},
        {"web.browser",             "ac_web_browser"},
        {"web.pdf",                 "ac_web_pdf"},
        {"web.text",                "ac_web_text"},
        {"web.inspect",             "ac_web_inspect"},
        {"web.ac_page",             "ac_web_ac_page"},
        {"web.page_get",            "ac_web_page_get"},
        {"web.help",                "ac_web_help"},
        // ── web-server library ──────────────────────────────────────────────
        {"server.db_run",        "ac_server_db_run"},
        {"server.db_run_p",      "ac_server_db_run_p"},
        {"server.db_import",     "ac_server_db_import"},
        {"server.db_reset",      "ac_server_db_reset"},
        {"server.db_stop",       "ac_server_db_stop"},
        {"server.listen",        "ac_server_listen"},
        {"server.accept",        "ac_server_accept"},
        {"server.req_method",    "ac_server_req_method"},
        {"server.req_path",      "ac_server_req_path"},
        {"server.req_query",     "ac_server_req_query"},
        {"server.req_body",      "ac_server_req_body"},
        {"server.req_header",    "ac_server_req_header"},
        {"server.respond",       "ac_server_respond"},
        {"server.respond_json",  "ac_server_respond_json"},
        {"server.close",         "ac_server_close"},
        {"server.help",          "ac_server_help"},
        // ── Native ML library ───────────────────────────────────────────────
        {"ml.tensor",                "ml_tensor"},
        {"ml.grid",                  "ml_grid"},
        {"ml.gradient_track",        "ml_gradient_track"},
        {"ml.backward",              "ml_backward"},
        {"ml.grad_wipe",             "ml_grad_wipe"},
        {"ml.weights",               "ml_weights"},
        {"ml.take",                  "ml_take"},
        {"ml.grad",                  "ml_get_grad"},
        {"ml.optimize",              "ml_optimize"},
        {"ml.add",                   "ml_add"},
        {"ml.multiply",              "ml_multiply"},
        {"ml.relu",                  "ml_relu"},
        // "maudio.stop" is the compiler's auto-injected no-arg shutdown call (ir.cpp's
        // injectAutoShutoff) — must NOT fall through to the generic "maudio." fallback below,
        // which would produce "ac_maudio_stop" (the PER-TRACK stop, takes a handle int — wrong
        // arity for a 0-arg call). ac_maudio_stop_all() is the real 0-arg one.
        {"maudio.stop",              "ac_maudio_stop_all"},
    };
    auto it = tbl.find(irName);
    if (it != tbl.end()) return it->second;
    // Generic ilib namespace fallback: os.cwd -> ac_os_cwd, regex.match -> ac_regex_match,
    // stringm.upper -> ac_stringm_upper, ncpu.dha -> ac_ncpu_dha, maudio.speak -> ac_maudio_speak
    // (the C-core export convention).
    for (const char* ns : {"os.", "regex.", "stringm.", "ncpu.", "maudio."}) {
        if (irName.rfind(ns, 0) == 0) {
            std::string s = "ac_" + irName;
            for (auto& c : s) if (c == '.') c = '_';
            return s;
        }
    }
    // native-cpu's carried-over ptr_* functions are called BARE (ptr_new, not
    // ncpu.ptr_new) but the real .so symbol is still ac_ncpu_ptr_new — map it here,
    // same as the dotted case above just without a '.' to replace.
    if (isNativeCpuPtrSym(irName)) return "ac_ncpu_" + irName;
    return irName;
}

static std::string libForSym(const std::string& exportName) {
    // libc functions - dynamic linking
    if (exportName == "printf" || exportName == "fprintf" || 
        exportName == "scanf" || exportName == "fscanf" ||
        exportName == "dlopen" || exportName == "dlsym" ||
        exportName == "malloc" || exportName == "free" ||
        exportName == "strlen" || exportName == "strcpy" ||
        exportName == "strncpy")
        return "libc.so.6";
    
    // pthread functions
    if (exportName.rfind("pthread_", 0) == 0)
        return "libpthread.so.0";
    
    // Camera/screen functions
    if (exportName.rfind("ac_camera_", 0) == 0 ||
        exportName.rfind("ac_sidebar_", 0) == 0 ||
        exportName.rfind("ac_screen_", 0) == 0)
        return "libaccamera.so";

    // web-server library (libacserver)
    if (exportName.rfind("ac_server_", 0) == 0)
        return "libacserver.so";

    // Web library (libacweb)
    if (exportName.rfind("ac_web_", 0) == 0)
        return "libacweb.so";

    // Widgets library (libacwidgets) — must precede the generic ac_ math fallback below,
    // which would otherwise wrongly claim every ac_widgets_* symbol too.
    if (exportName.rfind("ac_widgets_", 0) == 0)
        return "libacwidgets.so";

    // os / regex / string-cheese / native-cpu / machine-audio libraries (must precede the
    // generic ac_ math fallback below, which would otherwise wrongly claim these too)
    if (exportName.rfind("ac_os_", 0) == 0)      return "libacoos.so";
    if (exportName.rfind("ac_regex_", 0) == 0)   return "libacregex.so";
    if (exportName.rfind("ac_stringm_", 0) == 0) return "libacstringcheese.so";
    if (exportName.rfind("ac_ncpu_", 0) == 0)    return "libacncpu.so";
    if (exportName.rfind("ac_maudio_", 0) == 0)  return "libacmachinaaudio.so";
    // native-cpu's carried-over ptr_* functions stay bare (no ac_ncpu_ prefix) — see
    // isNativeCpuPtrSym().
    if (isNativeCpuPtrSym(exportName)) return "libacncpu.so";

    // Native ML library (libacml)
    if (exportName.rfind("ml_", 0) == 0 ||
        exportName.rfind("pt_", 0) == 0 ||
        exportName.rfind("tf_", 0) == 0)
        return "libacml.so";

    // aczip library (libaczip) — must precede the generic ac_ math fallback below, which
    // would otherwise wrongly claim ac_zip_compress_to_file/ac_get_compression_ratio/etc
    // too (they all start with "ac_" but aren't math functions).
    if (exportName.rfind("ac_zip_", 0) == 0 ||
        exportName == "ac_get_compression_ratio" ||
        exportName == "ac_free_bytes")
        return "libaczip.so";

    // Math library (libacmath)
    if (exportName.rfind("ac_", 0) == 0) return "libacmath.so";

    return "";
}

// Dynamic ELF writer — ET_EXEC with PLT/GOT for external symbol resolution.
static bool writeELFDynamic(const std::string& path,
                            const std::vector<uint8_t>& text,
                            const std::vector<uint8_t>& rodata,
                            uint64_t startOffset,
                            uint64_t gotpltVA,
                            uint64_t textVA,
                            const std::vector<ExtSym>& extSyms,
                            const std::vector<uint64_t>& pltPushVAs,
                            int gvarCount = 0,
                            const std::string& runpath = "")
{
    int N = (int)extSyms.size();
    if (N == 0) return false;

    const uint64_t BASE = 0x400000ULL;
    const uint64_t PGSZ = 0x1000ULL;
    const char INTERP[] = "/lib64/ld-linux-x86-64.so.2";
    size_t interpSize = sizeof(INTERP);

    // ── .dynstr ───────────────────────────────────────────────────────────────
    std::vector<uint8_t> dynstr;
    dynstr.push_back(0);
    auto addStr = [&](const std::string& s) -> uint32_t {
        uint32_t idx = (uint32_t)dynstr.size();
        for (char c : s) dynstr.push_back((uint8_t)c);
        dynstr.push_back(0);
        return idx;
    };
    std::vector<uint32_t> symNameOff(N);
    for (int i = 0; i < N; i++) symNameOff[i] = addStr(extSyms[i].exportName);

    std::vector<std::string> neededLibs;
    for (auto& s : extSyms)
        if (!s.lib.empty() &&
            std::find(neededLibs.begin(), neededLibs.end(), s.lib) == neededLibs.end())
            neededLibs.push_back(s.lib);
    std::vector<uint32_t> libNameOff(neededLibs.size());
    for (int i = 0; i < (int)neededLibs.size(); i++)
        libNameOff[i] = addStr(neededLibs[i]);
    // DT_RUNPATH: absolute ilib dirs so the loader finds the .so deps without LD_LIBRARY_PATH.
    bool     haveRunpath = !runpath.empty();
    uint32_t runpathOff  = haveRunpath ? addStr(runpath) : 0;

    // ── .dynsym ───────────────────────────────────────────────────────────────
    int nsyms = 1 + N;
    std::vector<Elf64Sym> dynsym(nsyms, Elf64Sym{});
    for (int i = 0; i < N; i++) {
        auto& s  = dynsym[1 + i];
        s.st_name  = symNameOff[i];
        s.st_info  = 0x12; // STB_GLOBAL | STT_FUNC
        s.st_other = 0;
        s.st_shndx = 0;
        s.st_value = 0;
        s.st_size  = 0;
    }

    // ── .hash ─────────────────────────────────────────────────────────────────
    // nbucket=1, nchain=nsyms, bucket[0]=1, chain[1..N] forms linear chain
    std::vector<uint32_t> hashBuf;
    hashBuf.push_back(1);            // nbucket
    hashBuf.push_back((uint32_t)nsyms); // nchain
    hashBuf.push_back(nsyms > 1 ? 1u : 0u); // bucket[0]
    hashBuf.push_back(0u);           // chain[0] = null sym
    for (int i = 1; i < nsyms; i++)
        hashBuf.push_back((i + 1 < nsyms) ? (uint32_t)(i + 1) : 0u);

    size_t hashSize   = hashBuf.size() * 4;
    size_t dynsymSize = nsyms * sizeof(Elf64Sym);
    size_t dynstrSize = dynstr.size();

    // ── .rela.plt ─────────────────────────────────────────────────────────────
    std::vector<Elf64Rela> rela(N);
    for (int i = 0; i < N; i++) {
        rela[i].r_offset = gotpltVA + (uint64_t)(3 + i) * 8;
        rela[i].r_info   = ((uint64_t)(1 + i) << 32) | 7; // R_X86_64_JUMP_SLOT
        rela[i].r_addend = 0;
    }
    size_t relaSize = N * sizeof(Elf64Rela);

    // ── Layout ────────────────────────────────────────────────────────────────
    // LOAD[0] starts at file offset 0 (covers ELF header + phdr + all RX sections).
    // This ensures AT_PHDR (program header VA = BASE+0x40) is within a mapped segment.
    size_t nPhdr    = 4;
    size_t hdrBytes = sizeof(ElfEhdr) + nPhdr * sizeof(ElfPhdr); // 288 bytes

    // File offsets:
    uint64_t interpOff  = hdrBytes;                    // right after headers
    uint64_t hashOff    = interpOff  + interpSize;
    uint64_t dynsymOff  = hashOff    + hashSize;
    uint64_t dynstrOff  = dynsymOff  + dynsymSize;
    uint64_t relapltOff = dynstrOff  + dynstrSize;
    uint64_t textOff2   = relapltOff + relaSize;
    // textVA parameter must equal BASE + textOff2
    (void)textVA; // already verified by caller

    // Page-align past actual data so RX and RW segments don't share a page.
    uint64_t textEnd    = textOff2 + text.size();
    uint64_t rodataOff  = ((textEnd + PGSZ - 1) / PGSZ) * PGSZ;
    uint64_t rodataEnd  = rodata.empty() ? rodataOff : rodataOff + rodata.size();
    uint64_t seg2Off    = ((rodataEnd + PGSZ - 1) / PGSZ) * PGSZ;
    if (rodata.empty()) seg2Off = rodataOff; // already page-aligned

    uint64_t seg2VA     = BASE + seg2Off;
    uint64_t gotpltOff  = seg2Off;

    size_t   gotpltSize = (size_t)(3 + N) * 8;
    uint64_t dynamicOff = gotpltOff + gotpltSize;
    uint64_t dynamicVA  = seg2VA    + gotpltSize;
    uint64_t hashVA     = BASE + hashOff;
    uint64_t dynsymVA   = BASE + dynsymOff;
    uint64_t dynstrVA   = BASE + dynstrOff;
    uint64_t relapltVA  = BASE + relapltOff;
    uint64_t rodataVA   = BASE + rodataOff; (void)rodataVA;

    // ── .dynamic ─────────────────────────────────────────────────────────────
    std::vector<Elf64Dyn> dynEntries;
    auto dyn = [&](int64_t tag, uint64_t val) { dynEntries.push_back({tag, val}); };
    for (auto& lo : libNameOff) dyn(1, (uint64_t)lo); // DT_NEEDED
    if (haveRunpath) dyn(29, (uint64_t)runpathOff); // DT_RUNPATH (self-locating .so deps)
    dyn(3,  gotpltVA);                             // DT_PLTGOT
    dyn(4,  hashVA);                               // DT_HASH
    dyn(5,  dynstrVA);                             // DT_STRTAB
    dyn(6,  dynsymVA);                             // DT_SYMTAB
    dyn(10, (uint64_t)dynstrSize);                 // DT_STRSZ
    dyn(11, (uint64_t)sizeof(Elf64Sym));           // DT_SYMENT
    dyn(2,  (uint64_t)relaSize);                   // DT_PLTRELSZ
    dyn(9,  (uint64_t)sizeof(Elf64Rela));          // DT_RELAENT
    dyn(20, 7ULL);                                 // DT_PLTREL = DT_RELA(7)
    dyn(23, relapltVA);                            // DT_JMPREL
    dyn(0,  0);                                    // DT_NULL
    size_t dynamicSize = dynEntries.size() * sizeof(Elf64Dyn);

    // ── .got.plt ──────────────────────────────────────────────────────────────
    std::vector<uint64_t> gotplt(3 + N, 0);
    gotplt[0] = dynamicVA;
    gotplt[1] = 0; // ld.so fills: link_map
    gotplt[2] = 0; // ld.so fills: _dl_runtime_resolve
    for (int i = 0; i < N; i++)
        gotplt[3 + i] = (i < (int)pltPushVAs.size()) ? pltPushVAs[i] : 0;

    // ── ELF header ────────────────────────────────────────────────────────────
    // seg1 starts at file offset 0, covers headers + all RX sections through text/rodata
    uint64_t seg1End   = rodata.empty() ? textEnd : (rodataOff + rodata.size());
    uint64_t seg1Size  = seg1End; // because seg1 starts at offset 0
    // NA→free: writable 8-byte slots placed right after .dynamic inside seg2 (R|W).
    size_t   gvarSize  = (size_t)gvarCount * 8;
    uint64_t seg2Size  = gotpltSize + dynamicSize + gvarSize;

    uint64_t entry     = BASE + textOff2 + startOffset;
    uint64_t interpVA  = BASE + interpOff;

    ElfEhdr eh = {};
    eh.e_ident[0]=0x7F; eh.e_ident[1]='E'; eh.e_ident[2]='L'; eh.e_ident[3]='F';
    eh.e_ident[4]=2; eh.e_ident[5]=1; eh.e_ident[6]=1;
    eh.e_type    = 2;     // ET_EXEC
    eh.e_machine = 0x3E;  // x86-64
    eh.e_version = 1;
    eh.e_entry   = entry;
    eh.e_phoff   = sizeof(ElfEhdr);
    eh.e_ehsize  = sizeof(ElfEhdr);
    eh.e_phentsize = sizeof(ElfPhdr);
    eh.e_phnum   = (uint16_t)nPhdr;

    // ── Program headers ───────────────────────────────────────────────────────
    ElfPhdr phInterp = {};
    phInterp.p_type   = 3; // PT_INTERP
    phInterp.p_flags  = 4; // R
    phInterp.p_offset = interpOff;
    phInterp.p_vaddr  = interpVA;
    phInterp.p_paddr  = interpVA;
    phInterp.p_filesz = interpSize;
    phInterp.p_memsz  = interpSize;
    phInterp.p_align  = 1;

    ElfPhdr phLoad1 = {};
    phLoad1.p_type   = 1; // PT_LOAD
    phLoad1.p_flags  = 5; // R|X — starts at 0 so AT_PHDR is within mapped range
    phLoad1.p_offset = 0;
    phLoad1.p_vaddr  = BASE;
    phLoad1.p_paddr  = BASE;
    phLoad1.p_filesz = seg1Size;
    phLoad1.p_memsz  = seg1Size;
    phLoad1.p_align  = PGSZ;

    ElfPhdr phLoad2 = {};
    phLoad2.p_type   = 1; // PT_LOAD
    phLoad2.p_flags  = 6; // R|W
    phLoad2.p_offset = seg2Off;
    phLoad2.p_vaddr  = seg2VA;
    phLoad2.p_paddr  = seg2VA;
    phLoad2.p_filesz = seg2Size;
    phLoad2.p_memsz  = seg2Size;
    phLoad2.p_align  = PGSZ;

    ElfPhdr phDynamic = {};
    phDynamic.p_type   = 2; // PT_DYNAMIC
    phDynamic.p_flags  = 6; // R|W
    phDynamic.p_offset = dynamicOff;
    phDynamic.p_vaddr  = dynamicVA;
    phDynamic.p_paddr  = dynamicVA;
    phDynamic.p_filesz = dynamicSize;
    phDynamic.p_memsz  = dynamicSize;
    phDynamic.p_align  = 8;

    // ── Write file ────────────────────────────────────────────────────────────
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // File starts at offset 0 (ELF header); LOAD[0] covers from offset 0 upward.
    f.write((char*)&eh, sizeof(eh));
    f.write((char*)&phInterp,  sizeof(ElfPhdr));
    f.write((char*)&phLoad1,   sizeof(ElfPhdr));
    f.write((char*)&phLoad2,   sizeof(ElfPhdr));
    f.write((char*)&phDynamic, sizeof(ElfPhdr));
    // No padding needed: .interp immediately follows headers (interpOff == hdrBytes)
    // .interp
    f.write(INTERP, interpSize);
    // .hash
    f.write((char*)hashBuf.data(), hashBuf.size() * 4);
    // .dynsym
    f.write((char*)dynsym.data(), dynsym.size() * sizeof(Elf64Sym));
    // .dynstr
    f.write((char*)dynstr.data(), dynstr.size());
    // .rela.plt
    f.write((char*)rela.data(), rela.size() * sizeof(Elf64Rela));
    // .text (immediately follows rela.plt, at textOff2)
    f.write((char*)text.data(), text.size());
    // pad to rodataOff
    {
        uint64_t cur = textOff2 + text.size();
        if (!rodata.empty() && cur < rodataOff) {
            std::vector<uint8_t> pad(rodataOff - cur, 0);
            f.write((char*)pad.data(), pad.size());
            f.write((char*)rodata.data(), rodata.size());
        }
    }
    // pad to seg2Off
    {
        uint64_t cur = rodata.empty() ? (textOff2 + text.size()) : (rodataOff + rodata.size());
        if (cur < seg2Off) {
            std::vector<uint8_t> pad(seg2Off - cur, 0);
            f.write((char*)pad.data(), pad.size());
        }
    }
    // .got.plt
    f.write((char*)gotplt.data(), gotplt.size() * sizeof(uint64_t));
    // .dynamic
    f.write((char*)dynEntries.data(), dynEntries.size() * sizeof(Elf64Dyn));
    // NA→free global slots (zero-initialized), inside the writable seg2
    if (gvarSize) {
        std::vector<uint8_t> z(gvarSize, 0);
        f.write((char*)z.data(), z.size());
    }

    f.close();
    chmod(path.c_str(), 0755);
    return true;
}


// ─── Top-Level Binary Compiler ────────────────────────────────────────────────
class BinaryCompiler {
    const AC_IR::IRProgram& prog;
    X64Emitter em;
    StringPool sp;
    ABI        abi;
    std::set<std::string> usingHeaders_; // from "using header X" in the program
    // NA→free: free-var names that must live in shared global slots, and name→slot map.
    std::set<std::string>     promotedGlobals_;
    std::map<std::string,int> gvarSlots_;
    // Bundle/class support — see the big comment at the `usesTry_`/CLASS_BEGIN pre-scan site for
    // the full rationale. classFields_: className -> ordered field names (offset = 8*index).
    // instanceClass_: instance var name -> className, updated live as CONSTRUCT calls are
    // compiled (a plain map, not a snapshot pre-scan, since which vars hold instances is a
    // per-statement fact discovered while walking the program, same as ASM's own instanceClass_).
    std::map<std::string, std::vector<std::string>> classFields_;
    std::map<std::string, std::string> instanceClass_;
    std::map<std::string, std::string> classReturnFuncs_;  // fn.name -> class name it always
                                                             // constructs+returns; see the
                                                             // prescan's own comment

    // widgets ilib: var name -> constructor kind, whole-program (populated during
    // collectExternalSymbols()'s scan, BEFORE any per-function FuncCompiler starts codegen) —
    // see FuncCompiler::widgetVarKind_'s comment for why this must be shared, not per-function.
    std::map<std::string, std::string> widgetVarKindGlobal_;
    // className -> field names ever assigned a string. `stringVarNames_` (the type-inference
    // result "self.field" needs to print correctly) is a PER-FUNCTION-COMPILATION scan
    // (`preScanStrings` only looks at the ONE function currently being compiled) — a field
    // assigned a string in `init` (a different function/FuncCompiler instance) is invisible to
    // `greet`'s own scan, so `Term.display self.name` inside `greet` printed the raw pointer
    // VALUE as a decimal integer instead of dereferencing it as a string (verified: printed
    // "4202497", not "unnamed"). Pre-scanned once across ALL of a class's methods, then seeded
    // into every method's own `stringVarNames_` at compile time (see compileFn).
    std::map<std::string, std::set<std::string>> classStringFields_;
    std::map<std::string,std::set<std::string>> stringParamHints_;
    std::map<std::string,std::set<std::string>> floatParamHints_;   // fn → params passed a float
    bool                      bundleStatic_ = false; // --static-link: splice freestanding ilib .text
    std::string               runpath_;              // ilib dirs (also where freestanding objs live)
    bool                      usesArrays_ = false; // program allocates lists/arrays
    bool                      usesIpow_   = false; // program uses `^` with a variable exponent
    bool                      usesLength_ = false; // program uses `length` on an array
    bool                      usesConcat_ = false; // program does runtime string concat
    bool                      usesStrEq_  = false; // program compares strings by content
    bool                      usesDict_   = false; // program uses string-keyed dicts
    bool                      usesAtoi_   = false; // program uses to_int/to_dec casts
    bool                      usesRand_   = false; // program uses random.number/choice
    bool                      usesAtomic_ = false; // program declares an `atomic` var
    bool                      usesSave_   = false; // program uses `save as`
    bool                      usesTry_    = false; // program uses try/catch
    bool                      usesEvents_ = false; // program uses event-listener/bind
    bool                      usesGenerators_ = false; // program has a `yield` generator

    struct FuncBounds { std::string name; uint64_t startOff, endOff; };

    // Mirror of ir_codegen's NA→free analysis: a mainloop depth-0 var that a function writes
    // must be shared (promoted) rather than a per-frame stack local.
    void computePromotedGlobals() {
        using namespace AC_IR;
        auto nameOf = [&](const IRRef& r) -> std::string {
            return (r.kind == IRRef::Kind::VAR && r.id >= 0) ? prog.symbols.getName(r.id) : "";
        };
        std::set<std::string> freeVarSet;
        int depth = 0;
        for (auto& ins : prog.globalInit) {
            if (ins.opcode == IROpcode::WHILE_BEGIN || ins.opcode == IROpcode::FOR_BEGIN) depth++;
            else if (ins.opcode == IROpcode::WHILE_END || ins.opcode == IROpcode::FOR_END) depth--;
            else if (depth == 0 && ins.result.kind == IRRef::Kind::VAR) {
                std::string n = nameOf(ins.result);
                if (!n.empty() && n.rfind("_ac_", 0) != 0) freeVarSet.insert(n);
            }
        }
        for (auto& fn : prog.functions) {
            std::set<std::string> params(fn.parameters.begin(), fn.parameters.end());
            for (auto& ins : fn.instructions) {
                if (ins.result.kind == IRRef::Kind::VAR) {
                    std::string n = nameOf(ins.result);
                    if (!n.empty() && !params.count(n) && freeVarSet.count(n))
                        promotedGlobals_.insert(n);
                }
                // `free x[, y…]` inside a fn binds those names to the free scope → promote.
                // Unconditional — a FREE_DECL is itself the explicit "this is free-scoped"
                // signal; requiring the same name to ALSO independently appear in the
                // top-level mainloop/globalInit scan (freeVarSet) was backwards, and meant a
                // var that's ONLY ever assigned via `free`/`<free>` inside a function (never
                // also written at top level) never got promoted at all — its FREE_DECL was
                // silently ignored, so it was never given a real global slot, and reading it
                // from outside the function read uninitialized/zeroed memory instead
                // (verified: `Make f func() { free x = 200; return x } ... Term.display x`
                // printed 200 from inside f() but 0 from the top level, on BNY only — every
                // other backend already promotes correctly here).
                if (ins.opcode == IROpcode::FREE_DECL)
                    for (auto& op : ins.typedOperands) {
                        std::string n = nameOf(op);
                        if (!n.empty() && !params.count(n))
                            promotedGlobals_.insert(n);
                    }
                // arr.append(v) MUTATES arr without a result var (#13) — count it as a write.
                if (ins.opcode == IROpcode::LIB_CALL && !ins.typedOperands.empty()) {
                    const auto& m = ins.typedOperands[0];
                    std::string mn;
                    if (m.kind == IRRef::Kind::CONST && m.value.type == IRType::STRING)
                        mn = std::get<std::string>(m.value.data);
                    else mn = nameOf(m);
                    auto dot = mn.rfind(".append");
                    if (dot != std::string::npos && dot == mn.size() - 7) {
                        std::string recv = mn.substr(0, dot);
                        if (!params.count(recv) && freeVarSet.count(recv))
                            promotedGlobals_.insert(recv);
                    }
                }
            }
        }
        int slot = 0;
        for (auto& n : promotedGlobals_) gvarSlots_[n] = slot++;

        // Arrays: if the program allocates any list, reserve a global slot for the heap
        // bump cursor (__heap_cursor), used by __ac_alloc__.
        auto hasAlloc = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code) if (ins.opcode == IROpcode::ALLOC) return true;
            return false;
        };
        usesArrays_ = hasAlloc(prog.globalInit);
        if (!usesArrays_)
            for (auto& fn : prog.functions) if (hasAlloc(fn.instructions)) { usesArrays_ = true; break; }
        // Runtime string concat (#6): any ADD with a string-const operand → emit __ac_concat__,
        // which needs the allocator (heap cursor slot) and __ac_strlen__.
        auto hasStrAdd = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::ADD)
                    for (auto& op : ins.typedOperands)
                        if (op.kind == IRRef::Kind::CONST && op.value.type == AC_IR::IRType::STRING)
                            return true;
            return false;
        };
        usesConcat_ = hasStrAdd(prog.globalInit);
        if (!usesConcat_)
            for (auto& fn : prog.functions) if (hasStrAdd(fn.instructions)) { usesConcat_ = true; break; }
        auto hasStrCmp = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::EQ || ins.opcode == IROpcode::NEQ)
                    for (auto& op : ins.typedOperands)
                        if (op.kind == IRRef::Kind::CONST && op.value.type == AC_IR::IRType::STRING)
                            return true;
            return false;
        };
        usesStrEq_ = hasStrCmp(prog.globalInit);
        if (!usesStrEq_)
            for (auto& fn : prog.functions) if (hasStrCmp(fn.instructions)) { usesStrEq_ = true; break; }
        auto hasDict = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::ALLOC && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == IRRef::Kind::CONST
                    && ins.typedOperands[0].value.type == AC_IR::IRType::STRING
                    && std::get<std::string>(ins.typedOperands[0].value.data) == "dict")
                    return true;
            return false;
        };
        usesDict_ = hasDict(prog.globalInit);
        if (!usesDict_)
            for (auto& fn : prog.functions) if (hasDict(fn.instructions)) { usesDict_ = true; break; }
        if (usesDict_) usesStrEq_ = true;   // dict get/set compare keys via __ac_streq__
        if (usesConcat_) { usesArrays_ = true; usesLength_ = true; }
        // The heap allocator also backs 1-char-string indexing (s[i]) and concat — always
        // reserve the cursor slot and emit it (small, and string-ness is only known later).
        usesArrays_ = true;
        gvarSlots_["__heap_cursor"] = slot++;

        // `atomic` vars: reserve one gvar slot for a real spinlock (xchg-based, no libc/syscall
        // needed — see X64Emitter::xchg_ptr_r) only if the program actually declares one.
        auto hasAtomic = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::TYPE_CAST && ins.resultType == AC_IR::IRType::ATOMIC)
                    return true;
            return false;
        };
        usesAtomic_ = hasAtomic(prog.globalInit);
        if (!usesAtomic_)
            for (auto& fn : prog.functions) if (hasAtomic(fn.instructions)) { usesAtomic_ = true; break; }
        if (usesAtomic_) gvarSlots_["__atomic_lock"] = slot++;

        // `save as <file>` — a 64KB buffer (mirrors AsmStrategy's identical fixed-size choice;
        // see its comment), lazily mmap'd via the existing __ac_alloc__ bump allocator the exact
        // same way __heap_cursor's OWN storage lazily mmaps on first use (proven pattern, not new
        // machinery). __save_buf_ptr==0 means "not yet allocated" — the append helpers check
        // that themselves.
        auto hasSave = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code) if (ins.opcode == IROpcode::SAVE_FILE) return true;
            return false;
        };
        usesSave_ = hasSave(prog.globalInit);
        if (!usesSave_)
            for (auto& fn : prog.functions) if (hasSave(fn.instructions)) { usesSave_ = true; break; }
        if (usesSave_) {
            gvarSlots_["__save_buf_ptr"] = slot++;
            gvarSlots_["__save_buf_len"] = slot++;
        }

        // Real try/catch — a hand-rolled setjmp/longjmp equivalent (BNY has no libc to call the
        // real ones). Previously `TRY_BEGIN` was a no-op and `CATCH_BEGIN` unconditionally
        // `jmp`'d PAST the catch body ("no exception occurred" hardcoded, always true) — the
        // catch body was genuinely unreachable dead code, and IDIV/MOD's zero-check always
        // hard-exited even inside a `try` (verified: `examples/showcase.ac`'s div-by-zero test
        // crashes instead of printing "caught..."). The div-by-zero usually happens inside a
        // CALLED FUNCTION, not the same function as the `try`, so a same-function jump can't
        // cross that boundary — needs a real cross-frame unwind (save RSP/RBP/catch-target at
        // TRY_BEGIN, restore + indirect-jmp at the error site), same shape as every other
        // backend's setjmp/longjmp fix this session, just hand-encoded since there's no libc
        // setjmp/longjmp to call into. `__try_stack_ptr` is a 32-slot buffer (24 bytes/slot:
        // saved RSP, RBP, catch-label address) lazily mmap'd via __ac_alloc__, same pattern as
        // __save_buf_ptr right above.
        auto hasTry = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code) if (ins.opcode == IROpcode::TRY_BEGIN) return true;
            return false;
        };
        usesTry_ = hasTry(prog.globalInit);
        if (!usesTry_)
            for (auto& fn : prog.functions) if (hasTry(fn.instructions)) { usesTry_ = true; break; }
        if (usesTry_) {
            gvarSlots_["__try_stack_ptr"] = slot++;
            gvarSlots_["__try_depth"] = slot++;
        }

        // `yield`/generators: single shared "currently active generator" slot — same design as
        // every other backend's fiber/thread implementation this session (see e.g. CStrategy's
        // ac_gen_cur in ir_codegen.cpp): fiber switches are strictly nested/sequential, never
        // concurrent, so one slot suffices. Holds the state-block pointer of whichever generator
        // is currently executing, so its prologue (loading params) and its YIELD/RETURN can find
        // their own state without threading an extra parameter through everything.
        auto hasGen = [&](const IRFunction& fn) { return fn.isGenerator; };
        usesGenerators_ = false;
        for (auto& fn : prog.functions) if (hasGen(fn)) { usesGenerators_ = true; break; }
        if (usesGenerators_) gvarSlots_["__ac_gen_cur"] = slot++;

        // Event-listener (`configure event-listener`/`on value is X`/`bind KEY to FUNC`):
        // EVENT_BIND/EVENT_TRIGGER were never handled anywhere in this file — a total no-op on
        // BNY (silently swallowed by the opcode switch's default case, no error), the one backend
        // left out of the fix that made this feature real everywhere else. Ported the same
        // fixed-64-slot parallel-array design AsmStrategy/CStrategy already use (a linear
        // strcmp/__ac_streq__ scan; 64 bindings is far past any realistic keybind count, so no
        // need to hand-roll a hash map). BNY has no raw .bss array primitive though — gvar slots
        // are individually-addressed 8-byte cells, not guaranteed contiguous in memory (the
        // static-link path lays them out in NAME-SORTED order, not slot-index order) — so the
        // array itself is a lazily __ac_alloc__'d 512-byte block (64 * 8 bytes) referenced by a
        // pointer slot, same "cursor==0 means not yet allocated" pattern as __save_buf_ptr/
        // __try_stack_ptr right above.
        auto hasEvents = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::EVENT_BIND || ins.opcode == IROpcode::EVENT_TRIGGER) return true;
            return false;
        };
        usesEvents_ = hasEvents(prog.globalInit);
        if (!usesEvents_)
            for (auto& fn : prog.functions) if (hasEvents(fn.instructions)) { usesEvents_ = true; break; }
        if (usesEvents_) {
            gvarSlots_["__ev_keys_ptr"] = slot++;
            gvarSlots_["__ev_fns_ptr"] = slot++;
            gvarSlots_["__ev_n"] = slot++;
        }

        // Bundle/class: field-order pre-scan. BNY had NO bundle/class codegen at all before this
        // (unlike ASM, which at least emitted plausible-looking method labels) — method labels
        // were just the bare method name (`greet`/`init`, no class prefix — a collision risk and,
        // separately, a naming mismatch with what any call site would need), `self.field`/
        // `instance.field` resolved via the SAME per-function local-slot mechanism as any other
        // named variable (completely disconnected from any real object memory — a field WRITE
        // vanished the instant the method returned), `c = Critter()` compiled to a plain `call
        // Critter` with no such label ever defined (hard link error — the exact failure that
        // surfaced this), and `c.greet()` silently matched nothing in emitLibCall's function-name
        // lookup (methods are named "greet", not "c.greet") — a pure no-op. Mirrors AsmStrategy's
        // own proven design exactly: malloc'd instances (8 bytes/field, declaration order),
        // `ClassName_method` labels, self/instance field access via pointer+offset.
        for (auto& fn : prog.functions) {
            if (fn.classOwner.empty()) continue;
            auto& fields = classFields_[fn.classOwner];
            for (auto& ins : fn.instructions) {
                if (ins.opcode != IROpcode::STORE_VAR && ins.opcode != IROpcode::TYPE_CAST) continue;
                IRRef tgt;
                if (ins.opcode == IROpcode::TYPE_CAST) tgt = ins.result;
                else if (ins.typedOperands.size() >= 2) tgt = ins.typedOperands[0];
                else if (ins.result.isValid()) tgt = ins.result;
                if (tgt.kind != IRRef::Kind::VAR || tgt.id < 0) continue;
                std::string nm = prog.symbols.getName(tgt.id);
                if (nm.rfind("self.", 0) != 0) continue;
                std::string field = nm.substr(5);
                if (std::find(fields.begin(), fields.end(), field) == fields.end())
                    fields.push_back(field);
                // Direct string-const assignment (`self.name = $unnamed$`) — the common case;
                // doesn't chase full data-flow (a field assigned FROM another string var/temp
                // isn't caught here), but matches the demonstrated, verified bug exactly.
                // Two STORE_VAR shapes exist (see the target-detection above): {target,source}
                // in typedOperands[0..1], OR target=ins.result with source=typedOperands[0]
                // alone. Must match whichever one this instruction actually used, or the source
                // check silently looks at the wrong operand (verified: `self.name = $unnamed$`
                // uses the SECOND shape — typedOperands.size()==1 — so checking
                // typedOperands[1] found nothing, and the field was never marked as a string).
                const IRRef* srcRef = nullptr;
                if (ins.typedOperands.size() >= 2) srcRef = &ins.typedOperands[1];
                else if (!ins.typedOperands.empty()) srcRef = &ins.typedOperands[0];
                if (srcRef && srcRef->kind == IRRef::Kind::CONST && srcRef->value.type == IRType::STRING)
                    classStringFields_[fn.classOwner].insert(field);
            }
        }
        // Ensure a class with zero self.field writes (unlikely, but matches the ASM/other-backend
        // convention of still having a valid, empty entry) still gets a map entry.
        for (auto& fn : prog.functions)
            if (!fn.classOwner.empty()) classFields_[fn.classOwner];

        // classReturnFuncs_: a free function whose every `return` traces to a var directly
        // constructed via `SomeClass()` earlier in that SAME function body — lets `q = f()`
        // be treated exactly like a direct `q = ClassName()` construct-call for
        // resolveFieldAccess's own `instanceClass_` lookup (see its comment), even when the
        // instance arrived across a function-return boundary. Real, verified bug (found+fixed
        // for ir_codegen.cpp's 7 shared-driver backends this same session, same root cause):
        // `Make f(): p = ClassName(); ...; return p` then `q = f(); q.x` printed 0 instead of
        // the real field value — instanceClass_[q] was only ever set at a DIRECT construct-
        // call site (the block just above/below this), never at an ordinary CALL whose callee
        // merely forwards a constructed instance back out.
        classReturnFuncs_.clear();
        for (auto& fn : prog.functions) {
            if (!fn.classOwner.empty() || fn.isGenerator) continue;
            std::map<std::string, std::string> varClass;
            for (auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::CALL && ins.result.kind == IRRef::Kind::VAR
                        && ins.result.id >= 0 && !ins.typedOperands.empty()
                        && ins.typedOperands[0].kind == IRRef::Kind::VAR
                        && ins.typedOperands[0].id >= 0) {
                    std::string callee = prog.symbols.getName(ins.typedOperands[0].id);
                    if (classFields_.count(callee))
                        varClass[prog.symbols.getName(ins.result.id)] = callee;
                }
            }
            std::string retClass; bool any = false, consistent = true;
            for (auto& ins : fn.instructions) {
                if (ins.opcode != IROpcode::RETURN || ins.typedOperands.empty()) continue;
                const auto& rv = ins.typedOperands[0];
                if (rv.kind != IRRef::Kind::VAR || rv.id < 0) { consistent = false; break; }
                auto it = varClass.find(prog.symbols.getName(rv.id));
                if (it == varClass.end()) { consistent = false; break; }
                if (!any) { retClass = it->second; any = true; }
                else if (retClass != it->second) { consistent = false; break; }
            }
            if (any && consistent) classReturnFuncs_[fn.name] = retClass;
        }

        // classParamTypes discovery (mirrors ir_codegen.cpp's UnifiedIRCodeGen driver fix — see
        // its own comment for the full rationale; found+fixed there first this same session,
        // same root cause, verified real: `Make show func(p): Term.display p.x` called as
        // `show(pt)` printed GARBAGE — not a compile error, since BNY has no static parameter
        // type to get wrong, but resolveFieldAccess's instanceClass_ lookup for "p" found
        // nothing, so `p.x` silently read an unrelated, never-written flat slot literally named
        // "p.x" instead of dereferencing p as a pointer into the real struct). Unlike
        // ir_codegen.cpp's fix, this needs no two-pass trick — `prog` here is already the
        // complete, fully-lowered program (ir.cpp finished long before this file runs), so one
        // ordinary whole-program scan is enough, same as classReturnFuncs_ just above.
        {
            std::map<std::string, std::string> varClassGlobal;
            auto scanConstructs = [&](const std::vector<IRInstruction>& instrs) {
                for (auto& ins : instrs) {
                    if (ins.opcode != IROpcode::CALL || ins.result.kind != IRRef::Kind::VAR
                            || ins.result.id < 0 || ins.typedOperands.empty()
                            || ins.typedOperands[0].kind != IRRef::Kind::VAR
                            || ins.typedOperands[0].id < 0) continue;
                    std::string callee = prog.symbols.getName(ins.typedOperands[0].id);
                    if (classFields_.count(callee))
                        varClassGlobal[prog.symbols.getName(ins.result.id)] = callee;
                    else if (classReturnFuncs_.count(callee))
                        varClassGlobal[prog.symbols.getName(ins.result.id)] = classReturnFuncs_[callee];
                }
            };
            for (auto& fn : prog.functions) scanConstructs(fn.instructions);
            scanConstructs(prog.globalInit);
            scanConstructs(prog.dataSection);
            scanConstructs(prog.mainSection);

            std::map<std::string, std::map<int, std::string>> classParamTypes;
            auto scanCalls = [&](const std::vector<IRInstruction>& instrs) {
                for (auto& ins : instrs) {
                    if ((ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL)
                            || ins.typedOperands.empty()
                            || ins.typedOperands[0].kind != IRRef::Kind::VAR) continue;
                    std::string calleeName = prog.symbols.getName(ins.typedOperands[0].id);
                    for (size_t ai = 1; ai < ins.typedOperands.size(); ai++) {
                        const IRRef& arg = ins.typedOperands[ai];
                        if (arg.kind != IRRef::Kind::VAR) continue;
                        auto vc = varClassGlobal.find(prog.symbols.getName(arg.id));
                        if (vc != varClassGlobal.end())
                            classParamTypes[calleeName][(int)(ai - 1)] = vc->second;
                    }
                }
            };
            for (auto& fn : prog.functions) scanCalls(fn.instructions);
            scanCalls(prog.globalInit);
            scanCalls(prog.dataSection);
            scanCalls(prog.mainSection);

            // Merge straight into instanceClass_ (a flat, unscoped var->class map, same
            // convention as ir_codegen.cpp's classInstanceVars_/classInstanceVarNames_) using
            // each matched function's REAL parameter names. Free functions only — fn.parameters
            // for a method has an extra leading "self" the caller never writes, which would
            // shift every index by one; out of scope for this fix, same as the shared driver's.
            for (auto& fn : prog.functions) {
                if (!fn.classOwner.empty()) continue;
                auto cpIt = classParamTypes.find(fn.name);
                if (cpIt == classParamTypes.end()) continue;
                for (auto& [idx, cls] : cpIt->second) {
                    if (idx < 0 || (size_t)idx >= fn.parameters.size()) continue;
                    instanceClass_[fn.parameters[(size_t)idx]] = cls;
                }
            }
        }

        // Builtin helper calls (ac_ipow from `^`/ptm/ptd, ac_length from `length`)
        // need their machine-code helpers emitted.
        auto scanHelpers = [&](const std::vector<IRInstruction>& code) {
            for (auto& ins : code)
                if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == IRRef::Kind::VAR) {
                    std::string fn = nameOf(ins.typedOperands[0]);
                    if (fn == "ac_ipow")   usesIpow_   = true;
                    else if (fn == "ac_length") usesLength_ = true;
                }
            for (auto& ins : code)
                if (ins.opcode == IROpcode::TYPE_CAST) { usesAtoi_ = true; break; }
            for (auto& ins : code)
                if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == IRRef::Kind::VAR) {
                    std::string fn = nameOf(ins.typedOperands[0]);
                    if (fn == "random.number" || fn == "random.choice") { usesRand_ = true; }
                }
        };
        scanHelpers(prog.globalInit);
        for (auto& fn : prog.functions) scanHelpers(fn.instructions);
    }

    void computeStringParamHints() {
        using namespace AC_IR;
        std::map<std::string,const IRFunction*> funcs;
        for (const auto& fn : prog.functions) funcs[fn.name] = &fn;

        auto refName = [&](const IRRef& r) -> std::string {
            if (r.kind == IRRef::Kind::VAR && r.id >= 0) return prog.symbols.getName(r.id);
            if (r.kind == IRRef::Kind::TEMP && r.id >= 0) return "t_" + std::to_string(r.id);
            return "";
        };
        auto funcNameOf = [&](const IRRef& r) -> std::string {
            if ((r.kind == IRRef::Kind::FUNCTION || r.kind == IRRef::Kind::VAR) && r.id >= 0)
                return prog.symbols.getName(r.id);
            if (r.value.type == IRType::STRING) return std::get<std::string>(r.value.data);
            return "";
        };
        auto scanBlock = [&](const std::vector<IRInstruction>& code,
                             std::set<std::string> strings) {
            bool changed = false;
            for (const auto& ins : code) {
                if (ins.opcode == IROpcode::INPUT && ins.result.isValid()) {
                    std::string n = refName(ins.result);
                    if (!n.empty()) strings.insert(n);
                } else if ((ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL)
                           && !ins.typedOperands.empty()
                           && (ins.result.isValid() || ins.typedOperands.size() >= 2)) {
                    bool resultForm = ins.result.isValid();
                    const IRRef& dst = resultForm ? ins.result : ins.typedOperands[0];
                    const IRRef& src = resultForm ? ins.typedOperands[0] : ins.typedOperands[1];
                    bool isString = (src.kind == IRRef::Kind::CONST && src.value.type == IRType::STRING)
                                 || strings.count(refName(src));
                    std::string dn = refName(dst);
                    if (isString && !dn.empty()) strings.insert(dn);
                } else if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL)
                           && !ins.typedOperands.empty()) {
                    std::string callee = funcNameOf(ins.typedOperands[0]);
                    auto fit = funcs.find(callee);
                    if (fit == funcs.end()) continue;
                    const auto& params = fit->second->parameters;
                    for (size_t i = 1; i < ins.typedOperands.size() && i <= params.size(); i++) {
                        const IRRef& arg = ins.typedOperands[i];
                        bool isString = (arg.kind == IRRef::Kind::CONST && arg.value.type == IRType::STRING)
                                     || strings.count(refName(arg));
                        if (isString && stringParamHints_[callee].insert(params[i - 1]).second)
                            changed = true;
                    }
                }
            }
            return changed;
        };

        bool changed = true;
        while (changed) {
            changed = false;
            changed |= scanBlock(prog.globalInit, {});
            for (const auto& fn : prog.functions) {
                std::set<std::string> strings = stringParamHints_[fn.name];
                changed |= scanBlock(fn.instructions, strings);
            }
        }
    }

    // Float-param inference — the analog of computeStringParamHints for doubles. A user function
    // param that a caller passes a float to must be loaded as a double (movq), not sign-converted
    // (cvtsi2sd) from its bits. Without this, `nsqrt(2.0)` fed x's raw double-bits through cvtsi2sd
    // → garbage. Float-ness of an argument is resolved with the SAME authority (acCallReturnsFloat)
    // used everywhere else, plus the usual float producers (float consts, DIV/FDIV, float arithmetic).
    void computeFloatParamHints() {
        using namespace AC_IR;
        std::map<std::string,const IRFunction*> funcs;
        for (const auto& fn : prog.functions) funcs[fn.name] = &fn;
        auto refName = [&](const IRRef& r) -> std::string {
            return (r.kind == IRRef::Kind::VAR && r.id >= 0) ? prog.symbols.getName(r.id) : "";
        };
        auto funcNameOf = [&](const IRRef& r) -> std::string {
            if ((r.kind == IRRef::Kind::FUNCTION || r.kind == IRRef::Kind::VAR) && r.id >= 0)
                return prog.symbols.getName(r.id);
            return "";
        };
        auto scanBlock = [&](const std::vector<IRInstruction>& code, std::set<std::string> fvars) {
            bool changed = false;
            std::set<int> ftemps;
            auto isFloat = [&](const IRRef& r) -> bool {
                if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
                if (r.kind == IRRef::Kind::TEMP)  return ftemps.count(r.id) > 0;
                if (r.kind == IRRef::Kind::VAR)   { std::string n = refName(r); return !n.empty() && fvars.count(n) > 0; }
                return false;
            };
            auto mark = [&](const IRRef& r) {
                if (r.kind == IRRef::Kind::TEMP) ftemps.insert(r.id);
                else if (r.kind == IRRef::Kind::VAR) { std::string n = refName(r); if (!n.empty()) fvars.insert(n); }
            };
            for (const auto& ins : code) {
                if ((ins.opcode == IROpcode::DIV || ins.opcode == IROpcode::FDIV) && ins.result.isValid())
                    mark(ins.result);
                if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                  || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::PMUL)
                    && ins.result.isValid() && ins.typedOperands.size() >= 2
                    && (isFloat(ins.typedOperands[0]) || isFloat(ins.typedOperands[1])))
                    mark(ins.result);
                if ((ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL
                  || ins.opcode == IROpcode::LOAD_CONST)) {
                    bool hasResult = ins.result.isValid() && !ins.typedOperands.empty();
                    IRRef val = hasResult ? ins.typedOperands[0]
                              : (ins.typedOperands.size() >= 2 ? ins.typedOperands[1] : IRRef());
                    if (isFloat(val)) mark(hasResult ? ins.result
                              : (ins.typedOperands.size() >= 2 ? ins.typedOperands[0] : IRRef()));
                }
                if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL)
                        && !ins.typedOperands.empty()) {
                    std::string callee = funcNameOf(ins.typedOperands[0]);
                    if (ins.result.isValid() && acCallReturnsFloat(callee))
                        mark(ins.result);
                    auto fit = funcs.find(callee);
                    if (fit != funcs.end()) {
                        const auto& params = fit->second->parameters;
                        for (size_t i = 1; i < ins.typedOperands.size() && i <= params.size(); i++)
                            if (isFloat(ins.typedOperands[i])
                                && floatParamHints_[callee].insert(params[i - 1]).second)
                                changed = true;
                    }
                }
            }
            return changed;
        };
        bool changed = true;
        while (changed) {
            changed = false;
            changed |= scanBlock(prog.globalInit, {});
            for (const auto& fn : prog.functions)
                changed |= scanBlock(fn.instructions, floatParamHints_[fn.name]);
        }
    }

    // For Windows: emit IAT stubs so calls to function names resolve
    void emitIATStubs(uint32_t textRVA, uint32_t idataRVA, uint32_t iatOff) {
        // Stubs: for each imported function, emit JMP [RIP + rel]
        // This must be called AFTER all code is emitted so stub offsets are known
        // Function order: 0=GetStdHandle, 1=WriteConsoleA, 2=ExitProcess
        struct Stub { const char* name; int idx; };
        Stub stubs[] = {{"GetStdHandle",0},{"WriteConsoleA",1},{"ExitProcess",2}};
        for (auto& s : stubs) {
            em.label(s.name);
            // JMP [RIP + offset_to_iat]
            // After this instruction (6 bytes), RIP = textRVA + current_offset + 6
            // IAT entry VA = ImageBase + idataRVA + iatOff + s.idx*8
            // But we don't have ImageBase here easily. Use a rel32 fixup to a label.
            // Emit: FF 25 <rel32> where the fixup label is the IAT entry label
            std::string iatLabel = "__iat_" + std::string(s.name) + "__";
            em.call_rip_rel(iatLabel);  // actually JMP; we'll use FF 15 (call indirect)
            // Note: FF 15 is CALL [rip+rel32], FF 25 is JMP [rip+rel32]
            // For stubs, JMP is better. Let me emit FF 25 directly:
            // We already emitted FF 15 above. Override: rewind and emit FF 25 instead.
            // Actually let me just use CALL stubs (they'll work as they restore rip)
        }
        // For simplicity, the IAT labels are defined in the idata section.
        // This requires knowing the text section's address at emit time.
        // We use a post-compile fixup approach: after building the ELF/PE,
        // patch stub rel32 values to point into .idata.
    }

    // Scan IR for external symbol calls (math.sin, math.pi, etc.)
    std::vector<ExtSym> collectExternalSymbols() {
        std::set<std::string> seen;
        std::vector<ExtSym> result;
        auto addSym = [&](const std::string& irName) {
            if (irName.empty()) return;
            if (seen.count(irName)) return;
            seen.insert(irName);
            std::string expName = normalizeExtSym(irName);
            if (expName.empty()) expName = irName;
            result.push_back({irName, expName, libForSym(expName)});
        };
        // One-time GTK init (see compileGlobal's matching comment for the full "every widget
        // test this session was actually hitting THIS bug" story) — registered here so its PLT
        // stub exists whenever compileGlobal's own call to it needs one.
        if (prog.importedLibs.count("widgets")) addSym("ac_widgets_init");
        // widgets: `use ilib widgets` exposes bare ctor calls (`display(root,...)`) and
        // dot-method calls on the returned handle (`lang_drop.add(...)`) — neither matches
        // any of the dotted-namespace patterns below, so this static scan (which decides the
        // PLT stub set BEFORE codegen runs) needs its own tracking, mirroring FuncCompiler's
        // widgetVarKind_ but simplified to just "what real symbols get called" (see that
        // member's comment for the full "undefined label" story this closes).
        auto& wKind = widgetVarKindGlobal_;
        // Pre-pass: populate wKind from EVERY function's widget ctors before the main scan below
        // runs (which also needs wKind to resolve method calls) — a widget var's ctor and its
        // method calls routinely live in DIFFERENT functions scanned in DECLARATION order (e.g.
        // applicant_form.ac's `OnSubmit` callback, which calls `pos_drop.get()`, is declared
        // BEFORE `<mainloop>`'s `pos_drop = dropdown(...)` ctor) — without this separate pass,
        // a single forward scan would miss `pos_drop`'s kind for every method call that happens
        // to be scanned first.
        auto prepassCtors = [&](const std::vector<AC_IR::IRInstruction>& instrs) {
            for (auto& ins : instrs) {
                if (ins.opcode != AC_IR::IROpcode::CALL || ins.typedOperands.empty()) continue;
                auto& r = ins.typedOperands[0];
                std::string irName;
                if (r.id >= 0) irName = prog.symbols.getName(r.id);
                if (irName.empty() && r.value.type == AC_IR::IRType::STRING)
                    irName = std::get<std::string>(r.value.data);
                if (bnyIsWidgetCtorName(irName) && ins.result.isValid()
                        && ins.result.kind == AC_IR::IRRef::Kind::VAR && ins.result.id >= 0) {
                    std::string vn = prog.symbols.getName(ins.result.id);
                    if (!vn.empty()) wKind[vn] = irName;
                }
            }
        };
        for (auto& fn : prog.functions) prepassCtors(fn.instructions);
        prepassCtors(prog.globalInit);
        // Shared by both the CALL-opcode dotted check (zero-arg method WITH parens, e.g.
        // `pos_drop.get()`) and the LIB_CALL check (any dotted call WITH args) — see
        // FuncCompiler::bnyWidgetCtor's sibling comment for why both opcodes need coverage.
        // Returns true if `mname` was a recognized widget method call (regardless of whether it
        // needed a real symbol — e.g. a bare `.pack` with no args still "handles" the dotted name).
        auto tryWidgetMethodSym = [&](const std::string& mname, int argc) -> bool {
            auto dot = mname.find('.');
            if (dot == std::string::npos) return false;
            std::string recv = mname.substr(0, dot);
            std::string meth = mname.substr(dot + 1);
            auto wit = wKind.find(recv);
            if (wit == wKind.end()) return false;
            const std::string& kind = wit->second;
            if (meth == "pack") {
                if (argc >= 2) addSym("ac_widgets_pack_spaced");
                else { std::string p = bnyWidgetPackFn(kind); if (!p.empty()) addSym(p); }
            } else if (meth == "mainloop" && kind == "Screen") addSym("ac_widgets_screen_mainloop");
            else if (meth == "update" && kind == "Screen") addSym("ac_widgets_screen_update");
            else if (meth == "destroy" && kind == "Screen") addSym("ac_widgets_screen_destroy");
            else if (meth == "dimensions" && kind == "Screen") addSym("ac_widgets_screen_dimensions");
            else if (meth == "add")
                addSym(kind == "dropdown" ? "ac_widgets_dropdown_add" :
                       kind == "listbox"  ? "ac_widgets_listbox_add"  :
                       kind == "table"    ? "ac_widgets_table_add"    : "ac_widgets_add");
            else if (meth == "set" || meth == "config") addSym(bnyWidgetSetFn(kind));
            else if (meth == "get") addSym(bnyWidgetGetFn(kind));
            else if (meth == "on_click" && kind == "btn") addSym("ac_widgets_btn_on_click");
            else if (meth == "add_tab" && kind == "tabs") addSym("ac_widgets_tabs_add_tab");
            else if (meth == "clear" && kind == "sketch") addSym("ac_widgets_sketch_clear");
            else if ((meth == "line" || meth == "rect" || meth == "circle" || meth == "text_at") && kind == "sketch")
                addSym("ac_widgets_sketch_" + (meth == "text_at" ? "text" : meth));
            else if (meth == "write" && kind == "textbox") addSym("ac_widgets_textbox_write");
            else if (meth == "find" && kind == "textbox") addSym("ac_widgets_textbox_find");
            else if (meth == "fix" && kind == "textbox") addSym("ac_widgets_textbox_fix");
            else return false;
            return true;
        };
        auto check = [&](const std::vector<AC_IR::IRInstruction>& instrs) {
            for (auto& ins : instrs) {
                // CALL: math.sin(x), math.sqrt(x), or bare gcd() with "using header math"
                if (ins.opcode == AC_IR::IROpcode::CALL) {
                    if (ins.typedOperands.empty()) continue;
                    auto& r = ins.typedOperands[0];
                    std::string irName;
                    if (r.id >= 0) irName = prog.symbols.getName(r.id);
                    if (irName.empty() && r.value.type == AC_IR::IRType::STRING)
                        irName = std::get<std::string>(r.value.data);
                    if (bnyIsWidgetCtorName(irName)) {
                        addSym(bnyWidgetNewFn(irName));
                        std::string packFn = bnyWidgetPackFn(irName);
                        if (!packFn.empty()) addSym(packFn);
                        // Registered unconditionally (cheap, matches this file's existing
                        // "unconditional = always-consistent" precedent — see emitConcatLinux's
                        // comment) rather than trying to detect a trailing `lazy` arg here too —
                        // simplest way to guarantee the PLT stub exists whenever bnyWidgetCtor's
                        // own isLazy branch (same ctor call, real codegen pass) needs it.
                        addSym("ac_widgets_set_lazy");
                        if (irName == "btn" && ins.typedOperands.size() > 3)
                            addSym("ac_widgets_btn_on_click");
                        if (ins.result.isValid() && ins.result.kind == AC_IR::IRRef::Kind::VAR
                                && ins.result.id >= 0) {
                            std::string vn = prog.symbols.getName(ins.result.id);
                            if (!vn.empty()) wKind[vn] = irName;
                        }
                        continue;
                    }
                    // Zero-arg dotted widget method call WITH parens (`pos_drop.get()`) lowers
                    // through this plain CALL opcode instead of LIB_CALL — see
                    // FuncCompiler::bnyWidgetCtor's matching comment for the full story.
                    if (tryWidgetMethodSym(irName, (int)ins.typedOperands.size() - 1)) continue;
                    if (irName.find("math.") != std::string::npos ||
                        irName.find("web.") != std::string::npos ||
                        irName.rfind("server.", 0) == 0 ||
                        irName.find("ml.") != std::string::npos ||
                        irName.rfind("os.", 0) == 0 ||
                        irName.rfind("regex.", 0) == 0 ||
                        irName.rfind("stringm.", 0) == 0 ||
                        irName.rfind("ncpu.", 0) == 0 ||
                        irName.rfind("maudio.", 0) == 0 ||
                        // camera/sidebar/screen/aczip were missing from this allowlist
                        // entirely — normalizeExtSym/libForSym both already had real
                        // entries for them, but collectExternalSymbols (this pre-pass,
                        // which decides which PLT stubs to allocate BEFORE codegen runs)
                        // never called addSym() for them, so every call was an unresolved
                        // label at link time regardless (verified: camera_demo.ac/
                        // aczip_demo.ac both hard "undefined label" on every single call).
                        irName.rfind("camera.", 0) == 0 ||
                        irName.rfind("sidebar.", 0) == 0 ||
                        irName.rfind("screen.", 0) == 0 ||
                        irName.rfind("aczip.", 0) == 0) {
                        addSym(irName);
                    } else if (isNativeCpuPtrSym(irName)) {
                        // native-cpu's carried-over ptr_* functions are called bare (no dotted
                        // receiver, matching the existing pointers-library convention) — `use ilib
                        // native-cpu` doesn't populate usingHeaders_, so they need their own
                        // explicit bare-name recognition here.
                        addSym(irName);
                    } else if (!usingHeaders_.empty() && irName.find('.') == std::string::npos && !irName.empty()) {
                        // Bare call + using header — check it's not user-defined
                        bool isUserDef = false;
                        for (auto& fn : prog.functions) if (fn.name == irName) { isUserDef = true; break; }
                        if (!isUserDef)
                            addSym(*usingHeaders_.begin() + "." + irName);
                    }
                }
                // LIB_CALL: Term.X(args) → needs math.X from libacmath.so
                else if (ins.opcode == AC_IR::IROpcode::LIB_CALL &&
                         !ins.typedOperands.empty()) {
                    std::string mname;
                    auto& m = ins.typedOperands[0];
                    if (m.kind == AC_IR::IRRef::Kind::VAR && m.id >= 0)
                        mname = prog.symbols.getName(m.id);
                    else if (m.kind == AC_IR::IRRef::Kind::CONST &&
                             m.value.type == AC_IR::IRType::STRING)
                        mname = std::get<std::string>(m.value.data);
                    if (tryWidgetMethodSym(mname, (int)ins.typedOperands.size() - 1)) continue;
                    if (mname.rfind("ml.", 0) == 0 || mname.rfind("os.", 0) == 0 ||
                        mname.rfind("regex.", 0) == 0 || mname.rfind("stringm.", 0) == 0 ||
                        mname.rfind("web.", 0) == 0 || mname.rfind("server.", 0) == 0 ||
                        mname.rfind("ncpu.", 0) == 0 || mname.rfind("maudio.", 0) == 0 ||
                        // Same missing-prefix gap as the CALL-opcode path above.
                        mname.rfind("camera.", 0) == 0 || mname.rfind("sidebar.", 0) == 0 ||
                        mname.rfind("screen.", 0) == 0 || mname.rfind("aczip.", 0) == 0) {
                        addSym(mname);
                        continue;
                    }
                    if (isNativeCpuPtrSym(mname)) { addSym(mname); continue; }
                    if (mname.rfind("Term.", 0) == 0 && mname.size() > 5) {
                        std::string fname = mname.substr(5);
                        if (fname != "display" && fname != "ask")
                            addSym("math." + fname);
                    }
                }
                // STORE_VAR: d = math.pi / math.e / math.phi etc.
                else if (ins.opcode == AC_IR::IROpcode::STORE_VAR) {
                    auto checkRef = [&](const AC_IR::IRRef& r) {
                        if (r.kind != AC_IR::IRRef::Kind::VAR || r.id < 0) return;
                        std::string n = prog.symbols.getName(r.id);
                        if (!mathConstantFunc(n).empty()) addSym(n);
                    };
                    if (!ins.typedOperands.empty()) checkRef(ins.typedOperands[0]);
                    if (ins.result.isValid()) checkRef(ins.result);
                }
            }
        };
        for (auto& fn : prog.functions) check(fn.instructions);
        check(prog.globalInit);
        // Include ac_print_double when any float-returning math call OR float constant is present
        bool needsPrintDouble = false;
        for (auto& s : result)
            if (isFloatReturningCall(s.irName)) { needsPrintDouble = true; break; }
        if (!needsPrintDouble) {
            // Check for float-valued constants (e.g. from folded division results)
            auto hasFloat = [](const std::vector<AC_IR::IRInstruction>& instrs) {
                for (auto& i : instrs) {
                    if (i.opcode == AC_IR::IROpcode::DIV) return true;
                    for (auto& op : i.typedOperands)
                        if (op.kind == AC_IR::IRRef::Kind::CONST && op.value.type == AC_IR::IRType::FLOAT)
                            return true;
                    if (i.result.kind == AC_IR::IRRef::Kind::CONST && i.result.value.type == AC_IR::IRType::FLOAT)
                        return true;
                }
                return false;
            };
            if (hasFloat(prog.globalInit)) needsPrintDouble = true;
            else for (auto& fn : prog.functions)
                if (hasFloat(fn.instructions)) { needsPrintDouble = true; break; }
        }
        // ac_print_double is now inlined (emitPrintDoubleLinux) — no PLT needed on Linux
        // Only add PLT entry if explicitly requested via libacmath (e.g. math constants)
        (void)needsPrintDouble;
        
        // NO unconditional libc. BNY's own runtime uses raw syscalls (__ac_print_*,
        // __ac_input_str__, __ac_strlen__) and ilib .so's are resolved as NEEDED deps by
        // ld-linux — none of printf/scanf/strlen/dlopen/dlsym is ever actually called.
        // Force-adding them made `result` non-empty on every binary, so the fully-static
        // path was dead and every trivial program dragged in libc.so.6 + ld-linux.
        //
        // Policy: a program with NO imports leaves `result` empty → static, ZERO-dependency
        // ELF. A program that `use`s an ilib gets exactly that ilib's .so as its dependency
        // (added above by the CALL/LIB_CALL scan) — nothing more.
        return result;
    }

public:
    BinaryCompiler(const AC_IR::IRProgram& p, bool targetWindows = false)
        : prog(p), abi((g_bnyTargetWindows = targetWindows, host_abi()))
    {}

    bool compile(const std::string& outPath,
                 bool debugInfo = false, const std::string& srcPath = "",
                 const std::string& runpath = "", bool bundleStatic = false) {
        bundleStatic_ = bundleStatic;
        runpath_ = runpath;
        if (needsCrossCompilation()) {
            // BNY's direct emitter is x86-64. The CLI routes ARM through AC->C before calling here.
            std::cerr << "AC->BNY direct emitter is x86-64 only; use the CLI ARM route through C.\n";
            return false;
        }
        
        const uint64_t BASE = 0x400000ULL;
        const uint64_t PGSZ = 0x1000ULL;

        // Pre-scan for "using header X" imports
        usingHeaders_.clear();
        auto scanUsing = [&](const std::vector<AC_IR::IRInstruction>& insns) {
            for (auto& ins : insns) {
                if (ins.opcode != AC_IR::IROpcode::LIB_CALL || ins.typedOperands.empty()) continue;
                if (ins.typedOperands[0].value.type != AC_IR::IRType::STRING) continue;
                std::string m = std::get<std::string>(ins.typedOperands[0].value.data);
                if ((m == "import") && ins.typedOperands.size() > 1) {
                    if (ins.typedOperands[1].value.type != AC_IR::IRType::STRING) continue;
                    std::string raw = std::get<std::string>(ins.typedOperands[1].value.data);
                    if (raw.rfind("using:", 0) == 0)
                        usingHeaders_.insert(raw.substr(6));
                }
            }
        };
        scanUsing(prog.globalInit);
        for (auto& fn : prog.functions) scanUsing(fn.instructions);

        // Collect external symbols before emitting any code. ELF-specific (ilib .so
        // DT_NEEDED deps) — the Windows path uses a completely separate mechanism
        // (PE import table, wired further down) since ilibs aren't built for Windows
        // at all yet; that's real follow-up work, not something to fake here.
        std::vector<ExtSym> extSyms;
        if (!g_bnyTargetWindows) extSyms = collectExternalSymbols();
        // --static-link: can we SPLICE freestanding ilib code instead of dynamic-linking .so's?
        // Load every freestanding object under the program's ilib dirs and check it defines each
        // needed external symbol. If ALL are covered (and none carry relocations we can't yet apply),
        // bundle statically → one zero-dep binary, no gcc/ld, no DT_NEEDED. Else fall back to dynamic.
        std::vector<SplicedObject> spliceObjs;
        bool willSplice = false;
        if (bundleStatic_ && !extSyms.empty()) {
            std::stringstream ss(runpath_);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                if (dir.empty()) continue;
                SplicedObject o = readFreestandingObject(dir + "/freestanding/fsmath.o");
                if (o.ok && !o.hadRelocs) spliceObjs.push_back(std::move(o));
            }
            willSplice = true;
            for (auto& es : extSyms) {
                bool prov = false;
                for (auto& o : spliceObjs) if (o.symOffset.count(es.exportName)) { prov = true; break; }
                if (!prov) { willSplice = false; break; }
            }
            if (!willSplice)
                std::cerr << "Note: --static-link fell back to dynamic linking — a used ilib function "
                             "has no freestanding implementation yet (only integer-math is so far).\n";
        }
        bool dynamic = !extSyms.empty() && !willSplice;

        // Decide promoted free-var slots AND whether the heap is needed — must run before
        // emitting the allocator helper (which depends on usesArrays_ / __heap_cursor slot).
        computePromotedGlobals();
        computeStringParamHints();
        computeFloatParamHints();

        // Emit print helpers
        emitPrintIntLinux(em);
        emitPrintStrLinux(em);
        emitPrintCStrLinux(em);
        emitPrintDoubleLinux(em);
        emitInputIntLinux(em);
        emitInputStrLinux(em, sp);
        if (usesArrays_) { emitAllocLinux(em, gvarSlots_["__heap_cursor"]); emitAppendLinux(em); emitPrintArrLinux(em); }
        if (usesAtomic_) { emitAtomicLockLinux(em, gvarSlots_["__atomic_lock"]); emitAtomicUnlockLinux(em, gvarSlots_["__atomic_lock"]); }
        if (usesSave_) {
            emitSaveAppendCStrLinux(em, gvarSlots_["__save_buf_ptr"], gvarSlots_["__save_buf_len"]);
            emitSaveAppendIntLinux(em);
            emitSaveAppendDoubleLinux(em, gvarSlots_["__save_buf_ptr"], gvarSlots_["__save_buf_len"]);
            emitSaveFileLinux(em, gvarSlots_["__save_buf_ptr"], gvarSlots_["__save_buf_len"]);
        }
        if (usesIpow_) emitIpowLinux(em);
        if (usesEvents_) {
            emitEventBindLinux(em, gvarSlots_["__ev_keys_ptr"], gvarSlots_["__ev_fns_ptr"], gvarSlots_["__ev_n"]);
            emitEventTriggerLinux(em, gvarSlots_["__ev_keys_ptr"], gvarSlots_["__ev_fns_ptr"], gvarSlots_["__ev_n"]);
        }
        emitLengthLinux(em);  // always: ~30 bytes; __ac_strlen__ backs string-FOR/concat/indexing
        // Concat + streq emitted ALWAYS: the constant folder can fold away the only const-string
        // ADD (turning the usesConcat_ gate off) while a string-VAR ADD still emits a __ac_concat__
        // call at codegen → undefined label. Both helpers are tiny; unconditional = always-consistent.
        emitConcatLinux(em);
        emitStrEqLinux(em);
        emitItoaLinux(em);
        emitWidgetTrampolinesLinux(em);
        if (usesDict_) { emitDictLinux(em, sp); emitDictSetLinux(em); }
        if (usesAtoi_) emitAtoiLinux(em);
        if (usesRand_) emitRandLinux(em);

        // Emit PLT stubs on Linux when there are external symbols
        if (dynamic) {
            em.emitPLT0();
            for (int i = 0; i < (int)extSyms.size(); i++)
                em.emitPLTStub(i, extSyms[i].irName);
        }

        // Shared set: user functions known to return float values
        std::set<std::string> floatFuncs;
        std::set<std::string> stringFuncs;   // user fns returning strings (#6)
        std::set<std::string> arrayFuncs;    // user fns returning list blocks

        // Emit user-defined functions; record start/end offsets for DWARF
        std::vector<FuncBounds> funcBounds;
        for (auto& fn : prog.functions) {
            uint64_t startOff = em.pos();
            FuncCompiler<X64Emitter> fc(prog, em, sp, abi, false, usingHeaders_);
            fc.floatFuncs_ = &floatFuncs;
            fc.stringFuncs_ = &stringFuncs;
            fc.arrayFuncs_ = &arrayFuncs;
            fc.forcedStringParams_ = stringParamHints_[fn.name];
            fc.forcedFloatParams_ = floatParamHints_[fn.name];
            fc.promotedGlobals_ = &promotedGlobals_;
            fc.gvarSlots_ = &gvarSlots_;
            fc.usesSave_ = usesSave_;
            fc.usesTry_ = usesTry_;
            fc.usesGenerators_ = usesGenerators_;
            fc.classFields_ = &classFields_;
            fc.instanceClass_ = &instanceClass_;
            fc.classStringFields_ = &classStringFields_;
            fc.classReturnFuncs_ = &classReturnFuncs_;
            fc.widgetVarKind_ = &widgetVarKindGlobal_;
            if (fn.isGenerator) fc.compileGeneratorFn(fn);
            else fc.compileFn(fn);
            funcBounds.push_back({fn.name, startOff, em.pos()});
        }

        // Emit global section as _start
        {
            uint64_t startOff = em.pos();
            FuncCompiler<X64Emitter> gc(prog, em, sp, abi, true, usingHeaders_);
            gc.floatFuncs_ = &floatFuncs;
            gc.stringFuncs_ = &stringFuncs;
            gc.arrayFuncs_ = &arrayFuncs;
            gc.promotedGlobals_ = &promotedGlobals_;
            gc.gvarSlots_ = &gvarSlots_;
            gc.usesSave_ = usesSave_;
            gc.usesTry_ = usesTry_;
            gc.usesGenerators_ = usesGenerators_;
            gc.classFields_ = &classFields_;
            gc.instanceClass_ = &instanceClass_;
            gc.classStringFields_ = &classStringFields_;
            gc.classReturnFuncs_ = &classReturnFuncs_;
            gc.widgetVarKind_ = &widgetVarKindGlobal_;
            gc.compileGlobal(prog.globalInit);
            funcBounds.push_back({"_start", startOff, em.pos()});
        }

        // Build string pool
        std::vector<size_t> strOffsets;
        std::vector<uint8_t> rodata = sp.build(strOffsets);

        // NA→free: reserve one zero-initialized 8-byte slot per promoted free var.
        // Static path: appended after the strings in the (made-writable) rodata segment.
        // Dynamic path: placed in the writable seg2 instead (handled below), since the
        // dynamic rodata shares the read-only RX segment.
        std::vector<size_t> gvarOffsets(gvarSlots_.size());
        if (!dynamic) {
            for (auto& [name, slot] : gvarSlots_) {
                gvarOffsets[slot] = rodata.size();
                for (int b = 0; b < 8; b++) rodata.push_back(0);
            }
        }

        // --static-link: splice the freestanding ilib .text in and bind each ilib call to it. Done
        // BEFORE applyFixups so em.call("math.gcd") resolves to the spliced code (not a PLT stub).
        if (willSplice) {
            std::map<std::string, size_t> exportAddr;   // export symbol → offset in the final text
            for (auto& o : spliceObjs) {
                size_t base = em.pos();
                em.appendSplicedText(o.text);
                for (auto& [name, off] : o.symOffset) exportAddr[name] = base + off;
            }
            for (auto& es : extSyms) {
                auto it = exportAddr.find(es.exportName);
                if (it != exportAddr.end()) em.defineLabelAt(es.irName, it->second);
            }
        }

        // Windows: bind every Win32 API call (currently just ExitProcess, from
        // emitHalt()) to its IAT slot BEFORE applyFixups(), exactly like the
        // --static-link splice above binds ilib calls to spliced code — same
        // generic fixup engine, just pointed at a different kind of target.
        std::vector<PEImport> peImports;
        if (g_bnyTargetWindows) {
            peImports.push_back({"KERNEL32.DLL", "ExitProcess"});
            PEImportLayout L = layoutPEImports(peImports);
            em.defineLabelAt("ExitProcess", L.iatSlotRVA(0, peImports) - PE_TEXT_RVA);
        }

        em.applyFixups();

        if (g_bnyTargetWindows) {
            // No string/gvar fixups yet — this path targets the trivial (no
            // print, no NA->free globals) case first; that's real follow-up work
            // (needs GetStdHandle/WriteFile imports), not faked here.
            std::vector<uint8_t> text = em.code();
            uint32_t entryOff = (uint32_t)em.getLabelOffset("_start");
            return writePE(outPath, text, peImports, entryOff);
        }

        // Platform-specific binary generation:
        // - Linux x86-64: direct ELF64 binary, no C intermediary.
        // - ARM is intentionally handled one layer up by generating C and invoking gcc/clang.

        if (!dynamic) {
            // Static ELF (original path)
            size_t hdrBytes = sizeof(ElfEhdr) + 2*sizeof(ElfPhdr);
            uint64_t textOff   = ((hdrBytes + PGSZ-1) / PGSZ) * PGSZ;
            uint64_t textVA    = BASE + textOff;
            uint64_t rodataOff = textOff + ((em.code().size() + PGSZ-1) / PGSZ) * PGSZ;
            uint64_t rodataVA  = BASE + rodataOff;
            em.applyStringFixups(rodataVA, rodata, strOffsets);
            em.applyGVarFixups(rodataVA, gvarOffsets); // NA→free global slot addresses
            bool rwData = em.hasGVars();
            std::vector<uint8_t> text = em.code();
            uint64_t startOffset = em.getLabelOffset("_start");
            if (!debugInfo || srcPath.empty())
                return writeELF(outPath, text, rodata, startOffset, nullptr, rwData);
            DWARFBuilder dwarf;
            dwarf.setSourceFile(srcPath, ".");
            for (auto& fb : funcBounds) {
                if (fb.name == "__ac_print_int__" || fb.name == "__ac_print_str__") continue;
                dwarf.addFunction(fb.name, textVA + fb.startOff, textVA + fb.endOff);
            }
            DebugSections dbg;
            dbg.abbrev = dwarf.buildAbbrev();
            dbg.info   = dwarf.buildInfo();
            dbg.line   = dwarf.buildLine();
            return writeELF(outPath, text, rodata, startOffset, &dbg, rwData);
        }

        // Dynamic ELF: compute layout identical to writeELFDynamic
        int N = (int)extSyms.size();
        const char INTERP[] = "/lib64/ld-linux-x86-64.so.2";
        size_t interpSize = sizeof(INTERP);
        int    nsyms      = 1 + N;
        size_t hashSize   = (size_t)(2 + 1 + nsyms) * 4; // nbucket+nchain+bucket+chain
        size_t dynsymSize = (size_t)nsyms * sizeof(Elf64Sym);

        // Build a temporary dynstr to compute its size
        size_t dynstrSize = 1; // leading null
        for (auto& s : extSyms) dynstrSize += s.exportName.size() + 1;
        std::set<std::string> seenLibs;
        for (auto& s : extSyms)
            if (!s.lib.empty() && seenLibs.find(s.lib) == seenLibs.end()) {
                dynstrSize += s.lib.size() + 1;
                seenLibs.insert(s.lib);
            }
        bool haveRunpath = !runpath.empty();
        if (haveRunpath) dynstrSize += runpath.size() + 1; // DT_RUNPATH string in .dynstr

        size_t relaSize  = (size_t)N * sizeof(Elf64Rela);
        size_t nPhdr     = 4;
        size_t hdrBytes  = sizeof(ElfEhdr) + nPhdr * sizeof(ElfPhdr); // 288
        // LOAD[0] starts at file offset 0 (includes ELF header) so AT_PHDR is mapped.
        uint64_t textOff = hdrBytes + interpSize + hashSize + dynsymSize + dynstrSize + relaSize;
        uint64_t textVA  = BASE + textOff;

        // Page-align past actual data so RX and RW segments don't share a page.
        uint64_t textEnd   = textOff + em.code().size();
        uint64_t rodataOff = ((textEnd + PGSZ - 1) / PGSZ) * PGSZ;
        uint64_t rodataVA  = BASE + rodataOff;
        uint64_t rodataEnd = rodata.empty() ? rodataOff : rodataOff + rodata.size();
        uint64_t seg2Off   = rodata.empty() ? rodataOff : ((rodataEnd + PGSZ - 1) / PGSZ) * PGSZ;
        uint64_t gotpltVA  = BASE + seg2Off;

        // Patch GOT-PLT rel32 values in PLT stubs
        em.applyGOTPLTFixups(textVA, gotpltVA);

        // Patch string addresses
        em.applyStringFixups(rodataVA, rodata, strOffsets);
        // NA→free: gvar slots live in the writable seg2, right after .got.plt and .dynamic.
        // Layout here MUST match writeELFDynamic: dynamic entries = (#libs DT_NEEDED) + 11 fixed.
        {
            size_t gotpltSize  = (size_t)(3 + N) * 8;
            size_t dynamicSize = (seenLibs.size() + 11 + (haveRunpath ? 1 : 0)) * sizeof(Elf64Dyn);
            uint64_t gvarBaseVA = gotpltVA + gotpltSize + dynamicSize;
            std::vector<size_t> dynGvarOffsets(gvarSlots_.size());
            for (size_t k = 0; k < dynGvarOffsets.size(); k++) dynGvarOffsets[k] = k * 8;
            em.applyGVarFixups(gvarBaseVA, dynGvarOffsets);
        }

        std::vector<uint8_t> text = em.code();
        uint64_t startOffset = em.getLabelOffset("_start");

        // Compute initial GOT.PLT[3+i] = VA of `push` instruction in each PLT stub
        // PLT stub layout: FF 25 rel32 (6 bytes) | 68 imm32 (5 bytes) | E9 rel32 (5 bytes)
        // `push` is at stub offset +6
        std::vector<uint64_t> pltPushVAs(N);
        for (int i = 0; i < N; i++) {
            uint64_t stubOff = em.getLabelOffset(extSyms[i].irName);
            pltPushVAs[i] = textVA + stubOff + 6;
        }

        return writeELFDynamic(outPath, text, rodata, startOffset,
                               gotpltVA, textVA, extSyms, pltPushVAs,
                               (int)gvarSlots_.size(), runpath);
    }
};

} // namespace AC_BinaryGen

// ─── Public API ───────────────────────────────────────────────────────────────
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                          bool debugInfo, const std::string& srcPath,
                          const std::string& runpath, bool bundleStatic,
                          bool targetWindows) {
    AC_BinaryGen::BinaryCompiler compiler(ir, targetWindows);
    return compiler.compile(outputFile, debugInfo, srcPath, runpath, bundleStatic);
}
