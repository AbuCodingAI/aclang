#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <algorithm>
#include "type.hpp"

// Forward declaration
struct ASTNode;

// AC Intermediate Representation (IR)
// This IR sits between the AST and code generation, allowing for:
// - Platform-independent optimizations
// - Easier addition of new backends
// - Consistent behavior across all targets

namespace AC_IR {

// Forward declaration
class SymbolTable;

enum class IROpcode {
    // Control flow
    LABEL,
    JUMP,
    JUMP_IF_TRUE,
    JUMP_IF_FALSE,
    CALL,
    RETURN,
    
    // High-level control flow (for languages without goto)
    IF_BEGIN,      // Start of if statement
    IF_ELSE,       // Else clause
    IF_END,        // End of if statement
    WHILE_BEGIN,   // Start of while loop
    WHILE_END,     // End of while loop
    FOR_BEGIN,     // Start of for loop
    FOR_END,       // End of for loop
    
    // Data operations
    LOAD_CONST,
    LOAD_VAR,
    STORE_VAR,
    
    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    
    // Comparison
    EQ,
    NEQ,
    LT,
    GT,
    LTE,
    GTE,
    
    // Logical
    AND,
    OR,
    NOT,
    XOR,   // Boolean XOR: (a!=0) != (b!=0)
    XNOR,  // Boolean XNOR: (a!=0) == (b!=0)
    XSUB,  // Inclusive range count: |a - b| + 1
    
    // Memory
    ALLOC,
    FREE,
    LOAD_INDEX,
    STORE_INDEX,
    
    // I/O
    PRINT,
    INPUT,
    
    // Special
    NOP,
    HALT,
    
    // Function management
    FUNC_BEGIN,
    FUNC_END,
    
    // Event system
    EVENT_BIND,
    EVENT_TRIGGER,
    
    // Library calls
    LIB_CALL,

    // Native built-ins
    EVAL,           // eval(expr) → evaluate string as AC expression

    // Class/bundle support
    CLASS_BEGIN,    // start of a bundle/class definition
    CLASS_END       // end of a bundle/class definition
};

enum class IRType {
    VOID,
    INT,
    FLOAT,
    STRING,
    BOOL,
    LIST,
    TUPLE,
    OBJECT,
    FUNCTION,
    POINTER  // For complex types
};

struct IRValue {
    IRType type;
    Type   acType;   // semantic AC type (from type.hpp); Unknown for non-literal values
    std::variant<int, double, std::string, bool> data;

    IRValue() : type(IRType::VOID) {}
    IRValue(int i)
        : type(IRType::INT),
          acType(Type::Numeral(i >= 0 ? NumeralSubtype::PosInt : NumeralSubtype::NegInt)),
          data(i) {}
    IRValue(double d)
        : type(IRType::FLOAT),
          acType(Type::Numeral(d >= 0.0 ? NumeralSubtype::PosDec : NumeralSubtype::NegDec)),
          data(d) {}
    IRValue(std::string s)
        : type(IRType::STRING), acType(Type::String()), data(std::move(s)) {}
    IRValue(bool b)
        : type(IRType::BOOL), acType(Type::Boolean()), data(b) {}
};

// Integerized IR Reference - uses integer IDs for optimal performance
// Requirements: 2.1, 2.2, 2.3, 2.4, 2.5
struct IRRef {
    enum class Kind { 
        TEMP,      // Temporary value: id = 0, 1, 2, ... (not "t0", "t1")
        VAR,       // Named variable: id = symbol table index (not string name)
        CONST,     // Constant value: embedded in value field
        LABEL,     // Jump label: id = 0, 1, 2, ... (not "L0", "L1")
        FUNCTION,  // Function reference: id = symbol table index
        NONE       // No reference (for instructions without result)
    };
    
    Kind kind;
    int id;              // Integer ID for TEMP (0,1,2...), LABEL (0,1,2...), VAR (symbol index)
    IRValue value;       // For CONST only - embedded constant value
    
    // Default constructor
    IRRef() : kind(Kind::NONE), id(0) {}
    
    // Constructors for convenience
    static IRRef temp(int tempId) { 
        IRRef ref;
        ref.kind = Kind::TEMP;
        ref.id = tempId;
        return ref;
    }
    
    // VAR now uses integer ID (symbol table index) instead of string name
    static IRRef var(int symbolId) { 
        IRRef ref;
        ref.kind = Kind::VAR;
        ref.id = symbolId;
        return ref;
    }
    
    // Legacy: VAR from string name (for backward compatibility during transition)
    // This should be replaced with symbol table lookups
    static IRRef varLegacy(const std::string& varName);
    
    static IRRef constant(const IRValue& val) { 
        IRRef ref;
        ref.kind = Kind::CONST;
        ref.value = val;
        return ref;
    }
    
    static IRRef label(int labelId) { 
        IRRef ref;
        ref.kind = Kind::LABEL;
        ref.id = labelId;
        return ref;
    }
    
    // FUNCTION now uses integer ID (symbol table index) instead of string name
    static IRRef func(int symbolId) { 
        IRRef ref;
        ref.kind = Kind::FUNCTION;
        ref.id = symbolId;
        return ref;
    }
    
    // Legacy: FUNCTION from string name (for backward compatibility during transition)
    static IRRef funcLegacy(const std::string& funcName);
    
    static IRRef none() {
        return IRRef();
    }
    
    // Helper to check if this is a valid reference
    bool isValid() const { return kind != Kind::NONE; }
    
    // Helper to get string representation (for debugging only)
    // String conversion only happens here - all internal operations use integers
    std::string toString() const {
        switch (kind) {
            case Kind::TEMP: return "t" + std::to_string(id);
            case Kind::VAR: return "v" + std::to_string(id);  // Show as v0, v1, v2... (symbol index)
            case Kind::CONST: {
                if (value.type == IRType::INT) return std::to_string(std::get<int>(value.data));
                if (value.type == IRType::FLOAT) return std::to_string(std::get<double>(value.data));
                if (value.type == IRType::STRING) return "\"" + std::get<std::string>(value.data) + "\"";
                if (value.type == IRType::BOOL) return std::get<bool>(value.data) ? "true" : "false";
                return "const";
            }
            case Kind::LABEL: return "L" + std::to_string(id);
            case Kind::FUNCTION: return "f" + std::to_string(id);  // Show as f0, f1, f2... (symbol index)
            case Kind::NONE: return "<none>";
        }
        return "<unknown>";
    }
    
    // Helper to get string representation with symbol table (for better debugging)
    std::string toStringWithSymbols(SymbolTable* symbols) const;
};

struct IRInstruction {
    IROpcode opcode;
    
    // OLD: String-based operands (kept for backward compatibility)
    std::vector<std::string> operands;
    IRValue value;
    
    // NEW: Typed operands and result
    IRRef result;                      // Where result goes (optional)
    std::vector<IRRef> typedOperands;  // Typed operands
    
    std::string comment;
    int lineNumber = 0;
    
    // Old constructors (backward compatible)
    IRInstruction(IROpcode op) : opcode(op) {}
    IRInstruction(IROpcode op, std::vector<std::string> ops) 
        : opcode(op), operands(std::move(ops)) {}
    IRInstruction(IROpcode op, std::vector<std::string> ops, IRValue val)
        : opcode(op), operands(std::move(ops)), value(std::move(val)) {}
    
    // NEW constructors
    IRInstruction(IROpcode op, IRRef res, std::vector<IRRef> ops)
        : opcode(op), result(res), typedOperands(std::move(ops)) {}
    IRInstruction(IROpcode op, std::vector<IRRef> ops)
        : opcode(op), typedOperands(std::move(ops)) {}
};

// Symbol Table with Integer Indexing
// Requirements: 15.1, 15.2, 15.3, 15.4, 15.5
// Maps variable names to sequential integer indices for O(1) lookup
class SymbolTable {
public:
    struct Symbol {
        std::string name;
        IRType type;
        int scope;  // Scope depth (0 = global, 1+ = nested)
    };
    
private:
    std::vector<Symbol> symbols;                           // Index → Symbol (sequential 0,1,2...)
    std::unordered_map<std::string, std::vector<int>> nameToIds;  // Name → [indices] (for scoping)
    int currentScope;
    
public:
    SymbolTable() : currentScope(0) {}
    
    // Intern a symbol: add to table and return its integer index
    // If symbol already exists in current scope, return existing index
    int intern(const std::string& name, IRType type = IRType::VOID) {
        // Check if symbol exists in current scope
        auto it = nameToIds.find(name);
        if (it != nameToIds.end()) {
            for (int id : it->second) {
                if (symbols[id].scope == currentScope) {
                    return id;  // Already exists in current scope
                }
            }
        }
        
        // Add new symbol
        int id = static_cast<int>(symbols.size());
        symbols.push_back({name, type, currentScope});
        nameToIds[name].push_back(id);
        return id;
    }
    
    // Lookup symbol by name: returns index or -1 if not found
    // Searches from current scope upward to global scope
    int lookup(const std::string& name) const {
        auto it = nameToIds.find(name);
        if (it == nameToIds.end()) {
            return -1;  // Not found
        }
        
        // Search from current scope upward
        for (int scope = currentScope; scope >= 0; --scope) {
            for (int id : it->second) {
                if (symbols[id].scope == scope) {
                    return id;
                }
            }
        }
        
        return -1;  // Not found
    }
    
    // Get symbol name by index
    const std::string& getName(int id) const {
        if (id >= 0 && id < static_cast<int>(symbols.size())) {
            return symbols[id].name;
        }
        static const std::string empty;
        return empty;
    }
    
    // Get symbol by index
    const Symbol& getSymbol(int id) const {
        return symbols[id];
    }
    
    // Enter a new scope (increment scope depth)
    void enterScope() {
        currentScope++;
    }
    
    // Exit current scope (decrement scope depth, remove symbols from exited scope)
    void exitScope() {
        if (currentScope > 0) {
            // Remove symbols from current scope
            for (auto& pair : nameToIds) {
                auto& ids = pair.second;
                ids.erase(
                    std::remove_if(ids.begin(), ids.end(),
                        [this](int id) { return symbols[id].scope == currentScope; }),
                    ids.end()
                );
            }
            currentScope--;
        }
    }
    
    // Get current scope depth
    int getCurrentScope() const {
        return currentScope;
    }
    
    // Get total number of symbols
    size_t size() const {
        return symbols.size();
    }
    
    // Clear all symbols
    void clear() {
        symbols.clear();
        nameToIds.clear();
        currentScope = 0;
    }
};

// Arena Allocator for IR Memory Management
// Requirements: 3.2, 3.4, 3.5
// Allocates memory in large blocks (64KB default) to eliminate per-instruction allocation overhead
class Arena {
public:
    struct Block {
        std::unique_ptr<char[]> data;
        size_t size;
        size_t used;
        
        Block(size_t sz) : data(new char[sz]), size(sz), used(0) {}
    };
    
private:
    std::vector<Block> blocks;
    size_t blockSize;  // Default: 64KB
    
public:
    // Constructor with configurable block size (default 64KB)
    explicit Arena(size_t blockSz = 65536) : blockSize(blockSz) {
        // Pre-allocate first block
        blocks.emplace_back(blockSize);
    }
    
    // Allocate memory from arena with alignment
    void* allocate(size_t size, size_t alignment = 8) {
        // Align size to alignment boundary
        size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
        
        // Try to allocate from current block
        if (!blocks.empty()) {
            Block& current = blocks.back();
            size_t alignedUsed = (current.used + alignment - 1) & ~(alignment - 1);
            
            if (alignedUsed + alignedSize <= current.size) {
                void* ptr = current.data.get() + alignedUsed;
                current.used = alignedUsed + alignedSize;
                return ptr;
            }
        }
        
        // Need new block
        size_t newBlockSize = std::max(blockSize, alignedSize);
        blocks.emplace_back(newBlockSize);
        Block& newBlock = blocks.back();
        void* ptr = newBlock.data.get();
        newBlock.used = alignedSize;
        return ptr;
    }
    
    // Allocate typed object (placement new)
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    // Reset arena (keep blocks, reset used counters)
    void reset() {
        for (auto& block : blocks) {
            block.used = 0;
        }
    }
    
    // Clear all blocks (free memory)
    void clear() {
        blocks.clear();
        // Re-allocate first block
        blocks.emplace_back(blockSize);
    }
    
    // Get total allocated memory
    size_t totalAllocated() const {
        size_t total = 0;
        for (const auto& block : blocks) {
            total += block.size;
        }
        return total;
    }
    
    // Get total used memory
    size_t totalUsed() const {
        size_t total = 0;
        for (const auto& block : blocks) {
            total += block.used;
        }
        return total;
    }
    
    // Get number of blocks
    size_t numBlocks() const {
        return blocks.size();
    }
};

struct IRFunction {
    std::string name;
    std::string classOwner;  // non-empty if this is a method of a bundle (e.g. "Dog")
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    IRType returnType;

    // NEW: Counters for generating unique IDs
    int tempCount = 0;
    int labelCount = 0;

    IRFunction(std::string n, std::string owner = "")
        : name(std::move(n)), classOwner(std::move(owner)), returnType(IRType::VOID) {}
    
    // Helper to allocate a new temp
    IRRef newTemp() {
        return IRRef::temp(tempCount++);
    }
    
    // Helper to allocate a new label
    IRRef newLabel() {
        return IRRef::label(labelCount++);
    }
};

struct IRProgram {
    // NEW: Symbol table and arena allocator for integerized IR
    SymbolTable symbols;                    // Shared symbol table for all functions
    Arena arena;                            // Arena allocator for IR memory

    std::vector<IRFunction> functions;      // All functions — "definitions" section
    std::vector<IRInstruction> globalInit;  // data + main combined (codegen compat)
    std::vector<IRInstruction> dataSection; // imports, global consts — cache section
    std::vector<IRInstruction> mainSection; // main loop body — cache section
    std::string backend;                    // Target backend (PY, JS, RS, etc.)
    std::string target;                     // Same as backend (for compatibility)
    bool useHighLevelIR = false;            // Use structured control flow instead of jumps

    // Global counters for temporaries and labels
    int globalTempCount = 0;
    int globalLabelCount = 0;
    
    IRProgram() = default;
    
    // Helper to find function
    IRFunction* findFunction(const std::string& name) {
        for (auto& func : functions) {
            if (func.name == name) return &func;
        }
        return nullptr;
    }
};

// Free functions
IRProgram generateIR(const ASTNode& ast, const std::string& backend);
std::string generateIRText(const IRProgram& program);

} // namespace AC_IR
