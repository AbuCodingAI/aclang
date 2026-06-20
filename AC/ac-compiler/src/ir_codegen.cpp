#include "../include/ac.hpp"
#include <sstream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cmath>
#ifdef _WIN32
#include <direct.h>
#define ac_getcwd _getcwd
#else
#include <unistd.h>
#include <sys/stat.h>
#define ac_getcwd getcwd
#endif

using namespace AC_IR;

// ─── FFI file reader ─────────────────────────────────────────────────────────
// Reads library/<libName>/ffi/<libName>_ffi.<ext>.
// Tries cwd-relative first, then binary-relative so "ac examples/foo.ac" works.
// Optional out-param foundLibDir: set to the absolute library/<libName> directory.
static std::string readFFIFile(const std::string &libName, const std::string &ext,
                               std::string *foundLibDir = nullptr)
{
    auto tryBase = [&](const std::string &base) -> std::string {
        std::string path = base + "/library/ilib/" + libName + "/ffi/" + libName + "_ffi." + ext;
        FILE *f = std::fopen(path.c_str(), "r");
        if (!f) return "";
        std::string content;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), f)) content += buf;
        std::fclose(f);
        if (foundLibDir) {
            std::string rawDir = base + "/library/ilib/" + libName;
#ifndef _WIN32
            char realBuf[4096] = {};
            *foundLibDir = realpath(rawDir.c_str(), realBuf) ? std::string(realBuf) : rawDir;
#else
            char realBuf[4096] = {};
            *foundLibDir = _fullpath(realBuf, rawDir.c_str(), sizeof(realBuf)) ? std::string(realBuf) : rawDir;
#endif
        }
        return content;
    };

    // 1. cwd-relative (works when run from project root)
    std::string result = tryBase(".");
    if (!result.empty()) return result;

#ifndef _WIN32
    // 2. binary-relative: <bindir>/../ (works when "ac" is run from any directory)
    char exeBuf[4096] = {};
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
    if (len > 0) {
        exeBuf[len] = '\0';
        std::string binDir(exeBuf);
        auto slash = binDir.rfind('/');
        if (slash != std::string::npos) binDir = binDir.substr(0, slash);
        result = tryBase(binDir + "/..");
        if (!result.empty()) return result;
    }
#endif

    return "";
}

// Returns the absolute path of the ilib directory for a given library name.
// Used to emit absolute -L and #include paths in C/C++ generated code.
static std::string resolveIlibDir(const std::string& libName) {
    auto tryBase = [&](const std::string& base) -> std::string {
        std::string raw = base + "/library/ilib/" + libName;
#ifndef _WIN32
        struct stat st{};
        if (::stat(raw.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return "";
        char buf[4096] = {};
        return realpath(raw.c_str(), buf) ? std::string(buf) : raw;
#else
        // Windows: just return the path if it exists (simplified check)
        char buf[4096] = {};
        return _fullpath(buf, raw.c_str(), sizeof(buf)) ? std::string(buf) : raw;
#endif
    };
    std::string r = tryBase(".");
    if (!r.empty()) return r;
#ifndef _WIN32
    char exeBuf[4096] = {};
    ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf)-1);
    if (len > 0) {
        std::string bd(exeBuf, len);
        auto sl = bd.rfind('/'); if (sl != std::string::npos) bd = bd.substr(0, sl);
        r = tryBase(bd + "/..");
        if (!r.empty()) return r;
    }
#endif
    return "./library/ilib/" + libName; // relative fallback
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
    if (std::isinf(d)) return d < 0 ? "-1e309" : "1e309";
    if (std::isnan(d)) return "0.0/0.0";
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
    // Handle both underscore-style (math_gcd) and dot-style (math.gcd)
    const char* name = s;
    if (strncmp(s, "math.", 5) == 0) name = s + 5;
    else if (strncmp(s, "math_", 5) == 0) name = s + 5;
    return strcmp(name, "to_int")   == 0
        || strcmp(name, "abs_int")  == 0
        || strcmp(name, "mod_int")  == 0
        || strcmp(name, "gcd")      == 0
        || strcmp(name, "lcm")      == 0
        || strcmp(name, "is_prime") == 0;
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
static std::string suffixTypedParams(const std::string &params, const std::string &typeName,
                                     const std::string &prefix = "")
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
        result += (prefix.empty() ? "" : prefix + " ") + name + " " + typeName;
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
                             const std::string &trueVal      = "true",
                             const std::string &falseVal     = "false",
                             const std::string &nullVal      = "null",
                             const std::string &nilVal       = "{}",
                             bool preserveDots               = true,
                             const std::string &wsPatternVal = "\" \\t\\n\\r\"")
{
    if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::FUNCTION) {
        std::string name = r.toStringWithSymbols(sym);
        if (!preserveDots)
            for (char& c : name) if (c == '.') c = '_';
        return name;
    }
    if (r.kind == IRRef::Kind::TEMP)
        return "t_" + std::to_string(r.id);
    if (r.kind == IRRef::Kind::LABEL)
        return "L" + std::to_string(r.id);
    if (r.kind == IRRef::Kind::CONST)
    {
        if (r.value.type == IRType::INT)
            return std::to_string(std::get<int64_t>(r.value.data));
        if (r.value.type == IRType::FLOAT)
            return fmtDouble(std::get<double>(r.value.data));
        if (r.value.type == IRType::BOOL)
            return std::get<bool>(r.value.data) ? trueVal : falseVal;
        if (r.value.type == IRType::STRING)
        {
            std::string s = std::get<std::string>(r.value.data);
            if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
                s = s.substr(1, s.size() - 2);
            if (s == "null") return nullVal;
            if (s == "nil")  return nilVal;
            // \ws escape: translate to per-backend whitespace string
            if (s == "__WS__") return wsPatternVal;
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
    // Convert a function name to backend-native style (e.g. dots→underscores for C)
    virtual std::string formatCallName(const std::string& n) const { return n; }

    virtual void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) = 0;
    // Type-aware variant: uses IRType from type system instead of heuristics.
    // Default falls back to emitStoreVar (heuristic). Override in typed backends.
    virtual void emitTypedStoreVar(std::ostringstream &out, int &indent,
                                   const std::string &var, const std::string &val, IRType t) {
        emitStoreVar(out, indent, var, val);
    }
    // const x = val — immutable binding; default falls back to regular store
    virtual void emitConstDecl(std::ostringstream &out, int &indent,
                               const std::string &var, const std::string &val, IRType t) {
        emitTypedStoreVar(out, indent, var, val, t);
    }
    // cp x = y — deep copy; default falls back to regular store (value-semantic backends)
    virtual void emitCopy(std::ostringstream &out, int &indent,
                          const std::string &dst, const std::string &src) {
        emitStoreVar(out, indent, dst, src);
    }

    // True division (5/2 = 2.5, not 2). Default: emit res = lhs / rhs (works for Python/JS).
    // Statically-typed backends override to cast operands to float before dividing.
    virtual void emitTrueDivision(std::ostringstream &out, int &indent,
                                  const std::string &res,
                                  const std::string &lhs, const std::string &rhs) {
        emitBinaryOp(out, indent, res, lhs, rhs, '/');
    }
    // Integer division (truncates toward zero). Default works for Python (//), JS (Math.trunc).
    // Statically-typed backends may override if integer division is already the native behavior.
    virtual void emitIntDiv(std::ostringstream &out, int &indent,
                            const std::string &res,
                            const std::string &lhs, const std::string &rhs) {
        // Default: call emitBinaryOp with '/' — works for languages where operands are already int
        emitBinaryOp(out, indent, res, lhs, rhs, '/');
    }
    // Integer modulo (always integer; cast floats before %). Default works for Python/JS.
    virtual void emitMod(std::ostringstream &out, int &indent,
                         const std::string &res,
                         const std::string &lhs, const std::string &rhs) {
        emitBinaryOp(out, indent, res, lhs, rhs, '%');
    }
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
    // Graceful exit: runs atexit/cleanup handlers before terminating.
    // Default falls back to emitHalt; override per backend where the calls differ.
    virtual void emitSoftHalt(std::ostringstream &out, int &indent) { emitHalt(out, indent); }
    // Pause execution for `secs` seconds (decimal OK).
    virtual void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) {}
    virtual void emitEval(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &expr) = 0;
    virtual void emitForeign(std::ostringstream &out, int &indent, const std::string &code)
    {
        // Emit raw target-language code at the current indent level.
        // Keep indentation stable so <Foreign> works inside function bodies.
        std::string raw = stripQuotes(code);
        std::istringstream ss(raw);
        std::string line;
        bool any = false;
        while (std::getline(ss, line)) {
            any = true;
            emit(out, indent, line);
        }
        if (!any) emit(out, indent, "");
    }
    // Browser-specific calls — default: silent noop (not meaningful in non-browser targets)
    virtual void emitBrowserPrint(std::ostringstream &out, int &indent)
        { (void)out; (void)indent; }
    virtual void emitAlert(std::ostringstream &out, int &indent, const std::string &val)
        { (void)out; (void)indent; (void)val; }
    // res may be empty when result is discarded; val is the prompt string
    virtual void emitConfirm(std::ostringstream &out, int &indent,
                             const std::string &res, const std::string &val)
        { (void)out; (void)indent; (void)res; (void)val; }

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
                           const std::string &content,
                           const std::string &content2 = "") {}
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
    // Called before emitFunctionBegin with vars the function declared `free`
    virtual void setFreeVars(const std::vector<std::string> &) {}
    // Called before emitHeader with all vars promoted to global scope across the program
    virtual void setPromotedGlobals(const std::vector<std::string> &) {}

    // Called before emitFunctionBegin: marks which function params should be typed as String
    virtual void setStringParams(const std::set<std::string>&) {}

    // Called before emitFunctionBegin: param name → call arity for function-typed params
    virtual void setFuncTypedParams(const std::map<std::string, int>&) {}

    // Indirect call through a variable (parameter holding a function reference)
    virtual void emitIndirectCall(std::ostringstream &out, int &indent,
                                  const std::string &res, const std::string &func,
                                  const std::string &args) {
        emitCall(out, indent, res, func, args); // default: same as direct (Python/JS/HTML)
    }

    // When a user function is passed as an argument value, how to refer to it.
    // Default: just the function name. Java overrides to emit ClassName::method.
    virtual std::string funcArgRef(const std::string &name) { return name; }

    // Pre-computed final cast types for variables (var name → final IRType after all dec/int/etc.)
    // Static backends use this to declare variables with their post-cast type from the start.
    virtual void setVarCastTypes(const std::map<std::string, IRType>&) {}

    // Type coercion: dec/int/string/bool x — converts x to the target type in-place
    // var = variable name, src = source expression, t = target IRType
    virtual void emitTypeCast(std::ostringstream &out, int &indent,
                              const std::string &var, const std::string &src, IRType t)
        { (void)out; (void)indent; (void)var; (void)src; (void)t; }

    // Raise a runtime error with the given message string
    virtual void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) {}

    // raise Clause($msg$) — print "Clause: msg" to stderr (non-fatal unless clause=="hint"/"toxic")
    // clause is the raw name; special values: "hint"→Suggestion, "toxic"→Toxic, others→verbatim
    virtual void emitRaiseClause(std::ostringstream &out, int &indent,
                                  const std::string &clause, const std::string &msg) {
        std::string prefix = clause;
        if (clause == "hint")  prefix = "Suggestion";
        else if (clause == "toxic") prefix = "Toxic";
        // Default: print to stderr. Backends override for language-specific stderr.
        emit(out, indent, "/* " + prefix + ": " + msg + " */");
    }

    // lazy_eval(expr) — evaluate expr wrapped in try/catch; result holds value or error sentinel
    virtual void emitLazyEval(std::ostringstream &out, int &indent,
                               const std::string &result, const std::string &expr) {
        // Default: just assign (dynamic backends don't need wrapping)
        emit(out, indent, result + " = " + expr);
    }

    // Tag structure — default: C-style section comment (backends override for their syntax)
    // <bound> creates a new indented scope block; <free> relies on FREE_DECL for globals
    virtual void emitTagBegin(std::ostringstream &out, int &indent, const std::string &tag) {
        if (tag == "bound") { emit(out, indent, "{ // <bound>"); indent++; }
        else                { emit(out, indent, "// <" + tag + ">"); }
    }
    virtual void emitTagEnd(std::ostringstream &out, int &indent, const std::string &tag) {
        if (tag == "bound") { indent--; emit(out, indent, "} // <bound>"); }
        else                { emit(out, indent, "// <" + tag + ">"); }
    }

    // Exception handling (try/catch/after)
    // Default no-ops: backends without native exceptions emit body sequentially
    virtual void emitTryBegin(std::ostringstream &out, int &indent)
        { (void)out; (void)indent; }
    virtual void emitCatchBegin(std::ostringstream &out, int &indent,
                                const std::string &exVar, const std::string &typeName)
        { (void)out; (void)indent; (void)exVar; (void)typeName; }
    virtual void emitAfterBegin(std::ostringstream &out, int &indent)
        { (void)out; (void)indent; }
    virtual void emitTryEnd(std::ostringstream &out, int &indent)
        { (void)out; (void)indent; }

    // Scoping: save free vars before loop entry (depth = nesting level, 0-based)
    virtual void emitScopeEnter(std::ostringstream &out, int &indent,
                                const std::vector<std::string> &vars, int depth)
        { (void)out; (void)indent; (void)vars; (void)depth; }
    // Scoping: restore free vars after loop exit
    virtual void emitScopeExit(std::ostringstream &out, int &indent,
                               const std::vector<std::string> &vars, int depth)
        { (void)out; (void)indent; (void)vars; (void)depth; }

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

    virtual void setReturnIsFloat(bool) {}  // called before emitFunctionBegin
    virtual void setFloatReturnFuncs(const std::set<std::string>&) {}  // funcs that return float
    virtual void setReturnIsList(bool) {}
    virtual void setListReturnFuncs(const std::set<std::string>&) {}
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
        emitRaw(out, "import sys, os, time");
        emitRaw(out, "from typing import Final");
        emitRaw(out, "def ac_pow(base, exp): return pow(base, exp)  # exponentiation operator ^");
        emitRaw(out, "def ac_length(x): return len(x)  # length builtin");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string foundLibDir;
                std::string ffi = readFFIFile(ln, "py", &foundLibDir);
                if (!ffi.empty()) {
                    if (!foundLibDir.empty()) {
                        std::string lnSafe = ln;
                        for (char& c : lnSafe) if (c == '-') c = '_';
                        std::string varName = "_ac_" + lnSafe + "_lib_dir";
                        out << varName << " = r'" << foundLibDir << "'\n";
                    }
                    out << ffi;
                    // Selective import: expose requested symbols at top level
                    std::string key = lt + ":" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty()) {
                        out << "# from " << ln << " use: ";
                        bool first = true;
                        for (auto& sym : it->second) { if (!first) out << ", "; out << sym; first = false; }
                        out << "\n";
                        for (auto& sym : it->second)
                            out << sym << " = " << ln << "." << sym << "\n";
                    }
                }
            } else if (lt == "flib") {
                std::string path = ln;
                auto slash = path.rfind('/');
                std::string modName = (slash == std::string::npos) ? path : path.substr(slash + 1);
                auto dot = modName.rfind('.');
                std::string ext = (dot != std::string::npos) ? modName.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    // Native shared library — load via ctypes
                    std::string libname = modName.substr(0, modName.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    out << "import ctypes as _ctypes\n";
                    out << "_flib_" << libname << " = _ctypes.CDLL(r'" << path << "')\n";
                } else {
                    // .ac/.ai flib — inject Python module path
                    if (dot != std::string::npos) modName = modName.substr(0, dot);
                    std::string dirPart = (slash == std::string::npos) ? "." : path.substr(0, slash);
                    out << "import sys as _sys; _sys.path.insert(0, r'" << dirPart << "')\n";
                    out << "import " << modName << "\n";
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
        return commonRef(r, sym, "True", "False", "None", "set()");
    }

    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        if      (t == IRType::FLOAT)  emit(out, indent, var + " = float(" + src + ")");
        else if (t == IRType::INT)    emit(out, indent, var + " = int(" + src + ")");
        else if (t == IRType::STRING) emit(out, indent, var + " = str(" + src + ")");
        else if (t == IRType::BOOL)   emit(out, indent, var + " = bool(" + src + ")");
    }
    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, var + " = " + val);
    }
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType) override
    {
        // Python has no runtime const; use Final type hint
        emit(out, indent, var + ": Final = " + val);
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    {
        emit(out, indent, dst + " = __import__('copy').deepcopy(" + src + ")");
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        // When + involves a string operand, coerce both sides to str() so Python doesn't
        // throw "can only concatenate str (not 'int') to str".
        if (op == '+' && (looksString(lhs) || looksString(rhs))) {
            auto wrap = [](const std::string& v) {
                return looksString(v) ? v : "str(" + v + ")";
            };
            emit(out, indent, res + " = " + wrap(lhs) + " + " + wrap(rhs));
        } else {
            emit(out, indent, res + " = " + lhs + " " + op + " " + rhs);
        }
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
    void emitIntDiv(std::ostringstream &out, int &indent,
                    const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, res + " = " + lhs + " // " + rhs); }
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
        emit(out, indent, "sys.stdout.flush(); os.abort()");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "sys.exit(0)");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "time.sleep(" + secs + ")");
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
    void emitTagBegin(std::ostringstream &out, int &indent, const std::string &tag) override {
        if (tag == "bound") { emit(out, indent, "if True:  # <bound>"); indent++; }
        else                { emit(out, indent, "# <" + tag + ">"); }
    }
    void emitTagEnd(std::ostringstream &out, int &indent, const std::string &tag) override {
        if (tag == "bound") { indent--; emit(out, indent, "# <bound>"); }
        else                { emit(out, indent, "# <" + tag + ">"); }
    }
    void emitForBegin(std::ostringstream &out, int &indent,
                      const std::string &iterVar, const std::string &collection) override
    {
        emit(out, indent, "for " + iterVar + " in " + collection + ":");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override { indent--; }
    void emitAlloc(std::ostringstream &out, int &indent,
                   const std::string &var, const std::string &type,
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") {
            emit(out, indent, var + " = range(" + content + ")");
        } else if (type == "sequence") {
            std::string b = content2.empty() ? content : content2;
            emit(out, indent, var + " = range(" + content + ", " + b + ")");
        } else if (type == "dict") {
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
        } else if (type == "object") {
            // Legacy: should not be reached — ObjDecl now emits ac_gl_obj_create + string decl
            emit(out, indent, var + " = \"" + content + "\"");
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
        std::string m = msg.empty() ? "\"Fatality occurred\"" : msg;
        emit(out, indent, "import sys as _sys; _sys.stderr.write(\"Preposterous: \" + str(" + m + ") + \"\\n\"); os.abort()");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        std::string m = msg.empty() ? "\"\"" : msg;
        emit(out, indent, "import sys as _sys; _sys.stderr.write(\"" + prefix + ": \" + str(" + m + ") + \"\\n\")");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "try:");
        indent++;
        emit(out, indent, result + " = " + expr);
        indent--;
        emit(out, indent, "except Exception as _lazy_err:");
        indent++;
        emit(out, indent, result + " = _lazy_err");
        indent--;
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "try:");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        std::string ty = typeName.empty() ? "Exception" : typeName;
        indent--;
        emit(out, indent, "except " + ty + " as _ac_exc:");
        indent++;
        // Bind exVar as a plain string so Term.display works naturally
        emit(out, indent, exVar + " = str(_ac_exc)");
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "finally:");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        (void)out;
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, pfx + v + " = " + v);
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v);
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
    std::set<std::string> freeVars_;       // free vars for current function
    std::set<std::string> promotedGlobals_; // all free-declared vars across program

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
    void setFreeVars(const std::vector<std::string> &vars) override
    {
        for (auto& v : vars) freeVars_.insert(v);
    }
    void setPromotedGlobals(const std::vector<std::string> &vars) override
    {
        for (auto& v : vars) promotedGlobals_.insert(v);
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->JS)");
        emitRaw(out, "'use strict';");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string foundLibDir;
                std::string ffi = readFFIFile(ln, "js", &foundLibDir);
                if (!ffi.empty()) {
                    if (!foundLibDir.empty()) {
                        std::string lnSafe = ln;
                        for (char& c : lnSafe) if (c == '-') c = '_';
                        out << "var _ac_" << lnSafe << "_lib_dir = '" << foundLibDir << "';\n";
                    }
                    out << ffi;
                }
            }
        }
        if (needsEvents_) {
            emitRaw(out, "const _acEvents = {};");
            emitRaw(out, "function _acBind(key, fn) { _acEvents[key] = fn; }");
            emitRaw(out, "function _acTrigger(key) { if (_acEvents[key]) _acEvents[key](); }");
        }
        for (auto& v : promotedGlobals_)
            emitRaw(out, "var " + v + ";");
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

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "null", "[]");
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
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType) override
    {
        declared.insert(var);
        emit(out, indent, "const " + var + " = " + val + ";");
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    {
        emit(out, indent, decl(dst, "structuredClone(" + src + ")"));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        std::string expr = lhs + " " + op + " " + rhs;
        emit(out, indent, decl(res, expr));
    }
    void emitIntDiv(std::ostringstream &out, int &indent,
                    const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "Math.trunc(" + lhs + " / " + rhs + ")")); }
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
        emit(out, indent, "process.abort();");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "process.exit(0);");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "{ const _t=Date.now()+("+secs+")*1000; while(Date.now()<_t); }");
    }
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        emit(out, indent, decl(res, "Function('return (' + " + expr + " + ')()')()"));
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "console.error(\"Preposterous: \" + String(" + msg + ")); process.abort();");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "console.error(\"" + prefix + ": \" + String(" + (msg.empty() ? "\"\"" : msg) + "));");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "let " + result + "; try { " + result + " = " + expr + "; } catch(_e) { " + result + " = _e; }");
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "try {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName; // JS has no typed catch; ignore
        indent--;
        // exVar is already the string (we throw strings, not Error objects)
        emit(out, indent, "} catch (" + exVar + ") {");
        indent++;
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} finally {");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "var " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") {
            emit(out, indent, "let " + var + " = [...Array(Number(" + content + ")).keys()];");
        } else if (type == "sequence") {
            std::string b = content2.empty() ? content : content2;
            emit(out, indent, "let " + var + " = Array.from({length:Number(" + b + ")-Number(" + content + ")},(_, _k)=>_k+Number(" + content + "));");
        } else if (type == "dict") {
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
        emit(out, indent, "let " + result + " = prompt(" + prompt + ");");
    }
    void emitBrowserPrint(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "print();");
    }
    void emitAlert(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "alert(" + val + ");");
    }
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        if (res.empty())
            emit(out, indent, "confirm(" + val + ");");
        else
            emit(out, indent, decl(res, "confirm(" + val + ")"));
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
        // Pre-populate declared with promoted globals and function-level free vars
        for (auto& v : promotedGlobals_) declared.insert(v);
        for (auto& v : freeVars_) declared.insert(v);
        freeVars_.clear();
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        std::string expr;
        if      (t == IRType::FLOAT)  expr = "Number(" + src + ")";
        else if (t == IRType::INT)    expr = "Math.trunc(Number(" + src + "))";
        else if (t == IRType::STRING) expr = "String(" + src + ")";
        else if (t == IRType::BOOL)   expr = "Boolean(" + src + ") ? 1 : 0";
        else return;
        emit(out, indent, decl(var, expr));
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        for (auto& v : promotedGlobals_) declared.insert(v);
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
    std::set<std::string> freeVars_;
    std::set<std::string> promotedGlobals_;

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void setFreeVars(const std::vector<std::string> &vars) override
    {
        for (auto& v : vars) freeVars_.insert(v);
    }
    void setPromotedGlobals(const std::vector<std::string> &vars) override
    {
        for (auto& v : vars) promotedGlobals_.insert(v);
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "<!DOCTYPE html>");
        emitRaw(out, "<html lang=\"en\">");
        emitRaw(out, "<head>");
        emitRaw(out, "  <meta charset=\"UTF-8\">");
        emitRaw(out, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
        emitRaw(out, "  <title>AC Program</title>");
        emitRaw(out, "  <style>");
        emitRaw(out, "    body { font-family: monospace; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 1rem; }");
        emitRaw(out, "    #_out { white-space: pre-wrap; line-height: 1.6; }");
        emitRaw(out, "    #_out b  { color: #f38ba8; }");
        emitRaw(out, "    #_out i  { color: #a6e3a1; font-style: italic; }");
        emitRaw(out, "    #_out h2 { color: #89b4fa; margin: 0.4em 0; }");
        emitRaw(out, "    #_out a  { color: #cba6f7; }");
        emitRaw(out, "    #_out code { background: #313244; padding: 0 4px; border-radius: 3px; color: #f9e2af; }");
        emitRaw(out, "    #_out p  { margin: 0.2em 0; }");
        emitRaw(out, "    #_out u  { color: #89dceb; text-decoration: underline; }");
        emitRaw(out, "    #_out mark { background: #f9e2af; color: #1e1e2e; padding: 0 2px; }");
        emitRaw(out, "    #_out hr { border: none; border-top: 1px solid #45475a; margin: 0.6em 0; }");
        emitRaw(out, "  </style>");
        emitRaw(out, "</head>");
        emitRaw(out, "<body>");
        emitRaw(out, "<div id=\"_out\"></div>");
        emitRaw(out, "<script>");
        emitRaw(out, "'use strict';");
        emitRaw(out, "const _el = document.getElementById('_out');");
        emitRaw(out, "function _print(v) { _el.appendChild(document.createTextNode(String(v) + '\\n')); }");
        emitRaw(out, "function _printHTML(h) { const d = document.createElement('div'); d.innerHTML = h; _el.appendChild(d); }");
        if (needsEvents_) {
            emitRaw(out, "const _acEvents = {};");
            emitRaw(out, "function _acBind(key, fn) { _acEvents[key] = fn; document.addEventListener('keydown', function(e) { if (e.key === key) fn(); }); }");
            emitRaw(out, "function _acTrigger(key) { if (_acEvents[key]) _acEvents[key](); }");
        }
        for (auto& v : promotedGlobals_)
            emitRaw(out, "var " + v + ";");
        emitRaw(out, "");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "</script>");
        emitRaw(out, "</body>");
        emitRaw(out, "</html>");
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

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "null", "[]");
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
    void emitIntDiv(std::ostringstream &out, int &indent,
                    const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "Math.trunc(" + lhs + " / " + rhs + ")")); }
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
        emit(out, indent, "console.log(" + val + ");");
    }
    void emitStyledPrint(std::ostringstream &out, int &indent,
                         const std::string &val, const std::string &style) override
    {
        if (style == "title")
            emit(out, indent, "document.title = String(" + val + ");");
        else if (style == "bold")
            emit(out, indent, "_printHTML('<b>' + String(" + val + ") + '</b>');");
        else if (style == "italic")
            emit(out, indent, "_printHTML('<i>' + String(" + val + ") + '</i>');");
        else if (style == "header")
            emit(out, indent, "_printHTML('<h2>' + String(" + val + ") + '</h2>');");
        else if (style == "link")
            emit(out, indent, "_printHTML('<a href=\"' + String(" + val + ") + '\">' + String(" + val + ") + '</a>');");
        else if (style == "code")
            emit(out, indent, "_printHTML('<code>' + String(" + val + ") + '</code>');");
        else if (style == "para")
            emit(out, indent, "_printHTML('<p>' + String(" + val + ") + '</p>');");
        else if (style == "underline")
            emit(out, indent, "_printHTML('<u>' + String(" + val + ") + '</u>');");
        else if (style == "mark")
            emit(out, indent, "_printHTML('<mark>' + String(" + val + ") + '</mark>');");
        else if (style == "hr")
            emit(out, indent, "_printHTML('<hr>');");
        else
            emit(out, indent, "_print(" + val + ");");
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "throw new Error('AC: /kill');");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "_printHTML('<hr><i>Program ended.</i>');");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "{ const _t=Date.now()+("+secs+")*1000; while(Date.now()<_t); }");
    }
    void emitInput(std::ostringstream &out, int &indent,
                   const std::string &result, const std::string &prompt) override
    {
        emit(out, indent, "let " + result + " = prompt(" + prompt + ");");
    }
    void emitBrowserPrint(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "print();");
    }
    void emitAlert(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, "alert(" + val + ");");
    }
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        if (res.empty())
            emit(out, indent, "confirm(" + val + ");");
        else
            emit(out, indent, decl(res, "confirm(" + val + ")"));
    }
    void emitEval(std::ostringstream &out, int &indent,
                  const std::string &res, const std::string &expr) override
    {
        emit(out, indent, decl(res, "Function('return (' + " + expr + " + ')()')()"));
    }
    void emitRaise(std::ostringstream &out, int &indent, const std::string &msg) override
    {
        emit(out, indent, "_printHTML('<b style=\"color:red\">Preposterous: ' + String(" + msg + ") + '</b>'); throw new Error(" + msg + ");");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "_printHTML('<i>" + prefix + ": ' + String(" + (msg.empty() ? "\"\"" : msg) + ") + '</i>');");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "let " + result + "; try { " + result + " = " + expr + "; } catch(_e) { " + result + " = _e; }");
    }
    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "try {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        indent--;
        emit(out, indent, "} catch (" + exVar + ") {");
        indent++;
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} finally {");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }
    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "var " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
    }
    void emitForeign(std::ostringstream &out, int &indent, const std::string &code) override
    {
        // Keep indentation so foreign code can live inside blocks.
        std::string raw = stripQuotes(code);
        std::istringstream ss(raw);
        std::string line;
        bool any = false;
        while (std::getline(ss, line)) { any = true; emit(out, indent, line); }
        if (!any) emit(out, indent, "");
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") {
            emit(out, indent, "let " + var + " = [...Array(Number(" + content + ")).keys()];");
        } else if (type == "sequence") {
            std::string b = content2.empty() ? content : content2;
            emit(out, indent, "let " + var + " = Array.from({length:Number(" + b + ")-Number(" + content + ")},(_, _k)=>_k+Number(" + content + "));");
        } else {
            emit(out, indent, "let " + var + " = [" + content + "];");
        }
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
        for (auto& v : promotedGlobals_) declared.insert(v);
        for (auto& v : freeVars_) declared.insert(v);
        freeVars_.clear();
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        std::string expr;
        if      (t == IRType::FLOAT)  expr = "Number(" + src + ")";
        else if (t == IRType::INT)    expr = "Math.trunc(Number(" + src + "))";
        else if (t == IRType::STRING) expr = "String(" + src + ")";
        else if (t == IRType::BOOL)   expr = "Boolean(" + src + ") ? 1 : 0";
        else return;
        emit(out, indent, decl(var, expr));
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        for (auto& v : promotedGlobals_) declared.insert(v);
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
    std::set<std::string> listVars;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;
    bool returnIsList_ = false;
    std::map<std::string, IRType> varCastTypes_;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::map<std::string, int> funcTypedParams_; // param → arity
    bool returnIsFloat_ = false;

    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math.pi" || v == "math.e" || v == "math.tau" || v == "math.em" || v == "math.phi" || v == "math.inf"
            || v.rfind("math.", 0) == 0
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
        emitRaw(out, "#include <unistd.h>");
        // Emit ilib C headers at file scope before main()
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string absDir = resolveIlibDir(ln);
                emitRaw(out, "#include \"" + absDir + "/" + ln + "_c.h\"");
                {
                    std::string _lnk = (ln == "machine-audio") ? "acmachinaaudio" : ("ac" + ln);
                    emitRaw(out, "// Link: gcc output.c -L\"" + absDir + "\" -l" + _lnk
                                 + " -Wl,-rpath,\"" + absDir + "\"");
                }
            } else if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    auto slash = ln.rfind('/');
                    std::string libdir   = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                    std::string basename = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                    std::string libname  = basename.substr(0, basename.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "// Link: gcc output.c -L" + libdir + " -l" + libname);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                }
            }
        }
        emitRaw(out, "");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        emitRaw(out, "");
    }

    std::string formatCallName(const std::string& n) const override {
        std::string s = n; for (char& c : s) if (c == '.') c = '_'; return s;
    }
    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "1", "0", "NULL", "NULL", false);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) return "ac_str " + var + " = " + val + ";";
            if (ct == IRType::INT || ct == IRType::BOOL) return "ac_int " + var + " = (ac_int)(" + val + ");";
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
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        bool isFloat = isFloatVal(val) || t == IRType::FLOAT;
        if (isFloat) { floatVars.insert(var); emit(out, indent, "const double " + var + " = " + val + ";"); }
        else emit(out, indent, "const ac_int " + var + " = " + val + ";");
        declared.insert(var);
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    { emit(out, indent, decl(dst, src)); } // C is value-semantic
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "(double)(" + lhs + ") / (double)(" + rhs + ")";
        bool isNew = declared.insert(res).second;
        if (isNew) { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else        emit(out, indent, res + " = " + expr + ";");
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "(long long)(" + lhs + ") % (long long)(" + rhs + ")";
        emit(out, indent, decl(res, expr));
    }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, t);
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "double " + var + " = (double)(" + val + ");"); }
            else if (declType == IRType::STRING) emit(out, indent, "ac_str " + var + " = " + val + ";");
            else if (declType == IRType::BOOL || declType == IRType::INT)
                                                  emit(out, indent, "ac_int " + var + " = (ac_int)(" + val + ");");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            emit(out, indent, var + " = " + val + ";");
        }
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
        if (fn.rfind("math.", 0) == 0) return true;
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
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            listVars.insert(res);
            emit(out, indent, "ac_int* " + res + " = " + call + ";");
        } else if ((isFloatReturningFunc(func) || isUserFloatReturningFunc(func))
                   && declared.insert(res).second) {
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
        emit(out, indent, "abort();");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "exit(0);");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "usleep((unsigned int)((" + secs + ") * 1000000u));");
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
        std::string m = msg.empty() ? "\"Fatality occurred\"" : msg;
        emit(out, indent, "fprintf(stderr, \"Preposterous: %s\\n\", (const char*)" + m + "); abort();");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        std::string m = msg.empty() ? "\"\"" : msg;
        emit(out, indent, "fprintf(stderr, \"" + prefix + ": %s\\n\", (const char*)" + m + ");");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        // C has no exceptions; lazy_eval is just assignment
        emit(out, indent, decl(result, expr));
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "/* try */ {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        indent--;
        emit(out, indent, "} /* catch (" + exVar + ") — C has no exceptions */");
        // Wrap catch body in dead code; declare exVar so body compiles
        emit(out, indent, "if (0) { long long " + exVar + " = 0;");
        indent++;
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} /* after */ {");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "__typeof__(" + v + ") " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
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
        // High-level IR: condition is empty (evaluated inside loop body with break)
        emit(out, indent, cond.empty() ? "while (1) {" : "while (" + cond + ") {");
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for (ac_int " + iterVar + " = 0; " + iterVar + " < (" + rangeOf_[collection] + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (ac_int " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") {
            rangeOf_[var] = content;
            return;
        }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
        if (type == "string")
            emit(out, indent, "ac_str " + var + " = \"" + content + "\";");
        else if (type == "dict")
            emit(out, indent, "/* dict " + var + " = {" + content + "} */");
        else {
            listVars.insert(var); declared.insert(var);
            // Count elements to determine array size
            int n = content.empty() ? 0 : 1;
            for (char c : content) if (c == ',') n++;
            // Heap-allocate so the pointer is safe to return from functions
            emit(out, indent, "ac_int* " + var + " = (ac_int*)malloc(" + std::to_string(n) + " * sizeof(ac_int));");
            // Assign each element by index
            std::istringstream iss(content);
            std::string tok; int idx2 = 0;
            while (std::getline(iss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string elem = (a != std::string::npos) ? tok.substr(a, b-a+1) : tok;
                emit(out, indent, var + "[" + std::to_string(idx2++) + "] = " + elem + ";");
            }
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
        emit(out, indent, "printf(\"%s\", " + prompt + ");");
        emit(out, indent, "char _buf_" + result + "[4096];");
        emit(out, indent, "fgets(_buf_" + result + ", sizeof(_buf_" + result + "), stdin);");
        emit(out, indent, "_buf_" + result + "[strcspn(_buf_" + result + ", \"\\n\")] = 0;");
        emit(out, indent, "ac_str " + result + " = _buf_" + result + ";");
        declared.insert(result);
    }

    void emitLabel(std::ostringstream &out, int indent, const std::string &label) override
    {
        std::string l = label;
        if (l.size() >= 2 && l.front() == '"') l = l.substr(1, l.size() - 2);
        if (l == "__break__" || l == "__continue__") return; // handled by break/continue
        out << l << ":;\n"; // semicolon needed so label can precede end of block
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
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if (!(" + cond + ")) continue;");
        else
            emit(out, indent, "if (!(" + cond + ")) goto " + label + ";");
    }
    void emitJumpIfTrue(std::ostringstream &out, int indent,
                        const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (" + cond + ") break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if (" + cond + ") continue;");
        else
            emit(out, indent, "if (" + cond + ") goto " + label + ";");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        std::string cName = name;
        if (!classOwner.empty())
            cName = (name == "init") ? classOwner + "_init" : classOwner + "_" + name;

        // Build typed param list, using function-pointer type for func-typed params
        std::string tparams;
        if (!params.empty()) {
            std::istringstream ss(params);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (fit != funcTypedParams_.end()) {
                    // emit function pointer: ac_int (*f)(ac_int, ...)
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) {
                        if (k > 0) argList += ", ";
                        argList += "ac_int";
                    }
                    tparams += "ac_int (*" + pname + ")(" + argList + ")";
                } else {
                    tparams += "ac_int " + pname;
                }
                first = false;
            }
        }
        if (!classOwner.empty()) {
            std::string selfParam = classOwner + "* self";
            tparams = tparams.empty() ? selfParam : selfParam + ", " + tparams;
        }
        if (tparams.empty()) tparams = "void";
        declareParams(params, declared);
        std::string retT = returnIsList_ ? "ac_int*" : returnIsFloat_ ? "double" : "ac_int";
        returnIsFloat_ = false; returnIsList_ = false;
        emit(out, indent, retT + " " + cName + "(" + tparams + ") {");
        indent++;
        funcTypedParams_.clear();
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    void emitIndirectCall(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &func,
                          const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        if (res.empty())
            emit(out, indent, call + ";");
        else
            emit(out, indent, decl(res, call));
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        // Variable was declared with the correct type via pre-scan; emit same-type cast.
        if (floatVars.count(var))
            emit(out, indent, var + " = (double)(" + src + ");");
        else if (t == IRType::STRING)
            emit(out, indent, var + " = " + src + ";");
        else
            emit(out, indent, var + " = (ac_int)(" + src + ");");
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
protected:
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    std::map<std::string, IRType> varCastTypes_;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string,std::set<std::string>> importSymbols_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::map<std::string, int> funcTypedParams_;
    bool returnIsFloat_ = false;
    bool returnIsList_ = false;
    bool curFuncReturnIsList_ = false;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;

    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    void setImportSymbols(const std::unordered_map<std::string,std::set<std::string>>& s) override {
        importSymbols_ = s;
    }

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
        emitRaw(out, "#include <functional>");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "#include <cstdlib>");
        emitRaw(out, "#include <thread>");
        emitRaw(out, "#include <chrono>");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        // Emit ilib/flib includes at file scope before main()
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                if (ln == "camera") {
                    emitRaw(out, "#include \"library/ilib/camera/camera_wrapper.hpp\"");
                    emitRaw(out, "// Link: g++ ... -lopencv_core -lopencv_videoio -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs");
                    // Define global instances (camera.cpp is compiled into this TU)
                    emitRaw(out, "namespace AC {");
                    emitRaw(out, "    Camera WebCam;");
                    emitRaw(out, "    Camera latestFrame;");
                    emitRaw(out, "    Camera firstFrame;");
                    emitRaw(out, "    SidebarConsole sidebar;");
                    emitRaw(out, "    Screen Background;");
                    emitRaw(out, "}");
                    emitRaw(out, "using namespace AC;");
                } else {
                    {
                        std::string absDir = resolveIlibDir(ln);
                        emitRaw(out, "#include \"" + absDir + "/" + ln + ".hpp\"");
                        std::string _lnk = (ln == "machine-audio") ? "acmachinaaudio" : ("ac" + ln);
                        emitRaw(out, "// Link: g++ out.cpp -I. -L\"" + absDir + "\" -l" + _lnk);
                    }
                    // Selective import: emit using-declarations for requested symbols
                    std::string key = "ilib:" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty()) {
                        out << "// from " << ln << " use: ";
                        bool first = true;
                        for (auto& sym : it->second) { if (!first) out << ", "; out << sym; first = false; }
                        out << "\n";
                        for (auto& sym : it->second)
                            out << "using ac_math::" << sym << ";\n";
                    }
                }
            } else if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    auto slash = ln.rfind('/');
                    std::string libdir   = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                    std::string basename = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                    std::string stemname = basename.substr(0, basename.rfind('.'));
                    std::string libname  = stemname;
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                    emitRaw(out, "// Link: g++ out.cpp -L" + libdir + " -l" + libname);
                    // Include the companion .h header (generated by AC->LIB alongside the .so)
                    std::string hPath = ln.substr(0, ln.rfind('.')) + ".h";
                    emitRaw(out, "#include \"" + hPath + "\"");
                } else {
                    emitRaw(out, "#include \"" + ln + ".hpp\"");
                }
            }
        }
        emitRaw(out, "");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "nullptr", "{}");
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) return "std::string " + var + " = std::to_string(" + val + ");";
            if (ct == IRType::INT || ct == IRType::BOOL) return "long long " + var + " = (long long)(" + val + ");";
            if (isFloatVal(val)) { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "auto " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        bool isFloat = isFloatVal(val) || t == IRType::FLOAT;
        if (isFloat) { floatVars.insert(var); emit(out, indent, "const double " + var + " = " + val + ";"); }
        else emit(out, indent, "const long long " + var + " = " + val + ";");
        declared.insert(var);
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    { emit(out, indent, decl(dst, src)); } // C++ value-semantic for primitives
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "(double)(" + lhs + ") / (double)(" + rhs + ")";
        bool isNew = declared.insert(res).second;
        if (isNew) { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else        emit(out, indent, res + " = " + expr + ";");
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "(long long)(" + lhs + ") % (long long)(" + rhs + ")")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, t);
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "double " + var + " = (double)(" + val + ");"); }
            else if (declType == IRType::STRING)  emit(out, indent, "std::string " + var + " = std::to_string(" + val + ");");
            else if (declType == IRType::INT || declType == IRType::BOOL)
                                                  emit(out, indent, "long long " + var + " = (long long)(" + val + ");");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            emit(out, indent, var + " = " + val + ";");
        }
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
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            emit(out, indent, "std::vector<long long> " + res + " = " + call + ";");
        } else if (isUserFloatReturningFunc(func) && declared.insert(res).second) {
            floatVars.insert(res);
            emit(out, indent, "double " + res + " = " + call + ";");
        } else {
            emit(out, indent, decl(res, call));
        }
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (val.empty()) emit(out, indent, curFuncReturnIsList_ ? "return {};" : "return 0;");
        else             emit(out, indent, "return " + val + ";");
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
        emit(out, indent, "std::abort();");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "std::exit(0);");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "std::this_thread::sleep_for(std::chrono::duration<double>(" + secs + "));");
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
        emit(out, indent, "std::cerr << \"Preposterous: \" << " + msg + " << '\\n'; std::abort();");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "std::cerr << \"" + prefix + ": \" << " + (msg.empty() ? "\"\"" : msg) + " << '\\n';");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "auto " + result + " = [&]() -> auto { try { return " + expr + "; } catch(...) { return decltype(" + expr + "){}; } }();");
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "try {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        indent--;
        if (typeName.empty())
            emit(out, indent, "} catch (std::exception& _ac_exc) {");
        else
            emit(out, indent, "} catch (" + typeName + "& _ac_exc) {");
        indent++;
        // Bind exVar as a plain std::string so Term.display works
        emit(out, indent, "std::string " + exVar + "(_ac_exc.what());");
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} // after (always runs)");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "auto " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for (long long " + iterVar + " = 0; " + iterVar + " < (" + rangeOf_[collection] + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (long long " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") {
            rangeOf_[var] = content;
            return;
        }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
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
            cppName = (name == "init") ? classOwner : name;
        }
        // Build typed params, using std::function for func-typed params
        std::string tparams;
        if (!cppParams.empty()) {
            std::istringstream ss(cppParams);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (fit != funcTypedParams_.end()) {
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) { if (k) argList += ", "; argList += "long long"; }
                    tparams += "std::function<long long(" + argList + ")> " + pname;
                } else {
                    tparams += "long long " + pname;
                }
                first = false;
            }
        }
        declareParams(cppParams, declared);
        curFuncReturnIsList_ = returnIsList_;
        std::string retType = (!classOwner.empty() && name == "init") ? ""
            : returnIsList_ ? "std::vector<long long> "
            : returnIsFloat_ ? "double " : "long long ";
        returnIsFloat_ = false; returnIsList_ = false;
        emit(out, indent, retType + cppName + "(" + tparams + ") {");
        indent++;
        funcTypedParams_.clear();
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
        curFuncReturnIsList_ = false;
    }
    void emitIndirectCall(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &func,
                          const std::string &args) override
    {
        std::string call = func + "(" + args + ")";
        if (res.empty())
            emit(out, indent, call + ";");
        else
            emit(out, indent, decl(res, call));
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        if (floatVars.count(var))
            emit(out, indent, var + " = (double)(" + src + ");");
        else if (t == IRType::STRING)
            emit(out, indent, var + " = std::to_string(" + src + ");");
        else
            emit(out, indent, var + " = (long long)(" + src + ");");
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
// 6. LIB  (AC->LIB) — shared library (.so / .dll); inherits C++ codegen
// ═══════════════════════════════════════════════════════════════════════════

class LibStrategy : public CppStrategy
{
    // No main() wrapper — exports top-level functions and globals only.
    // If a <mainloop> is present its code goes into ac_lib_init().
    bool hasMain_ = false;
public:
    void emitHeader(std::ostringstream &out) override
    {
        // Delegate to CppStrategy for includes, then override the comment
        // CppStrategy::emitHeader already emits everything we need.
        // Temporarily patch the banner by re-running with our own prefix.
        emitRaw(out, "// Generated by AC Compiler (AC->LIB)");
        emitRaw(out, "// Compile: g++ -std=c++17 -shared -fPIC <file>.cpp -o <file>.so");
        emitRaw(out, "#include <iostream>");
        emitRaw(out, "#include <string>");
        emitRaw(out, "#include <vector>");
        emitRaw(out, "#include <map>");
        emitRaw(out, "#include <functional>");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "#include <cstdlib>");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        // Emit ilib/flib deps via CppStrategy pendingImports_ logic
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                if (ln == "camera") {
                    emitRaw(out, "#include \"library/ilib/camera/camera_wrapper.hpp\"");
                    emitRaw(out, "namespace AC {");
                    emitRaw(out, "    Camera WebCam;");
                    emitRaw(out, "    Camera latestFrame;");
                    emitRaw(out, "    Camera firstFrame;");
                    emitRaw(out, "    SidebarConsole sidebar;");
                    emitRaw(out, "    Screen Background;");
                    emitRaw(out, "}");
                    emitRaw(out, "using namespace AC;");
                } else {
                    {
                        std::string absDir = resolveIlibDir(ln);
                        emitRaw(out, "#include \"" + absDir + "/" + ln + ".hpp\"");
                        std::string _lnk = (ln == "machine-audio") ? "acmachinaaudio" : ("ac" + ln);
                        emitRaw(out, "// Link: g++ out.cpp -I. -L\"" + absDir + "\" -l" + _lnk);
                    }
                    std::string key = "ilib:" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty())
                        for (auto& sym : it->second)
                            out << "using ac_math::" << sym << ";\n";
                }
            } else if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    auto slash = ln.rfind('/');
                    std::string libdir   = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                    std::string basename = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                    std::string libname  = basename.substr(0, basename.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                    emitRaw(out, "// Link: g++ out.cpp -L" + libdir + " -l" + libname);
                    emitRaw(out, "#include \"" + ln.substr(0, ln.rfind('.')) + ".h\"");
                } else {
                    emitRaw(out, "#include \"" + ln + ".hpp\"");
                }
            }
        }
        emitRaw(out, "");
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        // Wrap top-level statements in ac_lib_init() instead of main()
        hasMain_ = true;
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "extern \"C\" {");
        emitRaw(out, "#endif");
        emit(out, indent, "void ac_lib_init() {");
        indent++;
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "} // extern \"C\"");
        emitRaw(out, "#endif");
    }
    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name,
                           const std::string &params,
                           const std::string &classOwner) override
    {
        // Export every top-level function as extern "C"
        if (classOwner.empty()) {
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "extern \"C\" {");
            emitRaw(out, "#endif");
        }
        CppStrategy::emitFunctionBegin(out, indent, name, params, classOwner);
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        CppStrategy::emitFunctionEnd(out, indent);
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "} // extern \"C\"");
        emitRaw(out, "#endif");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 7. JAVA  (AC->Java)
// ═══════════════════════════════════════════════════════════════════════════

class JavaStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    std::string className = "Main";
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::set<std::string> stringParams_;  // function params that should be typed as String

    bool returnIsFloat_ = false;
    std::set<std::string> userFloatFuncs_;
    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    std::set<std::string> userListFuncs_;
    bool returnIsList_ = false;
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    void setStringParams(const std::set<std::string>& s) override { stringParams_ = s; }
    std::string funcArgRef(const std::string &name) override { return className + "::" + name; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        const char* s = v.c_str();
        if (strncmp(s, "AcMath.", 7) == 0) s += 7;
        if (isIntReturningMathFunc(s)) return false;
        return strncmp(s, "math_", 5) == 0 || strncmp(s, "math.", 5) == 0 || strncmp(s, "stat_", 5) == 0;
    }
    static bool isFloatReturningFunc(const std::string &fn) {
        const char* s = fn.c_str();
        if (strncmp(s, "AcMath.", 7) == 0) s += 7;
        if (isIntReturningMathFunc(s)) return false;
        return strncmp(s, "math_", 5) == 0 || strncmp(s, "math.", 5) == 0 || strncmp(s, "stat_", 5) == 0;
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
        emitRaw(out, "public class " + className + " {");
        emitRaw(out, "");
    }
    void emitFooter(std::ostringstream &out) override
    {
        emitRaw(out, "}  // class " + className);
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::INT)
            return std::to_string(std::get<int64_t>(r.value.data)) + "L";
        std::string s = commonRef(r, sym, "true", "false", "null", "null");
        // Dot-style (math.pi, math.sin): go to the `math` namespace class directly
        if (r.kind == IRRef::Kind::VAR && isKnownFloatName(s) && s.find('.') == std::string::npos)
            return "AcMath." + s;
        return s;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) return "String " + var + " = String.valueOf(" + val + ");";
            if (ct == IRType::INT || ct == IRType::BOOL) return "long " + var + " = (long)(" + val + ");";
            if (looksString(val)) return "String " + var + " = " + val + ";";
            if (isFloatVal(val))  { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "long " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        bool valIsFloat = isFloatVal(val);
        if (valIsFloat && declared.count(var) && !floatVars.count(var)) {
            // var was declared as long but now receives a double — cast to long to avoid Java error
            emit(out, indent, var + " = (long)(" + val + ");");
        } else {
            if (valIsFloat) floatVars.insert(var);
            emit(out, indent, decl(var, val));
        }
    }
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        bool isFloat = isFloatVal(val) || t == IRType::FLOAT;
        if (isFloat) { floatVars.insert(var); emit(out, indent, "final double " + var + " = " + val + ";"); }
        else emit(out, indent, "final long " + var + " = " + val + ";");
        declared.insert(var);
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    { emit(out, indent, decl(dst, src)); } // Java primitives are value-semantic
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "(double)(" + lhs + ") / (double)(" + rhs + ")";
        bool isNew = declared.insert(res).second;
        if (isNew) { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else        emit(out, indent, res + " = " + expr + ";");
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "((long)(" + lhs + ")) % ((long)(" + rhs + "))")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, t);
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "double " + var + " = (double)(" + val + ");"); }
            else if (declType == IRType::STRING)  emit(out, indent, "String " + var + " = String.valueOf(" + val + ");");
            else if (declType == IRType::INT || declType == IRType::BOOL)
                                                  emit(out, indent, "long " + var + " = (long)(" + val + ");");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            // Float value assigned to a long variable: cast to long to avoid Java type error
            bool valIsFloat = (t == IRType::FLOAT) || isFloatVal(val);
            if (valIsFloat && !floatVars.count(var))
                emit(out, indent, var + " = (long)(" + val + ");");
            else
                emit(out, indent, var + " = " + val + ";");
        }
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else if (isNew)        emit(out, indent, "long " + res + " = " + expr + ";");
        else if (isFloat && !floatVars.count(res))
                               emit(out, indent, res + " = (long)(" + expr + ");");
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
        // Translate ac_gl_* → AcGl.* for Java Panama FFI
        static const std::unordered_map<std::string,std::string> glMap = {
            {"ac_gl_init","AcGl.init"},{"ac_gl_quit","AcGl.quit"},
            {"ac_gl_screen_create","AcGl.screenCreate"},
            {"ac_gl_screen_set_bg","AcGl.screenSetBg"},
            {"ac_gl_screen_set_fps","AcGl.screenSetFps"},
            {"ac_gl_screen_w","AcGl.screenW"},{"ac_gl_screen_h","AcGl.screenH"},
            {"ac_gl_screen_init","AcGl.screenInit"},
            {"ac_gl_screen_animate","AcGl.screenAnimate"},
            {"ac_gl_screen_set_bg_by_name","AcGl.screenSetBgByName"},
            {"ac_gl_obj_create","AcGl.objCreate"},
            {"ac_gl_obj_geometry","AcGl.objGeometry"},
            {"ac_gl_obj_square","AcGl.objSquare"},
            {"ac_gl_obj_pos","AcGl.objPos"},
            {"ac_gl_obj_color","AcGl.objColor"},
            {"ac_gl_obj_velocity","AcGl.objVelocity"},
            {"ac_gl_obj_set_speed","AcGl.objSetSpeed"},
            {"ac_gl_obj_set_direction","AcGl.objSetDirection"},
            {"ac_gl_obj_speed_mult","AcGl.objSpeedMult"},
            {"ac_gl_obj_move_x","AcGl.objMoveX"},
            {"ac_gl_obj_move_y","AcGl.objMoveY"},
            {"ac_gl_obj_x","AcGl.objX"},{"ac_gl_obj_y","AcGl.objY"},
            {"ac_gl_obj_w","AcGl.objW"},{"ac_gl_obj_h","AcGl.objH"},
            {"ac_gl_obj_curveshape","AcGl.objCurveshape"},
            {"ac_gl_obj_vertex","AcGl.objVertex"},
            {"ac_gl_obj_to_draw","AcGl.objToDraw"},
            {"ac_gl_obj_circle_fall","AcGl.objCircleFall"},
            {"ac_gl_obj_circle_fell","AcGl.objCircleFell"},
            {"ac_gl_obj_set_spawn","AcGl.objSetSpawn"},
            {"ac_gl_obj_regen","AcGl.objRegen"},
            {"ac_gl_obj_animate","AcGl.objAnimate"},
            {"ac_gl_obj_color_by_name","AcGl.objColorByName"},
            {"ac_gl_obj_pos_from_spec","AcGl.objPosFromSpec"},
            {"ac_gl_obj_config_item","AcGl.objConfigItem"},
            {"ac_gl_obj_save_spawn","AcGl.objSaveSpawn"},
            {"ac_gl_hitbox_overlap","AcGl.hitboxOverlap"},
            {"ac_gl_hitbox_overlap_boundary","AcGl.hitboxBoundary"},
            {"ac_gl_hitbox_overlap_pattern","AcGl.hitboxOverlapPattern"},
            {"ac_gl_hitbox_many_overlap","AcGl.hitboxManyOverlap"},
            {"ac_gl_key_pressed","AcGl.keyPressed"},
            {"ac_gl_key_just_pressed","AcGl.keyJustPressed"},
            {"ac_gl_frame_begin","AcGl.frameBegin"},
            {"ac_gl_frame_update","AcGl.frameUpdate"},
            {"ac_gl_frame_render","AcGl.frameRender"},
            {"ac_gl_frame_end","AcGl.frameEnd"},
            {"ac_gl_delta_time","AcGl.deltaTime"},
            {"ac_gl_is_draw","AcGl.isDraw"},
            {"ac_gl_is_obj","AcGl.isObj"},
        };
        // Boolean-returning GL functions (need boolean var, not long)
        static const std::unordered_set<std::string> glBoolFuncs = {
            "AcGl.init","AcGl.screenCreate","AcGl.isDraw","AcGl.isObj",
            "AcGl.hitboxOverlap","AcGl.hitboxBoundary","AcGl.hitboxOverlapPattern",
            "AcGl.hitboxManyOverlap","AcGl.keyPressed","AcGl.keyJustPressed",
            "AcGl.frameBegin","AcGl.objCircleFell",
        };
        std::string actualFunc = func;
        auto git = glMap.find(func);
        if (git != glMap.end()) actualFunc = git->second;

        std::string call = actualFunc + "(" + args + ")";
        if (glBoolFuncs.count(actualFunc)) {
            if (res.empty()) {
                emit(out, indent, call + ";");
            } else if (declared.insert(res).second) {
                emit(out, indent, "boolean " + res + " = " + call + ";");
            } else {
                emit(out, indent, res + " = " + call + ";");
            }
            return;
        }
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isListReturningFunc(actualFunc) && declared.insert(res).second) {
            emit(out, indent, "long[] " + res + " = " + call + ";");
        } else if ((isFloatReturningFunc(actualFunc) || isUserFloatReturningFunc(actualFunc))
                   && declared.insert(res).second) {
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
        emit(out, indent, "Runtime.getRuntime().halt(134);");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "System.exit(0);");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "try { Thread.sleep((long)((" + secs + ") * 1000L)); } catch (InterruptedException _ac_ie) { Thread.currentThread().interrupt(); }");
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
        emit(out, indent, "System.err.println(\"Preposterous: \" + " + msg + "); Runtime.getRuntime().halt(134);");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "System.err.println(\"" + prefix + ": \" + " + (msg.empty() ? "\"\"" : msg) + ");");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "Object " + result + "; try { " + result + " = " + expr + "; } catch(Throwable _e) { " + result + " = _e; }");
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "try {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        std::string ty = typeName.empty() ? "Exception" : typeName;
        indent--;
        emit(out, indent, "} catch (" + ty + " _ac_exc) {");
        indent++;
        // Bind exVar as a plain String so Term.display works
        emit(out, indent, "String " + exVar + " = _ac_exc.getMessage();");
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} finally {");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "var " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for (long " + iterVar + " = 0; " + iterVar + " < (" + rangeOf_[collection] + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (long " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar);
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") { rangeOf_[var] = content; return; }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
        if (type == "string") {
            emit(out, indent, "String " + var + " = \"" + content + "\";");
            declared.insert(var);
        } else if (type == "dict") {
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
        // Build typed params: use String for GL-name params, long for others
        std::string tparams;
        if (!jParams.empty()) {
            std::istringstream pss(jParams);
            std::string ptok;
            bool pfirst = true;
            while (std::getline(pss, ptok, ',')) {
                size_t a = ptok.find_first_not_of(' ');
                size_t b = ptok.find_last_not_of(' ');
                std::string pname = (a != std::string::npos) ? ptok.substr(a, b-a+1) : ptok;
                if (!pfirst) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (fit != funcTypedParams_.end()) {
                    // java.util.function: 1-arg → LongUnaryOperator, else LongFunction<Long>
                    tparams += (fit->second == 1 ? "java.util.function.LongUnaryOperator "
                                                 : "java.util.function.LongFunction<Long> ") + pname;
                } else {
                    tparams += (stringParams_.count(pname) ? "String " : "long ") + pname;
                }
                pfirst = false;
            }
        }
        stringParams_.clear();
        funcTypedParams_.clear();
        std::string retT = returnIsList_ ? "long[]" : returnIsFloat_ ? "double" : "long"; returnIsList_ = false;
        returnIsFloat_ = false;
        std::string sig = isConstructor
            ? classOwner + "(" + tparams + ")"
            : "static " + retT + " " + jName + "(" + tparams + ")";
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
    void emitIndirectCall(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &func,
                          const std::string &args) override
    {
        // Java: call through a functional interface — use applyAsLong / apply
        std::string call = func + ".applyAsLong(" + args + ")";
        if (res.empty())
            emit(out, indent, call + ";");
        else
            emit(out, indent, decl(res, call));
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        if (floatVars.count(var))
            emit(out, indent, var + " = (double)(" + src + ");");
        else if (t == IRType::STRING)
            emit(out, indent, var + " = String.valueOf(" + src + ");");
        else
            emit(out, indent, var + " = (long)(" + src + ");");
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
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    bool lastWasReturn = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }

    bool returnIsFloat_ = false;
    bool returnIsList_ = false;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;
    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math.pi" || v == "math.e" || v == "math.tau" || v == "math.em" || v == "math.phi" || v == "math.inf"
            || v.rfind("math.", 0) == 0
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
            } else if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    auto slash = ln.rfind('/');
                    std::string libdir   = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                    std::string basename = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                    std::string libname  = basename.substr(0, basename.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "// Link: rustc output.rs -L" + libdir + " -l" + libname);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                }
            }
        }
        emitRaw(out, "");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "None", "None");
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "let mut " + var + ": f64 = " + (looksFloat(val) ? val : val + " as f64") + ";"; }
            if (ct == IRType::STRING) return "let mut " + var + ": String = " + val + ".to_string();";
            if (ct == IRType::INT || ct == IRType::BOOL) return "let mut " + var + ": i64 = " + val + " as i64;";
            if (looksString(val)) return "let mut " + var + ": &str = " + val + ";";
            if (isFloatVal(val)) { floatVars.insert(var); return "let mut " + var + ": f64 = " + val + ";"; }
            return "let mut " + var + ": i64 = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        // Rust: variables are immutable by default — use let (no mut)
        bool isFloat = isFloatVal(val) || t == IRType::FLOAT;
        if (isFloat) { floatVars.insert(var); emit(out, indent, "let " + var + ": f64 = " + val + ";"); }
        else emit(out, indent, "let " + var + ": i64 = " + val + " as i64;");
        declared.insert(var);
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    { emit(out, indent, decl(dst, src + ".clone()")); }
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = lhs + " as f64 / " + rhs + " as f64";
        bool isNew = declared.insert(res).second;
        if (isNew) { floatVars.insert(res); emit(out, indent, "let mut " + res + ": f64 = " + expr + ";"); }
        else        emit(out, indent, res + " = " + expr + ";");
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "(" + lhs + " as i64) % (" + rhs + " as i64)")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, t);
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "let mut " + var + ": f64 = " + (looksFloat(val) ? val : val + " as f64") + ";"); }
            else if (declType == IRType::STRING)  emit(out, indent, "let mut " + var + ": String = " + val + ".to_string();");
            else if (declType == IRType::BOOL)    emit(out, indent, "let mut " + var + ": bool = " + val + " != 0;");
            else if (declType == IRType::INT)     emit(out, indent, "let mut " + var + ": i64 = " + val + " as i64;");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            emit(out, indent, var + " = " + val + ";");
        }
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
        if (fn.rfind("math.", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;
        return false;
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        // math_to_int takes f64 — cast integer args explicitly
        std::string actualArgs = (func == "math_to_int" || func == "math.to_int") ? args + " as f64" : args;
        std::string call = func + "(" + actualArgs + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            emit(out, indent, "let mut " + res + ": Vec<i64> = " + call + ";");
        } else if ((isFloatReturningFunc(func) || isUserFloatReturningFunc(func))
                   && declared.insert(res).second) {
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
        emit(out, indent, "std::process::abort();");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "std::process::exit(0);");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        lastWasReturn = false;
        emit(out, indent, "std::thread::sleep(std::time::Duration::from_secs_f64(" + secs + "));");
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
        emit(out, indent, "eprintln!(\"Preposterous: {}\", " + msg + "); std::process::abort();");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "eprintln!(\"" + prefix + ": {}\", " + (msg.empty() ? "\"\"" : msg) + ");");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "let " + result + " = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| " + expr + ")).unwrap_or_default();");
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "let _ac_try_result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}));");
        emit(out, indent, "if let Err(_ac_err) = _ac_try_result {");
        indent++;
        // Extract the panic message string (panic!("{}", msg) stores a String payload)
        emit(out, indent, "let " + exVar + " = _ac_err.downcast_ref::<String>().cloned()");
        emit(out, indent + 1, ".or_else(|| _ac_err.downcast_ref::<&str>().map(|s| s.to_string()))");
        emit(out, indent + 1, ".unwrap_or_else(|| String::from(\"unknown error\"));");
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "} // after");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, "let mut " + pfx + v + " = " + v + ";");
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v + ";");
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for " + iterVar + " in 0.." + rangeOf_[collection] + " {");
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for " + iterVar + " in (" + a + ")..(" + b + ") {");
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") { rangeOf_[var] = content; return; }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
        if (type == "string") {
            emit(out, indent, "let " + var + ": &'static str = \"" + content + "\";");
            declared.insert(var);
        } else if (type == "dict") {
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
        // Build typed params with fn() type for function-typed params
        std::string tparams;
        if (!rParams.empty()) {
            std::istringstream ss(rParams);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (fit != funcTypedParams_.end()) {
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) { if (k) argList += ", "; argList += "i64"; }
                    tparams += "mut " + pname + ": fn(" + argList + ") -> i64";
                } else {
                    tparams += "mut " + pname + ": i64";
                }
                first = false;
            }
        }
        declareParams(rParams, declared);
        curFuncReturnIsList_ = returnIsList_;
        std::string retT = returnIsList_ ? "Vec<i64>" : returnIsFloat_ ? "f64" : "i64";
        returnIsList_ = false;
        returnIsFloat_ = false;
        bool isNew = !classOwner.empty() && name == "init";
        if (isNew) {
            emit(out, indent, "pub fn new(" + tparams + ") -> Self {");
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "pub fn " + rName + "(&mut self" + sep + tparams + ") -> " + retT + " {");
        } else {
            emit(out, indent, "fn " + rName + "(" + tparams + ") -> " + retT + " {");
        }
        indent++;
        funcTypedParams_.clear();
    }
    bool curFuncReturnIsList_ = false;
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        if (!lastWasReturn)
            emit(out, indent, curFuncReturnIsList_ ? "vec![]" : "0");
        lastWasReturn = false; curFuncReturnIsList_ = false;
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        if (floatVars.count(var))
            emit(out, indent, var + " = " + src + " as f64;");
        else if (t == IRType::STRING)
            emit(out, indent, var + " = " + src + ".to_string();");
        else
            emit(out, indent, var + " = " + src + " as i64;");
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
    std::set<std::string> stringVars;
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    bool needsOS_ = false;
    bool needsInput_ = false;
    bool returnIsFloat_ = false;
    bool returnIsList_ = false;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::vector<std::string> promotedGlobals_;
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::set<std::string> pendingFreeVars_;
    // Relative path from output file's directory to project root (e.g. ".." for examples/)
    std::string relRoot_ = ".";

    static bool isKnownFloatName(const std::string &v) {
        if (v.find('(') != std::string::npos) return false;
        if (isIntReturningMathFunc(v.c_str())) return false;
        return v == "math_pi" || v == "math_e" || v == "math_tau" || v == "math_em" || v == "math_inf"
            || v == "math.pi"  || v == "math.e"  || v == "math.tau" || v == "math.em" || v == "math.inf"
            || v.rfind("math.", 0) == 0
            || v.rfind("stat_", 0) == 0;
    }
    static bool isFloatReturningFunc(const std::string &fn) {
        if (isIntReturningMathFunc(fn.c_str())) return false;
        if (fn.rfind("math.", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;
        return false;
    }
    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setNeedsInput(bool v) override {
        needsInput_ = v;
        if (v) needsOS_ = true; // uses os.Stdin
    }
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
    void setPromotedGlobals(const std::vector<std::string> &vars) override
    {
        promotedGlobals_ = vars;
    }
    void setFreeVars(const std::vector<std::string> &vars) override
    {
        // Store for re-application in emitFunctionBegin (which calls declared.clear())
        for (auto& v : vars) pendingFreeVars_.insert(v);
    }
    void setVarCastTypes(const std::map<std::string, IRType>& m) override
    {
        varCastTypes_ = m;
    }

    // Returns the effective declaration type for a variable, using the cast type if available.
    IRType castDeclType(const std::string& var, IRType defaultType) const {
        auto it = varCastTypes_.find(var);
        return (it != varCastTypes_.end()) ? it->second : defaultType;
    }
    // Wrap a value expression with an explicit cast if needed for Go's strict type system.
    std::string goWrap(const std::string& val, IRType t) const {
        if (t == IRType::FLOAT && !looksFloat(val) && !isKnownFloatName(val))
            return "float64(" + val + ")";
        if (t == IRType::INT && looksFloat(val))
            return "int64(" + val + ")";
        return val;
    }

    static std::string soLinkFlags(const std::string &path) {
        auto dot   = path.rfind('.');
        auto slash = path.rfind('/');
        std::string libdir   = (slash == std::string::npos) ? "." : path.substr(0, slash);
        std::string basename = (slash == std::string::npos) ? path : path.substr(slash + 1);
        std::string libname  = basename.substr(0, dot != std::string::npos ? dot : basename.size());
        if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
        return "#cgo LDFLAGS: -L" + libdir + " -l" + libname;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->GO)");
        emitRaw(out, "package main");
        // CGO blocks must appear between package and regular import
        bool hasCGO = false;
        std::vector<std::string> wrapperBlocks;

        // Collect flib LDFLAGS to inject into the first CGO preamble (or a standalone block)
        std::string flibLDFlags;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    flibLDFlags += soLinkFlags(ln) + "\n";
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                }
            }
        }

        bool flibInjected = false;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string cgoBlock, wrappers;
                parseGoFFI(ln, cgoBlock, wrappers);
                if (!cgoBlock.empty()) {
                    // Replace ${SRCDIR}/library/ln with ${SRCDIR}/<relRoot_>/library/ln
                    // so the path resolves correctly regardless of where the .go file lives.
                    std::string from = "${SRCDIR}/library/ilib/" + ln;
                    std::string to   = "${SRCDIR}/" + relRoot_ + "/library/ilib/" + ln;
                    size_t pos = 0;
                    while ((pos = cgoBlock.find(from, pos)) != std::string::npos) {
                        cgoBlock.replace(pos, from.size(), to);
                        pos += to.size();
                    }
                    // Inject flib LDFLAGS into the first ilib CGO preamble (after "/*\n")
                    if (!flibLDFlags.empty() && !flibInjected) {
                        auto insertAt = cgoBlock.find("/*\n");
                        if (insertAt != std::string::npos)
                            cgoBlock.insert(insertAt + 3, flibLDFlags);
                        flibInjected = true;
                    }
                    hasCGO = true;
                    out << "\n" << cgoBlock;
                    wrapperBlocks.push_back(wrappers);
                }
            }
        }
        // If flib .so exists but no ilib CGO block was emitted, emit a standalone CGO preamble
        if (!flibLDFlags.empty() && !flibInjected) {
            hasCGO = true;
            out << "\n/*\n" << flibLDFlags << "*/\nimport \"C\"\n";
        }
        emitRaw(out, "");
        emitRaw(out, "import (");
        if (needsInput_) emitRaw(out, "    \"bufio\"");
        emitRaw(out, "    \"fmt\"");
        if (needsOS_)   emitRaw(out, "    \"os\"");
        if (needsInput_) emitRaw(out, "    \"strings\"");
        if (hasCGO)      emitRaw(out, "    \"unsafe\"");
        emitRaw(out, "    \"syscall\"");
        emitRaw(out, ")\n");
        emitRaw(out, "func _b(b bool) int64 { if b { return 1 }; return 0 }");
        emitRaw(out, "func _ac_abort() { syscall.Kill(syscall.Getpid(), syscall.SIGABRT) }\n");
        for (auto& w : wrapperBlocks)
            if (!w.empty()) out << w;
        // Package-level vars promoted via `free` keyword
        for (auto& v : promotedGlobals_)
            emitRaw(out, "var " + v + " int64");
        if (!promotedGlobals_.empty()) emitRaw(out, "");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "true", "false", "nil", "nil");
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "var " + var + " float64 = " + goWrap(val, IRType::FLOAT); }
            if (ct == IRType::STRING) { stringVars.insert(var); return "var " + var + " string = fmt.Sprintf(\"%v\", " + val + ")"; }
            if (ct == IRType::INT || ct == IRType::BOOL) { return "var " + var + " int64 = " + goWrap(val, IRType::INT); }
            // No cast override: use value heuristic
            if (looksString(val)) { stringVars.insert(var); return "var " + var + " string = " + val; }
            if (isFloatVal(val)) { floatVars.insert(var); return "var " + var + " float64 = " + val; }
            return "var " + var + " int64 = " + val;
        }
        // Already declared: emit assignment, wrapping value to match declared type
        if (floatVars.count(var) && !looksFloat(val) && !isKnownFloatName(val))
            return var + " = float64(" + val + ")";
        if (stringVars.count(var) && !looksString(val))
            return var + " = fmt.Sprintf(\"%v\", " + val + ")";
        return var + " = " + val;
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        // Go const only works for compile-time literals; use := with a comment
        declared.insert(var);
        bool isFloat = isFloatVal(val) || t == IRType::FLOAT;
        if (isFloat) { floatVars.insert(var); emit(out, indent, var + " := " + val + " /* const */"); }
        else emit(out, indent, var + " := " + val + " /* const */");
    }
    void emitCopy(std::ostringstream &out, int &indent,
                  const std::string &dst, const std::string &src) override
    { emit(out, indent, decl(dst, src)); } // Go value-semantic for scalars
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "float64(" + lhs + ") / float64(" + rhs + ")";
        declared.insert(res); floatVars.insert(res);
        emit(out, indent, res + " := " + expr);
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { declared.insert(res); emit(out, indent, res + " := int64(" + lhs + ") % int64(" + rhs + ")"); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, t);
            if (declType == IRType::FLOAT) {
                floatVars.insert(var);
                emit(out, indent, "var " + var + " float64 = " + goWrap(val, IRType::FLOAT));
            } else if (declType == IRType::STRING) {
                stringVars.insert(var);
                std::string sv = looksString(val) ? val : "fmt.Sprintf(\"%v\", " + val + ")";
                emit(out, indent, "var " + var + " string = " + sv);
            } else if (declType == IRType::BOOL) {
                emit(out, indent, "var " + var + " int64 = " + goWrap(val, IRType::INT));
            } else if (declType == IRType::INT) {
                emit(out, indent, "var " + var + " int64 = " + goWrap(val, IRType::INT));
            } else {
                // Unknown type: use value heuristics
                declared.erase(var);
                emit(out, indent, decl(var, val));
            }
        } else {
            // Already declared: emit assignment wrapped to match declared type
            if (floatVars.count(var) && !looksFloat(val) && !isKnownFloatName(val))
                emit(out, indent, var + " = float64(" + val + ")");
            else if (stringVars.count(var) && !looksString(val))
                emit(out, indent, var + " = fmt.Sprintf(\"%v\", " + val + ")");
            else
                emit(out, indent, var + " = " + val);
        }
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
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            emit(out, indent, "var " + res + " []int64 = " + call);
        } else if ((isFloatReturningFunc(func) || isUserFloatReturningFunc(func))
                   && declared.insert(res).second) {
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
        emit(out, indent, "_ac_abort()");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        needsOS_ = true;
        emit(out, indent, "os.Exit(0)");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        emit(out, indent, "time.Sleep(time.Duration(float64(time.Second) * " + secs + "))");
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
        needsOS_ = true;
        emit(out, indent, "fmt.Fprintf(os.Stderr, \"Preposterous: %v\\n\", " + msg + "); _ac_abort()");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "fmt.Fprintf(os.Stderr, \"" + prefix + ": %v\\n\", " + (msg.empty() ? "\"\"" : msg) + ")");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, result + " := func() (v interface{}) { defer func() { if r := recover(); r != nil { v = r } }(); v = " + expr + "; return }()");
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        // Go: wrap try body in an immediately-invoked closure with defer/recover
        emit(out, indent, "func() {");
        indent++;
        emit(out, indent, "defer func() {");
        indent++;
        emit(out, indent, "if _ac_r := recover(); _ac_r != nil { _ = _ac_r }");
        indent--;
        emit(out, indent, "}()");
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        indent--;
        emit(out, indent, "}()");
        emit(out, indent, "// catch (" + exVar + ") — Go uses defer/recover above");
        // Declare exVar so catch body compiles; wrap in dead block
        emit(out, indent, "if false {");
        indent++;
        emit(out, indent, "var " + exVar + " interface{} = nil; _ = " + exVar);
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} // after (always runs)");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, pfx + v + " := " + v);
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v);
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for " + iterVar + " := int64(0); " + iterVar + " < int64(" + rangeOf_[collection] + "); " + iterVar + "++ {");
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for " + iterVar + " := int64(" + a + "); " + iterVar + " < int64(" + b + "); " + iterVar + "++ {");
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") { rangeOf_[var] = content; return; }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
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
        declared.clear(); floatVars.clear(); stringVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v);
        for (auto& v : pendingFreeVars_) declared.insert(v);
        pendingFreeVars_.clear();
        std::string gParams = params;
        std::string gName   = name;
        if (!classOwner.empty()) {
            if (gParams.rfind("self, ", 0) == 0) gParams = gParams.substr(6);
            else if (gParams == "self") gParams = "";
            gName = (name == "init") ? "New" + classOwner : name;
        }
        // Build typed params with func type for function-typed params
        std::string tparams;
        if (!gParams.empty()) {
            std::istringstream ss(gParams);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (fit != funcTypedParams_.end()) {
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) { if (k) argList += ", "; argList += "int64"; }
                    tparams += pname + " func(" + argList + ") int64";
                } else {
                    tparams += pname + " int64";
                }
                first = false;
            }
        }
        declareParams(gParams, declared);
        curFuncReturnIsList_ = returnIsList_;
        std::string retT = returnIsList_ ? "[]int64" : returnIsFloat_ ? "float64" : "int64";
        returnIsList_ = false; returnIsFloat_ = false;
        bool isNew = !classOwner.empty() && name == "init";
        if (isNew) {
            emit(out, indent, "func New" + classOwner + "(" + tparams + ") *" + classOwner + " {");
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "func (self *" + classOwner + ") " + gName + "(" + tparams + ") " + retT + " {");
        } else {
            emit(out, indent, "func " + gName + "(" + tparams + ") " + retT + " {");
        }
        indent++;
        funcTypedParams_.clear();
    }
    bool curFuncReturnIsList_ = false;
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, curFuncReturnIsList_ ? "return nil" : "return 0");
        curFuncReturnIsList_ = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear(); stringVars.clear();
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        // The variable was already declared with the correct type via pre-scan (setVarCastTypes).
        // Just emit a same-type assignment to make the cast visible in the code.
        if (floatVars.count(var))
            emit(out, indent, var + " = float64(" + src + ")");
        else if (stringVars.count(var))
            emit(out, indent, var + " = fmt.Sprintf(\"%v\", " + src + ")");
        else if (t == IRType::BOOL)
            emit(out, indent, var + " = func() int64 { if " + src + " != 0 { return 1 }; return 0 }()");
        else
            emit(out, indent, var + " = int64(" + src + ")");
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear(); stringVars.clear();
        // Pre-populate promoted globals so they get bare assignment inside main
        for (auto& v : promotedGlobals_) declared.insert(v);
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
    std::set<std::string> floatVars;
    bool lastWasReturn = false;
    bool needsInput_ = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;

    // V: variable names cannot contain uppercase letters or start with _
    static std::string vName(const std::string& s) {
        std::string r = s;
        for (char& c : r) c = (char)std::tolower((unsigned char)c);
        if (!r.empty() && r[0] == '_') r[0] = 'a';
        return r;
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

    void setNeedsInput(bool v) override { needsInput_ = v; }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->V)");
        emitRaw(out, "module main");
        if (needsInput_) emitRaw(out, "import os");
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string libDir;
                std::string content = readFFIFile(ln, "v", &libDir);
                if (!content.empty()) {
                    // Expand @AC_LIBDIR@ placeholder with the resolved absolute library directory.
                    // If the path has spaces, create a no-space symlink in /tmp for tcc/gcc compat.
                    if (!libDir.empty()) {
                        std::string safeDir = libDir;
                        if (libDir.find(' ') != std::string::npos) {
                            std::string slug = ln;
                            for (char& c : slug) if (!std::isalnum((unsigned char)c)) c = '_';
#ifdef _WIN32
                            safeDir = libDir;  // Windows: no symlinks, use path directly
#else
                            safeDir = "/tmp/ac_vlib_" + slug;
                            struct stat st{};
                            bool needLink = true;
                            if (lstat(safeDir.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
                                char rbuf[4096] = {};
                                if (readlink(safeDir.c_str(), rbuf, sizeof(rbuf)-1) > 0)
                                    needLink = (std::string(rbuf) != libDir);
                            }
                            if (needLink) { unlink(safeDir.c_str()); symlink(libDir.c_str(), safeDir.c_str()); }
#endif
                        }
                        std::string tok = "@AC_LIBDIR@";
                        std::string::size_type p = 0;
                        while ((p = content.find(tok, p)) != std::string::npos) {
                            content.replace(p, tok.size(), safeDir);
                            p += safeDir.size();
                        }
                    }
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
            } else if (lt == "flib") {
                auto dot = ln.rfind('.');
                std::string ext = (dot != std::string::npos) ? ln.substr(dot) : "";
                if (ext == ".so" || ext == ".dll") {
                    auto slash = ln.rfind('/');
                    std::string libdir   = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                    std::string basename = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                    std::string libname  = basename.substr(0, basename.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "#flag -L" + libdir + " -l" + libname);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                }
            }
        }
        emitRaw(out, "");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym, "true", "false", "none", "none");
        // V: VAR/TEMP names must be lowercase; lowercase them here
        if (r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP)
            for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
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

    void emitConstDecl(std::ostringstream &out, int &indent,
                       const std::string &var, const std::string &val, IRType t) override
    {
        // V: const must be top-level; emit as regular mut variable inside functions
        emitTypedStoreVar(out, indent, var, val, t);
    }
    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        std::string vn = vName(var);
        if (declared.insert(vn).second)
            emit(out, indent, "mut " + vn + " := i64(" + val + ")");
        else
            emit(out, indent, vn + " = " + val);
    }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        std::string vn = vName(var);
        bool isFloat = (t == IRType::FLOAT);
        if (declared.insert(vn).second) {
            if (isFloat) { floatVars.insert(vn); emit(out, indent, "mut " + vn + " := f64(" + val + ")"); }
            else if (t == IRType::STRING) emit(out, indent, "mut " + vn + " := "      + val);
            else if (t == IRType::BOOL)   emit(out, indent, "mut " + vn + " := bool(" + val + ")");
            else if (looksString(val))    emit(out, indent, "mut " + vn + " := "      + val);
            else                          emit(out, indent, "mut " + vn + " := i64("  + val + ")");
        } else {
            // Cast float→int when assigning float value to int variable
            if (isFloat && !floatVars.count(vn))
                emit(out, indent, vn + " = i64(" + val + ")");
            else
                emit(out, indent, vn + " = " + val);
        }
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, char op) override
    {
        emit(out, indent, decl(res, lhs + " " + op + " " + rhs));
    }
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "f64(" + lhs + ") / f64(" + rhs + ")";
        if (declared.insert(res).second) emit(out, indent, "mut " + res + " := " + expr);
        else                             emit(out, indent, res + " = " + expr);
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "i64(" + lhs + ") % i64(" + rhs + ")")); }
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
        emit(out, indent, "exit(1)");
    }
    void emitSoftHalt(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "exit(0)");
    }
    void emitSleep(std::ostringstream &out, int &indent, const std::string &secs) override
    {
        lastWasReturn = false;
        emit(out, indent, "time.sleep(int(" + secs + " * time.second))");
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
        emit(out, indent, "eprintln('Preposterous: ' + " + msg + "); C.abort()");
    }
    void emitRaiseClause(std::ostringstream &out, int &indent,
                          const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        emit(out, indent, "eprintln('" + prefix + ": ' + " + (msg.empty() ? "''" : msg) + ")");
    }
    void emitLazyEval(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &expr) override
    {
        emit(out, indent, "mut " + result + " := " + expr);
    }

    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "// try");
        emit(out, indent, "{");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        lastWasReturn = false;
        indent--;
        emit(out, indent, "} // catch (" + exVar + ") — V uses 'or {}' for errors");
        emit(out, indent, "if false {");
        indent++;
        emit(out, indent, exVar + " := '' _ = " + exVar);
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "} // after");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "acs" + std::to_string(depth) + "_";  // V: no leading underscore
        for (const auto &v : vars)
            emit(out, indent, "mut " + pfx + v + " := " + v);
    }
    void emitScopeExit(std::ostringstream &out, int &indent,
                       const std::vector<std::string> &vars, int depth) override
    {
        std::string pfx = "acs" + std::to_string(depth) + "_";
        for (const auto &v : vars)
            emit(out, indent, v + " = " + pfx + v);
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
        if (rangeOf_.count(collection)) {
            emit(out, indent, "for " + iterVar + " in 0.." + rangeOf_[collection] + " {");
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for " + iterVar + " in " + a + ".." + b + " {");
            indent++;
            return;
        }
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
                   const std::string &content, const std::string &content2 = "") override
    {
        if (type == "range") { rangeOf_[var] = content; return; }
        if (type == "sequence") {
            seqOf_[var] = {content, content2.empty() ? content : content2};
            return;
        }
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
        declared.clear(); floatVars.clear();
        lastWasReturn = false;
        std::string vParams = params;
        std::string funcName = VStrategy::vName(name);
        if (!classOwner.empty()) {
            if (vParams.rfind("self, ", 0) == 0) vParams = vParams.substr(6);
            else if (vParams == "self") vParams = "";
            funcName = (name == "init") ? "init" : name;
        }
        // V: primitive params can't be mut. Use renamed params (p_) and copy to mut locals.
        std::vector<std::string> paramNames;
        if (!vParams.empty()) {
            std::istringstream ss(vParams); std::string tok;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                if (a != std::string::npos) paramNames.push_back(VStrategy::vName(tok.substr(a, b-a+1)));
            }
        }
        // Build signature with renamed params (name + "_p")
        std::string tparams;
        for (size_t i = 0; i < paramNames.size(); i++) {
            if (i > 0) tparams += ", ";
            tparams += paramNames[i] + "_p i64";
        }
        if (!classOwner.empty())
            emit(out, indent, "fn (" + classOwner + ") " + funcName + "(" + tparams + ") i64 {");
        else
            emit(out, indent, "fn " + funcName + "(" + tparams + ") i64 {");
        indent++;
        // Emit mut local copies and add to declared
        for (const auto& p : paramNames) {
            emit(out, indent, "mut " + p + " := " + p + "_p");
            declared.insert(p);
        }
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
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        std::string expr;
        if      (t == IRType::FLOAT)  expr = "f64(" + src + ")";
        else if (t == IRType::INT)    expr = "i64(" + src + ")";
        else if (t == IRType::STRING) expr = src + ".str()";
        else if (t == IRType::BOOL)   expr = "if " + src + " != 0 { i64(1) } else { i64(0) }";
        else return;
        if (declared.count(var)) emit(out, indent, var + " = " + expr);
        else {
            declared.insert(var);
            emit(out, indent, "mut " + var + " := " + expr);
        }
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
        emitRaw(out, "    extern printf, exit, abort");
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

    std::string formatCallName(const std::string& n) const override {
        std::string s = n; for (char& c : s) if (c == '.') c = '_'; return s;
    }
    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        return commonRef(r, sym, "1", "0", "0", "0", false);
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
        out << "    call abort\n";
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

    void emitTryBegin(std::ostringstream &out, int & /*indent*/) override
    {
        out << "    ; try begin\n";
    }
    void emitCatchBegin(std::ostringstream &out, int & /*indent*/,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        out << "    ; catch (" << exVar << ") — ASM: no native exception support\n";
    }
    void emitAfterBegin(std::ostringstream &out, int & /*indent*/) override
    {
        out << "    ; after begin\n";
    }
    void emitTryEnd(std::ostringstream &out, int & /*indent*/) override
    {
        out << "    ; try end\n";
    }

    void emitScopeEnter(std::ostringstream &out, int & /*indent*/,
                        const std::vector<std::string> &vars, int depth) override
    {
        for (const auto &v : vars)
            out << "    ; scope-enter[" << depth << "]: save " << v << "\n";
    }
    void emitScopeExit(std::ostringstream &out, int & /*indent*/,
                       const std::vector<std::string> &vars, int depth) override
    {
        for (const auto &v : vars)
            out << "    ; scope-exit[" << depth << "]: restore " << v << "\n";
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
    int loopDepth_ = 0;
    std::set<std::string> globalVarNames_;
    std::set<std::string> globalStringVars_; // string vars promoted to C global scope (before main)
    std::vector<std::string> freeVarNames_; // vars assigned at global scope (depth 0)
    std::vector<std::pair<std::string,std::string>> libImports_; // populated in generate() before genInstr
    std::vector<std::string> usingHeaders_; // namespaces declared with "using header X" (insertion order)
    std::unordered_map<std::string,std::string> usingSymbolNS_; // bare symbol → namespace (from ACL)
    bool usingNamespaceIlib_ = false;    // "using namespace ilib" — promotes all ilib libs into usingHeaders_
    std::set<std::string> userFuncNames_; // user-defined function names; unqualified calls not in this set get namespace prefix
    std::unordered_map<std::string,std::set<std::string>> aliasGroups_; // var -> full bidirectional alias group

    std::string ref(const IRRef &r)
    {
        return strategy->formatRef(r, const_cast<SymbolTable *>(&ir.symbols));
    }

    void registerAlias(const std::string& a, const std::string& b)
    {
        if (a.empty() || b.empty()) return;
        std::set<std::string> merged;
        auto ia = aliasGroups_.find(a);
        auto ib = aliasGroups_.find(b);
        if (ia != aliasGroups_.end()) merged.insert(ia->second.begin(), ia->second.end());
        if (ib != aliasGroups_.end()) merged.insert(ib->second.begin(), ib->second.end());
        merged.insert(a);
        merged.insert(b);
        for (const auto& name : merged) aliasGroups_[name] = merged;
    }

    std::vector<std::string> aliasesFor(const std::string& var) const
    {
        std::vector<std::string> out;
        auto it = aliasGroups_.find(var);
        if (it == aliasGroups_.end()) return out;
        for (const auto& name : it->second)
            if (name != var) out.push_back(name);
        return out;
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
                    src = std::to_string(std::get<int64_t>(i.value.data));
                strategy->emitStoreVar(out, indentLevel, ref(i.result), src);
            }
            break;

        case IROpcode::LOAD_VAR:
            if (i.result.isValid() && !i.typedOperands.empty())
                strategy->emitStoreVar(out, indentLevel, ref(i.result), ref(i.typedOperands[0]));
            break;

        case IROpcode::ALIAS_DECL:
            // Register/merge bidirectional alias groups; the initialising STORE_VAR follows immediately
            if (i.typedOperands.size() >= 2) {
                std::string a = ref(i.typedOperands[0]), b = ref(i.typedOperands[1]);
                registerAlias(a, b);
            }
            break;

        case IROpcode::CONST_DECL:
        {
            std::string var = ref(i.result);
            std::string val = i.typedOperands.empty() ? "0" : ref(i.typedOperands[0]);
            strategy->emitConstDecl(out, indentLevel, var, val, i.resultType);
            break;
        }

        case IROpcode::STORE_VAR:
        {
            std::string var, val;
            bool isCopy = !i.attrs.empty() && i.attrs[0] == "copy";
            if (i.typedOperands.size() >= 2) {
                var = ref(i.typedOperands[0]); val = ref(i.typedOperands[1]);
            } else if (i.result.isValid() && !i.typedOperands.empty()) {
                var = ref(i.result); val = ref(i.typedOperands[0]);
            }
            if (!var.empty() && !val.empty()) {
                if (isCopy)
                    strategy->emitCopy(out, indentLevel, var, val);
                else
                    strategy->emitTypedStoreVar(out, indentLevel, var, val, i.resultType);
                // Alias group write-through
                for (const auto& alias : aliasesFor(var))
                    strategy->emitTypedStoreVar(out, indentLevel, alias, val, i.resultType);
            }
            break;
        }

        case IROpcode::ALLOC:
            if (i.result.isValid() && i.typedOperands.size() >= 2)
            {
                // Skip if already emitted as a C global variable
                if (globalStringVars_.count(ref(i.result))) break;
                auto stripQ = [](std::string s) {
                    if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size() - 2);
                    return s;
                };
                std::string allocType = stripQ(ref(i.typedOperands[0]));
                std::string content   = stripQ(ref(i.typedOperands[1]));
                // For sequence: pass second arg (end) as extra content2
                std::string content2;
                if (i.typedOperands.size() >= 3)
                    content2 = stripQ(ref(i.typedOperands[2]));
                strategy->emitAlloc(out, indentLevel, ref(i.result), allocType, content, content2);
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
        case IROpcode::PMUL:
        case IROpcode::DIV:
        case IROpcode::IDIV:
        case IROpcode::MOD:
        {
            if (i.typedOperands.size() >= 2) {
                if (i.opcode == IROpcode::DIV) {
                    strategy->emitTrueDivision(out, indentLevel, ref(i.result),
                                               ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else if (i.opcode == IROpcode::IDIV) {
                    strategy->emitIntDiv(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else if (i.opcode == IROpcode::MOD) {
                    strategy->emitMod(out, indentLevel, ref(i.result),
                                      ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else {
                    char op = (i.opcode == IROpcode::ADD) ? '+'
                            : (i.opcode == IROpcode::SUB) ? '-'
                            : '*';
                    strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                           ref(i.typedOperands[0]), ref(i.typedOperands[1]), op);
                }
            }
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
            strategy->emitScopeEnter(out, indentLevel, freeVarNames_, loopDepth_);
            loopDepth_++;
            strategy->emitWhileBegin(out, indentLevel,
                                     i.typedOperands.empty() ? "" : ref(i.typedOperands[0]));
            break;

        case IROpcode::WHILE_END:
            strategy->emitWhileEnd(out, indentLevel);
            loopDepth_--;
            strategy->emitScopeExit(out, indentLevel, freeVarNames_, loopDepth_);
            break;

        case IROpcode::FOR_BEGIN:
            if (i.typedOperands.size() >= 2)
            {
                strategy->emitScopeEnter(out, indentLevel, freeVarNames_, loopDepth_);
                loopDepth_++;
                strategy->emitForBegin(out, indentLevel,
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            }
            break;

        case IROpcode::FOR_END:
            strategy->emitForEnd(out, indentLevel);
            loopDepth_--;
            strategy->emitScopeExit(out, indentLevel, freeVarNames_, loopDepth_);
            break;

        // ── exception handling ─────────────────────────────────────────
        case IROpcode::TRY_BEGIN:
            strategy->emitTryBegin(out, indentLevel);
            break;

        case IROpcode::CATCH_BEGIN:
        {
            std::string exVar = i.typedOperands.empty() ? "_exc"
                                : stripQuotes(ref(i.typedOperands[0]));
            std::string typeName = (i.typedOperands.size() > 1) ? stripQuotes(ref(i.typedOperands[1])) : "";
            strategy->emitCatchBegin(out, indentLevel, exVar, typeName);
            break;
        }

        case IROpcode::AFTER_BEGIN:
            strategy->emitAfterBegin(out, indentLevel);
            break;

        case IROpcode::TRY_END:
            strategy->emitTryEnd(out, indentLevel);
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
            // Read raw symbol name (before backend formatting) for remapping
            auto rawCallName = [&]() -> std::string {
                if (i.typedOperands.empty()) return "";
                const auto& op = i.typedOperands[0];
                if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                    return ir.symbols.getName(op.id);
                return "";
            };
            std::string rawName = rawCallName();

            std::string func = i.typedOperands.empty() ? "" : ref(i.typedOperands[0]);

            // Remap math constant-as-precision-function: math.pi(n)/math.e(n) are not
            // callable when math_pi/math_e are object-like constant macros in C headers.
            // Route them directly to the underlying precision functions.
            bool hasArgs = i.typedOperands.size() > 1;
            if (hasArgs && !strategy->dotCallSyntax()) {
                if (rawName == "math.pi" || func == "math_pi") func = "ac_math_pi";
                else if (rawName == "math.e" || func == "math_e") func = "ac_math_e";
                else if (rawName == "math.phi" || func == "math_phi") func = "ac_math_phi";
            }

            // Resolve unqualified call via "using header X" / "using namespace ilib"
            // bare name not in user funcs → prefix with matching namespace
            if (!usingHeaders_.empty() && rawName.find('.') == std::string::npos
                && !rawName.empty() && !userFuncNames_.count(rawName))
            {
                // First: check symbol→namespace map built from ACL files
                std::string ns;
                auto it = usingSymbolNS_.find(rawName);
                if (it != usingSymbolNS_.end()) {
                    ns = it->second;
                } else {
                    // Fallback: use first declared header (insertion order)
                    ns = usingHeaders_.front();
                }
                func = strategy->formatCallName(ns + "." + rawName);
                rawName = ns + "." + rawName;
            }

            std::string args;
            for (size_t j = 1; j < i.typedOperands.size(); j++)
            {
                if (j > 1)
                    args += ", ";
                const auto& aop = i.typedOperands[j];
                std::string aname = (aop.kind == IRRef::Kind::VAR && aop.id >= 0)
                                    ? ir.symbols.getName(aop.id) : "";
                // If this arg is a user function being passed as a value, let the
                // backend format it appropriately (e.g. Java: ClassName::fname)
                if (!aname.empty() && userFuncNames_.count(aname))
                    args += strategy->funcArgRef(aname);
                else
                    args += ref(aop);
            }
            std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
            // Indirect call: callee is a parameter variable, not a known user function
            bool isIndirect = !rawName.empty()
                              && !userFuncNames_.count(rawName)
                              && rawName.find('.') == std::string::npos
                              && !i.typedOperands.empty()
                              && i.typedOperands[0].kind == IRRef::Kind::VAR;
            if (isIndirect)
                strategy->emitIndirectCall(out, indentLevel, res, func, args);
            else
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
            // Read raw symbol name (dot-preserved) for dispatch; ref() may transform dots→underscores
            auto rawMethodName = [&]() -> std::string {
                const auto& op = i.typedOperands[0];
                if (op.kind == IRRef::Kind::VAR && op.id >= 0)
                    return ir.symbols.getName(op.id);
                if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::STRING)
                    return std::get<std::string>(op.value.data);
                return ref(op);
            };
            std::string methodRaw = rawMethodName();
            std::string method = ref(i.typedOperands[0]);
            // Resolve styled display variants: bold.display, italic.display, etc.
            auto extractStyle = [](const std::string& m) -> std::string {
                static const std::vector<std::string> styles =
                    {"bold", "italic", "header", "link", "title",
                     "code", "para", "underline", "mark", "hr"};
                for (auto& s : styles)
                    if (m == s + ".display" || m == "\"" + s + ".display\"")
                        return s;
                return "";
            };
            std::string displayStyle = extractStyle(methodRaw);

            if ((methodRaw == "Term.display" || methodRaw == "\"Term.display\"") &&
                i.typedOperands.size() > 1)
            {
                strategy->emitPrint(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if (methodRaw.rfind("Term.", 0) == 0 && methodRaw.size() > 5 &&
                     i.typedOperands.size() >= 1)
            {
                // General Term.X(args) — call X(args) and print result to terminal
                std::string fname = methodRaw.substr(5);
                // If math library is imported, route through math namespace
                for (auto& [lt, ln] : libImports_)
                    if (lt == "ilib" && ln == "math") { fname = "math." + fname; break; }
                std::string args;
                for (size_t j = 1; j < i.typedOperands.size(); j++) {
                    if (j > 1) args += ", ";
                    args += ref(i.typedOperands[j]);
                }
                std::string res = "_ac_term_r_";
                strategy->emitCall(out, indentLevel, res, fname, args);
                strategy->emitPrint(out, indentLevel, res);
            }
            else if (!displayStyle.empty() && i.typedOperands.size() > 1)
            {
                strategy->emitStyledPrint(out, indentLevel, ref(i.typedOperands[1]), displayStyle);
            }
            else if ((methodRaw == "import") &&
                     i.typedOperands.size() > 1)
            {
                // use ilib/elib/clib/flib <name> — emit backend-specific import
                std::string raw = stripQuotes(ref(i.typedOperands[1]));
                std::string libType = "ilib";
                std::string libName = raw;
                auto colon = raw.find(':');
                if (colon != std::string::npos) {
                    libType = raw.substr(0, colon);
                    libName = raw.substr(colon + 1);
                }
                // flib:__inlined__:path — already injected into AST; skip native import
                if (libType == "flib" && libName.rfind("__inlined__:", 0) == 0) break;
                // using <lib> — emit flat namespace aliases at current indent level
                if (libType == "using") {
                    static const std::unordered_map<std::string, std::vector<std::string>> nsMap = {
                        {"gl",      {"screen", "obj", "draw", "hitbox", "key", "frame"}},
                        {"widgets", {"Screen", "display", "ask", "btn", "ckbtn", "radbtn",
                                     "dropdown", "advance", "slider", "group", "tabs",
                                     "scroller", "listbox", "table", "sketch"}},
                        {"camera",  {"camera", "sidebar", "screen"}},
                    };
                    auto it = nsMap.find(libName);
                    if (it != nsMap.end())
                        for (auto& ns : it->second)
                            strategy->emit(out, indentLevel, ns + " = " + libName + "." + ns);
                    break;
                }
                strategy->emitLibImport(out, libType, libName);
            }
            else if ((methodRaw == "foreign") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitForeign(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if ((methodRaw == "raise") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitRaise(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if (methodRaw == "print_page" || methodRaw == "\"print_page\"")
            {
                strategy->emitBrowserPrint(out, indentLevel);
            }
            else if ((methodRaw == "alert" || methodRaw == "\"alert\"") &&
                     i.typedOperands.size() > 1)
            {
                strategy->emitAlert(out, indentLevel, ref(i.typedOperands[1]));
            }
            else if ((methodRaw == "sure" || methodRaw == "\"sure\"") &&
                     i.typedOperands.size() > 1)
            {
                std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
                strategy->emitConfirm(out, indentLevel, res, ref(i.typedOperands[1]));
            }
            else if (methodRaw == "__compound_assign__" && i.typedOperands.size() == 4)
            {
                std::string lhs = ref(i.typedOperands[1]);
                std::string op  = stripQuotes(ref(i.typedOperands[2]));
                std::string rhs = ref(i.typedOperands[3]);
                auto dotPos = lhs.find('.');
                if (ir.backend == "PY" && dotPos != std::string::npos) {
                    std::string base = lhs.substr(0, dotPos);
                    std::string attr = lhs.substr(dotPos + 1);
                    std::string arith_op = op.substr(0, op.size() - 1);
                    strategy->emit(out, indentLevel,
                        lhs + " = getattr(" + base + ", '" + attr + "', 0) " + arith_op + " " + rhs);
                } else if (ir.backend == "PY" || ir.backend == "JS" || ir.backend == "GO") {
                    strategy->emit(out, indentLevel, lhs + " " + op + " " + rhs);
                } else {
                    // C, C++, Rust, Java, V, Asm need explicit semicolon
                    strategy->emit(out, indentLevel, lhs + " " + op + " " + rhs + ";");
                }
            }
            else
            {
                // Generic lib/object call — ref() already handles dot→underscore
                // via preserveDots in commonRef(); no extra conversion needed here.
                std::string func = stripQuotes(method);
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

        case IROpcode::RAISE_CLAUSE:
            if (i.typedOperands.size() >= 2) {
                std::string clause = stripQuotes(ref(i.typedOperands[0]));
                std::string msg    = ref(i.typedOperands[1]);
                strategy->emitRaiseClause(out, indentLevel, clause, msg);
            }
            break;

        case IROpcode::LAZY_EVAL:
            if (i.result.isValid() && !i.typedOperands.empty())
                strategy->emitLazyEval(out, indentLevel, ref(i.result), ref(i.typedOperands[0]));
            break;

        case IROpcode::HALT:
            strategy->emitHalt(out, indentLevel);
            break;

        case IROpcode::SOFT_HALT:
            if (ir.hasShutoff)
                strategy->emitCall(out, indentLevel, "", "__ac_shutoff__", "");
            strategy->emitSoftHalt(out, indentLevel);
            break;

        case IROpcode::SLEEP: {
            std::string dur = i.typedOperands.empty() ? "1" : ref(i.typedOperands[0]);
            // Strip surrounding quotes if present (constant string)
            if (dur.size() >= 2 && dur.front() == '"' && dur.back() == '"')
                dur = dur.substr(1, dur.size() - 2);
            strategy->emitSleep(out, indentLevel, dur);
            break;
        }

        case IROpcode::TYPE_CAST: {
            std::string varName = ref(i.result);
            std::string srcVal = i.typedOperands.empty() ? varName : ref(i.typedOperands[0]);
            strategy->emitTypeCast(out, indentLevel, varName, srcVal, i.resultType);
            break;
        }

        case IROpcode::FUNC_BEGIN:
        case IROpcode::FUNC_END:
        case IROpcode::FREE_DECL:  // handled at function-begin time, not inline
        case IROpcode::NOP:
            break;

        case IROpcode::TAG_BEGIN: {
            std::string tagName;
            if (!i.typedOperands.empty() && i.typedOperands[0].kind == IRRef::Kind::CONST
                    && i.typedOperands[0].value.type == IRType::STRING)
                tagName = std::get<std::string>(i.typedOperands[0].value.data);
            strategy->emitTagBegin(out, indentLevel, tagName);
            break;
        }
        case IROpcode::TAG_END: {
            std::string tagName;
            if (!i.typedOperands.empty() && i.typedOperands[0].kind == IRRef::Kind::CONST
                    && i.typedOperands[0].value.type == IRType::STRING)
                tagName = std::get<std::string>(i.typedOperands[0].value.data);
            strategy->emitTagEnd(out, indentLevel, tagName);
            break;
        }

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
        // Collect vars explicitly declared `free` in this function
        std::vector<std::string> explicitFreeVars;
        {
            std::set<std::string> seen;
            for (const auto &instr : func.instructions)
                if (instr.opcode == IROpcode::FREE_DECL && !instr.typedOperands.empty())
                {
                    std::string name = ref(instr.typedOperands[0]);
                    if (seen.insert(name).second)
                        explicitFreeVars.push_back(name);
                }
        }
        if (!explicitFreeVars.empty())
            strategy->setFreeVars(explicitFreeVars);

        // Tell strategy which global vars this function writes (for Python `global` decls)
        // Merge implicitly-detected globals with explicitly-declared `free` vars.
        {
            std::vector<std::string> pendingGlobals = explicitFreeVars;
            std::set<std::string> seen(explicitFreeVars.begin(), explicitFreeVars.end());
            if (!globalVarNames_.empty())
                for (const auto &instr : func.instructions)
                    if (instr.opcode == IROpcode::STORE_VAR && instr.result.kind == IRRef::Kind::VAR)
                    {
                        std::string name = ref(instr.result);
                        if (!paramSet.count(name) && globalVarNames_.count(name) && seen.insert(name).second)
                            pendingGlobals.push_back(name);
                    }
            if (!pendingGlobals.empty())
                strategy->setPendingGlobals(pendingGlobals);
        }
        // Pre-scan: detect params used as callees (function-typed params) and their arity
        {
            std::map<std::string, int> funcTyped;
            for (const auto &instr : func.instructions) {
                if (instr.opcode != IROpcode::CALL || instr.typedOperands.empty()) continue;
                const auto &callee = instr.typedOperands[0];
                if (callee.kind != IRRef::Kind::VAR) continue;
                std::string cname = callee.id >= 0 ? ir.symbols.getName(callee.id) : "";
                if (cname.empty() || !paramSet.count(cname)) continue;
                // arity = number of args (operands after callee)
                int arity = (int)instr.typedOperands.size() - 1;
                funcTyped[cname] = arity;
            }
            if (!funcTyped.empty())
                strategy->setFuncTypedParams(funcTyped);
        }
        // Pre-scan: detect function return type (float or list)
        // Note: list literals use mkTemp() → Kind::TEMP; named vars use Kind::VAR. Check both.
        {
            bool retFloat = false, retList = false;
            // Helper to get ref id regardless of VAR or TEMP kind
            auto refId = [](const IRRef& r) -> int { return r.id; };
            auto isVarLike = [](const IRRef& r) {
                return r.kind == IRRef::Kind::VAR || r.kind == IRRef::Kind::TEMP;
            };
            auto refName = [&](const IRRef& r) -> std::string {
                if (r.kind == IRRef::Kind::VAR && r.id >= 0)
                    return ir.symbols.getName(r.id);
                if (r.kind == IRRef::Kind::TEMP)
                    return "t_" + std::to_string(r.id);
                return "";
            };
            for (const auto &instr : func.instructions) {
                if (instr.opcode == IROpcode::DIV && instr.resultType == IRType::FLOAT) retFloat = true;
                if (instr.opcode == IROpcode::RETURN && !instr.typedOperands.empty()) {
                    const auto &op = instr.typedOperands[0];
                    if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::FLOAT) retFloat = true;
                    if (isVarLike(op)) {
                        std::string rname = refName(op);
                        for (const auto& ai : func.instructions) {
                            if (ai.opcode == IROpcode::ALLOC && isVarLike(ai.result)
                                && refName(ai.result) == rname
                                && !ai.typedOperands.empty()
                                && ai.typedOperands[0].kind == IRRef::Kind::CONST) {
                                const auto& tv = ai.typedOperands[0].value;
                                if (tv.type == IRType::STRING && std::get<std::string>(tv.data) == "list")
                                    retList = true;
                            }
                        }
                    }
                }
            }
            if (retFloat) strategy->setReturnIsFloat(true);
            if (retList)  strategy->setReturnIsList(true);
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
        else if (b == "LIB")
            strategy = std::make_unique<LibStrategy>();
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
        libImports_.clear();
        usingHeaders_.clear();
        aliasGroups_.clear();
        usingSymbolNS_.clear();
        usingNamespaceIlib_ = false;
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
                // Skip flib:__inlined__:* — already injected into AST, no native import needed
                if (libType == "flib" && libName.rfind("__inlined__:", 0) == 0) return;
                // "using:namespace:ilib" — promote all ilib libs to flat scope
                if (libType == "using" && libName.rfind("namespace:", 0) == 0) {
                    usingNamespaceIlib_ = true;
                    return; // no import entry needed
                }
                // "using:X" — enables unqualified calls; treat as ilib:X for backend imports
                if (libType == "using") {
                    if (std::find(usingHeaders_.begin(), usingHeaders_.end(), libName) == usingHeaders_.end()) {
                        usingHeaders_.push_back(libName);
                        // Build symbol→namespace map from this library's ACL
                        std::string aclContent = readFFIFile(libName, "acl");
                        if (aclContent.empty()) {
                            // Try reading the .acl directly from library dir
                            auto tryAcl = [&](const std::string& base) -> std::string {
                                std::string p = base + "/library/ilib/" + libName + "/" + libName + ".acl";
                                FILE* f = std::fopen(p.c_str(), "r");
                                if (!f) return "";
                                std::string c; char buf[4096];
                                while (std::fgets(buf, sizeof(buf), f)) c += buf;
                                std::fclose(f); return c;
                            };
                            aclContent = tryAcl(".");
                            if (aclContent.empty()) {
#ifndef _WIN32
                                char exeBuf[4096] = {};
                                ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf)-1);
                                if (len > 0) {
                                    std::string bd(exeBuf, len);
                                    auto sl = bd.rfind('/');
                                    if (sl != std::string::npos) bd = bd.substr(0, sl);
                                    aclContent = tryAcl(bd + "/..");
                                }
#endif
                            }
                        }
                        std::istringstream aclss(aclContent);
                        std::string aclLine;
                        while (std::getline(aclss, aclLine)) {
                            auto hash = aclLine.find('#');
                            if (hash != std::string::npos) aclLine = aclLine.substr(0, hash);
                            auto arrow = aclLine.find(" -> ");
                            if (arrow == std::string::npos) continue;
                            std::string src = aclLine.substr(0, arrow);
                            // src is "libname:symbol" — extract bare symbol
                            auto colon = src.find(':');
                            if (colon != std::string::npos) {
                                std::string sym = src.substr(colon + 1);
                                while (!sym.empty() && sym.front() == ' ') sym.erase(0,1);
                                if (!sym.empty() && usingSymbolNS_.find(sym) == usingSymbolNS_.end())
                                    usingSymbolNS_[sym] = libName;
                            }
                        }
                    }
                    libType = "ilib";
                    raw = "ilib:" + libName;
                }
                if (seenImports.insert(raw).second)
                    libImports_.push_back({libType, libName});
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
        {
            int scanDepth = 0;
            std::set<std::string> freeVarSet;
            for (auto& ins : ir.globalInit) {
                if (ins.opcode == IROpcode::INPUT)      hasInput  = true;
                if (ins.opcode == IROpcode::EVENT_BIND) hasEvents = true;
                // NOTE: Do NOT mark mainloop variables as globals — they're local to mainloop!
                // if (ins.opcode == IROpcode::STORE_VAR && ins.result.kind == IRRef::Kind::VAR)
                //     globalVarNames_.insert(ref(ins.result));
                // Track scope depth and collect free vars (assigned at depth 0, not in loops)
                if (ins.opcode == IROpcode::WHILE_BEGIN || ins.opcode == IROpcode::FOR_BEGIN)
                    scanDepth++;
                else if (ins.opcode == IROpcode::WHILE_END || ins.opcode == IROpcode::FOR_END)
                    scanDepth--;
                else if (ins.opcode == IROpcode::STORE_VAR && scanDepth == 0) {
                    std::string varName;
                    if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                        varName = ref(ins.typedOperands[0]);
                    else if (ins.result.isValid() && !ins.typedOperands.empty() && ins.result.kind == IRRef::Kind::VAR)
                        varName = ref(ins.result);
                    if (!varName.empty() && varName.rfind("_ac_", 0) != 0)
                        freeVarSet.insert(varName);
                }
                scanImport(ins);
            }
            freeVarNames_.assign(freeVarSet.begin(), freeVarSet.end());
        }
        // Exempt vars declared `free` in globalInit from loop save/restore
        {
            std::set<std::string> explicitFree;
            for (auto& ins : ir.globalInit)
                if (ins.opcode == IROpcode::FREE_DECL && !ins.typedOperands.empty())
                    explicitFree.insert(ref(ins.typedOperands[0]));
            if (!explicitFree.empty())
                freeVarNames_.erase(
                    std::remove_if(freeVarNames_.begin(), freeVarNames_.end(),
                        [&](const std::string& v){ return explicitFree.count(v); }),
                    freeVarNames_.end());
        }
        // Collect all vars declared `free` in any function (for global promotion in JS/HTML/Go)
        {
            std::vector<std::string> promoted;
            std::set<std::string> seen;
            for (const auto& fn : ir.functions)
                for (const auto& ins : fn.instructions)
                    if (ins.opcode == IROpcode::FREE_DECL && !ins.typedOperands.empty())
                    {
                        std::string name = ref(ins.typedOperands[0]);
                        if (seen.insert(name).second)
                            promoted.push_back(name);
                    }
            if (!promoted.empty())
                strategy->setPromotedGlobals(promoted);
        }

        // Resolve "using namespace ilib" — add all ilib imports to usingHeaders_ (insertion order)
        if (usingNamespaceIlib_)
            for (auto& [lt, ln] : libImports_)
                if (lt == "ilib" && std::find(usingHeaders_.begin(), usingHeaders_.end(), ln) == usingHeaders_.end())
                    usingHeaders_.push_back(ln);

        // If a library import is selective ("from ilib X use a,b,c"), treat those symbols
        // as explicitly mapped into that namespace when resolving unqualified calls.
        // This powers `using math.sin` (which expands to a selective import + using header).
        for (const auto& [key, syms] : importSymbols) {
            // key is like "ilib:math"
            auto colon = key.find(':');
            if (colon == std::string::npos) continue;
            std::string libType = key.substr(0, colon);
            std::string libName = key.substr(colon + 1);
            if (libType != "ilib") continue;
            for (const auto& sym : syms)
                if (!sym.empty() && usingSymbolNS_.find(sym) == usingSymbolNS_.end())
                    usingSymbolNS_[sym] = libName;
        }

        // Pre-scan: collect the final cast type for each variable (dec/int/string/bool)
        // Static backends use this to declare variables with the correct type from the start.
        {
            std::map<std::string, IRType> varCastTypes;
            auto scanCasts = [&](const std::vector<IRInstruction>& insns) {
                for (const auto& ins : insns)
                    if (ins.opcode == IROpcode::TYPE_CAST && ins.result.kind == IRRef::Kind::VAR)
                        varCastTypes[ref(ins.result)] = ins.resultType;
            };
            scanCasts(ir.globalInit);
            for (const auto& fn : ir.functions) scanCasts(fn.instructions);
            if (!varCastTypes.empty())
                strategy->setVarCastTypes(varCastTypes);
        }

        // Build user-defined function name set for unqualified-call resolution
        userFuncNames_.clear();
        for (const auto& fn : ir.functions)
            userFuncNames_.insert(fn.name);

        // Build set of user-defined functions that return float (contain a DIV instruction)
        {
            std::set<std::string> floatFuncs;
            std::set<std::string> listFuncs;
            for (const auto& fn : ir.functions) {
                for (const auto& ins : fn.instructions) {
                    if (ins.opcode == IROpcode::DIV && ins.resultType == IRType::FLOAT)
                        floatFuncs.insert(fn.name);
                    if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()) {
                        const auto& rv = ins.typedOperands[0];
                        auto gRefName = [&](const IRRef& r) -> std::string {
                            if (r.kind == IRRef::Kind::VAR && r.id >= 0) return ir.symbols.getName(r.id);
                            if (r.kind == IRRef::Kind::TEMP) return "t_" + std::to_string(r.id);
                            return "";
                        };
                        bool rvVarLike = rv.kind == IRRef::Kind::VAR || rv.kind == IRRef::Kind::TEMP;
                        if (rvVarLike) {
                            std::string rname = gRefName(rv);
                            for (const auto& ai : fn.instructions) {
                                bool aiVarLike = ai.result.kind == IRRef::Kind::VAR || ai.result.kind == IRRef::Kind::TEMP;
                                if (ai.opcode == IROpcode::ALLOC && aiVarLike
                                    && gRefName(ai.result) == rname
                                    && !ai.typedOperands.empty()
                                    && ai.typedOperands[0].kind == IRRef::Kind::CONST) {
                                    const auto& tv = ai.typedOperands[0].value;
                                    if (tv.type == IRType::STRING && std::get<std::string>(tv.data) == "list")
                                        listFuncs.insert(fn.name);
                                }
                            }
                        }
                    }
                }
            }
            if (!floatFuncs.empty()) strategy->setFloatReturnFuncs(floatFuncs);
            if (!listFuncs.empty()) strategy->setListReturnFuncs(listFuncs);
        }

        strategy->setNeedsInput(hasInput);
        strategy->setNeedsEvents(hasEvents);
        strategy->setPendingImports(libImports_);
        strategy->setImportSymbols(importSymbols);

        strategy->emitHeader(out);

        // For C/C++/LIB/RS/Java backends: emit string ALLOC from globalInit as global variables
        // so they are visible inside event callback functions defined before main().
        if (ir.backend == "C" || ir.backend == "CPP" || ir.backend == "LIB" ||
            ir.backend == "RS" || ir.backend == "Java") {
            auto stripQ2 = [](std::string s) {
                if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size() - 2);
                return s;
            };
            for (const auto &instr : ir.globalInit) {
                if (instr.opcode == IROpcode::ALLOC && instr.result.isValid() &&
                    instr.typedOperands.size() >= 2) {
                    std::string allocType = stripQ2(ref(instr.typedOperands[0]));
                    if (allocType == "string") {
                        std::string varName = ref(instr.result);
                        std::string content = stripQ2(ref(instr.typedOperands[1]));
                        // Emit as global variable (backend-appropriate syntax)
                        if (ir.backend == "RS")
                            out << "static " << varName << ": &str = \"" << content << "\";\n";
                        else if (ir.backend == "Java")
                            out << "    static String " << varName << " = \"" << content << "\";\n";
                        else
                            out << "ac_str " << varName << " = \"" << content << "\";\n";
                        globalStringVars_.insert(varName);
                    }
                }
            }
            if (!globalStringVars_.empty()) out << "\n";
        }

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
                if (inClass) continue;
                genInstr(instr);
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
