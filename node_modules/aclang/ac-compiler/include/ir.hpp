#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

// Forward declaration
struct ASTNode;

// AC Intermediate Representation (IR)
// This IR sits between the AST and code generation, allowing for:
// - Platform-independent optimizations
// - Easier addition of new backends
// - Consistent behavior across all targets

namespace AC_IR {

enum class IROpcode {
    // Control flow
    LABEL,
    JUMP,
    JUMP_IF_TRUE,
    JUMP_IF_FALSE,
    CALL,
    RETURN,
    
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
    LIB_CALL
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
    std::variant<int, double, std::string, bool> data;
    
    IRValue() : type(IRType::VOID) {}
    IRValue(int i) : type(IRType::INT), data(i) {}
    IRValue(double d) : type(IRType::FLOAT), data(d) {}
    IRValue(std::string s) : type(IRType::STRING), data(std::move(s)) {}
    IRValue(bool b) : type(IRType::BOOL), data(b) {}
};

// NEW: Typed reference to values/variables/temps
struct IRRef {
    enum class Kind { 
        TEMP,      // Temporary value (t0, t1, etc.)
        VAR,       // Named variable
        CONST,     // Constant value
        LABEL,     // Jump label
        FUNCTION,  // Function reference
        NONE       // No reference (for instructions without result)
    };
    
    Kind kind;
    int id;              // For TEMP (t0, t1), LABEL (L0, L1)
    std::string name;    // For VAR, FUNCTION
    IRValue value;       // For CONST
    
    // Default constructor
    IRRef() : kind(Kind::NONE), id(0) {}
    
    // Constructors for convenience
    static IRRef temp(int tempId) { 
        IRRef ref;
        ref.kind = Kind::TEMP;
        ref.id = tempId;
        return ref;
    }
    
    static IRRef var(const std::string& varName) { 
        IRRef ref;
        ref.kind = Kind::VAR;
        ref.name = varName;
        return ref;
    }
    
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
    
    static IRRef func(const std::string& funcName) { 
        IRRef ref;
        ref.kind = Kind::FUNCTION;
        ref.name = funcName;
        return ref;
    }
    
    static IRRef none() {
        return IRRef();
    }
    
    // Helper to check if this is a valid reference
    bool isValid() const { return kind != Kind::NONE; }
    
    // Helper to get string representation
    std::string toString() const {
        switch (kind) {
            case Kind::TEMP: return "t" + std::to_string(id);
            case Kind::VAR: return name;
            case Kind::CONST: {
                if (value.type == IRType::INT) return std::to_string(std::get<int>(value.data));
                if (value.type == IRType::FLOAT) return std::to_string(std::get<double>(value.data));
                if (value.type == IRType::STRING) return "\"" + std::get<std::string>(value.data) + "\"";
                if (value.type == IRType::BOOL) return std::get<bool>(value.data) ? "true" : "false";
                return "const";
            }
            case Kind::LABEL: return "L" + std::to_string(id);
            case Kind::FUNCTION: return name;
            case Kind::NONE: return "<none>";
        }
        return "<unknown>";
    }
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

struct IRFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    IRType returnType;
    
    // NEW: Counters for generating unique IDs
    int tempCount = 0;
    int labelCount = 0;
    
    IRFunction(std::string n) : name(std::move(n)), returnType(IRType::VOID) {}
    
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
    std::vector<IRFunction> functions;
    std::vector<IRInstruction> globalInit;
    std::string backend;  // Target backend (PY, JS, RS, etc.)
    std::string target;   // Same as backend (for compatibility)
    
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
