/* 
═══════════════════════════════════════════════════════════════════════════
   EXPERIMENTAL BINARY GENERATOR (exp_bny.cpp)
   Direct machine code generation - skips ASM, .o, and external linker.
   Generates native executables with proper OS headers and linking.
   Architecture: x86-64 only (for now)
   OS Support: Linux (ELF), macOS (Mach-O), Windows (PE) - future
═══════════════════════════════════════════════════════════════════════════
*/
#include "../include/ir.hpp"
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <cstring>
#include <sys/stat.h>  // for chmod

#ifdef __linux__
    #define TARGET_LINUX 1
#elif __APPLE__
    #define TARGET_MACOS 1
#elif _WIN32
    #define TARGET_WINDOWS 1
#endif

namespace AC_BinaryGen {

// ═══════════════════════════════════════════════════════════════════════════
// REGISTER ALLOCATOR (Linear Scan)
// ═══════════════════════════════════════════════════════════════════════════

enum class PhysReg {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSI = 6, RDI = 7, R8 = 8, R9 = 9,
    R10 = 10, R11 = 11, R12 = 12, R13 = 13,
    R14 = 14, R15 = 15,
    SPILL = 255  // Spilled to stack
};

struct LiveInterval {
    int vreg;           // Virtual register ID
    int start;          // First use
    int end;            // Last use
    PhysReg physReg;    // Assigned physical register
    int spillSlot;      // Stack slot if spilled (-1 if not spilled)
    
    LiveInterval(int v, int s, int e) 
        : vreg(v), start(s), end(e), physReg(PhysReg::SPILL), spillSlot(-1) {}
};

class LinearScanAllocator {
    std::vector<LiveInterval> intervals;
    std::vector<PhysReg> availableRegs;
    std::vector<LiveInterval*> active;
    int nextSpillSlot = 0;
    
    // Available registers (caller-saved for simplicity)
    void initAvailableRegs() {
        availableRegs = {
            PhysReg::RAX, PhysReg::RCX, PhysReg::RDX,
            PhysReg::RSI, PhysReg::RDI,
            PhysReg::R8, PhysReg::R9, PhysReg::R10, PhysReg::R11
        };
    }
    
    void expireOldIntervals(int currentPos) {
        auto it = active.begin();
        while (it != active.end()) {
            if ((*it)->end < currentPos) {
                // Interval expired, free its register
                availableRegs.push_back((*it)->physReg);
                it = active.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void spillAtInterval(LiveInterval& interval) {
        // Find interval with furthest end point
        LiveInterval* spill = active.back();
        
        if (spill->end > interval.end) {
            // Spill the furthest one and use its register
            interval.physReg = spill->physReg;
            spill->physReg = PhysReg::SPILL;
            spill->spillSlot = nextSpillSlot++;
            
            // Remove spilled from active and add current
            active.pop_back();
            active.push_back(&interval);
            
            // Keep active sorted by end point
            std::sort(active.begin(), active.end(), 
                [](LiveInterval* a, LiveInterval* b) { return a->end < b->end; });
        } else {
            // Spill current interval
            interval.physReg = PhysReg::SPILL;
            interval.spillSlot = nextSpillSlot++;
        }
    }
    
public:
    void addInterval(int vreg, int start, int end) {
        intervals.emplace_back(vreg, start, end);
    }
    
    void allocate() {
        initAvailableRegs();
        
        // Sort intervals by start point
        std::sort(intervals.begin(), intervals.end(),
            [](const LiveInterval& a, const LiveInterval& b) { return a.start < b.start; });
        
        for (auto& interval : intervals) {
            expireOldIntervals(interval.start);
            
            if (availableRegs.empty()) {
                spillAtInterval(interval);
            } else {
                // Allocate register
                interval.physReg = availableRegs.back();
                availableRegs.pop_back();
                active.push_back(&interval);
                
                // Keep active sorted by end point
                std::sort(active.begin(), active.end(),
                    [](LiveInterval* a, LiveInterval* b) { return a->end < b->end; });
            }
        }
    }
    
    PhysReg getPhysReg(int vreg) const {
        for (const auto& interval : intervals) {
            if (interval.vreg == vreg) return interval.physReg;
        }
        return PhysReg::SPILL;
    }
    
    int getSpillSlot(int vreg) const {
        for (const auto& interval : intervals) {
            if (interval.vreg == vreg) return interval.spillSlot;
        }
        return -1;
    }
    
    int getTotalSpillSlots() const { return nextSpillSlot; }
};

// ═══════════════════════════════════════════════════════════════════════════
// MACHINE CODE EMITTER (x86-64)
// ═══════════════════════════════════════════════════════════════════════════

class X64CodeEmitter {
    std::vector<uint8_t> code;
    std::map<std::string, size_t> labels;      // label -> code offset
    std::map<size_t, std::string> fixups;      // offset -> label (for backpatching)
    
public:
    // Get current code offset
    size_t offset() const { return code.size(); }
    
    // Define a label at current position
    void label(const std::string& name) {
        labels[name] = offset();
    }
    
    // Emit raw bytes
    void emit(uint8_t byte) { code.push_back(byte); }
    void emit16(uint16_t val) { 
        emit(val & 0xFF); 
        emit((val >> 8) & 0xFF); 
    }
    void emit32(uint32_t val) { 
        emit(val & 0xFF); 
        emit((val >> 8) & 0xFF); 
        emit((val >> 16) & 0xFF); 
        emit((val >> 24) & 0xFF); 
    }
    void emit64(uint64_t val) {
        emit32(val & 0xFFFFFFFF);
        emit32((val >> 32) & 0xFFFFFFFF);
    }
    
    // REX prefix helper
    void emitREX(bool w, int reg, int rm) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08;  // REX.W
        if (reg >= 8) rex |= 0x04;  // REX.R
        if (rm >= 8) rex |= 0x01;   // REX.B
        if (rex != 0x40) emit(rex);
    }
    
    // ModR/M byte helper
    void emitModRM(int mod, int reg, int rm) {
        emit((mod << 6) | ((reg & 7) << 3) | (rm & 7));
    }
    
    // ── Generic register operations ─────────────────────────────────────────
    
    // mov reg64, imm32 (sign-extended)
    void mov_reg_imm32(PhysReg reg, int32_t imm) {
        emitREX(true, 0, (int)reg);
        emit(0xC7);
        emitModRM(3, 0, (int)reg);
        emit32(imm);
    }
    
    // mov reg64, imm64
    void mov_reg_imm64(PhysReg reg, uint64_t imm) {
        emitREX(true, 0, (int)reg);
        emit(0xB8 + ((int)reg & 7));
        emit64(imm);
    }
    
    // mov reg64, reg64
    void mov_reg_reg(PhysReg dst, PhysReg src) {
        emitREX(true, (int)src, (int)dst);
        emit(0x89);
        emitModRM(3, (int)src, (int)dst);
    }
    
    // mov reg64, [rbp+offset]
    void mov_reg_rbp_offset(PhysReg reg, int32_t offset) {
        emitREX(true, (int)reg, 5);  // rbp = 5
        emit(0x8B);
        if (offset >= -128 && offset <= 127) {
            emitModRM(1, (int)reg, 5);  // [rbp+disp8]
            emit(offset & 0xFF);
        } else {
            emitModRM(2, (int)reg, 5);  // [rbp+disp32]
            emit32(offset);
        }
    }
    
    // mov [rbp+offset], reg64
    void mov_rbp_offset_reg(int32_t offset, PhysReg reg) {
        emitREX(true, (int)reg, 5);  // rbp = 5
        emit(0x89);
        if (offset >= -128 && offset <= 127) {
            emitModRM(1, (int)reg, 5);  // [rbp+disp8]
            emit(offset & 0xFF);
        } else {
            emitModRM(2, (int)reg, 5);  // [rbp+disp32]
            emit32(offset);
        }
    }
    
    // add reg64, reg64
    void add_reg_reg(PhysReg dst, PhysReg src) {
        emitREX(true, (int)src, (int)dst);
        emit(0x01);
        emitModRM(3, (int)src, (int)dst);
    }
    
    // sub reg64, reg64
    void sub_reg_reg(PhysReg dst, PhysReg src) {
        emitREX(true, (int)src, (int)dst);
        emit(0x29);
        emitModRM(3, (int)src, (int)dst);
    }
    
    // imul reg64, reg64
    void imul_reg_reg(PhysReg dst, PhysReg src) {
        emitREX(true, (int)dst, (int)src);
        emit(0x0F); emit(0xAF);
        emitModRM(3, (int)dst, (int)src);
    }
    
    // xor reg64, reg64
    void xor_reg_reg(PhysReg dst, PhysReg src) {
        emitREX(true, (int)src, (int)dst);
        emit(0x31);
        emitModRM(3, (int)src, (int)dst);
    }
    
    // cmp reg64, reg64
    void cmp_reg_reg(PhysReg left, PhysReg right) {
        emitREX(true, (int)right, (int)left);
        emit(0x39);
        emitModRM(3, (int)right, (int)left);
    }
    
    // setcc reg8 (set byte on condition)
    void setcc(uint8_t condition, PhysReg reg) {
        if ((int)reg >= 4 && (int)reg <= 7) {
            emit(0x40);  // REX prefix for spl, bpl, sil, dil
        }
        emit(0x0F);
        emit(0x90 + condition);  // SETE, SETNE, SETL, etc.
        emitModRM(3, 0, (int)reg);
    }
    
    // movzx reg64, reg8 (zero-extend byte to qword)
    void movzx_reg_reg8(PhysReg dst, PhysReg src) {
        emitREX(true, (int)dst, (int)src);
        emit(0x0F); emit(0xB6);
        emitModRM(3, (int)dst, (int)src);
    }
    
    // push reg64
    void push_reg(PhysReg reg) {
        if ((int)reg >= 8) emit(0x41);  // REX.B
        emit(0x50 + ((int)reg & 7));
    }
    
    // pop reg64
    void pop_reg(PhysReg reg) {
        if ((int)reg >= 8) emit(0x41);  // REX.B
        emit(0x58 + ((int)reg & 7));
    }
    
    // push rbp
    void push_rbp() { emit(0x55); }
    
    // pop rbp
    void pop_rbp() { emit(0x5D); }
    
    // mov rbp, rsp
    void mov_rbp_rsp() {
        emit(0x48); emit(0x89); emit(0xE5);
    }
    
    // sub rsp, imm32
    void sub_rsp_imm32(int32_t imm) {
        emit(0x48); emit(0x81); emit(0xEC);
        emit32(imm);
    }
    
    // add rsp, imm32
    void add_rsp_imm32(int32_t imm) {
        emit(0x48); emit(0x81); emit(0xC4);
        emit32(imm);
    }
    
    // ret
    void ret() { emit(0xC3); }
    
    // call rel32
    void call_rel32(const std::string& target) {
        emit(0xE8);
        size_t fixup_offset = offset();
        emit32(0);
        fixups[fixup_offset] = target;
    }
    
    // syscall
    void syscall() {
        emit(0x0F); emit(0x05);
    }
    
    // ── Fixup pass ──────────────────────────────────────────────────────────
    
    void applyFixups() {
        for (auto& [offset, target] : fixups) {
            if (labels.find(target) == labels.end()) continue;
            size_t target_offset = labels[target];
            int32_t rel = target_offset - (offset + 4);
            code[offset + 0] = (rel >> 0) & 0xFF;
            code[offset + 1] = (rel >> 8) & 0xFF;
            code[offset + 2] = (rel >> 16) & 0xFF;
            code[offset + 3] = (rel >> 24) & 0xFF;
        }
    }
    
    const std::vector<uint8_t>& getCode() const { return code; }
};

// ═══════════════════════════════════════════════════════════════════════════
// ABI COMPLIANCE LAYER
// ═══════════════════════════════════════════════════════════════════════════

enum class ABI {
    SYSV_AMD64,   // Linux, macOS, BSD
    WIN64         // Windows
};

struct ABIInfo {
    ABI type;
    std::vector<PhysReg> argRegs;
    std::vector<PhysReg> callerSaved;
    std::vector<PhysReg> calleeSaved;
    int shadowSpace;      // Windows requires 32 bytes
    int stackAlignment;   // 16-byte alignment
    bool hasRedZone;      // 128-byte red zone (SysV only)
    
    static ABIInfo getSysV() {
        return {
            ABI::SYSV_AMD64,
            {PhysReg::RDI, PhysReg::RSI, PhysReg::RDX, PhysReg::RCX, PhysReg::R8, PhysReg::R9},
            {PhysReg::RAX, PhysReg::RCX, PhysReg::RDX, PhysReg::RSI, PhysReg::RDI, 
             PhysReg::R8, PhysReg::R9, PhysReg::R10, PhysReg::R11},
            {PhysReg::RBX, PhysReg::R12, PhysReg::R13, PhysReg::R14, PhysReg::R15},
            0,    // No shadow space
            16,   // 16-byte alignment
            true  // Has red zone
        };
    }
    
    static ABIInfo getWin64() {
        return {
            ABI::WIN64,
            {PhysReg::RCX, PhysReg::RDX, PhysReg::R8, PhysReg::R9},
            {PhysReg::RAX, PhysReg::RCX, PhysReg::RDX, PhysReg::R8, PhysReg::R9, 
             PhysReg::R10, PhysReg::R11},
            {PhysReg::RBX, PhysReg::RDI, PhysReg::RSI, PhysReg::R12, PhysReg::R13, 
             PhysReg::R14, PhysReg::R15},
            32,   // 32-byte shadow space
            16,   // 16-byte alignment
            false // No red zone
        };
    }
    
    static ABIInfo getCurrent() {
#ifdef _WIN32
        return getWin64();
#else
        return getSysV();
#endif
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// STACK FRAME MANAGER
// ═══════════════════════════════════════════════════════════════════════════

class StackFrameManager {
    int localSize = 0;
    int spillSize = 0;
    int totalSize = 0;
    ABIInfo abi;
    
public:
    StackFrameManager() : abi(ABIInfo::getCurrent()) {}
    
    void setLocalSize(int size) { localSize = size; }
    void setSpillSize(int slots) { spillSize = slots * 8; }
    
    int calculateFrameSize() {
        // Total = locals + spills + shadow space
        int total = localSize + spillSize + abi.shadowSpace;
        
        // Align to 16 bytes (required by ABI)
        // Account for return address push (8 bytes)
        // Frame must be 16-byte aligned AFTER call
        total = ((total + 8 + 15) / 16) * 16 - 8;
        
        totalSize = total;
        return total;
    }
    
    int getLocalOffset(int index) const {
        // Locals start after spills
        return -(spillSize + (index + 1) * 8);
    }
    
    int getSpillOffset(int slot) const {
        // Spills are at top of frame
        return -(slot + 1) * 8;
    }
    
    int getTotalSize() const { return totalSize; }
    const ABIInfo& getABI() const { return abi; }
};

// ═══════════════════════════════════════════════════════════════════════════
// INSTRUCTION SELECTOR
// ═══════════════════════════════════════════════════════════════════════════

struct MachineOperand {
    enum class Kind { REG, IMM, MEM, LABEL } kind;
    PhysReg reg;
    int64_t imm;
    int memOffset;
    std::string label;
    
    static MachineOperand reg(PhysReg r) {
        MachineOperand op;
        op.kind = Kind::REG;
        op.reg = r;
        return op;
    }
    
    static MachineOperand imm(int64_t val) {
        MachineOperand op;
        op.kind = Kind::IMM;
        op.imm = val;
        return op;
    }
    
    static MachineOperand mem(int offset) {
        MachineOperand op;
        op.kind = Kind::MEM;
        op.memOffset = offset;
        return op;
    }
    
    static MachineOperand label(const std::string& lbl) {
        MachineOperand op;
        op.kind = Kind::LABEL;
        op.label = lbl;
        return op;
    }
};

struct MachineInstr {
    enum class Op {
        MOV, ADD, SUB, MUL, DIV, MOD,
        CMP, JMP, JE, JNE, JL, JG, JLE, JGE,
        CALL, RET, PUSH, POP,
        LOAD, STORE, SPILL, RELOAD
    } opcode;
    
    MachineOperand dst;
    MachineOperand src1;
    MachineOperand src2;
    
    MachineInstr(Op op) : opcode(op) {}
    MachineInstr(Op op, MachineOperand d, MachineOperand s1) 
        : opcode(op), dst(d), src1(s1) {}
    MachineInstr(Op op, MachineOperand d, MachineOperand s1, MachineOperand s2)
        : opcode(op), dst(d), src1(s1), src2(s2) {}
};

class InstructionSelector {
    const AC_IR::IRProgram& ir;
    LinearScanAllocator& allocator;
    StackFrameManager& frameManager;
    std::vector<MachineInstr> instructions;
    std::map<std::string, int> varToLocal;
    int nextLocal = 0;
    
    PhysReg getPhysReg(const AC_IR::IRRef& ref) {
        if (ref.kind == AC_IR::IRRef::Kind::TEMP) {
            return allocator.getPhysReg(ref.id);
        }
        return PhysReg::RAX;
    }
    
    int getLocalSlot(const std::string& varName) {
        if (varToLocal.find(varName) == varToLocal.end()) {
            varToLocal[varName] = nextLocal++;
        }
        return varToLocal[varName];
    }
    
    MachineOperand selectOperand(const AC_IR::IRRef& ref) {
        using namespace AC_IR;
        
        switch (ref.kind) {
            case IRRef::Kind::CONST:
                if (ref.value.type == IRType::INT) {
                    return MachineOperand::imm(std::get<int>(ref.value.data));
                }
                return MachineOperand::imm(0);
                
            case IRRef::Kind::TEMP: {
                PhysReg reg = allocator.getPhysReg(ref.id);
                if (reg == PhysReg::SPILL) {
                    int slot = allocator.getSpillSlot(ref.id);
                    return MachineOperand::mem(frameManager.getSpillOffset(slot));
                }
                return MachineOperand::reg(reg);
            }
            
            case IRRef::Kind::VAR: {
                std::string varName = ref.toStringWithSymbols(const_cast<SymbolTable*>(&ir.symbols));
                int slot = getLocalSlot(varName);
                return MachineOperand::mem(frameManager.getLocalOffset(slot));
            }
            
            case IRRef::Kind::LABEL:
                return MachineOperand::label("L" + std::to_string(ref.id));
                
            default:
                return MachineOperand::imm(0);
        }
    }
    
    void selectInstruction(const AC_IR::IRInstruction& instr) {
        // Pattern matching on IR opcodes - to be implemented
    }
    
public:
    InstructionSelector(const AC_IR::IRProgram& program, LinearScanAllocator& alloc, StackFrameManager& frame)
        : ir(program), allocator(alloc), frameManager(frame) {}
    
    void selectInstructions(const std::vector<AC_IR::IRInstruction>& irInstrs) {
        for (const auto& instr : irInstrs) {
            selectInstruction(instr);
        }
    }
    
    const std::vector<MachineInstr>& getInstructions() const { return instructions; }
    int getLocalCount() const { return nextLocal; }
};

#ifdef TARGET_LINUX

// ═══════════════════════════════════════════════════════════════════════════
// ELF BINARY WRITER (Multi-Segment)
// ═══════════════════════════════════════════════════════════════════════════

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

class ELFWriter {
    std::vector<uint8_t> textSection;
    std::vector<uint8_t> dataSection;
    std::vector<uint8_t> rodataSection;
    size_t bssSize = 0;
    
    const size_t PAGE_SIZE = 0x1000;
    const uint64_t BASE_ADDR = 0x400000;
    
public:
    void setTextSection(const std::vector<uint8_t>& code) {
        textSection = code;
    }
    
    void setDataSection(const std::vector<uint8_t>& data) {
        dataSection = data;
    }
    
    void setRodataSection(const std::vector<uint8_t>& rodata) {
        rodataSection = rodata;
    }
    
    void setBssSize(size_t size) {
        bssSize = size;
    }
    
    bool write(const std::string& filename) {
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;
        
        // Calculate offsets with proper alignment
        size_t ehdr_size = sizeof(Elf64_Ehdr);
        size_t phdr_size = sizeof(Elf64_Phdr);
        size_t num_segments = 2;  // TEXT + DATA (for now, simplified)
        
        size_t headers_size = ehdr_size + (num_segments * phdr_size);
        size_t text_offset = ((headers_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        size_t data_offset = text_offset + ((textSection.size() + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        
        uint64_t text_vaddr = BASE_ADDR + text_offset;
        uint64_t data_vaddr = BASE_ADDR + data_offset;
        
        // ELF Header
        Elf64_Ehdr ehdr = {};
        ehdr.e_ident[0] = 0x7F;
        ehdr.e_ident[1] = 'E';
        ehdr.e_ident[2] = 'L';
        ehdr.e_ident[3] = 'F';
        ehdr.e_ident[4] = 2;  // 64-bit
        ehdr.e_ident[5] = 1;  // Little endian
        ehdr.e_ident[6] = 1;  // ELF version
        ehdr.e_ident[7] = 0;  // System V ABI
        ehdr.e_type = 2;      // ET_EXEC
        ehdr.e_machine = 0x3E;  // x86-64
        ehdr.e_version = 1;
        ehdr.e_entry = text_vaddr;
        ehdr.e_phoff = ehdr_size;
        ehdr.e_shoff = 0;
        ehdr.e_flags = 0;
        ehdr.e_ehsize = ehdr_size;
        ehdr.e_phentsize = phdr_size;
        ehdr.e_phnum = num_segments;
        ehdr.e_shentsize = 0;
        ehdr.e_shnum = 0;
        ehdr.e_shstrndx = 0;
        
        out.write(reinterpret_cast<const char*>(&ehdr), ehdr_size);
        
        // Program Header: TEXT segment (R+X)
        Elf64_Phdr text_phdr = {};
        text_phdr.p_type = 1;  // PT_LOAD
        text_phdr.p_flags = 5;  // PF_R | PF_X
        text_phdr.p_offset = text_offset;
        text_phdr.p_vaddr = text_vaddr;
        text_phdr.p_paddr = text_vaddr;
        text_phdr.p_filesz = textSection.size();
        text_phdr.p_memsz = textSection.size();
        text_phdr.p_align = PAGE_SIZE;
        
        out.write(reinterpret_cast<const char*>(&text_phdr), phdr_size);
        
        // Program Header: DATA segment (R+W)
        Elf64_Phdr data_phdr = {};
        data_phdr.p_type = 1;  // PT_LOAD
        data_phdr.p_flags = 6;  // PF_R | PF_W
        data_phdr.p_offset = data_offset;
        data_phdr.p_vaddr = data_vaddr;
        data_phdr.p_paddr = data_vaddr;
        data_phdr.p_filesz = dataSection.size();
        data_phdr.p_memsz = dataSection.size() + bssSize;
        data_phdr.p_align = PAGE_SIZE;
        
        out.write(reinterpret_cast<const char*>(&data_phdr), phdr_size);
        
        // Padding to text section
        size_t pad1 = text_offset - headers_size;
        std::vector<uint8_t> padding1(pad1, 0);
        out.write(reinterpret_cast<const char*>(padding1.data()), pad1);
        
        // TEXT section
        out.write(reinterpret_cast<const char*>(textSection.data()), textSection.size());
        
        // Padding to data section
        size_t pad2 = data_offset - (text_offset + textSection.size());
        std::vector<uint8_t> padding2(pad2, 0);
        out.write(reinterpret_cast<const char*>(padding2.data()), pad2);
        
        // DATA section
        out.write(reinterpret_cast<const char*>(dataSection.data()), dataSection.size());
        
        out.close();
        chmod(filename.c_str(), 0755);
        
        return true;
    }
};

bool generateELF(const std::string& filename, const std::vector<uint8_t>& code) {
    ELFWriter writer;
    writer.setTextSection(code);
    return writer.write(filename);
}

#endif

// ═══════════════════════════════════════════════════════════════════════════
// CODE EMITTER (Machine Instructions → Opcodes)
// ═══════════════════════════════════════════════════════════════════════════

class CodeEmitter {
    X64CodeEmitter& emitter;
    
    void emitLoad(PhysReg dst, const MachineOperand& src) {
        if (src.kind == MachineOperand::Kind::IMM) {
            if (src.imm >= INT32_MIN && src.imm <= INT32_MAX) {
                emitter.mov_reg_imm32(dst, src.imm);
            } else {
                emitter.mov_reg_imm64(dst, src.imm);
            }
        } else if (src.kind == MachineOperand::Kind::MEM) {
            emitter.mov_reg_rbp_offset(dst, src.memOffset);
        } else if (src.kind == MachineOperand::Kind::REG) {
            if (dst != src.reg) {
                emitter.mov_reg_reg(dst, src.reg);
            }
        }
    }
    
    void emitStore(const MachineOperand& dst, PhysReg src) {
        if (dst.kind == MachineOperand::Kind::MEM) {
            emitter.mov_rbp_offset_reg(dst.memOffset, src);
        } else if (dst.kind == MachineOperand::Kind::REG) {
            if (dst.reg != src) {
                emitter.mov_reg_reg(dst.reg, src);
            }
        }
    }
    
public:
    CodeEmitter(X64CodeEmitter& e) : emitter(e) {}
    
    void emit(const MachineInstr& instr) {
        using Op = MachineInstr::Op;
        
        switch (instr.opcode) {
            case Op::MOV:
                if (instr.dst.kind == MachineOperand::Kind::REG) {
                    emitLoad(instr.dst.reg, instr.src1);
                } else {
                    PhysReg temp = PhysReg::RAX;
                    emitLoad(temp, instr.src1);
                    emitStore(instr.dst, temp);
                }
                break;
                
            case Op::ADD: {
                PhysReg dst = instr.dst.kind == MachineOperand::Kind::REG ? 
                              instr.dst.reg : PhysReg::RAX;
                emitLoad(dst, instr.src1);
                
                if (instr.src2.kind == MachineOperand::Kind::REG) {
                    emitter.add_reg_reg(dst, instr.src2.reg);
                } else {
                    PhysReg temp = PhysReg::RCX;
                    emitLoad(temp, instr.src2);
                    emitter.add_reg_reg(dst, temp);
                }
                
                if (instr.dst.kind != MachineOperand::Kind::REG) {
                    emitStore(instr.dst, dst);
                }
                break;
            }
            
            case Op::SUB: {
                PhysReg dst = instr.dst.kind == MachineOperand::Kind::REG ? 
                              instr.dst.reg : PhysReg::RAX;
                emitLoad(dst, instr.src1);
                
                if (instr.src2.kind == MachineOperand::Kind::REG) {
                    emitter.sub_reg_reg(dst, instr.src2.reg);
                } else {
                    PhysReg temp = PhysReg::RCX;
                    emitLoad(temp, instr.src2);
                    emitter.sub_reg_reg(dst, temp);
                }
                
                if (instr.dst.kind != MachineOperand::Kind::REG) {
                    emitStore(instr.dst, dst);
                }
                break;
            }
            
            case Op::MUL: {
                PhysReg dst = instr.dst.kind == MachineOperand::Kind::REG ? 
                              instr.dst.reg : PhysReg::RAX;
                emitLoad(dst, instr.src1);
                
                if (instr.src2.kind == MachineOperand::Kind::REG) {
                    emitter.imul_reg_reg(dst, instr.src2.reg);
                } else {
                    PhysReg temp = PhysReg::RCX;
                    emitLoad(temp, instr.src2);
                    emitter.imul_reg_reg(dst, temp);
                }
                
                if (instr.dst.kind != MachineOperand::Kind::REG) {
                    emitStore(instr.dst, dst);
                }
                break;
            }
            
            case Op::CMP: {
                PhysReg left = PhysReg::RAX;
                emitLoad(left, instr.src1);
                
                if (instr.src2.kind == MachineOperand::Kind::REG) {
                    emitter.cmp_reg_reg(left, instr.src2.reg);
                } else {
                    PhysReg right = PhysReg::RCX;
                    emitLoad(right, instr.src2);
                    emitter.cmp_reg_reg(left, right);
                }
                break;
            }
            
            case Op::RET:
                emitter.ret();
                break;
                
            case Op::CALL:
                if (instr.dst.kind == MachineOperand::Kind::LABEL) {
                    emitter.call_rel32(instr.dst.label);
                }
                break;
                
            default:
                break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// IR TO BINARY COMPILER (Complete Pipeline)
// ═══════════════════════════════════════════════════════════════════════════

class IRToBinaryCompiler {
    const AC_IR::IRProgram& ir;
    X64CodeEmitter emitter;
    LinearScanAllocator allocator;
    StackFrameManager frameManager;
    
    void buildLivenessInfo() {
        // Simple liveness: each temp lives from first def to last use
        std::map<int, int> firstUse;
        std::map<int, int> lastUse;
        int pos = 0;
        
        for (const auto& instr : ir.globalInit) {
            for (const auto& op : instr.typedOperands) {
                if (op.kind == AC_IR::IRRef::Kind::TEMP) {
                    if (firstUse.find(op.id) == firstUse.end()) {
                        firstUse[op.id] = pos;
                    }
                    lastUse[op.id] = pos;
                }
            }
            if (instr.result.kind == AC_IR::IRRef::Kind::TEMP) {
                if (firstUse.find(instr.result.id) == firstUse.end()) {
                    firstUse[instr.result.id] = pos;
                }
                lastUse[instr.result.id] = pos;
            }
            pos++;
        }
        
        for (const auto& func : ir.functions) {
            for (const auto& instr : func.instructions) {
                for (const auto& op : instr.typedOperands) {
                    if (op.kind == AC_IR::IRRef::Kind::TEMP) {
                        if (firstUse.find(op.id) == firstUse.end()) {
                            firstUse[op.id] = pos;
                        }
                        lastUse[op.id] = pos;
                    }
                }
                if (instr.result.kind == AC_IR::IRRef::Kind::TEMP) {
                    if (firstUse.find(instr.result.id) == firstUse.end()) {
                        firstUse[instr.result.id] = pos;
                    }
                    lastUse[instr.result.id] = pos;
                }
                pos++;
            }
        }
        
        // Add intervals to allocator
        for (const auto& [vreg, start] : firstUse) {
            allocator.addInterval(vreg, start, lastUse[vreg]);
        }
    }
    
    void compileSimple() {
        // Simplified compilation (no full instruction selection yet)
        // Just emit basic code for testing
        
        emitter.label("_start");
        emitter.push_rbp();
        emitter.mov_rbp_rsp();
        
        int frameSize = frameManager.calculateFrameSize();
        if (frameSize > 0) {
            emitter.sub_rsp_imm32(frameSize);
        }
        
        // Compile global init (simplified)
        for (const auto& instr : ir.globalInit) {
            if (instr.opcode == AC_IR::IROpcode::HALT) {
                emitter.mov_reg_imm32(PhysReg::RAX, 60);  // sys_exit
                emitter.xor_reg_reg(PhysReg::RDI, PhysReg::RDI);  // exit code 0
                emitter.syscall();
            }
        }
        
        if (frameSize > 0) {
            emitter.add_rsp_imm32(frameSize);
        }
        emitter.pop_rbp();
        
        // Fallback exit
        emitter.mov_reg_imm32(PhysReg::RAX, 60);
        emitter.xor_reg_reg(PhysReg::RDI, PhysReg::RDI);
        emitter.syscall();
    }
    
public:
    IRToBinaryCompiler(const AC_IR::IRProgram& program) : ir(program) {}
    
    bool compile(const std::string& outputFile) {
        // Phase 1: Liveness analysis
        buildLivenessInfo();
        
        // Phase 2: Register allocation
        allocator.allocate();
        
        // Phase 3: Calculate frame size
        frameManager.setSpillSize(allocator.getTotalSpillSlots());
        frameManager.calculateFrameSize();
        
        // Phase 4: Code generation
        compileSimple();
        
        // Phase 5: Apply fixups
        emitter.applyFixups();
        
        // Phase 6: Write binary
#ifdef TARGET_LINUX
        return generateELF(outputFile, emitter.getCode());
#else
        return false;
#endif
    }
};

} // namespace AC_BinaryGen

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

bool generateBinaryFromIR(const AC_IR::IRProgram& ir, const std::string& outputFile) {
    AC_BinaryGen::IRToBinaryCompiler compiler(ir);
    return compiler.compile(outputFile);
}
