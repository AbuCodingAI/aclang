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

private:
    ACError(const std::string& msg) : std::runtime_error(msg) {}
    
    static ACError createError(ErrorType type, const std::string& message, int line, int col) {
        std::ostringstream ss;
        ss << "Preposterous: ";
        
        switch (type) {
            case ErrorType::Syntax:
                ss << "Syntax Error";
                break;
            case ErrorType::Runtime:
                ss << "Runtime Error";
                break;
            case ErrorType::File:
                ss << "File Error";
                break;
            case ErrorType::Backend:
                ss << "Backend Error";
                break;
            case ErrorType::Type:
                ss << "Type Error";
                break;
            case ErrorType::Semantic:
                ss << "Semantic Error";
                break;
        }
        
        ss << ": " << message;
        
        if (line > 0) {
            ss << " at line " << line;
            if (col > 0) ss << " character " << col;
        }
        
        return ACError(ss.str());
    }
};

// Convenience macros for common error patterns
#define SYNTAX_ERROR(msg, line, col) ACError::syntax(msg, line, col)
#define RUNTIME_ERROR(msg) ACError::runtime(msg)
#define FILE_ERROR(op, path) ACError::file(op, path)
#define BACKEND_ERROR(msg) ACError::backend(msg)
#define TYPE_ERROR(msg, line, col) ACError::type(msg, line, col)
#define SEMANTIC_ERROR(msg, line, col) ACError::semantic(msg, line, col)
