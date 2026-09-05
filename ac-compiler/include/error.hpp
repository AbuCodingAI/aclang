#pragma once
#include <string>
#include <stdexcept>
#include <sstream>
#include <string_view>

// AC Error Handling - Consolidated to "Preposterous" format only
// All errors should use: Preposterous: [type] [message] at line [line] character [col]

enum class ErrorType {
    Syntax,         // Syntax errors in AC code
    Runtime,        // Runtime errors in generated code
    File,          // File I/O errors
    Backend,       // Backend/compiler errors
    Type,          // Type checking errors
    Semantic       // Semantic errors
};

class ACError : public std::runtime_error {
public:
    static ACError syntax(const std::string& message, int line, int col = 0) {
        return createError(ErrorType::Syntax, message, line, col);
    }
    
    static ACError runtime(const std::string& message) {
        return createError(ErrorType::Runtime, message, 0, 0);
    }
    
    static ACError file(const std::string& operation, const std::string& path) {
        std::ostringstream ss;
        ss << "Cannot " << operation << " file: " << path;
        return createError(ErrorType::File, ss.str(), 0, 0);
    }
    
    static ACError backend(const std::string& message) {
        return createError(ErrorType::Backend, message, 0, 0);
    }
    
    static ACError type(const std::string& message, int line = 0, int col = 0) {
        return createError(ErrorType::Type, message, line, col);
    }
    
    static ACError semantic(const std::string& message, int line = 0, int col = 0) {
        return createError(ErrorType::Semantic, message, line, col);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Named diagnostics — AC's error voice, all in one place.
    // Throw sites call these instead of embedding message strings, so the whole
    // vocabulary (roasts included) lives in exactly one file. Edit a message here
    // and it changes everywhere it's thrown.
    // ─────────────────────────────────────────────────────────────────────────

    // Roasts keep their own prefix (Toxic:) — no "Preposterous: <Type>" wrapper.
    static ACError fluencyInCPU() {          // foreign { … } block targeting the native (BNY) backend
        return ACError("Toxic: User attempts fluency in CPU");
    }

    // Type / semantic / backend compiler errors (routed through the standard formatter).
    static ACError nonNumericArith(const std::string& op) {
        return type("'" + op + "' requires numeric operands (strings are not numbers)");
    }
    static ACError unknownBinaryOp(const std::string& op) {
        return backend("unknown binary operator '" + op + "'");
    }
    static ACError unknownUnaryOp(const std::string& op) {
        return backend("unknown unary operator '" + op + "'");
    }
    static ACError lengthNeedsArg() {
        return backend("length() requires an argument");
    }
    static ACError methodNotFound(const std::string& method) {
        return backend(method + " does not exist (use Term.display)");
    }
    static ACError tryMissingCatch() {
        return backend("try block missing catch clause");
    }
    static ACError foreignDisabled() {
        return backend("Foreign blocks are disabled. Recompile with --allow-foreign "
                       "to enable raw code passthrough.");
    }
    static ACError conglomerNativeOnly(const std::string& lib, const std::string& backendName) {
        return backend("`conglomer " + lib + "` links a native C header — only the C/C++ "
                       "backends can honor it (target here is " + backendName + "). Wrap it as "
                       "an ilib/flib for portable use, or compile to C/C++.");
    }
    static ACError undefinedLabel(const std::string& name, long long offset) {
        return backend("BNY: undefined label '" + name + "' referenced at offset "
                       + std::to_string(offset));
    }
    static ACError unknownBackend(const std::string& name) {  // AC->Xyzzy where Xyzzy isn't a backend
        return ACError("Preposterous: I don't speak " + name);
    }
    // A tuple with no `; any` / `; TYPE` annotation must be homogeneous (elements sharing one
    // type, mixed int/dec widening to dec) — anything else is rejected at compile time rather
    // than silently building a broken/mistyped shape.
    static ACError tupleNotHomogeneous(const std::string& typeA, const std::string& typeB) {
        return type("tuple elements have mismatched types (" + typeA + " and " + typeB +
                    ") — wrap the tuple as `(...; any)` to allow mixed types (constant index "
                    "only), or `(...; TYPE)` to convert every element to one type");
    }
    // `(elems; TYPE)` — a "colloid" tuple: every element is coerced to TYPE before IR is
    // generated. A literal element that provably can't convert (e.g. `$hello$` to int) is
    // rejected immediately at compile time, not deferred to a runtime failure.
    static ACError tupleColloidConversionFailed(const std::string& literal, const std::string& targetType) {
        return type("tuple element " + literal + " cannot convert to " + targetType);
    }
    // Staged for math.MemInt (planned arbitrary-precision int, 64 MB/value cap): a value would blow
    // past the ceiling and starve the system → catchable error. Wire at the MemInt alloc guard.
    static ACError memIntRagequit() {
        return ACError("Preposterous: The system has unexpectedly ragequit, please try a smaller number");
    }

    // Internal control-flow signal (caught by the parse loop; not shown to the user).
    static ACError tooManyParseErrors() {
        return ACError("Too many parse errors");
    }

private:
    ACError(const std::string& msg) : std::runtime_error(msg) {}
    
    static ACError createError(ErrorType type, const std::string& message, int line, int col) {
        std::ostringstream ss;
        ss << "Preposterous: ";
        
        switch (type) {
            case ErrorType::Syntax:
                ss << "SyntaxError (It's all Greek to me)";
                break;
            case ErrorType::Runtime:
                ss << "RuntimeError";
                break;
            case ErrorType::File:
                ss << "FileError";
                break;
            case ErrorType::Backend:
                ss << "BackendError";
                break;
            case ErrorType::Type:
                ss << "TypeError";
                break;
            case ErrorType::Semantic:
                ss << "SemanticError";
                break;
        }
        
        ss << " at line " << line << " char " << col << ": " << message;
        
        return ACError(ss.str());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Toxic — the non-fatal roasts (printed to stderr, not thrown). Kept here so AC's
// whole error voice lives in one file. These return std::string (warnings are
// printed): `std::cerr << Toxic::usedC() << "\n";`
// ─────────────────────────────────────────────────────────────────────────────
namespace Toxic {
    inline std::string usedC() {
        return "Toxic: User used C, not effective";
    }
    inline std::string slackingOffUnread(const std::string& name) {
        return "Toxic: Compiler is slacking off — '" + name + "' assigned but never read";
    }
    inline std::string slackingOffUncalled(const std::string& name) {
        return "Toxic: Compiler is slacking off — '" + name + "' defined but never called";
    }
    inline std::string impossibleOperations() {
        return "Toxic: What did WE do to you for this math?";
    }
    inline std::string threeBusinessDays() {
        return "Toxic: Compiler requested 3 business days to finish this math";
    }
    inline std::string outputIgnoredWithAll() {
        return "Toxic: --output is ignored with --all — one name can't hold every backend";
    }
    // Downstream toolchain failures (AC emitted the code; the host compiler rejected it).
    inline std::string gccChoked(int rc) {
        return "Toxic: gcc choked on the generated C (exit " + std::to_string(rc) + ")";
    }
    inline std::string gxxChoked(int rc) {
        return "Toxic: g++ choked on the generated C++ (exit " + std::to_string(rc) + ")";
    }
    inline std::string libBuildFellOver(int rc) {
        return "Toxic: the LIB build fell over (exit " + std::to_string(rc) + ")";
    }
    inline std::string rustcOpinions(int rc) {
        return "Toxic: rustc had opinions about the generated Rust (exit " + std::to_string(rc) + ")";
    }
    inline std::string javacNotHavingIt(int rc) {
        return "Toxic: javac wasn't having it (exit " + std::to_string(rc) + ")";
    }
    // -supercalifragilisticexpialidocious: a line the parser can't make sense of is
    // dropped and compilation continues, instead of a Preposterous syntax error + abort.
    inline std::string confusedToo() {
        return "Toxic: The compiler is confused as well, get back to the tutorials";
    }
}

// Convenience macros for common error patterns
#define SYNTAX_ERROR(msg, line, col) ACError::syntax(msg, line, col)
#define RUNTIME_ERROR(msg) ACError::runtime(msg)
#define FILE_ERROR(op, path) ACError::file(op, path)
#define BACKEND_ERROR(msg) ACError::backend(msg)
#define TYPE_ERROR(msg, line, col) ACError::type(msg, line, col)
#define SEMANTIC_ERROR(msg, line, col) ACError::semantic(msg, line, col)
