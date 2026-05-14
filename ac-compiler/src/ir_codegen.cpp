#include "../include/ac.hpp"
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>

using namespace AC_IR;

// ─── shared helpers ──────────────────────────────────────────────────────────

static bool looksNumeric(const std::string &s)
{
    if (s.empty())
        return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    return i < s.size() && (std::isdigit(s[i]) || s[i] == '.');
}
static bool looksString(const std::string &s)
{
    return s.size() >= 2 && s.front() == '"' && s.back() == '"';
}
// Returns true only for simple float literals like "3.14" or "-1.5" (no operators/spaces)
static bool looksFloat(const std::string &s)
{
    if (s.empty())
        return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    bool hasDot = false;
    for (; i < s.size(); i++)
    {
        if (s[i] == '.')
            hasDot = true;
        else if (!std::isdigit((unsigned char)s[i]))
            return false;
    }
    return hasDot;
}
// Format a double without trailing zeros; always include a decimal point
static std::string fmtDouble(double d)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10g", d);
    std::string s(buf);
    if (s.find('.') == std::string::npos &&
        s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos)
        s += ".0";
    return s;
}
static std::string stripQuotes(const std::string &s)
{
    return looksString(s) ? s.substr(1, s.size() - 2) : s;
}

// Reformat "p1, p2" → "TYPE p1, TYPE p2"
static std::string typedParams(const std::string &params, const std::string &typeName)
{
    if (params.empty())
        return "";
    std::istringstream ss(params);
    std::string result, token;
    bool first = true;
    while (std::getline(ss, token, ','))
    {
        size_t a = token.find_first_not_of(' ');
        size_t b = token.find_last_not_of(' ');
        std::string name = (a == std::string::npos) ? "" : token.substr(a, b - a + 1);
        if (!first)
            result += ", ";
        result += typeName + " " + name;
        first = false;
    }
    return result;
}

// Reformat "p1, p2" → "p1 TYPE, p2 TYPE"  (Go style: no colon)
static std::string goTypedParams(const std::string &params, const std::string &typeName)
{
    if (params.empty())
        return "";
    std::istringstream ss(params);
    std::string result, token;
    bool first = true;
    while (std::getline(ss, token, ','))
    {
        size_t a = token.find_first_not_of(' ');
        size_t b = token.find_last_not_of(' ');
        std::string name = (a == std::string::npos) ? "" : token.substr(a, b - a + 1);
        if (!first)
            result += ", ";
        result += name + " " + typeName;
        first = false;
    }
    return result;
}

// Reformat "p1, p2" → "p1: TYPE, p2: TYPE"  (Rust / V style)
static std::string suffixTypedParams(const std::string &params, const std::string &typeName)
{
    if (params.empty())
        return "";
    std::istringstream ss(params);
    std::string result, token;
    bool first = true;
    while (std::getline(ss, token, ','))
    {
        size_t a = token.find_first_not_of(' ');
        size_t b = token.find_last_not_of(' ');
        std::string name = (a == std::string::npos) ? "" : token.substr(a, b - a + 1);
        if (!first)
            result += ", ";
        result += name + ": " + typeName;
        first = false;
    }
    return result;
}

// Common ref formatter (all backends except ASM share this)
static std::string commonRef(const IRRef &r, SymbolTable *sym,
                             const std::string &trueVal = "true",
                             const std::string &falseVal = "false")
{
    if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::FUNCTION)
        return r.toStringWithSymbols(sym);
    if (r.kind == IRRef::Kind::TEMP)
        return "t" + std::to_string(r.id);
    if (r.kind == IRRef::Kind::LABEL)
        return "L" + std::to_string(r.id);
    if (r.kind == IRRef::Kind::CONST)
    {
        if (r.value.type == IRType::INT)
            return std::to_string(std::get<int>(r.value.data));
        if (r.value.type == IRType::FLOAT)
            return fmtDouble(std::get<double>(r.value.data));
        if (r.value.type == IRType::BOOL)
            return std::get<bool>(r.value.data) ? trueVal : falseVal;
        if (r.value.type == IRType::STRING)
        {
            std::string s = std::get<std::string>(r.value.data);
            if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
                s = s.substr(1, s.size() - 2);
            return "\"" + s + "\"";
        }
    }
    return "0";
}

// ─── interface ───────────────────────────────────────────────────────────────

class BackendStrategy
{
public:
    virtual ~BackendStrategy() = default;

    virtual void emit(std::ostringstream &out, int indent, const std::string &line) = 0;
    virtual void emitRaw(std::ostringstream &out, const std::string &line) = 0;
    virtual void emitHeader(std::ostringstream &out) = 0;
    virtual void emitFooter(std::ostringstream &out) {}
    // Called before emitHeader so strategies can adapt to the output filename stem
    virtual void setOutputStem(const std::string &) {}

    virtual std::string formatRef(const IRRef &r, SymbolTable *sym) = 0;

    virtual void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) = 0;
    virtual void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                              const std::string &lhs, const std::string &rhs, char op) = 0;
    virtual void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                                const std::string &lhs, const std::string &rhs, const std::string &op) = 0;
    virtual void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                          const std::string &func, const std::string &args) = 0;
    virtual void emitReturn(std::ostringstream &out, int &indent, const std::string &val) = 0;
    virtual void emitPrint(std::ostringstream &out, int &indent, const std::string &val) = 0;
    virtual void emitHalt(std::ostringstream &out, int &indent) = 0;
    virtual void emitForeign(std::ostringstream &out, int &indent, const std::string &code)
    {
        emitRaw(out, stripQuotes(code));
    }

    virtual void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) = 0;
    virtual void emitIfElse(std::ostringstream &out, int &indent) = 0;
    virtual void emitIfEnd(std::ostringstream &out, int &indent) = 0;
    virtual void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &cond) {}
    virtual void emitWhileEnd(std::ostringstream &out, int &indent) {}
    virtual void emitForBegin(std::ostringstream &out, int &indent,
                              const std::string &iterVar, const std::string &collection) {}
    virtual void emitForEnd(std::ostringstream &out, int &indent) {}
    virtual void emitAlloc(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &allocType,
                           const std::string &content) {}

    virtual void emitLabel(std::ostringstream &out, int indent, const std::string &label) = 0;
    virtual void emitJump(std::ostringstream &out, int indent, const std::string &label) = 0;
    virtual void emitJumpIfFalse(std::ostringstream &out, int indent,
                                 const std::string &cond, const std::string &label) = 0;
    virtual void emitJumpIfTrue(std::ostringstream &out, int indent,
                                const std::string &cond, const std::string &label) {}

    virtual void emitFunctionBegin(std::ostringstream &out, int &indent,
                                   const std::string &name, const std::string &params) = 0;
    virtual void emitFunctionEnd(std::ostringstream &out, int &indent) = 0;
    virtual void emitMainBegin(std::ostringstream &out, int &indent) = 0;
    virtual void emitMainEnd(std::ostringstream &out, int &indent) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// 1. PYTHON  (AC->PY)
// ═══════════════════════════════════════════════════════════════════════════

class PythonStrategy : public BackendStrategy
{
    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "# Generated by AC Compiler (AC->PY)");
        emitRaw(out, "import sys\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "True", "False");
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, var + " = " + val);
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        emit(out, indent, res + " = " + lhs + " " + op + " " + rhs);
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "int(bool(" + lhs + ") ^ bool(" + rhs + "))";
        else if (op == "xnor")
            expr = "int(bool(" + lhs + ") == bool(" + rhs + "))";
        else if (op == "xsub")
            expr = "abs(" + lhs + " - " + rhs + ") + 1";
        else if (op == "not")
            expr = "int(not bool(" + lhs + "))";
        else
            expr = "int(" + lhs + " " + op + " " + rhs + ")";
        emit(out, indent, res + " = " + expr);
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call : res + " = " + call);
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return" : "return " + val);
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "print(" + val + ")");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "sys.exit(0)");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "if " + cond + ":");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "else:");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override { indent--; }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "while True:");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override { indent--; }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for " + iterVar + " in " + collection + ":");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override { indent--; }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, var + " = [" + content + "]");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        emit(out, 0, "# " + label + ":");
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue");
        else
            emit(out, indent, "# goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if not (" + cond + "): break");
        else
            emit(out, indent, "if not (" + cond + "): pass  # goto " + label);
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        emit(out, indent, "def " + name + "(" + params + "):");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emitRaw(out, "");
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "if __name__ == \"__main__\":");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override { indent--; }
};

// ═══════════════════════════════════════════════════════════════════════════
// 2. JAVASCRIPT  (AC->JS)
// ═══════════════════════════════════════════════════════════════════════════

class JavaScriptStrategy : public BackendStrategy
{
    std::set<std::string> declared;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->JS)");
        emitRaw(out, "'use strict';\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
            return "let " + var + " = " + val + ";";
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        std::string expr = lhs + " " + op + " " + rhs;
        emit(out, indent, decl(res, expr));
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "(((" + lhs + ") !== 0) !== ((" + rhs + ") !== 0)) ? 1 : 0";
        else if (op == "xnor")
            expr = "(((" + lhs + ") !== 0) === ((" + rhs + ") !== 0)) ? 1 : 0";
        else if (op == "xsub")
            expr = "Math.abs((" + lhs + ") - (" + rhs + ")) + 1";
        else if (op == "not")
            expr = "((" + lhs + ") !== 0) ? 0 : 1";
        else
        {
            // Use === for == to avoid JS coercion issues
            std::string jsop = (op == "==") ? "===" : (op == "!=") ? "!=="
                                                                   : op;
            expr = "(" + lhs + " " + jsop + " " + rhs + ") ? 1 : 0";
        }
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return;" : "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "console.log(" + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "process.exit(0);");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "if (" + cond + ") {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "while (true) {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for (let " + iterVar + " of " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, "let " + var + " = [" + content + "];");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        out << label << ":\n";
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue;");
        else
            emit(out, indent, "// goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (!(" + cond + ")) break;");
        else
            emit(out, indent, "if (!(" + cond + ")) { /* goto " + label + " */ }");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear();
        emit(out, indent, "function " + name + "(" + params + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        emit(out, indent, "(function() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "})();");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 3. HTML  (AC->HTML)
// ═══════════════════════════════════════════════════════════════════════════

class HTMLStrategy : public BackendStrategy
{
    std::set<std::string> declared;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "<!DOCTYPE html>");
        emitRaw(out, "<html><head><meta charset=\"UTF-8\"><title>AC Program</title></head>");
        emitRaw(out, "<body><pre id=\"_out\"></pre>\n<script>");
        emitRaw(out, "'use strict';");
        emitRaw(out, "const _el = document.getElementById('_out');");
        emitRaw(out, "function _print(v) { _el.textContent += String(v) + '\\n'; }\n");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "</script>\n</body>\n</html>");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
            return "let " + var + " = " + val + ";";
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        emit(out, indent, decl(res, lhs + " " + op + " " + rhs));
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "(((" + lhs + ") !== 0) !== ((" + rhs + ") !== 0)) ? 1 : 0";
        else if (op == "xnor")
            expr = "(((" + lhs + ") !== 0) === ((" + rhs + ") !== 0)) ? 1 : 0";
        else if (op == "xsub")
            expr = "Math.abs((" + lhs + ") - (" + rhs + ")) + 1";
        else if (op == "not")
            expr = "((" + lhs + ") !== 0) ? 0 : 1";
        else
        {
            std::string jsop = (op == "==") ? "===" : (op == "!=") ? "!=="
                                                                   : op;
            expr = "(" + lhs + " " + jsop + " " + rhs + ") ? 1 : 0";
        }
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return;" : "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "_print(" + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "// program halted");
    }
    void emitForeign(std::ostringstream &out, int &indent, const std::string &code) override
    {
        emitRaw(out, stripQuotes(code));
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "if (" + cond + ") {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "while (true) {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for (let " + iterVar + " of " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, "let " + var + " = [" + content + "];");
    }
    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        out << label << ":\n";
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue;");
        else
            emit(out, indent, "// goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (!(" + cond + ")) break;");
        else
            emit(out, indent, "if (!(" + cond + ")) { /* goto " + label + " */ }");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear();
        emit(out, indent, "function " + name + "(" + params + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        emit(out, indent, "(function() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "})();");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 4. C  (AC->C)
// ═══════════════════════════════════════════════════════════════════════════

class CStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "/* Generated by AC Compiler (AC->C) */");
        emitRaw(out, "#include <stdio.h>");
        emitRaw(out, "#include <stdlib.h>");
        emitRaw(out, "#include <string.h>");
        emitRaw(out, "#include <stdint.h>\n");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "1", "0");
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (looksString(val)) return "ac_str " + var + " = " + val + ";";
            if (isFloatVal(val))  { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "ac_int " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else if (isNew)        emit(out, indent, "ac_int " + res + " = " + expr + ";");
        else                   emit(out, indent, res + " = " + expr + ";");
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "(ac_int)((!!((" + lhs + ")!=0)) ^ (!!((" + rhs + ")!=0)))";
        else if (op == "xnor")
            expr = "(ac_int)((!!((" + lhs + ")!=0)) == (!!((" + rhs + ")!=0)))";
        else if (op == "xsub")
            expr = "(ac_int)(llabs((ac_int)(" + lhs + ") - (ac_int)(" + rhs + ")) + 1)";
        else if (op == "not")
            expr = "(ac_int)(!(" + lhs + "))";
        else
            expr = "(ac_int)(" + lhs + " " + op + " " + rhs + ")";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return 0;" : "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (looksString(val))
            emit(out, indent, "printf(\"%s\\n\", " + val + ");");
        else if (isFloatVal(val))
            emit(out, indent, "printf(\"%.16g\\n\", (double)(" + val + "));");
        else
            emit(out, indent, "printf(\"%lld\\n\", (long long)(" + val + "));");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "exit(0);");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "if (" + cond + ") {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "while (" + cond + ") {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        out << label << ":;\n"; // semicolon needed so label can precede end of block
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        emit(out, indent, "goto " + label + ";");
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        emit(out, indent, "if (!(" + cond + ")) goto " + label + ";");
    }
    void emitJumpIfTrue(std::ostringstream &out, int indent,
                        const std::string &cond, const std::string &label) override
    {
        emit(out, indent, "if (" + cond + ") goto " + label + ";");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear(); floatVars.clear();
        std::string tparams = typedParams(params, "ac_int");
        if (tparams.empty())
            tparams = "void";
        emit(out, indent, "ac_int " + name + "(" + tparams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        emit(out, indent, "int main(void) {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "return 0;");
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 5. C++  (AC->CPP / AC->C++)
// ═══════════════════════════════════════════════════════════════════════════

class CppStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->CPP)");
        emitRaw(out, "#include <iostream>");
        emitRaw(out, "#include <string>");
        emitRaw(out, "#include <vector>");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "#include <cstdlib>");
        emitRaw(out, "using namespace std;\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (isFloatVal(val)) { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "auto " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else if (isNew)        emit(out, indent, "auto " + res + " = " + expr + ";");
        else                   emit(out, indent, res + " = " + expr + ";");
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "(long long)((!!((" + lhs + ")!=0)) ^ (!!((" + rhs + ")!=0)))";
        else if (op == "xnor")
            expr = "(long long)((!!((" + lhs + ")!=0)) == (!!((" + rhs + ")!=0)))";
        else if (op == "xsub")
            expr = "(long long)(std::abs((long long)(" + lhs + ") - (long long)(" + rhs + ")) + 1)";
        else if (op == "not")
            expr = "(long long)(!(" + lhs + "))";
        else
            expr = "(long long)(" + lhs + " " + op + " " + rhs + ")";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return 0;" : "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (isFloatVal(val))
            emit(out, indent, "printf(\"%.16g\\n\", (double)(" + val + "));");
        else
            emit(out, indent, "cout << " + val + " << \"\\n\";");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "exit(0);");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        emit(out, indent, "if (" + cond + ") {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "while (true) {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for (long long " + iterVar + " : " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, "std::vector<long long> " + var + " = {" + content + "};");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        out << label << ":;\n";
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue;");
        else
            emit(out, indent, "goto " + label + ";");
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (!(" + cond + ")) break;");
        else
            emit(out, indent, "if (!(" + cond + ")) goto " + label + ";");
    }
    void emitJumpIfTrue(std::ostringstream &out, int indent,
                        const std::string &cond, const std::string &label) override
    {
        emit(out, indent, "if (" + cond + ") goto " + label + ";");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear(); floatVars.clear();
        std::string tparams = typedParams(params, "long long");
        emit(out, indent, "long long " + name + "(" + tparams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        emit(out, indent, "int main() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "return 0;");
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 6. JAVA  (AC->Java)
// ═══════════════════════════════════════════════════════════════════════════

class JavaStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    std::string className = "Main";

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setOutputStem(const std::string &stem) override { className = stem; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->Java)");
        emitRaw(out, "public class " + className + " {\n");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "}  // class " + className);
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (looksString(val)) return "String " + var + " = " + val + ";";
            if (isFloatVal(val))  { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "long " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else if (isNew)        emit(out, indent, "long " + res + " = " + expr + ";");
        else                   emit(out, indent, res + " = " + expr + ";");
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "((" + lhs + " != 0) != (" + rhs + " != 0)) ? 1L : 0L";
        else if (op == "xnor")
            expr = "((" + lhs + " != 0) == (" + rhs + " != 0)) ? 1L : 0L";
        else if (op == "xsub")
            expr = "Math.abs((" + lhs + ") - (" + rhs + ")) + 1L";
        else if (op == "not")
            expr = "(" + lhs + " == 0) ? 1L : 0L";
        else
            expr = "(" + lhs + " " + op + " " + rhs + ") ? 1L : 0L";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // Bare return (empty val) is the IR's safety fallthrough — suppress it in Java.
        // Java's own flow analysis catches genuinely missing returns.
        if (!val.empty())
            emit(out, indent, "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "System.out.println(" + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "System.exit(0);");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        // Java requires boolean; AC stores comparisons as long (0/1), always needs != 0
        emit(out, indent, "if (" + cond + " != 0) {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "while (true) {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for (long " + iterVar + " : " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, "long[] " + var + " = {" + content + "};");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        emit(out, indent, "// " + label + ":");
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue;");
        else
            emit(out, indent, "// goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (!(" + cond + " != 0)) break;");
        else
            emit(out, indent, "// if (!(" + cond + ")) goto " + label);
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear(); floatVars.clear();
        std::string tparams = typedParams(params, "long");
        emit(out, indent, "static long " + name + "(" + tparams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        emit(out, indent, "public static void main(String[] args) {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 7. RUST  (AC->RS)
// ═══════════════════════════════════════════════════════════════════════════

class RustStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    bool lastWasReturn = false;

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->RS)");
        emitRaw(out, "#![allow(unused_variables, unused_mut, unused_assignments, non_snake_case)]\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (looksString(val))
                return "let mut " + var + ": &str = " + val + ";";
            if (isFloatVal(val))
            {
                floatVars.insert(var);
                return "let mut " + var + ": f64 = " + val + ";";
            }
            return "let mut " + var + ": i64 = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)
        {
            floatVars.insert(res);
            emit(out, indent, "let mut " + res + ": f64 = " + expr + ";");
        }
        else if (isNew)
            emit(out, indent, "let mut " + res + ": i64 = " + expr + ";");
        else
            emit(out, indent, res + " = " + expr + ";");
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "((" + lhs + " != 0) ^ (" + rhs + " != 0)) as i64";
        else if (op == "xnor")
            expr = "((" + lhs + " != 0) == (" + rhs + " != 0)) as i64";
        else if (op == "xsub")
            expr = "((" + lhs + ") as i64 - (" + rhs + ") as i64).abs() + 1";
        else if (op == "not")
            expr = "(" + lhs + " == 0) as i64";
        else
            // Cast bool → i64 via `as i64`
            expr = "(" + lhs + " " + op + " " + rhs + ") as i64";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // Suppress bare empty return; emitFunctionEnd provides the fallthrough `0`
        if (!val.empty())
        {
            emit(out, indent, "return " + val + ";");
            lastWasReturn = true;
        }
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        lastWasReturn = false;
        emit(out, indent, "println!(\"{}\", " + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "std::process::exit(0);");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        lastWasReturn = false;
        // Rust: booleans only; wrap integer conditions
        std::string rcond = looksNumeric(cond) ? (cond + " != 0") : "(" + cond + ") != 0";
        emit(out, indent, "if " + rcond + " {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "loop {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for " + iterVar + " in " + collection + ".iter() {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, "let " + var + " = vec![" + content + "];");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        emit(out, indent, "// " + label + ":");
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue;");
        else
            emit(out, indent, "// goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
        {
            std::string rcond = "(" + cond + ") != 0";
            emit(out, indent, "if !(" + rcond + ") { break; }");
        }
        else
        {
            emit(out, indent, "// if !(" + cond + ") goto " + label);
        }
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear(); floatVars.clear();
        lastWasReturn = false;
        std::string tparams = suffixTypedParams(params, "i64");
        emit(out, indent, "fn " + name + "(" + tparams + ") -> i64 {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        if (!lastWasReturn)
            emit(out, indent, "0"); // default return only if no explicit return
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        lastWasReturn = false;
        emit(out, indent, "fn main() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 8. GO  (AC->GO)
// ═══════════════════════════════════════════════════════════════════════════

class GoStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    bool needsOS = false;

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->GO)");
        emitRaw(out, "package main\n");
        emitRaw(out, "import (");
        emitRaw(out, "    \"fmt\"");
        emitRaw(out, "    \"os\"");
        emitRaw(out, ")\n");
        // Helper: convert int64 boolean result
        emitRaw(out, "func _b(b bool) int64 { if b { return 1 }; return 0 }\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (looksString(val))
                return "var " + var + " string = " + val;
            if (isFloatVal(val)) { floatVars.insert(var); return "var " + var + " float64 = " + val; }
            return "var " + var + " int64 = " + val;
        }
        return var + " = " + val;
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "var " + res + " float64 = " + expr); }
        else if (isNew)        emit(out, indent, "var " + res + " int64 = " + expr);
        else                   emit(out, indent, res + " = " + expr);
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "_b((" + lhs + " != 0) != (" + rhs + " != 0))";
        else if (op == "xnor")
            expr = "_b((" + lhs + " != 0) == (" + rhs + " != 0))";
        else if (op == "xsub")
        {
            // Go has no abs for int64; use closure
            expr = "func() int64 { _d := (" + lhs + ") - (" + rhs + "); if _d < 0 { return -_d + 1 }; return _d + 1 }()";
        }
        else if (op == "not")
            expr = "_b(" + lhs + " == 0)";
        else
            expr = "_b(" + lhs + " " + op + " " + rhs + ")";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return 0" : "return " + val);
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "fmt.Println(" + val + ")");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "os.Exit(0)");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        // Go requires bool; int64 conditions always need != 0
        emit(out, indent, "if " + cond + " != 0 {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "for {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for _, " + iterVar + " := range " + collection + " {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        emit(out, indent, var + " := []int64{" + content + "}");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        out << label << ":\n";
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue");
        else
            emit(out, indent, "goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if " + cond + " == 0 { break }");
        else
            emit(out, indent, "if " + cond + " == 0 { goto " + label + " }");
    }
    void emitJumpIfTrue(std::ostringstream &out, int indent,
                        const std::string &cond, const std::string &label) override
    {
        emit(out, indent, "if " + cond + " != 0 { goto " + label + " }");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear(); floatVars.clear();
        std::string tparams = goTypedParams(params, "int64");
        emit(out, indent, "func " + name + "(" + tparams + ") int64 {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "return 0");
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        emit(out, indent, "func main() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 9. V  (AC->V)  — vlang.io
// ═══════════════════════════════════════════════════════════════════════════

class VStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    bool lastWasReturn = false;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->V)");
        emitRaw(out, "module main\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (looksString(val))
                return "mut " + var + " := " + val;
            return "mut " + var + " := i64(" + val + ")";
        }
        return var + " = " + val;
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        emit(out, indent, decl(res, lhs + " " + op + " " + rhs));
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr;
        if (op == "xor")
            expr = "if (" + lhs + " != 0) != (" + rhs + " != 0) { i64(1) } else { i64(0) }";
        else if (op == "xnor")
            expr = "if (" + lhs + " != 0) == (" + rhs + " != 0) { i64(1) } else { i64(0) }";
        else if (op == "xsub")
            expr = "if (" + lhs + ") >= (" + rhs + ") { (" + lhs + ") - (" + rhs + ") + 1 } else { (" + rhs + ") - (" + lhs + ") + 1 }";
        else if (op == "not")
            expr = "if " + lhs + " == 0 { i64(1) } else { i64(0) }";
        else
            expr = "if " + lhs + " " + op + " " + rhs + " { i64(1) } else { i64(0) }";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // Suppress bare empty return; emitFunctionEnd provides the fallthrough
        if (!val.empty())
        {
            emit(out, indent, "return " + val);
            lastWasReturn = true;
        }
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        lastWasReturn = false;
        emit(out, indent, "println(" + val + ")");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "exit(0)");
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        lastWasReturn = false;
        emit(out, indent, "if " + cond + " != 0 {");
        indent++;
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} else {");
        indent++;
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &) override
    {
        emit(out, indent, "for {");
        indent++;
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for " + iterVar + " in " + collection + " {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string & /*type*/,
                   const std::string &content) override
    {
        // V requires explicit type on first element
        std::string c = content;
        size_t comma = c.find(',');
        std::string vContent = comma != std::string::npos
                                   ? "i64(" + c.substr(0, comma) + ")" + c.substr(comma)
                                   : "i64(" + c + ")";
        emit(out, indent, "mut " + var + " := [" + vContent + "]");
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        emit(out, indent, "// " + label + ":");
    }
    void emitJump(std::ostringstream &out, int indent, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "break");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "continue");
        else
            emit(out, indent, "// goto " + label);
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if " + cond + " == 0 { break }");
        else
            emit(out, indent, "// if !(" + cond + ") goto " + label);
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        declared.clear();
        lastWasReturn = false;
        std::string tparams = suffixTypedParams(params, "i64");
        emit(out, indent, "fn " + name + "(" + tparams + ") i64 {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        if (!lastWasReturn)
            emit(out, indent, "return i64(0)");
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        lastWasReturn = false;
        emit(out, indent, "fn main() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 10. ASM  (AC->ASM)  — NASM x86-64 Linux, links via gcc
// ═══════════════════════════════════════════════════════════════════════════

class AsmStrategy : public BackendStrategy
{
    std::vector<std::string> dataSec; // accumulates .data entries
    std::map<std::string, int> slot;  // var/temp → rbp offset
    int nextSlot = 0;
    int strIdx = 0;

    // ── helpers ─────────────────────────────────────────────────────────────

    int getSlot(const std::string &name)
    {
        if (slot.find(name) == slot.end())
        {
            nextSlot += 8;
            slot[name] = nextSlot;
        }
        return slot[name];
    }

    // Emit code to load `val` into RAX
    void loadRAX(std::ostringstream &out, const std::string &val)
    {
        if (looksNumeric(val))
        {
            out << "    mov rax, " << val << "\n";
        }
        else if (looksString(val))
        {
            std::string lbl = ".str" + std::to_string(strIdx++);
            dataSec.push_back(lbl + " db " + val + ", 0");
            out << "    lea rax, [rel " << lbl << "]\n";
        }
        else
        {
            out << "    mov rax, [rbp-" << getSlot(val) << "]\n";
        }
    }

    void storeRAX(std::ostringstream &out, const std::string &dst)
    {
        out << "    mov [rbp-" << getSlot(dst) << "], rax\n";
    }

    void emit(std::ostringstream &out, int /*indent*/, const std::string &line) override
    {
        out << "    " << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override
    {
        out << line << "\n";
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "; Generated by AC Compiler (AC->ASM)");
        emitRaw(out, "; x86-64 Linux NASM — assemble: nasm -f elf64 out.s && gcc out.o -o out");
        emitRaw(out, "");
        emitRaw(out, "    default rel");
        emitRaw(out, "    extern printf, exit");
        emitRaw(out, "    global main");
        emitRaw(out, "");
        emitRaw(out, "section .data");
        emitRaw(out, "    _fmt_d db \"%lld\", 10, 0");
        emitRaw(out, "    _fmt_s db \"%s\",  10, 0");
        emitRaw(out, "");
        emitRaw(out, "section .text");
    }

    void emitFooter(std::ostringstream &out) override
    {
        if (dataSec.empty())
            return;
        // Extra string literals go after the code
        emitRaw(out, "");
        emitRaw(out, "section .data");
        for (auto &d : dataSec)
            emitRaw(out, "    " + d);
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "1", "0");
    }

    void emitStoreVar(std::ostringstream &out, int & /*indent*/, const std::string &var, const std::string &val) override
    {
        loadRAX(out, val);
        storeRAX(out, var);
    }

    void emitBinaryOp(std::ostringstream &out, int & /*indent*/, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        loadRAX(out, lhs);
        if (op == '+')
        {
            if (looksNumeric(rhs))
                out << "    add rax, " << rhs << "\n";
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
                out << "    add rax, rbx\n";
            }
        }
        else if (op == '-')
        {
            if (looksNumeric(rhs))
                out << "    sub rax, " << rhs << "\n";
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
                out << "    sub rax, rbx\n";
            }
        }
        else if (op == '*')
        {
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    imul rax, rbx\n";
        }
        else if (op == '/')
        {
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    cqo\n    idiv rbx\n"; // quotient in RAX
        }
        else if (op == '%')
        {
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    cqo\n    idiv rbx\n    mov rax, rdx\n"; // remainder in RDX
        }
        storeRAX(out, res);
    }

    void emitComparison(std::ostringstream &out, int & /*indent*/, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        if (op == "xsub")
        {
            // |lhs - rhs| + 1 using two's-complement abs trick: cqo / xor / sub / inc
            loadRAX(out, lhs);
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    sub rax, rbx\n";
            out << "    cqo\n";          // sign-extend rax into rdx (0 or -1)
            out << "    xor rax, rdx\n"; // flip bits if negative
            out << "    sub rax, rdx\n"; // add 1 if was negative (abs)
            out << "    inc rax\n";      // +1 for inclusive count
            storeRAX(out, res);
            return;
        }
        if (op == "xor")
        {
            loadRAX(out, lhs);
            out << "    test rax, rax\n";
            out << "    setne al\n";
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    test rbx, rbx\n";
            out << "    setne bl\n";
            out << "    xor al, bl\n";
            out << "    movzx rax, al\n";
            storeRAX(out, res);
            return;
        }
        if (op == "xnor")
        {
            loadRAX(out, lhs);
            out << "    test rax, rax\n";
            out << "    setne al\n";
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            out << "    test rbx, rbx\n";
            out << "    setne bl\n";
            out << "    xor al, bl\n";
            out << "    xor al, 1\n"; // invert for xnor
            out << "    movzx rax, al\n";
            storeRAX(out, res);
            return;
        }
        loadRAX(out, lhs);
        if (looksNumeric(rhs))
            out << "    cmp rax, " << rhs << "\n";
        else
        {
            out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            out << "    cmp rax, rbx\n";
        }
        // SETcc → AL, then zero-extend
        std::string setcc =
            (op == "==") ? "sete" : (op == "!=") ? "setne"
                                : (op == "<")    ? "setl"
                                : (op == ">")    ? "setg"
                                : (op == "<=")   ? "setle"
                                                 : "setge";
        out << "    " << setcc << " al\n";
        out << "    movzx rax, al\n";
        storeRAX(out, res);
    }

    void emitCall(std::ostringstream &out, int & /*indent*/, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        // Simplified: push args right-to-left is complex; just call with no args for now
        // A full implementation would parse `args` and push each one
        out << "    ; call " << func << "(" << args << ")\n";
        out << "    call " << func << "\n";
        if (!res.empty())
            storeRAX(out, res);
    }

    void emitReturn(std::ostringstream &out, int & /*indent*/, const std::string &val) override
    {
        if (!val.empty())
            loadRAX(out, val);
        else
            out << "    xor eax, eax\n";
        out << "    leave\n    ret\n";
    }

    void emitPrint(std::ostringstream &out, int & /*indent*/, const std::string &val) override
    {
        if (looksString(val))
        {
            std::string lbl = ".str" + std::to_string(strIdx++);
            dataSec.push_back(lbl + " db " + val + ", 0");
            out << "    lea rsi, [rel " << lbl << "]\n";
            out << "    lea rdi, [rel _fmt_s]\n";
        }
        else
        {
            loadRAX(out, val);
            out << "    mov rsi, rax\n";
            out << "    lea rdi, [rel _fmt_d]\n";
        }
        out << "    xor eax, eax\n";
        out << "    call printf\n";
    }

    void emitHalt(std::ostringstream &out, int & /*indent*/) override
    {
        out << "    xor edi, edi\n";
        out << "    call exit\n";
    }

    void emitIfBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        // cond is a variable holding 0 or 1
        loadRAX(out, cond);
        std::string skip = ".Lskip" + std::to_string(strIdx++);
        out << "    test rax, rax\n";
        out << "    jz " << skip << "\n";
        out << "; if-begin\n";
        // We can't easily push the label here; emit a comment placeholder
        // The user should use high-level IR for best results
    }
    void emitIfElse(std::ostringstream &out, int &indent) override
    {
        out << "; else\n";
    }
    void emitIfEnd(std::ostringstream &out, int &indent) override
    {
        out << "; end-if\n";
    }
    void emitWhileBegin(std::ostringstream &out, int &indent, const std::string &cond) override
    {
        out << "; while-begin (" << cond << ")\n";
    }
    void emitWhileEnd(std::ostringstream &out, int &indent) override
    {
        out << "; while-end\n";
    }

    void emitLabel(std::ostringstream &out, int /*indent*/, const std::string &label) override
    {
        out << label << ":\n";
    }
    void emitJump(std::ostringstream &out, int /*indent*/, const std::string &label) override
    {
        out << "    jmp " << label << "\n";
    }
    void emitJumpIfFalse(std::ostringstream &out, int /*indent*/,
                         const std::string &cond, const std::string &label) override
    {
        loadRAX(out, cond);
        out << "    test rax, rax\n";
        out << "    jz " << label << "\n";
    }
    void emitJumpIfTrue(std::ostringstream &out, int /*indent*/,
                        const std::string &cond, const std::string &label) override
    {
        loadRAX(out, cond);
        out << "    test rax, rax\n";
        out << "    jnz " << label << "\n";
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params) override
    {
        slot.clear();
        nextSlot = 0;
        emitRaw(out, "");
        emitRaw(out, name + ":");
        out << "    push rbp\n    mov rbp, rsp\n    sub rsp, 128\n";
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        out << "    xor eax, eax\n    leave\n    ret\n";
        slot.clear();
        nextSlot = 0;
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        slot.clear();
        nextSlot = 0;
        emitRaw(out, "\nmain:");
        out << "    push rbp\n    mov rbp, rsp\n    sub rsp, 256\n";
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        out << "    xor edi, edi\n    call exit\n";
        slot.clear();
        nextSlot = 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// UNIFIED IR CODE GENERATOR
// ═══════════════════════════════════════════════════════════════════════════

class UnifiedIRCodeGen
{
    const IRProgram &ir;
    std::unique_ptr<BackendStrategy> strategy;
    std::ostringstream out;
    int indentLevel = 0;

    std::string ref(const IRRef &r)
    {
        return strategy->formatRef(r, const_cast<SymbolTable *>(&ir.symbols));
    }

    void genInstr(const IRInstruction &i)
    {
        switch (i.opcode)
        {

        // ── data movement ──────────────────────────────────────────────
        case IROpcode::LOAD_CONST:
            if (i.result.isValid())
            {
                std::string src = i.typedOperands.empty() ? "0" : ref(i.typedOperands[0]);
                // fallback: try old-style value field
                if (i.typedOperands.empty() && i.value.type == IRType::INT)
                    src = std::to_string(std::get<int>(i.value.data));
                strategy->emitStoreVar(out, indentLevel, ref(i.result), src);
            }
            break;

        case IROpcode::LOAD_VAR:
            if (i.result.isValid() && !i.typedOperands.empty())
                strategy->emitStoreVar(out, indentLevel, ref(i.result), ref(i.typedOperands[0]));
            break;

        case IROpcode::STORE_VAR:
            if (i.typedOperands.size() >= 2)
                strategy->emitStoreVar(out, indentLevel, ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            else if (i.result.isValid() && !i.typedOperands.empty())
                strategy->emitStoreVar(out, indentLevel, ref(i.result), ref(i.typedOperands[0]));
            break;

        case IROpcode::ALLOC:
            if (i.result.isValid() && i.typedOperands.size() >= 2)
            {
                // Strip surrounding quotes from type tag and content
                std::string allocType = ref(i.typedOperands[0]);
                if (allocType.size() >= 2 && allocType.front() == '"')
                    allocType = allocType.substr(1, allocType.size() - 2);
                std::string content = ref(i.typedOperands[1]);
                if (content.size() >= 2 && content.front() == '"')
                    content = content.substr(1, content.size() - 2);
                strategy->emitAlloc(out, indentLevel, ref(i.result), allocType, content);
            }
            break;

        // ── arithmetic ─────────────────────────────────────────────────
        case IROpcode::ADD:
        case IROpcode::SUB:
        case IROpcode::MUL:
        case IROpcode::DIV:
        case IROpcode::MOD:
        {
            char op = (i.opcode == IROpcode::ADD) ? '+' : (i.opcode == IROpcode::SUB) ? '-'
                                                      : (i.opcode == IROpcode::MUL)   ? '*'
                                                      : (i.opcode == IROpcode::DIV)   ? '/'
                                                                                      : '%';
            if (i.typedOperands.size() >= 2)
                strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]), op);
            break;
        }

        // ── comparisons ────────────────────────────────────────────────
        case IROpcode::EQ:
        case IROpcode::NEQ:
        case IROpcode::LT:
        case IROpcode::GT:
        case IROpcode::LTE:
        case IROpcode::GTE:
        {
            std::string op = (i.opcode == IROpcode::EQ) ? "==" : (i.opcode == IROpcode::NEQ) ? "!="
                                                             : (i.opcode == IROpcode::LT)    ? "<"
                                                             : (i.opcode == IROpcode::GT)    ? ">"
                                                             : (i.opcode == IROpcode::LTE)   ? "<="
                                                                                             : ">=";
            if (i.typedOperands.size() >= 2)
                strategy->emitComparison(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]), op);
            break;
        }

        // ── logical AND / OR ───────────────────────────────────────────
        case IROpcode::AND:
        case IROpcode::OR:
        {
            // Map to comparison: AND = both != 0, OR = either != 0
            // Use emitComparison with a special op string that each backend maps
            // Simpler: treat as arithmetic (non-zero & non-zero)
            // For now emit as binary op with & or |
            char op = (i.opcode == IROpcode::AND) ? '&' : '|';
            if (i.typedOperands.size() >= 2)
                strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]), op);
            break;
        }

        // ── logical NOT ────────────────────────────────────────────────
        case IROpcode::NOT:
            if (!i.typedOperands.empty())
            {
                // Emit as XOR with 1 is complex; use "not" special op in emitComparison
                strategy->emitComparison(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), "", "not");
            }
            break;

        // ── boolean XOR / XNOR ─────────────────────────────────────────
        case IROpcode::XOR:
            if (i.typedOperands.size() >= 2)
                strategy->emitComparison(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]), "xor");
            break;
        case IROpcode::XNOR:
            if (i.typedOperands.size() >= 2)
                strategy->emitComparison(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]), "xnor");
            break;
        case IROpcode::XSUB:
            if (i.typedOperands.size() >= 2)
                strategy->emitComparison(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]), "xsub");
            break;

        // ── control flow (high-level IR) ───────────────────────────────
        case IROpcode::IF_BEGIN:
            if (!i.typedOperands.empty())
                strategy->emitIfBegin(out, indentLevel, ref(i.typedOperands[0]));
            break;

        case IROpcode::IF_ELSE:
            strategy->emitIfElse(out, indentLevel);
            break;

        case IROpcode::IF_END:
            strategy->emitIfEnd(out, indentLevel);
            break;

        case IROpcode::WHILE_BEGIN:
            strategy->emitWhileBegin(out, indentLevel,
                                     i.typedOperands.empty() ? "" : ref(i.typedOperands[0]));
            break;

        case IROpcode::WHILE_END:
            strategy->emitWhileEnd(out, indentLevel);
            break;

        case IROpcode::FOR_BEGIN:
            if (i.typedOperands.size() >= 2)
                strategy->emitForBegin(out, indentLevel,
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            break;

        case IROpcode::FOR_END:
            strategy->emitForEnd(out, indentLevel);
            break;

        // ── control flow (low-level IR) ────────────────────────────────
        case IROpcode::LABEL:
            if (!i.typedOperands.empty())
                strategy->emitLabel(out, indentLevel, ref(i.typedOperands[0]));
            else if (i.result.isValid())
                strategy->emitLabel(out, indentLevel, ref(i.result));
            break;

        case IROpcode::JUMP:
            if (!i.typedOperands.empty())
                strategy->emitJump(out, indentLevel, ref(i.typedOperands[0]));
            break;

        case IROpcode::JUMP_IF_FALSE:
            if (i.typedOperands.size() >= 2)
                strategy->emitJumpIfFalse(out, indentLevel,
                                          ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            break;

        case IROpcode::JUMP_IF_TRUE:
            if (i.typedOperands.size() >= 2)
                strategy->emitJumpIfTrue(out, indentLevel,
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            break;

        // ── function call ──────────────────────────────────────────────
        case IROpcode::CALL:
        {
            std::string func = i.typedOperands.empty() ? "" : ref(i.typedOperands[0]);
            std::string args;
            for (size_t j = 1; j < i.typedOperands.size(); j++)
            {
                if (j > 1)
                    args += ", ";
                args += ref(i.typedOperands[j]);
            }
            strategy->emitCall(out, indentLevel, ref(i.result), func, args);
            break;
        }

        case IROpcode::RETURN:
            strategy->emitReturn(out, indentLevel,
                                 i.typedOperands.empty() ? "" : ref(i.typedOperands[0]));
            break;

        // ── I/O & special ──────────────────────────────────────────────
        case IROpcode::PRINT:
            if (!i.typedOperands.empty())
                strategy->emitPrint(out, indentLevel, ref(i.typedOperands[0]));
            break;

        case IROpcode::LIB_CALL:
        {
            if (i.typedOperands.empty())
                break;
            std::string method = ref(i.typedOperands[0]);
            if ((method == "\"Term.display\"" || method == "Term.display") &&
                i.typedOperands.size() > 1)
            {
                strategy->emitPrint(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if ((method == "\"foreign\"" || method == "foreign") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitForeign(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if (i.typedOperands.size() > 1)
            {
                // Generic lib call with result
                std::string args;
                for (size_t j = 1; j < i.typedOperands.size(); j++)
                {
                    if (j > 1)
                        args += ", ";
                    args += ref(i.typedOperands[j]);
                }
                strategy->emitCall(out, indentLevel, ref(i.result), stripQuotes(method), args);
            }
            break;
        }

        case IROpcode::HALT:
            strategy->emitHalt(out, indentLevel);
            break;

        case IROpcode::FUNC_BEGIN:
        case IROpcode::FUNC_END:
        case IROpcode::NOP:
            break;

        default:
            break;
        }
    }

    void genFunction(const IRFunction &func)
    {
        std::string params;
        for (size_t i = 0; i < func.parameters.size(); i++)
        {
            if (i > 0)
                params += ", ";
            params += func.parameters[i];
        }
        strategy->emitFunctionBegin(out, indentLevel, func.name, params);
        for (const auto &instr : func.instructions)
            genInstr(instr);
        strategy->emitFunctionEnd(out, indentLevel);
    }

public:
    explicit UnifiedIRCodeGen(const IRProgram &program, const std::string &stem = "Main") : ir(program)
    {
        const std::string &b = program.backend;
        if (b == "PY")
            strategy = std::make_unique<PythonStrategy>();
        else if (b == "JS")
            strategy = std::make_unique<JavaScriptStrategy>();
        else if (b == "HTML")
            strategy = std::make_unique<HTMLStrategy>();
        else if (b == "C")
            strategy = std::make_unique<CStrategy>();
        else if (b == "C++" || b == "CPP")
            strategy = std::make_unique<CppStrategy>();
        else if (b == "Java")
            strategy = std::make_unique<JavaStrategy>();
        else if (b == "RS")
            strategy = std::make_unique<RustStrategy>();
        else if (b == "GO")
            strategy = std::make_unique<GoStrategy>();
        else if (b == "V")
            strategy = std::make_unique<VStrategy>();
        else if (b == "ASM")
            strategy = std::make_unique<AsmStrategy>();
        else
            strategy = std::make_unique<PythonStrategy>();
        strategy->setOutputStem(stem);
    }

    std::string generate()
    {
        strategy->emitHeader(out);

        for (const auto &func : ir.functions)
            genFunction(func);

        strategy->emitMainBegin(out, indentLevel);
        for (const auto &instr : ir.globalInit)
            genInstr(instr);
        strategy->emitMainEnd(out, indentLevel);

        strategy->emitFooter(out);
        return out.str();
    }
};

// ─── public API ──────────────────────────────────────────────────────────────

std::string generateFromIR(const IRProgram &ir, const std::string &stem)
{
    UnifiedIRCodeGen gen(ir, stem);
    return gen.generate();
}
