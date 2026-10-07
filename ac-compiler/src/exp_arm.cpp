/*
  exp_arm.cpp — AC Native Binary Generator, AArch64
  Target: Linux ARM64 (aarch64) ELF64 only.

  Mirrors exp_bny.cpp's role for AArch64 — hand-encoded machine code, no external assembler,
  no external linker. Every instruction encoding below was verified against a real
  `aarch64-linux-gnu-as` + `objdump -d` round-trip during development (not hand-derived from
  the ARM ARM alone) — see the session notes for the exact .s snippets used to confirm each
  bit pattern before it was hardcoded here.

  Current scope (v1.2, verified via byte-for-byte comparison against PY across the whole
  examples/ suite — not a hidden gap list, an honest snapshot): a single <mainloop>, integer and
  double arithmetic (a genuinely exact %.16g-equivalent printer, derived from each value's own
  IEEE-754 exponent/mantissa bits rather than a fixed 2^52 scale — see emitPrintFloatRoutine),
  comparisons, WHILST/IF control flow, user functions (incl. recursion, real per-call stack
  frames), a whole-program float pre-scan (preScanFloatVars) that correctly promotes
  loop-carried accumulators even when the promotion only becomes visible after the first
  iteration, Term.display of integers/floats/strings/arrays, Term.ask string input, plain
  integer arrays/lists (literal construction, indexing, mutation, .append with
  capacity-doubling growth, length, FOR-in iteration — ported from BNY's own
  [cap][len][e0][e1]... heap layout and bump allocator), real heap-allocated strings (concat,
  compare, index, itoa/atoi, length — ported from BNY's NUL-terminated C-string design), and
  dicts (linear-scan get/set with the same capacity-doubling growth as arrays), and
  bundles/classes — flat 8-bytes-per-field instances (no header, no vtable), a whole-program
  field-order + classReturnFuncs_/classParamTypes pre-scan (computeClassFields/
  computeClassParamTypes) so `self.field`, an external `p.field`/`p.method()`, a bundle-typed
  free-function parameter, and an instance returned across a function boundary all resolve
  statically — and tuples, which need no ARM-specific code at all (ir.cpp scalarizes/synthesizes
  them into anonymous bundles backend-agnostically; this phase's own test suite is what actually
  exercises that path on ARM for the first time). Division by zero (DIV/FDIV/IDIV/MOD) raises a
  clean runtime error instead of AArch64's silent SDIV-returns-0/FDIV-returns-inf behavior.
  Still missing, same as BNY's own early history: try/catch, atomics, generators, and ilib
  dynamic linking (the ELF/PLT/GOT machinery exists but has a known writable-globals-page bug —
  see the ARM-parity plan's Phase 7). Native ilib calls use the AArch64 dynamic ELF path
  by default, or the static cross-link path with --static-link when their ARM build is available.
  Globals/temps live in a small fixed-address data page
  (no relocation/fixup machinery needed for v1: both the data page and the code page have
  addresses fixed at compile time, independent of how much code is generated), so there is no
  two-pass address-patching system yet either — that becomes necessary once string constants
  (rodata) are added.
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
#include <cmath>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

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
    // Inline data (string constants, padding) is recorded so the assembly listing renders it as
    // data rather than decoding it as instructions.
    std::vector<std::pair<size_t,size_t>> dataRanges;   // [begin, end) byte ranges of inline data
    void bytesRaw(const std::vector<uint8_t>& data) {
        size_t begin = buf.size();
        buf.insert(buf.end(), data.begin(), data.end());
        if (!data.empty()) dataRanges.push_back({begin, buf.size()});
    }
    void byteRaw(uint8_t b) {
        if (!dataRanges.empty() && dataRanges.back().second == buf.size()) dataRanges.back().second++;
        else dataRanges.push_back({buf.size(), buf.size() + 1});
        buf.push_back(b);
    }
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

    // ── scalar double-precision FP (V registers use the same 0..31 numbers) ──
    void fmov_d_from_x(R d, R n) { emit32(0x9E670000u | (rn(n)<<5) | rn(d)); }
    void fmov_x_from_d(R d, R n) { emit32(0x9E660000u | (rn(n)<<5) | rn(d)); }
    void fadd_d(R d, R n, R m) { emit32(0x1E602800u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void fsub_d(R d, R n, R m) { emit32(0x1E603800u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void fmul_d(R d, R n, R m) { emit32(0x1E600800u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void fdiv_d(R d, R n, R m) { emit32(0x1E601800u | (rn(m)<<16) | (rn(n)<<5) | rn(d)); }
    void fcmp_d(R n, R m) { emit32(0x1E602000u | (rn(m)<<16) | (rn(n)<<5)); }
    void scvtf_d_x(R d, R n) { emit32(0x9E620000u | (rn(n)<<5) | rn(d)); }
    void fcvtzs_x_d(R d, R n) { emit32(0x9E780000u | (rn(n)<<5) | rn(d)); }

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
    void ldrb0(R t, R n) { emit32(0x39400000u | (rn(n)<<5) | rn(t)); }
    // Escape hatch for fixup patching only — never used for first-pass emission.
    void word32(uint32_t w) { emit32(w); }

    // ── fixed prologue/epilogue idiom (verified exact bytes for THESE specific operands) ──
    void stp_x29_x30_presp16()  { emit32(0xA9BF7BFDu); }  // stp x29,x30,[sp,#-16]!
    void ldp_x29_x30_postsp16() { emit32(0xA8C17BFDu); }  // ldp x29,x30,[sp],#16
    void mov_x29_sp()           { emit32(0x910003FDu); }  // mov x29, sp
    void ret()                  { emit32(0xD65F03C0u); }
    void br(R n)                { emit32(0xD61F0000u | (rn(n)<<5)); }

    // ── branches (relative; label resolution is the caller's job — see LabelFixup below) ──
    void b_rel(int32_t imm26)     { emit32(0x14000000u | ((uint32_t)imm26 & 0x3FFFFFFu)); }
    void adr_rel(R d)             { emit32(0x10000000u | rn(d)); } // immediate patched by a fixup
    void blr(R n)                 { emit32(0xD63F0000u | (rn(n)<<5)); }
    void sxtw(R d, R n)           { emit32(0x93407C00u | (rn(n)<<5) | rn(d)); } // d = sign-extend(w_n)
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

#pragma pack(push,1)
struct Elf64Sym {
    uint32_t st_name;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
    uint64_t st_value, st_size;
};
struct Elf64Rela { uint64_t r_offset, r_info; int64_t r_addend; };
struct Elf64Dyn { int64_t d_tag; uint64_t d_val; };
#pragma pack(pop)

struct ArmExternal {
    std::string irName;
    std::string exportName;
    std::string library;
    size_t stubImmOffset = 0;
};

struct ArmCompiledImage {
    std::vector<uint8_t> text;
    int slotCount = 0;
    std::vector<ArmExternal> externals;
    std::vector<std::pair<size_t,std::string>> externalCalls;
    std::vector<std::pair<size_t,size_t>> dataRanges;          // inline data inside text
    std::map<size_t, std::vector<std::string>> symbols;        // text offset -> label names
};

static bool armExternalName(const std::string& irName, std::string& exportName,
                            std::string& library) {
    static const std::map<std::string, std::pair<std::string,std::string>> exact = {
        // native-cpu's carried-over pointer functions are called BARE (ptr_new, not ncpu.ptr_new);
        // the real symbol is ac_ncpu_ptr_*. Same mapping as BNY's isNativeCpuPtrSym.
        {"ptr_new", {"ac_ncpu_ptr_new", "libacncpu.so"}},
        {"ptr_deref", {"ac_ncpu_ptr_deref_str", "libacncpu.so"}}, // the AC-facing string form
        {"ptr_null", {"ac_ncpu_ptr_null", "libacncpu.so"}},
        {"ptr_is_null", {"ac_ncpu_ptr_is_null", "libacncpu.so"}},
        {"ptr_eq", {"ac_ncpu_ptr_eq", "libacncpu.so"}},
        {"ptr_copy", {"ac_ncpu_ptr_copy", "libacncpu.so"}},
        {"ptr_update", {"ac_ncpu_ptr_update", "libacncpu.so"}},
        {"ptr_free", {"ac_ncpu_ptr_free", "libacncpu.so"}},
        {"math.sin", {"ac_sin", "libacmath.so"}},
        {"math.cos", {"ac_cos", "libacmath.so"}},
        {"math.tan", {"ac_tan", "libacmath.so"}},
        {"math.sqrt", {"ac_sqrt", "libacmath.so"}},
        {"math.pow", {"ac_pow", "libacmath.so"}},
        {"math.floor", {"ac_floor", "libacmath.so"}},
        {"math.ceil", {"ac_ceil", "libacmath.so"}},
        {"math.round", {"ac_round", "libacmath.so"}},
        {"math.mod", {"ac_mod", "libacmath.so"}},
        {"math.to_dec", {"ac_to_dec", "libacmath.so"}},
        {"math.pi", {"ac_math_pi_const", "libacmath.so"}},
        {"math.e", {"ac_math_e_const", "libacmath.so"}},
        {"math.eval", {"ac_eval", "libacmath.so"}},
        {"widgets.screen_dimensions", {"ac_widgets_screen_dimensions", "libacwidgets.so"}},
        {"widgets.screen_mainloop", {"ac_widgets_screen_mainloop", "libacwidgets.so"}},
        {"widgets.update", {"ac_widgets_screen_update", "libacwidgets.so"}},
        {"widgets.destroy", {"ac_widgets_screen_destroy", "libacwidgets.so"}},
        {"widgets.pack", {"ac_widgets_pack", "libacwidgets.so"}},
        {"widgets.add", {"ac_widgets_add", "libacwidgets.so"}},
        {"widgets.get", {"ac_widgets_get", "libacwidgets.so"}},
        {"widgets.set", {"ac_widgets_set", "libacwidgets.so"}},
        {"widgets.set_d", {"ac_widgets_set_d", "libacwidgets.so"}},
        {"widgets.tabs_add_tab", {"ac_widgets_tabs_add_tab", "libacwidgets.so"}},
        {"Screen", {"ac_widgets_screen_new", "libacwidgets.so"}},
        {"display", {"ac_widgets_display_new", "libacwidgets.so"}},
        {"ask", {"ac_widgets_ask_new", "libacwidgets.so"}},
        {"btn", {"ac_widgets_btn_new", "libacwidgets.so"}},
        {"ckbtn", {"ac_widgets_ckbtn_new", "libacwidgets.so"}},
        {"radbtn", {"ac_widgets_ckbtn_new", "libacwidgets.so"}},
        {"dropdown", {"ac_widgets_dropdown_new", "libacwidgets.so"}},
        {"advance", {"ac_widgets_advance_new", "libacwidgets.so"}},
        {"slider", {"ac_widgets_slider_new", "libacwidgets.so"}},
        {"group", {"ac_widgets_group_new", "libacwidgets.so"}},
        {"tabs", {"ac_widgets_tabs_new", "libacwidgets.so"}},
        {"scroller", {"ac_widgets_scroller_new", "libacwidgets.so"}},
        {"listbox", {"ac_widgets_listbox_new", "libacwidgets.so"}},
        {"table", {"ac_widgets_table_new", "libacwidgets.so"}},
        {"sketch", {"ac_widgets_sketch_new", "libacwidgets.so"}},
        {"textbox", {"ac_widgets_textbox_new", "libacwidgets.so"}},
        {"widgets.sketch_clear", {"ac_widgets_sketch_clear", "libacwidgets.so"}},
        {"widgets.sketch_line", {"ac_widgets_sketch_line", "libacwidgets.so"}},
        {"widgets.sketch_rect", {"ac_widgets_sketch_rect", "libacwidgets.so"}},
        {"widgets.sketch_circle", {"ac_widgets_sketch_circle", "libacwidgets.so"}},
        {"widgets.sketch_text", {"ac_widgets_sketch_text", "libacwidgets.so"}},
        {"stringm.strip", {"ac_stringm_trim", "libacstringcheese.so"}},
        {"stringm.strip_clause", {"ac_stringm_strip_clause", "libacstringcheese.so"}},
        {"stringm.stripln", {"ac_stringm_stripln", "libacstringcheese.so"}},
        {"stringm.trim", {"ac_stringm_trim", "libacstringcheese.so"}},
        {"stringm.len", {"ac_stringm_len", "libacstringcheese.so"}},
        {"stringm.length", {"ac_stringm_len", "libacstringcheese.so"}},
        {"ml.tensor", {"ml_tensor", "libacml.so"}},
        {"ml.grid", {"ml_grid", "libacml.so"}},
        {"ml.gradient_track", {"ml_gradient_track", "libacml.so"}},
        {"ml.backward", {"ml_backward", "libacml.so"}},
        {"ml.grad_wipe", {"ml_grad_wipe", "libacml.so"}},
        {"ml.weights", {"ml_weights", "libacml.so"}},
        {"ml.take", {"ml_take", "libacml.so"}},
        {"ml.optimize", {"ml_optimize", "libacml.so"}},
        {"ml.add", {"ml_add", "libacml.so"}},
        {"ml.multiply", {"ml_multiply", "libacml.so"}},
        {"ml.relu", {"ml_relu", "libacml.so"}},
        {"maudio.stop", {"ac_maudio_stop_all", "libacmachinaaudio.so"}},
    };
    auto it = exact.find(irName);
    if (it != exact.end()) {
        exportName = it->second.first;
        library = it->second.second;
        return true;
    }
    struct Prefix { const char* ir; const char* out; const char* lib; };
    static const Prefix prefixes[] = {
        {"math.", "ac_", "libacmath.so"},
        {"stringm.", "ac_stringm_", "libacstringcheese.so"},
        {"regex.", "ac_regex_", "libacregex.so"},
        {"os.", "ac_os_", "libacoos.so"},
        {"web.", "ac_web_", "libacweb.so"},
        {"server.", "ac_server_", "libacserver.so"},
        {"maudio.", "ac_maudio_", "libacmachinaaudio.so"},
        {"ncpu.", "ac_ncpu_", "libacncpu.so"},
        {"camera.", "ac_camera_", "libaccamera.so"},
        {"widgets.", "ac_widgets_", "libacwidgets.so"},
        {"dns.", "ac_dns_", "libacdns.so"},
    };
    for (const auto& p : prefixes) {
        if (irName.rfind(p.ir, 0) == 0) {
            exportName = p.out + irName.substr(std::strlen(p.ir));
            std::replace(exportName.begin(), exportName.end(), '.', '_');
            library = p.lib;
            return true;
        }
    }
    return false;
}

// Export names whose C signature returns `const char*` (checked against the ilib headers, not
// guessed from the AC names). Every string an ilib returns is copied into the program's heap
// (see emitStrDupRoutine): several of these point into a shared buffer that the next call
// overwrites, so keeping the raw pointer would alias values that PY keeps distinct.
static bool armExternalReturnsString(const std::string& irName) {
    // Exact names, not namespace prefixes: stringm, web, server and maudio also contain
    // integer-returning functions (stringm.endswith, for one), which must stay integers.
    static const std::set<std::string> cStringExports = {
        "ac_os_cwd", "ac_os_env", "ac_os_read", "ac_os_join", "ac_os_basename", "ac_os_dirname",
        "ac_os_homedir", "ac_os_mktmpdir", "ac_os_tmpdir", "ac_os_tmpfile",
        "ac_regex_search", "ac_regex_replace", "ac_regex_replace_all", "ac_regex_escape",
        "ac_sidebar_ask", "ac_sidebar_getinput",
        "ac_ncpu_ptr_deref_str", "ac_ncpu_version",
        "ac_stringm_b", "ac_stringm_f", "ac_stringm_t", "ac_stringm_format", "ac_stringm_getline",
        "ac_stringm_lower", "ac_stringm_upper", "ac_stringm_replace", "ac_stringm_split_nth",
        "ac_stringm_strip", "ac_stringm_strip_clause", "ac_stringm_stripln", "ac_stringm_trim",
        "ac_web_page_get", "ac_web_help",
        "ac_server_db_import", "ac_server_db_reset", "ac_server_db_run", "ac_server_db_run_p",
        "ac_server_help", "ac_server_req_body", "ac_server_req_header", "ac_server_req_method",
        "ac_server_req_path", "ac_server_req_query",
        "ac_maudio_decode", "ac_maudio_listen",
    };
    if (irName == "dns.resolve" || irName == "dns.list") return true;
    std::string exportName, library;
    return armExternalName(irName, exportName, library) && cStringExports.count(exportName) > 0;
}

// ilib exports whose C return type is a 32-bit `int` (or bool). AArch64 zero-extends a W-register
// result into X0, so a C -1 would read as 4294967295; these get an SXTW after the call. Derived
// from the ilib headers (grep of `^int ac_`), not guessed. 64-bit and pointer returns are NOT here.
static bool armIntReturningExport(const std::string& exportName) {
    static const std::set<std::string> intExports = {
        "ac_camera_capture", "ac_camera_capture_first", "ac_camera_capture_latest", "ac_camera_init",
        "ac_is_prime", "ac_maudio_load_mp3", "ac_maudio_tts_ok",
        "ac_ncpu_arena_abort", "ac_ncpu_arena_dealloc", "ac_ncpu_arena_destroy", "ac_ncpu_ptr_eq",
        "ac_ncpu_ptr_free", "ac_ncpu_ptr_is_null", "ac_ncpu_ptr_update", "ac_ncpu_recieve",
        "ac_os_app_open", "ac_os_append_to", "ac_os_bash", "ac_os_chdir", "ac_os_copy", "ac_os_exists",
        "ac_os_isdir", "ac_os_isfile", "ac_os_mkdir", "ac_os_mkfile", "ac_os_move", "ac_os_pid",
        "ac_os_rmdir", "ac_os_rmfile", "ac_os_sbash", "ac_os_wait", "ac_os_write_to",
        "ac_regex_count", "ac_regex_match", "ac_regex_test",
        "ac_server_accept", "ac_server_listen", "ac_server_respond", "ac_server_respond_json",
        "ac_stringm_count", "ac_stringm_endswith", "ac_stringm_find", "ac_stringm_ischar",
        "ac_stringm_isws", "ac_stringm_scan", "ac_stringm_startswith",
        "ac_web_inspect", "ac_web_pdf", "ac_web_text", "ac_widgets_ckbtn_get",
        "ac_widgets_listbox_count", "ac_widgets_table_count",
        "ac_zip_decompress", "ac_zip_decompress_from_file", "ac_zip_iso", "ac_zip_package",
        "ml_grad_wipe", "ml_gradient_track", "ml_weights", "ml_backward", "ml_optimize",
        "ml_adam_step", "ml_sgd_step",
    };
    return intExports.count(exportName) > 0;
}

// ilib calls whose PY wrapper returns a real Python bool (printed True/False). Their C side returns
// an int (0/1). Checked against the ffi wrappers: os.isdir/isfile, stringm.isws/ischar return 1/0 in PY
// and are deliberately NOT here.
static bool armBoolReturningIr(const std::string& irName) {
    static const std::set<std::string> boolExports = {
        "ac_os_exists", "ac_regex_match", "ac_regex_test", "ac_stringm_startswith",
        "ac_stringm_endswith", "ac_web_pdf", "ac_web_text", "ac_web_inspect",
    };
    std::string exportName, library;
    return armExternalName(irName, exportName, library) && boolExports.count(exportName) > 0;
}

// ilib functions whose C side returns `char**` plus an `int* out_count` (see os_c.h, regex_c.h,
// string_cheese_c.h). ARM copies that into an AC string list instead of treating it as one string.
// AAPCS64 for ilib calls: doubles travel in D0-D7 and everything else in X0-X7, each class counted
// separately. Parameter classes come from the C prototypes (ilib headers), one letter per parameter:
// 'd' double, 'x' integer, 's' C string, 'p' pointer. nullptr = unknown (see loadExternalArgs).
static const char* armExternalParamClasses(const std::string& exportName) {
    static const std::map<std::string, const char*> classes = {
        {"ac_abs","d"}, {"ac_acos","d"}, {"ac_acot","d"}, {"ac_acsc","d"}, {"ac_asec","d"},
        {"ac_asin","d"}, {"ac_atan","d"}, {"ac_cbrt","d"}, {"ac_ceil","d"}, {"ac_cos","d"},
        {"ac_cot","d"}, {"ac_csc","d"}, {"ac_deg2rad","d"}, {"ac_floor","d"}, {"ac_ln","d"},
        {"ac_log10","d"}, {"ac_log2","d"}, {"ac_rad2deg","d"}, {"ac_round","d"}, {"ac_sec","d"},
        {"ac_sin","d"}, {"ac_sqrt","d"}, {"ac_tan","d"},
        {"ac_atan2","dd"}, {"ac_hypot","dd"}, {"ac_mod","dd"}, {"ac_pow","dd"}, {"ac_log_base","dd"},
        {"ac_clamp","ddd"},
        {"ac_derivative","sd"}, {"ac_eval_at","sd"}, {"ac_limit","sd"}, {"ac_eval","s"},
        {"ac_integrate","sdd"}, {"ac_maxima","sdd"}, {"ac_minima","sdd"},
        {"ac_math_e","x"}, {"ac_math_phi","x"}, {"ac_math_pi","x"}, {"ac_to_dec","x"},
        {"ac_sigma","px"}, {"ac_stat_avg","px"}, {"ac_stat_max","px"}, {"ac_stat_median","px"},
        {"ac_stat_min","px"}, {"ac_stat_mode","px"}, {"ac_stat_q1","px"}, {"ac_stat_q3","px"},
        {"ac_get_compression_ratio","xx"},
        {"ac_mod_int","xx"}, {"ac_gcd","xx"}, {"ac_lcm","xx"}, {"ac_abs_int","x"},
        {"ml_tensor","d"}, {"ml_weights","dx"}, {"ml_take","x"},
    };
    auto it = classes.find(exportName);
    return it == classes.end() ? nullptr : it->second;
}

// ilib exports whose C return type is `double`: the result arrives in D0 and is moved to X0.
static bool armDoubleReturningExport(const std::string& exportName) {
    static const std::set<std::string> doubles = {
        "ac_abs","ac_acos","ac_acot","ac_acsc","ac_asec","ac_asin","ac_atan","ac_atan2","ac_cbrt",
        "ac_ceil","ac_clamp","ac_cos","ac_cot","ac_csc","ac_deg2rad","ac_derivative","ac_eval",
        "ac_eval_at","ac_floor","ac_get_compression_ratio","ac_hypot","ac_integrate","ac_limit",
        "ac_ln","ac_log10","ac_log2","ac_log_base","ac_maxima","ac_minima","ac_mod","ac_pow",
        "ac_rad2deg","ac_round","ac_sec","ac_sigma","ac_sin","ac_sqrt","ac_stat_avg","ac_stat_max",
        "ac_stat_median","ac_stat_min","ac_stat_mode","ac_stat_q1","ac_stat_q3","ac_tan","ac_to_dec",
        "ac_math_e_const","ac_math_em_const","ac_math_inf","ac_math_phi_const","ac_math_pi_const",
        "ac_math_tau_const","ac_math_e","ac_math_phi","ac_math_pi", "ml_take",
    };
    return doubles.count(exportName) > 0;
}

// True for text that is a complete integer literal ("3", "-7").
static bool armTextIsIntLiteral(const std::string& s) {
    size_t i = (!s.empty() && s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); i++) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

// True for text that is a complete float literal ("2.0", "1e-3"), not an identifier or an integer.
static bool armTextIsFloatLiteral(const std::string& s) {
    if (s.find_first_of(".eE") == std::string::npos) return false;
    size_t consumed = 0;
    try { std::stod(s, &consumed); } catch (...) { return false; }
    return consumed == s.size();
}

// True when a list literal's text is made only of float literals (at least one). Mirrors the
// tokenizer in the ALLOC "list" codegen: an integer or non-numeric element makes it false.
static bool armLiteralIsFloatList(const std::string& s) {
    bool anyFloat = false;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(',', i);
        std::string tok = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
        size_t a = tok.find_first_not_of(" \t"), b = tok.find_last_not_of(" \t");
        if (a != std::string::npos) {
            tok = tok.substr(a, b - a + 1);
            size_t consumed = 0;
            try { std::stoll(tok, &consumed); if (consumed == tok.size()) return false; } catch (...) {}
            consumed = 0;
            try {
                std::stod(tok, &consumed);
                if (consumed != tok.size()) return false;
                anyFloat = true;
            } catch (...) { return false; }
        }
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return anyFloat;
}

static bool armStrListReturning(const std::string& irName) {
    return irName == "stringm.split" || irName == "regex.find_all"
        || irName == "regex.split" || irName == "regex.groups" || irName == "os.listdir";
}

static bool armExternalReturnsFloat(const std::string& irName) {
    if (irName.rfind("math.", 0) != 0) {
        // Other ilibs (ml.take, ...): float when the C prototype returns a double.
        std::string exportName, library;
        return armExternalName(irName, exportName, library) && armDoubleReturningExport(exportName);
    }
    // "math.mod" (bare, no "_int" suffix) is a genuine exception: PY's own math_mod wrapper
    // ("int-exact when operands and result are whole") always returns a real int for integer
    // operands, since int-mod is unconditionally exact — unlike math.abs or true division,
    // which stay float-returning even for whole results. This backend's CALL dispatch mirrors
    // that by routing bare "math.abs"/non-int-operand math.mod through genuine float codegen and
    // ONLY math.mod (both operands non-float) through compileIntegerMod — see that call site.
    // Verified real bug this classifier alone closes: preScanFloatVars (a standalone scan, not
    // routed through the CALL dispatch's own `break`-shielded special cases) classified bare
    // "math.mod" as float using this function alone, wrongly marking is_armstrong's digit/
    // accumulator as float and breaking its `total is n` integer comparison (153 vs 153.0's bit
    // pattern never match). A SEPARATE, now-reverted attempt to also exempt "math.abs" here was
    // itself a real bug: math.abs has no such int-exact wrapper on PY (always float, even for
    // `math.abs(-5)` → `5.0`), so exempting it here diverged from PY on any non-constant call.
    static const std::set<std::string> integerResults = {
        "math.to_int", "math.abs_int", "math.mod_int", "math.mod",
        "math.gcd", "math.lcm", "math.is_prime",
        // C returns int64_t for these (ac_modpow/ac_modinv/ac_modmul); -1 marks an invalid input.
        "math.modpow", "math.modinv", "math.modmul"
    };
    return integerResults.count(irName) == 0;
}

// ─── Branch fixups ──────────────────────────────────────────────────────────
enum class FixKind { B, BL, CBZ, CBNZ, BCOND, ADR };
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
    std::map<std::string,size_t> externalStubOffsets_;
    std::vector<ArmExternal> externals_;
    std::vector<std::pair<size_t,std::string>> externalCalls_;
    std::map<std::string,int> slotOf;      // qualified key (see keyFor) -> slot index
    std::set<std::string> arrayRefs_;
    // Arrays whose elements are strings: ilib string-list results (split/listdir/find_all/groups).
    std::set<std::string> strListRefs_;
    // Arrays whose elements are floats (float literals, or a float appended). Elements are stored
    // as IEEE bits either way; this only decides how a read or FOR loop variable is printed/used.
    std::set<std::string> floatListRefs_;
    std::set<std::string> floatListReturningFuncs_;
    // callee -> parameter positions that receive a float list at some call site (see computeArrayReturningFuncs).
    std::map<std::string, std::set<int>> floatParamListHints_;
    // Values of boolean type (comparison results, True/False), so Term.display prints True/False.
    std::set<std::string> boolRefs_;
    std::set<std::string> dictRefs_;
    std::set<std::string> dictStringKeys_;
    std::set<std::string> dictStrValued_;   // dicts whose every value is a string (any key reads a string)
    std::set<std::string> arrayReturningFuncs_;
    std::set<std::string> stringRefs_;
    std::set<std::string> floatRefs_;
    std::set<std::string> stringNames_;
    std::set<std::string> stringReturningFuncs_;
    std::set<std::string> mixedStringReturningFuncs_;
    // Verified real bug this closes: a user function returning a float (e.g. `return s / length
    // arr`) correctly computed the right IEEE-754 bit pattern, but nothing at the CALL site in
    // the CALLER's scope knew the result was a float — arrayReturningFuncs_/stringReturningFuncs_
    // already existed for exactly this purpose for arrays/strings, floats had no equivalent, so
    // a later Term.display on the call's result fell through to the plain-int print routine and
    // printed the raw bits as a huge integer (array_average.ac: 5.0 printed as
    // 4617315517961601024). Computed in computeArrayReturningFuncs() alongside its siblings.
    std::set<std::string> floatReturningFuncs_;
    std::map<std::string,std::set<std::string>> stringParamHints_;
    // Verified real bug this closes: a function parameter passed a literal float argument at
    // some call site (`nsqrt(2.0)`) was never marked float-typed inside the function body at
    // all — `x / 2.0` inside `nsqrt` then took the INTEGER division path, dividing the float's
    // raw bit pattern by 2 as if it were a plain int64, printing a huge garbage "float" (the
    // wrongly-halved bit pattern reinterpreted as a double) instead of the real quotient. Same
    // call-site-scanning mechanism as stringParamHints_ just above, mirrored for floats.
    std::map<std::string,std::set<std::string>> floatParamHints_;
    // Bundle/class support (ported from BNY's identical design): a whole-program pre-scan finds
    // every `self.field` STORE_VAR inside each class's methods and assigns each field a flat
    // offset (8*index, first-seen order) — no header, no vtable, just a bump-allocated block of
    // 8-byte slots. instanceClass_ tracks which class each constructed variable belongs to, set
    // live as construction call sites are compiled, so a LATER `p.field`/`p.method()` on that
    // same variable can resolve statically. currentClass_ is set while compiling a method body
    // (from fn.classOwner) so a bare `self.field` inside it resolves against ITS class.
    std::map<std::string, std::vector<std::string>> classFields_;
    // class -> fields that hold strings (a `self.f = $text$` default, e.g. `public name = $x$`),
    // so a later `self.f` / `p.f` read prints as text, not as a raw pointer.
    std::map<std::string, std::set<std::string>> stringFields_;
    std::map<std::string, std::string> instanceClass_;
    // A free function whose every `return` traces to a var directly constructed via
    // `SomeClass()` earlier in that same function body — lets `q = f()` be treated exactly like
    // a direct `q = ClassName()` construct for instanceClass_ purposes, even though the instance
    // arrived across a function-return boundary (ported from BNY's identical mechanism).
    std::map<std::string, std::string> classReturnFuncs_;
    std::string currentClass_;
    int fieldOffset(const std::string& cls, const std::string& field) const {
        auto it = classFields_.find(cls);
        if (it == classFields_.end()) return -1;
        for (size_t i = 0; i < it->second.size(); i++)
            if (it->second[i] == field) return (int)(8 * i);
        return -1;
    }
    // A dotted VAR name ("self.hp", "p.x") is field access iff its base resolves to a known
    // class (self -> currentClass_, else -> instanceClass_[base]). Returns false for an
    // ordinary dotted name that isn't actually a field (lets the normal slot path handle it).
    bool resolveFieldAccess(const std::string& name, std::string& base, int& offset) const {
        auto dot = name.find('.');
        if (dot == std::string::npos) return false;
        base = name.substr(0, dot);
        std::string field = name.substr(dot + 1);
        std::string cls = (base == "self") ? currentClass_
                         : (instanceClass_.count(base) ? instanceClass_.at(base) : std::string());
        if (cls.empty()) return false;
        offset = fieldOffset(cls, field);
        return offset >= 0;
    }
    // Slot 0 of the globals page is reserved for the array-heap bump cursor (see
    // emitAllocRoutine) — never assigned to a real var/temp, so slotFor's lazy allocation
    // starts from 1. Mirrors BNY's own dedicated cursorSlot, just a fixed offset instead of
    // going through BNY's general global-variable-slot allocator.
    int nextGlobalSlot_ = 2;   // slot 1 is the allocator's chunk limit (see emitAllocRoutine)
    int nextFuncSlot_ = 0;
    // Reserved synthetic key for a method's `self` parameter — never a bare VAR in the IR (only
    // ever fused into compound names like "self.hp", a completely separate symbol), so it can't
    // be found via the normal name-matching scan every other parameter uses. Given its own fixed
    // slot (0) in every method's frame, distinct from the ordinary per-symbol slot map.
    static constexpr int SELF_SLOT = 0;
    bool compilingMethod_ = false;
    const IRProgram& prog;
    uint64_t codeVA_ = 0;
    bool dynamicLink_ = false;
    bool staticLink_ = false;
    struct IfCtx { int elseLabel; int endLabel; bool sawElse; };
    struct ForCtx { IRRef arrRef; IRRef idxRef; int startLabel; int endLabel; bool stringMode; };
    std::vector<IfCtx> ifStack_;
    std::vector<ForCtx> forStack_;
    int nextHiddenTemp_ = -100000;

    // Which IRFunction (by name) is currently being compiled — "" while compiling the
    // mainloop. Real symbol (VAR) ids are globally unique across the whole program (the
    // SymbolTable's intern counter never resets), but TEMP ids are NOT — each IRFunction has
    // its own tempCount starting back at 0 (see IRFunction's own declaration), so `t_0` inside
    // `add` and `t_0` inside the mainloop are two totally different values that would silently
    // alias the same memory slot without this qualifier. Same reasoning extends to VAR too, out
    // of caution, since nothing here re-derives the real SymbolTable scoping rules directly.
    std::string currentFuncName_;
    std::set<std::string> currentFuncParamNames_;

    // Vars and temps share one key space here (qualified by function + kind so no two
    // different bindings can ever collide) — same "give everything a memory slot, no register
    // allocation" choice BNY itself could have started from before its own register allocator
    // existed. NOTE (v1.1 scope, not silently accepted — see this file's header comment):
    // every function's locals get their OWN distinct slots but STILL share the one flat
    // globals page, not a real per-call stack frame — so this does not yet support recursion
    // (a recursive call would overwrite its own in-flight locals). Real stack-frame-backed
    // locals are the natural next step once plain non-recursive calls are solid.
    std::string keyFor(const IRRef& r) const {
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return currentFuncName_ + "#" + kind + std::to_string(r.id);
    }
    int slotFor(const IRRef& r) {
        std::string k = keyFor(r);
        auto it = slotOf.find(k);
        if (it != slotOf.end()) return it->second;
        int s = currentFuncName_.empty() ? nextGlobalSlot_++ : nextFuncSlot_++;
        slotOf[k] = s;
        return s;
    }
    void markArrayRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) arrayRefs_.insert(keyFor(r));
    }
    void markDictRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) dictRefs_.insert(keyFor(r));
    }
    void markDictStringKey(const IRRef& r, const std::string& key) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP)
            dictStringKeys_.insert(keyFor(r) + "|" + key);
    }
    bool isDictRef(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return dictRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    void markDictStrValued(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) dictStrValued_.insert(keyFor(r));
    }
    bool isDictStrValued(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        return dictStrValued_.count(keyFor(r)) > 0;
    }
    bool isDictStringKey(const IRRef& r, const std::string& key) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        return dictStringKeys_.count(keyFor(r) + "|" + key) > 0;
    }
    void markStringRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) stringRefs_.insert(keyFor(r));
        if (r.kind == IRRef::Kind::VAR && r.id >= 0)
            stringNames_.insert(currentFuncName_ + "#" + prog.symbols.getName(r.id));
    }
    void markFloatRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) floatRefs_.insert(keyFor(r));
    }
    bool isArrayRef(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return arrayRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    void markStrListRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) strListRefs_.insert(keyFor(r));
    }
    void markFloatListRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) floatListRefs_.insert(keyFor(r));
    }
    bool isFloatListRef(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return floatListRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    bool isStrListRef(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return strListRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    void markBoolRef(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP) boolRefs_.insert(keyFor(r));
    }
    bool isBoolRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::BOOL;
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return boolRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    bool isStringRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING) return true;
        if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
            // A field read (`self.name`, `p.name`) whose class stores a string into that field.
            std::string nm = prog.symbols.getName(r.id);
            auto dot = nm.find('.');
            if (dot != std::string::npos) {
                std::string base = nm.substr(0, dot), field = nm.substr(dot + 1);
                std::string cls = (base == "self") ? currentClass_
                                 : (instanceClass_.count(base) ? instanceClass_.at(base) : std::string());
                auto it = stringFields_.find(cls);
                if (!cls.empty() && it != stringFields_.end() && it->second.count(field)) return true;
            }
        }
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        if (r.kind == IRRef::Kind::VAR && r.id >= 0
                && stringNames_.count(currentFuncName_ + "#" + prog.symbols.getName(r.id)) > 0)
            return true;
        return stringRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    // A value that is a float, whether it is a float constant or a variable/temp marked float.
    bool isFloatValue(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
        if (r.kind == IRRef::Kind::VAR && r.id >= 0 && armTextIsFloatLiteral(prog.symbols.getName(r.id))) return true;
        return isFloatRef(r);
    }
    bool isFloatRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
        // A literal hoisted into a variable named by its text (see armLiteralVarValue) is a constant.
        if (r.kind == IRRef::Kind::VAR && r.id >= 0 && armTextIsFloatLiteral(prog.symbols.getName(r.id))) return true;
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return false;
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return floatRefs_.count(currentFuncName_ + "#" + kind + std::to_string(r.id)) > 0;
    }
    static std::string refKeyRaw(const IRRef& r) {
        if (r.kind == IRRef::Kind::VAR) return "v" + std::to_string(r.id);
        if (r.kind == IRRef::Kind::TEMP) return "t" + std::to_string(r.id);
        return "";
    }
    const IRFunction* findFunction(const std::string& name) const {
        for (const auto& fn : prog.functions) {
            if (fn.name == name) return &fn;
        }
        return nullptr;
    }
    void computeStringParamHintsFromLiteralCalls() {
        stringParamHints_.clear();
        floatParamHints_.clear();
        auto scan = [&](const std::vector<IRInstruction>& instrs) {
            for (const auto& ins : instrs) {
                if ((ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL)
                        || ins.typedOperands.empty()) continue;
                std::string callee = callableName(ins.typedOperands[0]);
                const IRFunction* fn = findFunction(callee);
                if (!fn) continue;
                for (size_t i = 1; i < ins.typedOperands.size() && i <= fn->parameters.size(); ++i) {
                    const IRRef& arg = ins.typedOperands[i];
                    if (arg.kind == IRRef::Kind::CONST && arg.value.type == IRType::STRING)
                        stringParamHints_[callee].insert(fn->parameters[i - 1]);
                    // A float literal argument is either a float constant or, after the optimizer
                    // hoists it, a variable named by the literal text ("2.0") — both are floats.
                    const bool floatLiteralVar = arg.kind == IRRef::Kind::VAR && arg.id >= 0
                        && armTextIsFloatLiteral(prog.symbols.getName(arg.id));
                    if ((arg.kind == IRRef::Kind::CONST && arg.value.type == IRType::FLOAT) || floatLiteralVar)
                        floatParamHints_[callee].insert(fn->parameters[i - 1]);
                }
            }
        };
        scan(prog.globalInit);
        for (const auto& fn : prog.functions) scan(fn.instructions);
        // String-ness through calls: a unit's string names are its string parameters, string constants,
        // and element loads from strings; a call that passes one makes the callee's parameter a string.
        // Repeat until no parameter is added (a chain of forwarding calls needs one round per link).
        auto refName = [&](const IRRef& r) -> std::string {
            if (r.kind == IRRef::Kind::VAR && r.id >= 0) return prog.symbols.getName(r.id);
            if (r.kind == IRRef::Kind::TEMP) return "#t" + std::to_string(r.id);
            return "";
        };
        bool stringChanged = true;
        for (int round = 0; stringChanged && round < 16; ++round) {
            stringChanged = false;
            auto unitPass = [&](const std::string& unitName, const std::vector<IRInstruction>& instrs,
                                const std::vector<std::string>& unitParams) {
                std::set<std::string> strs;
                auto hit = stringParamHints_.find(unitName);
                if (hit != stringParamHints_.end()) strs.insert(hit->second.begin(), hit->second.end());
                auto isStr = [&](const IRRef& r) {
                    if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::STRING;
                    return strs.count(refName(r)) > 0;
                };
                bool grew = true;
                while (grew) {
                    grew = false;
                    for (const auto& ins : instrs) {
                        if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2
                                && isStr(ins.typedOperands[1]) && !isStr(ins.typedOperands[0])) {
                            strs.insert(refName(ins.typedOperands[0])); grew = true;
                        } else if (ins.opcode == IROpcode::LOAD_INDEX && ins.result.isValid()
                                && ins.typedOperands.size() >= 2 && isStr(ins.typedOperands[0])
                                && !isStr(ins.result)) {
                            strs.insert(refName(ins.result)); grew = true;
                        }
                    }
                }
                for (const auto& ins : instrs) {
                    // A user function called as a statement is a LIB_CALL: it passes arguments the same way.
                    if ((ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL) || ins.typedOperands.empty()) continue;
                    const std::string callee = callableName(ins.typedOperands[0]);
                    const IRFunction* cf = findFunction(callee);
                    if (!cf) continue;
                    for (size_t i = 1; i < ins.typedOperands.size() && i <= cf->parameters.size(); i++) {
                        if (!isStr(ins.typedOperands[i])) continue;
                        if (stringParamHints_[callee].insert(cf->parameters[i - 1]).second) stringChanged = true;
                    }
                }
                (void)unitParams;
            };
            for (const auto& fn : prog.functions) unitPass(fn.name, fn.instructions, fn.parameters);
            unitPass("", prog.globalInit, {});
        }
        // A parameter compared with a string literal (`IF c is $7$`) is a string, whatever the callers pass:
        // the comparison itself says so, and a caller that passes a string element still sends a string pointer.
        for (const auto& fn : prog.functions) {
            std::set<std::string> params(fn.parameters.begin(), fn.parameters.end());
            for (const auto& ins : fn.instructions) {
                if (ins.opcode != IROpcode::EQ && ins.opcode != IROpcode::NEQ) continue;
                if (ins.typedOperands.size() < 2) continue;
                for (int side = 0; side < 2; side++) {
                    const IRRef& var = ins.typedOperands[side];
                    const IRRef& lit = ins.typedOperands[1 - side];
                    if (var.kind == IRRef::Kind::VAR && var.id >= 0 && lit.kind == IRRef::Kind::CONST
                            && lit.value.type == IRType::STRING) {
                        const std::string name = prog.symbols.getName(var.id);
                        if (params.count(name)) stringParamHints_[fn.name].insert(name);
                    }
                }
            }
        }
    }
    // Fixed-point pre-scan (ported from BNY's preScanFloats): marks every var/temp that is EVER
    // assigned a float value anywhere in this instruction stream, run BEFORE any codegen for it.
    // Verified real bug this closes: without it, float-ness was only ever recorded by "mark as
    // you go" DURING codegen — fine for straight-line code, but a loop body is only compiled
    // ONCE even though it runs many times, so a var that becomes float via a compound update
    // inside a WHILST (`total = total + da`, da float) had its OWN left-operand load compiled
    // BEFORE that same instruction's store got around to marking it float. The compiled load
    // instruction is fixed forever at that point — every subsequent runtime iteration re-read
    // `total`'s (by-then genuinely double) bits and wrongly `scvtf`'d them as if they were a
    // plain integer (verified: `benchmark.ac`'s statsStress loop went from -0.79999... on
    // iteration 1 — coincidentally correct, since 0 and 0.0 share a bit pattern — to a garbage
    // 19-digit "float" by iteration 2, where the accumulator was no longer exactly zero).
    // Parameter positions that some caller passes a float to (keyed by the callee's label).
    std::map<std::string, std::set<size_t>> armParamFloatHints_;
    // Marks the parameters a caller already sends as floats, in the current scope, before its pre-scan.
    // The symbol id a parameter has inside THIS function's own body. lookupAnyScope can return another
    // scope's variable of the same name (the mainloop's `h` for a parameter `h`), so scan the body instead.
    // The symbol id of a name as this unit's own body uses it (any operand or result), or -1.
    int unitSymbolId(const IRFunction& fn, const std::string& name) const {
        int found = -1;
        auto check = [&](const IRRef& r) {
            if (found < 0 && r.kind == IRRef::Kind::VAR && r.id >= 0 && prog.symbols.getName(r.id) == name)
                found = r.id;
        };
        for (const auto& ins : fn.instructions) {
            check(ins.result);
            for (const auto& op : ins.typedOperands) check(op);
            if (found >= 0) break;
        }
        return found;
    }
    int paramSymbolId(const IRFunction& fn, const std::string& pname) const {
        int found = -1;
        auto check = [&](const IRRef& r) {
            if (found < 0 && r.kind == IRRef::Kind::VAR && r.id >= 0 && prog.symbols.getName(r.id) == pname)
                found = r.id;
        };
        for (const auto& ins : fn.instructions) {
            check(ins.result);
            for (const auto& op : ins.typedOperands) check(op);
            if (found >= 0) break;
        }
        return found;
    }
    void seedParamFloats(const std::string& label, const IRFunction& fn) {
        auto it = armParamFloatHints_.find(label);
        if (it == armParamFloatHints_.end()) return;
        for (size_t i : it->second) {
            if (i >= fn.parameters.size()) continue;
            int sid = paramSymbolId(fn, fn.parameters[i]);
            if (sid >= 0) markFloatRef(IRRef::var(sid));
        }
    }
    // Records, for every user-function call in this scope, which argument positions carry a float.
    void recordCallFloatArgs(const std::vector<IRInstruction>& instrs) {
        std::set<std::string> userFns;
        for (const auto& f : prog.functions) userFns.insert(f.classOwner.empty() ? f.name : f.classOwner + "_" + f.name);
        for (const auto& ins : instrs) {
            if ((ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL) || ins.typedOperands.empty()) continue;
            std::string callee = callableName(ins.typedOperands[0]);
            if (!userFns.count(callee)) continue;
            for (size_t i = 1; i < ins.typedOperands.size(); i++)
                if (isFloatValue(ins.typedOperands[i])) armParamFloatHints_[callee].insert(i - 1);
        }
    }
    void preScanFloatVars(const std::vector<IRInstruction>& instrs) {
        bool changed = true;
        while (changed) {
            changed = false;
            auto markIfNew = [&](const IRRef& r) {
                if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return;
                if (isFloatRef(r)) return;
                markFloatRef(r);
                changed = true;
            };
            for (const auto& ins : instrs) {
                if (ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL
                    || ins.opcode == IROpcode::LOAD_CONST) {
                    bool hasResultForm = ins.result.isValid() && !ins.typedOperands.empty();
                    IRRef src = hasResultForm ? ins.typedOperands[0]
                              : (ins.typedOperands.size() >= 2 ? ins.typedOperands[1] : IRRef());
                    bool srcFloat = (src.kind == IRRef::Kind::CONST && src.value.type == IRType::FLOAT)
                                 || isFloatRef(src);
                    // A STORE_VAR whose own resultType is a non-float integer-width qualifier means
                    // the value is being coerced INTO that type at this store, not turning the var
                    // float (mirrors BNY's identical guard — see its comment for the x=5;x=5.5 case).
                    bool coercedNonFloat = ins.resultType != IRType::VOID
                        && ins.resultType != IRType::FLOAT && irIntWidth(ins.resultType);
                    if (srcFloat && !coercedNonFloat) {
                        if (ins.result.isValid()) markIfNew(ins.result);
                        else if (ins.typedOperands.size() >= 2) markIfNew(ins.typedOperands[0]);
                    }
                }
                if (ins.opcode == IROpcode::TYPE_CAST && ins.resultType == IRType::FLOAT
                    && ins.result.isValid())
                    markIfNew(ins.result);
                if ((ins.opcode == IROpcode::DIV || ins.opcode == IROpcode::FDIV) && ins.result.isValid())
                    markIfNew(ins.result);
                if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                  || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::PMUL)
                    && ins.result.isValid() && ins.typedOperands.size() >= 2) {
                    if (isFloatRef(ins.typedOperands[0]) || isFloatRef(ins.typedOperands[1]))
                        markIfNew(ins.result);
                }
                if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL)
                    && ins.result.isValid() && !ins.typedOperands.empty()) {
                    std::string callee = callableName(ins.typedOperands[0]);
                    if (floatReturningFuncs_.count(callee) || armExternalReturnsFloat(callee))
                        markIfNew(ins.result);
                }
            }
        }
    }

    // Two passes: the second sees the call-site float-list hints the first one collected, so a
    // parameter typed by a caller that is analyzed later is still typed in the callee.
    void computeArrayReturningFuncs() {
        floatParamListHints_.clear();
        // Repeat until the hints and return sets stop growing: a chain of forwarding calls (f passes its list
        // to g, g to h) needs one pass per link, and callers are often analyzed after their callees.
        auto signature = [&]() {
            size_t n = floatParamListHints_.size() * 1000 + floatListReturningFuncs_.size() * 100
                     + floatReturningFuncs_.size() * 10 + arrayReturningFuncs_.size();
            for (const auto& [_, set] : floatParamListHints_) n += set.size();
            return n;
        };
        size_t prev = (size_t)-1;
        for (int round = 0; round < 8 && signature() != prev; ++round) {
            prev = signature();
            computeArrayReturningFuncsPass();
        }
    }
    void computeArrayReturningFuncsPass() {
        arrayReturningFuncs_.clear();
        stringReturningFuncs_.clear();
        mixedStringReturningFuncs_.clear();
        floatReturningFuncs_.clear();
        floatListReturningFuncs_.clear();
        computeStringParamHintsFromLiteralCalls();
        // The main program body is not in prog.functions (it lives in globalInit), but it calls
        // user functions and builds lists too, so it is analyzed as one more unit (empty name).
        std::vector<IRFunction> units(prog.functions.begin(), prog.functions.end());
        IRFunction mainUnit("");
        mainUnit.instructions = prog.globalInit;
        units.push_back(std::move(mainUnit));
        for (const auto& fn : units) {
            std::set<std::string> arrays;
            std::set<std::string> strings;
            std::set<std::string> floats;
            auto hit = stringParamHints_.find(fn.name);
            if (hit != stringParamHints_.end()) {
                for (const std::string& param : hit->second) {
                    for (const auto& ins : fn.instructions) {
                        auto seed = [&](const IRRef& r) {
                            if (r.kind == IRRef::Kind::VAR && r.id >= 0
                                    && prog.symbols.getName(r.id) == param)
                                strings.insert(refKeyRaw(r));
                        };
                        seed(ins.result);
                        for (const auto& op : ins.typedOperands) seed(op);
                    }
                }
            }
            auto floatHit = floatParamHints_.find(fn.name);
            if (floatHit != floatParamHints_.end()) {
                for (const std::string& param : floatHit->second) {
                    for (const auto& ins : fn.instructions) {
                        auto seed = [&](const IRRef& r) {
                            if (r.kind == IRRef::Kind::VAR && r.id >= 0
                                    && prog.symbols.getName(r.id) == param)
                                floats.insert(refKeyRaw(r));
                        };
                        seed(ins.result);
                        for (const auto& op : ins.typedOperands) seed(op);
                    }
                }
            }
            // Parameters a caller passes float VARIABLES to (found by the float pre-pass, see compile()).
            const std::string unitLabel = fn.classOwner.empty() ? fn.name : fn.classOwner + "_" + fn.name;
            auto varHit = armParamFloatHints_.find(unitLabel);
            if (varHit != armParamFloatHints_.end()) {
                for (size_t pi : varHit->second) {
                    if (pi >= fn.parameters.size()) continue;
                    const std::string& param = fn.parameters[pi];
                    for (const auto& ins : fn.instructions) {
                        auto seed = [&](const IRRef& r) {
                            if (r.kind == IRRef::Kind::VAR && r.id >= 0 && prog.symbols.getName(r.id) == param)
                                floats.insert(refKeyRaw(r));
                        };
                        seed(ins.result);
                        for (const auto& op : ins.typedOperands) seed(op);
                    }
                }
            }
            // Float lists (see floatListRefs_), typed by the same fixed point as the rest. A parameter
            // starts as a float list when an earlier pass saw a float list passed to it.
            std::set<std::string> floatLists;
            if (floatParamListHints_.count(fn.name))
                for (int i : floatParamListHints_.at(fn.name))
                    if (i >= 0 && (size_t)i < fn.parameters.size()) {
                        int sym = unitSymbolId(fn, fn.parameters[(size_t)i]);
                        if (sym >= 0) floatLists.insert(refKeyRaw(IRRef::var(sym)));
                    }
            bool changed = true;
            while (changed) {
                changed = false;
                auto addFL = [&](const std::string& k) {
                    if (!k.empty() && floatLists.insert(k).second) changed = true;
                };
                for (const auto& ins : fn.instructions) {
                    if (ins.opcode == IROpcode::ALLOC && ins.typedOperands.size() >= 2
                            && ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[1].kind == IRRef::Kind::CONST
                            && ins.typedOperands[1].value.type == IRType::STRING
                            && ins.result.isValid()
                            && armLiteralIsFloatList(std::get<std::string>(ins.typedOperands[1].value.data)))
                        addFL(refKeyRaw(ins.result));
                    else if ((ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL)
                            && ins.typedOperands.size() >= 2) {
                        std::string cn = callableName(ins.typedOperands[0]);
                        const IRRef& val = ins.typedOperands[1];
                        const bool valIsFloat = floats.count(refKeyRaw(val)) > 0
                            || (val.kind == IRRef::Kind::CONST && val.value.type == IRType::FLOAT);
                        if (cn.size() > 7 && cn.compare(cn.size() - 7, 7, ".append") == 0 && valIsFloat) {
                            int sym = unitSymbolId(fn, cn.substr(0, cn.size() - 7));
                            if (sym >= 0) addFL(refKeyRaw(IRRef::var(sym)));
                        }
                    } else if ((ins.opcode == IROpcode::LOAD_VAR || ins.opcode == IROpcode::STORE_VAR
                                || ins.opcode == IROpcode::CONST_DECL)
                            && !ins.typedOperands.empty() && floatLists.count(refKeyRaw(ins.typedOperands.back()))) {
                        if (ins.result.isValid()) addFL(refKeyRaw(ins.result));
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) addFL(refKeyRaw(ins.typedOperands[0]));
                    }
                    // A call to a function that returns a float list yields one (so a forwarding wrapper keeps it).
                    if (ins.opcode == IROpcode::CALL && ins.result.isValid() && !ins.typedOperands.empty()
                            && floatListReturningFuncs_.count(callableName(ins.typedOperands[0])))
                        addFL(refKeyRaw(ins.result));
                    // An element of a float list is a float.
                    if (ins.opcode == IROpcode::LOAD_INDEX && ins.result.isValid() && ins.typedOperands.size() >= 2
                            && floatLists.count(refKeyRaw(ins.typedOperands[0]))) {
                        std::string k = refKeyRaw(ins.result);
                        if (!k.empty() && floats.insert(k).second) changed = true;
                    }
                }
                for (const auto& ins : fn.instructions) {
                    auto addRef = [&](const IRRef& r) {
                        std::string k = refKeyRaw(r);
                        if (!k.empty() && arrays.insert(k).second) changed = true;
                    };
                    auto hasRef = [&](const IRRef& r) {
                        std::string k = refKeyRaw(r);
                        return !k.empty() && arrays.count(k) > 0;
                    };
                    auto addStrRef = [&](const IRRef& r) {
                        std::string k = refKeyRaw(r);
                        if (!k.empty() && strings.insert(k).second) changed = true;
                    };
                    auto hasStrRef = [&](const IRRef& r) {
                        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING) return true;
                        std::string k = refKeyRaw(r);
                        return !k.empty() && strings.count(k) > 0;
                    };
                    auto addFloatRef2 = [&](const IRRef& r) {
                        std::string k = refKeyRaw(r);
                        if (!k.empty() && floats.insert(k).second) changed = true;
                    };
                    auto hasFloatRef2 = [&](const IRRef& r) {
                        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
                        std::string k = refKeyRaw(r);
                        return !k.empty() && floats.count(k) > 0;
                    };
                    if (ins.opcode == IROpcode::ALLOC && !ins.typedOperands.empty()
                            && ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[0].value.type == IRType::STRING) {
                        const std::string& t = std::get<std::string>(ins.typedOperands[0].value.data);
                        if (t == "list" || t == "range" || t == "sequence") addRef(ins.result);
                    } else if ((ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL
                                || ins.opcode == IROpcode::LOAD_VAR)
                            && !ins.typedOperands.empty() && hasRef(ins.typedOperands.back())) {
                        addRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) addRef(ins.typedOperands[0]);
                    }
                    if ((ins.opcode == IROpcode::LOAD_CONST || ins.opcode == IROpcode::LOAD_VAR
                                || ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL)
                            && !ins.typedOperands.empty() && hasStrRef(ins.typedOperands.back())) {
                        addStrRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) addStrRef(ins.typedOperands[0]);
                    } else if (ins.opcode == IROpcode::ADD && ins.typedOperands.size() >= 2
                            && (hasStrRef(ins.typedOperands[0]) || hasStrRef(ins.typedOperands[1]))) {
                        if (ins.resultType == IRType::STRING) {
                            if (hasStrRef(ins.typedOperands[0])) addStrRef(ins.typedOperands[1]);
                            if (hasStrRef(ins.typedOperands[1])) addStrRef(ins.typedOperands[0]);
                        }
                        addStrRef(ins.result);
                    } else if (ins.opcode == IROpcode::FOR_BEGIN && ins.typedOperands.size() >= 2
                            && hasStrRef(ins.typedOperands[1])) {
                        addStrRef(ins.typedOperands[0]);
                    } else if (ins.opcode == IROpcode::LOAD_INDEX && ins.typedOperands.size() >= 2
                            && ins.resultType == IRType::STRING) {
                        addStrRef(ins.typedOperands[0]);
                        addStrRef(ins.result);
                    } else if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()) {
                        std::string cn = callableName(ins.typedOperands[0]);
                        if (arrayReturningFuncs_.count(cn)) addRef(ins.result);
                        if (stringReturningFuncs_.count(cn)) addStrRef(ins.result);
                        if (floatReturningFuncs_.count(cn) || armExternalReturnsFloat(cn)) addFloatRef2(ins.result);
                    } else if (ins.opcode == IROpcode::LIB_CALL && !ins.typedOperands.empty()) {
                        // ilib float results (math.sqrt, math.integrate, ...) feed user functions too.
                        if (armExternalReturnsFloat(callableName(ins.typedOperands[0]))) addFloatRef2(ins.result);
                    }
                    // Float tracking, mirroring the string rules just above — see
                    // floatReturningFuncs_'s own comment for the bug this closes.
                    if (ins.resultType == IRType::FLOAT) {
                        addFloatRef2(ins.result);
                    } else if ((ins.opcode == IROpcode::LOAD_CONST || ins.opcode == IROpcode::LOAD_VAR
                                || ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL)
                            && !ins.typedOperands.empty() && hasFloatRef2(ins.typedOperands.back())) {
                        addFloatRef2(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR) addFloatRef2(ins.typedOperands[0]);
                    } else if (ins.opcode == IROpcode::LOAD_INDEX && !ins.typedOperands.empty()
                            && floatLists.count(refKeyRaw(ins.typedOperands[0]))) {
                        // an element of a float list is a float (a `return xs[i] * xs[i]` must return a float)
                        addFloatRef2(ins.result);
                    } else if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                                || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::DIV
                                || ins.opcode == IROpcode::IDIV || ins.opcode == IROpcode::FDIV)
                            && ins.typedOperands.size() >= 2
                            && (hasFloatRef2(ins.typedOperands[0]) || hasFloatRef2(ins.typedOperands[1]))) {
                        addFloatRef2(ins.result);
                    } else if (ins.opcode == IROpcode::TYPE_CAST && ins.resultType == IRType::FLOAT) {
                        addFloatRef2(ins.result);
                    } else if (ins.opcode == IROpcode::CALL && ins.typedOperands.size() == 2
                            && callableName(ins.typedOperands[0]).empty() && hasFloatRef2(ins.typedOperands[1])) {
                        // a parenthesized float expression (identity CALL with an empty name)
                        addFloatRef2(ins.result);
                    }
                }
            }
            bool anyStringReturn = false;
            bool anyNonStringReturn = false;
            for (const auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()) {
                    if (arrays.count(refKeyRaw(ins.typedOperands[0]))) {
                        arrayReturningFuncs_.insert(fn.name);
                        break;
                    }
                }
            }
            for (const auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()
                        && (floats.count(refKeyRaw(ins.typedOperands[0]))
                            || (ins.typedOperands[0].kind == IRRef::Kind::CONST
                                && ins.typedOperands[0].value.type == IRType::FLOAT))) {
                    floatReturningFuncs_.insert(fn.name);
                    break;
                }
            }
            // Float lists, mirroring the codegen rules (see floatListRefs_): a float-literal ALLOC,
            // or an append of a float value, then followed through loads and stores. A function that
            // returns one is float-list-returning, so its caller's result keeps the typing.
            {
                for (const auto& ins : fn.instructions) {
                    if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()
                            && floatLists.count(refKeyRaw(ins.typedOperands[0]))) {
                        floatListReturningFuncs_.insert(fn.name);
                        break;
                    }
                }
                // A float list passed to a user function types that function's parameter.
                for (const auto& ins : fn.instructions) {
                    if (ins.opcode != IROpcode::CALL || ins.typedOperands.empty()) continue;
                    const std::string cn = callableName(ins.typedOperands[0]);
                    for (size_t a = 1; a < ins.typedOperands.size(); a++)
                        if (floatLists.count(refKeyRaw(ins.typedOperands[a])))
                            floatParamListHints_[cn].insert((int)a - 1);
                }
            }
            for (const auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()) {
                    if (hasStringLiteralOrRaw(strings, ins.typedOperands[0])) anyStringReturn = true;
                    else anyNonStringReturn = true;
                }
            }
            if (anyStringReturn && anyNonStringReturn) mixedStringReturningFuncs_.insert(fn.name);
            else if (anyStringReturn) stringReturningFuncs_.insert(fn.name);
        }
    }

    static bool hasStringLiteralOrRaw(const std::set<std::string>& strings, const IRRef& r) {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING) return true;
        std::string k = refKeyRaw(r);
        return !k.empty() && strings.count(k) > 0;
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
    // Per user function: which parameter positions its body treats as floats (see compileFunction).
    std::map<std::string, std::vector<bool>> armParamFloat_;
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
    int nextInternalLabel_ = -2000;

    // try/catch state (see TRY_BEGIN below). Both live in fixed global slots, so every function
    // reads the same depth and save area no matter how deep the call stack is.
    int tryDepthSlot_ = -1;
    int tryStackSlot_ = -1;

    // Event bindings (`bind <key> to <fn>`, `configure event-listener`): a heap table of
    // [key ptr, handler address] pairs, scanned linearly by EVENT_TRIGGER — the same design as
    // BNY's __ac_bind__/__ac_trigger__. The table is allocated on the first bind.
    int evSlot_ = -1;        // global slot: table pointer (0 until the first bind)
    int evCountSlot_ = -1;   // global slot: number of bindings
    bool evTriggerUsed_ = false;
    static constexpr int EV_MAX = 256;
    struct FuncAdrFixup { size_t bufOff; std::string name; R reg; }; // ADR of a function's entry point
    std::vector<FuncAdrFixup> funcAdrFixups_;
    void ensureEventSlots() {
        if (evSlot_ >= 0) return;
        evSlot_ = nextGlobalSlot_++;
        evCountSlot_ = nextGlobalSlot_++;
    }

    // Generators (`yield`): each generator is a fiber with its own 64 KB stack, ported from BNY's
    // emitFiberSwap design. The state block (heap) holds DONE, VALUE, the generator side's saved
    // SP/X29/X30/X19-X28/resume PC, the caller side's the same, then the arguments. A switch saves
    // the running side into its half, then restores the other side and jumps to its resume PC.
    // The state pointer lives in X17 across the switch, which never restores X17 (so it survives).
    static constexpr int GEN_DONE = 0;
    static constexpr int GEN_VALUE = 8;
    static constexpr int GEN_SIDE_G = 16;      // generator side: SP@+0, X29@+8, X30@+16, X19-X28@+24.., PC@+104
    static constexpr int GEN_SIDE_C = 128;     // caller side, same layout
    static constexpr int GEN_HEADER = 240;     // arguments start here
    static constexpr int GEN_STACK_BYTES = 65536;
    int genCurSlot_ = -1;                      // global slot: the running generator's state block
    bool curFnIsGenerator_ = false;
    void ensureGenSlot() {
        if (genCurSlot_ < 0) genCurSlot_ = nextGlobalSlot_++;
    }
    void emitGenSwap(bool intoGenerator) {
        const int selfBase = intoGenerator ? GEN_SIDE_C : GEN_SIDE_G;
        const int otherBase = intoGenerator ? GEN_SIDE_G : GEN_SIDE_C;
        const int resumeL = nextInternalLabel_--;
        em.mov_from_sp(R::X16);
        em.str_imm(R::X16, R::X17, selfBase);
        em.str_imm(R::X29, R::X17, selfBase + 8);
        em.str_imm(R::X30, R::X17, selfBase + 16);
        for (int i = 0; i < 10; i++) em.str_imm((R)((int)R::X19 + i), R::X17, selfBase + 24 + 8 * i);
        emitBranch(FixKind::ADR, resumeL, Cond::EQ, R::X16);     // this side's resume point
        em.str_imm(R::X16, R::X17, selfBase + 104);
        em.ldr_imm(R::X16, R::X17, otherBase);
        em.mov_sp_from(R::X16);
        em.ldr_imm(R::X29, R::X17, otherBase + 8);
        em.ldr_imm(R::X30, R::X17, otherBase + 16);
        for (int i = 0; i < 10; i++) em.ldr_imm((R)((int)R::X19 + i), R::X17, otherBase + 24 + 8 * i);
        em.ldr_imm(R::X16, R::X17, otherBase + 104);
        em.br(R::X16);
        emitLabel(resumeL);
    }
    // A generator body's return (explicit or the implicit trailing one): mark done, switch back.
    void emitGenReturnSwap() {
        ensureGenSlot();
        em.ldr_imm(R::X17, GLOBALS_BASE, 8 * genCurSlot_);
        em.mov_imm64(R::X9, 1);
        em.str_imm(R::X9, R::X17, GEN_DONE);
        emitGenSwap(false);
    }
    std::vector<int> catchEntry_;   // catch-entry label of each open try (innermost last)
    std::vector<int> catchSkip_;    // label past each catch body, emitted at TRY_END
    static constexpr int TRY_SLOTS = 32;     // nesting depth (same as BNY)
    static constexpr int TRY_SLOT_BYTES = 128; // 14 words used: SP, X29, X30, X19-X28, catch PC
    void ensureTrySlots() {
        if (tryDepthSlot_ >= 0) return;
        tryDepthSlot_ = nextGlobalSlot_++;
        tryStackSlot_ = nextGlobalSlot_++;
    }

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

    void loadStringLiteralPtr(R dst, const std::string& s) {
        int strLabel = nextInternalLabel_--;
        int afterLabel = nextInternalLabel_--;
        emitBranch(FixKind::B, afterLabel);
        emitLabel(strLabel);
        std::vector<uint8_t> data(s.begin(), s.end());
        data.push_back(0);
        em.bytesRaw(data);
        while (em.pos() % 4 != 0) em.byteRaw(0);
        emitLabel(afterLabel);
        auto it = labelOffsets.find(strLabel);
        if (it == labelOffsets.end())
            throw ACError::backend("ARM backend: internal string-label emission failed");
        em.mov_imm64(dst, (int64_t)(codeVA_ + it->second));
    }

    // Loads a plain (non-field) variable NAME's value into `dst` — used to get the object
    // pointer a field access needs to add its offset to (ported from BNY's identical helper).
    void loadNamedVar(const std::string& name, R dst) {
        if (name == "self") {
            em.ldr_imm(dst, R::SP, (uint32_t)(SELF_SLOT * 8));
            return;
        }
        int sid = prog.symbols.lookupAnyScope(name);
        if (sid >= 0) { loadOperand(IRRef::var(sid), dst); return; }
        em.mov_imm64(dst, 0);   // shouldn't happen — name wasn't a known var
    }
    void loadOperand(const IRRef& r, R dst) {
        if (r.kind == IRRef::Kind::TEMP && r.id == pendingTempId_) {
            if (dst != pendingTempReg_) em.mov_reg(dst, pendingTempReg_);
            pendingTempId_ = -1;
            return;
        }
        protectPending(dst);
        if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
            // The optimizer sometimes passes a literal as a variable named by its text ("2.0", "3").
            // Nothing ever stores into such a name, so its value is the literal itself.
            const std::string litName = prog.symbols.getName(r.id);
            if (armTextIsFloatLiteral(litName)) {
                const double dv = std::stod(litName);
                int64_t bits;
                std::memcpy(&bits, &dv, 8);
                em.mov_imm64(dst, bits);
                return;
            }
            if (armTextIsIntLiteral(litName)) {
                em.mov_imm64(dst, std::stoll(litName));
                return;
            }
            std::string fbase; int foff;
            if (resolveFieldAccess(prog.symbols.getName(r.id), fbase, foff)) {
                // self.field / instance.field — real pointer+offset access, not this var's own
                // slot (see classFields_'s comment). Was completely disconnected before: this
                // name would otherwise just get its own local slot like any unrelated plain
                // variable, so a field WRITE would never reach the actual object.
                loadNamedVar(fbase, dst);
                if (foff != 0) em.add_imm(dst, dst, foff);
                em.ldr_imm(dst, dst, 0);
                return;
            }
        }
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::INT) {
            em.mov_imm64(dst, std::get<int64_t>(r.value.data));
        } else if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::BOOL) {
            em.mov_imm64(dst, std::get<bool>(r.value.data) ? 1 : 0);
        } else if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING) {
            // Method-arg constants may still carry the $...$ delimiters (BNY strips them the same way).
            std::string sval = std::get<std::string>(r.value.data);
            if (sval.size() >= 2 && sval.front() == '$' && sval.back() == '$') sval = sval.substr(1, sval.size() - 2);
            loadStringLiteralPtr(dst, sval);
        } else if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::FLOAT) {
            uint64_t bits = 0;
            double value = std::get<double>(r.value.data);
            std::memcpy(&bits, &value, sizeof(bits));
            em.mov_imm64(dst, (int64_t)bits);
        } else if (r.kind == IRRef::Kind::CONST) {
            throw ACError::backend("ARM backend: constant type " + std::to_string((int)r.value.type)
                + " is not yet implemented as a runtime value");
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
        if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
            std::string fbase; int foff;
            if (resolveFieldAccess(prog.symbols.getName(r.id), fbase, foff)) {
                R addr = (src == R::X16) ? R::X15 : R::X16;
                loadNamedVar(fbase, addr);
                if (foff != 0) em.add_imm(addr, addr, foff);
                em.str_imm(src, addr, 0);
                return;
            }
        }
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
            case FixKind::ADR:   em.adr_rel(reg); break;
        }
    }

    static int labelIdOf(const IRRef& r) { return r.id; }

    std::string callableName(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING)
            return std::get<std::string>(r.value.data);
        if ((r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::FUNCTION) && r.id >= 0)
            return prog.symbols.getName(r.id);
        return "";
    }

    std::string describeRef(const IRRef& r) const {
        if (r.kind == IRRef::Kind::CONST) return r.toStringWithSymbols(const_cast<SymbolTable*>(&prog.symbols));
        if ((r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::FUNCTION) && r.id >= 0)
            return prog.symbols.getName(r.id);
        return r.toStringWithSymbols(const_cast<SymbolTable*>(&prog.symbols));
    }

    std::string refKindKeyForMessage(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR && r.kind != IRRef::Kind::TEMP) return "";
        std::string kind = (r.kind == IRRef::Kind::VAR) ? "v" : "t";
        return currentFuncName_ + "#" + kind + std::to_string(r.id);
    }

    std::string refNameKeyForMessage(const IRRef& r) const {
        if (r.kind != IRRef::Kind::VAR || r.id < 0) return "";
        return currentFuncName_ + "#" + prog.symbols.getName(r.id);
    }
    bool isCurrentFunctionParam(const IRRef& r) const {
        return r.kind == IRRef::Kind::VAR && r.id >= 0
            && !currentFuncName_.empty()
            && currentFuncParamNames_.count(prog.symbols.getName(r.id)) > 0;
    }

    void storeBoolFromCond(Cond c, const IRInstruction& ins) {
        em.cset(S0, c);
        storeResult(ins.result, S0);
    }

    void compileTruthNot(const IRInstruction& ins) {
        loadOperand(ins.typedOperands[0], S0);
        em.cmp_reg(S0, R::XZR);
        storeBoolFromCond(Cond::EQ, ins);
    }

    void compileTruthBin(const IRInstruction& ins, IROpcode op) {
        loadOperand(ins.typedOperands[0], S0);
        em.cmp_reg(S0, R::XZR);
        em.cset(S0, Cond::NE);
        loadOperand(ins.typedOperands[1], S1);
        em.cmp_reg(S1, R::XZR);
        em.cset(S1, Cond::NE);
        if (op == IROpcode::AND) em.and_reg(S0, S0, S1);
        else if (op == IROpcode::OR) em.orr_reg(S0, S0, S1);
        else {
            em.eor_reg(S0, S0, S1);
            if (op == IROpcode::XNOR) {
                em.cmp_reg(S0, R::XZR);
                em.cset(S0, Cond::EQ);
            }
        }
        storeResult(ins.result, S0);
    }

    // AArch64's SDIV silently returns 0 on a zero divisor instead of trapping (unlike x86's IDIV,
    // which raises SIGFPE — the behavior BNY's own zero-guard was written to catch). Verified real
    // bug: with no guard at all, `5 / b` (b=0) printed garbage instead of erroring like PY's
    // ZeroDivisionError does, and this was true even for FDIV/DIV (float division by 0.0 doesn't
    // trap either — it silently yields +-inf/nan, which the print routine has no way to render).
    // divisorReg holds either a plain int or the raw bit pattern of a float — 0 means zero either
    // way, so one check covers both DIV's dispatch paths.
    void emitDivZeroGuard(R divisorReg) {
        int skip = nextInternalLabel_--;
        emitBranch(FixKind::CBNZ, skip, Cond::EQ, divisorReg);
        emitBranch(FixKind::BL, DIVZERO_LABEL);
        emitLabel(skip);
    }

    // An integer operand that is float-typed (a variable that also holds a `/` result) is truncated to
    // an integer rather than used as raw float bits. S2 is the scratch D-register for the conversion.
    void loadIntOperand(const IRRef& r, R d) {
        if (isFloatValue(r)) {
            loadFloatOperand(r, S2);
            em.fcvtzs_x_d(d, S2);
        } else {
            loadOperand(r, d);
        }
    }

    void compileIntegerMod(const IRInstruction& ins, size_t lhsIdx, size_t rhsIdx) {
        loadIntOperand(ins.typedOperands[lhsIdx], S0);
        loadIntOperand(ins.typedOperands[rhsIdx], S1);
        emitDivZeroGuard(S1);
        em.sdiv(S2, S0, S1);
        em.msub(S0, S2, S1, S0);          // S0 = a - (a/b)*b: the TRUNCATED remainder (sign of a)
        // AC's `%`/math.mod is floor modulo (sign of the divisor, as PY and the C backend do): when the
        // remainder is nonzero and its sign differs from b's, add b. Without this, -7 mod 3 gave -1.
        int done = nextInternalLabel_--;
        emitBranch(FixKind::CBZ, done, Cond::EQ, S0);
        em.eor_reg(S2, S0, S1);           // sign bit of (r xor b) is set iff the signs differ
        em.cmp_reg(S2, R::XZR);
        emitBranch(FixKind::BCOND, done, Cond::GE);
        em.add_reg(S0, S0, S1);
        emitLabel(done);
        storeResult(ins.result, S0);
    }

    void compileIntegerAbs(const IRInstruction& ins, size_t argIdx) {
        loadOperand(ins.typedOperands[argIdx], S0);
        em.cmp_reg(S0, R::XZR);
        int doneLabel = nextInternalLabel_--;
        emitBranch(FixKind::BCOND, doneLabel, Cond::GE);
        em.neg_reg(S0, S0);
        emitLabel(doneLabel);
        storeResult(ins.result, S0);
    }

    void compileWriteStringLiteral(const std::string& s, bool newline) {
        int strLabel = nextInternalLabel_--;
        int afterLabel = nextInternalLabel_--;
        emitBranch(FixKind::B, afterLabel);
        emitLabel(strLabel);
        std::vector<uint8_t> data(s.begin(), s.end());
        if (newline) data.push_back((uint8_t)'\n');
        em.bytesRaw(data);
        while (em.pos() % 4 != 0) em.byteRaw(0);
        emitLabel(afterLabel);
        auto it = labelOffsets.find(strLabel);
        if (it == labelOffsets.end())
            throw ACError::backend("ARM backend: internal string-label emission failed");
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X1, (int64_t)(codeVA_ + it->second));
        em.mov_imm64(R::X2, (int64_t)data.size());
        em.mov_imm64(R::X8, 64);
        em.svc0();
    }

    void compilePrintStringLiteral(const std::string& s) {
        compileWriteStringLiteral(s, true);
    }

    void compileInput(const IRInstruction& ins) {
        if (!ins.typedOperands.empty()) {
            const IRRef& prompt = ins.typedOperands[0];
            if (prompt.kind == IRRef::Kind::CONST && prompt.value.type == IRType::STRING) {
                compileWriteStringLiteral(std::get<std::string>(prompt.value.data), false);
            } else if (isStringRef(prompt)) {
                loadOperand(prompt, R::X0);
                emitBranch(FixKind::BL, WRITE_CSTR_LABEL);
            } else {
                throw ACError::backend("ARM backend: Term.ask prompt must be a string");
            }
        }

        em.mov_imm64(R::X0, 4096);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X9, R::X0);              // input buffer base
        em.mov_reg(R::X10, R::X0);             // cursor
        em.mov_imm64(R::X11, 0);               // chars accepted
        em.mov_imm64(R::X12, 4095);            // leave room for NUL

        int loopLabel = nextInternalLabel_--;
        int doneLabel = nextInternalLabel_--;
        emitLabel(loopLabel);
        em.cmp_reg(R::X11, R::X12);
        emitBranch(FixKind::BCOND, doneLabel, Cond::GE);
        em.mov_imm64(R::X0, 0);                // stdin
        em.mov_reg(R::X1, R::X10);             // read one byte at cursor
        em.mov_imm64(R::X2, 1);
        em.mov_imm64(R::X8, 63);               // read (AArch64 Linux syscall table)
        em.svc0();
        em.cmp_reg(R::X0, R::XZR);
        emitBranch(FixKind::BCOND, doneLabel, Cond::LE);
        em.ldrb0(R::X13, R::X10);
        em.mov_imm64(R::X14, (int64_t)'\n');
        em.cmp_reg(R::X13, R::X14);
        emitBranch(FixKind::BCOND, doneLabel, Cond::EQ);
        em.add_imm(R::X10, R::X10, 1);
        em.add_imm(R::X11, R::X11, 1);
        emitBranch(FixKind::B, loopLabel);

        emitLabel(doneLabel);
        em.mov_imm64(R::X13, 0);
        em.strb0(R::X13, R::X10);
        storeResult(ins.result, R::X9);
        markStringRef(ins.result);
    }

    void compileRangeAlloc(const IRInstruction& ins, bool isRange) {
        if (!ins.result.isValid())
            throw ACError::backend("ARM backend: range/sequence ALLOC without a result");
        if (isRange) {
            em.mov_imm64(R::X12, 0);                         // start
            if (ins.typedOperands.size() >= 2) loadOperand(ins.typedOperands[1], R::X13);
            else em.mov_imm64(R::X13, 0);
            em.mov_imm64(R::X14, 1);                         // step
        } else {
            if (ins.typedOperands.size() >= 2) loadOperand(ins.typedOperands[1], R::X12);
            else em.mov_imm64(R::X12, 0);
            loadOperand(ins.typedOperands.size() >= 3 ? ins.typedOperands[2] : ins.typedOperands[1], R::X13);
            if (ins.typedOperands.size() >= 4) loadOperand(ins.typedOperands[3], R::X14);
            else em.mov_imm64(R::X14, 1);
        }

        int countDone = nextInternalLabel_--;
        int countLoopAsc = nextInternalLabel_--;
        int countDesc = nextInternalLabel_--;
        int countLoopDesc = nextInternalLabel_--;
        int fillLoop = nextInternalLabel_--;
        int fillDone = nextInternalLabel_--;

        em.mov_imm64(R::X15, 0);                             // count
        em.mov_reg(R::X11, R::X12);                          // iter
        em.cmp_reg(R::X14, R::XZR);
        emitBranch(FixKind::BCOND, countDone, Cond::EQ);
        em.cmp_reg(R::X14, R::XZR);
        emitBranch(FixKind::BCOND, countDesc, Cond::LT);
        emitLabel(countLoopAsc);
        em.cmp_reg(R::X11, R::X13);
        emitBranch(FixKind::BCOND, countDone, Cond::GE);
        em.add_imm(R::X15, R::X15, 1);
        em.add_reg(R::X11, R::X11, R::X14);
        emitBranch(FixKind::B, countLoopAsc);
        emitLabel(countDesc);
        emitLabel(countLoopDesc);
        em.cmp_reg(R::X11, R::X13);
        emitBranch(FixKind::BCOND, countDone, Cond::LE);
        em.add_imm(R::X15, R::X15, 1);
        em.add_reg(R::X11, R::X11, R::X14);
        emitBranch(FixKind::B, countLoopDesc);
        emitLabel(countDone);

        em.add_imm(R::X0, R::X15, 2);
        em.mov_imm64(R::X9, 8);
        em.mul_reg(R::X0, R::X0, R::X9);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.str_imm(R::X15, R::X0, 0);                        // raw[0] = cap
        em.add_imm(R::X0, R::X0, 8);
        em.str_imm(R::X15, R::X0, 0);                        // ptr[0] = len
        em.mov_reg(R::X9, R::X0);                            // ptr
        em.mov_imm64(R::X10, 0);                             // i
        em.mov_reg(R::X11, R::X12);                          // iter
        emitLabel(fillLoop);
        em.cmp_reg(R::X10, R::X15);
        emitBranch(FixKind::BCOND, fillDone, Cond::GE);
        em.add_imm(R::X16, R::X10, 1);
        em.mov_imm64(R::X17, 8);
        em.mul_reg(R::X16, R::X16, R::X17);
        em.add_reg(R::X17, R::X9, R::X16);
        em.str_imm(R::X11, R::X17, 0);
        em.add_reg(R::X11, R::X11, R::X14);
        em.add_imm(R::X10, R::X10, 1);
        emitBranch(FixKind::B, fillLoop);
        emitLabel(fillDone);
        storeResult(ins.result, R::X9);
        markArrayRef(ins.result);
    }

    void preseedValueKinds(const std::vector<IRInstruction>& instrs) {
        bool changed = true;
        while (changed) {
            changed = false;
            size_t arrBefore = arrayRefs_.size();
            size_t strBefore = stringRefs_.size();
            size_t floatBefore = floatRefs_.size();
            size_t boolBefore = boolRefs_.size();
            for (const auto& ins : instrs) {
                if (ins.result.isValid() && ins.resultType == IRType::BOOL) markBoolRef(ins.result);
                if (ins.opcode == IROpcode::ALLOC && !ins.typedOperands.empty()
                        && ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING
                        && std::get<std::string>(ins.typedOperands[0].value.data) == "dict") {
                    markDictRef(ins.result);
                    if (ins.typedOperands.size() >= 2 && ins.typedOperands[1].kind == IRRef::Kind::CONST
                            && ins.typedOperands[1].value.type == IRType::STRING) {
                        std::string content = std::get<std::string>(ins.typedOperands[1].value.data);
                        size_t pos = 0;
                        while (pos < content.size()) {
                            size_t comma = content.find(',', pos);
                            std::string pair = content.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                            size_t colon = pair.find(':');
                            if (colon != std::string::npos) {
                                std::string key = pair.substr(0, colon);
                                size_t a = key.find_first_not_of(" \t"), b = key.find_last_not_of(" \t");
                                if (a != std::string::npos) key = key.substr(a, b - a + 1);
                                if (key.size() >= 2 && key.front() == '$' && key.back() == '$')
                                    key = key.substr(1, key.size() - 2);
                                std::string val = pair.substr(colon + 1);
                                a = val.find_first_not_of(" \t"); b = val.find_last_not_of(" \t");
                                if (a != std::string::npos) val = val.substr(a, b - a + 1);
                                if (val.size() >= 2 && val.front() == '$' && val.back() == '$')
                                    markDictStringKey(ins.result, key);
                            }
                            if (comma == std::string::npos) break;
                            pos = comma + 1;
                        }
                    }
                }
                if (ins.opcode == IROpcode::ALLOC && !ins.typedOperands.empty()
                        && ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING) {
                    const std::string& t = std::get<std::string>(ins.typedOperands[0].value.data);
                    if (t == "list" || t == "range" || t == "sequence") markArrayRef(ins.result);
                }
                if ((ins.opcode == IROpcode::LOAD_CONST || ins.opcode == IROpcode::LOAD_VAR
                            || ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::CONST_DECL)
                        && !ins.typedOperands.empty()) {
                    const IRRef& src = ins.typedOperands.back();
                    if (isArrayRef(src)) {
                        markArrayRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markArrayRef(ins.typedOperands[0]);
                    }
                    if (isFloatListRef(src)) {
                        markFloatListRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markFloatListRef(ins.typedOperands[0]);
                    }
                    if (isStrListRef(src)) {
                        markStrListRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markStrListRef(ins.typedOperands[0]);
                    }
                    if (isBoolRef(src)) {
                        markBoolRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markBoolRef(ins.typedOperands[0]);
                    }
                    if (isDictRef(src)) {
                        markDictRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markDictRef(ins.typedOperands[0]);
                    }
                    if (isStringRef(src)) {
                        markStringRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markStringRef(ins.typedOperands[0]);
                    }
                    // A store's float-ness is its value's: a whole smart-division result folded to an int
                    // constant still carries the division's FLOAT resultType, and must not make the target float.
                    if ((ins.opcode != IROpcode::STORE_VAR && ins.resultType == IRType::FLOAT) || isFloatRef(src)) {
                        markFloatRef(ins.result);
                        if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) markFloatRef(ins.typedOperands[0]);
                    }
                    if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()
                            && isStringRef(ins.typedOperands[0])) {
                        markStringRef(src);
                    }
                }
                if ((ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                            || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::DIV
                            || ins.opcode == IROpcode::IDIV || ins.opcode == IROpcode::FDIV)
                        && ins.resultType == IRType::FLOAT) {
                    markFloatRef(ins.result);
                    for (const auto& op : ins.typedOperands) if (isFloatRef(op)) markFloatRef(op);
                }
                if (ins.opcode == IROpcode::TYPE_CAST && ins.resultType == IRType::FLOAT)
                    markFloatRef(ins.result);
                if (ins.opcode == IROpcode::LOAD_INDEX && ins.typedOperands.size() >= 2
                        && isDictRef(ins.typedOperands[0])) {
                    markDictRef(ins.result);
                    if (ins.typedOperands[1].kind == IRRef::Kind::CONST
                            && ins.typedOperands[1].value.type == IRType::STRING) {
                        std::string key = std::get<std::string>(ins.typedOperands[1].value.data);
                        if (isDictStringKey(ins.typedOperands[0], key)) markStringRef(ins.result);
                    }
                }
                if (ins.opcode == IROpcode::STORE_INDEX && ins.typedOperands.size() >= 3
                        && isDictRef(ins.typedOperands[0])
                        && ins.typedOperands[1].kind == IRRef::Kind::CONST
                        && ins.typedOperands[1].value.type == IRType::STRING
                        && isStringRef(ins.typedOperands[2])) {
                    markDictStringKey(ins.typedOperands[0], std::get<std::string>(ins.typedOperands[1].value.data));
                }
                if (ins.opcode == IROpcode::ADD && ins.typedOperands.size() >= 2
                        && (isStringRef(ins.typedOperands[0]) || isStringRef(ins.typedOperands[1]))) {
                    if (ins.resultType == IRType::STRING) {
                        if (isStringRef(ins.typedOperands[0])) markStringRef(ins.typedOperands[1]);
                        if (isStringRef(ins.typedOperands[1])) markStringRef(ins.typedOperands[0]);
                    }
                    markStringRef(ins.result);
                }
                if (ins.opcode == IROpcode::FOR_BEGIN && ins.typedOperands.size() >= 2
                        && (isStringRef(ins.typedOperands[1]) || !isArrayRef(ins.typedOperands[1]))) {
                    markStringRef(ins.typedOperands[0]);
                }
                if (ins.opcode == IROpcode::LOAD_INDEX && ins.typedOperands.size() >= 2
                        && (ins.resultType == IRType::STRING || isStringRef(ins.result))) {
                    markStringRef(ins.typedOperands[0]);
                    markStringRef(ins.result);
                }
                if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()) {
                    std::string cn = callableName(ins.typedOperands[0]);
                    if (arrayReturningFuncs_.count(cn)) markArrayRef(ins.result);
                    if (stringReturningFuncs_.count(cn)) markStringRef(ins.result);
                    if (floatReturningFuncs_.count(cn)) markFloatRef(ins.result);
                }
            }
            changed = arrayRefs_.size() != arrBefore || stringRefs_.size() != strBefore
                   || floatRefs_.size() != floatBefore || boolRefs_.size() != boolBefore;
        }
    }

    void loadFloatOperand(const IRRef& r, R d) {
        loadOperand(r, R::X17);
        em.fmov_d_from_x(d, R::X17);
        if (!isFloatValue(r)) em.scvtf_d_x(d, R::X17);   // float constants already hold double bits
    }

    void storeFloatResult(const IRRef& r, R d) {
        // X17 is the transport register here, and a pending elided temp may still live in it:
        // spill that first, or the fmov below overwrites it (the two-operand divide bug).
        ensureSpilled();
        em.fmov_x_from_d(R::X17, d);
        storeResult(r, R::X17);
        markFloatRef(r);   // every float result is marked here, so later reads take the float path
        // Verified real bug: ADD/SUB/MUL/DIV's float branches all call this and then just
        // `break` — none of them separately called markFloatRef(r), so a later PRINT (or any
        // other isFloatRef-gated dispatch) on that var/temp couldn't tell it was float and fell
        // through to the plain-int path, printing the raw IEEE-754 bit pattern as a huge
        // integer (e.g. array_average.ac's `s / length arr` computed exactly 5.0/2.333... —
        // the arithmetic was correct — but printed 4617315517961601024). Marking here once
        // fixes every existing and future storeFloatResult call site at once.
        markFloatRef(r);
    }

    bool isFloatOperation(const IRInstruction& ins) const {
        if (ins.resultType == IRType::FLOAT) return true;
        for (const auto& op : ins.typedOperands) if (isFloatRef(op)) return true;
        return false;
    }

    void compileCompare(Cond c, const IRInstruction& ins) {
        loadOperand(ins.typedOperands[0], S0);
        loadOperand(ins.typedOperands[1], S1);
        em.cmp_reg(S0, S1);
        em.cset(S0, c);
        storeResult(ins.result, S0);
    }

    void compileInstr(const IRInstruction& ins) {
        // Bool-ness reaches a value from code emitted after the pre-scan (a `sure` result, say),
        // so a plain load or store carries it along here too.
        if ((ins.opcode == IROpcode::LOAD_VAR || ins.opcode == IROpcode::STORE_VAR
                || ins.opcode == IROpcode::LOAD_CONST) && !ins.typedOperands.empty()
                && isBoolRef(ins.typedOperands.back())) {
            if (ins.result.isValid()) markBoolRef(ins.result);
            if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) markBoolRef(ins.typedOperands[0]);
        }
        // The same for float-ness: a float read from a list or a float computation keeps its
        // type through a variable, so the arithmetic that consumes it takes the float path.
        if ((ins.opcode == IROpcode::LOAD_VAR || ins.opcode == IROpcode::STORE_VAR
                || ins.opcode == IROpcode::LOAD_CONST) && !ins.typedOperands.empty()
                && isFloatRef(ins.typedOperands.back())) {
            if (ins.result.isValid()) markFloatRef(ins.result);
            if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) markFloatRef(ins.typedOperands[0]);
        }
        switch (ins.opcode) {
            // The optimizer constant-folds aggressively (verified: `x@y` for two known-constant
            // operands becomes `t = ldc 85` directly, never reaching a real MUL instruction at
            // all) — LOAD_CONST is just as central to v1's actual test coverage as any
            // arithmetic opcode, not a rare edge case. Float constants (`ldc 3.4`) are a real, documented v1
            // gap (no FP/NEON support yet) — a hard error, not a no-op: silently storing 0
            // instead of the real value is exactly the "compiles clean, wrong at runtime"
            // failure mode this file no longer allows (see the default: case below).
            case IROpcode::LOAD_CONST:
            case IROpcode::LOAD_VAR:
                loadOperand(ins.typedOperands[0], S0);
                storeResult(ins.result, S0);
                if (ins.resultType == IRType::FLOAT || isFloatRef(ins.typedOperands[0])) markFloatRef(ins.result);
                if (isStringRef(ins.typedOperands[0])) markStringRef(ins.result);
                if (isArrayRef(ins.typedOperands[0])) markArrayRef(ins.result);
                break;
            // Structural markers with no runtime semantics of their own — every real AC
            // program has at least a TAG_BEGIN/TAG_END pair around <mainloop> (verified:
            // --stop-after-ir shows `tag_begin "mainloop"` on literally every test case), so
            // these are true no-ops, not gaps, unlike the throwing default: case below.
            // WHILE_BEGIN/WHILE_END are structural markers too: the loop's own control flow is the
            // explicit LABEL/JUMP instructions around them (as in BNY, which ignores them as well).
            case IROpcode::TAG_BEGIN:
            case IROpcode::TAG_END:
            case IROpcode::WHILE_BEGIN:
            case IROpcode::WHILE_END:
            case IROpcode::NOP:
                break;
            // Structural markers only — a class body's real content is its methods (compiled as
            // ordinary, separately-labeled IRFunctions, see compileFunction) and its field
            // defaults (folded into the auto-synthesized `init` method by ir.cpp) — nothing here
            // needs runtime code of its own, same reasoning as TAG_BEGIN/TAG_END above.
            case IROpcode::CLASS_BEGIN:
            case IROpcode::CLASS_END:
                break;
            case IROpcode::CONST_DECL:
                if (!ins.typedOperands.empty()) {
                    loadOperand(ins.typedOperands[0], S0);
                    storeResult(ins.result, S0);
                    if (isArrayRef(ins.typedOperands[0])) markArrayRef(ins.result);
                    if (isStringRef(ins.typedOperands[0])) markStringRef(ins.result);
                }
                break;
            case IROpcode::IF_BEGIN: {
                IfCtx c{nextInternalLabel_--, nextInternalLabel_--, false};
                if (!ins.typedOperands.empty()) loadOperand(ins.typedOperands[0], S0);
                else em.mov_imm64(S0, 0);
                emitBranch(FixKind::CBZ, c.elseLabel, Cond::EQ, S0);
                ifStack_.push_back(c);
                break;
            }
            case IROpcode::IF_ELSE:
                if (ifStack_.empty())
                    throw ACError::backend("ARM backend: IF_ELSE without IF_BEGIN");
                emitBranch(FixKind::B, ifStack_.back().endLabel);
                emitLabel(ifStack_.back().elseLabel);
                ifStack_.back().sawElse = true;
                break;
            case IROpcode::IF_END: {
                if (ifStack_.empty())
                    throw ACError::backend("ARM backend: IF_END without IF_BEGIN");
                auto c = ifStack_.back();
                ifStack_.pop_back();
                if (!c.sawElse) emitLabel(c.elseLabel);
                emitLabel(c.endLabel);
                break;
            }
            case IROpcode::FOR_BEGIN: {
                if (ins.typedOperands.size() < 2)
                    throw ACError::backend("ARM backend: malformed FOR_BEGIN");
                bool stringMode = isStringRef(ins.typedOperands[1])
                    || isStringRef(ins.typedOperands[0])
                    || isCurrentFunctionParam(ins.typedOperands[1])
                    || !isArrayRef(ins.typedOperands[1]);
                ForCtx c{IRRef::temp(nextHiddenTemp_--), IRRef::temp(nextHiddenTemp_--),
                    nextInternalLabel_--, nextInternalLabel_--, stringMode};
                loadOperand(ins.typedOperands[1], S0);
                storeResult(c.arrRef, S0);
                if (c.stringMode) markStringRef(c.arrRef);
                else markArrayRef(c.arrRef);
                if (!c.stringMode && isStrListRef(ins.typedOperands[1])) markStrListRef(c.arrRef);
                if (!c.stringMode && isFloatListRef(ins.typedOperands[1])) markFloatListRef(c.arrRef);
                em.mov_imm64(S0, 0);
                storeResult(c.idxRef, S0);
                emitLabel(c.startLabel);
                if (c.stringMode) {
                    loadOperand(c.arrRef, R::X9);
                    loadOperand(c.idxRef, R::X10);
                    em.add_reg(R::X9, R::X9, R::X10);
                    em.ldrb0(R::X11, R::X9);
                    em.cmp_reg(R::X11, R::XZR);
                    emitBranch(FixKind::BCOND, c.endLabel, Cond::EQ);
                    IRRef charRef = IRRef::temp(nextHiddenTemp_--);
                    storeResult(charRef, R::X11);
                    em.mov_imm64(R::X0, 2);
                    emitBranch(FixKind::BL, ALLOC_LABEL);
                    loadOperand(charRef, R::X11);
                    em.strb0(R::X11, R::X0);
                    em.add_imm(R::X12, R::X0, 1);
                    em.strb0(R::XZR, R::X12);
                    storeResult(ins.typedOperands[0], R::X0);
                    markStringRef(ins.typedOperands[0]);
                } else {
                    loadOperand(c.idxRef, S0);
                    loadOperand(c.arrRef, S1);
                    em.ldr_imm(S1, S1, 0);
                    em.cmp_reg(S0, S1);
                    emitBranch(FixKind::BCOND, c.endLabel, Cond::GE);
                    loadOperand(c.arrRef, R::X9);
                    loadOperand(c.idxRef, R::X10);
                    em.add_imm(R::X10, R::X10, 1);
                    em.mov_imm64(R::X11, 8);
                    em.mul_reg(R::X10, R::X10, R::X11);
                    em.add_reg(R::X9, R::X9, R::X10);
                    em.ldr_imm(R::X9, R::X9, 0);
                    storeResult(ins.typedOperands[0], R::X9);
                    if (isStrListRef(c.arrRef)) markStringRef(ins.typedOperands[0]);
                    if (isFloatListRef(c.arrRef)) markFloatRef(ins.typedOperands[0]);
                }
                forStack_.push_back(c);
                break;
            }
            case IROpcode::FOR_END: {
                if (forStack_.empty())
                    throw ACError::backend("ARM backend: FOR_END without FOR_BEGIN");
                auto c = forStack_.back();
                forStack_.pop_back();
                loadOperand(c.idxRef, S0);
                em.add_imm(S0, S0, 1);
                storeResult(c.idxRef, S0);
                emitBranch(FixKind::B, c.startLabel);
                emitLabel(c.endLabel);
                break;
            }
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
                    // A variable that holds a float (anywhere in the program) keeps float bits: an
                    // integer stored into it is converted, and a float stored into it marks it float.
                    const bool srcFloat = isFloatValue(ins.typedOperands[1]);
                    const bool dstFloat = isFloatRef(ins.typedOperands[0]);
                    if (dstFloat && !srcFloat) {
                        loadOperand(ins.typedOperands[1], R::X0);
                        em.scvtf_d_x(R::X0, R::X0);
                        storeFloatResult(ins.typedOperands[0], R::X0);
                        break;
                    }
                    loadOperand(ins.typedOperands[1], S0);
                    storeResult(ins.typedOperands[0], S0);
                    if (srcFloat) markFloatRef(ins.typedOperands[0]);
                    if (isArrayRef(ins.typedOperands[1])) markArrayRef(ins.typedOperands[0]);
                    if (isStringRef(ins.typedOperands[1])) markStringRef(ins.typedOperands[0]);
                } else if (!ins.typedOperands.empty()) {
                    // Same float rule as the two-operand form above: an integer stored into a float
                    // variable is converted; a float source marks the target float.
                    if (isFloatRef(ins.result) && !isFloatValue(ins.typedOperands[0])) {
                        loadOperand(ins.typedOperands[0], R::X0);
                        em.scvtf_d_x(R::X0, R::X0);
                        storeFloatResult(ins.result, R::X0);
                        break;
                    }
                    loadOperand(ins.typedOperands[0], S0);
                    storeResult(ins.result, S0);
                    if (isFloatValue(ins.typedOperands[0])) markFloatRef(ins.result);
                    if (isArrayRef(ins.typedOperands[0])) markArrayRef(ins.result);
                    if (isStringRef(ins.typedOperands[0])) markStringRef(ins.result);
                } else {
                    throw ACError::backend("ARM backend: malformed STORE_VAR with no operands");
                }
                break;
            }
            case IROpcode::ADD:
                if (ins.typedOperands.size() >= 2 && (isStringRef(ins.typedOperands[0]) || isStringRef(ins.typedOperands[1]))) {
                    if (!isStringRef(ins.typedOperands[0]) || !isStringRef(ins.typedOperands[1]))
                        throw ACError::backend("ARM backend: mixed string/non-string '+' needs to_string support (left="
                            + describeRef(ins.typedOperands[0]) + (isStringRef(ins.typedOperands[0]) ? ":string" : ":non-string")
                            + ", right=" + describeRef(ins.typedOperands[1]) + (isStringRef(ins.typedOperands[1]) ? ":string" : ":non-string")
                            + ", scope=" + (currentFuncName_.empty() ? std::string("<mainloop>") : currentFuncName_)
                            + ", left_key=" + refKindKeyForMessage(ins.typedOperands[0])
                            + ", right_key=" + refKindKeyForMessage(ins.typedOperands[1])
                            + ", left_name_key=" + refNameKeyForMessage(ins.typedOperands[0])
                            + (stringNames_.count(refNameKeyForMessage(ins.typedOperands[0])) ? ":tagged" : ":untagged")
                            + ", result_type=" + std::to_string((int)ins.resultType)
                            + ")");
                    loadOperand(ins.typedOperands[0], R::X0);
                    loadOperand(ins.typedOperands[1], R::X1);
                    emitBranch(FixKind::BL, CONCAT_LABEL);
                    storeResult(ins.result, R::X0);
                    markStringRef(ins.result);
                    break;
                }
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fadd_d(R::X0, R::X0, R::X1);
                    storeFloatResult(ins.result, R::X0);
                    break;
                }
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.add_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::SUB:
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fsub_d(R::X0, R::X0, R::X1);
                    storeFloatResult(ins.result, R::X0);
                    break;
                }
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.sub_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::MUL:
            case IROpcode::PMUL:
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fmul_d(R::X0, R::X0, R::X1);
                    storeFloatResult(ins.result, R::X0);
                    break;
                }
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.mul_reg(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::DIV:
            case IROpcode::IDIV:
            case IROpcode::FDIV:
                if (isFloatOperation(ins) || ins.opcode == IROpcode::FDIV) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fmov_x_from_d(R::X15, R::X1);
                    emitDivZeroGuard(R::X15);
                    em.fdiv_d(R::X0, R::X0, R::X1);
                    storeFloatResult(ins.result, R::X0);
                    break;
                }
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                emitDivZeroGuard(S1);
                em.sdiv(S0, S0, S1); storeResult(ins.result, S0);
                break;
            case IROpcode::MOD:
                compileIntegerMod(ins, 0, 1);
                break;
            case IROpcode::AND:
            case IROpcode::OR:
            case IROpcode::XOR:
            case IROpcode::XNOR:
                compileTruthBin(ins, ins.opcode);
                break;
            case IROpcode::NOT:
                compileTruthNot(ins);
                break;
            case IROpcode::XSUB:
                loadOperand(ins.typedOperands[0], S0); loadOperand(ins.typedOperands[1], S1);
                em.sub_reg(S0, S0, S1);
                em.cmp_reg(S0, R::XZR);
                {
                    int doneLabel = nextInternalLabel_--;
                    emitBranch(FixKind::BCOND, doneLabel, Cond::GE);
                    em.neg_reg(S0, S0);
                    emitLabel(doneLabel);
                }
                em.add_imm(S0, S0, 1);
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
            case IROpcode::DICT_HAS:
                // `dict has key`: the pair scan returns 1/0 in X0.
                loadOperand(ins.typedOperands[0], R::X0);
                loadOperand(ins.typedOperands[1], R::X1);
                emitBranch(FixKind::BL, DICT_HAS_LABEL);
                storeResult(ins.result, R::X0);
                break;
            case IROpcode::WILDCARD_MATCH:
                // `subject % pattern`: the always-emitted matcher returns 1/0 in X0.
                loadOperand(ins.typedOperands[0], R::X0);
                loadOperand(ins.typedOperands[1], R::X1);
                emitBranch(FixKind::BL, WILDCARD_LABEL);
                storeResult(ins.result, R::X0);
                break;
            case IROpcode::EQ:
                if (ins.typedOperands.size() >= 2
                        && isStringRef(ins.typedOperands[0]) && isStringRef(ins.typedOperands[1])) {
                    loadOperand(ins.typedOperands[0], R::X0);
                    loadOperand(ins.typedOperands[1], R::X1);
                    emitBranch(FixKind::BL, STREQ_LABEL);
                    storeResult(ins.result, R::X0);
                    break;
                }
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fcmp_d(R::X0, R::X1);
                    em.cset(S0, Cond::EQ);
                    storeResult(ins.result, S0);
                    break;
                }
                compileCompare(Cond::EQ, ins);
                break;
            case IROpcode::NEQ:
                if (ins.typedOperands.size() >= 2
                        && isStringRef(ins.typedOperands[0]) && isStringRef(ins.typedOperands[1])) {
                    loadOperand(ins.typedOperands[0], R::X0);
                    loadOperand(ins.typedOperands[1], R::X1);
                    emitBranch(FixKind::BL, STREQ_LABEL);
                    em.cmp_reg(R::X0, R::XZR);
                    storeBoolFromCond(Cond::EQ, ins);
                    break;
                }
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fcmp_d(R::X0, R::X1);
                    em.cset(S0, Cond::NE);
                    storeResult(ins.result, S0);
                    break;
                }
                compileCompare(Cond::NE, ins);
                break;
            case IROpcode::LT:
            case IROpcode::GT:
            case IROpcode::LTE:
            case IROpcode::GTE:
                if (isFloatOperation(ins)) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    loadFloatOperand(ins.typedOperands[1], R::X1);
                    em.fcmp_d(R::X0, R::X1);
                    em.cset(S0, ins.opcode == IROpcode::LT ? Cond::LT
                              : ins.opcode == IROpcode::GT ? Cond::GT
                              : ins.opcode == IROpcode::LTE ? Cond::LE : Cond::GE);
                    storeResult(ins.result, S0);
                } else if (ins.opcode == IROpcode::LT) compileCompare(Cond::LT, ins);
                else if (ins.opcode == IROpcode::GT) compileCompare(Cond::GT, ins);
                else if (ins.opcode == IROpcode::LTE) compileCompare(Cond::LE, ins);
                else compileCompare(Cond::GE, ins);
                break;
            // AAPCS64: first 8 integer args in X0..X7, return value in X0. v1 caps calls at 8
            // args (register-passed only, no stack-passed overflow yet) — matches this file's
            // "get a real subset working end to end" scope; a 9th argument is a real gap, not
            // silently dropped (see the args.size()>8 check below).
            // arr.append(value): grow the receiver array in place (or reallocate, per
            // __ac_append__'s own capacity check) and write the resulting pointer back into the
            // receiver var. There's no dedicated APPEND opcode — ir.cpp lowers a plain
            // `arr.append(x)` statement to a LIB_CALL (verified via --stop-after-ir:
            // `lib_call arr.append, 40`, typedOperands[0] a VAR named "arr.append", not a CALL
            // as BNY's own ".append" comment might suggest at a skim — BNY handles both CALL AND
            // LIB_CALL through one shared emitLibCall-style dispatch, so this same suffix-match
            // needs to run from both opcodes here too, not just one).
            case IROpcode::CALL:
            case IROpcode::LIB_CALL: {
                std::string calleeName = callableName(ins.typedOperands[0]);
                if (calleeName == "import") break;
                // A parenthesized expression is lowered as a CALL with an empty name wrapping one
                // operand: an identity. It carries the operand's type so later uses take the same path.
                if (calleeName.empty() && ins.typedOperands.size() == 2 && ins.result.isValid()) {
                    const IRRef& src = ins.typedOperands[1];
                    if (isFloatRef(src)) {
                        loadFloatOperand(src, R::X0);
                        storeFloatResult(ins.result, R::X0);
                    } else {
                        loadOperand(src, S0);
                        storeResult(ins.result, S0);
                        if (isStringRef(src)) markStringRef(ins.result);
                        if (isArrayRef(src)) markArrayRef(ins.result);
                        if (isBoolRef(src)) markBoolRef(ins.result);
                    }
                    break;
                }
                // header.display is a styled Term.display: the same plain text line on every backend
                // (see ir.cpp's Term.display lowering), so ARM prints it as a PRINT instead of linking.
                if (calleeName == "header.display" && ins.typedOperands.size() >= 2) {
                    IRInstruction printIns(IROpcode::PRINT);
                    printIns.typedOperands = {ins.typedOperands[1]};
                    compileInstr(printIns);
                    break;
                }
                if (calleeName == "foreign")
                    throw ACError::fluencyInCPU();
                if (mixedStringReturningFuncs_.count(calleeName))
                    throw ACError::backend("ARM backend: function '" + calleeName
                        + "' mixes string and non-string returns; tagged dynamic returns are not yet implemented");
                if (calleeName == "ac_length" && ins.typedOperands.size() >= 2) {
                    loadOperand(ins.typedOperands[1], R::X0);
                    emitBranch(FixKind::BL, isStringRef(ins.typedOperands[1]) ? STRLEN_LABEL : LENGTH_LABEL);
                    storeResult(ins.result, R::X0);
                    break;
                }
                // Browser-only statements. Off the browser they do nothing, which is what the PY
                // reference does too (BackendStrategy's defaults are silent no-ops), except `sure`,
                // which prints its prompt and answers False so `result = sure $x$` stays defined.
                if (calleeName == "print_page") break;
                if (calleeName == "alert" && ins.typedOperands.size() >= 2) break;
                if (calleeName == "sure" && ins.typedOperands.size() >= 2) {
                    loadOperand(ins.typedOperands[1], R::X0);
                    emitBranch(FixKind::BL, PRINT_CSTR_LABEL);
                    if (ins.result.isValid()) {
                        storeResult(ins.result, R::XZR);
                        markBoolRef(ins.result);
                    }
                    break;
                }
                // Legacy widget constructor sugar such as dimensions(480x520) is compile-time
                // metadata consumed by the following Screen call. It has no native runtime
                // operation of its own, so preserve the value as a harmless zero handle.
                if (calleeName == "dimensions") {
                    if (ins.result.isValid()) storeResult(ins.result, R::XZR);
                    break;
                }
                // ARM has no separate lightweight-thread runtime yet. Match the other
                // synchronous backends by executing quickthread's target function normally;
                // this preserves program behavior while making the keyword available.
                if (calleeName == "quickthread" && ins.typedOperands.size() >= 2) {
                    std::string target = callableName(ins.typedOperands[1]);
                    const size_t nArgs = ins.typedOperands.size() - 2;
                    if (nArgs > 8)
                        throw ACError::backend("ARM backend: quickthread calls with more than 8 arguments are not implemented");
                    static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                    for (size_t i = 0; i < nArgs; i++) loadOperand(ins.typedOperands[i + 2], argRegs[i]);
                    callFixups.push_back({em.pos(), target});
                    em.bl_rel(0);
                    if (ins.result.isValid()) storeResult(ins.result, R::X0);
                    break;
                }
                if ((calleeName == "math.mod_int" || calleeName == "math_mod_int")
                        && ins.typedOperands.size() >= 3) {
                    compileIntegerMod(ins, 1, 2);
                    break;
                }
                if ((calleeName == "math.mod" || calleeName == "math_mod")
                        && ins.typedOperands.size() >= 3) {
                    if (!isFloatRef(ins.typedOperands[1]) && !isFloatRef(ins.typedOperands[2])) {
                        // Integer modulo of two non-float operands is always exact — matches PY's
                        // own math_mod wrapper ("int-exact when operands and result are whole"),
                        // which always returns a genuine int for integer operands since int-mod
                        // can never be fractional. See armExternalReturnsFloat's comment.
                        compileIntegerMod(ins, 1, 2);
                        break;
                    }
                    // At least one operand is float: genuine (possibly fractional) remainder,
                    // C's fmod semantics: a - trunc(a/b)*b (result takes the sign of a).
                    loadFloatOperand(ins.typedOperands[1], R::X0);
                    loadFloatOperand(ins.typedOperands[2], R::X1);
                    em.fmov_x_from_d(R::X15, R::X1);
                    emitDivZeroGuard(R::X15);
                    em.fdiv_d(R::X2, R::X0, R::X1);
                    em.fcvtzs_x_d(R::X16, R::X2);      // truncate quotient toward zero
                    em.scvtf_d_x(R::X2, R::X16);
                    em.fmul_d(R::X2, R::X2, R::X1);
                    em.fsub_d(R::X0, R::X0, R::X2);
                    storeFloatResult(ins.result, R::X0);
                    break;
                }
                if ((calleeName == "math.to_int" || calleeName == "math_to_int") && ins.typedOperands.size() >= 2) {
                    if (isFloatRef(ins.typedOperands[1])) {
                        // Truncate toward zero (fcvtzs), the same conversion as Python's int(float).
                        loadFloatOperand(ins.typedOperands[1], R::X0);
                        em.fcvtzs_x_d(R::X0, R::X0);
                    } else {
                        loadOperand(ins.typedOperands[1], R::X0);
                    }
                    storeResult(ins.result, R::X0);
                    break;
                }
                if ((calleeName == "math.abs_int" || calleeName == "math_abs_int")
                        && ins.typedOperands.size() >= 2) {
                    compileIntegerAbs(ins, 1);
                    break;
                }
                if ((calleeName == "math.abs" || calleeName == "math_abs")
                        && ins.typedOperands.size() >= 2) {
                    // Always float-returning (matches PY's plain `_d1('ac_abs')` wrapper — math.abs
                    // has no smart int-when-exact shortcut the way math.mod's own wrapper does).
                    // Verified real bug: this used to alias compileIntegerAbs (like math.abs_int),
                    // which stores a raw int64 — a later float-typed read of the result would then
                    // reinterpret those bits as garbage IEEE-754 instead of the real converted value.
                    if (isFloatRef(ins.typedOperands[1])) {
                        loadFloatOperand(ins.typedOperands[1], R::X0);
                        em.fmov_x_from_d(R::X15, R::X0);
                        em.mov_imm64(R::X16, 0x7FFFFFFFFFFFFFFFLL);
                        em.and_reg(R::X15, R::X15, R::X16);   // clear sign bit
                        em.fmov_d_from_x(R::X0, R::X15);
                        storeFloatResult(ins.result, R::X0);
                    } else {
                        loadOperand(ins.typedOperands[1], S0);
                        em.cmp_reg(S0, R::XZR);
                        int doneLabel = nextInternalLabel_--;
                        emitBranch(FixKind::BCOND, doneLabel, Cond::GE);
                        em.neg_reg(S0, S0);
                        emitLabel(doneLabel);
                        em.scvtf_d_x(R::X0, S0);
                        storeFloatResult(ins.result, R::X0);
                    }
                    break;
                }
                // AC's eval(expr) is the shared math library evaluator on native backends.
                // The ARM ABI returns the double bits in X0, which is also the representation
                // used by the backend's float slots.
                if (calleeName == "eval" || calleeName == "math.eval") {
                    if (ins.typedOperands.size() < 2)
                        throw ACError::backend("ARM backend: eval requires an expression argument");
                    loadExternalArgs({ins.typedOperands[1]}, "ac_eval");
                    callFixups.push_back({em.pos(), "math.eval"});
                    em.bl_rel(0);
                    em.fmov_x_from_d(R::X0, R::X0);            // the double result comes back in D0
                    storeResult(ins.result, R::X0);
                    markFloatRef(ins.result);
                    break;
                }
                // Widget methods are represented with the source variable name in the IR
                // (for example root.dimensions), while the native symbol is stable.
                if (calleeName.size() > 11
                        && calleeName.compare(calleeName.size() - 11, 11, ".dimensions") == 0
                        && ins.typedOperands.size() >= 3) {
                    std::string receiver = calleeName.substr(0, calleeName.size() - 11);
                    int receiverId = prog.symbols.lookupAnyScope(receiver);
                    if (receiverId < 0)
                        throw ACError::backend("ARM backend: widget receiver '" + receiver + "' is unresolved");
                    loadOperand(IRRef::var(receiverId), R::X0);
                    loadOperand(ins.typedOperands[1], R::X1);
                    loadOperand(ins.typedOperands[2], R::X2);
                    callFixups.push_back({em.pos(), "widgets.screen_dimensions"});
                    em.bl_rel(0);
                    break;
                }
                // Widget methods keep their source receiver in the callee name (for example
                // pos_drop.add), while the C++ ilib exposes universal runtime-dispatch helpers.
                // Resolve the receiver slot here and let the library inspect its actual widget
                // kind; this covers dropdowns, listboxes, tables, labels, and textboxes without
                // duplicating widget type tracking in the ARM backend.
                auto widgetReceiver = [&](const std::string& name) -> int {
                    auto dot = name.rfind('.');
                    if (dot == std::string::npos || dot == 0) return -1;
                    return prog.symbols.lookupAnyScope(name.substr(0, dot));
                };
                auto widgetCall = [&](const std::string& linkName, int receiverId,
                                      size_t firstArg, bool returnsString) {
                    loadOperand(IRRef::var(receiverId), R::X0);
                    size_t nArgs = ins.typedOperands.size() > firstArg
                        ? ins.typedOperands.size() - firstArg : 0;
                    if (nArgs > 1)
                        throw ACError::backend("ARM backend: widget method has too many arguments");
                    if (nArgs) loadOperand(ins.typedOperands[firstArg], R::X1);
                    callFixups.push_back({em.pos(), linkName});
                    em.bl_rel(0);
                    if (ins.result.isValid()) {
                        storeResult(ins.result, R::X0);
                        if (returnsString) markStringRef(ins.result);
                    }
                };
                if (calleeName.size() > 4) {
                    const std::string method = calleeName.substr(calleeName.rfind('.') + 1);
                    int receiverId = widgetReceiver(calleeName);
                    if (receiverId >= 0 && (method == "line" || method == "rect"
                            || method == "circle" || method == "text_at")) {
                        const size_t expected = method == "line" || method == "rect" ? 7
                            : method == "circle" ? 6 : 6;
                        if (ins.typedOperands.size() < expected + 1)
                            throw ACError::backend("ARM backend: sketch." + method + " needs more arguments");
                        loadOperand(IRRef::var(receiverId), R::X0);
                        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                        size_t floatArgs = method == "line" || method == "rect" ? 4
                            : method == "circle" ? 3 : 2;
                        for (size_t i = 0; i < expected; i++) {
                            if (i < floatArgs) loadFloatOperand(ins.typedOperands[i + 1], argRegs[i + 1]);
                            else loadOperand(ins.typedOperands[i + 1], argRegs[i + 1]);
                        }
                        callFixups.push_back({em.pos(), method == "line" ? "widgets.sketch_line"
                            : method == "rect" ? "widgets.sketch_rect"
                            : method == "circle" ? "widgets.sketch_circle" : "widgets.sketch_text"});
                        em.bl_rel(0);
                        break;
                    }
                    if (receiverId >= 0 && method == "add" && ins.typedOperands.size() >= 2) {
                        widgetCall("widgets.add", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "pack") {
                        widgetCall("widgets.pack", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "get") {
                        widgetCall("widgets.get", receiverId, 1, true);
                        break;
                    }
                    if (receiverId >= 0 && method == "set" && ins.typedOperands.size() >= 2) {
                        bool numeric = ins.typedOperands[1].kind == IRRef::Kind::CONST
                            && ins.typedOperands[1].value.type != IRType::STRING;
                        if (numeric) {
                            loadOperand(IRRef::var(receiverId), R::X0);
                            loadFloatOperand(ins.typedOperands[1], R::X1);
                            callFixups.push_back({em.pos(), "widgets.set_d"});
                            em.bl_rel(0);
                        } else {
                            widgetCall("widgets.set", receiverId, 1, false);
                        }
                        break;
                    }
                    if (receiverId >= 0 && method == "mainloop") {
                        widgetCall("widgets.screen_mainloop", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "update") {
                        widgetCall("widgets.update", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "destroy") {
                        widgetCall("widgets.destroy", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "clear") {
                        widgetCall("widgets.sketch_clear", receiverId, 1, false);
                        break;
                    }
                    if (receiverId >= 0 && method == "add_tab" && ins.typedOperands.size() >= 2) {
                        widgetCall("widgets.tabs_add_tab", receiverId, 1, false);
                        break;
                    }
                }
                if (calleeName.size() > 7 && calleeName.compare(calleeName.size() - 7, 7, ".append") == 0
                        && ins.typedOperands.size() >= 2) {
                    std::string recv = calleeName.substr(0, calleeName.size() - 7);
                    int symId = prog.symbols.lookupAnyScope(recv);
                    if (symId >= 0) {
                        IRRef arrRef = IRRef::var(symId);
                        loadOperand(arrRef, R::X0);
                        loadOperand(ins.typedOperands[1], R::X1);
                        emitBranch(FixKind::BL, APPEND_LABEL);
                        storeResult(arrRef, R::X0);
                        if (isFloatRef(ins.typedOperands[1])) markFloatListRef(arrRef);
                        break;
                    }
                }
                // `p.method(args)` / `c.greet()` — instance method call on a var known (via
                // instanceClass_) to hold a constructed bundle. Must run before the generic
                // LIB_CALL/external-symbol dispatch below, which would otherwise reject it as an
                // unknown ilib call (no function is ever literally named "p.method").
                {
                    auto dot = calleeName.find('.');
                    if (dot != std::string::npos) {
                        std::string recv = calleeName.substr(0, dot);
                        std::string mname = calleeName.substr(dot + 1);
                        auto instIt = instanceClass_.find(recv);
                        if (instIt != instanceClass_.end()) {
                            static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                            size_t nArgs = ins.typedOperands.size() - 1;
                            if (nArgs > 7)
                                throw ACError::backend("ARM backend: method calls with more than 7 arguments are not yet implemented");
                            for (size_t ai = 0; ai < nArgs; ai++) loadOperand(ins.typedOperands[1 + ai], argRegs[ai + 1]);
                            loadNamedVar(recv, R::X0);   // self, loaded last: args may use X0-adjacent scratch
                            callFixups.push_back({em.pos(), instIt->second + "_" + mname});
                            em.bl_rel(0);
                            if (ins.result.isValid()) storeResult(ins.result, R::X0);
                            break;
                        }
                    }
                }
                // `ClassName(args)` — bundle construction: allocate a flat 8-bytes-per-field
                // block (no header, same bump allocator arrays use) and call the class's
                // auto-synthesized `init` unconditionally (ir.cpp always emits one, even with no
                // user-written `init` method, to run field-default initializers). Ported from
                // BNY's identical design — see classFields_'s own comment.
                if (classFields_.count(calleeName)) {
                    int n = (int)classFields_[calleeName].size();
                    em.mov_imm64(R::X0, 8 * (n > 0 ? n : 1));
                    emitBranch(FixKind::BL, ALLOC_LABEL);
                    IRRef selfTmp = IRRef::temp(nextHiddenTemp_--);
                    storeResult(selfTmp, R::X0);   // stash the new object ptr across the init call
                    static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                    size_t nArgs = ins.typedOperands.size() - 1;
                    if (nArgs > 7)
                        throw ACError::backend("ARM backend: constructors with more than 7 arguments are not yet implemented");
                    for (size_t ai = 0; ai < nArgs; ai++) loadOperand(ins.typedOperands[1 + ai], argRegs[ai + 1]);
                    loadOperand(selfTmp, R::X0);
                    callFixups.push_back({em.pos(), calleeName + "_init"});
                    em.bl_rel(0);
                    loadOperand(selfTmp, R::X0);
                    if (ins.result.isValid()) {
                        storeResult(ins.result, R::X0);
                        if (ins.result.kind == IRRef::Kind::VAR && ins.result.id >= 0)
                            instanceClass_[prog.symbols.getName(ins.result.id)] = calleeName;
                    }
                    break;
                }
                // Native ilib calls use the same AArch64 ABI as ordinary calls. Previously all
                // LIB_CALL instructions were rejected here even when armExternalName already
                // had a valid mapping, making machine-audio and the other native ilibs dead code
                // on ARM.
                // A list-returning ilib function can arrive as either LIB_CALL or CALL (e.g.
                // `names = os.listdir(...)` lowers to CALL); both must take the list path.
                if (ins.opcode == IROpcode::LIB_CALL || armStrListReturning(calleeName)) {
                    std::string exportName, library;
                    if (armExternalName(calleeName, exportName, library)) {
                        if (armStrListReturning(calleeName)) {
                            emitCStrListCall(ins, calleeName);
                            break;
                        }
                        size_t nArgs = ins.typedOperands.size() - 1;
                        if (nArgs > 8)
                            throw ACError::backend("ARM backend: calls with more than 8 arguments are not yet implemented");
                        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                        loadExternalArgs(std::vector<IRRef>(ins.typedOperands.begin() + 1, ins.typedOperands.end()), exportName);
                        callFixups.push_back({em.pos(), calleeName});
                        em.bl_rel(0);
                        if (armDoubleReturningExport(exportName)) em.fmov_x_from_d(R::X0, R::X0);
                        if (armIntReturningExport(exportName)) em.sxtw(R::X0, R::X0);
                        if (ins.result.isValid()) {
                            if (armExternalReturnsString(calleeName)) emitBranch(FixKind::BL, STRDUP_LABEL);
                            storeResult(ins.result, R::X0);
                            if (armExternalReturnsString(calleeName)) markStringRef(ins.result);
                            if (armExternalReturnsFloat(calleeName)) markFloatRef(ins.result);
                            if (armBoolReturningIr(calleeName)) markBoolRef(ins.result);
                        }
                        break;
                    }
                    // Not a real ilib/widget external symbol. ir.cpp also lowers an ORDINARY
                    // user-function call to this same LIB_CALL shape whenever its result is
                    // discarded (a bare statement call, e.g. `show(p1)` where `show` returns
                    // void) — verified real bug: every such call was rejected here as "not yet
                    // implemented" even though the function itself compiled fine, because this
                    // branch never considered anything but the external-symbol case. Fall through
                    // to the generic user-function dispatch below instead of rejecting it.
                }
                static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
                size_t nArgs = ins.typedOperands.size() - 1;
                if (nArgs > 8)
                    throw ACError::backend("ARM backend: calls with more than 8 arguments are not yet implemented");
                std::string linkName = calleeName;
                if (calleeName == "stringm.strip" && nArgs == 3)
                    linkName = "stringm.strip_clause";
                std::string exportName, library;
                const bool isExternal = armExternalName(calleeName, exportName, library);
                if (isExternal) {
                    loadExternalArgs(std::vector<IRRef>(ins.typedOperands.begin() + 1, ins.typedOperands.end()), exportName);
                } else {
                    auto pit = armParamFloat_.find(calleeName);
                    for (size_t i = 0; i < nArgs; i++) {
                        const IRRef& a = ins.typedOperands[1+i];
                        const bool wantFloat = pit != armParamFloat_.end() && i < pit->second.size() && pit->second[i];
                        if (wantFloat && !isFloatValue(a)) {
                            // The callee's body uses this parameter as a float: send the integer as a double.
                            loadOperand(a, S0);
                            em.scvtf_d_x(S2, S0);
                            em.fmov_x_from_d(argRegs[i], S2);
                        } else {
                            loadOperand(a, argRegs[i]);
                        }
                    }
                }
                callFixups.push_back({em.pos(), linkName});
                em.bl_rel(0);
                if (isExternal) {
                    if (armDoubleReturningExport(exportName)) em.fmov_x_from_d(R::X0, R::X0);
                    if (armIntReturningExport(exportName)) em.sxtw(R::X0, R::X0);
                }
                if (ins.result.kind != IRRef::Kind::NONE) {
                    if (armExternalReturnsString(calleeName)) emitBranch(FixKind::BL, STRDUP_LABEL);
                    storeResult(ins.result, R::X0);
                    if (arrayReturningFuncs_.count(calleeName)) markArrayRef(ins.result);
                    if (floatListReturningFuncs_.count(calleeName)) markFloatListRef(ins.result);
                    if (stringReturningFuncs_.count(calleeName)) markStringRef(ins.result);
                    if (armExternalReturnsString(calleeName)) markStringRef(ins.result);
                    if (armExternalReturnsFloat(calleeName)) markFloatRef(ins.result);
                    if (armBoolReturningIr(calleeName)) markBoolRef(ins.result);
                    if (floatReturningFuncs_.count(calleeName)) markFloatRef(ins.result);
                    // `q = f()` where f always constructs+returns one bundle class
                    // (classReturnFuncs_) — same treatment as a direct construct-call, even
                    // though the instance arrived across a function-return boundary. Verified
                    // real bug: without this, `q.x` after `q = f()` silently read a field offset
                    // from whatever garbage/zeroed memory q's own (wrongly untracked) slot held.
                    if (ins.result.kind == IRRef::Kind::VAR && ins.result.id >= 0) {
                        auto crf = classReturnFuncs_.find(calleeName);
                        if (crf != classReturnFuncs_.end())
                            instanceClass_[prog.symbols.getName(ins.result.id)] = crf->second;
                    }
                }
                break;
            }
            // arr = alloc "list", "e0,e1,..." -> heap block [cap][len][e0][e1]... (see
            // emitAllocRoutine's header comment for the layout, ported from BNY's identical
            // ALLOC "list" site in exp_bny.cpp). v1.2 scope: integer-literal elements and
            // plain-variable-name elements (their CURRENT value, loaded at construction time)
            // — function-pointer-array elements (`funcs = [f1, f2]`), like BNY supports, are
            // not yet implemented here.
            case IROpcode::ALLOC: {
                const auto& opsA = ins.typedOperands;
                if (opsA.empty() || opsA[0].kind != IRRef::Kind::CONST
                        || opsA[0].value.type != IRType::STRING)
                    throw ACError::backend("ARM backend: ALLOC of a non-constant/non-string type is not yet implemented");
                const std::string& allocType = std::get<std::string>(opsA[0].value.data);
                if (allocType == "dict") {
                    struct Pair { std::string key; std::string value; };
                    std::vector<Pair> pairs;
                    if (opsA.size() >= 2 && opsA[1].kind == IRRef::Kind::CONST
                            && opsA[1].value.type == IRType::STRING) {
                        std::string content = std::get<std::string>(opsA[1].value.data);
                        size_t pos = 0;
                        while (pos < content.size()) {
                            size_t comma = content.find(',', pos);
                            std::string pair = content.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                            size_t colon = pair.find(':');
                            if (colon != std::string::npos) {
                                Pair p{pair.substr(0, colon), pair.substr(colon + 1)};
                                auto trim = [](std::string& s) {
                                    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
                                    s = a == std::string::npos ? "" : s.substr(a, b - a + 1);
                                };
                                trim(p.key); trim(p.value);
                                if (p.key.size() >= 2 && p.key.front() == '$' && p.key.back() == '$')
                                    p.key = p.key.substr(1, p.key.size() - 2);
                                pairs.push_back(std::move(p));
                            }
                            if (comma == std::string::npos) break;
                            pos = comma + 1;
                        }
                    }
                    int64_t cap = std::max<int64_t>(8, (int64_t)pairs.size());
                    em.mov_imm64(R::X0, (cap * 2 + 1) * 8 + 8);
                    emitBranch(FixKind::BL, ALLOC_LABEL);
                    em.mov_reg(R::X9, R::X0);
                    em.mov_imm64(R::X10, cap);
                    em.str_imm(R::X10, R::X9, 0);
                    em.add_imm(R::X9, R::X9, 8);
                    em.mov_imm64(R::X10, 0);
                    em.str_imm(R::X10, R::X9, 0);
                    storeResult(ins.result, R::X9);
                    markDictRef(ins.result);
                    bool allStrVals = !pairs.empty();
                    for (const Pair& p : pairs)
                        if (!(p.value.size() >= 2 && p.value.front() == '$' && p.value.back() == '$')) allStrVals = false;
                    if (allStrVals) markDictStrValued(ins.result);
                    for (const Pair& p : pairs) {
                        loadOperand(ins.result, R::X0);
                        loadStringLiteralPtr(R::X1, p.key);
                        if (p.value.size() >= 2 && p.value.front() == '$' && p.value.back() == '$') {
                            loadStringLiteralPtr(R::X2, p.value.substr(1, p.value.size() - 2));
                            markDictStringKey(ins.result, p.key);
                        } else {
                            int64_t v = 0;
                            try { size_t used = 0; v = std::stoll(p.value, &used); if (used != p.value.size()) v = 0; }
                            catch (...) { v = 0; }
                            em.mov_imm64(R::X2, v);
                        }
                        emitBranch(FixKind::BL, DICT_SET_LABEL);
                        storeResult(ins.result, R::X0);
                    }
                    break;
                }
                if (allocType != "list")
                {
                    if ((allocType == "range" || allocType == "sequence")) {
                        compileRangeAlloc(ins, allocType == "range");
                        break;
                    }
                    throw ACError::backend("ARM backend: ALLOC \"" + allocType
                        + "\" is not yet implemented (only \"list\" — plain arrays — so far)");
                }
                struct Elem { bool isVar; int64_t val; std::string varName; };
                std::vector<Elem> elems;
                int floatLiteralElems = 0;
                if (opsA.size() >= 2 && opsA[1].kind == IRRef::Kind::CONST
                        && opsA[1].value.type == IRType::STRING) {
                    const std::string& s = std::get<std::string>(opsA[1].value.data);
                    size_t i = 0;
                    while (i < s.size()) {
                        size_t j = s.find(',', i);
                        std::string tok = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
                        size_t a = tok.find_first_not_of(" \t");
                        size_t b = tok.find_last_not_of(" \t");
                        if (a != std::string::npos) {
                            tok = tok.substr(a, b - a + 1);
                            try {
                                size_t consumed = 0;
                                int64_t v = std::stoll(tok, &consumed);
                                if (consumed == tok.size()) elems.push_back({false, v, ""});
                                else {
                                    // A float literal element: stored as its IEEE-754 bits (the same
                                    // raw 64-bit form a float variable uses).
                                    size_t consumedD = 0;
                                    double dv = std::stod(tok, &consumedD);
                                    if (consumedD != tok.size()) throw std::invalid_argument(tok);
                                    int64_t bits;
                                    std::memcpy(&bits, &dv, 8);
                                    elems.push_back({false, bits, ""});
                                    floatLiteralElems++;
                                }
                            } catch (...) { elems.push_back({true, 0, tok}); }
                        }
                        if (j == std::string::npos) break;
                        i = j + 1;
                    }
                }
                int64_t N = (int64_t)elems.size();
                int64_t CAP0 = N > 4 ? N : 4;
                em.mov_imm64(R::X0, (CAP0 + 2) * 8);
                emitBranch(FixKind::BL, ALLOC_LABEL);
                em.mov_reg(R::X9, R::X0);              // X9 = raw block
                em.mov_imm64(R::X10, CAP0);
                em.str_imm(R::X10, R::X9, 0);          // raw[0] = cap
                em.add_imm(R::X9, R::X9, 8);           // X9 = ptr (skip cap word)
                em.mov_imm64(R::X10, N);
                em.str_imm(R::X10, R::X9, 0);          // ptr[0] = length
                for (int64_t k = 0; k < N; k++) {
                    if (elems[k].isVar) {
                        int symId = prog.symbols.lookupAnyScope(elems[k].varName);
                        if (symId < 0)
                            throw ACError::backend("ARM backend: array element '" + elems[k].varName
                                + "' is not a known variable");
                        loadOperand(IRRef::var(symId), R::X10);
                    } else {
                        em.mov_imm64(R::X10, elems[k].val);
                    }
                    em.str_imm(R::X10, R::X9, (uint32_t)(8 * (k + 1)));
                }
                if (floatLiteralElems > 0 && floatLiteralElems != N)
                    throw ACError::backend("ARM backend: a list literal mixing float and non-float elements is not yet implemented");
                if (ins.result.isValid()) {
                    storeResult(ins.result, R::X9);
                    markArrayRef(ins.result);
                    if (floatLiteralElems > 0) markFloatListRef(ins.result);
                }
                break;
            }
            case IROpcode::FREE:
                break; // no-op — the bump allocator never frees, same as BNY
            // `free x`/`bound x` (AC's free/bound scoping system) — a genuine no-op at the
            // point of direct inline encounter everywhere else in the codebase too (see
            // ir_codegen.cpp: "handled at function-begin time, not inline" — the real effect is
            // a separate whole-function pre-pass that changes how OTHER instructions reference
            // that variable, not a runtime action at this instruction's own position). At
            // mainloop scope (currentFuncName_ empty) this is doubly moot: every mainloop var
            // already lives in the flat globals page regardless of free/bound status. Real
            // promotion of a `bound` var used INSIDE a function's own per-call stack frame isn't
            // implemented yet — a separate, non-array-related gap from today's array work.
            case IROpcode::FREE_DECL:
                break;
            case IROpcode::TYPE_CAST:
                if (ins.typedOperands.empty())
                    throw ACError::backend("ARM backend: malformed TYPE_CAST");
                if (ins.resultType == IRType::FLOAT) {
                    if (ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[0].value.type == IRType::INT) {
                        double value = (double)std::get<int64_t>(ins.typedOperands[0].value.data);
                        uint64_t bits = 0;
                        std::memcpy(&bits, &value, sizeof(bits));
                        em.mov_imm64(R::X0, (int64_t)bits);
                        storeResult(ins.result, R::X0);
                    } else {
                        if (isFloatRef(ins.typedOperands[0])) {
                            loadFloatOperand(ins.typedOperands[0], R::X0);
                        } else {
                            loadOperand(ins.typedOperands[0], R::X0);
                            em.scvtf_d_x(R::X0, R::X0);
                        }
                        storeFloatResult(ins.result, R::X0);
                    }
                    markFloatRef(ins.result);
                    break;
                }
                if (ins.resultType == IRType::STRING) {
                    if (isFloatRef(ins.typedOperands[0])) {
                        // to_string(<float>): a smart value (whole `/` result, math.mod) shows as
                        // "4"/"1.5", a hard float as "4.0"/"1.5" — same routine Term.display uses.
                        bool smart = false;
                        for (const auto& a : ins.attrs) if (a == "smart") smart = true;
                        loadFloatOperand(ins.typedOperands[0], R::X0);
                        emitBranch(FixKind::BL, smart ? FLOAT_TOSTR_SMART_LABEL : FLOAT_TOSTR_LABEL);
                        storeResult(ins.result, R::X0);
                        markStringRef(ins.result);
                        break;
                    }
                    // Already text (a string constant or a string-typed variable, e.g. an f-string
                    // interpolating a name): the value passes through unchanged.
                    if ((ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[0].value.type == IRType::STRING)
                            || isStringRef(ins.typedOperands[0])) {
                        loadOperand(ins.typedOperands[0], R::X0);
                    } else {
                        loadOperand(ins.typedOperands[0], R::X0);
                        emitBranch(FixKind::BL, INT_TO_CSTR_LABEL);
                    }
                    storeResult(ins.result, R::X0);
                    markStringRef(ins.result);
                    break;
                }
                if (ins.resultType != IRType::BOOL && isStringRef(ins.typedOperands[0])) {
                    loadOperand(ins.typedOperands[0], R::X0);
                    emitBranch(FixKind::BL, CSTR_TO_INT_LABEL);
                    storeResult(ins.result, R::X0);
                    break;
                }
                if (ins.resultType == IRType::BOOL) {
                    if (isStringRef(ins.typedOperands[0])) {
                        loadOperand(ins.typedOperands[0], R::X0);
                        emitBranch(FixKind::BL, CSTR_TO_INT_LABEL);
                        em.cmp_reg(R::X0, R::XZR);
                        em.cset(S0, Cond::NE);
                        storeResult(ins.result, S0);
                    } else if (isFloatRef(ins.typedOperands[0])) {
                        loadFloatOperand(ins.typedOperands[0], R::X0);
                        em.fmov_d_from_x(R::X1, R::XZR);
                        em.fcmp_d(R::X0, R::X1);
                        em.cset(S0, Cond::NE);
                        storeResult(ins.result, S0);
                    } else {
                        loadOperand(ins.typedOperands[0], S0);
                        em.cmp_reg(S0, R::XZR);
                        storeBoolFromCond(Cond::NE, ins);
                    }
                } else {
                    if (isFloatRef(ins.typedOperands[0])) {
                        loadFloatOperand(ins.typedOperands[0], R::X0);
                        em.fcvtzs_x_d(R::X0, R::X0);
                        storeResult(ins.result, R::X0);
                    } else {
                        loadOperand(ins.typedOperands[0], S0);
                        storeResult(ins.result, S0);
                    }
                }
                break;
            // arr[idx] (0-based in IR) -> block[1+idx]; special index "__len__" -> block[0].
            // Strings share the same IR opcode: s["__len__"] lowers to strlen(s), while s[i]
            // returns a heap-backed one-character string.
            case IROpcode::LOAD_INDEX: {
                const auto& opsA = ins.typedOperands;
                if (!ins.result.isValid() || opsA.size() < 2)
                    throw ACError::backend("ARM backend: malformed LOAD_INDEX");
                bool isLen = opsA.size() >= 2 && opsA[1].kind == IRRef::Kind::CONST
                          && opsA[1].value.type == IRType::STRING
                          && std::get<std::string>(opsA[1].value.data) == "__len__";
                bool stringIndex = isStringRef(opsA[0]) || ins.resultType == IRType::STRING;
                if (!isLen && isStrListRef(opsA[0])) markStringRef(ins.result);
                if (!isLen && isFloatListRef(opsA[0])) markFloatRef(ins.result);
                if (!isLen && opsA.size() >= 2 && isDictRef(opsA[0])) {
                    loadOperand(opsA[0], R::X0);
                    if (opsA[1].kind == IRRef::Kind::CONST && opsA[1].value.type == IRType::STRING)
                        loadStringLiteralPtr(R::X1, std::get<std::string>(opsA[1].value.data));
                    else loadOperand(opsA[1], R::X1);
                    emitBranch(FixKind::BL, DICT_GET_LABEL);
                    storeResult(ins.result, R::X0);
                    if (opsA[1].kind == IRRef::Kind::CONST && opsA[1].value.type == IRType::STRING
                            && isDictStringKey(opsA[0], std::get<std::string>(opsA[1].value.data)))
                        markStringRef(ins.result);
                    if (isDictStrValued(opsA[0])) markStringRef(ins.result);   // variable key: every value is a string
                    break;
                }
                loadOperand(opsA[0], R::X9);
                if (stringIndex && isLen) {
                    em.mov_reg(R::X0, R::X9);
                    emitBranch(FixKind::BL, STRLEN_LABEL);
                    storeResult(ins.result, R::X0);
                    break;
                }
                if (stringIndex) {
                    loadOperand(opsA[1], R::X10);
                    em.add_reg(R::X9, R::X9, R::X10);
                    em.ldrb0(R::X11, R::X9);
                    IRRef charRef = IRRef::temp(nextHiddenTemp_--);
                    storeResult(charRef, R::X11);
                    em.mov_imm64(R::X0, 2);
                    emitBranch(FixKind::BL, ALLOC_LABEL);
                    loadOperand(charRef, R::X11);
                    em.strb0(R::X11, R::X0);
                    em.add_imm(R::X12, R::X0, 1);
                    em.strb0(R::XZR, R::X12);
                    storeResult(ins.result, R::X0);
                    markStringRef(ins.result);
                    break;
                }
                if (isLen) {
                    em.ldr_imm(R::X9, R::X9, 0);       // length = ptr[0]
                } else if (opsA.size() >= 2) {
                    loadOperand(opsA[1], R::X10);      // X10 = index (0-based)
                    emitWrapNegativeIndex(R::X10, R::X9);
                    em.add_imm(R::X10, R::X10, 1);     // +1 (skip length header)
                    em.mov_imm64(R::X11, 8);
                    em.mul_reg(R::X10, R::X10, R::X11);
                    em.add_reg(R::X9, R::X9, R::X10);
                    em.ldr_imm(R::X9, R::X9, 0);       // element value
                }
                storeResult(ins.result, R::X9);
                break;
            }
            // arr[idx] = val (0-based in IR) — same address math as LOAD_INDEX above.
            case IROpcode::STORE_INDEX: {
                const auto& opsA = ins.typedOperands;
                if (opsA.size() < 3)
                    throw ACError::backend("ARM backend: malformed STORE_INDEX");
                if (isDictRef(opsA[0])) {
                    loadOperand(opsA[0], R::X0);
                    if (opsA[1].kind == IRRef::Kind::CONST && opsA[1].value.type == IRType::STRING)
                        loadStringLiteralPtr(R::X1, std::get<std::string>(opsA[1].value.data));
                    else loadOperand(opsA[1], R::X1);
                    loadOperand(opsA[2], R::X2);
                    emitBranch(FixKind::BL, DICT_SET_LABEL);
                    storeResult(opsA[0], R::X0);
                    break;
                }
                loadOperand(opsA[0], R::X9);           // X9 = array ptr
                loadOperand(opsA[1], R::X10);          // X10 = index (0-based)
                emitWrapNegativeIndex(R::X10, R::X9);
                em.add_imm(R::X10, R::X10, 1);
                em.mov_imm64(R::X11, 8);
                em.mul_reg(R::X10, R::X10, R::X11);
                em.add_reg(R::X9, R::X9, R::X10);
                loadOperand(opsA[2], R::X12);          // X12 = value
                em.str_imm(R::X12, R::X9, 0);
                break;
            }
            case IROpcode::RETURN:
                if (curFnIsGenerator_) { emitGenReturnSwap(); break; }
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
            case IROpcode::INPUT:
                compileInput(ins);
                break;
            case IROpcode::PRINT:
                if (ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING) {
                    // The keyword literals reach the backend as the strings "null" and "nil"; PY (the
                    // reference) prints them as None and set(), and the same mapping covers a
                    // $null$/$nil$ literal, which PY also prints that way.
                    std::string text = std::get<std::string>(ins.typedOperands[0].value.data);
                    if (text == "null") text = "None";
                    else if (text == "nil") text = "set()";
                    compilePrintStringLiteral(text);
                    break;
                }
                if (isBoolRef(ins.typedOperands[0])) {
                    const int falseL = nextInternalLabel_--;
                    const int doneL = nextInternalLabel_--;
                    loadOperand(ins.typedOperands[0], S0);
                    emitBranch(FixKind::CBZ, falseL, Cond::EQ, S0);
                    compilePrintStringLiteral("True");
                    emitBranch(FixKind::B, doneL);
                    emitLabel(falseL);
                    compilePrintStringLiteral("False");
                    emitLabel(doneL);
                    break;
                }
                if (isStringRef(ins.typedOperands[0])) {
                    loadOperand(ins.typedOperands[0], R::X0);
                    emitBranch(FixKind::BL, PRINT_CSTR_LABEL);
                    break;
                }
                if (isArrayRef(ins.typedOperands[0])) {
                    loadOperand(ins.typedOperands[0], R::X0);
                    emitBranch(FixKind::BL, PRINT_ARR_LABEL);
                    break;
                }
                if (isFloatRef(ins.typedOperands[0])) {
                    loadFloatOperand(ins.typedOperands[0], R::X0);
                    // ir.cpp's smart-division pass tags a print of a value that came from `/`: a
                    // whole value shows as an integer (8/2.0 is 4), a fractional one as %.16g.
                    bool smart = false;
                    for (const auto& a : ins.attrs) if (a == "smart") smart = true;
                    emitBranch(FixKind::BL, smart ? PRINT_FLOAT_SMART_LABEL : PRINT_FLOAT_LABEL);
                    break;
                }
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
            case IROpcode::EVAL: {
                if (ins.typedOperands.empty())
                    throw ACError::backend("ARM backend: eval requires an expression argument");
                // eval() is the math ilib's evaluator; ARM has no ilib linking yet (roadmap Phase 7),
                // so calling it would jump to an unresolved target. Refuse at compile time.
                throw ACError::backend("eval() is not implemented in the ARM backend (no ilib linking yet)");
            }
            // try/catch (setjmp-style, ported from BNY's TRY_BEGIN/CATCH_BEGIN). TRY_BEGIN saves
            // SP, X29, X30, X19-X28 and the catch address into the slot for the current depth,
            // then increments depth. The only raise site is division by zero (emitDivZeroTrapRoutine),
            // which unwinds to the innermost catch. CATCH_BEGIN is reached by fallthrough after a
            // normal try body (decrement depth, skip the catch body) or by the trap (which has
            // already restored state), so the catch body runs directly after the entry label.
            case IROpcode::TRY_BEGIN: {
                ensureTrySlots();
                const int haveBuf = nextInternalLabel_--;
                em.ldr_imm(R::X9, GLOBALS_BASE, 8 * tryStackSlot_);
                emitBranch(FixKind::CBNZ, haveBuf, Cond::EQ, R::X9);
                em.mov_imm64(R::X0, TRY_SLOTS * TRY_SLOT_BYTES);
                emitBranch(FixKind::BL, ALLOC_LABEL);
                em.str_imm(R::X0, GLOBALS_BASE, 8 * tryStackSlot_);
                em.mov_reg(R::X9, R::X0);
                emitLabel(haveBuf);
                em.ldr_imm(R::X10, GLOBALS_BASE, 8 * tryDepthSlot_);   // X10 = depth
                em.mov_imm64(R::X11, TRY_SLOT_BYTES);
                em.mul_reg(R::X11, R::X10, R::X11);
                em.add_reg(R::X12, R::X9, R::X11);                    // X12 = this depth's slot
                em.mov_from_sp(R::X13);
                em.str_imm(R::X13, R::X12, 0);                        // SP
                em.str_imm(R::X29, R::X12, 8);
                em.str_imm(R::X30, R::X12, 16);
                for (int i = 0; i < 10; i++)                          // X19..X28
                    em.str_imm((R)((int)R::X19 + i), R::X12, 24 + 8 * i);
                const int catchL = nextInternalLabel_--;
                emitBranch(FixKind::ADR, catchL, Cond::EQ, R::X13);   // X13 = catch entry address
                em.str_imm(R::X13, R::X12, 104);
                em.add_imm(R::X10, R::X10, 1);
                em.str_imm(R::X10, GLOBALS_BASE, 8 * tryDepthSlot_);
                catchEntry_.push_back(catchL);
                break;
            }
            case IROpcode::CATCH_BEGIN: {
                ensureTrySlots();
                em.ldr_imm(R::X9, GLOBALS_BASE, 8 * tryDepthSlot_);
                em.sub_imm(R::X9, R::X9, 1);
                em.str_imm(R::X9, GLOBALS_BASE, 8 * tryDepthSlot_);
                const int skipL = nextInternalLabel_--;
                emitBranch(FixKind::B, skipL);
                catchSkip_.push_back(skipL);
                if (!catchEntry_.empty()) { emitLabel(catchEntry_.back()); catchEntry_.pop_back(); }
                // Only the trap reaches this point (the fallthrough branched over it), so the catch
                // variable gets the error text here, the way PY's `report err` binds str(exception).
                if (!ins.typedOperands.empty() && ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING) {
                    int symId = prog.symbols.lookupAnyScope(std::get<std::string>(ins.typedOperands[0].value.data));
                    if (symId >= 0) {
                        loadStringLiteralPtr(R::X0, "division by zero");
                        IRRef errVar = IRRef::var(symId);
                        storeResult(errVar, R::X0);
                        markStringRef(errVar);
                    }
                }
                break;
            }
            case IROpcode::AFTER_BEGIN:
            case IROpcode::TRY_END:
                if (!catchSkip_.empty()) { emitLabel(catchSkip_.back()); catchSkip_.pop_back(); }
                break;
            // `raise Clause($msg$)`: "Clause: msg" on stderr, then execution continues. Non-fatal on
            // every backend and on PY (the reference) — BNY's own exit(1) for custom clauses was a bug.
            case IROpcode::RAISE_CLAUSE: {
                std::string clause = (!ins.typedOperands.empty() && ins.typedOperands[0].kind == IRRef::Kind::CONST
                                      && ins.typedOperands[0].value.type == IRType::STRING)
                                     ? std::get<std::string>(ins.typedOperands[0].value.data) : "Preposterous";
                std::string msg = (ins.typedOperands.size() > 1 && ins.typedOperands[1].kind == IRRef::Kind::CONST
                                   && ins.typedOperands[1].value.type == IRType::STRING)
                                  ? std::get<std::string>(ins.typedOperands[1].value.data) : "";
                if (clause == "hint") clause = "Suggestion"; else if (clause == "toxic") clause = "Toxic";
                std::string line = clause + ": " + msg + "\n";
                loadStringLiteralPtr(R::X1, line);
                em.mov_imm64(R::X0, 2);                              // stderr
                em.mov_imm64(R::X2, (int64_t)line.size());
                em.mov_imm64(R::X8, 64);                             // write
                em.svc0();
                break;
            }
            // `atomic` brackets. ARM has no concurrency at all (quickthread runs its body synchronously,
            // there are no other threads), so there is nothing to exclude: the lock is a true no-op
            // here. A real spinlock belongs with real threads, not before them.
            case IROpcode::LOCK_BEGIN:
            case IROpcode::LOCK_END:
                break;
            // `sleep n` (seconds, int or float): nanosleep(n) through a heap timespec.
            case IROpcode::SLEEP: {
                if (ins.typedOperands.empty())
                    throw ACError::backend("ARM backend: malformed SLEEP");
                const IRRef& secs = ins.typedOperands[0];
                // The whole-seconds and nanoseconds parts end up in X12 and X13 (ALLOC clobbers X9-X11).
                if (secs.kind == IRRef::Kind::CONST && secs.value.type == IRType::STRING) {
                    // `/halt n` carries its argument as text, parsed here: "2" or "0.25".
                    const std::string& text = std::get<std::string>(secs.value.data);
                    int64_t whole = 0, nanos = 0;
                    size_t consumed = 0;
                    try {
                        long long v = std::stoll(text, &consumed);
                        if (consumed == text.size()) whole = v;
                        else throw std::invalid_argument(text);
                    } catch (...) {
                        size_t c2 = 0;
                        double d = 0;
                        try { d = std::stod(text, &c2); } catch (...) { c2 = 0; }
                        if (c2 != text.size())
                            throw ACError::backend("ARM backend: sleep argument '" + text + "' is not a number");
                        whole = (int64_t)d;
                        nanos = (int64_t)std::llround((d - (double)whole) * 1e9);
                    }
                    em.mov_imm64(R::X12, whole);
                    em.mov_imm64(R::X13, nanos);
                } else if (isFloatRef(secs)) {
                    loadFloatOperand(secs, R::X0);            // D0 = seconds
                    em.fcvtzs_x_d(R::X12, R::X0);             // whole seconds (truncate)
                    em.scvtf_d_x(R::X1, R::X12);              // D1 = whole seconds as a double
                    em.fsub_d(R::X0, R::X0, R::X1);           // D0 = fractional part
                    em.mov_imm64(R::X11, 0x41CDCD6500000000LL); // IEEE-754 bits of 1e9
                    em.fmov_d_from_x(R::X1, R::X11);
                    em.fmul_d(R::X0, R::X0, R::X1);           // D0 = nanoseconds
                    em.fcvtzs_x_d(R::X13, R::X0);
                } else {
                    loadOperand(secs, R::X12);
                    em.mov_imm64(R::X13, 0);
                }
                em.mov_imm64(R::X0, 16);
                emitBranch(FixKind::BL, ALLOC_LABEL);
                em.mov_reg(R::X9, R::X0);                     // X9 = timespec {sec, nsec}
                em.str_imm(R::X12, R::X9, 0);
                em.str_imm(R::X13, R::X9, 8);
                em.mov_reg(R::X0, R::X9);
                em.mov_imm64(R::X1, 0);                       // no remainder buffer
                em.mov_imm64(R::X8, 101);                     // nanosleep (AArch64 Linux syscall table)
                em.svc0();
                break;
            }
            case IROpcode::EVENT_BIND: {
                if (ins.typedOperands.size() < 2 || ins.typedOperands[0].kind != IRRef::Kind::CONST
                        || ins.typedOperands[0].value.type != IRType::STRING)
                    throw ACError::backend("ARM backend: malformed EVENT_BIND");
                std::string key = std::get<std::string>(ins.typedOperands[0].value.data);
                if (key.size() >= 2 && key.front() == '$' && key.back() == '$') key = key.substr(1, key.size() - 2);
                ensureEventSlots();
                const int haveTable = nextInternalLabel_--;
                const int room = nextInternalLabel_--;
                em.ldr_imm(R::X9, GLOBALS_BASE, 8 * evSlot_);
                emitBranch(FixKind::CBNZ, haveTable, Cond::EQ, R::X9);
                em.mov_imm64(R::X0, EV_MAX * 16);
                emitBranch(FixKind::BL, ALLOC_LABEL);
                em.str_imm(R::X0, GLOBALS_BASE, 8 * evSlot_);
                em.mov_reg(R::X9, R::X0);
                emitLabel(haveTable);
                em.ldr_imm(R::X10, GLOBALS_BASE, 8 * evCountSlot_);     // X10 = count
                em.mov_imm64(R::X11, EV_MAX);
                em.sub_reg(R::X11, R::X11, R::X10);                    // room left
                emitBranch(FixKind::CBNZ, room, Cond::EQ, R::X11);
                // Table full: hard stop, like every other runtime limit here (no silent drop).
                const std::string fullMsg = "Preposterous: too many event bindings (limit 256)\n";
                loadStringLiteralPtr(R::X1, fullMsg);
                em.mov_imm64(R::X0, 2);
                em.mov_imm64(R::X2, (int64_t)fullMsg.size());
                em.mov_imm64(R::X8, 64);
                em.svc0();
                em.mov_imm64(R::X0, 1);
                em.mov_imm64(R::X8, 94);
                em.svc0();
                emitLabel(room);
                em.mov_imm64(R::X11, 16);
                em.mul_reg(R::X11, R::X10, R::X11);
                em.add_reg(R::X12, R::X9, R::X11);                     // X12 = new entry
                loadStringLiteralPtr(R::X13, key);
                em.str_imm(R::X13, R::X12, 0);
                funcAdrFixups_.push_back({em.pos(), callableName(ins.typedOperands[1]), R::X13});
                em.adr_rel(R::X13);                                    // X13 = handler address
                em.str_imm(R::X13, R::X12, 8);
                em.add_imm(R::X10, R::X10, 1);
                em.str_imm(R::X10, GLOBALS_BASE, 8 * evCountSlot_);
                break;
            }
            case IROpcode::EVENT_TRIGGER: {
                if (ins.typedOperands.empty() || ins.typedOperands[0].kind != IRRef::Kind::CONST
                        || ins.typedOperands[0].value.type != IRType::STRING)
                    throw ACError::backend("ARM backend: malformed EVENT_TRIGGER");
                std::string key = std::get<std::string>(ins.typedOperands[0].value.data);
                if (key.size() >= 2 && key.front() == '$' && key.back() == '$') key = key.substr(1, key.size() - 2);
                ensureEventSlots();
                evTriggerUsed_ = true;
                loadStringLiteralPtr(R::X0, key);
                emitBranch(FixKind::BL, EVTRIG_LABEL);
                break;
            }
            case IROpcode::YIELD: {
                ensureGenSlot();
                if (!ins.typedOperands.empty()) loadOperand(ins.typedOperands[0], R::X10);
                else em.mov_imm64(R::X10, 0);
                em.ldr_imm(R::X17, GLOBALS_BASE, 8 * genCurSlot_);     // the running generator
                em.str_imm(R::X10, R::X17, GEN_VALUE);
                emitGenSwap(false);                                    // back to the caller
                break;
            }
            case IROpcode::GEN_CREATE: {
                // ops[0] = the generator body's name, ops[1..] = its arguments. The body is never
                // called: the first GEN_NEXT switches into it, with SP on the fresh fiber stack.
                ensureGenSlot();
                const std::string body = callableName(ins.typedOperands[0]);
                const size_t nArgs = ins.typedOperands.size() - 1;
                const int64_t blockBytes = ((GEN_HEADER + 8 * (int64_t)nArgs) + 15) & ~15LL;
                IRRef stateTmp = IRRef::temp(nextHiddenTemp_--);
                em.mov_imm64(R::X0, blockBytes);
                emitBranch(FixKind::BL, ALLOC_LABEL);
                storeResult(stateTmp, R::X0);
                em.mov_imm64(R::X0, GEN_STACK_BYTES);
                emitBranch(FixKind::BL, ALLOC_LABEL);                  // X0 = fiber stack base
                em.mov_imm64(R::X10, GEN_STACK_BYTES);
                em.add_reg(R::X0, R::X0, R::X10);                      // stacks grow down: top
                em.mov_imm64(R::X10, -16);
                em.and_reg(R::X0, R::X0, R::X10);                      // 16-byte aligned (AAPCS64)
                loadOperand(stateTmp, R::X17);
                em.str_imm(R::XZR, R::X17, GEN_DONE);
                em.str_imm(R::X0, R::X17, GEN_SIDE_G);                 // generator SP
                // X28 is the pinned globals base: the body needs it from its very first instruction,
                // and the first switch restores it from this slot, so it must be seeded here.
                em.str_imm(R::X28, R::X17, GEN_SIDE_G + 24 + 8 * 9);
                funcAdrFixups_.push_back({em.pos(), body, R::X10});
                em.adr_rel(R::X10);                                    // body entry point
                em.str_imm(R::X10, R::X17, GEN_SIDE_G + 104);          // generator resume PC
                for (size_t i = 0; i < nArgs; i++) {
                    loadOperand(ins.typedOperands[1 + i], R::X10);
                    em.str_imm(R::X10, R::X17, (uint32_t)(GEN_HEADER + 8 * i));
                }
                if (ins.result.isValid()) storeResult(ins.result, R::X17);
                break;
            }
            case IROpcode::GEN_NEXT: {
                ensureGenSlot();
                loadOperand(ins.typedOperands[0], R::X17);
                const int skipL = nextInternalLabel_--;
                em.ldr_imm(R::X9, R::X17, GEN_DONE);
                emitBranch(FixKind::CBNZ, skipL, Cond::EQ, R::X9);     // never resume a finished generator
                em.str_imm(R::X17, GLOBALS_BASE, 8 * genCurSlot_);     // it becomes the running one
                emitGenSwap(true);                                     // into the generator
                emitLabel(skipL);
                loadOperand(ins.typedOperands[0], R::X17);
                em.ldr_imm(R::X0, R::X17, GEN_VALUE);
                if (ins.result.isValid()) storeResult(ins.result, R::X0);
                break;
            }
            case IROpcode::GEN_DONE: {
                loadOperand(ins.typedOperands[0], R::X17);
                em.ldr_imm(R::X0, R::X17, GEN_DONE);
                if (ins.result.isValid()) storeResult(ins.result, R::X0);
                break;
            }
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
    static const int ALLOC_LABEL = -1004;
    static const int ALLOC_HAVE_LABEL = -1005;
    static const int APPEND_LABEL = -1006;
    static const int APPEND_FAST_LABEL = -1007;
    static const int APPEND_COPY_LABEL = -1008;
    static const int APPEND_COPYDONE_LABEL = -1009;
    static const int APPEND_DONE_LABEL = -1010;
    static const int IPOW_LOOP_LABEL = -1011;
    static const int IPOW_DONE_LABEL = -1012;
    static const int PRINT_RAW_LABEL = -1013;
    static const int PRINT_RAW_POS_LABEL = -1014;
    static const int PRINT_RAW_LOOP_LABEL = -1015;
    static const int PRINT_RAW_NOSIGN_LABEL = -1016;
    static const int PRINT_ARR_LABEL = -1017;
    static const int PRINT_ARR_LOOP_LABEL = -1018;
    static const int PRINT_ARR_ELEM_LABEL = -1019;
    static const int CSTRLIST_LABEL = -1100;
    static const int STRDUP_LABEL = -1101;
    static const int WILDCARD_LABEL = -1103;
    static const int DICT_HAS_LABEL = -1104;
    static const int EVTRIG_LABEL = -1102;
    static const int PRINT_ARR_DONE_LABEL = -1020;
    static const int STRLEN_LABEL = -1021;
    static const int STRLEN_LOOP_LABEL = -1022;
    static const int STRLEN_DONE_LABEL = -1023;
    static const int PRINT_CSTR_LABEL = -1024;
    static const int CONCAT_LABEL = -1025;
    static const int CONCAT_COPY_LEFT_LABEL = -1026;
    static const int CONCAT_LEFT_DONE_LABEL = -1027;
    static const int CONCAT_COPY_RIGHT_LABEL = -1028;
    static const int CONCAT_RIGHT_DONE_LABEL = -1029;
    static const int LENGTH_LABEL = -1030;
    static const int WRITE_CSTR_LABEL = -1031;
    static const int STREQ_LABEL = -1032;
    static const int STREQ_LOOP_LABEL = -1033;
    static const int STREQ_FALSE_LABEL = -1034;
    static const int STREQ_TRUE_LABEL = -1035;
    static const int STREQ_DONE_LABEL = -1036;
    static const int PRINT_FLOAT_LABEL = -1037;
    // (the old naive fixed-6-decimal print routine's sibling labels lived here — replaced by
    // emitPrintFloatRoutine's %.16g-equivalent port, which allocates its internal control-flow
    // labels dynamically via nextInternalLabel_ instead, same as every other multi-branch
    // synthetic routine added after this one.)
    static const int INT_TO_CSTR_LABEL = -1042;
    static const int INT_TO_CSTR_POS_LABEL = -1043;
    static const int INT_TO_CSTR_LOOP_LABEL = -1044;
    static const int INT_TO_CSTR_NOSIGN_LABEL = -1045;
    static const int CSTR_TO_INT_LABEL = -1046;
    static const int CSTR_TO_INT_SIGN_LABEL = -1047;
    static const int CSTR_TO_INT_LOOP_LABEL = -1048;
    static const int CSTR_TO_INT_DONE_LABEL = -1049;
    static const int DICT_GET_LABEL = -1050;
    static const int DICT_GET_LOOP_LABEL = -1051;
    static const int DICT_GET_NEXT_LABEL = -1052;
    static const int DICT_GET_MISS_LABEL = -1053;
    static const int DICT_GET_DONE_LABEL = -1059;
    static const int DICT_SET_LABEL = -1054;
    static const int DICT_SET_LOOP_LABEL = -1055;
    static const int DICT_SET_NEXT_LABEL = -1056;
    static const int DICT_SET_FULL_LABEL = -1057;
    static const int DICT_SET_DONE_LABEL = -1058;
    static const int DICT_SET_CAPERR_LABEL = -1060;
    static const int DIVZERO_LABEL = -1061;
    static const int PRINT_FLOAT_SMART_LABEL = -1062;
    static const int PRINT_FLOAT_BODY_LABEL = -1063;
    static const int FLOAT_TOSTR_LABEL = -1064;
    static const int FLOAT_TOSTR_SMART_LABEL = -1065;
    static const int FLOAT_TOSTR_OUT_LABEL = -1066;
    static const int FLOAT_TOSTR_COPY_LABEL = -1067;
    static const int FLOAT_TOSTR_DONE_LABEL = -1068;
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

    void emitPrintIntRawRoutine() {
        emitLabel(PRINT_RAW_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.mov_reg(R::X12, R::X0);
        em.mov_imm64(R::X13, 0);
        em.cmp_reg(R::X12, R::XZR);
        emitBranch(FixKind::BCOND, PRINT_RAW_POS_LABEL, Cond::GE);
        em.mov_imm64(R::X13, 1);
        em.neg_reg(R::X12, R::X12);
        emitLabel(PRINT_RAW_POS_LABEL);
        em.mov_from_sp(R::X14);
        em.add_imm(R::X14, R::X14, 30);
        em.mov_imm64(R::X15, 10);
        emitLabel(PRINT_RAW_LOOP_LABEL);
        em.sdiv(R::X0, R::X12, R::X15);
        em.msub(R::X1, R::X0, R::X15, R::X12);
        em.add_imm(R::X1, R::X1, (uint32_t)'0');
        em.strb0(R::X1, R::X14);
        em.sub_imm(R::X14, R::X14, 1);
        em.mov_reg(R::X12, R::X0);
        emitBranch(FixKind::CBNZ, PRINT_RAW_LOOP_LABEL, Cond::EQ, R::X12);
        em.cmp_reg(R::X13, R::XZR);
        emitBranch(FixKind::BCOND, PRINT_RAW_NOSIGN_LABEL, Cond::EQ);
        em.mov_imm64(R::X1, (int64_t)'-');
        em.strb0(R::X1, R::X14);
        em.sub_imm(R::X14, R::X14, 1);
        emitLabel(PRINT_RAW_NOSIGN_LABEL);
        em.add_imm(R::X14, R::X14, 1);
        em.mov_from_sp(R::X1);
        em.add_imm(R::X1, R::X1, 31);
        em.sub_reg(R::X2, R::X1, R::X14);
        em.mov_reg(R::X1, R::X14);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X8, 64);
        em.svc0();
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // Prints a double the same way every OTHER backend in this project does: up to 16
    // significant decimal digits, correctly rounded, trailing zeros stripped, whole-valued
    // floats get an explicit ".0" — glibc's `%.16g` convention, unified across all 12 backends
    // earlier this session. Direct port of BNY's `emitPrintDoubleLinux`/`ac_print_double`
    // (exp_bny.cpp) — same algorithm, re-expressed in AArch64 registers/instructions. Ported
    // because the previous version here (fixed 6 fractional digits, no trimming, no ".0") was a
    // real, confirmed bug: array_average.ac printed "5.000000"/"2.333333" instead of PY's
    // "5.0"/"2.333333333333333".
    //
    // AArch64 gives 9 clean caller-saved scratch registers (X9-X17, all "may be clobbered by a
    // call" under AAPCS64) — enough to hold every value BNY's x86-64 version needed push/pop
    // save/restore for (rbx/r12-r15 are callee-saved on x86-64 SysV, this routine's x86
    // counterpart has to preserve them), so this port needs no register spilling at all:
    //   X0/D0 = value in (mutates into the fractional part, matching BNY's xmm0 reuse)
    //   D1    = scratch double
    //   X9    = output buffer write cursor      (BNY: r12)
    //   X10   = integer part / |integer part| / final rounded integer value (BNY: r13 then rbx)
    //   X11   = sign flag, 0/1                  (BNY: a stack byte, kept in a register here
    //                                             since there's a free one)
    //   X12   = multi-phase scratch cursor      (BNY: r14)
    //   X13   = fracKeep (significant-digit budget, 0-16, kept in a register — BNY: [rbp-124])
    //   X14   = scaled fraction (frac * 2^52, exact integer)  (BNY: r15)
    //   X15,X16 = loop/arithmetic scratch
    //   X17   = fixed SP snapshot, read once, never mutated — buffer addresses are computed
    //           from this rather than from X9 (which moves as output chars are written)
    // Buffers, all byte-addressed (see strb0/ldrb0's own zero-offset-only convention, matching
    // this file's existing digit-buffer idiom in emitPrintIntRoutine): outBuf at X17+0 (32
    // bytes), fracBuf (raw 0-9 digit VALUES, MSB-first, 17 slots = 16 kept + 1 guard) at X17+32.
    // No integer-digit reverse buffer is needed the way BNY's is (x86 has no data-dependent
    // instruction count concern here) — the ARM version below reverses in place using the
    // output buffer itself as scratch, extracting into it end-first then rotating: WRONG, so
    // instead this port keeps a 20-byte reverse buffer too, exactly like BNY's, at X17+64, for
    // the same reason BNY has one (digit count isn't known ahead of the extraction loop).
    // Frame layout (all offsets from X17, the fixed SP-derived base):
    //   outBuf     : X17+0   .. X17+111  (112 bytes)
    //   fracBuf    : X17+112 .. X17+153  (42 bytes: 25 leading-zero slots + 16 significant + 1 guard)
    //   reverseBuf : X17+154 .. X17+177  (24 bytes, fills downward from the X17+178 boundary)
    static const int32_t OUTBUF_OFF = 0;
    static const int32_t FRACBUF_OFF = 112;
    static const int32_t FRACBUF_LEN = 42;
    static const int32_t REVBUF_END_OFF = 178;
    static const int32_t PRINT_FLOAT_FRAME = 192;

    void emitPrintFloatRoutine() {
        // Four entry points over one body. X2 is a mode flag the body only reads at the ".0" step
        // and at the final output step (X2 is otherwise first written by the final write-syscall
        // setup): bit 0 = smart-division display (a whole value shows as "4", not "4.0"); bit 1 =
        // to-string mode: no newline, no write — copy the text into a fresh heap string and return
        // its pointer in X0 (backs to_string(<float>)).
        emitLabel(PRINT_FLOAT_SMART_LABEL);
        em.mov_imm64(R::X2, 1);
        emitBranch(FixKind::B, PRINT_FLOAT_BODY_LABEL);
        emitLabel(FLOAT_TOSTR_LABEL);
        em.mov_imm64(R::X2, 2);
        emitBranch(FixKind::B, PRINT_FLOAT_BODY_LABEL);
        emitLabel(FLOAT_TOSTR_SMART_LABEL);
        em.mov_imm64(R::X2, 3);
        emitBranch(FixKind::B, PRINT_FLOAT_BODY_LABEL);
        emitLabel(PRINT_FLOAT_LABEL);
        em.mov_imm64(R::X2, 0);
        emitLabel(PRINT_FLOAT_BODY_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, PRINT_FLOAT_FRAME);
        em.mov_from_sp(R::X17);                // fixed base for all three buffers
        em.mov_reg(R::X9, R::X17);              // output buffer write cursor starts at outBuf[0]

        // ---- Integer part + sign (sign from the ORIGINAL value, not the truncated int, which
        // loses sign entirely for |value|<1 — e.g. -0.25 truncates to 0) ----
        em.fcvtzs_x_d(R::X10, R::X0);           // X10 = trunc(value)
        em.mov_imm64(R::X15, 0);
        em.fmov_d_from_x(R::X1, R::X15);        // D1 = 0.0
        em.fcmp_d(R::X0, R::X1);
        em.cset(R::X11, Cond::LT);              // X11 = 1 if value<0 else 0

        em.scvtf_d_x(R::X1, R::X10);            // D1 = (double)X10
        em.fsub_d(R::X0, R::X0, R::X1);         // D0 = fractional part (negative when value<0)
        {
            int posLabel = nextInternalLabel_--;
            em.cmp_reg(R::X11, R::XZR);
            emitBranch(FixKind::BCOND, posLabel, Cond::EQ);
            em.mov_imm64(R::X15, (int64_t)0xBFF0000000000000LL); // -1.0 bits
            em.fmov_d_from_x(R::X1, R::X15);
            em.fmul_d(R::X0, R::X0, R::X1);     // D0 = -fractional part (now positive)
            emitLabel(posLabel);
        }
        {
            int absDoneLabel = nextInternalLabel_--;
            em.cmp_reg(R::X11, R::XZR);
            emitBranch(FixKind::BCOND, absDoneLabel, Cond::EQ);
            em.neg_reg(R::X10, R::X10);         // X10 = |integer part|
            emitLabel(absDoneLabel);
        }

        // ---- significant-digit budget: count |intpart|'s own decimal digits (min 1, even for
        // 0), so the fraction only keeps 16-minus-that-many digits — matches %.16g's convention
        // of counting significant digits from the integer part's first digit, not the decimal
        // point (see BNY's own comment on this for the exact bug this avoids). ----
        {
            em.mov_reg(R::X15, R::X10);
            em.mov_imm64(R::X13, 0);
            int cntLoop = nextInternalLabel_--;
            emitLabel(cntLoop);
            em.mov_imm64(R::X16, 10);
            em.sdiv(R::X15, R::X15, R::X16);
            em.add_imm(R::X13, R::X13, 1);
            em.cmp_reg(R::X15, R::XZR);
            emitBranch(FixKind::BCOND, cntLoop, Cond::NE); // do-while: 0 itself counts as 1 digit
            em.mov_imm64(R::X15, 16);
            em.sub_reg(R::X13, R::X15, R::X13);     // X13 = 16 - digitcount
            int okLabel = nextInternalLabel_--;
            em.cmp_reg(R::X13, R::XZR);
            emitBranch(FixKind::BCOND, okLabel, Cond::GE);
            em.mov_imm64(R::X13, 0);
            emitLabel(okLabel);
        }

        // ---- Exact fixed-point scale, derived from frac's OWN IEEE-754 exponent/mantissa bits,
        // not a fixed frac*2^52 multiply. The fixed-2^52 approach silently loses precision for
        // ANY frac<0.5 (every binary exponent below -1 needs more than 52 bits of scale to be
        // exact) — verified real bug: 2/7's frac has exponent -2, so frac*2^52 lands on an exact
        // half-integer (1286742750677284.5), and fcvtzs's truncation of that half-unit compounded
        // through the digit-extraction loop into a wrong final rounding decision:
        // "0.2857142857142856" instead of the correct "...142857". Fix: pull frac's raw 53-bit
        // mantissa (with the implicit leading 1) straight out of its bit pattern as M, and its
        // true binary exponent E; frac == M * 2^(E-52) EXACTLY (no multiply, no rounding — just
        // reading bits IEEE-754 already stores). Scaling by SH = 52-E instead of a fixed 52 makes
        // M itself the exact "scaled fraction" for any frac down to about 2^-8 before SH would
        // make the loop's *10 step overflow 64 bits; smaller fracs than that (|value| under
        // ~0.0039) fall back to the old approximate path rather than reach for 128-bit arithmetic
        // this routine has nowhere to put.
        em.fmov_x_from_d(R::X5, R::X0);           // X5 = raw bits of frac
        em.mov_imm64(R::X16, 52);
        em.lsr_reg(R::X6, R::X5, R::X16);
        em.mov_imm64(R::X16, 0x7FF);
        em.and_reg(R::X6, R::X6, R::X16);         // X6 = biased exponent (0 for subnormal/zero)
        em.mov_imm64(R::X16, 0xFFFFFFFFFFFFFLL);  // (1<<52)-1
        em.and_reg(R::X7, R::X5, R::X16);         // X7 = raw mantissa bits
        em.mov_imm64(R::X16, 0x10000000000000LL); // 1<<52 (implicit leading bit)
        em.orr_reg(R::X7, R::X7, R::X16);         // X7 = M (53-bit exact mantissa)
        em.mov_imm64(R::X16, 1075);
        em.sub_reg(R::X8, R::X16, R::X6);         // X8 = SH = 1075 - biased_exponent = 52 - E
        em.mov_imm64(R::X16, 60);
        em.cmp_reg(R::X8, R::X16);
        {
            int exactPath = nextInternalLabel_--;
            int scaleDone = nextInternalLabel_--;
            emitBranch(FixKind::BCOND, exactPath, Cond::LE);
            // Fallback: frac too small for a single-register exact scale — old approximate path.
            em.mov_imm64(R::X15, (int64_t)0x4330000000000000LL); // 2^52 as a double
            em.fmov_d_from_x(R::X1, R::X15);
            em.fmul_d(R::X0, R::X0, R::X1);
            em.fcvtzs_x_d(R::X14, R::X0);
            em.mov_imm64(R::X8, 52);
            emitBranch(FixKind::B, scaleDone);
            emitLabel(exactPath);
            em.mov_reg(R::X14, R::X7);            // X14 = M (exact scaled fraction, no truncation)
            emitLabel(scaleDone);
        }
        em.mov_imm64(R::X6, 1);
        em.lsl_reg(R::X6, R::X6, R::X8);
        em.sub_imm(R::X6, R::X6, 1);              // X6 = mask = (1<<SH)-1

        // Extract FRACBUF_LEN raw digit values into fracBuf (X17+112), MSB-first: scaled*=10;
        // digit=scaled>>SH; scaled&=mask. Extracting far more than the 16-significant+1-guard
        // minimum lets the budget step below correctly skip past leading zeros in the fraction
        // (see that step's own comment for the bug this fixes) instead of hard-capping at 17.
        em.add_imm(R::X12, R::X17, FRACBUF_OFF); // X12 = fracBuf write cursor
        em.mov_imm64(R::X15, FRACBUF_LEN);      // loop counter
        {
            int fdigLoop = nextInternalLabel_--;
            emitLabel(fdigLoop);
            em.mov_imm64(R::X16, 10);
            em.mul_reg(R::X14, R::X14, R::X16);         // X14 *= 10 (safe: X14<2^SH<=2^60, *10<2^64)
            em.mov_reg(R::X0, R::X14);
            em.lsr_reg(R::X0, R::X0, R::X8);            // X0 = top digit (0-9), shift = SH
            em.strb0(R::X0, R::X12);
            em.add_imm(R::X12, R::X12, 1);
            em.and_reg(R::X14, R::X14, R::X6);          // X14 &= mask
            em.sub_imm(R::X15, R::X15, 1);
            em.cmp_reg(R::X15, R::XZR);
            emitBranch(FixKind::BCOND, fdigLoop, Cond::NE);
        }

        // ---- Leading-zero budget override (intpart==0 only). The digit-count step above always
        // reserves 1 significant-digit slot for the integer part, even when that part is "0" —
        // correct when intpart!=0, but %.16g does NOT count a leading "0." as a significant digit,
        // so for |value|<1 that reservation is one digit too many. Worse, every leading zero the
        // fraction itself has (0.0001... has 3) also needs to NOT count against the 16-digit
        // budget, or those digits get silently dropped. Verified real bugs this fixes: 2/7 printed
        // "0.285714285714286" (15 digits, correct is "...142857", 16) and 0.0001234567890123456
        // printed "0.000123456789012" (lost 4 trailing digits) — both because fracKeep was capped
        // at 15 regardless of how many of the extracted digits were non-significant leading zeros.
        // Fix: when intpart==0, scan the already-extracted fracBuf for the first nonzero digit at
        // index k, then fracKeep = k+16 (every leading zero is kept+printed, then 16 real
        // significant digits after it) instead of the flat 15. Clamped so fracKeep+1 (the guard
        // digit) stays inside the 42-byte buffer; an all-zero buffer means nothing survives at
        // this precision at all, so fracKeep=0 (prints ".0"), matching the existing strip-to-zero
        // path used for whole numbers.
        {
            int fkSkip = nextInternalLabel_--;
            em.cmp_reg(R::X10, R::XZR);
            emitBranch(FixKind::BCOND, fkSkip, Cond::NE);   // intpart != 0 -> keep tentative fracKeep

            em.add_imm(R::X3, R::X17, FRACBUF_OFF);         // X3 = scan cursor
            em.mov_imm64(R::X4, 0);                         // X4 = k
            int fkScan = nextInternalLabel_--;
            int fkFound = nextInternalLabel_--;
            int fkNone = nextInternalLabel_--;
            emitLabel(fkScan);
            em.ldrb0(R::X15, R::X3);
            em.cmp_reg(R::X15, R::XZR);
            emitBranch(FixKind::BCOND, fkFound, Cond::NE);
            em.add_imm(R::X3, R::X3, 1);
            em.add_imm(R::X4, R::X4, 1);
            em.mov_imm64(R::X16, FRACBUF_LEN);
            em.cmp_reg(R::X4, R::X16);
            emitBranch(FixKind::BCOND, fkScan, Cond::LT);
            emitBranch(FixKind::B, fkNone);                 // scanned the whole buffer, all zero
            emitLabel(fkFound);
            {
                int fkClampOk = nextInternalLabel_--;
                em.mov_imm64(R::X16, FRACBUF_LEN - 17);     // max k so k+16+1(guard) <= FRACBUF_LEN
                em.cmp_reg(R::X4, R::X16);
                emitBranch(FixKind::BCOND, fkClampOk, Cond::LE);
                em.mov_imm64(R::X4, FRACBUF_LEN - 17);
                emitLabel(fkClampOk);
            }
            em.add_imm(R::X13, R::X4, 16);                  // fracKeep = k + 16
            emitBranch(FixKind::B, fkSkip);
            emitLabel(fkNone);
            em.mov_imm64(R::X13, 0);
            emitLabel(fkSkip);
        }

        // ---- Round-half-up using the guard digit at fracBuf[fracKeep], carrying leftward
        // through the fracKeep kept digits; a carry that escapes past digit 0 (or fracKeep==0,
        // meaning there's no fractional digit to carry through at all) bumps X10 (the integer
        // part) by 1 before the integer-digit loop below runs. ----
        int noRound = nextInternalLabel_--;
        {
            em.add_imm(R::X12, R::X17, FRACBUF_OFF);
            em.add_reg(R::X12, R::X12, R::X13);     // &fracBuf[fracKeep] (the guard digit)
            em.ldrb0(R::X15, R::X12);
            em.mov_imm64(R::X16, 5);
            em.cmp_reg(R::X15, R::X16);
            emitBranch(FixKind::BCOND, noRound, Cond::LT);
            int haveFrac = nextInternalLabel_--;
            em.cmp_reg(R::X13, R::XZR);
            emitBranch(FixKind::BCOND, haveFrac, Cond::NE);
            em.add_imm(R::X10, R::X10, 1);          // fracKeep==0: bump integer part directly
            emitBranch(FixKind::B, noRound);
            emitLabel(haveFrac);
            em.add_imm(R::X12, R::X17, FRACBUF_OFF);
            em.add_reg(R::X12, R::X12, R::X13);
            em.sub_imm(R::X12, R::X12, 1);          // &fracBuf[fracKeep-1] (last kept digit)
            int carryLoop = nextInternalLabel_--;
            emitLabel(carryLoop);
            em.ldrb0(R::X15, R::X12);
            em.add_imm(R::X15, R::X15, 1);
            em.mov_imm64(R::X16, 10);
            em.cmp_reg(R::X15, R::X16);
            int noOverflow = nextInternalLabel_--;
            emitBranch(FixKind::BCOND, noOverflow, Cond::LT);
            em.mov_imm64(R::X15, 0);
            em.strb0(R::X15, R::X12);
            {
                int cont = nextInternalLabel_--;
                em.sub_imm(R::X12, R::X12, 1);
                em.add_imm(R::X16, R::X17, FRACBUF_OFF - 1);     // &fracBuf[-1] boundary check
                em.cmp_reg(R::X12, R::X16);
                emitBranch(FixKind::BCOND, cont, Cond::NE);
                em.add_imm(R::X10, R::X10, 1);       // carry escaped past digit 0
                emitBranch(FixKind::B, noRound);
                emitLabel(cont);
            }
            emitBranch(FixKind::B, carryLoop);
            emitLabel(noOverflow);
            em.strb0(R::X15, R::X12);
        }
        emitLabel(noRound);

        // ---- Sign character (X10 now holds the FINAL, possibly rounding-bumped integer value)
        // ----
        {
            int noSign = nextInternalLabel_--;
            em.cmp_reg(R::X11, R::XZR);
            emitBranch(FixKind::BCOND, noSign, Cond::EQ);
            em.mov_imm64(R::X15, (int64_t)'-');
            em.strb0(R::X15, R::X9);
            em.add_imm(R::X9, R::X9, 1);
            emitLabel(noSign);
        }

        // ---- Integer digits: reverse-extract X10 into the reverse buffer (X17+64, 20 bytes),
        // then copy forward into outBuf ----
        em.add_imm(R::X12, R::X17, REVBUF_END_OFF);         // one past the end of the reverse-digit area
        {
            int idigLoop = nextInternalLabel_--;
            emitLabel(idigLoop);
            em.mov_imm64(R::X16, 10);
            em.sdiv(R::X15, R::X10, R::X16);
            em.msub(R::X0, R::X15, R::X16, R::X10);  // X0 = X10 % 10
            em.add_imm(R::X0, R::X0, (uint32_t)'0');
            em.sub_imm(R::X12, R::X12, 1);
            em.strb0(R::X0, R::X12);
            em.mov_reg(R::X10, R::X15);
            em.cmp_reg(R::X10, R::XZR);
            emitBranch(FixKind::BCOND, idigLoop, Cond::NE);
        }
        {
            // Verified real bug: this was `X17+64` (the reverse buffer's START) instead of its
            // END (X17+84) — X12 already starts >= X17+64 after extraction (it only ever
            // decrements down to wherever the most-significant digit landed, never below the
            // buffer's start), so "loop while X12 < X17+64" was false on the very first check
            // and the do-while body ran exactly once: every multi-digit integer part printed as
            // just its single most-significant digit (12345.5 -> "1.5").
            em.add_imm(R::X16, R::X17, REVBUF_END_OFF);         // one past the reverse buffer's last digit

            int icpy = nextInternalLabel_--;
            emitLabel(icpy);
            em.ldrb0(R::X0, R::X12);
            em.strb0(R::X0, R::X9);
            em.add_imm(R::X9, R::X9, 1);
            em.add_imm(R::X12, R::X12, 1);
            em.cmp_reg(R::X12, R::X16);
            emitBranch(FixKind::BCOND, icpy, Cond::LT);
        }

        // ---- Fractional digits: strip trailing zeros from the (rounded, fracKeep-digit)
        // buffer. X13 = count of digits still kept, shrinks while the last is 0. ----
        {
            int stripLoop = nextInternalLabel_--;
            int stripDone = nextInternalLabel_--;
            emitLabel(stripLoop);
            em.cmp_reg(R::X13, R::XZR);
            emitBranch(FixKind::BCOND, stripDone, Cond::EQ);
            em.add_imm(R::X12, R::X17, FRACBUF_OFF);
            em.add_reg(R::X12, R::X12, R::X13);
            em.sub_imm(R::X12, R::X12, 1);          // &fracBuf[fracKeep-1]
            em.ldrb0(R::X15, R::X12);
            em.cmp_reg(R::X15, R::XZR);
            emitBranch(FixKind::BCOND, stripDone, Cond::NE);
            em.sub_imm(R::X13, R::X13, 1);
            emitBranch(FixKind::B, stripLoop);
            emitLabel(stripDone);
        }

        // ---- Print '.' + kept fractional digits, or ".0" if none survived stripping ----
        {
            int dotZero = nextInternalLabel_--;
            int afterFrac = nextInternalLabel_--;
            em.cmp_reg(R::X13, R::XZR);
            emitBranch(FixKind::BCOND, dotZero, Cond::EQ);
            em.mov_imm64(R::X15, (int64_t)'.');
            em.strb0(R::X15, R::X9);
            em.add_imm(R::X9, R::X9, 1);
            em.add_imm(R::X12, R::X17, FRACBUF_OFF);         // fracBuf cursor, index 0 first
            em.mov_reg(R::X16, R::X13);             // remaining count
            {
                int fcpy = nextInternalLabel_--;
                emitLabel(fcpy);
                em.ldrb0(R::X15, R::X12);
                em.add_imm(R::X15, R::X15, (uint32_t)'0');
                em.strb0(R::X15, R::X9);
                em.add_imm(R::X9, R::X9, 1);
                em.add_imm(R::X12, R::X12, 1);
                em.sub_imm(R::X16, R::X16, 1);
                em.cmp_reg(R::X16, R::XZR);
                emitBranch(FixKind::BCOND, fcpy, Cond::NE);
            }
            emitBranch(FixKind::B, afterFrac);
            emitLabel(dotZero);
            em.mov_imm64(R::X16, 1);
            em.and_reg(R::X15, R::X2, R::X16);
            em.cmp_reg(R::X15, R::XZR);
            emitBranch(FixKind::BCOND, afterFrac, Cond::NE);   // smart display: just the integer digits
            em.mov_imm64(R::X15, (int64_t)'.');
            em.strb0(R::X15, R::X9);
            em.add_imm(R::X9, R::X9, 1);
            em.mov_imm64(R::X15, (int64_t)'0');
            em.strb0(R::X15, R::X9);
            em.add_imm(R::X9, R::X9, 1);
            emitLabel(afterFrac);
        }

        // ---- to-string mode (X2 bit 1): hand back a heap copy of the text, write nothing ----
        em.mov_imm64(R::X16, 2);
        em.and_reg(R::X15, R::X2, R::X16);
        emitBranch(FixKind::CBNZ, FLOAT_TOSTR_OUT_LABEL, Cond::EQ, R::X15);

        // ---- Newline + write(1, outBuf, X9-outBuf) ----
        em.mov_imm64(R::X15, (int64_t)'\n');
        em.strb0(R::X15, R::X9);
        em.add_imm(R::X9, R::X9, 1);
        em.sub_reg(R::X2, R::X9, R::X17);
        em.mov_reg(R::X1, R::X17);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X8, 64);
        em.svc0();

        em.add_imm(R::SP, R::SP, PRINT_FLOAT_FRAME);
        em.ldp_x29_x30_postsp16();
        em.ret();

        // to-string exit: len = X9-X17; the alloc call clobbers X9-X17 (and X0-X5, X8), so park
        // len in the frame (X17+184, past the buffers) and rebuild the base from SP afterwards.
        emitLabel(FLOAT_TOSTR_OUT_LABEL);
        em.sub_reg(R::X12, R::X9, R::X17);
        em.str_imm(R::X12, R::X17, 184);
        em.add_imm(R::X0, R::X12, 1);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X13, R::X0);              // dest base (returned)
        em.mov_reg(R::X12, R::X0);              // dest cursor
        em.mov_from_sp(R::X17);
        em.ldr_imm(R::X15, R::X17, 184);        // len
        emitLabel(FLOAT_TOSTR_COPY_LABEL);
        emitBranch(FixKind::CBZ, FLOAT_TOSTR_DONE_LABEL, Cond::EQ, R::X15);
        em.ldrb0(R::X16, R::X17);
        em.strb0(R::X16, R::X12);
        em.add_imm(R::X17, R::X17, 1);
        em.add_imm(R::X12, R::X12, 1);
        em.sub_imm(R::X15, R::X15, 1);
        emitBranch(FixKind::B, FLOAT_TOSTR_COPY_LABEL);
        emitLabel(FLOAT_TOSTR_DONE_LABEL);
        em.mov_imm64(R::X15, 0);
        em.strb0(R::X15, R::X12);               // NUL
        em.mov_reg(R::X0, R::X13);
        em.add_imm(R::SP, R::SP, PRINT_FLOAT_FRAME);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // Integer-to-string cast used by the core coercion examples. The result points into a
    // fresh 32-byte bump-allocated block and is NUL-terminated.
    void emitIntToCstrRoutine() {
        emitLabel(INT_TO_CSTR_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 16);
        em.str_imm(R::X19, R::SP, 0);
        em.mov_reg(R::X19, R::X0);
        em.mov_imm64(R::X0, 32);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X9, R::X0);
        em.add_imm(R::X10, R::X0, 30);
        em.mov_reg(R::X12, R::X19);
        em.mov_imm64(R::X13, 0);
        em.cmp_reg(R::X12, R::XZR);
        emitBranch(FixKind::BCOND, INT_TO_CSTR_POS_LABEL, Cond::GE);
        em.mov_imm64(R::X13, 1);
        em.neg_reg(R::X12, R::X12);
        emitLabel(INT_TO_CSTR_POS_LABEL);
        em.mov_imm64(R::X14, 10);
        emitLabel(INT_TO_CSTR_LOOP_LABEL);
        em.sdiv(R::X0, R::X12, R::X14);
        em.msub(R::X1, R::X0, R::X14, R::X12);
        em.add_imm(R::X1, R::X1, (uint32_t)'0');
        em.strb0(R::X1, R::X10);
        em.sub_imm(R::X10, R::X10, 1);
        em.mov_reg(R::X12, R::X0);
        emitBranch(FixKind::CBNZ, INT_TO_CSTR_LOOP_LABEL, Cond::EQ, R::X12);
        em.cmp_reg(R::X13, R::XZR);
        emitBranch(FixKind::BCOND, INT_TO_CSTR_NOSIGN_LABEL, Cond::EQ);
        em.mov_imm64(R::X1, (int64_t)'-');
        em.strb0(R::X1, R::X10);
        em.sub_imm(R::X10, R::X10, 1);
        emitLabel(INT_TO_CSTR_NOSIGN_LABEL);
        em.add_imm(R::X10, R::X10, 1);
        em.mov_imm64(R::X1, 0);
        em.strb0(R::X1, R::X9);
        em.mov_reg(R::X0, R::X10);
        em.ldr_imm(R::X19, R::SP, 0);
        em.add_imm(R::SP, R::SP, 16);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitCstrToIntRoutine() {
        emitLabel(CSTR_TO_INT_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.mov_reg(R::X9, R::X0);
        em.mov_imm64(R::X10, 0);
        em.mov_imm64(R::X11, 1);
        em.ldrb0(R::X12, R::X9);
        em.mov_imm64(R::X13, (int64_t)'-');
        em.cmp_reg(R::X12, R::X13);
        emitBranch(FixKind::BCOND, CSTR_TO_INT_SIGN_LABEL, Cond::EQ);
        emitBranch(FixKind::B, CSTR_TO_INT_LOOP_LABEL);
        emitLabel(CSTR_TO_INT_SIGN_LABEL);
        em.mov_imm64(R::X11, -1);
        em.add_imm(R::X9, R::X9, 1);
        emitLabel(CSTR_TO_INT_LOOP_LABEL);
        em.ldrb0(R::X12, R::X9);
        em.cmp_reg(R::X12, R::XZR);
        emitBranch(FixKind::BCOND, CSTR_TO_INT_DONE_LABEL, Cond::EQ);
        em.sub_imm(R::X12, R::X12, (uint32_t)'0');
        em.mov_imm64(R::X13, 10);
        em.mul_reg(R::X10, R::X10, R::X13);
        em.add_reg(R::X10, R::X10, R::X12);
        em.add_imm(R::X9, R::X9, 1);
        emitBranch(FixKind::B, CSTR_TO_INT_LOOP_LABEL);
        emitLabel(CSTR_TO_INT_DONE_LABEL);
        em.cmp_reg(R::X11, R::XZR);
        int positive = nextInternalLabel_--;
        emitBranch(FixKind::BCOND, positive, Cond::GE);
        em.neg_reg(R::X10, R::X10);
        emitLabel(positive);
        em.mov_reg(R::X0, R::X10);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // __ac_dict_has__: X0 = dict pointer, X1 = key C-string -> X0 = 1 if the key is present, else 0.
    // The same pair scan as __ac_dict_get__ (STREQ_LABEL per key), but a miss is an answer, not a KeyError.
    void emitDictHasRoutine() {
        emitLabel(DICT_HAS_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.mov_reg(R::X19, R::X0); // dict pointer
        em.mov_reg(R::X20, R::X1); // key pointer
        em.mov_imm64(R::X21, 0);   // pair index
        const int loopL = nextInternalLabel_--;
        const int hitL = nextInternalLabel_--;
        const int nextL = nextInternalLabel_--;
        const int missL = nextInternalLabel_--;
        const int doneL = nextInternalLabel_--;
        emitLabel(loopL);
        em.ldr_imm(R::X0, R::X19, 0); // count
        em.cmp_reg(R::X21, R::X0);
        emitBranch(FixKind::BCOND, missL, Cond::GE);
        em.mov_reg(R::X12, R::X21);
        em.mov_imm64(R::X13, 16);
        em.mul_reg(R::X12, R::X12, R::X13);
        em.add_reg(R::X14, R::X19, R::X12);
        em.add_imm(R::X14, R::X14, 8);
        em.ldr_imm(R::X0, R::X14, 0);
        em.mov_reg(R::X1, R::X20);
        emitBranch(FixKind::BL, STREQ_LABEL);
        em.cmp_reg(R::X0, R::XZR);
        emitBranch(FixKind::BCOND, nextL, Cond::EQ);
        emitBranch(FixKind::B, hitL);
        emitLabel(nextL);
        em.add_imm(R::X21, R::X21, 1);
        emitBranch(FixKind::B, loopL);
        emitLabel(hitL);
        em.mov_imm64(R::X0, 1);
        emitBranch(FixKind::B, doneL);
        emitLabel(missL);
        em.mov_imm64(R::X0, 0);
        emitLabel(doneL);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitDictGetRoutine() {
        emitLabel(DICT_GET_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.mov_reg(R::X19, R::X0); // dict pointer
        em.mov_reg(R::X20, R::X1); // key pointer
        em.mov_imm64(R::X21, 0);   // pair index
        emitLabel(DICT_GET_LOOP_LABEL);
        em.ldr_imm(R::X0, R::X19, 0); // count
        em.cmp_reg(R::X21, R::X0);
        emitBranch(FixKind::BCOND, DICT_GET_MISS_LABEL, Cond::GE);
        em.mov_reg(R::X12, R::X21);
        em.mov_imm64(R::X13, 16);
        em.mul_reg(R::X12, R::X12, R::X13);
        em.add_reg(R::X14, R::X19, R::X12);
        em.add_imm(R::X14, R::X14, 8);
        em.ldr_imm(R::X0, R::X14, 0);
        em.mov_reg(R::X1, R::X20);
        emitBranch(FixKind::BL, STREQ_LABEL);
        em.cmp_reg(R::X0, R::XZR);
        emitBranch(FixKind::BCOND, DICT_GET_NEXT_LABEL, Cond::EQ);
        em.add_imm(R::X14, R::X14, 8);
        em.ldr_imm(R::X0, R::X14, 0);
        emitBranch(FixKind::B, DICT_GET_DONE_LABEL);
        emitLabel(DICT_GET_NEXT_LABEL);
        em.add_imm(R::X21, R::X21, 1);
        emitBranch(FixKind::B, DICT_GET_LOOP_LABEL);
        emitLabel(DICT_GET_MISS_LABEL);
        // A missing key is a hard error (PY raises KeyError): the message goes to stderr (fd 2) and
        // the program exits 1. Returning 0 here used to print a wrong value and keep running.
        static const char kMissMsg[] = "Preposterous: KeyError: key not found\n";
        em.mov_imm64(R::X0, 2);
        loadStringLiteralPtr(R::X1, "Preposterous: KeyError: key not found\n");
        em.mov_imm64(R::X2, sizeof(kMissMsg) - 1);
        em.mov_imm64(R::X8, 64);   // write
        em.svc0();
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X8, 94);   // exit_group
        em.svc0();
        emitLabel(DICT_GET_DONE_LABEL);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitDictSetRoutine() {
        emitLabel(DICT_SET_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.str_imm(R::X22, R::SP, 24);
        em.mov_reg(R::X19, R::X0); // dict pointer
        em.mov_reg(R::X20, R::X1); // key pointer
        em.mov_reg(R::X21, R::X2); // value
        em.mov_imm64(R::X22, 0);   // pair index
        emitLabel(DICT_SET_LOOP_LABEL);
        em.ldr_imm(R::X0, R::X19, 0);
        em.cmp_reg(R::X22, R::X0);
        emitBranch(FixKind::BCOND, DICT_SET_FULL_LABEL, Cond::GE);
        em.mov_reg(R::X12, R::X22);
        em.mov_imm64(R::X13, 16);
        em.mul_reg(R::X12, R::X12, R::X13);
        em.add_reg(R::X14, R::X19, R::X12);
        em.add_imm(R::X14, R::X14, 8);
        em.ldr_imm(R::X0, R::X14, 0);
        em.mov_reg(R::X1, R::X20);
        emitBranch(FixKind::BL, STREQ_LABEL);
        em.cmp_reg(R::X0, R::XZR);
        emitBranch(FixKind::BCOND, DICT_SET_NEXT_LABEL, Cond::EQ);
        em.add_imm(R::X14, R::X14, 8);
        em.str_imm(R::X21, R::X14, 0);
        emitBranch(FixKind::B, DICT_SET_DONE_LABEL);
        emitLabel(DICT_SET_NEXT_LABEL);
        em.add_imm(R::X22, R::X22, 1);
        emitBranch(FixKind::B, DICT_SET_LOOP_LABEL);
        emitLabel(DICT_SET_FULL_LABEL);
        em.sub_reg(R::X12, R::X19, R::XZR);
        em.sub_imm(R::X12, R::X12, 8);
        em.ldr_imm(R::X13, R::X12, 0);
        em.cmp_reg(R::X22, R::X13);
        emitBranch(FixKind::BCOND, DICT_SET_CAPERR_LABEL, Cond::GE);
        em.mov_reg(R::X12, R::X22);
        em.mov_imm64(R::X13, 16);
        em.mul_reg(R::X12, R::X12, R::X13);
        em.add_reg(R::X14, R::X19, R::X12);
        em.add_imm(R::X14, R::X14, 8);
        em.str_imm(R::X20, R::X14, 0);
        em.add_imm(R::X14, R::X14, 8);
        em.str_imm(R::X21, R::X14, 0);
        em.add_imm(R::X22, R::X22, 1);
        em.str_imm(R::X22, R::X19, 0);
        emitBranch(FixKind::B, DICT_SET_DONE_LABEL);
        emitLabel(DICT_SET_CAPERR_LABEL);
        loadStringLiteralPtr(R::X0, "Preposterous: DictError: dictionary capacity exceeded");
        emitBranch(FixKind::BL, PRINT_CSTR_LABEL);
        emitLabel(DICT_SET_DONE_LABEL);
        em.mov_reg(R::X0, R::X19);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.ldr_imm(R::X22, R::SP, 24);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitPrintArrayRoutine() {
        emitLabel(PRINT_ARR_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 48);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.mov_reg(R::X19, R::X0);             // ptr
        em.ldr_imm(R::X20, R::X19, 0);         // len
        em.mov_imm64(R::X21, 1);               // word index, 1..len
        em.mov_from_sp(R::X1);
        em.add_imm(R::X1, R::X1, 40);
        em.mov_imm64(R::X2, (int64_t)'[');
        em.strb0(R::X2, R::X1);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X2, 1);
        em.mov_imm64(R::X8, 64);
        em.svc0();

        emitLabel(PRINT_ARR_LOOP_LABEL);
        em.cmp_reg(R::X21, R::X20);
        emitBranch(FixKind::BCOND, PRINT_ARR_DONE_LABEL, Cond::GT);
        em.cmp_reg(R::X21, R::XZR);
        em.mov_imm64(R::X9, 1);
        em.cmp_reg(R::X21, R::X9);
        emitBranch(FixKind::BCOND, PRINT_ARR_ELEM_LABEL, Cond::EQ);
        em.mov_from_sp(R::X1);
        em.add_imm(R::X1, R::X1, 40);
        em.mov_imm64(R::X2, (int64_t)',');
        em.strb0(R::X2, R::X1);
        em.add_imm(R::X1, R::X1, 1);
        em.mov_imm64(R::X2, (int64_t)' ');
        em.strb0(R::X2, R::X1);
        em.sub_imm(R::X1, R::X1, 1);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X2, 2);
        em.mov_imm64(R::X8, 64);
        em.svc0();

        emitLabel(PRINT_ARR_ELEM_LABEL);
        em.mov_imm64(R::X9, 8);
        em.mul_reg(R::X10, R::X21, R::X9);
        em.add_reg(R::X10, R::X19, R::X10);
        em.ldr_imm(R::X0, R::X10, 0);
        emitBranch(FixKind::BL, PRINT_RAW_LABEL);
        em.add_imm(R::X21, R::X21, 1);
        emitBranch(FixKind::B, PRINT_ARR_LOOP_LABEL);

        emitLabel(PRINT_ARR_DONE_LABEL);
        em.mov_from_sp(R::X1);
        em.add_imm(R::X1, R::X1, 40);
        em.mov_imm64(R::X2, (int64_t)']');
        em.strb0(R::X2, R::X1);
        em.add_imm(R::X1, R::X1, 1);
        em.mov_imm64(R::X2, (int64_t)'\n');
        em.strb0(R::X2, R::X1);
        em.sub_imm(R::X1, R::X1, 1);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X2, 2);
        em.mov_imm64(R::X8, 64);
        em.svc0();
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.add_imm(R::SP, R::SP, 48);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitStrlenRoutine() {
        emitLabel(STRLEN_LABEL);
        em.mov_reg(R::X9, R::X0);              // cursor
        em.mov_imm64(R::X0, 0);                // length
        emitLabel(STRLEN_LOOP_LABEL);
        em.ldrb0(R::X10, R::X9);
        em.cmp_reg(R::X10, R::XZR);
        emitBranch(FixKind::BCOND, STRLEN_DONE_LABEL, Cond::EQ);
        em.add_imm(R::X0, R::X0, 1);
        em.add_imm(R::X9, R::X9, 1);
        emitBranch(FixKind::B, STRLEN_LOOP_LABEL);
        emitLabel(STRLEN_DONE_LABEL);
        em.ret();
    }

    void emitPrintCstrRoutine() {
        emitLabel(PRINT_CSTR_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 16);
        emitBranch(FixKind::BL, WRITE_CSTR_LABEL);
        em.mov_from_sp(R::X1);
        em.mov_imm64(R::X2, (int64_t)'\n');
        em.strb0(R::X2, R::X1);
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X2, 1);
        em.mov_imm64(R::X8, 64);
        em.svc0();
        em.add_imm(R::SP, R::SP, 16);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitWriteCstrRoutine() {
        emitLabel(WRITE_CSTR_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.mov_reg(R::X19, R::X0);
        emitBranch(FixKind::BL, STRLEN_LABEL);
        em.mov_reg(R::X2, R::X0);
        em.mov_imm64(R::X0, 1);
        em.mov_reg(R::X1, R::X19);
        em.mov_imm64(R::X8, 64);
        em.svc0();
        em.ldr_imm(R::X19, R::SP, 0);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    void emitStrEqRoutine() {
        emitLabel(STREQ_LABEL);
        emitLabel(STREQ_LOOP_LABEL);
        em.ldrb0(R::X9, R::X0);
        em.ldrb0(R::X10, R::X1);
        em.cmp_reg(R::X9, R::X10);
        emitBranch(FixKind::BCOND, STREQ_FALSE_LABEL, Cond::NE);
        em.cmp_reg(R::X9, R::XZR);
        emitBranch(FixKind::BCOND, STREQ_TRUE_LABEL, Cond::EQ);
        em.add_imm(R::X0, R::X0, 1);
        em.add_imm(R::X1, R::X1, 1);
        emitBranch(FixKind::B, STREQ_LOOP_LABEL);
        emitLabel(STREQ_TRUE_LABEL);
        em.mov_imm64(R::X0, 1);
        emitBranch(FixKind::B, STREQ_DONE_LABEL);
        emitLabel(STREQ_FALSE_LABEL);
        em.mov_imm64(R::X0, 0);
        emitLabel(STREQ_DONE_LABEL);
        em.ret();
    }

    void emitConcatRoutine() {
        emitLabel(CONCAT_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 64);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.str_imm(R::X22, R::SP, 24);
        em.str_imm(R::X23, R::SP, 32);
        em.str_imm(R::X24, R::SP, 40);
        em.mov_reg(R::X19, R::X0);             // left
        em.mov_reg(R::X20, R::X1);             // right
        emitBranch(FixKind::BL, STRLEN_LABEL);
        em.mov_reg(R::X21, R::X0);             // left length
        em.mov_reg(R::X0, R::X20);
        emitBranch(FixKind::BL, STRLEN_LABEL);
        em.mov_reg(R::X22, R::X0);             // right length
        em.add_reg(R::X0, R::X21, R::X22);
        em.add_imm(R::X0, R::X0, 1);           // NUL terminator
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X23, R::X0);             // dest base
        em.mov_reg(R::X24, R::X0);             // dest cursor

        em.mov_reg(R::X9, R::X19);             // src cursor
        em.mov_reg(R::X10, R::X21);            // bytes left
        emitLabel(CONCAT_COPY_LEFT_LABEL);
        em.cmp_reg(R::X10, R::XZR);
        emitBranch(FixKind::BCOND, CONCAT_LEFT_DONE_LABEL, Cond::EQ);
        em.ldrb0(R::X11, R::X9);
        em.strb0(R::X11, R::X24);
        em.add_imm(R::X9, R::X9, 1);
        em.add_imm(R::X24, R::X24, 1);
        em.sub_imm(R::X10, R::X10, 1);
        emitBranch(FixKind::B, CONCAT_COPY_LEFT_LABEL);
        emitLabel(CONCAT_LEFT_DONE_LABEL);

        em.mov_reg(R::X9, R::X20);
        em.mov_reg(R::X10, R::X22);
        emitLabel(CONCAT_COPY_RIGHT_LABEL);
        em.cmp_reg(R::X10, R::XZR);
        emitBranch(FixKind::BCOND, CONCAT_RIGHT_DONE_LABEL, Cond::EQ);
        em.ldrb0(R::X11, R::X9);
        em.strb0(R::X11, R::X24);
        em.add_imm(R::X9, R::X9, 1);
        em.add_imm(R::X24, R::X24, 1);
        em.sub_imm(R::X10, R::X10, 1);
        emitBranch(FixKind::B, CONCAT_COPY_RIGHT_LABEL);
        emitLabel(CONCAT_RIGHT_DONE_LABEL);
        em.mov_imm64(R::X11, 0);
        em.strb0(R::X11, R::X24);
        em.mov_reg(R::X0, R::X23);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.ldr_imm(R::X22, R::SP, 24);
        em.ldr_imm(R::X23, R::SP, 32);
        em.ldr_imm(R::X24, R::SP, 40);
        em.add_imm(R::SP, R::SP, 64);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // __ac_alloc__: X0 = bytes requested -> X0 = pointer. Bump allocator over a single
    // lazily-mmap'd 16 MB region, ported straight from BNY's own emitAllocLinux (exp_bny.cpp)
    // — same algorithm (mmap once, bump a cursor forever, never free), re-expressed in AArch64
    // registers. The cursor lives at globals-page offset 0 (slot 0, reserved — see nextSlot's
    // own comment) instead of BNY's own dedicated global slot, otherwise identical.
    //
    // X9/X10 hold "bytes requested" / "cursor value" across the mmap syscall rather than
    // spilling to the stack the way BNY's x86-64 version does (it pushes rdi/rcx specifically
    // because the x86-64 `syscall` instruction is DOCUMENTED to clobber rcx and r11) — AArch64's
    // `svc` has no such documented clobber beyond X0 (return value) and X8 (syscall number),
    // so any register outside the six argument registers (X0-X5) and X8 survives a syscall
    // untouched; verified this holds in practice via a real qemu-aarch64 run before trusting it.
    void emitAllocRoutine() {
        // Globals slot 0 = bump cursor, slot 1 = end of the current chunk. When a request does not fit
        // in the current chunk, another chunk is mapped (at least 16 MB, or the request rounded up to a
        // page) and allocation continues there; pointers into earlier chunks stay valid. Memory is never
        // freed, as before, so this only removes the hard crash at 16 MB of total allocation.
        // Only X0, X1, X9, X10, X11, X16, X17 are used: callers keep live values in X12-X15 across this call.
        emitLabel(ALLOC_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.mov_reg(R::X9, R::X0);              // X9 = bytes requested
        em.ldr_imm(R::X10, GLOBALS_BASE, 0);   // X10 = cursor (0 before the first chunk exists)
        const int newChunk = nextInternalLabel_--;
        const int haveRoom = nextInternalLabel_--;
        const int chunkOk = nextInternalLabel_--;
        emitBranch(FixKind::CBZ, newChunk, Cond::EQ, R::X10);
        em.add_reg(R::X11, R::X10, R::X9);     // X11 = cursor after this allocation
        em.ldr_imm(R::X16, GLOBALS_BASE, 8);   // X16 = end of the current chunk
        em.cmp_reg(R::X11, R::X16);
        emitBranch(FixKind::BCOND, haveRoom, Cond::LE);   // signed compare: user-space addresses are positive
        emitLabel(newChunk);
        // X1 = chunk size = max(16 MB, X9 rounded up to a 4 KB page)
        em.mov_imm64(R::X17, 0xFFF);
        em.add_reg(R::X1, R::X9, R::X17);
        em.mov_imm64(R::X17, ~(int64_t)0xFFF);
        em.and_reg(R::X1, R::X1, R::X17);
        em.mov_imm64(R::X17, 0x1000000);       // 16 MB
        em.cmp_reg(R::X1, R::X17);
        emitBranch(FixKind::BCOND, chunkOk, Cond::GE);
        em.mov_reg(R::X1, R::X17);
        emitLabel(chunkOk);
        // mmap(NULL, X1, PROT_READ|WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
        em.mov_imm64(R::X0, 0);
        em.mov_imm64(R::X2, 3);                // PROT_READ|PROT_WRITE
        em.mov_imm64(R::X3, 0x22);             // MAP_PRIVATE|MAP_ANONYMOUS
        em.mov_imm64(R::X4, -1);               // fd
        em.mov_imm64(R::X5, 0);                // offset
        em.mov_imm64(R::X8, 222);              // mmap (AArch64 Linux syscall table)
        em.svc0();
        em.mov_reg(R::X10, R::X0);             // X10 = base of the new chunk
        em.str_imm(R::X10, GLOBALS_BASE, 0);   // cursor = base
        em.add_reg(R::X16, R::X10, R::X1);
        em.str_imm(R::X16, GLOBALS_BASE, 8);   // limit = base + chunk size
        emitLabel(haveRoom);
        em.add_reg(R::X11, R::X10, R::X9);     // X11 = new cursor = old + bytes
        em.str_imm(R::X11, GLOBALS_BASE, 0);
        em.mov_reg(R::X0, R::X10);             // return the OLD cursor (start of this allocation)
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // Places an ilib call's arguments per AAPCS64 (see armExternalParamClasses). `args` are the call's
    // operands after the callee. With no known prototype, a float-typed value goes to a D register
    // and anything else to an X register, which matches every ilib function that takes only one kind.
    void loadExternalArgs(const std::vector<IRRef>& args, const std::string& exportName) {
        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
        const char* classes = armExternalParamClasses(exportName);
        int nextX = 0, nextD = 0;
        for (size_t i = 0; i < args.size(); i++) {
            const char cls = classes && i < std::strlen(classes) ? classes[i]
                           : (isFloatRef(args[i]) ? 'd' : 'x');
            if (cls == 'd') {
                if (nextD >= 8) throw ACError::backend("ARM backend: more than 8 floating-point arguments to '" + exportName + "'");
                loadFloatOperand(args[i], (R)((int)R::X0 + nextD));    // D<nextD> (same register number)
                nextD++;
            } else {
                if (nextX >= 8) throw ACError::backend("ARM backend: more than 8 integer arguments to '" + exportName + "'");
                if (isFloatValue(args[i])) {
                    // A float-typed value (a variable that also takes a `/` result) passed to an integer
                    // parameter is converted, not sent as raw bits. S2 is a scratch D-register, so no
                    // already-loaded double argument is overwritten.
                    loadFloatOperand(args[i], S2);
                    em.fcvtzs_x_d(argRegs[nextX], S2);
                } else {
                    loadOperand(args[i], argRegs[nextX]);
                }
                nextX++;
            }
        }
    }

    // Python-style negative index on an array: a negative index counts from the end (idx += len).
    // The IR index is the AC index minus one, so AC index 0 arrives here as -1 and reads the last
    // element, exactly as PY's a[i-1] does. `base` is the array pointer (length at base+0).
    void emitWrapNegativeIndex(R idx, R base) {
        const int nonneg = nextInternalLabel_--;
        em.cmp_reg(idx, R::XZR);
        emitBranch(FixKind::BCOND, nonneg, Cond::GE);
        em.ldr_imm(R::X11, base, 0);
        em.add_reg(idx, idx, R::X11);
        emitLabel(nonneg);
    }

    // Call an ilib function returning `char**` + `int* out_count` and produce an AC string list.
    // The count cell is a fresh heap word (zeroed first, so the C side's 32-bit int write
    // leaves a clean 64-bit value). The C array itself is not freed: the AC list copies the
    // pointers, and the strings stay valid for the program's lifetime, same as every other
    // bump-allocated value here.
    void emitCStrListCall(const IRInstruction& ins, const std::string& calleeName) {
        const size_t nArgs = ins.typedOperands.size() - 1;
        if (nArgs > 7)
            throw ACError::backend("ARM backend: list-returning ilib call '" + calleeName
                + "' takes more than 7 arguments");
        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
        em.mov_imm64(R::X0, 8);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.str_imm(R::XZR, R::X0, 0);
        IRRef countRef = IRRef::temp(nextHiddenTemp_--);
        storeResult(countRef, R::X0);
        for (size_t i = 0; i < nArgs; i++) loadOperand(ins.typedOperands[1+i], argRegs[i]);
        loadOperand(countRef, argRegs[nArgs]);
        callFixups.push_back({em.pos(), calleeName});
        em.bl_rel(0);                                  // X0 = char**
        loadOperand(countRef, R::X1);                  // X1 = count cell
        em.ldr_imm(R::X1, R::X1, 0);                   // X1 = count
        emitBranch(FixKind::BL, CSTRLIST_LABEL);       // X0 = AC list
        if (ins.result.isValid()) {
            storeResult(ins.result, R::X0);
            markArrayRef(ins.result);
            markStrListRef(ins.result);
        }
    }

    // __ac_trigger__: X0 = key string. Scans the binding table in bind order with string equality
    // and calls the first matching handler (no arguments), like BNY's __ac_trigger__. No match
    // is a silent no-op, same as PY. X19-X21 are callee-saved and spilled around the scan.
    void emitEventTriggerRoutine() {
        emitLabel(EVTRIG_LABEL);
        const int doneL = nextInternalLabel_--;
        const int loopL = nextInternalLabel_--;
        const int nextL = nextInternalLabel_--;
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.mov_reg(R::X21, R::X0);                             // X21 = wanted key
        em.ldr_imm(R::X19, GLOBALS_BASE, 8 * evSlot_);         // X19 = table cursor
        emitBranch(FixKind::CBZ, doneL, Cond::EQ, R::X19);     // nothing bound yet
        em.ldr_imm(R::X20, GLOBALS_BASE, 8 * evCountSlot_);    // X20 = entries left
        emitLabel(loopL);
        emitBranch(FixKind::CBZ, doneL, Cond::EQ, R::X20);
        em.ldr_imm(R::X0, R::X19, 0);                          // X0 = bound key
        em.mov_reg(R::X1, R::X21);
        emitBranch(FixKind::BL, STREQ_LABEL);
        emitBranch(FixKind::CBZ, nextL, Cond::EQ, R::X0);
        em.ldr_imm(R::X16, R::X19, 8);                         // handler
        em.blr(R::X16);
        emitBranch(FixKind::B, doneL);
        emitLabel(nextL);
        em.add_imm(R::X19, R::X19, 16);
        em.sub_imm(R::X20, R::X20, 1);
        emitBranch(FixKind::B, loopL);
        emitLabel(doneL);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // __ac_wildcard_match__: X0 = subject C-string, X1 = pattern C-string -> X0 = 1 if the whole
    // subject matches, else 0. `%` = any run of characters: classic linear glob that backtracks to
    // the most recent `%`. A leaf routine (no BL inside), so LR is never disturbed.
    //   X2 = pattern position of the last `%` (0 = none yet), X3 = subject position it started at.
    void emitWildcardRoutine() {
        emitLabel(WILDCARD_LABEL);
        const int loopL = nextInternalLabel_--;
        const int litL = nextInternalLabel_--;
        const int backL = nextInternalLabel_--;
        const int tailL = nextInternalLabel_--;
        const int tailEndL = nextInternalLabel_--;
        const int failL = nextInternalLabel_--;
        em.mov_imm64(R::X2, 0);
        emitLabel(loopL);
        em.ldrb0(R::X4, R::X0);                              // subject char
        emitBranch(FixKind::CBZ, tailL, Cond::EQ, R::X4);   // subject used up
        em.ldrb0(R::X5, R::X1);                              // pattern char
        em.mov_imm64(R::X6, '%');
        em.cmp_reg(R::X5, R::X6);
        emitBranch(FixKind::BCOND, litL, Cond::NE);
        em.mov_reg(R::X2, R::X1);                            // remember this %
        em.add_imm(R::X1, R::X1, 1);
        em.mov_reg(R::X3, R::X0);
        emitBranch(FixKind::B, loopL);
        emitLabel(litL);
        emitBranch(FixKind::CBZ, backL, Cond::EQ, R::X5);   // pattern ended, subject did not
        em.cmp_reg(R::X4, R::X5);
        emitBranch(FixKind::BCOND, backL, Cond::NE);
        em.add_imm(R::X0, R::X0, 1);
        em.add_imm(R::X1, R::X1, 1);
        emitBranch(FixKind::B, loopL);
        emitLabel(backL);
        emitBranch(FixKind::CBZ, failL, Cond::EQ, R::X2);   // no % to widen
        em.add_imm(R::X1, R::X2, 1);                         // pattern resumes after the %
        em.add_imm(R::X3, R::X3, 1);                         // the % absorbs one more char
        em.mov_reg(R::X0, R::X3);
        emitBranch(FixKind::B, loopL);
        emitLabel(tailL);
        em.ldrb0(R::X5, R::X1);
        em.mov_imm64(R::X6, '%');
        em.cmp_reg(R::X5, R::X6);
        emitBranch(FixKind::BCOND, tailEndL, Cond::NE);
        em.add_imm(R::X1, R::X1, 1);
        emitBranch(FixKind::B, tailL);
        emitLabel(tailEndL);
        emitBranch(FixKind::CBNZ, failL, Cond::NE, R::X5);  // a non-% pattern char is left
        em.mov_imm64(R::X0, 1);
        em.ret();
        emitLabel(failL);
        em.mov_imm64(R::X0, 0);
        em.ret();
    }

    // __ac_strdup__: X0 = C string (or NULL) -> X0 = fresh heap copy (or NULL).
    // ilib string results often point into a library-owned buffer that the next call overwrites
    // (os.join and friends use one shared thread-local buffer), so every string an ilib hands back
    // is copied into the program's own heap before it is stored, same as the C backend's ac_str_dup.
    void emitStrDupRoutine() {
        emitLabel(STRDUP_LABEL);
        const int nullLabel = nextInternalLabel_--;
        const int epilogueLabel = nextInternalLabel_--;
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.mov_reg(R::X19, R::X0);                     // X19 = source
        emitBranch(FixKind::CBZ, nullLabel, Cond::EQ, R::X19);
        em.mov_reg(R::X10, R::X19);                    // X10 = scan cursor
        em.mov_imm64(R::X11, 0);                       // X11 = length so far
        const int scanLoop = nextInternalLabel_--;
        const int scanDone = nextInternalLabel_--;
        emitLabel(scanLoop);
        em.ldrb0(R::X12, R::X10);
        emitBranch(FixKind::CBZ, scanDone, Cond::EQ, R::X12);
        em.add_imm(R::X10, R::X10, 1);
        em.add_imm(R::X11, R::X11, 1);
        emitBranch(FixKind::B, scanLoop);
        emitLabel(scanDone);
        em.add_imm(R::X0, R::X11, 1);                  // length + NUL
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X20, R::X0);                     // X20 = destination
        em.mov_reg(R::X9, R::X19);                     // X9 = source cursor
        em.mov_reg(R::X10, R::X20);                    // X10 = destination cursor
        const int copyLoop = nextInternalLabel_--;
        const int copyDone = nextInternalLabel_--;
        emitLabel(copyLoop);
        em.ldrb0(R::X12, R::X9);
        em.strb0(R::X12, R::X10);
        emitBranch(FixKind::CBZ, copyDone, Cond::EQ, R::X12);
        em.add_imm(R::X9, R::X9, 1);
        em.add_imm(R::X10, R::X10, 1);
        emitBranch(FixKind::B, copyLoop);
        emitLabel(copyDone);
        em.mov_reg(R::X0, R::X20);
        emitBranch(FixKind::B, epilogueLabel);
        emitLabel(nullLabel);
        em.mov_imm64(R::X0, 0);
        emitLabel(epilogueLabel);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // __ac_cstrlist__: X0 = char** (NUL-terminated C strings), X1 = count -> X0 = AC list pointer.
    // Layout matches the list literal path: raw[0] = capacity, ptr = raw+8 holds the length at
    // ptr+0, element k at ptr+8*(k+1). Capacity is count+4 so a later .append has room.
    void emitCStrListRoutine() {
        emitLabel(CSTRLIST_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 48);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.str_imm(R::X22, R::SP, 24);
        em.str_imm(R::X23, R::SP, 32);
        em.mov_reg(R::X19, R::X0);                     // X19 = source char** cursor
        em.mov_reg(R::X20, R::X1);                     // X20 = remaining count
        em.add_imm(R::X21, R::X20, 4);                 // X21 = capacity
        em.add_imm(R::X0, R::X21, 2);                  // (capacity + 2) words
        em.add_reg(R::X0, R::X0, R::X0);               // ×8 bytes, by three doublings
        em.add_reg(R::X0, R::X0, R::X0);
        em.add_reg(R::X0, R::X0, R::X0);
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.mov_reg(R::X22, R::X0);                     // X22 = raw block
        em.str_imm(R::X21, R::X22, 0);                 // raw[0] = capacity
        em.add_imm(R::X22, R::X22, 8);                 // X22 = list pointer (returned)
        em.str_imm(R::X20, R::X22, 0);                 // ptr[0] = length
        em.add_imm(R::X23, R::X22, 8);                 // X23 = first element slot
        const int loopLabel = nextInternalLabel_--;
        const int doneLabel = nextInternalLabel_--;
        emitLabel(loopLabel);
        emitBranch(FixKind::CBZ, doneLabel, Cond::EQ, R::X20);
        em.ldr_imm(R::X9, R::X19, 0);                  // X9 = next C string pointer
        em.str_imm(R::X9, R::X23, 0);
        em.add_imm(R::X19, R::X19, 8);
        em.add_imm(R::X23, R::X23, 8);
        em.sub_imm(R::X20, R::X20, 1);
        emitBranch(FixKind::B, loopLabel);
        emitLabel(doneLabel);
        em.mov_reg(R::X0, R::X22);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.ldr_imm(R::X22, R::SP, 24);
        em.ldr_imm(R::X23, R::SP, 32);
        em.add_imm(R::SP, R::SP, 48);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // __ac_append__: X0 = ptr, X1 = value -> X0 = new ptr (same pointer whenever capacity
    // already allows — O(1) amortized). Ported from BNY's emitAppendLinux (exp_bny.cpp),
    // same [cap@ptr-8][len@ptr+0][e0][e1]... layout, same capacity-doubling algorithm.
    // X19-X22 are AAPCS64 callee-saved, so a real caller expects them preserved across this
    // call — spilled to the stack around the body exactly like BNY spills rbx/r12/r13/r14.
    void emitAppendRoutine() {
        emitLabel(APPEND_LABEL);
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        em.sub_imm(R::SP, R::SP, 32);
        em.str_imm(R::X19, R::SP, 0);
        em.str_imm(R::X20, R::SP, 8);
        em.str_imm(R::X21, R::SP, 16);
        em.str_imm(R::X22, R::SP, 24);
        em.mov_reg(R::X19, R::X0);             // X19 = ptr
        em.mov_reg(R::X20, R::X1);             // X20 = value
        em.ldr_imm(R::X21, R::X19, 0);         // X21 = len = ptr[0]
        em.sub_imm(R::X9, R::X19, 8);
        em.ldr_imm(R::X22, R::X9, 0);          // X22 = cap = ptr[-8]
        em.cmp_reg(R::X21, R::X22);
        emitBranch(FixKind::BCOND, APPEND_FAST_LABEL, Cond::LT);

        // ---- slow path: capacity exhausted — double and copy, O(len), amortized O(1) ----
        em.add_reg(R::X22, R::X22, R::X22);    // X22 = newcap = cap*2
        em.add_imm(R::X0, R::X22, 2);
        em.mov_imm64(R::X9, 8);
        em.mul_reg(R::X0, R::X0, R::X9);       // X0 = (newcap+2)*8 bytes
        emitBranch(FixKind::BL, ALLOC_LABEL);
        em.str_imm(R::X22, R::X0, 0);          // new_raw[0] = newcap
        em.add_imm(R::X0, R::X0, 8);           // X0 = new ptr (skip cap word)
        em.str_imm(R::X21, R::X0, 0);          // new_ptr[0] = len (temporary)
        em.mov_imm64(R::X9, 1);                // X9 = copy index i
        emitLabel(APPEND_COPY_LABEL);
        em.cmp_reg(R::X9, R::X21);
        emitBranch(FixKind::BCOND, APPEND_COPYDONE_LABEL, Cond::GT);
        em.mov_imm64(R::X10, 8);
        em.mul_reg(R::X10, R::X9, R::X10);     // X10 = i*8
        em.add_reg(R::X11, R::X19, R::X10);
        em.ldr_imm(R::X12, R::X11, 0);         // X12 = old[i]
        em.add_reg(R::X11, R::X0, R::X10);
        em.str_imm(R::X12, R::X11, 0);         // new[i] = old[i]
        em.add_imm(R::X9, R::X9, 1);
        emitBranch(FixKind::B, APPEND_COPY_LABEL);
        emitLabel(APPEND_COPYDONE_LABEL);
        em.add_imm(R::X9, R::X21, 1);          // len+1
        em.mov_imm64(R::X10, 8);
        em.mul_reg(R::X10, R::X9, R::X10);
        em.add_reg(R::X11, R::X0, R::X10);
        em.str_imm(R::X20, R::X11, 0);         // new[len+1] = value
        em.str_imm(R::X9, R::X0, 0);           // new[0] = len+1
        emitBranch(FixKind::B, APPEND_DONE_LABEL);

        // ---- fast path: len < cap — write in place, O(1), zero allocation/copy ----
        emitLabel(APPEND_FAST_LABEL);
        em.add_imm(R::X9, R::X21, 1);          // len+1
        em.mov_imm64(R::X10, 8);
        em.mul_reg(R::X10, R::X9, R::X10);
        em.add_reg(R::X11, R::X19, R::X10);
        em.str_imm(R::X20, R::X11, 0);         // ptr[len+1] = value
        em.str_imm(R::X9, R::X19, 0);          // ptr[0] = len+1
        em.mov_reg(R::X0, R::X19);             // return same ptr — nothing moved

        emitLabel(APPEND_DONE_LABEL);
        em.ldr_imm(R::X19, R::SP, 0);
        em.ldr_imm(R::X20, R::SP, 8);
        em.ldr_imm(R::X21, R::SP, 16);
        em.ldr_imm(R::X22, R::SP, 24);
        em.add_imm(R::SP, R::SP, 32);
        em.ldp_x29_x30_postsp16();
        em.ret();
    }

    // `length arr` doesn't lower to LOAD_INDEX+"__len__" the way this file's own LOAD_INDEX
    // case might suggest — ir.cpp instead emits a plain CALL to a function literally named
    // "ac_length" (comment there: "each backend has ac_length"), so every backend must supply
    // its own. Registered directly in funcOffsets (not via the emitBranch/label-fixup path
    // emitAllocRoutine/emitAppendRoutine use) since CALL's own resolution looks callees up
    // there specifically. BNY's version (exp_bny.cpp) also handles a STRING receiver via
    // __ac_strlen__ — not reachable here yet since ARM has no string support at all.
    void emitLengthRoutine() {
        emitLabel(LENGTH_LABEL);
        funcOffsets["ac_length"] = em.pos();
        em.ldr_imm(R::X0, R::X0, 0);  // length = ptr[0]
        em.ret();
    }

    // Reached only when a divisor was 0 (see emitDivZeroGuard). With an active try (depth > 0)
    // it unwinds to the innermost catch: depth--, restore SP/X29/X30/X19-X28 from that slot, and
    // branch to the saved catch address. With no active try it falls through to BNY's exact
    // fatal path (same message, same stderr+exit(1) shape).
    void emitDivZeroTrapRoutine() {
        emitLabel(DIVZERO_LABEL);
        const int fatalL = nextInternalLabel_--;
        if (tryDepthSlot_ >= 0) {
            em.ldr_imm(R::X9, GLOBALS_BASE, 8 * tryDepthSlot_);
            emitBranch(FixKind::CBZ, fatalL, Cond::EQ, R::X9);     // depth 0: no active try
            em.sub_imm(R::X9, R::X9, 1);
            em.str_imm(R::X9, GLOBALS_BASE, 8 * tryDepthSlot_);     // depth--
            em.ldr_imm(R::X10, GLOBALS_BASE, 8 * tryStackSlot_);
            em.mov_imm64(R::X11, TRY_SLOT_BYTES);
            em.mul_reg(R::X11, R::X9, R::X11);
            em.add_reg(R::X12, R::X10, R::X11);                    // X12 = slot of the innermost try
            em.ldr_imm(R::X13, R::X12, 0);
            em.mov_sp_from(R::X13);                                // SP
            em.ldr_imm(R::X29, R::X12, 8);
            em.ldr_imm(R::X30, R::X12, 16);
            for (int i = 0; i < 10; i++)                           // X19..X28
                em.ldr_imm((R)((int)R::X19 + i), R::X12, 24 + 8 * i);
            em.ldr_imm(R::X16, R::X12, 104);
            em.br(R::X16);                                         // into the catch body
        }
        emitLabel(fatalL);
        static const char msg[] = "Preposterous: 3rd grade mathematics violated (ZeroDivisionError)\n";
        std::vector<uint8_t> data(msg, msg + sizeof(msg) - 1);
        int strLabel = nextInternalLabel_--;
        int afterLabel = nextInternalLabel_--;
        emitBranch(FixKind::B, afterLabel);
        emitLabel(strLabel);
        em.bytesRaw(data);
        while (em.pos() % 4 != 0) em.byteRaw(0);
        emitLabel(afterLabel);
        auto it = labelOffsets.find(strLabel);
        if (it == labelOffsets.end())
            throw ACError::backend("ARM backend: internal string-label emission failed");
        em.mov_imm64(R::X0, 2);                              // stderr
        em.mov_imm64(R::X1, (int64_t)(codeVA_ + it->second));
        em.mov_imm64(R::X2, (int64_t)data.size());
        em.mov_imm64(R::X8, 64);                             // write
        em.svc0();
        em.mov_imm64(R::X0, 1);
        em.mov_imm64(R::X8, 94);                             // exit_group
        em.svc0();
    }

    // ac_ipow(base, exp): integer exponentiation helper emitted by IR for `^` whenever
    // constexpr folding cannot reduce it away. Negative exponents intentionally collapse to
    // 1 here, matching the simple integer helper used by the typed backends.
    void emitIpowRoutine() {
        funcOffsets["ac_ipow"] = em.pos();
        em.mov_reg(R::X10, R::X0);             // base
        em.mov_reg(R::X11, R::X1);             // exponent
        em.mov_imm64(R::X9, 1);                // result
        emitLabel(IPOW_LOOP_LABEL);
        em.cmp_reg(R::X11, R::XZR);
        emitBranch(FixKind::BCOND, IPOW_DONE_LABEL, Cond::LE);
        em.mul_reg(R::X9, R::X9, R::X10);
        em.sub_imm(R::X11, R::X11, 1);
        emitBranch(FixKind::B, IPOW_LOOP_LABEL);
        emitLabel(IPOW_DONE_LABEL);
        em.mov_reg(R::X0, R::X9);
        em.ret();
    }

public:
    ArmCompiler(const IRProgram& p, uint64_t codeVA, bool dynamicLink, bool staticLink)
        : prog(p), codeVA_(codeVA), dynamicLink_(dynamicLink), staticLink_(staticLink) {}

    void emitExternalStub(const std::string& name, const std::string& exportName,
                          const std::string& library) {
        ArmExternal ext{name, exportName, library, em.pos()};
        externalStubOffsets_[name] = em.pos();
        em.mov_imm64(R::X16, 0); // patched to this symbol's GOT slot by the ELF writer
        em.ldr_imm(R::X17, R::X16, 0);
        em.br(R::X17);
        externals_.push_back(std::move(ext));
    }

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
        // Bundle methods need a class-qualified label ("Critter_greet") — no function is ever
        // literally named just "greet" from a call site's point of view (construction/method
        // dispatch, see the CALL case, always resolve to this qualified form).
        std::string label = fn.classOwner.empty() ? fn.name : fn.classOwner + "_" + fn.name;
        currentClass_ = fn.classOwner;
        compilingMethod_ = !fn.classOwner.empty();
        funcOffsets[label] = em.pos();
        curFnIsGenerator_ = fn.isGenerator;
        currentFuncName_ = label;
        currentFuncParamNames_ = std::set<std::string>(fn.parameters.begin(), fn.parameters.end());
        // Slot 0 is reserved for `self` in a method's frame (see SELF_SLOT) — never handed out
        // by the ordinary per-symbol slot allocator, so ordinary locals start at 1 instead of 0.
        // Verified real bug: SELF_SLOT is written directly (bypassing slotFor, which is the only
        // thing that ever bumps funcMaxSlot_), so a method whose body never happens to reference
        // any OTHER var/temp (e.g. a zero-arg constructor doing only `self.f = 0`) left
        // funcMaxSlot_ at -1 — a 0-byte frame — while still writing self's incoming pointer to
        // slot 0. That write landed squarely on the just-pushed saved x29/x30 (the frame the
        // `stp x29,x30,[sp,#-16]!` prologue had JUST written there), corrupting the caller's
        // frame pointer the moment that method was called from inside another function (harmless
        // garbage at the top-level mainloop, where nothing downstream reads the stale x29, but a
        // real segfault one call deeper — verified via a qemu instruction trace).
        funcMaxSlot_ = compilingMethod_ ? SELF_SLOT : -1;
        nextFuncSlot_ = compilingMethod_ ? 1 : 0;
        preScanFloatVars(fn.instructions);
        for (const std::string& param : stringParamHints_[fn.name]) {
            for (const auto& ins : fn.instructions) {
                auto markIfParam = [&](const IRRef& r) {
                    if (r.kind == IRRef::Kind::VAR && r.id >= 0
                            && prog.symbols.getName(r.id) == param)
                        markStringRef(r);
                };
                markIfParam(ins.result);
                for (const auto& op : ins.typedOperands) markIfParam(op);
            }
        }
        // Recomputed per function, not once for the whole program: temp ids restart at 0 in
        // each IRFunction (same reason keyFor is function-qualified — see its own comment), so
        // reusing one elidableTemps_ set across functions would let function B's temp 0 wrongly
        // inherit function A's temp 0's elision eligibility.
        elidableTemps_ = computeElidableTemps(fn.instructions);
        preseedValueKinds(fn.instructions);
        pendingTempId_ = -1;
        em.stp_x29_x30_presp16();
        em.mov_x29_sp();
        size_t subSpOff = em.pos();
        em.sub_imm(R::SP, R::SP, 0); // placeholder — patched below once the real frame size is known
        static const R argRegs[8] = {R::X0,R::X1,R::X2,R::X3,R::X4,R::X5,R::X6,R::X7};
        if (fn.parameters.size() > 8)
            throw ACError::backend("ARM backend: functions with more than 8 parameters are not yet implemented");
        for (size_t i = 0; i < fn.parameters.size(); i++) {
            if (fn.isGenerator) {
                // A generator body's arguments were laid out in its state block by GEN_CREATE.
                ensureGenSlot();
                em.ldr_imm(R::X16, GLOBALS_BASE, 8 * genCurSlot_);
                em.ldr_imm(argRegs[i], R::X16, (uint32_t)(GEN_HEADER + 8 * i));
            }
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
            // A bundle/tuple-instance parameter referenced ONLY via dotted field access
            // ("p.x"/"p.y", each its own separate symbol) never appears as a bare "p" VAR
            // anywhere in fn.instructions — the scan above can't find it — even though
            // ir.cpp already interned the real symbol id for the plain parameter name. Ported
            // from BNY's identical fix: look it up directly instead of leaving the incoming
            // pointer with nowhere to land (verified real segfault without this: resolveFieldAccess
            // correctly knew `p` was a Point via computeClassParamTypes, but `loadNamedVar("p",...)`
            // read an unwritten/garbage slot instead of the real incoming pointer).
            if (symId < 0 && instanceClass_.count(fn.parameters[i]))
                symId = prog.symbols.lookupAnyScope(fn.parameters[i]);
            if (symId >= 0) {
                IRRef pref = IRRef::var(symId);
                if (stringParamHints_[fn.name].count(fn.parameters[i])) markStringRef(pref);
                if (floatParamHints_[fn.name].count(fn.parameters[i])) markFloatRef(pref);
                if (floatParamListHints_.count(fn.name) && floatParamListHints_.at(fn.name).count((int)i))
                    markFloatListRef(pref);
                storeResult(pref, argRegs[i]);
            } else if (i == 0 && compilingMethod_ && fn.parameters[i] == "self") {
                // `self` is never a bare VAR anywhere in the IR — only ever fused into compound
                // names like "self.hp" (a completely separate symbol) — so the scan above can
                // never find it. Store its incoming pointer into the dedicated self slot instead.
                em.str_imm(argRegs[i], R::SP, (uint32_t)(SELF_SLOT * 8));
            }
            // A parameter never referenced in the body at all needs no slot — nothing to do.
        }
        for (auto& ins : fn.instructions) {
            if (ins.opcode == IROpcode::FUNC_BEGIN || ins.opcode == IROpcode::FUNC_END) continue;
            compileInstr(ins);
        }
        ensureSpilled(); // in case the body's last instruction left something uncommitted
        // Implicit void return, if the body fell off the end without an explicit `return`.
        if (fn.isGenerator) {
            emitGenReturnSwap();                 // a generator finishes by switching back, never by ret
        } else {
            em.mov_sp_from(R::X29);
            em.ldp_x29_x30_postsp16();
            em.ret();
        }
        curFnIsGenerator_ = false;
        // Now that the whole body (and its params) have assigned every slot they'll ever use,
        // patch the placeholder `sub sp,sp,#0` from the prologue with this function's real
        // frame size — 16-byte-aligned per AAPCS64 (SP must stay 16-aligned at every call).
        uint32_t frameBytes = (uint32_t)(((funcMaxSlot_+1)*8 + 15) & ~15);
        if (frameBytes > 4095)
            throw ACError::backend("ARM backend: function '" + fn.name
                + "' needs more local-frame space than this v1.1 slice's 12-bit immediate encoding supports");
        em.patch32(subSpOff, 0xD1000000u | (frameBytes<<10) | (rn(R::SP)<<5) | rn(R::SP));
        // Which parameters this body treats as floats (its callers must pass doubles there).
        {
            std::vector<bool> flags;
            for (const auto& pname : fn.parameters) {
                int sid = prog.symbols.lookupAnyScope(pname);
                flags.push_back(sid >= 0 && isFloatRef(IRRef::var(sid)));
            }
            armParamFloat_[fn.name] = flags;
        }
        currentFuncName_.clear();
        currentFuncParamNames_.clear();
        currentClass_.clear();
        compilingMethod_ = false;
    }

    // Returns {machine code bytes, data-slot count needed}.
    // Whole-program field-order pre-scan (ported from BNY's identical design): every
    // `self.field` STORE_VAR/TYPE_CAST across a class's methods (in first-seen order) becomes
    // that field's slot, offset = 8*index. A class with zero self.field writes still gets an
    // entry (matches BNY's own "always present, possibly empty" convention).
    void computeClassFields() {
        classFields_.clear();
        for (auto& fn : prog.functions) {
            if (fn.classOwner.empty()) continue;
            auto& fields = classFields_[fn.classOwner];
            // Temps loaded from a string constant (a field default is usually stored through one).
            std::set<int> stringTemps;
            for (auto& ins : fn.instructions)
                if (ins.opcode == IROpcode::LOAD_CONST && ins.result.kind == IRRef::Kind::TEMP
                        && !ins.typedOperands.empty() && ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING)
                    stringTemps.insert(ins.result.id);
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
                if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()) {
                    // Two shapes: [target, value] or, when the target is the result, [value] alone.
                    const IRRef& val = ins.typedOperands.size() >= 2 ? ins.typedOperands[1] : ins.typedOperands[0];
                    bool strValue = (val.kind == IRRef::Kind::CONST && val.value.type == IRType::STRING)
                        || (val.kind == IRRef::Kind::TEMP && stringTemps.count(val.id));
                    if (strValue) stringFields_[fn.classOwner].insert(field);
                }
            }
        }
        for (auto& fn : prog.functions)
            if (!fn.classOwner.empty()) classFields_[fn.classOwner];
    }

    // Ported from BNY's identical two-part discovery (see its own comment for the full
    // rationale): (1) classReturnFuncs_ — a free function that always returns a var directly
    // constructed via `SomeClass()`, so `q = f()` gets the same instanceClass_ treatment as
    // `q = SomeClass()`. (2) classParamTypes — for every CALL/LIB_CALL site passing a
    // known-instance variable as an argument, record which parameter position of the callee
    // receives it, then seed instanceClass_ with that callee's REAL parameter name. Without
    // this, a free function's bundle-typed parameter (`Make show func(p): p.x`) has no way to
    // know `p` is an instance at all — `p.x` would resolve as an ordinary (wrong, uninitialized)
    // local instead of a real field access.
    void computeClassParamTypes() {
        classReturnFuncs_.clear();
        for (auto& fn : prog.functions) {
            if (!fn.classOwner.empty()) continue;
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

        // Free functions only — a method's fn.parameters has an extra leading "self" the caller
        // never writes, which would shift every index by one; out of scope for this fix, same as
        // BNY's own.
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

    ArmCompiledImage compile(uint64_t globalsVA) {
        computeArrayReturningFuncs();
        computeClassFields();
        computeClassParamTypes();
        // Pin X28 = globals base address once, at program start.
        em.mov_imm64(GLOBALS_BASE, (int64_t)globalsVA);
        // globalInit already IS data+main combined (see IRProgram's own comment on the field) —
        // looping mainSection too would compile the mainloop body twice (verified real bug:
        // caught it via a byte-for-byte objdump read of the very first test binary, which
        // showed the store/print/halt sequence duplicated even though --stop-after-ir's LIR
        // dump only shows it once, since the dump reads a different view).
        // Parameter float-ness flows both ways: a caller that passes a float makes the callee's parameter
        // a float (the callee body needs that to compute with it), and a float parameter makes the caller
        // send a double. Iterate the scopes until no new hint appears; the mainloop is one more scope.
        for (int pass = 0; pass < 6; ++pass) {
            if (pass > 0 && pass % 2 == 0) computeArrayReturningFuncsPass();   // see the float-return seeding
            size_t before = armParamFloatHints_.size();
            for (const auto& [_, set] : armParamFloatHints_) before += set.size();
            for (const auto& fn : prog.functions) {
                const std::string label = fn.classOwner.empty() ? fn.name : fn.classOwner + "_" + fn.name;
                currentFuncName_ = label;
                seedParamFloats(label, fn);
                preScanFloatVars(fn.instructions);
                recordCallFloatArgs(fn.instructions);
                std::vector<bool> flags;
                for (const auto& pname : fn.parameters) {
                    int sid = paramSymbolId(fn, pname);
                    flags.push_back(sid >= 0 && isFloatRef(IRRef::var(sid)));
                }
                armParamFloat_[label] = flags;
                currentFuncName_.clear();
            }
            currentFuncName_.clear();
            preScanFloatVars(prog.globalInit);
            recordCallFloatArgs(prog.globalInit);
            size_t after = armParamFloatHints_.size();
            for (const auto& [_, set] : armParamFloatHints_) after += set.size();
            if (after == before && pass > 1) break;
        }
        preScanFloatVars(prog.globalInit);
        elidableTemps_ = computeElidableTemps(prog.globalInit);
        preseedValueKinds(prog.globalInit);
        pendingTempId_ = -1;
        for (auto& ins : prog.globalInit) compileInstr(ins);
        ensureSpilled(); // in case the mainloop's last instruction left something uncommitted
        // Fallthrough safety net: ensure a clean exit even if no explicit /kill was compiled.
        em.mov_imm64(R::X0, 0);
        em.mov_imm64(R::X8, 94);
        em.svc0();
        for (auto& fn : prog.functions) compileFunction(fn);
        emitPrintIntRoutine();
        emitPrintIntRawRoutine();
        emitPrintFloatRoutine();
        emitIntToCstrRoutine();
        emitCstrToIntRoutine();
        emitDictGetRoutine();
        emitDictSetRoutine();
        emitDictHasRoutine();
        emitPrintArrayRoutine();
        emitStrlenRoutine();
        emitPrintCstrRoutine();
        emitWriteCstrRoutine();
        emitStrEqRoutine();
        emitAllocRoutine();
        emitConcatRoutine();
        emitAppendRoutine();
        emitCStrListRoutine();
        emitStrDupRoutine();
        emitWildcardRoutine();
        if (evTriggerUsed_) emitEventTriggerRoutine();
        emitLengthRoutine();
        emitIpowRoutine();
        emitDivZeroTrapRoutine();

        if (dynamicLink_) {
            std::set<std::string> seen;
            for (const auto& [unusedOffset, calleeName] : callFixups) {
                if (funcOffsets.count(calleeName) || !seen.insert(calleeName).second) continue;
                std::string exportName, library;
                if (!armExternalName(calleeName, exportName, library)) continue;
                emitExternalStub(calleeName, exportName, library);
            }
        } else if (staticLink_) {
            std::set<std::string> seen;
            for (const auto& [unusedOffset, calleeName] : callFixups) {
                if (funcOffsets.count(calleeName) || !seen.insert(calleeName).second) continue;
                std::string exportName, library;
                if (armExternalName(calleeName, exportName, library))
                    externals_.push_back({calleeName, exportName, library, 0});
            }
        }

        for (auto& fx : fixups) {
            auto it = labelOffsets.find(fx.labelId);
            if (it == labelOffsets.end())
                throw ACError::backend("ARM backend: unresolved branch label " + std::to_string(fx.labelId));
            int64_t delta = (int64_t)it->second - (int64_t)fx.bufOff;
            if ((delta % 4) != 0)
                throw ACError::backend("ARM backend: unaligned branch target " + std::to_string(fx.labelId));
            int32_t words = (int32_t)(delta / 4);
            if ((fx.kind == FixKind::B || fx.kind == FixKind::BL)
                    && (words < -(1<<25) || words >= (1<<25)))
                throw ACError::backend("ARM backend: branch target out of range");
            if ((fx.kind == FixKind::CBZ || fx.kind == FixKind::CBNZ || fx.kind == FixKind::BCOND)
                    && (words < -(1<<18) || words >= (1<<18)))
                throw ACError::backend("ARM backend: conditional branch target out of range");
            switch (fx.kind) {
                case FixKind::B:     em.patch32(fx.bufOff, 0x14000000u | ((uint32_t)words & 0x3FFFFFFu)); break;
                case FixKind::BL:    em.patch32(fx.bufOff, 0x94000000u | ((uint32_t)words & 0x3FFFFFFu)); break;
                case FixKind::CBZ:   em.patch32(fx.bufOff, 0xB4000000u | (((uint32_t)words & 0x7FFFFu)<<5) | rn(fx.reg)); break;
                case FixKind::CBNZ:  em.patch32(fx.bufOff, 0xB5000000u | (((uint32_t)words & 0x7FFFFu)<<5) | rn(fx.reg)); break;
                case FixKind::BCOND: em.patch32(fx.bufOff, 0x54000000u | (((uint32_t)words & 0x7FFFFu)<<5) | (uint32_t)fx.cond); break;
                case FixKind::ADR: {
                    // ADR takes a byte offset (immlo = low 2 bits, immhi = the rest), not a word count.
                    uint32_t bytes = (uint32_t)(int32_t)(delta);
                    em.patch32(fx.bufOff, 0x10000000u | ((bytes & 3u) << 29) | (((bytes >> 2) & 0x7FFFFu) << 5) | rn(fx.reg));
                    break;
                }
            }
        }
        // ADR to an event handler's entry point (bind <key> to <fn>): both are known only now.
        for (const auto& fx : funcAdrFixups_) {
            auto it = funcOffsets.find(fx.name);
            if (it == funcOffsets.end())
                throw ACError::backend("ARM backend: '" + fx.name + "' is not a compiled function");
            int64_t d = (int64_t)it->second - (int64_t)fx.bufOff;
            uint32_t bytes = (uint32_t)(int32_t)d;
            em.patch32(fx.bufOff, 0x10000000u | ((bytes & 3u) << 29) | (((bytes >> 2) & 0x7FFFFu) << 5) | rn(fx.reg));
        }
        for (auto& [bufOff, calleeName] : callFixups) {
            auto it = funcOffsets.find(calleeName);
            if (it == funcOffsets.end()) {
                auto ext = externalStubOffsets_.find(calleeName);
                if (ext != externalStubOffsets_.end()) {
                    int64_t delta = (int64_t)ext->second - (int64_t)bufOff;
                    if ((delta % 4) != 0)
                        throw ACError::backend("ARM backend: unaligned external call target '" + calleeName + "'");
                    int32_t words = (int32_t)(delta / 4);
                    if (words < -(1<<25) || words >= (1<<25))
                        throw ACError::backend("ARM backend: external call target out of range for '" + calleeName + "'");
                    em.patch32(bufOff, 0x94000000u | ((uint32_t)words & 0x3FFFFFFu));
                    continue;
                }
                if (staticLink_) {
                    std::string exportName, library;
                    if (armExternalName(calleeName, exportName, library)) {
                        externalCalls_.push_back({bufOff, calleeName});
                        em.patch32(bufOff, 0x94000000u); // R_AARCH64_CALL26 filled by ld
                        continue;
                    }
                }
                if (calleeName.find('.') != std::string::npos)
                    throw ACError::backend("ARM backend: ilib call '" + calleeName
                        + "' requires ARM library linking; no ARM library mapping exists yet");
                throw ACError::backend("ARM backend: call to unresolved function '" + calleeName
                    + "' (bundle methods and forward-declared-only functions are not yet implemented)");
            }
            int64_t delta = (int64_t)it->second - (int64_t)bufOff;
            if ((delta % 4) != 0)
                throw ACError::backend("ARM backend: unaligned call target '" + calleeName + "'");
            int32_t words = (int32_t)(delta / 4);
            if (words < -(1<<25) || words >= (1<<25))
                throw ACError::backend("ARM backend: call target out of range for '" + calleeName + "'");
            em.patch32(bufOff, 0x94000000u | ((uint32_t)words & 0x3FFFFFFu));
        }
        // Symbols for the assembly listing: function entries, IR labels, and the external stubs
        // (named by their export, so a linker can bind them to the ilib .so).
        std::map<size_t, std::vector<std::string>> symbols;
        auto clean = [](std::string s) {
            for (auto& c : s) if (!(std::isalnum((unsigned char)c) || c == '_')) c = '_';
            return s;
        };
        for (const auto& [name, off] : funcOffsets) symbols[off].push_back(clean(name));
        for (const auto& [id, off] : labelOffsets)
            symbols[off].push_back(id < 0 ? ".Lsyn" + std::to_string(-id) : ".L" + std::to_string(id));
        for (const auto& ext : externals_) symbols[ext.stubImmOffset].push_back(clean(ext.exportName));
        ArmCompiledImage img;
        img.text = em.bytes();
        img.slotCount = nextGlobalSlot_;
        img.externals = externals_;
        img.externalCalls = externalCalls_;
        img.dataRanges = em.dataRanges;
        img.symbols = std::move(symbols);
        return img;
    }
};

static void patchArmMovImm64(std::vector<uint8_t>& text, size_t off, uint64_t value) {
    for (int hw = 0; hw < 4; hw++) {
        uint32_t w = (hw == 0 ? 0xD2800000u : 0xF2800000u)
                   | ((uint32_t)hw << 21)
                   | (uint32_t)(((value >> (16 * hw)) & 0xFFFFu) << 5)
                   | (uint32_t)R::X16;
        text[off + hw*4 + 0] = (uint8_t)w;
        text[off + hw*4 + 1] = (uint8_t)(w >> 8);
        text[off + hw*4 + 2] = (uint8_t)(w >> 16);
        text[off + hw*4 + 3] = (uint8_t)(w >> 24);
    }
}

static std::string armShellQuote(const std::string& s) {
    std::string q = "'";
    for (char c : s) {
        if (c == '\'') q += "'\\''";
        else q += c;
    }
    return q + "'";
}

static std::string armAsmQuote(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '\\' || c == '"') q += '\\';
        q += c;
    }
    return q + "\"";
}

static bool writeArmStaticELF(const std::string& path, const ArmCompiledImage& image,
                              const std::string& libraryPaths) {
    const std::string asmPath = path + ".static.s";
    const std::string scriptPath = path + ".static.ld";
    {
        std::ofstream asmFile(asmPath);
        std::ofstream script(scriptPath);
        if (!asmFile || !script) return false;
        asmFile << ".section .ac_text,\"ax\"\n.global ac_entry\n.type ac_entry,%function\nac_entry:\n";
        asmFile << ".incbin " << armAsmQuote(path + ".static.text") << "\n";
        for (const auto& [off, name] : image.externalCalls) {
            std::string exportName, library;
            if (!armExternalName(name, exportName, library)) return false;
            asmFile << ".reloc ac_entry + " << off << ", R_AARCH64_CALL26, "
                    << exportName << "\n";
        }
        asmFile << ".size ac_entry, .-ac_entry\n.section .text\n"
                << ".global main\n.type main,%function\nmain:\n"
                << "  bl ac_entry\n  mov w0, #0\n  ret\n.size main, .-main\n";
        // Keep the cross-linker's normal static startup/program headers.  Only insert the
        // fixed AC pages before the toolchain's .text output; a complete replacement script
        // breaks glibc's early aux-vector setup before ac_entry is reached.
        script << "SECTIONS\n{\n"
               << "  . = 0x401000;\n  .ac_bss (NOLOAD) : { . += 0x1000; }\n"
               << "  . = 0x402000;\n  .ac_text : { KEEP(*(.ac_text)) }\n"
               << "}\nINSERT BEFORE .text\n";
    }
    {
        std::ofstream raw(path + ".static.text", std::ios::binary);
        if (!raw) return false;
        raw.write((const char*)image.text.data(), (std::streamsize)image.text.size());
    }

    std::string cmd = "aarch64-linux-gnu-g++ -static -no-pie -Wl,-z,max-page-size=0x1000 -Wl,-T," + armShellQuote(scriptPath)
                    + " -o " + armShellQuote(path) + " " + armShellQuote(asmPath);
    std::set<std::string> seenPaths;
    size_t begin = 0;
    while (begin <= libraryPaths.size()) {
        size_t end = libraryPaths.find(':', begin);
        std::string dir = libraryPaths.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!dir.empty() && seenPaths.insert(dir).second) cmd += " -L" + armShellQuote(dir);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    std::set<std::string> linkedLibs;
    for (const auto& ext : image.externals) {
        if (!linkedLibs.insert(ext.library).second) continue;
        std::string lib = ext.library;
        if (lib.rfind("lib", 0) == 0) lib.erase(0, 3);
        size_t dot = lib.find(".so");
        if (dot != std::string::npos) lib.erase(dot);
        cmd += " -Wl,--whole-archive -l" + armShellQuote(lib) + " -Wl,--no-whole-archive";
    }
    cmd += " -lstdc++ -lc -lgcc -lgcc_eh -lpthread -lm";
    int rc = std::system(cmd.c_str());
    std::remove(asmPath.c_str());
    std::remove(scriptPath.c_str());
    std::remove((path + ".static.text").c_str());
    if (rc != 0) {
        std::remove(path.c_str());
        throw ACError::backend("ARM backend: static linker failed");
    }
    chmod(path.c_str(), 0755);
    return true;
}

static bool writeArmDynamicELF(const std::string& path, ArmCompiledImage image,
                               const std::string& runpath) {
    const uint64_t BASE = 0x400000ULL, PGSZ = 0x1000ULL;
    const uint64_t textOff = 2 * PGSZ; // preserves exp_arm's fixed string/code addresses
    const char INTERP[] = "/lib/ld-linux-aarch64.so.1";
    const int n = (int)image.externals.size();

    std::vector<uint8_t> dynstr(1, 0);
    auto addStr = [&](const std::string& s) -> uint32_t {
        uint32_t off = (uint32_t)dynstr.size();
        dynstr.insert(dynstr.end(), s.begin(), s.end());
        dynstr.push_back(0);
        return off;
    };
    std::vector<uint32_t> symNames;
    for (const auto& e : image.externals) symNames.push_back(addStr(e.exportName));
    std::vector<std::string> libs;
    std::vector<uint32_t> libNames;
    for (const auto& e : image.externals) {
        if (std::find(libs.begin(), libs.end(), e.library) == libs.end()) {
            libs.push_back(e.library);
            libNames.push_back(addStr(e.library));
        }
    }
    uint32_t runpathName = 0;
    if (!runpath.empty()) runpathName = addStr(runpath);

    std::vector<uint32_t> hash = {1u, (uint32_t)(n + 1), n ? 1u : 0u, 0u};
    for (int i = 1; i <= n; i++) hash.push_back(i == n ? 0u : (uint32_t)(i + 1));
    std::vector<Elf64Sym> syms((size_t)n + 1);
    for (int i = 0; i < n; i++) {
        syms[i + 1].st_name = symNames[i];
        syms[i + 1].st_info = 0x12; // STB_GLOBAL | STT_FUNC
        syms[i + 1].st_shndx = 0;    // undefined; resolved from DT_NEEDED objects
    }
    std::vector<Elf64Rela> relas((size_t)n);

    // Layout (mirrors writeArmELF): page 0 = read-only ELF metadata, page 1 = zero-filled
    // globals page (RW, no file bytes), page 2 = code (RX), then GOT+.dynamic (RW).
    const size_t hdrBytes = sizeof(ElfEhdr) + 6 * sizeof(ElfPhdr);
    const uint64_t interpOff = hdrBytes;
    const uint64_t hashOff = interpOff + sizeof(INTERP);
    const uint64_t symOff = hashOff + hash.size() * sizeof(uint32_t);
    const uint64_t strOff = symOff + syms.size() * sizeof(Elf64Sym);
    const uint64_t relaOff = strOff + dynstr.size();
    const uint64_t metaEnd = relaOff + relas.size() * sizeof(Elf64Rela);
    if (metaEnd > PGSZ)
        throw ACError::backend("ARM backend: dynamic ELF metadata exceeds the read-only header page");

    const uint64_t textVA = BASE + textOff;
    const uint64_t textEnd = textOff + image.text.size();
    const uint64_t seg2Off = (textEnd + PGSZ - 1) & ~(PGSZ - 1);
    const uint64_t seg2VA = BASE + seg2Off;
    const uint64_t gotOff = seg2Off;
    const uint64_t gotVA = BASE + gotOff;
    const size_t gotSize = (size_t)n * 8;

    std::vector<Elf64Dyn> dyn;
    auto addDyn = [&](int64_t tag, uint64_t val) { dyn.push_back({tag, val}); };
    for (uint32_t off : libNames) addDyn(1, off);       // DT_NEEDED
    if (!runpath.empty()) addDyn(29, runpathName);       // DT_RUNPATH
    addDyn(4, BASE + hashOff);                          // DT_HASH
    addDyn(5, BASE + strOff);                           // DT_STRTAB
    addDyn(6, BASE + symOff);                           // DT_SYMTAB
    addDyn(10, dynstr.size());                          // DT_STRSZ
    addDyn(11, sizeof(Elf64Sym));                       // DT_SYMENT
    addDyn(7, BASE + relaOff);                          // DT_RELA
    addDyn(8, relas.size() * sizeof(Elf64Rela));        // DT_RELASZ
    addDyn(9, sizeof(Elf64Rela));                       // DT_RELAENT
    addDyn(2, relas.size() * sizeof(Elf64Rela));        // DT_PLTRELSZ
    addDyn(20, 7);                                      // DT_PLTREL = DT_RELA
    addDyn(23, BASE + relaOff);                         // DT_JMPREL
    addDyn(30, 8);                                      // DF_BIND_NOW
    addDyn(0, 0);
    const uint64_t dynamicOff = gotOff + gotSize;
    const uint64_t dynamicVA = BASE + dynamicOff;
    addDyn(3, gotVA);                                   // DT_PLTGOT

    // DT_PLTGOT is optional for eager binding, but keep it before DT_NULL in the table.
    // Rebuild the table with the tag in the correct pre-NULL position.
    dyn.pop_back();
    dyn.pop_back();
    addDyn(3, gotVA);
    addDyn(0, 0);

    for (int i = 0; i < n; i++) {
        relas[i].r_offset = gotVA + (uint64_t)i * 8;
        relas[i].r_info = ((uint64_t)(i + 1) << 32) | 1026u; // R_AARCH64_JUMP_SLOT
        relas[i].r_addend = 0;
        patchArmMovImm64(image.text, image.externals[i].stubImmOffset, gotVA + (uint64_t)i * 8);
    }

    const uint64_t dynamicSize = dyn.size() * sizeof(Elf64Dyn);
    const uint64_t seg2Size = gotSize + dynamicSize;
    ElfEhdr eh{};
    eh.e_ident[0]=0x7f; eh.e_ident[1]='E'; eh.e_ident[2]='L'; eh.e_ident[3]='F';
    eh.e_ident[4]=2; eh.e_ident[5]=1; eh.e_ident[6]=1;
    eh.e_type=2; eh.e_machine=EM_AARCH64; eh.e_version=1;
    eh.e_entry=textVA; eh.e_phoff=sizeof(ElfEhdr); eh.e_ehsize=sizeof(ElfEhdr);
    eh.e_phentsize=sizeof(ElfPhdr); eh.e_phnum=6;
    ElfPhdr interp{}, meta{}, globals{}, code{}, rw{}, dp{};
    interp.p_type=3; interp.p_flags=4; interp.p_offset=interpOff;
    interp.p_vaddr=BASE+interpOff; interp.p_paddr=interp.p_vaddr;
    interp.p_filesz=sizeof(INTERP); interp.p_memsz=sizeof(INTERP); interp.p_align=1;
    meta.p_type=1; meta.p_flags=4; meta.p_offset=0; meta.p_vaddr=BASE; meta.p_paddr=BASE;
    meta.p_filesz=metaEnd; meta.p_memsz=metaEnd; meta.p_align=PGSZ; // R: ELF headers + tables
    globals.p_type=1; globals.p_flags=6; globals.p_offset=0; globals.p_vaddr=BASE+PGSZ;
    globals.p_paddr=globals.p_vaddr; globals.p_filesz=0; globals.p_memsz=PGSZ; globals.p_align=PGSZ;
    code.p_type=1; code.p_flags=5; code.p_offset=textOff; code.p_vaddr=textVA; code.p_paddr=textVA;
    code.p_filesz=image.text.size(); code.p_memsz=image.text.size(); code.p_align=PGSZ; // RX
    rw.p_type=1; rw.p_flags=6; rw.p_offset=seg2Off; rw.p_vaddr=seg2VA; rw.p_paddr=seg2VA;
    rw.p_filesz=seg2Size; rw.p_memsz=seg2Size; rw.p_align=PGSZ;
    dp.p_type=2; dp.p_flags=6; dp.p_offset=dynamicOff; dp.p_vaddr=dynamicVA; dp.p_paddr=dynamicVA;
    dp.p_filesz=dynamicSize; dp.p_memsz=dynamicSize; dp.p_align=8;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((char*)&eh, sizeof(eh)); f.write((char*)&interp, sizeof(interp));
    f.write((char*)&meta, sizeof(meta)); f.write((char*)&globals, sizeof(globals));
    f.write((char*)&code, sizeof(code)); f.write((char*)&rw, sizeof(rw)); f.write((char*)&dp, sizeof(dp));
    auto padTo = [&](uint64_t off) {
        uint64_t cur = (uint64_t)f.tellp();
        if (cur < off) { std::vector<uint8_t> z((size_t)(off-cur)); f.write((char*)z.data(), z.size()); }
    };
    padTo(interpOff); f.write(INTERP, sizeof(INTERP));
    padTo(hashOff); f.write((char*)hash.data(), hash.size()*sizeof(uint32_t));
    padTo(symOff); f.write((char*)syms.data(), syms.size()*sizeof(Elf64Sym));
    padTo(strOff); f.write((char*)dynstr.data(), dynstr.size());
    padTo(relaOff); f.write((char*)relas.data(), relas.size()*sizeof(Elf64Rela));
    padTo(textOff); f.write((char*)image.text.data(), image.text.size());
    padTo(seg2Off); f.write((char*)relas.data(), 0); // keep the write cursor at the RW segment
    std::vector<uint64_t> got((size_t)n, 0); f.write((char*)got.data(), got.size()*sizeof(uint64_t));
    f.write((char*)dyn.data(), dyn.size()*sizeof(Elf64Dyn));
    f.close(); chmod(path.c_str(), 0755); return true;
}

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

bool generateArmBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile,
                             bool staticLink) {
    using namespace AC_ArmGen;
    const uint64_t BASE = 0x400000ULL, PGSZ = 0x1000ULL;
    const uint64_t dataVA = BASE + PGSZ, codeVA = BASE + 2*PGSZ;
    ArmCompiler compiler(ir, codeVA, !staticLink, staticLink);
    ArmCompiledImage image = compiler.compile(dataVA);
    if ((size_t)image.slotCount * 8 > PGSZ) return false; // fixed-page limit
    if (staticLink && !image.externals.empty()) {
        const char* lp = std::getenv("AC_ARM_ILIB_PATH");
        return writeArmStaticELF(outputFile, image, lp ? lp : "");
    }
    if (!image.externals.empty()) {
        const char* rp = std::getenv("AC_ARM_ILIB_PATH");
        return writeArmDynamicELF(outputFile, std::move(image), rp ? rp : "");
    }
    return writeArmELF(outputFile, image.text);
}

// ── GNU assembly listing (the RISC target) ──────────────────────────────────────────────────────
// The RISC target is the same ARM codegen, rendered as assembly text instead of an ELF file. The
// instruction text comes from objdump (the ISA decoder) applied to the exact bytes this backend
// emitted, so the listing can't drift from the binary. Branch and adr targets become labels, inline
// data becomes .byte lines, and absolute addresses (globals page, code page) keep the ARM ELF layout.

using AC_ArmGen::ArmCompiledImage;

// Disassembles one run of code bytes placed at `va`; one (address, text) pair per instruction word.
static bool armDisassembleRun(const std::vector<uint8_t>& bytes, uint64_t va,
                              std::vector<std::pair<uint64_t, std::string>>& out) {
    char tmpl[] = "/tmp/acarm_listing_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return false;
    bool wrote = write(fd, bytes.data(), bytes.size()) == (ssize_t)bytes.size();
    close(fd);
    if (!wrote) { unlink(tmpl); return false; }
    char vaText[32];
    std::snprintf(vaText, sizeof(vaText), "0x%llx", (unsigned long long)va);
    std::string cmd = std::string("aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=") + vaText + " " + tmpl + " 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) { unlink(tmpl); return false; }
    char line[1024];
    while (std::fgets(line, sizeof(line), p)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t i = s.find_first_not_of(' ');
        if (i == std::string::npos) continue;
        size_t colon = s.find(':', i);
        if (colon == std::string::npos) continue;
        std::string addrText = s.substr(i, colon - i);
        if (addrText.empty() || addrText.find_first_not_of("0123456789abcdef") != std::string::npos) continue;
        // "addr:\tword \ttext": skip the encoded word, keep the mnemonic and operands.
        size_t tab = s.find('\t', colon + 1);
        if (tab == std::string::npos) continue;
        size_t tab2 = s.find('\t', tab + 1);
        std::string text = (tab2 == std::string::npos) ? s.substr(tab + 1) : s.substr(tab2 + 1);
        for (auto& c : text) if (c == '\t') c = ' ';
        size_t a = text.find_first_not_of(' ');
        text = (a == std::string::npos) ? "" : text.substr(a);
        while (!text.empty() && text.back() == ' ') text.pop_back();
        out.push_back({std::stoull(addrText, nullptr, 16), text});
    }
    pclose(p);
    unlink(tmpl);
    return true;
}

// Renders a compiled image as GNU AArch64 assembly text.
static bool armRenderListing(const ArmCompiledImage& img, uint64_t codeVA, std::string& listing, std::string& error) {
    const size_t n = img.text.size();
    // Code runs are the bytes outside the recorded inline-data ranges.
    std::vector<std::pair<size_t,size_t>> dataRanges = img.dataRanges;
    std::sort(dataRanges.begin(), dataRanges.end());
    std::vector<std::pair<size_t,size_t>> codeRuns;
    size_t cursor = 0;
    for (const auto& [b, e] : dataRanges) {
        if (b > cursor) codeRuns.push_back({cursor, b});
        cursor = std::max(cursor, e);
    }
    if (cursor < n) codeRuns.push_back({cursor, n});

    // Decode every code run; entries are (text offset, instruction text).
    std::map<size_t, std::string> insnAt;
    for (const auto& [b, e] : codeRuns) {
        std::vector<uint8_t> bytes(img.text.begin() + b, img.text.begin() + e);
        std::vector<std::pair<uint64_t, std::string>> decoded;
        if (!armDisassembleRun(bytes, codeVA + b, decoded)) {
            error = "could not run aarch64-linux-gnu-objdump (needed for the RISC listing)";
            return false;
        }
        for (const auto& [addr, text] : decoded) insnAt[(size_t)(addr - codeVA)] = text;
    }

    // Symbols: the compiled ones, plus a label for every branch/adr target that has none yet.
    std::map<size_t, std::vector<std::string>> symbols = img.symbols;
    auto labelFor = [&](size_t off) -> std::string {
        auto it = symbols.find(off);
        if (it != symbols.end() && !it->second.empty()) return it->second.front();
        std::string name = ".Lt" + std::to_string(off);
        symbols[off].push_back(name);
        return name;
    };
    // A branch-like instruction's target operand is the first 0x-number before any comment.
    auto isBranchLike = [](const std::string& text) {
        std::string mn = text.substr(0, text.find(' '));
        return mn == "b" || mn == "bl" || mn.rfind("b.", 0) == 0 || mn == "cbz" || mn == "cbnz" || mn == "adr";
    };
    for (auto& [off, text] : insnAt) {
        if (!isBranchLike(text)) continue;
        size_t cut = text.find("//");
        std::string code = text.substr(0, cut);
        size_t hx = code.find("0x");
        if (hx == std::string::npos) continue;
        size_t end = code.find_first_not_of("0123456789abcdefx", hx);
        uint64_t target = std::stoull(code.substr(hx, end == std::string::npos ? std::string::npos : end - hx), nullptr, 0);
        if (target < codeVA || target >= codeVA + n) continue;
        std::string name = labelFor((size_t)(target - codeVA));
        text = code.substr(0, hx) + name + (cut == std::string::npos ? "" : " " + text.substr(cut));
    }

    std::ostringstream out;
    out << "// AC -> RISC: AArch64 GNU assembly. The same code the ARM backend emits, rendered as text.\n";
    out << "// Absolute addresses assume the ARM ELF layout: code at 0x" << std::hex << codeVA
        << ", globals page at 0x" << (codeVA - 0x1000) << std::dec << ".\n";
    out << "// Link it at those addresses: aarch64-linux-gnu-ld -Ttext=0x" << std::hex << codeVA
        << " --section-start=.bss=0x" << (codeVA - 0x1000) << std::dec << " out.o -o out\n";
    out << "// External ilib calls go through stub labels named after their export. The stub immediate is the\n";
    out << "// GOT slot, which only the ARM ELF writer fills in, so run an ilib program as the ARM binary.\n";
    out << "    .text\n    .global _start\n";

    // Walk the image in offset order: code instructions and inline data interleaved.
    auto emitSymbols = [&](size_t off) {
        auto it = symbols.find(off);
        if (it == symbols.end()) return;
        for (const auto& name : it->second) {
            out << name << ":\n";
        }
    };
    out << "_start:\n";            // the program entry is always the first instruction
    emitSymbols(0);
    size_t off = 0;
    size_t dataIdx = 0;
    while (off < n) {
        if (dataIdx < dataRanges.size() && dataRanges[dataIdx].first == off) {
            // Inline data, 16 bytes to a .byte line.
            size_t end = dataRanges[dataIdx].second;
            emitSymbols(off);
            for (size_t i = off; i < end; i++) {
                if ((i - off) % 16 == 0) {
                    if (i != off) out << "\n";
                    out << "    .byte ";
                } else out << ", ";
                char hb[8];
                std::snprintf(hb, sizeof(hb), "0x%02x", img.text[i]);
                out << hb;
            }
            out << "\n";
            off = end;
            dataIdx++;
            continue;
        }
        emitSymbols(off);
        auto it = insnAt.find(off);
        if (it == insnAt.end()) { error = "internal: no decoded instruction at offset " + std::to_string(off); return false; }
        out << "    " << it->second << "\n";
        off += 4;
    }
    listing = out.str();
    return true;
}

// Entry point for the RISC target: same codegen as generateArmBinaryFromIR, text output.
bool generateArmListingFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile, std::string& error) {
    using namespace AC_ArmGen;
    const uint64_t BASE = 0x400000ULL, PGSZ = 0x1000ULL;
    const uint64_t dataVA = BASE + PGSZ, codeVA = BASE + 2*PGSZ;
    ArmCompiler compiler(ir, codeVA, /*dynamicLink=*/true, /*staticLink=*/false);
    ArmCompiledImage image = compiler.compile(dataVA);
    // An ilib call goes through a GOT slot that only the ARM ELF writer fills in; a listing has no way to
    // resolve it, so refuse here rather than emit a program that jumps to an unresolved address.
    if (!image.externals.empty()) {
        error = "program calls an ilib library (" + image.externals.front().exportName
              + "); the RISC listing cannot link libraries, use --target ARM";
        return false;
    }
    if ((size_t)image.slotCount * 8 > PGSZ) { error = "too many globals for the fixed globals page"; return false; }
    std::string listing;
    if (!armRenderListing(image, codeVA, listing, error)) return false;
    // The globals page is zeroed read/write memory the code addresses absolutely; the linker places this
    // .bss at the globals address (see the header comment), so the listing is runnable as written.
    listing += "    .bss\n    .balign 4096\n    .zero 4096\n";
    std::ofstream f(outputFile);
    if (!f) { error = "cannot write " + outputFile; return false; }
    f << listing;
    return true;
}
