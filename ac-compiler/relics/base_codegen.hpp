#pragma once
#include "../include/ast.hpp"
#include "../include/type.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <functional>
#include <map>

class BaseCodeGen {
protected:
    std::ostringstream out;
    std::ostringstream functions;
    int indentLevel = 0;
    bool collectingFunctions = false;
    std::map<std::string, TypePtr> varTypes;  // Track variable types

    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }

    void emitRaw(const std::string& line) {
        out << line << "\n";
    }

    void indent() {
        indentLevel++;
    }

    void dedent() {
        indentLevel--;
    }

    std::string quoteIfString(const std::string& s) {
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
            return "\"" + s.substr(1, s.size() - 2) + "\"";
        return s;
    }

    std::string unwrapDollars(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    result += "\"" + s.substr(i + 1, end - i - 1) + "\"";
                    i = end;
                    continue;
                }
            } else {
                result += s[i];
            }
        }
        return result;
    }

    // Common helper functions
    std::string translateCondition(const std::string& cond);
    std::string translateExpr(const std::string& expr);
    
    // Pure virtual function - each codegen implements differently
    virtual std::string generate(const ASTNode& node) = 0;
};
