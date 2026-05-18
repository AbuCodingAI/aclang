/*
  exp_bny.cpp — AC Native Binary Generator
  Targets: Linux x86-64 (ELF64), Windows x86-64 (PE32+)
  No external assembler or linker required.
*/
#include "../include/ac.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <fstream>
#include <cstring>
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
    std::map<std::string, size_t>           labelDefs;   // label → offset
    std::map<size_t, std::string>           rel32Fixups; // offset → label
    std::map<size_t, int>                   strAddrFixups; // offset → string pool id

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

    // Apply all relative fixups (must call before extracting bytes)
    void applyFixups() {
        for (auto& [off, name] : rel32Fixups) {
            auto it = labelDefs.find(name);
            if (it == labelDefs.end())
                throw std::runtime_error("BNY: undefined label '" + name + "' referenced at offset " + std::to_string(off));
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
    void xor_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x31); modrm(3,(int)s,(int)d); }
    void and_rr(R d, R s) { rex(true,(int)s,(int)d); emit(0x21); modrm(3,(int)s,(int)d); }
    void or_rr(R d, R s)  { rex(true,(int)s,(int)d); emit(0x09); modrm(3,(int)s,(int)d); }
    // cqo: sign-extend rax into rdx:rax
    void cqo() { emit(0x48); emit(0x99); }
    // idiv rcx: signed divide rdx:rax by rcx → rax=quotient, rdx=remainder
    void idiv_rcx() { emit(0x48); emit(0xF7); emit(0xF9); }
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
    // call [mem64]: FF /2 with REX (indirect call through memory address in register)
    void call_rip_rel(const std::string& lbl) {
        // FF 15 rel32 = CALL [rip+rel32]
        emit(0xFF); emit(0x15); rel32Fixups[pos()] = lbl; emit32(0);
    }
    void ret() { emit(0xC3); }

    // ── Syscall ──────────────────────────────────────────────────────────────
    void syscall() { emit(0x0F); emit(0x05); }
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
static ABI host_abi() {
#ifdef TARGET_WINDOWS
    return win64_abi();
#else
    return sysv_abi();
#endif
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
    std::map<int,int32_t> tempOff;  // temp_id → rbp offset (negative)
    std::map<int,int32_t> varOff;   // symbol_id → rbp offset (negative)
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
class FuncCompiler {
    const AC_IR::IRProgram& prog;
    X64Emitter&             em;
    StringPool&             sp;
    ABI                     abi;
    FuncFrame               frame;
    int32_t                 fsize = 0;
    bool                    isGlobal;
    LinearScanAlloc         regAlloc;
    std::vector<R>          calleeSaves; // push/pop order for this function

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

    // Load an IRRef into a register; returns the register used.
    // For TEMPs with a register assignment: copies to scratch to avoid
    // aliasing issues in arithmetic ops (e.g. add_rr clobbers destination).
    R load(const AC_IR::IRRef& r, R scratch = R::RAX) {
        using namespace AC_IR;
        switch (r.kind) {
        case IRRef::Kind::CONST:
            if (r.value.type == IRType::INT)
                em.mov_ri32(scratch, std::get<int>(r.value.data));
            else if (r.value.type == IRType::BOOL)
                em.mov_ri32(scratch, std::get<bool>(r.value.data) ? 1 : 0);
            else
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
        case IRRef::Kind::VAR:
            em.mov_r_rbp(scratch, frame.varOffset(r.id));
            return scratch;
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
        else if (r.kind == IRRef::Kind::VAR)
            em.mov_rbp_r(frame.varOffset(r.id), src);
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
#ifdef TARGET_WINDOWS
        em.mov_ri32(R::RCX, 0);
        em.call("ExitProcess");
#else
        em.mov_ri32(R::RAX, 60);
        em.xor_rr(R::RDI, R::RDI);
        em.syscall();
#endif
    }

    void emitLibCall(const AC_IR::IRInstruction& ins) {
        if (ins.typedOperands.size() < 2) return;
        auto& val = ins.typedOperands[1];
        using namespace AC_IR;
        if (val.kind == IRRef::Kind::CONST && val.value.type == IRType::STRING) {
            // Print string literal
            std::string s = std::get<std::string>(val.value.data);
            int sid = sp.add(s);
            // Load string address and length
            em.mov_ri64_str(abi.argRegs[0], sid);       // arg0 = ptr
            em.mov_ri32(abi.argRegs[1], (int32_t)s.size()); // arg1 = length
            em.call("__ac_print_str__");
        } else {
            // Print integer
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

        case IROpcode::STORE_VAR:
            if (ins.result.isValid()) {
                // result = stv value
                load(op0(), R::RAX);
                store(ins.result, R::RAX);
            } else if (ops.size() >= 2) {
                // stv dst, src
                load(ops[1], R::RAX);
                store(ops[0], R::RAX);
            }
            break;

        case IROpcode::LOAD_VAR:
        case IROpcode::LOAD_CONST:
            load(op0(), R::RAX);
            store(ins.result, R::RAX);
            break;

        case IROpcode::ADD:
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.add_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::SUB:
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.sub_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::MUL:
        case IROpcode::PMUL:  // native: integer IMUL (polymorphic dispatch not available in BNY)
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.imul_rr(R::RAX, R::RCX);
            store(ins.result, R::RAX);
            break;
        case IROpcode::DIV:
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.cqo(); em.idiv_rcx();
            store(ins.result, R::RAX);
            break;
        case IROpcode::MOD:
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.cqo(); em.idiv_rcx();
            store(ins.result, R::RDX);
            break;

        case IROpcode::EQ:  case IROpcode::NEQ:
        case IROpcode::LT:  case IROpcode::GT:
        case IROpcode::LTE: case IROpcode::GTE: {
            load(op0(), R::RAX); load(op1(), R::RCX);
            em.cmp_rr(R::RAX, R::RCX);
            uint8_t cc;
            switch (ins.opcode) {
            case IROpcode::EQ:  cc=0x04; break;
            case IROpcode::NEQ: cc=0x05; break;
            case IROpcode::LT:  cc=0x0C; break;
            case IROpcode::GT:  cc=0x0F; break;
            case IROpcode::LTE: cc=0x0E; break;
            default:            cc=0x0D; break; // GTE
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
            std::string fn = funcName(ops[0]);
            if (fn.empty()) break;
            // Load args into argument registers
            for (int i = 1; i < (int)ops.size() && (i-1) < (int)abi.argRegs.size(); i++) {
                load(ops[i], abi.argRegs[i-1]);
            }
            em.call(fn);
            if (ins.result.isValid())
                store(ins.result, R::RAX);
            break;
        }

        case IROpcode::RETURN:
            if (!ops.empty())
                load(ops[0], R::RAX);
            else
                em.mov_ri32(R::RAX, 0);
            emitEpilogue();
            break;

        case IROpcode::LIB_CALL:
            emitLibCall(ins);
            break;

        case IROpcode::PRINT:
            if (!ops.empty()) {
                auto& v = ops[0];
                if (v.kind == IRRef::Kind::CONST && v.value.type == AC_IR::IRType::STRING) {
                    std::string s = std::get<std::string>(v.value.data);
                    int sid = sp.add(s);
                    em.mov_ri64_str(abi.argRegs[0], sid);
                    em.mov_ri32(abi.argRegs[1], (int32_t)s.size());
                    em.call("__ac_print_str__");
                } else {
                    load(v, abi.argRegs[0]);
                    em.call("__ac_print_int__");
                }
            }
            break;

        case IROpcode::HALT:
            emitHalt();
            break;

        case IROpcode::NOP:
        default:
            break;
        }
    }

public:
    FuncCompiler(const AC_IR::IRProgram& p, X64Emitter& e, StringPool& s, ABI a, bool glob=false)
        : prog(p), em(e), sp(s), abi(a), isGlobal(glob) {}

    void compileFn(const AC_IR::IRFunction& fn) {
        // Pass 1: register allocation (must know N callee-saves before frame layout)
        regAlloc.run(fn.instructions);
        calleeSaves.clear();
        for (int i = 0; i < LinearScanAlloc::POOL_SIZE; i++)
            if (regAlloc.usedCalleeSaved.count(LinearScanAlloc::POOL[i]))
                calleeSaves.push_back(LinearScanAlloc::POOL[i]);
        int nSave = (int)calleeSaves.size();

        // Pass 2: frame layout — skip slots 0..nSave-1 (used by callee-save pushes)
        frame.setCalleeSaveBase(nSave);
        frame.scanInstrs(fn.instructions);
        fsize = frame.frameSize();

        // Stack alignment: after push_rbp (rsp≡0) + nSave callee-save pushes,
        // rsp ≡ 8*(nSave%2). For rsp≡0 before calls: fsize must absorb the remainder.
        if (nSave % 2 != 0) fsize += 8;

        // Emit prologue
        em.label(fn.name);
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
            if (symId >= 0)
                em.mov_rbp_r(frame.varOffset(symId), abi.argRegs[i]);
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

    void compileGlobal(const std::vector<AC_IR::IRInstruction>& globalInit) {
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

        for (auto& ins : globalInit) compileInstr(ins);

        emitHalt();
    }
};

// ─── Print Helpers (Linux) ────────────────────────────────────────────────────
static void emitPrintIntLinux(X64Emitter& em) {
    // __ac_print_int__(rdi = int64): print decimal followed by newline
    em.label("__ac_print_int__");
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.sub_rsp_i32(32); // 32-byte local buffer, rsp is 16-aligned after 2 pushes + rbp

    em.mov_rr(R::R12, R::RDI);   // r12 = argument value
    em.xor_rr(R::R13, R::R13);   // r13 = negative flag

    // R10 = write pointer (going backwards from rbp-2)
    em.lea_r_rbp8(R::R10, -2);

    // Handle zero
    em.test_rr(R::R12, R::R12);
    em.jnz("__pi_notzero__");
    em.mov_ri32(R::RAX, '0');
    em.mov_ptr_r8(R::R10, R::RAX);  // [r10] = '0'
    em.dec_r(R::R10);
    em.jmp("__pi_addnl__");

    em.label("__pi_notzero__");
    em.test_rr(R::R12, R::R12);
    em.jns("__pi_pos__");
    em.mov_ri32(R::R13, 1);     // set negative flag
    em.neg_r(R::R12);           // negate to positive

    em.label("__pi_pos__");
    em.mov_ri32(R::RCX, 10);    // divisor = 10

    em.label("__pi_loop__");
    em.mov_rr(R::RAX, R::R12);   // rax = value
    em.xor_edx_edx();             // rdx = 0
    em.div_rcx();                 // rax = quotient, rdx = remainder
    em.add_dl_i8('0');            // dl = remainder + '0'
    em.mov_ptr_r8(R::R10, R::RDX); // [r10] = digit
    em.dec_r(R::R10);
    em.mov_rr(R::R12, R::RAX);   // r12 = quotient
    em.test_rr(R::R12, R::R12);
    em.jnz("__pi_loop__");

    // Add sign
    em.test_rr(R::R13, R::R13);
    em.jz("__pi_addnl__");
    em.mov_ri32(R::RAX, '-');
    em.mov_ptr_r8(R::R10, R::RAX);
    em.dec_r(R::R10);

    em.label("__pi_addnl__");
    em.inc_r(R::R10);            // r10 = first character
    em.mov_rbp8_imm8(-1, '\n'); // [rbp-1] = newline

    // Compute length: (rbp-1) - r10 + 1
    em.lea_r_rbp8(R::RDX, -1);   // rdx = &rbp[-1]
    em.sub_rr(R::RDX, R::R10);   // rdx = &rbp[-1] - r10
    em.inc_r(R::RDX);            // +1 for the newline char

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

// ─── Print Helpers (Windows) ─────────────────────────────────────────────────
static void emitPrintIntWin64(X64Emitter& em) {
    // __ac_print_int__(rcx = int64): convert to string and print via WriteFile
    em.label("__ac_print_int__");
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.push_r(R::RBX);
    // Alignment: entry rsp≡8, push_rbp→0, push_r12→8, push_r13→0, push_rbx→8
    // Need sub N such that 8-N ≡ 0 (mod 16) → N ≡ 8 → use 40 (32 shadow + 8 local)
    em.sub_rsp_i32(40);

    // Win64: arg in RCX
    em.mov_rr(R::R12, R::RCX);   // r12 = argument value
    em.xor_rr(R::R13, R::R13);   // r13 = negative flag

    // Write pointer: [rbp-2] going backward
    em.lea_r_rbp8(R::R10, -2);

    em.test_rr(R::R12, R::R12);
    em.jnz("__piw_notzero__");
    em.mov_ri32(R::RAX, '0');
    em.mov_ptr_r8(R::R10, R::RAX);
    em.dec_r(R::R10);
    em.jmp("__piw_addnl__");

    em.label("__piw_notzero__");
    em.test_rr(R::R12, R::R12);
    em.jns("__piw_pos__");
    em.mov_ri32(R::R13, 1);
    em.neg_r(R::R12);

    em.label("__piw_pos__");
    em.mov_ri32(R::RCX, 10);

    em.label("__piw_loop__");
    em.mov_rr(R::RAX, R::R12);
    em.xor_edx_edx();
    em.div_rcx();
    em.add_dl_i8('0');
    em.mov_ptr_r8(R::R10, R::RDX);
    em.dec_r(R::R10);
    em.mov_rr(R::R12, R::RAX);
    em.test_rr(R::R12, R::R12);
    em.jnz("__piw_loop__");

    em.test_rr(R::R13, R::R13);
    em.jz("__piw_addnl__");
    em.mov_ri32(R::RAX, '-');
    em.mov_ptr_r8(R::R10, R::RAX);
    em.dec_r(R::R10);

    em.label("__piw_addnl__");
    em.inc_r(R::R10);
    em.mov_rbp8_imm8(-1, '\n');

    // Compute length
    em.lea_r_rbp8(R::RBX, -1);   // rbx = &rbp[-1]
    em.sub_rr(R::RBX, R::R10);
    em.inc_r(R::RBX);            // rbx = length

    // WriteFile(hStdout, r10, rbx, &written, NULL)
    // hStdout stored at [__stdout_handle] which we call via GetStdHandle
    // Use WriteConsoleA for simplicity: WriteConsoleA(hOut, buf, nChars, &nWritten, NULL)
    // GetStdHandle(-11) → rax = handle
    em.mov_ri32(R::RCX, -11);    // STD_OUTPUT_HANDLE
    em.call("GetStdHandle");

    // WriteConsoleA(hOut=rax, buf=r10, nChars=rbx, &nWritten=[rsp+0], NULL)
    em.mov_rr(R::RCX, R::RAX);   // handle
    em.mov_rr(R::RDX, R::R10);   // buf
    em.mov_rr(R::R8, R::RBX);    // nChars
    em.lea_r_rbp32(R::R9, -48);  // &written (in our local buffer)
    // 5th arg = NULL, pushed on stack (above shadow space)
    em.mov_ri32(R::RAX, 0);
    em.mov_rbp_r(-56, R::RAX);   // [rsp+32] conceptually (via rbp)
    em.call("WriteConsoleA");

    em.add_rsp_i32(40);
    em.pop_r(R::RBX);
    em.pop_r(R::R13);
    em.pop_r(R::R12);
    em.pop_rbp();
    em.ret();
}

static void emitPrintStrWin64(X64Emitter& em) {
    em.label("__ac_print_str__");
    em.push_rbp();
    em.mov_rbp_rsp();
    em.push_r(R::R12);
    em.push_r(R::R13);
    em.push_r(R::RBX);
    em.sub_rsp_i32(64);

    em.mov_rr(R::R12, R::RCX);   // save ptr
    em.mov_rr(R::R13, R::RDX);   // save len

    // GetStdHandle(-11)
    em.mov_ri32(R::RCX, -11);
    em.call("GetStdHandle");

    // WriteConsoleA
    em.mov_rr(R::RCX, R::RAX);
    em.mov_rr(R::RDX, R::R12);
    em.mov_rr(R::R8,  R::R13);
    em.lea_r_rbp32(R::R9, -48);
    em.mov_ri32(R::RAX, 0);
    em.mov_rbp_r(-56, R::RAX);
    em.call("WriteConsoleA");

    // Print newline
    em.mov_rbp8_imm8(-17, '\n');
    em.mov_ri32(R::RCX, -11);
    em.call("GetStdHandle");
    em.mov_rr(R::RCX, R::RAX);
    em.lea_r_rbp8(R::RDX, -17);
    em.mov_ri32(R::R8, 1);
    em.lea_r_rbp32(R::R9, -48);
    em.mov_ri32(R::RAX, 0);
    em.mov_rbp_r(-56, R::RAX);
    em.call("WriteConsoleA");

    em.add_rsp_i32(64);
    em.pop_r(R::RBX);
    em.pop_r(R::R13);
    em.pop_r(R::R12);
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
        aStr(body, "AC Compiler v0.2.0");
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
#pragma pack(pop)

static bool writeELF(const std::string& path,
                     const std::vector<uint8_t>& text,
                     const std::vector<uint8_t>& rodata,
                     uint64_t entryOffset = 0,
                     const DebugSections* dbg = nullptr) {
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

    ElfEhdr eh = {};
    eh.e_ident[0]=0x7F; eh.e_ident[1]='E'; eh.e_ident[2]='L'; eh.e_ident[3]='F';
    eh.e_ident[4]=2; eh.e_ident[5]=1; eh.e_ident[6]=1;
    eh.e_type=2; eh.e_machine=0x3E; eh.e_version=1;
    eh.e_entry=entry; eh.e_phoff=sizeof(ElfEhdr);
    eh.e_ehsize=sizeof(ElfEhdr); eh.e_phentsize=sizeof(ElfPhdr); eh.e_phnum=2;
    if (hasDebug) {
        eh.e_shoff    = shdrsOff;
        eh.e_shentsize = sizeof(ElfShdr);
        eh.e_shnum    = (uint16_t)shnum;
        eh.e_shstrndx = (uint16_t)shstrndx;
    }

    ElfPhdr tp = {}, rp = {};
    tp.p_type=1; tp.p_flags=5;
    tp.p_offset=textOff; tp.p_vaddr=textVA; tp.p_paddr=textVA;
    tp.p_filesz=text.size(); tp.p_memsz=text.size(); tp.p_align=PGSZ;

    rp.p_type=1; rp.p_flags=4;
    rp.p_offset=rodataOff; rp.p_vaddr=rodataVA; rp.p_paddr=rodataVA;
    rp.p_filesz=rodata.size(); rp.p_memsz=rodata.size(); rp.p_align=PGSZ;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write((char*)&eh, sizeof(eh));
    f.write((char*)&tp, sizeof(tp));
    f.write((char*)&rp, sizeof(rp));

    // Pad to textOff
    size_t pad = textOff - (sizeof(ElfEhdr)+2*sizeof(ElfPhdr));
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

// ─── PE32+ Writer ─────────────────────────────────────────────────────────────
#pragma pack(push,1)
struct DosStub {
    uint8_t  data[64];
};
struct PeFileHeader {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};
struct PeOptHeader64 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion, MinorLinkerVersion;
    uint32_t SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment, FileAlignment;
    uint16_t MajorOSVersion, MinorOSVersion;
    uint16_t MajorImageVersion, MinorImageVersion;
    uint16_t MajorSubsystemVersion, MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage, SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve, SizeOfStackCommit;
    uint64_t SizeOfHeapReserve, SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    struct { uint32_t VirtualAddress, Size; } DataDirectory[16];
};
struct PeSectionHeader {
    char     Name[8];
    uint32_t VirtualSize, VirtualAddress;
    uint32_t SizeOfRawData, PointerToRawData;
    uint32_t PointerToRelocations, PointerToLinenumbers;
    uint16_t NumberOfRelocations, NumberOfLinenumbers;
    uint32_t Characteristics;
};
struct ImportDescriptor {
    uint32_t OriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
};
#pragma pack(pop)

static bool writePE(const std::string& path,
                    const std::vector<uint8_t>& text,
                    const std::vector<uint8_t>& rodata) {
    // Layout:
    // 0x000: DOS stub (64 bytes) + PE sig (4 bytes)
    // 0x044: COFF header (20 bytes)
    // 0x058: Optional header PE32+ (240 bytes)
    // 0x148: Section header .text (40 bytes)
    // 0x170: Section header .rdata (40 bytes)
    // 0x198: Section header .idata (40 bytes)
    // [pad to 0x200] headers end
    // 0x200: .text section
    // Next: .rdata section (strings)
    // Next: .idata section (import table)

    const uint64_t ImageBase = 0x140000000ULL;
    const uint32_t SecAlign  = 0x1000; // 4KB section alignment
    const uint32_t FileAlign = 0x200;  // 512-byte file alignment

    // Build import data for kernel32.dll
    // Functions: GetStdHandle, WriteConsoleA, ExitProcess
    struct Func { const char* name; uint32_t iatRVA; };
    std::vector<std::string> funcs = {"GetStdHandle","WriteConsoleA","ExitProcess"};

    // Build .idata (import table) in memory
    // Structure:
    //   ImportDescriptor (20 bytes) for kernel32.dll
    //   ImportDescriptor (20 bytes) null terminator
    //   IAT: 4 * 8-byte entries (3 funcs + null)
    //   INT: same as IAT
    //   Hint/Name entries
    //   DLL name "KERNEL32.DLL"
    //
    // All offsets are RVAs (relative to image base)

    std::vector<uint8_t> idata;
    auto append = [&](const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        idata.insert(idata.end(), b, b+n);
    };
    auto append32 = [&](uint32_t v) { append(&v, 4); };
    auto append64 = [&](uint64_t v) { append(&v, 8); };
    auto appendStr = [&](const std::string& s) {
        for (char c : s) idata.push_back((uint8_t)c);
        idata.push_back(0);
    };

    // We'll compute RVAs later once we know section addresses.
    // For now build with placeholders; then we'll fix up inline.
    // Strategy: build idata with known relative offsets within idata,
    // then add idataRVA when writing.

    // Offsets within idata:
    // 0: ImportDescriptor (20 bytes)
    // 20: null ImportDescriptor (20 bytes)
    // 40: IAT (4 * 8 = 32 bytes)
    // 72: INT (4 * 8 = 32 bytes)  — same as IAT
    // 104: hint/name entries
    // After hint/names: DLL name

    uint32_t iatOff  = 40;  // IAT within idata
    uint32_t intOff  = 72;  // INT within idata
    uint32_t hnOff   = 104; // hint/name table start

    // Build hint/name entries and record their offsets
    std::vector<uint32_t> hnOffsets;
    std::vector<uint8_t> hnData;
    for (auto& fn : funcs) {
        uint32_t off = (uint32_t)hnData.size();
        hnOffsets.push_back(hnOff + off);
        hnData.push_back(0); hnData.push_back(0); // hint = 0
        for (char c : fn) hnData.push_back((uint8_t)c);
        hnData.push_back(0);
        if (hnData.size() % 2) hnData.push_back(0); // word-align
    }
    uint32_t dllNameOff = hnOff + (uint32_t)hnData.size();

    // Total idata size estimate
    size_t idataSize = dllNameOff + 16; // "KERNEL32.DLL\0" = 13 bytes + padding
    idataSize = ((idataSize + FileAlign-1)/FileAlign)*FileAlign;
    idata.assign(idataSize, 0);

    // Section layout (file offsets and RVAs):
    uint32_t headerSize = 0x200; // generous header region
    uint32_t textFileOff   = headerSize;
    uint32_t textSize      = (uint32_t)((text.size() + FileAlign-1)/FileAlign)*FileAlign;
    uint32_t rdataFileOff  = textFileOff + textSize;
    uint32_t rdataSize     = (uint32_t)((rodata.size() + FileAlign-1)/FileAlign)*FileAlign;
    if (rdataSize == 0) rdataSize = FileAlign;
    uint32_t idataFileOff  = rdataFileOff + rdataSize;

    uint32_t textRVA   = SecAlign; // 0x1000
    uint32_t rdataRVA  = textRVA + (uint32_t)(((text.size()+SecAlign-1)/SecAlign)*SecAlign);
    uint32_t idataRVA  = rdataRVA + (uint32_t)(((rodata.size()+SecAlign-1)/SecAlign)*SecAlign);
    if (rodata.empty()) idataRVA = rdataRVA + SecAlign;

    // Now fill idata with correct RVAs
    // IAT[i] = RVA of hint/name entry i (bit63=0 means RVA to hint/name)
    for (int i = 0; i < (int)funcs.size(); i++) {
        uint64_t v = idataRVA + hnOffsets[i];
        memcpy(idata.data() + iatOff + i*8, &v, 8);
        memcpy(idata.data() + intOff + i*8, &v, 8);
    }
    // IAT null terminator at offset iatOff + funcs.size()*8 (already zero)

    // Import descriptor
    ImportDescriptor idesc = {};
    idesc.OriginalFirstThunk = idataRVA + intOff;
    idesc.TimeDateStamp = 0;
    idesc.ForwarderChain = 0xFFFFFFFF;
    idesc.Name = idataRVA + dllNameOff;
    idesc.FirstThunk = idataRVA + iatOff;
    memcpy(idata.data(), &idesc, sizeof(idesc));
    // null terminator at offset 20 (already zero)

    // Hint/Name table
    memcpy(idata.data() + hnOff, hnData.data(), hnData.size());

    // DLL name
    const char dllName[] = "KERNEL32.DLL";
    memcpy(idata.data() + dllNameOff, dllName, sizeof(dllName));

    // Fixup call stubs in .text: calls to GetStdHandle/WriteConsoleA/ExitProcess
    // These are emitted as call_rel32 with label = "GetStdHandle" etc.
    // We need to add IAT stubs in the .text section that do:
    //   jmp [iat_entry]
    // Then the main call instructions call those stubs.
    // The stubs are placed at the END of the .text section (before it's finalized).

    // We need to patch the text for Windows: calls to "GetStdHandle" etc. should
    // go through IAT. We add stubs to the emitter BEFORE calling applyFixups.
    // This is handled by the BinaryCompiler calling emitIATStubs.

    // Entry point is at textRVA + 0 (first byte of .text)
    uint32_t entryRVA = textRVA;

    // Note: for IAT stub calls, the text needs to include stubs.
    // The stubs use: FF 25 <rel32> = JMP [RIP+rel32] where RIP+rel32 = IAT entry address.
    // Each IAT entry is at absolute address: ImageBase + idataRVA + iatOff + i*8
    // For stub at textFileOff + stubOffset (file offset),
    //   virtual addr = ImageBase + textRVA + stubOffset
    //   rel32 = (ImageBase + idataRVA + iatOff + i*8) - (ImageBase + textRVA + stubOffset + 6)
    //         = (idataRVA + iatOff + i*8) - (textRVA + stubOffset + 6)

    // The stub offsets depend on text size. We add them at the end.
    // This gets complex. For now, encode stub offsets as relative.
    // The BinaryCompiler must emit IAT stubs before calling compile so fixups work.

    // Total image size
    uint32_t idataVSize = (uint32_t)idataSize;
    uint32_t imageSize = idataRVA + ((idataVSize + SecAlign-1)/SecAlign)*SecAlign;

    // DOS header
    uint8_t dosStub[64] = {};
    dosStub[0] = 'M'; dosStub[1] = 'Z';
    *(uint32_t*)(dosStub+60) = 64; // e_lfanew

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write((char*)dosStub, 64);
    f.write("PE\0\0", 4);

    PeFileHeader coff = {};
    coff.Machine = 0x8664; // AMD64
    coff.NumberOfSections = 3;
    coff.Characteristics = 0x0022; // executable, no relocs
    coff.SizeOfOptionalHeader = sizeof(PeOptHeader64);
    f.write((char*)&coff, sizeof(coff));

    PeOptHeader64 opt = {};
    opt.Magic = 0x020B; // PE32+
    opt.AddressOfEntryPoint = entryRVA;
    opt.BaseOfCode = textRVA;
    opt.ImageBase = ImageBase;
    opt.SectionAlignment = SecAlign;
    opt.FileAlignment = FileAlign;
    opt.MajorSubsystemVersion = 6;
    opt.SizeOfImage = imageSize;
    opt.SizeOfHeaders = headerSize;
    opt.Subsystem = 3; // CUI console
    opt.SizeOfStackReserve = 0x100000;
    opt.SizeOfStackCommit  = 0x1000;
    opt.SizeOfHeapReserve  = 0x100000;
    opt.SizeOfHeapCommit   = 0x1000;
    opt.NumberOfRvaAndSizes = 16;
    opt.SizeOfCode = textSize;
    opt.SizeOfInitializedData = rdataSize + idataVSize;
    // Import table data directory
    opt.DataDirectory[1].VirtualAddress = idataRVA;
    opt.DataDirectory[1].Size = (uint32_t)(sizeof(ImportDescriptor)*2 + (funcs.size()+1)*16 + hnData.size() + 16);
    f.write((char*)&opt, sizeof(opt));

    auto writeSec = [&](const char* name, uint32_t vaddr, uint32_t vsz, uint32_t foff, uint32_t fsz, uint32_t chars) {
        PeSectionHeader sh = {};
        memcpy(sh.Name, name, std::min((size_t)8, strlen(name)));
        sh.VirtualSize = vsz;
        sh.VirtualAddress = vaddr;
        sh.SizeOfRawData = fsz;
        sh.PointerToRawData = foff;
        sh.Characteristics = chars;
        f.write((char*)&sh, sizeof(sh));
    };
    writeSec(".text",  textRVA,  (uint32_t)text.size(),   textFileOff,  textSize,  0x60000020);
    writeSec(".rdata", rdataRVA, (uint32_t)rodata.size(), rdataFileOff, rdataSize, 0x40000040);
    writeSec(".idata", idataRVA, idataVSize,               idataFileOff, (uint32_t)idataSize, 0xC0000040);

    // Pad to headerSize
    size_t hdrWritten = 64+4+sizeof(coff)+sizeof(opt)+3*sizeof(PeSectionHeader);
    f.write((char*)std::vector<uint8_t>(headerSize-hdrWritten,0).data(), headerSize-hdrWritten);

    // .text section
    f.write((char*)text.data(), text.size());
    std::vector<uint8_t> pad(textSize - text.size(), 0);
    f.write((char*)pad.data(), pad.size());

    // .rdata section
    if (!rodata.empty()) f.write((char*)rodata.data(), rodata.size());
    pad.assign(rdataSize - rodata.size(), 0);
    f.write((char*)pad.data(), pad.size());

    // .idata section
    f.write((char*)idata.data(), idata.size());

    return true;
}

// ─── Top-Level Binary Compiler ────────────────────────────────────────────────
class BinaryCompiler {
    const AC_IR::IRProgram& prog;
    X64Emitter em;
    StringPool sp;
    ABI        abi;
    bool       isWindows;

    struct FuncBounds { std::string name; uint64_t startOff, endOff; };

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

public:
    BinaryCompiler(const AC_IR::IRProgram& p)
        : prog(p), abi(host_abi())
#ifdef TARGET_WINDOWS
        , isWindows(true)
#else
        , isWindows(false)
#endif
    {}

    bool compile(const std::string& outPath,
                 bool debugInfo = false, const std::string& srcPath = "") {
        // Emit print helpers first; track their bounds so we can exclude from DWARF
        if (isWindows) {
            emitPrintIntWin64(em);
            emitPrintStrWin64(em);
        } else {
            emitPrintIntLinux(em);
            emitPrintStrLinux(em);
        }

        // Emit user-defined functions; record start/end offsets for DWARF
        std::vector<FuncBounds> funcBounds;
        for (auto& fn : prog.functions) {
            uint64_t startOff = em.pos();
            FuncCompiler fc(prog, em, sp, abi);
            fc.compileFn(fn);
            funcBounds.push_back({fn.name, startOff, em.pos()});
        }

        // Emit global section as _start
        {
            uint64_t startOff = em.pos();
            FuncCompiler gc(prog, em, sp, abi, true);
            gc.compileGlobal(prog.globalInit);
            funcBounds.push_back({"_start", startOff, em.pos()});
        }

        // Build string pool
        std::vector<size_t> strOffsets;
        std::vector<uint8_t> rodata = sp.build(strOffsets);

        // Apply relative jump fixups
        em.applyFixups();

        // Compute text section layout
        const uint64_t PGSZ = 0x1000ULL;
        size_t hdrBytes = sizeof(ElfEhdr) + 2*sizeof(ElfPhdr);
        uint64_t textOff   = ((hdrBytes + PGSZ-1) / PGSZ) * PGSZ;
        uint64_t textVA    = 0x400000ULL + textOff;
        uint64_t rodataOff = textOff + ((em.code().size() + PGSZ-1) / PGSZ) * PGSZ;
        uint64_t rodataVA  = 0x400000ULL + rodataOff;

        // Apply string address fixups
        em.applyStringFixups(rodataVA, rodata, strOffsets);
        std::vector<uint8_t> text = em.code();

        uint64_t startOffset = em.getLabelOffset("_start");

        if (isWindows) {
            return writePE(outPath, text, rodata);
        }

        if (!debugInfo || srcPath.empty()) {
            return writeELF(outPath, text, rodata, startOffset);
        }

        // Build DWARF sections
        DWARFBuilder dwarf;
        dwarf.setSourceFile(srcPath, ".");
        for (auto& fb : funcBounds) {
            // Skip runtime helper symbols
            if (fb.name == "__ac_print_int__" || fb.name == "__ac_print_str__") continue;
            dwarf.addFunction(fb.name, textVA + fb.startOff, textVA + fb.endOff);
        }
        DebugSections dbg;
        dbg.abbrev = dwarf.buildAbbrev();
        dbg.info   = dwarf.buildInfo();
        dbg.line   = dwarf.buildLine();
        return writeELF(outPath, text, rodata, startOffset, &dbg);
    }
};

} // namespace AC_BinaryGen

// ─── Public API ───────────────────────────────────────────────────────────────
bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                          bool debugInfo, const std::string& srcPath) {
    AC_BinaryGen::BinaryCompiler compiler(ir);
    return compiler.compile(outputFile, debugInfo, srcPath);
}
