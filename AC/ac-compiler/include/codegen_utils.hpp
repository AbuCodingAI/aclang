#pragma once
#include "../include/ast.hpp"
#include "../include/type.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <functional>

// Base class for all code generators - eliminates duplication
class CodegenUtils {
protected:
    std::ostringstream out;
    int indentLevel = 0;
    std::map<std::string, TypePtr> varTypes;

    // Common utility methods used by all backends
    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    
    void emitRaw(const std::string& line) { 
        out << line << "\n"; 
    }

    // String handling utilities - shared across all backends
    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return getQuoteChar() + v.substr(1, v.size() - 2) + getQuoteChar();
        return v;
    }

    std::string unwrapDollars(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    result += getQuoteChar() + s.substr(i + 1, end - i - 1) + getQuoteChar();
                    i = end;
                    continue;
                }
            }
            result += s[i];
        }
        return result;
    }

    // Common condition translation logic
    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);
        
        // Normalize spaced dots
        while (r.find(" . ") != std::string::npos) r.replace(r.find(" . "), 3, ".");
        while (r.find(". ") != std::string::npos) r.replace(r.find(". "), 2, ".");
        while (r.find(" .") != std::string::npos) r.replace(r.find(" ."), 2, ".");

        // Handle overlap patterns
        size_t overlapPos;
        if ((overlapPos = r.find("Hitbox.Coords Overlap")) != std::string::npos) {
            std::string left = r.substr(0, overlapPos);
            if (!left.empty() && left.back() == ' ') left.pop_back();
            std::string right = r.substr(overlapPos + strlen("Hitbox.Coords Overlap"));
            while (!right.empty() && right.front() == ' ') right.erase(0, 1);
            r = left + ".overlaps(" + right + ")";
        }

        // CircleFell pattern
        if (r.find("CircleFell") != std::string::npos) {
            size_t atPos = r.find("CircleFell");
            std::string prefix = r.substr(0, atPos);
            if (prefix.find("Cactus") != std::string::npos) {
                r = "cactusPhysics.circleFell()";
            }
        }

        // Hitbox overlap fallback
        if (r.find("hitbox overlap") != std::string::npos) {
            r = getTrueValue();
        }

        // Not found handling
        if (r.find("not found") != std::string::npos) {
            if (r.find("AC.Search") != std::string::npos) {
                r = r.substr(0, r.find(" not found"));
            } else {
                r = getTrueValue();
            }
        }

        // Operator translations
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, getNotEqualOperator()), p += 2;

        // Equality translations
        translateEqualityPatterns(r);

        return r;
    }

    // Generate range validation code
    std::string generateRangeValidation(const std::string& n) {
        return getRangeCheckTemplate(n);
    }

    std::string generateSequenceValidation(const std::string& x, const std::string& y) {
        return getSequenceCheckTemplate(x, y);
    }

public:
    virtual ~CodegenUtils() = default;
    
    // Pure virtual methods that each backend must implement
    virtual std::string getQuoteChar() const = 0;
    virtual std::string getTrueValue() const = 0;
    virtual std::string getNotEqualOperator() const = 0;
    virtual std::string getRangeCheckTemplate(const std::string& n) const = 0;
    virtual std::string getSequenceCheckTemplate(const std::string& x, const std::string& y) const = 0;
    virtual void translateEqualityPatterns(std::string& r) const = 0;
    
    // Common interface
    virtual std::string generate(const ASTNode& node) = 0;
    virtual void genNode(const ASTNode& node) = 0;
    
    // Helper for common patterns
    std::string getResult() const { return out.str(); }
    void setIndent(int level) { indentLevel = level; }
    void increaseIndent() { indentLevel++; }
    void decreaseIndent() { indentLevel--; }
};
