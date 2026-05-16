#include "../include/ac.hpp"
#include <sstream>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#define ac_getcwd _getcwd
#else
#include <unistd.h>
#define ac_getcwd getcwd
#endif

using namespace AC_IR;

// ─── FFI file reader ─────────────────────────────────────────────────────────
// Reads library/<libName>/ffi/<libName>_ffi.<ext> relative to cwd.
static std::string readFFIFile(const std::string &libName, const std::string &ext)
{
    std::string path = "library/" + libName + "/ffi/" + libName + "_ffi." + ext;
    FILE *f = std::fopen(path.c_str(), "r");
    if (!f) return "";
    std::string content;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) content += buf;
    std::fclose(f);
    return content;
}

// Parses a Go FFI file into two parts for inlining:
//   cgoBlock   — everything from the CGO comment up to and including `import "C"`
//   wrapperCode — everything after (skipping `import "unsafe"` standalone lines,
//                 skipping the `package` declaration line)
static void parseGoFFI(const std::string &libName,
                       std::string &cgoBlock,
                       std::string &wrapperCode)
{
    cgoBlock.clear(); wrapperCode.clear();
    std::string content = readFFIFile(libName, "go");
    if (content.empty()) return;
    std::istringstream ss(content);
    std::string line;
    bool pastCImport = false;
    while (std::getline(ss, line)) {
        if (line.rfind("package ", 0) == 0) continue;  // skip package line
        if (!pastCImport) {
            cgoBlock += line + "\n";
            if (line == "import \"C\"") pastCImport = true;
        } else {
            if (line == "import \"unsafe\"") continue;  // merged into regular imports
            wrapperCode += line + "\n";
        }
    }
}

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

// Pre-populate a declared set with parameter names from "p1, p2, ..." so that
// assignments to params emit plain "n = val" instead of "TYPE n = val".
static void declareParams(const std::string &params, std::set<std::string> &declared)
{
    if (params.empty()) return;
    std::istringstream ss(params);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        size_t s = tok.find_first_not_of(" \t");
        size_t e = tok.find_last_not_of(" \t");
        if (s != std::string::npos)
            declared.insert(tok.substr(s, e - s + 1));
    }
}

// Math library functions that return integers, not floats.
// Strips any "AcMath." prefix before comparing (for Java backend).
static bool isIntReturningMathFunc(const char* s) {
    if (strncmp(s, "AcMath.", 7) == 0) s += 7;
    return strcmp(s, "math_to_int")   == 0
        || strcmp(s, "math_abs_int")  == 0
        || strcmp(s, "math_mod_int")  == 0
        || strcmp(s, "math_gcd")      == 0
        || strcmp(s, "math_lcm")      == 0
        || strcmp(s, "math_is_prime") == 0;
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
// Reformat "p1, p2" → "mut p1: TYPE, mut p2: TYPE"  (Rust mutable params)
static std::string rustMutTypedParams(const std::string &params, const std::string &typeName)
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
        result += "mut " + name + ": " + typeName;
        first = false;
    }
    return result;
}

// Common ref formatter (all backends except ASM share this)
// Escape a raw string for embedding in a double-quoted literal.
static std::string escapeStr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}

static std::string commonRef(const IRRef &r, SymbolTable *sym,
                             const std::string &trueVal  = "true",
                             const std::string &falseVal = "false",
                             const std::string &nullVal  = "null")
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
            // null literal → backend-specific null
            if (s == "null") return nullVal;
            return "\"" + escapeStr(s) + "\"";
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
    // Full output base path (e.g. "examples/math_number") — used by backends that need
    // to compute paths relative to the output file's directory.
    virtual void setOutputBase(const std::string &) {}

    // True for backends that call functions with dot syntax (obj.method / lib.func).
    // False (default) for backends that need lib_func underscore naming.
    virtual bool dotCallSyntax() const { return false; }

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
    // style: "bold", "italic", "header", "link", "title" — default falls back to plain print
    virtual void emitStyledPrint(std::ostringstream &out, int &indent,
                                 const std::string &val, const std::string &style)
    {
        emitPrint(out, indent, val);
    }
    virtual void emitHalt(std::ostringstream &out, int &indent) = 0;
    virtual void emitEval(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &expr) = 0;
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
    virtual void emitLoadIndex(std::ostringstream &out, int &indent,
                               const std::string &result, const std::string &arr,
                               const std::string &idx) {}
    virtual void emitStoreIndex(std::ostringstream &out, int &indent,
                                const std::string &arr, const std::string &idx,
                                const std::string &val) {}
    virtual void emitInput(std::ostringstream &out, int &indent,
                           const std::string &result, const std::string &prompt) {}
    // libType: "ilib", "elib", "clib"; libName: the library name (no prefix)
    virtual void emitLibImport(std::ostringstream &out, const std::string &libType,
                               const std::string &libName) {}

    // Called before emitHeader so strategies can adapt to what opcodes are used
    virtual void setNeedsInput(bool) {}

    // Called before emitHeader with all ilib/elib/clib imports found in the program.
    // Strategies that need file-scope includes (C, C++) emit them in emitHeader.
    // Others emit them on-demand in emitLibImport.
    virtual void setPendingImports(const std::vector<std::pair<std::string,std::string>>&) {}

    // Optional: per-library selective symbol list from "from ilib X use a,b,c".
    // Key = "ilib:math", value = {"sin","cos",...}. Empty set = import all.
    virtual void setImportSymbols(const std::unordered_map<std::string,std::set<std::string>>&) {}

    // Called before emitting any function to declare which global vars the function writes to
    virtual void setPendingGlobals(const std::vector<std::string> &) {}

    // Raise a runtime error with the given message string
    virtual void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) {}

    // Event system — called when EVENT_BIND / EVENT_TRIGGER opcodes are present
    virtual void setNeedsEvents(bool) {}
    virtual void emitEventBind(std::ostringstream &out, int &indent,
                               const std::string &key, const std::string &callback) {}
    virtual void emitEventTrigger(std::ostringstream &out, int &indent,
                                  const std::string &key) {}

    virtual void emitLabel(std::ostringstream &out, int indent, const std::string &label) = 0;
    virtual void emitJump(std::ostringstream &out, int indent, const std::string &label) = 0;
    virtual void emitJumpIfFalse(std::ostringstream &out, int indent,
                                 const std::string &cond, const std::string &label) = 0;
    virtual void emitJumpIfTrue(std::ostringstream &out, int indent,
                                const std::string &cond, const std::string &label) {}

    virtual void emitFunctionBegin(std::ostringstream &out, int &indent,
                                   const std::string &name, const std::string &params,
                                   const std::string &classOwner = "") = 0;
    virtual void emitFunctionEnd(std::ostringstream &out, int &indent) = 0;
    virtual void emitMainBegin(std::ostringstream &out, int &indent) = 0;
    virtual void emitMainEnd(std::ostringstream &out, int &indent) = 0;

    // Class/bundle support
    virtual void emitClassBegin(std::ostringstream &out, int &indent,
                                const std::string &name) { (void)out; (void)indent; (void)name; }
    virtual void emitClassEnd(std::ostringstream &out, int &indent) { (void)out; (void)indent; }
    // Return true if the backend cannot emit field-default instructions inside a class body.
    virtual bool suppressClassBody() const { return false; }
};

// ═══════════════════════════════════════════════════════════════════════════
// ── shared helpers ────────────────────────────────────────────────────────────

// Convert "key1:val1,key2:val2" dict content to raw {key, value} pairs.
// Dollar-sign strings like $Alice$ are stored as-is; bare identifiers too.
static std::vector<std::pair<std::string,std::string>> parseDictPairs(const std::string& content) {
    std::vector<std::pair<std::string,std::string>> out;
    std::string s = content;
    while (!s.empty()) {
        auto comma = s.find(',');
        std::string pair = comma == std::string::npos ? s : s.substr(0, comma);
        s = comma == std::string::npos ? "" : s.substr(comma + 1);
        auto colon = pair.find(':');
        if (colon != std::string::npos)
            out.push_back({pair.substr(0, colon), pair.substr(colon + 1)});
    }
    return out;
}

// Format a dict KEY: always a string in the target language.
// bare 'name' → "name";  '$Alice$' → "Alice"
static std::string fmtDictKey(const std::string& k) {
    if (k.size() >= 2 && k.front() == '$' && k.back() == '$')
        return "\"" + k.substr(1, k.size() - 2) + "\"";
    return "\"" + k + "\"";
}

// Format a dict VALUE: $..$ → quoted string; number/bool/var → as-is.
static std::string fmtDictVal(const std::string& v) {
    if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
        return "\"" + v.substr(1, v.size() - 2) + "\"";
    return v;
}

// 1. PYTHON  (AC->PY)
// ═══════════════════════════════════════════════════════════════════════════

class PythonStrategy : public BackendStrategy
{
    std::set<std::string> globalVars_;
    std::vector<std::string> pendingGlobals_;
    bool needsEvents_ = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string,std::set<std::string>> importSymbols_;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }
    void setImportSymbols(const std::unordered_map<std::string,std::set<std::string>>& s) override
    {
        importSymbols_ = s;
    }

    // Build a comment listing the selectively imported symbols.
    static std::string selectiveImportComment(const std::set<std::string>& syms) {
        std::string s = "# selective: use ";
        bool first = true;
        for (auto& sym : syms) { if (!first) s += ", "; s += sym; first = false; }
        return s + "\n";
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "# Generated by AC Compiler (AC->PY)");
        emitRaw(out, "import sys");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string ffi = readFFIFile(ln, "py");
                if (!ffi.empty()) {
                    // Emit selective-import comment if symbols were specified
                    std::string key = lt + ":" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty())
                        out << selectiveImportComment(it->second);
                    out << ffi;
                }
            }
        }
        if (needsEvents_) {
            emitRaw(out, "_ac_events = {}");
            emitRaw(out, "def _ac_bind(key, fn): _ac_events[key] = fn");
            emitRaw(out, "def _ac_trigger(key):");
            emitRaw(out, "    if key in _ac_events: _ac_events[key]()");
        }
        emitRaw(out, "");
    }

    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_ac_bind(" + key + ", " + callback + ")");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_ac_trigger(" + key + ")");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "True", "False", "None");
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        emit(out, indent, res + " = float(eval(" + expr + "))");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            std::string d = "{";
            bool first = true;
            for (auto& [k, v] : parseDictPairs(content)) {
                if (!first) d += ", ";
                d += fmtDictKey(k) + ": " + fmtDictVal(v);
                first = false;
            }
            emit(out, indent, var + " = " + d + "}");
        } else if (type == "tuple") {
            emit(out, indent, var + " = (" + content + ",)");
        } else {
            emit(out, indent, var + " = [" + content + "]");
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, result + " = " + arr + "[" + idx + "]");
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val);
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, result + " = input(" + prompt + ")");
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

    void setPendingGlobals(const std::vector<std::string> &g) override
    {
        pendingGlobals_ = g;
    }

    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "raise Exception(\"Preposterous: \" + str(" + msg + "))");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        // init → __init__ for Python classes
        std::string pyName = (!classOwner.empty() && name == "init") ? "__init__" : name;
        emit(out, indent, "def " + pyName + "(" + params + "):");
        indent++;
        for (auto &g : pendingGlobals_)
            emit(out, indent, "global " + g);
        pendingGlobals_.clear();
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emitRaw(out, "");
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emitRaw(out, "class " + name + ":");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emitRaw(out, "");
    }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (FFI file inlined there)
        if (libType == "elib")
            emitRaw(out, "import " + libName);
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
    bool needsEvents_ = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->JS)");
        emitRaw(out, "'use strict';");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string ffi = readFFIFile(ln, "js");
                if (!ffi.empty()) out << ffi;
            }
        }
        if (needsEvents_) {
            emitRaw(out, "const _acEvents = {};");
            emitRaw(out, "function _acBind(key, fn) { _acEvents[key] = fn; }");
            emitRaw(out, "function _acTrigger(key) { if (_acEvents[key]) _acEvents[key](); }");
        }
        emitRaw(out, "");
    }

    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_acBind(" + key + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_acTrigger(" + key + ");");
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        emit(out, indent, decl(res, "Function('return (' + " + expr + " + ')()')()"));
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "throw new Error(\"Preposterous: \" + String(" + msg + "));");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            std::string d = "{";
            bool first = true;
            for (auto& [k, v] : parseDictPairs(content)) {
                if (!first) d += ", ";
                d += fmtDictKey(k) + ": " + fmtDictVal(v);
                first = false;
            }
            emit(out, indent, "let " + var + " = " + d + "};");
        } else if (type == "tuple") {
            emit(out, indent, "let " + var + " = [" + content + "];");
        } else {
            emit(out, indent, "let " + var + " = [" + content + "];");
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, "let " + result + " = " + arr + "[" + idx + "];");
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val + ";");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "let " + result + " = require('readline-sync').question(" + prompt + ");");
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear();
        // Strip 'self' from JS method params — JS uses 'this' implicitly
        std::string jsParams = params;
        if (!classOwner.empty()) {
            if (jsParams.rfind("self, ", 0) == 0) jsParams = jsParams.substr(6);
            else if (jsParams == "self") jsParams = "";
        }
        std::string jsName = (!classOwner.empty() && name == "init") ? "constructor" : name;
        if (classOwner.empty())
            emit(out, indent, "function " + jsName + "(" + jsParams + ") {");
        else
            emit(out, indent, jsName + "(" + jsParams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
    }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (FFI file inlined there)
        if (libType == "elib")
            emitRaw(out, "const " + libName + " = require('" + libName + "');");
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
    bool needsEvents_ = false;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsEvents(bool v) override { needsEvents_ = v; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "<!DOCTYPE html>");
        emitRaw(out, "<html><head><meta charset=\"UTF-8\"><title>AC Program</title></head>");
        emitRaw(out, "<body><pre id=\"_out\"></pre>\n<script>");
        emitRaw(out, "'use strict';");
        emitRaw(out, "const _el = document.getElementById('_out');");
        emitRaw(out, "function _print(v) { _el.textContent += String(v) + '\\n'; }");
        if (needsEvents_) {
            emitRaw(out, "const _acEvents = {};");
            emitRaw(out, "function _acBind(key, fn) { _acEvents[key] = fn; document.addEventListener('keydown', function(e) { if (e.key === key) fn(); }); }");
            emitRaw(out, "function _acTrigger(key) { if (_acEvents[key]) _acEvents[key](); }");
        }
        emitRaw(out, "");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "</script>\n</body>\n</html>");
    }

    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_acBind(" + key + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_acTrigger(" + key + ");");
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
    void emitStyledPrint(std::ostringstream &out, int &indent,
                         const std::string &val, const std::string &style) override
    {
        if (style == "title")
            emit(out, indent, "_el.ownerDocument.title = " + val + ";");
        else if (style == "bold")
            emit(out, indent, "_el.innerHTML += '<b>' + " + val + " + '</b>\\n';");
        else if (style == "italic")
            emit(out, indent, "_el.innerHTML += '<i>' + " + val + " + '</i>\\n';");
        else if (style == "header")
            emit(out, indent, "_el.innerHTML += '<h2>' + " + val + " + '</h2>\\n';");
        else if (style == "link")
            emit(out, indent, "_el.innerHTML += '<a href=\"#\">' + " + val + " + '</a>\\n';");
        else
            emit(out, indent, "_print(" + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "// program halted");
    }
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        emit(out, indent, decl(res, "Function('return (' + " + expr + " + ')()')()"));
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "throw new Error(\"Preposterous: \" + String(" + msg + "));");
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear();
        std::string jsParams = params;
        if (!classOwner.empty()) {
            if (jsParams.rfind("self, ", 0) == 0) jsParams = jsParams.substr(6);
            else if (jsParams == "self") jsParams = "";
        }
        std::string jsName = (!classOwner.empty() && name == "init") ? "constructor" : name;
        if (classOwner.empty())
            emit(out, indent, "function " + jsName + "(" + jsParams + ") {");
        else
            emit(out, indent, jsName + "(" + jsParams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math_pi" || v == "math_e" || v == "math_tau" || v == "math_em" || v == "math_inf"
            || v.rfind("math_", 0) == 0
            || v.rfind("stat_", 0) == 0
            || v.rfind("ac_",   0) == 0;
    }
    bool isFloatVal(const std::string &v) const {
        return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v);
    }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imports) override
    {
        pendingImports_ = imports;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "/* Generated by AC Compiler (AC->C) */");
        emitRaw(out, "#include <stdio.h>");
        emitRaw(out, "#include <stdlib.h>");
        emitRaw(out, "#include <string.h>");
        emitRaw(out, "#include <stdint.h>");
        // Emit ilib C headers at file scope before main()
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                emitRaw(out, "#include \"library/" + ln + "/" + ln + "_c.h\"");
                emitRaw(out, "// Link: gcc output.c -L./library/" + ln + " -lac" + ln);
            }
        }
        emitRaw(out, "");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;\n");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "1", "0", "NULL");
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
    // Returns true for function names known to return double (math library pattern)
    static bool isFloatReturningFunc(const std::string &fn) {
        if (isIntReturningMathFunc(fn.c_str())) return false;
        if (fn.rfind("math_", 0) == 0) return true;   // math_sin, math_sqrt, ...
        if (fn.rfind("stat_", 0) == 0) return true;   // stat_avg, stat_median, ...
        if (fn.rfind("ac_",   0) == 0) return true;   // ac_sin, ac_sqrt, ...
        return false;
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isFloatReturningFunc(func) && declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = " + call + ";");
        } else {
            emit(out, indent, decl(res, call));
        }
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!val.empty()) emit(out, indent, "return " + val + ";");
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        if (declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = ac_eval(" + expr + ");");
        } else {
            emit(out, indent, res + " = ac_eval(" + expr + ");");
        }
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "fprintf(stderr, \"Preposterous: %s\\n\", " + msg + "); exit(1);");
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

    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for (size_t _fi = 0; _fi < sizeof(" + collection + ")/sizeof(" + collection + "[0]); _fi++) {");
        emit(out, indent + 1, "ac_int " + iterVar + " = " + collection + "[_fi];");
        declared.insert(iterVar);
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict")
            emit(out, indent, "/* dict " + var + " = {" + content + "} */");
        else
            emit(out, indent, "ac_int " + var + "[] = {" + content + "};");
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[" + idx + "]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val + ";");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "printf(\"%s\", " + prompt + ");");
        emit(out, indent, "char _buf_" + result + "[4096];");
        emit(out, indent, "fgets(_buf_" + result + ", sizeof(_buf_" + result + "), stdin);");
        emit(out, indent, "_buf_" + result + "[strcspn(_buf_" + result + ", \"\\n\")] = 0;");
        emit(out, indent, "ac_str " + result + " = _buf_" + result + ";");
        declared.insert(result);
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        // C methods: ClassName_methodName(ClassName* self, ...)
        std::string cParams = params;
        std::string cName   = name;
        if (!classOwner.empty()) {
            std::string selfParam = classOwner + "* self";
            if (cParams.rfind("self, ", 0) == 0)
                cParams = selfParam + ", " + cParams.substr(6);
            else if (cParams == "self")
                cParams = selfParam;
            else
                cParams = selfParam + (cParams.empty() ? "" : ", " + cParams);
            cName = (name == "init") ? classOwner + "_init" : classOwner + "_" + name;
        }
        declareParams(params, declared);
        std::string tparams = !classOwner.empty() ? cParams : typedParams(params, "ac_int");
        if (tparams.empty()) tparams = "void";
        emit(out, indent, "ac_int " + cName + "(" + tparams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        // C: opaque struct — field defaults can't be struct initialisers, suppress body
        currentClass_ = name;
        emitRaw(out, "/* bundle " + name + " */");
        emitRaw(out, "typedef struct " + name + " { long long _tag; } " + name + ";");
        emitRaw(out, "");
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        currentClass_ = "";
        (void)indent;
    }
    bool suppressClassBody() const override { return true; }
    std::string currentClass_;
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib handled in emitHeader via setPendingImports
        if (libType == "clib")
            emitRaw(out, "#include <" + libName + ".h>");
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        emit(out, indent, "int main()");
        emit(out, indent, "{");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imports) override
    {
        pendingImports_ = imports;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->CPP)");
        emitRaw(out, "#include <iostream>");
        emitRaw(out, "#include <string>");
        emitRaw(out, "#include <vector>");
        emitRaw(out, "#include <map>");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "#include <cstdlib>");
        // Emit ilib includes at file scope before main()
        for (auto& [lt, ln] : pendingImports_)
            emitRaw(out, "#include \"library/" + ln + "/" + ln + ".hpp\"");
        emitRaw(out, "");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "nullptr");
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
            emit(out, indent, "std::cout << " + val + " << \"\\n\";");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "exit(0);");
    }
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        if (declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = math_eval(" + expr + ");");
        } else {
            emit(out, indent, res + " = math_eval(" + expr + ");");
        }
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "std::cerr << \"Preposterous: \" << " + msg + " << std::endl; exit(1);");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            std::string d = "std::map<std::string,std::string> " + var + " = {";
            bool first = true;
            for (auto& [k, v] : parseDictPairs(content)) {
                if (!first) d += ", ";
                d += "{" + fmtDictKey(k) + ", " + fmtDictVal(v) + "}";
                first = false;
            }
            emit(out, indent, d + "};");
            declared.insert(var);
        } else {
            emit(out, indent, "std::vector<long long> " + var + " = {" + content + "};");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[" + idx + "]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val + ";");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "std::cout << " + prompt + ";");
        emit(out, indent, "std::string " + result + "; std::getline(std::cin, " + result + ");");
        declared.insert(result);
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        std::string cppParams = params;
        std::string cppName   = name;
        if (!classOwner.empty()) {
            if (cppParams.rfind("self, ", 0) == 0) cppParams = cppParams.substr(6);
            else if (cppParams == "self") cppParams = "";
            cppName = (name == "init") ? classOwner : name; // init → constructor
        }
        declareParams(cppParams, declared);
        std::string tparams = typedParams(cppParams, "long long");
        std::string retType = (!classOwner.empty() && name == "init") ? "" : "long long ";
        emit(out, indent, retType + cppName + "(" + tparams + ") {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        emit(out, indent, "public:");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "};");
        emitRaw(out, "");
    }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib handled in emitHeader via setPendingImports
        if (libType == "clib")
            emitRaw(out, "#include <" + libName + ">");
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        const char* s = v.c_str();
        if (strncmp(s, "AcMath.", 7) == 0) s += 7;
        if (isIntReturningMathFunc(s)) return false;
        return strncmp(s, "math_", 5) == 0 || strncmp(s, "stat_", 5) == 0;
    }
    static bool isFloatReturningFunc(const std::string &fn) {
        const char* s = fn.c_str();
        if (strncmp(s, "AcMath.", 7) == 0) s += 7;
        if (isIntReturningMathFunc(s)) return false;
        return strncmp(s, "math_", 5) == 0 || strncmp(s, "stat_", 5) == 0;
    }
    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setOutputStem(const std::string &stem) override { className = stem; }
    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->Java)");
        // ilib FFI classes emitted before main class (Java allows multiple non-public classes per file)
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string ffi = readFFIFile(ln, "java");
                if (!ffi.empty()) out << ffi << "\n";
            }
        }
        emitRaw(out, "public class " + className + " {\n");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "}  // class " + className);
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym);
        if (r.kind == IRRef::Kind::VAR && isKnownFloatName(s))
            return "AcMath." + s;
        return s;
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
        // func already has AcMath. prefix from formatRef (function name is VAR kind)
        std::string call = func + "(" + args + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isFloatReturningFunc(func) && declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = " + call + ";");
        } else {
            emit(out, indent, decl(res, call));
        }
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        if (declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = AcMath.math_eval(" + expr + ");");
        } else {
            emit(out, indent, res + " = AcMath.math_eval(" + expr + ");");
        }
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "throw new RuntimeException(\"Preposterous: \" + " + msg + ");");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            emit(out, indent, "java.util.Map<String,String> " + var + " = new java.util.HashMap<>();");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + ".put(" + fmtDictKey(k) + ", " + fmtDictVal(v) + ");");
            declared.insert(var);
        } else {
            emit(out, indent, "long[] " + var + " = {" + content + "};");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[(int)(" + idx + ")]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[(int)(" + idx + ")] = " + val + ";");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "System.out.print(" + prompt + ");");
        emit(out, indent, "String " + result + " = new java.util.Scanner(System.in).nextLine();");
        declared.insert(result);
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        std::string jParams = params;
        std::string jName   = name;
        bool isConstructor  = !classOwner.empty() && name == "init";
        if (!classOwner.empty()) {
            if (jParams.rfind("self, ", 0) == 0) jParams = jParams.substr(6);
            else if (jParams == "self") jParams = "";
            jName = isConstructor ? classOwner : name;
        }
        declareParams(jParams, declared);
        std::string tparams = typedParams(jParams, "long");
        std::string sig = isConstructor
            ? classOwner + "(" + tparams + ")"
            : "static long " + jName + "(" + tparams + ")";
        emit(out, indent, sig + " {");
        indent++;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
    }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (FFI class inlined there)
        if (libType == "elib")
            emitRaw(out, "import " + libName + ";");
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math_pi" || v == "math_e" || v == "math_tau" || v == "math_em" || v == "math_inf"
            || v.rfind("math_", 0) == 0
            || v.rfind("stat_", 0) == 0;
    }
    bool isFloatVal(const std::string &v) const {
        return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v);
    }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->RS)");
        emitRaw(out, "#![allow(unused_variables, unused_mut, unused_assignments, non_snake_case)]");
        emitRaw(out, "use std::io::Write;");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string ffi = readFFIFile(ln, "rs");
                if (!ffi.empty()) { emitRaw(out, ""); out << ffi; }
            }
        }
        emitRaw(out, "");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "None");
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
    // Ensure a value string is usable as f64 in a Rust expression.
    // Numeric integer literals get ".0" appended; variables/exprs get "as f64".
    static std::string toRustF64(const std::string &v) {
        if (looksFloat(v)) return v;
        if (looksNumeric(v)) return v + ".0";  // e.g. "0" → "0.0"
        return "(" + v + ") as f64";
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool lhsFloat = isFloatVal(lhs), rhsFloat = isFloatVal(rhs);
        bool isFloat = lhsFloat || rhsFloat;
        // In Rust, mixing integer and float types in arithmetic is a compile error.
        // When the result is float, ensure both operands are f64.
        std::string elhs = (isFloat && !lhsFloat) ? toRustF64(lhs) : lhs;
        std::string erhs = (isFloat && !rhsFloat) ? toRustF64(rhs) : rhs;
        std::string expr = elhs + " " + op + " " + erhs;
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
    static bool isFloatReturningFunc(const std::string &fn) {
        if (isIntReturningMathFunc(fn.c_str())) return false;
        if (fn.rfind("math_", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;
        return false;
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        // math_to_int takes f64 — cast integer args explicitly
        std::string actualArgs = (func == "math_to_int") ? args + " as f64" : args;
        std::string call = func + "(" + actualArgs + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isFloatReturningFunc(func) && declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "let mut " + res + ": f64 = " + call + ";");
        } else {
            emit(out, indent, decl(res, call));
        }
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        lastWasReturn = false;
        if (declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "let mut " + res + ": f64 = math_eval(" + expr + ".as_str());");
        } else {
            emit(out, indent, res + " = math_eval(" + expr + ".as_str());");
        }
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        lastWasReturn = false;
        emit(out, indent, "eprintln!(\"Preposterous: {}\", " + msg + "); std::process::exit(1);");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            emit(out, indent, "let mut " + var + ": std::collections::HashMap<&str,&str> = std::collections::HashMap::new();");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + ".insert(" + fmtDictKey(k) + ", " + fmtDictVal(v) + ");");
            declared.insert(var);
        } else {
            emit(out, indent, "let mut " + var + " = vec![" + content + "];");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[(" + idx + ") as usize]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[(" + idx + ") as usize] = " + val + ";");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "print!(\"{}\", " + prompt + ");");
        emit(out, indent, "let mut _buf_" + result + " = String::new();");
        emit(out, indent, "std::io::stdin().read_line(&mut _buf_" + result + ").unwrap();");
        emit(out, indent, "let " + result + " = _buf_" + result + ".trim().to_string();");
        declared.insert(result);
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        lastWasReturn = false;
        std::string rParams = params;
        std::string rName   = name;
        if (!classOwner.empty()) {
            if (rParams.rfind("self, ", 0) == 0) rParams = rParams.substr(6);
            else if (rParams == "self") rParams = "";
            rName = (name == "init") ? "new" : name;
        }
        declareParams(rParams, declared);
        std::string tparams = rustMutTypedParams(rParams, "i64");
        bool isNew = !classOwner.empty() && name == "init";
        if (isNew) {
            emit(out, indent, "pub fn new(" + tparams + ") -> Self {");
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "pub fn " + rName + "(&mut self" + sep + tparams + ") -> i64 {");
        } else {
            emit(out, indent, "fn " + rName + "(" + tparams + ") -> i64 {");
        }
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
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "struct " + name + " { _tag: i64 }");
        emitRaw(out, "");
        emit(out, indent, "impl " + name + " {");
        indent++;
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
    }
    bool suppressClassBody() const override { return true; } // field defaults can't go inside impl
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (FFI file inlined there)
        if (libType == "elib")
            emitRaw(out, "extern crate " + libName + ";");
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
    bool needsInput_ = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    // Relative path from output file's directory to project root (e.g. ".." for examples/)
    std::string relRoot_ = ".";

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math_pi" || v == "math_e" || v == "math_tau" || v == "math_em" || v == "math_inf"
            || v.rfind("math_", 0) == 0
            || v.rfind("stat_", 0) == 0;
    }
    static bool isFloatReturningFunc(const std::string &fn) {
        if (isIntReturningMathFunc(fn.c_str())) return false;
        if (fn.rfind("math_", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;
        return false;
    }
    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsInput(bool v) override { needsInput_ = v; }
    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }
    void setOutputBase(const std::string &base) override
    {
        // Count directory components in 'base' to build the relative root path.
        // e.g. "examples/foo" has 1 slash → 1 depth → relRoot_ = ".."
        //      "foo" has 0 slashes → depth 0 → relRoot_ = "."
        int depth = 0;
        for (char c : base)
            if (c == '/' || c == '\\') depth++;
        relRoot_ = ".";
        for (int i = 0; i < depth; i++)
            relRoot_ = relRoot_ == "." ? ".." : "../" + relRoot_;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->GO)");
        emitRaw(out, "package main");
        // CGO blocks must appear between package and regular import
        bool hasCGO = false;
        std::vector<std::string> wrapperBlocks;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string cgoBlock, wrappers;
                parseGoFFI(ln, cgoBlock, wrappers);
                if (!cgoBlock.empty()) {
                    // Replace ${SRCDIR}/library/ln with ${SRCDIR}/<relRoot_>/library/ln
                    // so the path resolves correctly regardless of where the .go file lives.
                    std::string from = "${SRCDIR}/library/" + ln;
                    std::string to   = "${SRCDIR}/" + relRoot_ + "/library/" + ln;
                    size_t pos = 0;
                    while ((pos = cgoBlock.find(from, pos)) != std::string::npos) {
                        cgoBlock.replace(pos, from.size(), to);
                        pos += to.size();
                    }
                    hasCGO = true;
                    out << "\n" << cgoBlock;
                    wrapperBlocks.push_back(wrappers);
                }
            }
        }
        emitRaw(out, "");
        emitRaw(out, "import (");
        if (needsInput_) emitRaw(out, "    \"bufio\"");
        emitRaw(out, "    \"fmt\"");
        emitRaw(out, "    \"os\"");
        if (needsInput_) emitRaw(out, "    \"strings\"");
        if (hasCGO)      emitRaw(out, "    \"unsafe\"");
        emitRaw(out, ")\n");
        emitRaw(out, "func _b(b bool) int64 { if b { return 1 }; return 0 }\n");
        for (auto& w : wrapperBlocks)
            if (!w.empty()) out << w;
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "nil");
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
        // math_to_int takes float64 — cast integer args explicitly
        std::string actualArgs = (func == "math_to_int") ? "float64(" + args + ")" : args;
        std::string call = func + "(" + actualArgs + ")";
        if (res.empty()) {
            emit(out, indent, call);
        } else if (isFloatReturningFunc(func) && declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "var " + res + " float64 = " + call);
        } else {
            emit(out, indent, decl(res, call));
        }
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // Suppress bare empty returns — they are IR safety fallthroughs, not real returns.
        // Go functions that always exit via os.Exit or explicit return don't need them.
        if (!val.empty())
            emit(out, indent, "return " + val);
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "fmt.Println(" + val + ")");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "os.Exit(0)");
    }
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        if (declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "var " + res + " float64 = math_eval(" + expr + ")");
        } else {
            emit(out, indent, res + " = math_eval(" + expr + ")");
        }
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "fmt.Fprintf(os.Stderr, \"Preposterous: %s\\n\", " + msg + "); os.Exit(1)");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            emit(out, indent, var + " := map[string]string{}");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + "[" + fmtDictKey(k) + "] = " + fmtDictVal(v));
            declared.insert(var);
        } else {
            emit(out, indent, var + " := []int64{" + content + "}");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[" + idx + "]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val);
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "fmt.Print(" + prompt + ")");
        emit(out, indent, "_rdr_" + result + " := bufio.NewReader(os.Stdin)");
        emit(out, indent, "_raw_" + result + ", _ := _rdr_" + result + ".ReadString('\\n')");
        emit(out, indent, decl(result, "strings.TrimRight(_raw_" + result + ", \"\\r\\n\")"));
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        std::string gParams = params;
        std::string gName   = name;
        if (!classOwner.empty()) {
            if (gParams.rfind("self, ", 0) == 0) gParams = gParams.substr(6);
            else if (gParams == "self") gParams = "";
            gName = (name == "init") ? "New" + classOwner : name;
        }
        declareParams(gParams, declared);
        std::string tparams = goTypedParams(gParams, "int64");
        bool isNew = !classOwner.empty() && name == "init";
        if (isNew) {
            emit(out, indent, "func New" + classOwner + "(" + tparams + ") *" + classOwner + " {");
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "func (self *" + classOwner + ") " + gName + "(" + tparams + ") int64 {");
        } else {
            emit(out, indent, "func " + gName + "(" + tparams + ") int64 {");
        }
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
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "type " + name + " struct { _tag int64 }");
        emitRaw(out, "");
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        (void)out; (void)indent;
    }
    bool suppressClassBody() const override { return true; }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (CGO FFI inlined there)
        if (libType == "elib")
            emitRaw(out, "import \"" + libName + "\"");
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->V)");
        emitRaw(out, "module main");
        emitRaw(out, "import os");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string content = readFFIFile(ln, "v");
                if (!content.empty()) {
                    // Skip any leading "module" line — we're already in module main
                    std::istringstream ss(content);
                    std::string line2;
                    bool first = true;
                    out << "\n";
                    while (std::getline(ss, line2)) {
                        if (first && line2.rfind("module ", 0) == 0) { first = false; continue; }
                        first = false;
                        out << line2 << "\n";
                    }
                }
            }
        }
        emitRaw(out, "");
    }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "none");
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
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        lastWasReturn = false;
        emit(out, indent, "mut " + res + " := f64(0) /* eval(" + expr + ") not supported in V backend */");
        declared.insert(res);
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        lastWasReturn = false;
        emit(out, indent, "eprintln('Preposterous: ' + " + msg + "); exit(1)");
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
                   const std::string &var, const std::string &type,
                   const std::string &content) override
    {
        if (type == "dict") {
            emit(out, indent, "mut " + var + " := map[string]string{}");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + "[" + fmtDictKey(k) + "] = " + fmtDictVal(v));
            declared.insert(var);
        } else {
            // V requires explicit type on first element
            std::string c = content;
            size_t comma = c.find(',');
            std::string vContent = comma != std::string::npos
                                       ? "i64(" + c.substr(0, comma) + ")" + c.substr(comma)
                                       : "i64(" + c + ")";
            emit(out, indent, "mut " + var + " := [" + vContent + "]");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        emit(out, indent, decl(result, arr + "[" + idx + "]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        emit(out, indent, arr + "[" + idx + "] = " + val);
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, decl(result, "os.input(" + prompt + ")"));
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear();
        lastWasReturn = false;
        std::string vParams = params;
        std::string vName   = name;
        if (!classOwner.empty()) {
            if (vParams.rfind("self, ", 0) == 0) vParams = vParams.substr(6);
            else if (vParams == "self") vParams = "";
            vName = (name == "init") ? "init" : name;
        }
        std::string tparams = suffixTypedParams(vParams, "i64");
        if (!classOwner.empty())
            emit(out, indent, "fn (" + classOwner + ") " + vName + "(" + tparams + ") i64 {");
        else
            emit(out, indent, "fn " + vName + "(" + tparams + ") i64 {");
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
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "struct " + name + " { _tag i64 }");
        emitRaw(out, "");
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        (void)out; (void)indent;
    }
    bool suppressClassBody() const override { return true; }
    void emitLibImport(std::ostringstream &out, const std::string &libType,
                       const std::string &libName) override
    {
        // ilib: handled in emitHeader via setPendingImports (FFI file inlined there)
        if (libType == "elib")
            emitRaw(out, "import " + libName);
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
    std::vector<std::pair<std::string,std::string>> pendingImports_;

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

    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "; Generated by AC Compiler (AC->ASM)");
        emitRaw(out, "; x86-64 Linux NASM — assemble: nasm -f elf64 out.s && gcc out.o -o out");
        emitRaw(out, "");
        emitRaw(out, "    default rel");
        emitRaw(out, "    extern printf, exit");
        // Emit extern declarations for ilib functions
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string content = readFFIFile(ln, "asm");
                if (!content.empty()) {
                    std::istringstream ss(content);
                    std::string aline;
                    while (std::getline(ss, aline)) {
                        if (aline.rfind("extern ", 0) == 0)
                            emitRaw(out, "    " + aline);
                    }
                }
            }
        }
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
        return commonRef(r, sym, "1", "0", "0");
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
    void emitEval(std::ostringstream &out, int & /*indent*/,
                  const std::string &res, const std::string &expr) override
    {
        out << "    ; eval(" << expr << ") — not implemented in ASM backend\n";
        if (!res.empty()) out << "    mov qword [rbp-" << getSlot(res) << "], 0\n";
    }
    void emitRaise(std::ostringstream &out, int & /*indent*/, const std::string &) override
    {
        out << "    mov edi, 1\n";
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
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        slot.clear();
        nextSlot = 0;
        (void)params; (void)indent;
        std::string label = classOwner.empty() ? name : classOwner + "_" + name;
        if (!classOwner.empty() && name == "init") label = classOwner + "_init";
        emitRaw(out, "");
        emitRaw(out, label + ":");
        out << "    push rbp\n    mov rbp, rsp\n    sub rsp, 128\n";
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        (void)indent;
        out << "    xor eax, eax\n    leave\n    ret\n";
        slot.clear();
        nextSlot = 0;
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        (void)indent;
        emitRaw(out, "; bundle " + name);
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        (void)out; (void)indent;
    }
    bool suppressClassBody() const override { return true; }
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
    std::set<std::string> globalVarNames_;

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

        case IROpcode::LOAD_INDEX:
            if (i.typedOperands.size() >= 2)
                strategy->emitLoadIndex(out, indentLevel, ref(i.result),
                                        ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            break;

        case IROpcode::STORE_INDEX:
            if (i.typedOperands.size() >= 3)
                strategy->emitStoreIndex(out, indentLevel,
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]),
                                         ref(i.typedOperands[2]));
            break;

        case IROpcode::INPUT:
            strategy->emitInput(out, indentLevel, ref(i.result),
                                i.typedOperands.empty() ? "\"\"" : ref(i.typedOperands[0]));
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
            if (!strategy->dotCallSyntax())
                for (char& c : func) if (c == '.') c = '_';
            std::string args;
            for (size_t j = 1; j < i.typedOperands.size(); j++)
            {
                if (j > 1)
                    args += ", ";
                args += ref(i.typedOperands[j]);
            }
            std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
            strategy->emitCall(out, indentLevel, res, func, args);
            break;
        }

        case IROpcode::RETURN:
            strategy->emitReturn(out, indentLevel,
                                 i.typedOperands.empty() ? "" : ref(i.typedOperands[0]));
            break;

        case IROpcode::EVAL:
        {
            std::string expr = i.typedOperands.empty() ? "\"\"" : ref(i.typedOperands[0]);
            strategy->emitEval(out, indentLevel, ref(i.result), expr);
            break;
        }

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
            // Resolve styled display variants: bold.display, italic.display, etc.
            auto extractStyle = [](const std::string& m) -> std::string {
                static const std::vector<std::string> styles =
                    {"bold", "italic", "header", "link", "title"};
                for (auto& s : styles)
                    if (m == s + ".display" || m == "\"" + s + ".display\"")
                        return s;
                return "";
            };
            std::string displayStyle = extractStyle(method);

            if ((method == "\"Term.display\"" || method == "Term.display") &&
                i.typedOperands.size() > 1)
            {
                strategy->emitPrint(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if (!displayStyle.empty() && i.typedOperands.size() > 1)
            {
                strategy->emitStyledPrint(out, indentLevel, ref(i.typedOperands[1]), displayStyle);
            }
            else if ((method == "\"import\"" || method == "import") &&
                     i.typedOperands.size() > 1)
            {
                // use ilib/elib/clib <name> — emit backend-specific import
                std::string raw = stripQuotes(ref(i.typedOperands[1]));
                std::string libType = "ilib";
                std::string libName = raw;
                auto colon = raw.find(':');
                if (colon != std::string::npos) {
                    libType = raw.substr(0, colon);
                    libName = raw.substr(colon + 1);
                }
                strategy->emitLibImport(out, libType, libName);
            }
            else if ((method == "\"foreign\"" || method == "foreign") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitForeign(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if ((method == "\"raise\"" || method == "raise") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitRaise(out, indentLevel, ref(i.typedOperands[1]));
            }
            else
            {
                // Generic lib/object call.
                // dotCallSyntax backends (Python): preserve dots — math.sin(x), d.bark().
                // Other backends: replace dots with underscores — math_sin(x).
                std::string func = stripQuotes(method);
                if (!strategy->dotCallSyntax())
                    for (char& c : func) if (c == '.') c = '_';
                std::string args;
                for (size_t j = 1; j < i.typedOperands.size(); j++)
                {
                    if (j > 1)
                        args += ", ";
                    args += ref(i.typedOperands[j]);
                }
                std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
                strategy->emitCall(out, indentLevel, res, func, args);
            }
            break;
        }

        case IROpcode::EVENT_BIND:
            if (i.typedOperands.size() >= 2)
                strategy->emitEventBind(out, indentLevel,
                                        ref(i.typedOperands[0]),
                                        ref(i.typedOperands[1]));
            break;

        case IROpcode::EVENT_TRIGGER:
            if (!i.typedOperands.empty())
                strategy->emitEventTrigger(out, indentLevel, ref(i.typedOperands[0]));
            break;

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
        std::set<std::string> paramSet(func.parameters.begin(), func.parameters.end());
        for (size_t i = 0; i < func.parameters.size(); i++)
        {
            if (i > 0)
                params += ", ";
            params += func.parameters[i];
        }
        // Tell strategy which global vars this function writes (for Python `global` decls)
        if (!globalVarNames_.empty())
        {
            std::vector<std::string> pendingGlobals;
            std::set<std::string> seen;
            for (const auto &instr : func.instructions)
                if (instr.opcode == IROpcode::STORE_VAR && instr.result.kind == IRRef::Kind::VAR)
                {
                    std::string name = ref(instr.result);
                    if (!paramSet.count(name) && globalVarNames_.count(name) && seen.insert(name).second)
                        pendingGlobals.push_back(name);
                }
            strategy->setPendingGlobals(pendingGlobals);
        }
        strategy->emitFunctionBegin(out, indentLevel, func.name, params, func.classOwner);
        for (const auto &instr : func.instructions)
            genInstr(instr);
        strategy->emitFunctionEnd(out, indentLevel);
    }

public:
    explicit UnifiedIRCodeGen(const IRProgram &program, const std::string &stem = "Main",
                               const std::string &outputBase = "") : ir(program)
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
        strategy->setOutputBase(outputBase.empty() ? stem : outputBase);
    }

    std::string generate()
    {
        // Pre-scan: collect global var names, check for INPUT / EVENT_BIND / LIB_CALL imports
        bool hasInput  = false;
        bool hasEvents = false;
        std::vector<std::pair<std::string,std::string>> libImports;
        std::set<std::string> seenImports;
        std::unordered_map<std::string,std::set<std::string>> importSymbols; // libKey → {sym,...}

        auto scanImport = [&](const IRInstruction& ins) {
            if (ins.opcode != IROpcode::LIB_CALL || ins.typedOperands.empty()) return;
            std::string method = ref(ins.typedOperands[0]);
            if ((method == "\"import\"" || method == "import") && ins.typedOperands.size() > 1) {
                std::string raw = stripQuotes(ref(ins.typedOperands[1]));
                std::string libType = "ilib", libName = raw;
                auto colon = raw.find(':');
                if (colon != std::string::npos) {
                    libType = raw.substr(0, colon);
                    libName = raw.substr(colon + 1);
                }
                if (seenImports.insert(raw).second)
                    libImports.push_back({libType, libName});
                // Third operand: selective symbol list (from ilib X use a,b,c)
                if (ins.typedOperands.size() > 2) {
                    std::string symList = stripQuotes(ref(ins.typedOperands[2]));
                    auto& symSet = importSymbols[raw];
                    // parse comma-separated
                    std::istringstream ss(symList);
                    std::string sym;
                    while (std::getline(ss, sym, ',')) {
                        if (!sym.empty()) symSet.insert(sym);
                    }
                }
            }
        };

        for (auto& fn : ir.functions)
            for (auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::INPUT)      hasInput  = true;
                if (ins.opcode == IROpcode::EVENT_BIND) hasEvents = true;
                scanImport(ins);
            }
        for (auto& ins : ir.globalInit) {
            if (ins.opcode == IROpcode::INPUT)      hasInput  = true;
            if (ins.opcode == IROpcode::EVENT_BIND) hasEvents = true;
            if (ins.opcode == IROpcode::STORE_VAR && ins.result.kind == IRRef::Kind::VAR)
                globalVarNames_.insert(ref(ins.result));
            scanImport(ins);
        }
        strategy->setNeedsInput(hasInput);
        strategy->setNeedsEvents(hasEvents);
        strategy->setPendingImports(libImports);
        strategy->setImportSymbols(importSymbols);

        strategy->emitHeader(out);

        // Emit free (non-method) functions
        for (const auto &func : ir.functions)
            if (func.classOwner.empty()) genFunction(func);

        // First pass over globalInit: emit class definitions (with field defaults and methods)
        {
            bool inClass = false;
            std::string curClass;
            for (const auto &instr : ir.globalInit) {
                if (instr.opcode == IROpcode::CLASS_BEGIN) {
                    curClass = stripQuotes(ref(instr.typedOperands[0]));
                    strategy->emitClassBegin(out, indentLevel, curClass);
                    inClass = true;
                } else if (instr.opcode == IROpcode::CLASS_END) {
                    // Emit methods for this class before closing
                    for (const auto &func : ir.functions)
                        if (func.classOwner == curClass) genFunction(func);
                    strategy->emitClassEnd(out, indentLevel);
                    inClass = false; curClass = "";
                } else if (inClass && !strategy->suppressClassBody()) {
                    genInstr(instr); // field defaults inside class body
                }
            }
        }

        // Second pass: emit main body (skip class blocks)
        strategy->emitMainBegin(out, indentLevel);
        {
            bool inClass = false;
            for (const auto &instr : ir.globalInit) {
                if (instr.opcode == IROpcode::CLASS_BEGIN) { inClass = true;  continue; }
                if (instr.opcode == IROpcode::CLASS_END)   { inClass = false; continue; }
                if (!inClass) genInstr(instr);
            }
        }
        strategy->emitMainEnd(out, indentLevel);

        strategy->emitFooter(out);
        return out.str();
    }
};

// ─── public API ──────────────────────────────────────────────────────────────

std::string generateFromIR(const IRProgram &ir, const std::string &stem,
                           const std::string &outputBase)
{
    UnifiedIRCodeGen gen(ir, stem, outputBase);
    return gen.generate();
}
