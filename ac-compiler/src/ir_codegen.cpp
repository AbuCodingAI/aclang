#include "../include/ac.hpp"
#include <sstream>
#include <fstream>
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
    if (const char* acp = getenv("AC_PATH")) {
        std::string r0 = tryBase(acp);
        if (!r0.empty()) return r0;
    }
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

// Read a raw top-level ilib file (library/ilib/<libName>/<fileName>) — distinct from
// readFFIFile()'s ffi/<lib>_ffi.<ext> convention. Used by the HTML backend to source a
// library's real browser-DOM implementation (e.g. widgets.js) instead of a Node-only
// native FFI file (e.g. ffi/widgets_ffi.js, which calls into a .so via ffi-napi — that
// doesn't exist in a browser at all).
static std::string readIlibRawFile(const std::string& libName, const std::string& fileName) {
    std::string dir = resolveIlibDir(libName);
    std::ifstream f(dir + "/" + fileName);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ── keybinds: AC key-name -> real per-language native key value ──────────────
// `configure event-listener` / `on value is <key>` (parser.cpp) captures <key> as
// a bare AC identifier (e.g. "space") and passes it straight through as a string
// constant. That string is only meaningful on its own for backends with no live
// OS-level key hook; JS/HTML register a REAL `document.addEventListener('keydown',
// ...)` and compare against the browser's own KeyboardEvent.key value, which is
// NOT "space" for the spacebar — it's a literal " ".
//
// This used to be a `library/ilib/keybinds/` ilib (6 hand-duplicated per-language
// constant files) read from disk at compile time. Removed 2026-08-02: nothing ever
// consumed it as a real ilib (no `use ilib keybinds` anywhere, no FFI dispatch —
// only this JS/HTML lookup, done by directly parsing the .js file's text). A static
// lookup table the compiler itself needs isn't an ilib; it's compiler data. Folded
// in directly — one source of truth, no more per-language sync drift (a duplicate
// KEY_ENTER across all 6 files was exactly that failure mode).
static const std::map<std::string, std::string>& jsKeybindsTable() {
    static const std::map<std::string, std::string> table = {
        {"1","1"}, {"2","2"}, {"3","3"}, {"4","4"}, {"5","5"}, {"6","6"}, {"7","7"},
        {"8","8"}, {"9","9"}, {"0","0"}, {"MINUS","-"}, {"EQUAL","="},
        {"BACKSPACE","Backspace"}, {"BACKSLASH","\\"},
        {"TAB","Tab"}, {"Q","q"}, {"W","w"}, {"E","e"}, {"R","r"}, {"T","t"},
        {"Y","y"}, {"U","u"}, {"I","i"}, {"O","o"}, {"P","p"},
        {"LBRACKET","["}, {"RBRACKET","]"},
        {"CAPS","CapsLock"}, {"A","a"}, {"S","s"}, {"D","d"}, {"F","f"}, {"G","g"},
        {"H","h"}, {"J","j"}, {"K","k"}, {"L","l"}, {"SEMICOLON",";"},
        {"APOSTROPHE","'"}, {"ENTER","Enter"},
        {"SHIFT","Shift"}, {"Z","z"}, {"X","x"}, {"C","c"}, {"V","v"}, {"B","b"},
        {"N","n"}, {"M","m"}, {"COMMA",","}, {"PERIOD","."}, {"SLASH","/"},
        {"CTRL","Control"}, {"ALT","Alt"}, {"SPACE"," "}, {"FN","Fn"},
        {"SUPER","Meta"}, {"WINDOWS","Meta"},
        {"UP","ArrowUp"}, {"DOWN","ArrowDown"}, {"LEFT","ArrowLeft"}, {"RIGHT","ArrowRight"},
    };
    return table;
}

// `quotedKey` arrives already-quoted (e.g. "\"space\""), matching commonRef()'s
// string-constant rendering. Returns a replacement quoted JS string literal if the
// AC key name is found in the table above, else returns quotedKey unchanged.
static std::string translateJsKeyConstant(const std::string &quotedKey) {
    if (quotedKey.size() < 2 || quotedKey.front() != '"' || quotedKey.back() != '"')
        return quotedKey;
    std::string acName = quotedKey.substr(1, quotedKey.size() - 2);
    for (char& c : acName) c = (char)std::toupper((unsigned char)c);
    auto& table = jsKeybindsTable();
    auto it = table.find(acName);
    if (it == table.end()) return quotedKey;
    return "\"" + it->second + "\"";
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
    std::string libDir;
    std::string content = readFFIFile(libName, "go", &libDir);
    if (content.empty()) return;
    if (!libDir.empty()) {
        std::string safeDir = libDir;
        if (libDir.find(' ') != std::string::npos) {
            std::string slug = libName;
            for (char& c : slug) if (!std::isalnum((unsigned char)c)) c = '_';
#ifndef _WIN32
            safeDir = "/tmp/ac_golib_" + slug;
            struct stat st{};
            bool needLink = true;
            if (lstat(safeDir.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
                char rbuf[4096] = {};
                ssize_t n = readlink(safeDir.c_str(), rbuf, sizeof(rbuf)-1);
                if (n > 0) {
                    rbuf[n] = '\0';
                    needLink = (std::string(rbuf) != libDir);
                }
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
    // Not every ilib's Go FFI needs cgo (machine-audio's is subprocess-based, `exec.Command`
    // only) — parseGoFFI used to assume EVERY file has an `import "C"` line and unconditionally
    // dumped everything before it (or the WHOLE file, if that line never appears) into cgoBlock,
    // which emitHeader prints immediately/inline at the ilib-import loop, well before its own
    // later `import (...)` block. For a cgo-less file this meant the file's OWN import block
    // AND all its function bodies landed inline before AC's own imports — a hard Go compile
    // error ("imports must appear before other declarations", verified via audio_test.ac).
    // Route non-cgo files entirely through wrapperCode instead, where the caller's import-
    // hoisting logic (see emitHeader's hoistedImports) can find and relocate its import block
    // correctly, same as any other wrapper.
    if (content.find("import \"C\"") == std::string::npos) {
        std::istringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.rfind("package ", 0) == 0) continue;
            wrapperCode += line + "\n";
        }
        return;
    }
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
// Returns true for simple float literals like "3.14", "-1.5", or scientific notation like
// "1e+01"/"1E-5" (no operators/spaces). fmtDouble's shortest-round-trip formatting switches to
// %g's exponential form for values like 10.0 at low precision (e.g. "1e+01") — this used to be
// rejected here (no '.', an unrecognized 'e'/'+' character), so ASM's loadRAX fell through to
// looksNumeric's plain-integer path and emitted `mov rax, 1e+01` — invalid NASM syntax for a GPR
// immediate (verified: math.ac's `math.log10(1000)`-style whole-number-float results failed to
// assemble). NASM's `dq` directive parses scientific notation directly, so recognizing it here is
// enough to route these through the existing float-literal (data-section) path correctly.
static bool looksFloat(const std::string &s)
{
    if (s.empty())
        return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    bool hasDot = false, hasExp = false, sawDigit = false;
    for (; i < s.size(); i++)
    {
        char c = s[i];
        if (c == '.' && !hasExp)
            hasDot = true;
        else if ((c == 'e' || c == 'E') && !hasExp && sawDigit)
            hasExp = true;
        else if ((c == '+' || c == '-') && i > 0 && (s[i-1] == 'e' || s[i-1] == 'E'))
            continue;
        else if (std::isdigit((unsigned char)c))
            sawDigit = true;
        else
            return false;
    }
    return sawDigit && (hasDot || hasExp);
}

static bool isMLFloatReturningFunc(const std::string &fn)
{
    return fn == "ml.take" || fn == "ml_take";
}

// Format a double without trailing zeros; always include a decimal point
static std::string fmtDouble(double d)
{
    if (std::isinf(d)) return d < 0 ? "-1e309" : "1e309";
    if (std::isnan(d)) return "0.0/0.0";
    char buf[64];
    // Shortest ROUND-TRIP representation (what Python's repr does): the fewest significant digits
    // that parse back to exactly this double. %.17g always round-trips but over-prints
    // (0.1 → 0.10000000000000001); this rounds down to the shortest exact form (0.1).
    int prec = 17;
    for (int p = 1; p < 17; p++) {
        std::snprintf(buf, sizeof(buf), "%.*g", p, d);
        if (std::strtod(buf, nullptr) == d) { prec = p; break; }
    }
    std::snprintf(buf, sizeof(buf), "%.*g", prec, d);
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

    // iota/stream value builtins return a (concatenated) string, so typed backends must
    // declare their result with the string type or compiler type-inference (auto/:=/let).
    static bool isAcStrFunc(const std::string &f) {
        if (f == "ac_iota" || f == "ac_stream" ||
            f == "web_page_get" || f == "web_help" ||
            f == "web.page_get" || f == "web.help" ||
            f == "ac_web_page_get" || f == "ac_web_help" ||
            // maudio_listen returns transcribed speech text (or "" — see machine_audio.cpp's
            // own comment on the "Nothing heard or STT unavailable" fallback), not a number —
            // same "known string-returning ilib function" pattern as web.page_get above.
            // Without this, `heard = maudio_listen(4000)` inferred an INT result on every
            // backend, and `IF heard is $$` (audio_test.ac's own string-empty check) either
            // silently miscompared or, on typed backends, wrapped the real `const char*` in an
            // int-to-string coercion instead of using it directly (hard compile error).
            f == "maudio_listen" || f == "maudio.listen") return true;
        // ilib string-returning functions (dotted and underscore forms)
        // `bash`/`sbash` do NOT belong here — both are documented `int ac_os_bash(const char*)`/
        // `ac_os_sbash(...)` in os_c.h and confirmed by every backend's own real implementation
        // (Python: `return result.returncode`) to return the shell command's exit code, not
        // captured output text. Misclassifying them as string-returning was silently tolerated
        // by every example that only called `os.bash(...)`/`os.sbash(...)` as a bare statement
        // (discarding the result) — the first one to actually USE the return code numerically
        // exposed it (verified: jarvis.ac's `rc = os.sbash(cmd)` then `IF rc < 0` — Java declared
        // `rc` as String from this same misclassification, "bad operand types for binary
        // operator '<'" comparing a String against a long literal).
        static const std::set<std::string> tails = {
            "upper", "lower", "trim", "strip", "replace", "b", "format", "getline",
            "cwd", "env", "read_from", "search", "escape", "join",
        };
        for (const char* ns : {"stringm", "os", "regex"}) {
            std::string d = std::string(ns) + ".", u = std::string(ns) + "_";
            if (f.rfind(d, 0) == 0 && tails.count(f.substr(d.size()))) return true;
            if (f.rfind(u, 0) == 0 && tails.count(f.substr(u.size()))) return true;
        }
        // server: db_run/db_run_p/db_import/db_reset and the req_* accessors all
        // return strings (never numbers) — same "known string-returning function" pattern.
        static const std::set<std::string> wsTails = {
            "db_run", "db_run_p", "db_import", "db_reset", "help",
            "req_method", "req_path", "req_body", "req_query", "req_header",
        };
        {
            std::string d = "server.", u = "server_";
            if (f.rfind(d, 0) == 0 && wsTails.count(f.substr(d.size()))) return true;
            if (f.rfind(u, 0) == 0 && wsTails.count(f.substr(u.size()))) return true;
        }
        return false;
    }

    // ilib functions that return a list of strings — typed backends need an explicit
    // []string / Vec<String>-style declaration instead of the plain int64/i64 default.
    // Currently only stringm.split; extend the tails set if more are added.
    static bool isAcStrListFunc(const std::string &f) {
        return f == "stringm.split" || f == "stringm_split";
    }

    virtual void emit(std::ostringstream &out, int indent, const std::string &line) = 0;
    virtual void emitRaw(std::ostringstream &out, const std::string &line) = 0;
    virtual void emitHeader(std::ostringstream &out) = 0;
    virtual void emitFooter(std::ostringstream &out) {}
    // Called once on the FULLY assembled output text, right before it's returned to the
    // caller — lets a strategy do a whole-file text fixup that needs information only known
    // after generation completes (e.g. AsmStrategy's exact per-function stack-frame size,
    // which depends on how many slots that function's body ended up using — not knowable at
    // the point its prologue is emitted). Default: no-op.
    virtual std::string postProcess(const std::string &s) { return s; }
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
        emitBinaryOp(out, indent, res, lhs, rhs, "/");
    }
    // `///` always-float division. Default: same as emitTrueDivision (already float on typed
    // backends). PY overrides `/` to SMART (ac_div) and keeps this one plain float.
    virtual void emitFloatDivision(std::ostringstream &out, int &indent, const std::string &res,
                                   const std::string &lhs, const std::string &rhs) {
        emitTrueDivision(out, indent, res, lhs, rhs);
    }
    // Integer division (truncates toward zero). Default works for Python (//), JS (Math.trunc).
    // Statically-typed backends may override if integer division is already the native behavior.
    virtual void emitIntDiv(std::ostringstream &out, int &indent,
                            const std::string &res,
                            const std::string &lhs, const std::string &rhs) {
        // Default: call emitBinaryOp with '/' — works for languages where operands are already int
        emitBinaryOp(out, indent, res, lhs, rhs, "/");
    }
    // Integer modulo (always integer; cast floats before %). Default works for Python/JS.
    virtual void emitMod(std::ostringstream &out, int &indent,
                         const std::string &res,
                         const std::string &lhs, const std::string &rhs) {
        emitBinaryOp(out, indent, res, lhs, rhs, "%");
    }
    virtual void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                              const std::string &lhs, const std::string &rhs, const std::string &op) = 0;
    virtual void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                                const std::string &lhs, const std::string &rhs, const std::string &op) = 0;
    virtual void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                          const std::string &func, const std::string &args) = 0;
    virtual void emitReturn(std::ostringstream &out, int &indent, const std::string &val) = 0;
    virtual void emitPrint(std::ostringstream &out, int &indent, const std::string &val) = 0;
    // Printing a bare `null`/`nil` literal: every text-substitution backend (commonRef's
    // nullVal/nilVal) already turns these into real readable text ("null"/"None"/etc) by the time
    // emitPrint sees `val`, so no special handling is needed there. ASM is the one backend whose
    // VALUE representation for null/nil must stay the plain integer 0 (matches every other
    // backend's underlying falsy semantics, and is what `is null`-style comparisons/arithmetic
    // correctly resolve against) — so it can't reuse the text-substitution trick without breaking
    // those uses. This hook runs BEFORE `val` is computed at all, with the raw pre-null/nil
    // IRRef, letting ASM intercept and print real text WITHOUT changing null/nil's value
    // representation anywhere else. Default: not handled, falls through to the normal ref+emitPrint
    // path (every other backend keeps working exactly as before).
    virtual bool emitPrintNullText(std::ostringstream &out, int &indent, bool isNil) {
        (void)out; (void)indent; (void)isNil; return false;
    }
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
    // quickthread f(args) — lightweight thread. Default: plain synchronous call
    // (only Go has native goroutines; others degrade gracefully).
    virtual void emitQuickThread(std::ostringstream &out, int &indent,
                                 const std::string &func, const std::string &args)
    {
        emitCall(out, indent, "", func, args);
    }
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
    // Symbol IDs backing the SAME promoted-globals list (see globalVarSymIds_'s own comment on
    // the shared dispatcher side for why name-only matching isn't safe). Default no-op; only
    // RustStrategy currently needs symbol-ID-gated application (see its own formatRef comment).
    virtual void setPromotedGlobalSymIds(const std::set<int> &) {}
    // Subset of promotedGlobals whose value comes from a STRUCT-returning constructor call
    // (ilib widget constructors — display/btn/dropdown/... — the concrete case; not an AC
    // bundle/class, those have their own emitConstructCall path) rather than a scalar/list.
    // Maps var name -> the constructor/type name used at its first assignment (e.g.
    // "name_inp" -> "ask"). Needed by statically-typed backends whose struct types have no
    // default constructor, so a file-scope global for one must be pointer-based
    // (`TypeName* v = nullptr;`, constructed later via `v = new TypeName(args);`) rather than
    // plain-declared — default no-op; only backends that implement this (currently CppStrategy)
    // need to override it.
    virtual void setStructGlobals(const std::map<std::string, std::string> &) {}

    // Called before emitFunctionBegin: marks which function params should be typed as String
    virtual void setStringParams(const std::set<std::string>&) {}

    // Inferred string-typed locals+params for this function (#6): backends declare them as their
    // native string type and iterate them as characters. Set by genFunction before emitFunctionBegin.
    std::set<std::string> stringVars_;
    virtual void setStringVars(const std::set<std::string>& s) { stringVars_ = s; }

    // Dict variables (string-keyed maps) — set at emitAlloc("dict"), consulted by index emission.
    std::set<std::string> dictVars_;      // all dicts
    std::set<std::string> dictStrVals_;   // subset whose VALUES are strings
    // Lists whose elements are THEMSELVES dict vars (e.g. datac-imported multi-row data:
    // `pets = [_dc_pets_0, _dc_pets_1]`, each `_dc_pets_N` a dict from emitAlloc("dict")) — set
    // at emitAlloc's generic list branch when every element name is already in dictVars_,
    // consulted by emitLoadIndex so `p1 = pets[1]` propagates dict-ness onto `p1` itself (else
    // every static backend's list codegen defaults to a plain int/int64 list, "cannot use
    // map[string]string as int64 value" (Go) / "cannot convert std::map<...> to long long" (C++)
    // / "incompatible types: Map<String,String> cannot be converted to long" (Java) — one shared
    // root cause across every statically-typed backend, verified via
    // examples/keyword_catalog_modules.ac).
    std::set<std::string> listOfDictVars_;
    std::set<std::string> listOfDictStrVals_;   // subset whose dict elements are string-valued

    // Inferred float-typed locals (#42): typed backends merge these into their floatVars so
    // declarations come out double/f64/float64 and mixed int/float ops cast correctly.
    // Called AFTER emitFunctionBegin/emitMainBegin (those clear per-function state).
    virtual void setFloatVarsFull(const std::set<std::string>&) {}
    bool isStringVar(const std::string& v) const { return stringVars_.count(v) > 0; }

    // Called before emitFunctionBegin: param name → call arity for function-typed params
    virtual void setFuncTypedParams(const std::map<std::string, int>&) {}
    // Whole-program map of top-level user function name -> declared parameter count. Default
    // no-op; only backends that need real top-level arity (currently RustStrategy, for
    // adapting an AC callback of unknown arity to a fixed-signature `fn(i64) -> i64` widget
    // callback slot — see its emitCall's `btn` handling) override it. NOT the same thing as
    // setFuncTypedParams, which tracks a narrower case (a function PARAMETER used as a callee).
    virtual void setUserFuncArity(const std::map<std::string, int>&) {}
    // Whole-program map: user function name -> indices of its parameters that are float-typed
    // (see #floatparam). Lets a CALL SITE cast an int-literal/int-typed argument to f64 for a
    // callee param the callee's OWN body treats as float (e.g. `collatz(6)` calling a `n: f64`
    // param) — Rust has no implicit numeric coercion, so passing a bare `6` where `f64` is
    // expected is a hard type error.
    virtual void setUserFloatParams(const std::map<std::string, std::set<int>>&) {}
    // Loop vars declared in a for-HEADER are scoped to the loop's braces; strategies
    // push them here at ForBegin and erase from `declared` at ForEnd so a later use
    // at outer scope re-declares instead of referencing an out-of-scope name.
    std::vector<std::string> forVarStack_;
    // Params that receive/hold LISTS (detected from indexing/length/iteration usage) —
    // typed backends must declare them as arrays, not integers.
    std::set<std::string> listParams_;
    virtual void setListParams(const std::set<std::string>& s) { listParams_ = s; }
    // Params inferred float-typed from local usage (see detectFloatParams) — default no-op;
    // only backends whose param types must be explicit and correct up front (no implicit
    // numeric coercion at use sites) need to override this. Currently just RustStrategy.
    virtual void setFloatParams(const std::set<std::string>&) {}
    // Forward declaration for a user function (C/C++ need these for mutual recursion).
    // retKind: 0 = int, 1 = float, 2 = list. Default: backends that don't need prototypes.
    virtual void emitFunctionPrototype(std::ostringstream&, const std::string& /*name*/,
                                       const std::string& /*params*/, int /*retKind*/) {}

    // Indirect call through a variable (parameter holding a function reference)
    virtual void emitIndirectCall(std::ostringstream &out, int &indent,
                                  const std::string &res, const std::string &func,
                                  const std::string &args) {
        emitCall(out, indent, res, func, args); // default: same as direct (Python/JS/HTML)
    }

    // When a user function is passed as an argument value, how to refer to it.
    // Default: just the function name. Java overrides to emit ClassName::method.
    virtual std::string funcArgRef(const std::string &name) { return name; }

    // How an ilib (LIB_CALL) argument is passed. C++ overrides: std::string → .c_str()
    // (ilib ABI surfaces are const char*; #C5).
    virtual std::string libArgRef(const std::string& name, bool isString) { (void)isString; return name; }

    // ac_unstring — the compile-time inverse of stringification (dynamic re-typing, string→number).
    // NOT a runtime parse (no double-flattening, no precision loss): it literally strips the string
    // delimiters off a KNOWN stringified-number literal and yields the bare token; the caller declares
    // the target with `auto`/inferred type so `"5"`→5 (int), `"3.14"`→3.14 (float), `"true"`→true
    // (bool) type exactly. INVARIANT: only ever applied to a value known to be a stringified number —
    // acUnstring("Hi") is mangled code, never a legitimate runtime case.
    static std::string acUnstring(const std::string& v) {
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            return v.substr(1, v.size() - 2);
        return v;   // already a bare numeric token / temp
    }

    // Promoted globals that hold LISTS (C: declared ac_int* at file scope; ALLOC assigns).
    std::set<std::string> listGlobals_;
    virtual void setListGlobals(const std::set<std::string>& s) { listGlobals_ = s; }

    // How to pass a plain value argument given its IR type. Default: unchanged.
    // Rust overrides to `.clone()` list args (Vec isn't Copy → passing by value moves
    // it, so `f(xs); g(xs)` would be a use-after-move; clone gives value semantics, #22).
    virtual std::string valueArgRef(const std::string &name, IRType) { return name; }

    // Pre-computed final cast types for variables (var name → final IRType after all dec/int/etc.)
    // Static backends use this to declare variables with their post-cast type from the start.
    virtual void setVarCastTypes(const std::map<std::string, IRType>&) {}

    // Cross-block locals (first written inside a loop/if, so a block-scoped target language would
    // scope the declaration to that block). Block-scoped backends (C/C++/V) HOIST these to the top
    // of the function with a concrete type, so later uses in sibling/outer blocks resolve (#41).
    // Set per-function by genFunction before emitFunctionBegin.
    std::map<std::string, IRType> hoistVars_;
    virtual void setHoistVars(const std::map<std::string, IRType>& m) { hoistVars_ = m; }

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
    // Whether the program uses any `raise`/`raise Clause(...)`/`/stop` construct — set from a
    // whole-program pre-scan BEFORE emitHeader runs (same pattern as setNeedsInput/
    // setNeedsEvents). Go needs this: `emitRaise`/`emitRaiseClause`/`emitSoftHalt` all use
    // `os.Stderr`/`os.Exit`, but used to flip their OWN `needsOS_` flag reactively DURING body
    // emission — always too late, since `emitHeader` (which decides whether to import "os") had
    // already run by then. "undefined: os" on any program using a named raise clause without
    // ALSO happening to use a bare `raise ERR(...)` elsewhere (which had the same bug, just
    // rarely alone in a program that also imports os some other way).
    virtual void setNeedsOS(bool) {}
    // `save as <file>` — needs every PRINT to also append its exact printed text to an
    // in-memory accumulator (only when the program actually uses `save as` somewhere; gated the
    // same way as setNeedsInput/setNeedsEvents/setNeedsOS), so SAVE_FILE can write out
    // "everything printed so far" when it fires. `emitCapture` is called right after EVERY
    // emitPrint by the shared dispatcher; a backend that hasn't implemented it (or when
    // needsSave_ is false) just no-ops here, matching every print call site rather than
    // duplicating each backend's own type-detection logic (isFloatVal/isStr/etc.) a second time.
    virtual void setNeedsSave(bool) {}
    // Which of the misc runtime-helper builtins (ac_ipow/ac_div/ac_length/_ac_add/random) this
    // PROGRAM actually uses — a backend whose preamble injects one function per builtin
    // unconditionally (Python) should gate each on the matching flag here instead of emitting
    // every one of them into every single generated file regardless of use (verified: a program
    // using none of `^`/`/`/`length`/`random.*` still got all their defs — dead weight in the
    // overwhelming majority of programs, matching `range`/`bxor`'s existing pattern of only
    // emitting/inlining what's actually used). Backends that already inline these natively at
    // the use site (most of them) have no reason to override this.
    virtual void setUsedBuiltinOps(bool /*hasDiv*/, bool /*hasIpow*/, bool /*hasLength*/,
                                    bool /*hasAdd*/, bool /*hasRandom*/, bool /*hasIdiv*/,
                                    bool /*hasEval*/, bool /*hasTry*/) {}
    virtual void emitCapture(std::ostringstream &out, int &indent, const std::string &val)
        { (void)out; (void)indent; (void)val; }
    virtual void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename)
        { (void)out; (void)indent; (void)filename; }
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
    virtual void setReturnIsList(bool v) { baseReturnIsList_ = v; }
    bool baseReturnIsList_ = false;
    bool baseReturnIsString_ = false;
    virtual void setReturnIsString(bool v) { baseReturnIsString_ = v; } // #6: fn returns a string
    virtual void setListReturnFuncs(const std::set<std::string>&) {}
    virtual void setStringReturnFuncs(const std::set<std::string>&) {}  // #6: funcs that return string
    // Function has zero `return <value>;` statements anywhere (only bare `return;`, if any) — a
    // genuinely void function. Backends that pick a numeric type by default (Java: "long") need
    // this to emit "void" instead — Java hard-errors on "missing return statement" otherwise,
    // unlike e.g. C++ which merely has UB if the fallthrough is ever actually reached.
    virtual void setReturnIsVoid(bool) {}
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
    // Backends with real static field/struct-member declarations (C, C++, Java, Rust, Go, V)
    // override this to emit one member declaration; called once per field, right after
    // emitClassBegin, for every `self.<field>` found across the class's methods. Backends whose
    // instances are dynamically-typed (Python attrs, JS/HTML class fields assigned in the
    // constructor) can leave the no-op default — they don't need a separate declaration.
    virtual void emitFieldDecl(std::ostringstream &out, int &indent,
                               const std::string &field, IRType t) { (void)out; (void)indent; (void)field; (void)t; }
    // Called once after all of a class's fields are declared (even if zero fields), before its
    // methods are emitted. Backends whose class body is one open brace the whole time (C++,
    // Java) don't need this — emitClassBegin already opened everything methods need to sit
    // inside. Backends that must CLOSE the field/struct block before opening a separate methods
    // block (Rust's `struct { ... }` + `impl { ... }`) override it to do that transition.
    virtual void emitFieldsEnd(std::ostringstream &out, int &indent) { (void)out; (void)indent; }
    // `c = ClassName()` — bundle instantiation. Default: identical to a plain call (correct for
    // Python/C++/JS/HTML, where `ClassName(args)` already IS valid construction syntax). Backends
    // whose constructor has a different call form (Java `new`, Rust `::new`, Go `NewX()`) override.
    virtual void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                                   const std::string &className, const std::string &args) {
        emitCall(out, indent, res, className, args);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// ── shared helpers ────────────────────────────────────────────────────────────

// Split a plain top-level-comma-separated element list ("_dc_pets_0, _dc_pets_1"), trimming
// whitespace around each — used to check whether a list literal's elements are all dict-var
// names (see listOfDictVars_'s own comment for why this exists).
static std::vector<std::string> splitCommaTrimmed(const std::string& content) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : content) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!content.empty()) out.push_back(cur);
    for (auto& s : out) {
        size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
        s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
    }
    return out;
}

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

// True when every value in the literal is numeric → emit an int-valued map (AC's i64 model).
static bool dictValsAllNumeric(const std::string& content) {
    for (auto& [k, v] : parseDictPairs(content)) {
        (void)k;
        std::string t = v;
        size_t a = t.find_first_not_of(' '), b = t.find_last_not_of(' ');
        if (a == std::string::npos) return false;
        t = t.substr(a, b - a + 1);
        size_t i = (t[0] == '-') ? 1 : 0;
        if (i >= t.size()) return false;
        bool dot = false;
        for (; i < t.size(); ++i) {
            if (t[i] == '.') { if (dot) return false; dot = true; }
            else if (!std::isdigit((unsigned char)t[i])) return false;
        }
    }
    return true;
}

// Format a dict VALUE: $..$ → quoted string; number/bool/var → as-is.
static std::string fmtDictVal(const std::string& v) {
    if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
        return "\"" + v.substr(1, v.size() - 2) + "\"";
    return v;
}

// Same, but for a dict whose declared map VALUE TYPE is string (dictValsAllNumeric()==false —
// i.e. at least one field is a real string, so ALL values share the string-typed map/homogeneous
// container every static backend builds for a mixed dict). A bare numeric field (e.g. `age:5`)
// in that same literal must ALSO come out as a quoted string literal, not the raw number — the
// declared container has no room for a second value type (verified: examples/
// keyword_catalog_modules.ac's datac-imported `pets` rows, each `{name:$Whiskers$, age:5}` —
// C++: "could not convert {{...},{"age", 5}} ... to std::map<string,string>"; Java: "incompatible
// types: String cannot be converted to int"; Rust: "casting &'static str as usize is invalid" —
// three backends, one shared root cause: fmtDictVal() only quotes values already `$..$`-wrapped,
// leaving a bare number unquoted against a string-typed container).
static std::string fmtDictValStr(const std::string& v) {
    if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
        return "\"" + v.substr(1, v.size() - 2) + "\"";
    return "\"" + v + "\"";
}

// 1. PYTHON  (AC->PY)
// ═══════════════════════════════════════════════════════════════════════════

class PythonStrategy : public BackendStrategy
{
    std::set<std::string> globalVars_;
    std::vector<std::string> pendingGlobals_;
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasDivOp_ = false, hasIpowOp_ = false, hasLengthOp_ = false, hasAddOp_ = false, hasRandomOp_ = false;
    void setUsedBuiltinOps(bool d, bool ip, bool l, bool a, bool r, bool /*idiv*/, bool /*eval*/, bool /*etry*/) override {
        hasDivOp_ = d; hasIpowOp_ = ip; hasLengthOp_ = l; hasAddOp_ = a; hasRandomOp_ = r;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        emit(out, indent, "_ac_saved.append(str(" + val + ") + \"\\n\")");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "open(" + filename + ", \"w\").write(\"\".join(_ac_saved))");
    }
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string,std::set<std::string>> importSymbols_;
    std::map<std::string, IRType> varCastTypes_;
    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    bool isAtomicVar(const std::string& var) const {
        auto it = varCastTypes_.find(var);
        return it != varCastTypes_.end() && it->second == IRType::ATOMIC;
    }
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }

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
        if (needsSave_) emitRaw(out, "_ac_saved = []  # `save as`: accumulates everything printed so far");
        if (anyAtomicVars()) {
            emitRaw(out, "import threading");
            emitRaw(out, "_ac_atomic_lock = threading.Lock()  # `atomic` vars: any op touching one is a global critical section");
        }
        // Each of these runtime helpers is only emitted when the PROGRAM actually needs it (see
        // setUsedBuiltinOps' comment) — matching how `range`/`bxor` etc. already only inline
        // what's used, rather than dumping every possible builtin into every generated file
        // regardless of whether it's ever called (verified real waste: `ac_pow` — a leftover
        // for `^` from before `ac_ipow` took over that role — was NEVER called by any generated
        // Python output at all; deleted outright rather than gated, since gating something with
        // zero call sites is pointless).
        // `/` smart division: int result when the division is exact, else float (#16)
        if (hasDivOp_) {
            emitRaw(out, "def ac_div(a, b):");
            emitRaw(out, "    q = a / b");
            emitRaw(out, "    qi = int(q)");
            emitRaw(out, "    return qi if qi == q else q");
        }
        // `length` builtin: works on strings, lists, and dicts alike
        if (hasLengthOp_) {
            emitRaw(out, "def ac_length(x):");
            emitRaw(out, "    return len(x)");
        }
        // AC `+` is arithmetic for numbers, concat for strings. Static coercion only fired when a
        // quoted literal was present → `count + name` (int var + str var) hit Python's TypeError.
        // Runtime dispatch handles every combination (str+int, int+str, num+num, list+list).
        if (hasAddOp_) {
            emitRaw(out, "def _ac_add(a, b):");
            emitRaw(out, "    if isinstance(a, str) or isinstance(b, str):");
            emitRaw(out, "        return str(a) + str(b)");
            emitRaw(out, "    return a + b");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "def ac_iota(n):");
        emitRaw(out, "    return ''.join(str(i) for i in range(int(n)))");
        // `^` operator: integer power
        if (hasIpowOp_) {
            emitRaw(out, "def ac_ipow(b, e):");
            emitRaw(out, "    return b ** e");
        }
        if (hasRandomOp_) {
            emitRaw(out, "import random as _acr");
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "def ac_rand(n):");
            emitRaw(out, "    return _acr.randrange(int(n)) if n else 0");
            // `random.choice(list)`
            emitRaw(out, "def ac_choice(xs):");
            emitRaw(out, "    return _acr.choice(xs)");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "def ac_stream(a, b, s=1):");
        emitRaw(out, "    return ''.join(str(i) for i in range(int(a), int(b), int(s)))");
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
            emitRaw(out, "def _ac_bind(key, fn):");
            emitRaw(out, "    _ac_events[key] = fn");
            emitRaw(out, "def _ac_trigger(key):");
            emitRaw(out, "    if key in _ac_events:");
            emitRaw(out, "        _ac_events[key]()");
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
        // Python has no fixed-width int type, so short/mini are plain ints (width is advisory here).
        else if (t == IRType::INT || t == IRType::SHORT || t == IRType::MINI)
                                      emit(out, indent, var + " = int(" + src + ")");
        else if (t == IRType::ATOMIC) emit(out, indent, "with _ac_atomic_lock: " + var + " = int(" + src + ")");
        else if (t == IRType::STRING) emit(out, indent, var + " = str(" + src + ")");
        else if (t == IRType::BOOL)   emit(out, indent, var + " = bool(" + src + ")");
    }
    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (isAtomicVar(var)) emit(out, indent, "with _ac_atomic_lock: " + var + " = " + val);
        else emit(out, indent, var + " = " + val);
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // When + involves a string operand, coerce both sides to str() so Python doesn't
        // throw "can only concatenate str (not 'int') to str".
        if (op == "+" && (looksString(lhs) || looksString(rhs))) {
            auto wrap = [](const std::string& v) {
                return looksString(v) ? v : "str(" + v + ")";
            };
            emit(out, indent, res + " = " + wrap(lhs) + " + " + wrap(rhs));
        } else if (op == "+") {
            // Neither side is a known string literal, but a variable could hold a string at runtime
            // (`count + name`). _ac_add dispatches: str+anything → concat, else numeric add.
            emit(out, indent, res + " = _ac_add(" + lhs + ", " + rhs + ")");
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
    // `sure $msg$` (browser confirm()) has no Python UI to ask through — the base
    // BackendStrategy::emitConfirm default is a pure no-op that never assigns `res` at all,
    // which is fine when the result is discarded (`sure $x$` alone) but crashes with
    // "NameError: name 't_N' is not defined" the moment a caller actually captures it
    // (`result = sure $x$`). Print the prompt for visibility and default to False (auto-
    // decline) so the assignment is always well-defined outside a browser.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        emit(out, indent, "print(" + val + ")");
        if (!res.empty()) emit(out, indent, res + " = False");
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
        } else if (type == "string") {
            // Matches CStrategy's own `type == "string"` case (see its comment) — without this,
            // a genuinely string-typed ALLOC fell through to the generic list-literal branch
            // below, splicing the bare, unquoted content text directly into a `[...]` literal —
            // Python then parsed it as a VARIABLE REFERENCE, not a string (verified real bug:
            // gl_bounce.ac's promoted `Ball` object name — `ac_gl_obj_create("Ball"); Ball =
            // [Ball]` — "NameError: name 'Ball' is not defined", since `Ball` the variable
            // doesn't exist yet at that point; the source clearly meant `Ball = "Ball"`).
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
    void emitTrueDivision(std::ostringstream &out, int &indent, const std::string &res,
                          const std::string &lhs, const std::string &rhs) override
    {   // `/` = SMART: int when it divides evenly, else float — exact on dynamic PY (#16)
        emit(out, indent, res + " = ac_div(" + lhs + ", " + rhs + ")");
    }
    void emitFloatDivision(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &lhs, const std::string &rhs) override
    {   // `///` = always-float
        emit(out, indent, res + " = (" + lhs + ") / (" + rhs + ")");
    }
    void emitJumpIfFalse(std::ostringstream &out, int indent,
                         const std::string &cond, const std::string &label) override
    {
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if not (" + cond + "): break");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if not (" + cond + "): continue");
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
protected:
    std::set<std::string> declared;   // protected so HTMLStrategy shares ONE set (was shadowed → let x twice)
    // protected so HTMLStrategy's own emitHeader can see what was imported — HTML never had
    // its own way to know, which is exactly why AC->HTML loaded ZERO ilib content for ANY
    // library (every `use ilib X` threw ReferenceError in a real browser). Fixed below.
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    // JS numbers have no int/float distinction at runtime — 9 and 9.0 are the SAME value, so
    // printing a whole-number float (`to_dec(9)`) via plain console.log loses the decimal point
    // AC's Python reference always shows (repr(9.0) == "9.0"). Track which var/temp names were
    // produced by a FLOAT type-cast so emitPrint can route them through `_acpf` (forces ".0" on
    // whole numbers) instead of the default `_acp` — a real value-vs-display bug, not cosmetic
    // (silently makes a float look like an int in the program's actual output).
    std::set<std::string> floatVars_;
private:
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false, hasIdivOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool idiv, bool /*eval*/, bool /*etry*/) override {
        hasIpowOp_ = ip; hasRandomOp_ = r; hasIdivOp_ = idiv;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        emit(out, indent, "_acCap(" + val + ");");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "require('fs').writeFileSync(" + filename + ", _ac_saved.join(''));");
    }
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
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "function ac_iota(n) {");
        emitRaw(out, "    let r = '';");
        emitRaw(out, "    for (let i = 0; i < n; i++) r += i;");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "function ac_stream(a, b, s) {");
        emitRaw(out, "    s = s || 1;");
        emitRaw(out, "    let r = '';");
        emitRaw(out, "    for (let i = a; i < b; i += s) r += i;");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // See setUsedBuiltinOps' comment on PythonStrategy — same "only emit what's actually
        // used" gating, ported here (verified same dead-weight pattern: `ac_ipow`/`ac_idiv`/
        // `ac_rand`/`ac_choice` were unconditionally injected into every generated file
        // regardless of whether the program uses `^`, `//`, or `random.*`).
        // `^` operator: integer power
        if (hasIpowOp_) {
            emitRaw(out, "function ac_ipow(b, e) {");
            emitRaw(out, "    let r = 1;");
            emitRaw(out, "    for (let i = 0; i < e; i++) r *= b;");
            emitRaw(out, "    return r;");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "function ac_rand(n) {");
            emitRaw(out, "    return n > 0 ? Math.floor(Math.random() * Number(n)) : 0;");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "function ac_choice(xs) {");
            emitRaw(out, "    return xs[Math.floor(Math.random() * xs.length)];");
            emitRaw(out, "}");
        }
        emitRaw(out, "function _acp(x) {");
        emitRaw(out, "    console.log(Array.isArray(x) ? '[' + x.join(', ') + ']' : x);");
        emitRaw(out, "}");
        // JS has no int/float distinction at runtime (9 === 9.0), so a whole-number value from a
        // FLOAT type-cast (`to_dec(9)`) would print as "9" via plain console.log, losing the
        // decimal point AC's Python reference always shows. Only used at call sites the compiler
        // knows are float-typed (see JavaScriptStrategy::floatVars_) — never applied to genuine ints.
        emitRaw(out, "function _acpf(x) {");
        emitRaw(out, "    console.log((typeof x === 'number' && Number.isInteger(x)) ? x.toFixed(1) : x);");
        emitRaw(out, "}");
        // `//` truncating integer division
        if (hasIdivOp_) {
            emitRaw(out, "function ac_idiv(a, b) {");
            emitRaw(out, "    if (b === 0) throw new Error('3rd grade mathematics violated (ZeroDivisionError)');");
            emitRaw(out, "    return Math.trunc(a / b);");
            emitRaw(out, "}");
        }
        if (needsSave_) {
            emitRaw(out, "// `save as`: accumulates everything printed so far");
            emitRaw(out, "const _ac_saved = [];");
            emitRaw(out, "function _acCap(x) {");
            emitRaw(out, "    _ac_saved.push((Array.isArray(x) ? '[' + x.join(', ') + ']' : x) + '\\n');");
            emitRaw(out, "}");
        }
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib" && ln == "math") {
                // JS math = native Math — the FFI needed the ffi-napi npm module; the browser
                // (HTML backend inherits this) has no FFI at all. Math.* covers the surface.
                emitRaw(out, "const math = {");
                emitRaw(out, "  pi: Math.PI, e: Math.E, tau: 2*Math.PI, phi: 1.6180339887498949, em: 0.5772156649015329,");
                emitRaw(out, "  sin: Math.sin, cos: Math.cos, tan: Math.tan, asin: Math.asin, acos: Math.acos, atan: Math.atan,");
                emitRaw(out, "  sqrt: Math.sqrt, cbrt: Math.cbrt, exp: Math.exp, ln: Math.log, log2: Math.log2, log10: Math.log10,");
                emitRaw(out, "  log: (b, x) => Math.log(x)/Math.log(b), pow: Math.pow, hypot: Math.hypot,");
                emitRaw(out, "  floor: Math.floor, ceil: Math.ceil, round: Math.round, abs: Math.abs, abs_int: Math.abs,");
                emitRaw(out, "  min: Math.min, max: Math.max,");
                emitRaw(out, "  mod: (a, b) => { let r = a % b; if (r !== 0 && (r < 0) !== (b < 0)) r += b; return r; },");
                emitRaw(out, "  mod_int: (a, b) => { let r = Math.trunc(a) % Math.trunc(b); if (r !== 0 && (r < 0) !== (b < 0)) r += Math.trunc(b); return r; },");
                emitRaw(out, "  is_prime: (n) => { if (n < 2) return 0; for (let d = 2; d*d <= n; d++) if (n % d === 0) return 0; return 1; },");
                emitRaw(out, "  to_int: Math.trunc, deg2rad: (x) => x*Math.PI/180, rad2deg: (x) => x*180/Math.PI,");
                emitRaw(out, "};");
                continue;
            }
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
            emitRaw(out, "function _acBind(key, fn) {");
            emitRaw(out, "    _acEvents[key] = fn;");
            emitRaw(out, "}");
            emitRaw(out, "function _acTrigger(key) {");
            emitRaw(out, "    if (_acEvents[key]) _acEvents[key]();");
            emitRaw(out, "}");
        }
        for (auto& v : promotedGlobals_)
            emitRaw(out, "var " + v + ";");
        emitRaw(out, "");
    }

    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        // _acBind registers a real document.addEventListener('keydown', ...) that
        // compares against the browser's own KeyboardEvent.key — translate the AC
        // key name (e.g. "space") to what the browser actually reports (" "),
        // per keybinds.js, instead of comparing against the AC name literally.
        emit(out, indent, "_acBind(" + translateJsKeyConstant(key) + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_acTrigger(" + translateJsKeyConstant(key) + ");");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym, "true", "false", "null", "[]");
        // Bundle field READ (self.field as an operand): translate to the real JS member access.
        if (s.rfind("self.", 0) == 0) return "this." + s.substr(5);
        return s;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...) inside a method: a real class field, assigned
        // via `this.field`, never `let`-redeclared. `var` may arrive raw ("self.field") or
        // already translated ("this.field", from formatRef() on the READ side) — handle both.
        if (var.rfind("self.", 0) == 0) return "this." + var.substr(5) + " = " + val + ";";
        if (var.rfind("this.", 0) == 0) return var + " = " + val + ";";
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        std::string expr = lhs + " " + op + " " + rhs;
        emit(out, indent, decl(res, expr));
    }
    void emitIntDiv(std::ostringstream &out, int &indent,
                    const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "ac_idiv(" + lhs + ", " + rhs + ")")); } // throws on 0 so try/catch fires
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
        if (func == "ac_length" && !res.empty()) { bool nw = declared.insert(res).second; emit(out, indent, (nw ? "let " : "") + res + " = (" + args + ").length;"); return; }
        { auto ap = func.rfind(".append");
          if (ap != std::string::npos && ap == func.size() - 7) {
              emit(out, indent, func.substr(0, ap) + ".push(" + args + ");"); return; } }
        std::string call = func + "(" + args + ")";
        emit(out, indent, res.empty() ? call + ";" : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        emit(out, indent, val.empty() ? "return;" : "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (floatVars_.count(val)) emit(out, indent, "_acpf(" + val + ");");
        else emit(out, indent, "_acp(" + val + ");");   // array-aware (Node column-wraps long arrays)
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
        // Real blocking sleep (Node) — Atomics.wait suspends the thread for the timeout
        // instead of the old while(Date.now()<_t) busy-wait that pinned a CPU core.
        // (HTML keeps its busy-wait: Atomics.wait throws on the browser main thread.)
        emit(out, indent, "{ Atomics.wait(new Int32Array(new SharedArrayBuffer(4)),0,0,Math.max(0,Math.floor(("+secs+")*1000))); }");
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
        } else if (type == "string") {
            // Same bug/fix as PythonStrategy's emitAlloc (see its comment) — a genuinely
            // string-typed ALLOC fell through to the generic list-literal branch, splicing the
            // bare unquoted content directly into a `[...]` literal — JS then parsed it as a
            // variable reference, not a string.
            emit(out, indent, "let " + var + " = \"" + content + "\";");
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
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if (!(" + cond + ")) continue;");
        else
            emit(out, indent, "if (!(" + cond + ")) { /* goto " + label + " */ }");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear();
        floatVars_.clear();
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
        // Register parameters as declared so reassigning one (`n = n // 2`) emits `n = ...`,
        // not `let n = ...` (which shadows the param and triggers a TDZ ReferenceError).
        { std::string cur; for (char c : jsParams) {
            if (c == ',') { size_t a=cur.find_first_not_of(' '); if(a!=std::string::npos){ size_t b=cur.find_last_not_of(' '); declared.insert(cur.substr(a,b-a+1)); } cur.clear(); }
            else cur += c; }
          size_t a=cur.find_first_not_of(' '); if(a!=std::string::npos){ size_t b=cur.find_last_not_of(' '); declared.insert(cur.substr(a,b-a+1)); } }
        for (const auto& [v, t] : hoistVars_) { (void)t;   // #41 JS: let is block-scoped → hoist
            if (declared.insert(v).second) emit(out, indent, "let " + v + " = 0;"); }
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
        floatVars_.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        indent++;
    }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // JS classes hard-require `new` — calling `ClassName()` bare throws "Class constructor
        // ClassName cannot be invoked without 'new'" at runtime.
        emit(out, indent, decl(res, "new " + className + "(" + args + ")"));
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
        if      (t == IRType::FLOAT)  { expr = "Number(" + src + ")"; floatVars_.insert(var); }
        // JS numbers have no fixed width, so short/mini are plain ints (width is advisory here).
        // `atomic` needs no lock here either — JS's run-to-completion model means no other JS code
        // can ever interleave mid-statement (the only way it could is `await` inside the statement,
        // which AC never generates for a plain assignment), so a single statement is already
        // uninterruptible for free. Real multi-thread races (Worker threads) aren't something AC's
        // JS backend exposes at all, so there's nothing to protect against beyond what's already true.
        else if (t == IRType::INT || t == IRType::SHORT || t == IRType::MINI || t == IRType::ATOMIC)
                                      expr = "Math.trunc(Number(" + src + "))";
        else if (t == IRType::STRING) expr = "String(" + src + ")";
        else if (t == IRType::BOOL)   expr = "Boolean(" + src + ") ? 1 : 0";
        else return;
        emit(out, indent, decl(var, expr));
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        floatVars_.clear();
        for (auto& v : promotedGlobals_) declared.insert(v);
        emit(out, indent, "(function() {");
        indent++;
        for (const auto& [v, t] : hoistVars_) { (void)t;   // #41 JS mainloop hoist
            if (declared.insert(v).second) emit(out, indent, "let " + v + " = 0;"); }
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

// HTML target = JS that renders into the DOM. Inherit ALL of JS's logic (arrays, indexing,
// strings, calls, etc.) and override ONLY the browser-output bits below (header/footer/print).
// This kills the "HTML is a stale JS copy" drift (#14) — one place to maintain.
class HTMLStrategy : public JavaScriptStrategy
{
    // NOTE: `declared` is intentionally NOT redeclared here — it now lives in JavaScriptStrategy
    // as protected, so inherited emitCall/emitReturn/emitConstDecl and HTML's own overrides all
    // share ONE set. A shadow here made `x=5` then `x=foo()` both emit `let x` → JS redeclare error.
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false, hasIdivOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool idiv, bool /*eval*/, bool /*etry*/) override {
        hasIpowOp_ = ip; hasRandomOp_ = r; hasIdivOp_ = idiv;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        emit(out, indent, "_acCap(" + val + ");");
    }
    // No filesystem in a browser — trigger a real download via a Blob + synthetic anchor click
    // (the standard client-side "Save As" idiom), not a stub.
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "{ const _b = new Blob([_ac_saved.join('')], {type: 'text/plain'}); "
                           "const _u = URL.createObjectURL(_b); const _a = document.createElement('a'); "
                           "_a.href = _u; _a.download = " + filename + "; _a.click(); URL.revokeObjectURL(_u); }");
    }
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
        // Same whole-number-float fix as JS's `_acpf` (see JavaScriptStrategy::emitHeader) — only
        // used at call sites the compiler knows are float-typed (JavaScriptStrategy::floatVars_).
        emitRaw(out, "function _printf(v) { _el.appendChild(document.createTextNode(((typeof v === 'number' && Number.isInteger(v)) ? v.toFixed(1) : v) + '\\n')); }");
        emitRaw(out, "function _printHTML(h) { const d = document.createElement('div'); d.innerHTML = h; _el.appendChild(d); }");
        // HTML-escape any value before it goes into innerHTML/attributes — a displayed value
        // containing <script>…</script> or `\"` used to inject markup/JS into the page (XSS).
        emitRaw(out, "function _esc(s){ return String(s).replace(/[&<>\"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c])); }");
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "function ac_iota(n) {");
        emitRaw(out, "    let r = '';");
        emitRaw(out, "    for (let i = 0; i < n; i++) r += i;");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "function ac_stream(a, b, s) {");
        emitRaw(out, "    s = s || 1;");
        emitRaw(out, "    let r = '';");
        emitRaw(out, "    for (let i = a; i < b; i += s) r += i;");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually uses it. `ac_idiv` in particular was a genuine live bug: HTML's
        // emitIntDiv (see below) always calls it, but it was NEVER DEFINED anywhere in this
        // header — any AC program using `//` on this backend threw a real browser
        // "ac_idiv is not defined" ReferenceError (verified: examples/count_digits.ac).
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "function ac_ipow(b, e) {");
            emitRaw(out, "    let r = 1;");
            emitRaw(out, "    for (let i = 0; i < e; i++) r *= b;");
            emitRaw(out, "    return r;");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "function ac_rand(n) {");
            emitRaw(out, "    return n > 0 ? Math.floor(Math.random() * Number(n)) : 0;");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "function ac_choice(xs) {");
            emitRaw(out, "    return xs[Math.floor(Math.random() * xs.length)];");
            emitRaw(out, "}");
        }
        if (hasIdivOp_) {
            // `//` truncating integer division
            emitRaw(out, "function ac_idiv(a, b) {");
            emitRaw(out, "    if (b === 0) throw new Error('3rd grade mathematics violated (ZeroDivisionError)');");
            emitRaw(out, "    return Math.trunc(a / b);");
            emitRaw(out, "}");
        }
        emitRaw(out, "function _acp(x) {");
        emitRaw(out, "    console.log(Array.isArray(x) ? '[' + x.join(', ') + ']' : x);");
        emitRaw(out, "}");
        if (needsSave_) {
            emitRaw(out, "// `save as`: accumulates everything printed so far");
            emitRaw(out, "const _ac_saved = [];");
            emitRaw(out, "function _acCap(x) {");
            emitRaw(out, "    _ac_saved.push((Array.isArray(x) ? '[' + x.join(', ') + ']' : x) + '\\n');");
            emitRaw(out, "}");
        }
        // AC->HTML used to load ZERO ilib content for ANY library — inheriting JS's `pendingImports_`
        // (now protected, shared) fixes that. GUI-ish libraries need a browser-DOM implementation,
        // not JS's Node-only native-FFI one, so they're special-cased to a raw top-level file
        // (e.g. `widgets.js`) instead of `ffi/<lib>_ffi.js`; everything else reuses the exact same
        // FFI file JS does (works as-is for pure-computation ilibs; Node-only ones like os/regex
        // that shell out or touch the filesystem won't function in a real browser regardless of
        // what's inlined here — that's an inherent limitation of the library, not this loader).
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib" && ln == "math") {
                emitRaw(out, "const math = {");
                emitRaw(out, "  pi: Math.PI, e: Math.E, tau: 2*Math.PI, phi: 1.6180339887498949, em: 0.5772156649015329,");
                emitRaw(out, "  sin: Math.sin, cos: Math.cos, tan: Math.tan, asin: Math.asin, acos: Math.acos, atan: Math.atan,");
                emitRaw(out, "  sqrt: Math.sqrt, cbrt: Math.cbrt, exp: Math.exp, ln: Math.log, log2: Math.log2, log10: Math.log10,");
                emitRaw(out, "  log: (b, x) => Math.log(x)/Math.log(b), pow: Math.pow, hypot: Math.hypot,");
                emitRaw(out, "  floor: Math.floor, ceil: Math.ceil, round: Math.round, abs: Math.abs, abs_int: Math.abs,");
                emitRaw(out, "  min: Math.min, max: Math.max,");
                emitRaw(out, "  mod: (a, b) => { let r = a % b; if (r !== 0 && (r < 0) !== (b < 0)) r += b; return r; },");
                emitRaw(out, "  mod_int: (a, b) => { let r = Math.trunc(a) % Math.trunc(b); if (r !== 0 && (r < 0) !== (b < 0)) r += Math.trunc(b); return r; },");
                emitRaw(out, "  is_prime: (n) => { if (n < 2) return 0; for (let d = 2; d*d <= n; d++) if (n % d === 0) return 0; return 1; },");
                emitRaw(out, "  to_int: Math.trunc, deg2rad: (x) => x*Math.PI/180, rad2deg: (x) => x*180/Math.PI,");
                emitRaw(out, "};");
                continue;
            }
            if (lt == "ilib" && ln == "widgets") {
                std::string dom = readIlibRawFile("widgets", "widgets.js");
                if (!dom.empty()) out << dom << "\n";
                continue;
            }
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
            emitRaw(out, "function _acBind(key, fn) {");
            emitRaw(out, "    _acEvents[key] = fn;");
            emitRaw(out, "    document.addEventListener('keydown', function(e) {");
            emitRaw(out, "        if (e.key === key) fn();");
            emitRaw(out, "    });");
            emitRaw(out, "}");
            emitRaw(out, "function _acTrigger(key) {");
            emitRaw(out, "    if (_acEvents[key]) _acEvents[key]();");
            emitRaw(out, "}");
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
        // _acBind registers a real document.addEventListener('keydown', ...) that
        // compares against the browser's own KeyboardEvent.key — translate the AC
        // key name (e.g. "space") to what the browser actually reports (" "),
        // per keybinds.js, instead of comparing against the AC name literally.
        emit(out, indent, "_acBind(" + translateJsKeyConstant(key) + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_acTrigger(" + translateJsKeyConstant(key) + ");");
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym, "true", "false", "null", "[]");
        // Bundle field READ (self.field as an operand): translate to the real JS member access.
        if (s.rfind("self.", 0) == 0) return "this." + s.substr(5);
        return s;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...) inside a method: a real class field, assigned
        // via `this.field`, never `let`-redeclared. `var` may arrive raw ("self.field") or
        // already translated ("this.field", from formatRef() on the READ side) — handle both.
        if (var.rfind("self.", 0) == 0) return "this." + var.substr(5) + " = " + val + ";";
        if (var.rfind("this.", 0) == 0) return var + " = " + val + ";";
        if (declared.insert(var).second)
            return "let " + var + " = " + val + ";";
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        emit(out, indent, decl(var, val));
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        emit(out, indent, decl(res, lhs + " " + op + " " + rhs));
    }
    void emitIntDiv(std::ostringstream &out, int &indent,
                    const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "ac_idiv(" + lhs + ", " + rhs + ")")); } // throws on 0 so try/catch fires
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
    // emitCall/emitReturn/arrays/strings all inherited from JavaScriptStrategy now.
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (floatVars_.count(val)) emit(out, indent, "_printf(" + val + ");");
        else emit(out, indent, "_print(" + val + ");");  // render into the page, not the console
    }
    void emitStyledPrint(std::ostringstream &out, int &indent,
                         const std::string &val, const std::string &style) override
    {
        if (style == "title")
            emit(out, indent, "document.title = String(" + val + ");");   // textual, not innerHTML — safe
        else if (style == "bold")
            emit(out, indent, "_printHTML('<b>' + _esc(" + val + ") + '</b>');");
        else if (style == "italic")
            emit(out, indent, "_printHTML('<i>' + _esc(" + val + ") + '</i>');");
        else if (style == "header")
            emit(out, indent, "_printHTML('<h2>' + _esc(" + val + ") + '</h2>');");
        else if (style == "link")
            emit(out, indent, "_printHTML('<a href=\"' + _esc(" + val + ") + '\">' + _esc(" + val + ") + '</a>');");
        else if (style == "code")
            emit(out, indent, "_printHTML('<code>' + _esc(" + val + ") + '</code>');");
        else if (style == "para")
            emit(out, indent, "_printHTML('<p>' + _esc(" + val + ") + '</p>');");
        else if (style == "underline")
            emit(out, indent, "_printHTML('<u>' + _esc(" + val + ") + '</u>');");
        else if (style == "mark")
            emit(out, indent, "_printHTML('<mark>' + _esc(" + val + ") + '</mark>');");
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
        } else if (type == "string") {
            // Same bug/fix as PythonStrategy/JavaScriptStrategy's emitAlloc (see their comment).
            emit(out, indent, "let " + var + " = \"" + content + "\";");
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
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if (!(" + cond + ")) continue;");
        else
            emit(out, indent, "if (!(" + cond + ")) { /* goto " + label + " */ }");
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear();
        floatVars_.clear();
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
        // Register parameters as declared so reassigning one (`n = n // 2`) emits `n = ...`,
        // not `let n = ...` (which shadows the param and triggers a TDZ ReferenceError).
        { std::string cur; for (char c : jsParams) {
            if (c == ',') { size_t a=cur.find_first_not_of(' '); if(a!=std::string::npos){ size_t b=cur.find_last_not_of(' '); declared.insert(cur.substr(a,b-a+1)); } cur.clear(); }
            else cur += c; }
          size_t a=cur.find_first_not_of(' '); if(a!=std::string::npos){ size_t b=cur.find_last_not_of(' '); declared.insert(cur.substr(a,b-a+1)); } }
        for (const auto& [v, t] : hoistVars_) { (void)t;   // #41 JS: let is block-scoped → hoist
            if (declared.insert(v).second) emit(out, indent, "let " + v + " = 0;"); }
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
        floatVars_.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "class " + name + " {");
        indent++;
    }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // JS classes hard-require `new` — calling `ClassName()` bare throws "Class constructor
        // ClassName cannot be invoked without 'new'" at runtime.
        emit(out, indent, decl(res, "new " + className + "(" + args + ")"));
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
        if      (t == IRType::FLOAT)  { expr = "Number(" + src + ")"; floatVars_.insert(var); }
        // JS numbers have no fixed width, so short/mini are plain ints (width is advisory here).
        // `atomic` needs no lock here either — JS's run-to-completion model means no other JS code
        // can ever interleave mid-statement (the only way it could is `await` inside the statement,
        // which AC never generates for a plain assignment), so a single statement is already
        // uninterruptible for free. Real multi-thread races (Worker threads) aren't something AC's
        // JS backend exposes at all, so there's nothing to protect against beyond what's already true.
        else if (t == IRType::INT || t == IRType::SHORT || t == IRType::MINI || t == IRType::ATOMIC)
                                      expr = "Math.trunc(Number(" + src + "))";
        else if (t == IRType::STRING) expr = "String(" + src + ")";
        else if (t == IRType::BOOL)   expr = "Boolean(" + src + ") ? 1 : 0";
        else return;
        emit(out, indent, decl(var, expr));
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear();
        floatVars_.clear();
        for (auto& v : promotedGlobals_) declared.insert(v);
        emit(out, indent, "(function() {");
        indent++;
        for (const auto& [v, t] : hoistVars_) { (void)t;   // #41 JS mainloop hoist
            if (declared.insert(v).second) emit(out, indent, "let " + v + " = 0;"); }
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
    std::set<std::string> classInstanceVars_;   // vars holding a bundle instance (see emitConstructCall)
    std::set<std::string> bundleStringFields_;  // FIELD names (not full paths) known to be string-typed
    // Shadows (not overrides — the base isn't virtual) BackendStrategy::isStringVar for every
    // call within CStrategy's own methods. C never populates the base `stringVars_` set at all
    // (no setStringVars() override exists) — the ONLY string-detection C had was `looksString`
    // (syntactic, literal-only) and its own incrementally-built `strVars`. Neither one ever
    // gets populated for a bundle field: `decl()`'s self.-branch (for the write) and every
    // caller elsewhere (for reads like `Term.display self.name` or `Term.display c.name`)
    // bypass that bookkeeping entirely, since fields aren't "a var that got a string value at
    // some point in THIS function" — they're a named, typed STRUCT MEMBER declared once,
    // globally, via emitFieldDecl. Field-NAME-based (not full-path-based) is the only heuristic
    // that works for both the self->field (write side, inside a method) and instance.field
    // (read side, outside it, on a completely different variable) spellings of the same field.
    bool isStringVar(const std::string& v) const {
        if (stringVars_.count(v)) return true;
        auto arrow = v.rfind("->");
        if (arrow != std::string::npos) return bundleStringFields_.count(v.substr(arrow + 2)) > 0;
        auto dot = v.rfind('.');
        if (dot != std::string::npos) return bundleStringFields_.count(v.substr(dot + 1)) > 0;
        return false;
    }
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(v);   // #42: pre-inferred float locals
    }
    std::set<std::string> listVars;
    std::set<std::string> strVars;   // vars holding char* (e.g. iota/stream results)
    std::set<std::string> userFloatFuncs_;
    std::map<std::string, std::string> widgetVars_; // C widget handles: var -> constructor kind
    std::map<std::string, int> userFuncArity_;
protected:
    std::set<std::string> promotedGlobals_; // free vars hoisted to file scope (NA→free)
public:
    void setPromotedGlobals(const std::vector<std::string> &vars) override
    {
        for (auto& v : vars) promotedGlobals_.insert(v);
    }
    void setStructGlobals(const std::map<std::string, std::string> &m) override
    {
        for (const auto& [var, kind] : m) widgetVars_[var] = kind;
    }
private:
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
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
    bool returnIsVoid_ = false;
    void setUserFuncArity(const std::map<std::string, int>& m) override { userFuncArity_ = m; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    // #retstring: CStrategy never consulted baseReturnIsString_ at all — a user function
    // returning a bare string literal (e.g. via `cond`/`return $prime$`, examples/showcase.ac's
    // `describe`) got the generic `ac_int` default both at its own definition AND every call
    // site, silently printing the STRING'S POINTER VALUE as a number instead of the text. Same
    // bug class already found+fixed for Rust this session — found here via bundle-work
    // regression testing, unrelated to bundles.
    std::set<std::string> userStringFuncs_;
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
    bool isUserStringReturningFunc(const std::string& fn) const { return userStringFuncs_.count(fn) > 0; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false, hasEvalOp_ = false, hasIdivOp_ = false, hasTryOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool idiv, bool ev, bool etry) override {
        hasIpowOp_ = ip; hasRandomOp_ = r; hasEvalOp_ = ev; hasIdivOp_ = idiv; hasTryOp_ = etry;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        if (looksString(val) || strVars.count(val) || isStringVar(val))
            emit(out, indent, "ac_save_append(" + val + ");");
        else if (isFloatVal(val))
            emit(out, indent, "ac_save_append_double((double)(" + val + "));");
        else
            emit(out, indent, "ac_save_append_int((long long)(" + val + "));");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "{ FILE* _f = fopen(" + filename + ", \"w\"); if (_f) { fwrite(_ac_saved_buf, 1, _ac_saved_len, _f); fclose(_f); } }");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_ac_bind(" + key + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_ac_trigger(" + key + ");");
    }
    // Fixed-width int type name for `short`/`mini`, sourced from the type include (type.hpp).
    static const char* acIntTy(IRType t)   { return acIntTypeC(irIntWidth(t)); }
    static bool isNarrowInt(IRType t)       { return irIntWidth(t) != 0; }
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
        if (anyAtomicVars()) {
            emitRaw(out, "#include <pthread.h>");
            emitRaw(out, "static pthread_mutex_t _ac_atomic_lock = PTHREAD_MUTEX_INITIALIZER; /* `atomic` vars: any op touching one is a global critical section */");
        }
        if (needsEvents_) {
            // `configure event-listener` / `bind KEY to FUNC`: 64-slot parallel-array table,
            // linear-scan on trigger (a hash map isn't worth reaching for real C keybindings).
            emitRaw(out, "typedef void (*ac_evfn)(void);");
            emitRaw(out, "static const char* _ac_ev_keys[64];");
            emitRaw(out, "static ac_evfn _ac_ev_fns[64];");
            emitRaw(out, "static int _ac_ev_n = 0;");
            emitRaw(out, "static void _ac_bind(const char* key, ac_evfn fn){ _ac_ev_keys[_ac_ev_n]=key; _ac_ev_fns[_ac_ev_n]=fn; _ac_ev_n++; }");
            emitRaw(out, "static void _ac_trigger(const char* key){ for(int _i=0;_i<_ac_ev_n;_i++) if(strcmp(_ac_ev_keys[_i],key)==0){ _ac_ev_fns[_i](); return; } }");
        }
        // Emit ilib C headers at file scope before main()
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string absDir = resolveIlibDir(ln);
                emitRaw(out, "#include \"" + absDir + "/" + ln + "_c.h\"");
                {
                    // .so names drop hyphens (libacstringcheese.so) + historical quirks
                    std::string _lnk;
                    if (ln == "machine-audio") _lnk = "acmachinaaudio";
                    else if (ln == "web-server") _lnk = "acserver";
                    else if (ln == "native-cpu") _lnk = "acncpu";
                    else if (ln == "os")       _lnk = "acoos";   // libacoos.so (sic)
                    else { _lnk = "ac"; for (char c : ln) if (c != '-') _lnk += c; }
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
            } else if (lt == "conglomer") {
                // conglomer <foo.h>: include the raw C header + DYNAMICALLY link lib<stem>.so
                emitRaw(out, "#include \"" + ln + "\"");
                auto slash = ln.rfind('/');
                std::string dir  = (slash == std::string::npos) ? "." : ln.substr(0, slash);
                std::string base = (slash == std::string::npos) ? ln : ln.substr(slash + 1);
                std::string stem = base.substr(0, base.rfind('.'));
                if (stem.rfind("lib", 0) == 0) stem = stem.substr(3);
                emitRaw(out, "// CONGLOMER: " + ln + " (dynamic)");
                emitRaw(out, "// Link: gcc output.c -I" + dir + " -L" + dir + " -l" + stem
                             + " -Wl,-rpath," + dir);
            }
        }
        emitRaw(out, "");
        emitRaw(out, "#include <stdint.h>");   // int32_t/int16_t for `short`/`mini` typed vars
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        emitRaw(out, "static const char* lazy = \"lazy\";");
        emitRaw(out, "static void _ac_widget_call0(void* fn) {");
        emitRaw(out, "    ((ac_int (*)(void))fn)();");
        emitRaw(out, "}");
        emitRaw(out, "static void _ac_widget_call1(void* fn) {");
        emitRaw(out, "    ((ac_int (*)(ac_int))fn)(0);");
        emitRaw(out, "}");
        // Dynamic-array runtime (#8/#C1): stretchy buffer — [len][cap][data...], user pointer aims
        // at data so arr[i] stays raw-pointer speed. Length travels WITH the pointer (works for
        // params/returns). Push reallocs (x2) and returns the (possibly moved) data pointer.
        emitRaw(out, "static ac_int* ac_arr_new(ac_int n) {");
        emitRaw(out, "    ac_int cap = n > 4 ? n : 4;");
        emitRaw(out, "    ac_int* p = (ac_int*)malloc((size_t)(2 + cap) * sizeof(ac_int));");
        emitRaw(out, "    p[0] = n;");
        emitRaw(out, "    p[1] = cap;");
        emitRaw(out, "    return p + 2;");
        emitRaw(out, "}");
        emitRaw(out, "static ac_int ac_arr_len(const ac_int* a) {");
        emitRaw(out, "    return a ? a[-2] : 0;");
        emitRaw(out, "}");
        emitRaw(out, "static ac_int* ac_arr_push(ac_int* a, ac_int v) {");
        emitRaw(out, "    if (!a) a = ac_arr_new(0);");
        emitRaw(out, "    ac_int len = a[-2], cap = a[-1];");
        emitRaw(out, "    if (len == cap) {");
        emitRaw(out, "        cap *= 2;");
        emitRaw(out, "        a = (ac_int*)realloc(a - 2, (size_t)(2 + cap) * sizeof(ac_int)) + 2;");
        emitRaw(out, "        a[-1] = cap;");
        emitRaw(out, "    }");
        emitRaw(out, "    a[len] = v;");
        emitRaw(out, "    a[-2] = len + 1;");
        emitRaw(out, "    return a;");
        emitRaw(out, "}");
        emitRaw(out, "static void ac_arr_print(const ac_int* a) {");
        emitRaw(out, "    printf(\"[\");");
        emitRaw(out, "    for (ac_int i = 0; i < ac_arr_len(a); i++)");
        emitRaw(out, "        printf(i ? \", %lld\" : \"%lld\", (long long)a[i]);");
        emitRaw(out, "    printf(\"]\\n\");");
        emitRaw(out, "}");
        // `try`/`catch`: real via setjmp/longjmp, not `exit()` — exit() terminates the whole
        // process, unconditionally uncatchable; a real runtime error inside a `try` needs to
        // unwind to the matching `catch` instead. A small fixed-depth stack of jmp_bufs
        // supports nested try blocks (32 is far past any realistic nesting depth). Any runtime
        // helper that can fail at a point AC considers "catchable" (currently just division by
        // zero) checks `_ac_try_depth` first: inside a `try`, longjmp back to it; otherwise
        // fall through to the previous exit(1) behavior (an uncaught error still terminates).
        // Gated on hasTryOp_ (a real `try`/`catch` block) OR hasIdivOp_ (`//`, the only other
        // caller of this stack) — only emitted when the program actually needs either.
        if (hasTryOp_ || hasIdivOp_) {
            emitRaw(out, "#include <setjmp.h>");
            emitRaw(out, "static jmp_buf _ac_try_stack[32];");
            emitRaw(out, "static int _ac_try_depth = 0;");
        }
        if (hasIdivOp_) {
            emitRaw(out, "static ac_int ac_idiv(ac_int a, ac_int b) {");
            emitRaw(out, "    if (!b) {");
            emitRaw(out, "        if (_ac_try_depth > 0) longjmp(_ac_try_stack[_ac_try_depth - 1], 1);");
            emitRaw(out, "        fprintf(stderr, \"Preposterous: 3rd grade mathematics violated (ZeroDivisionError)\\n\");");
            emitRaw(out, "        exit(1);");
            emitRaw(out, "    }");
            emitRaw(out, "    return a / b;");
            emitRaw(out, "}");
        }
        emitRaw(out, "static const char* ac_concat(const char* a, const char* b) {");
        emitRaw(out, "    size_t la = strlen(a), lb = strlen(b);");
        emitRaw(out, "    char* r = (char*)malloc(la + lb + 1);");
        emitRaw(out, "    memcpy(r, a, la);");
        emitRaw(out, "    memcpy(r + la, b, lb + 1);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // int -> string (re-typing coercion)
        emitRaw(out, "static const char* ac_to_str(ac_int n) {");
        emitRaw(out, "    char* r = (char*)malloc(24);");
        emitRaw(out, "    snprintf(r, 24, \"%lld\", (long long)n);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // `%.16g` alone drops the trailing decimal point on a whole-number double (9.0 -> "9"),
        // silently making a float print indistinguishable from an int — wrong for any
        // expression-form decimal cast that happens to land on a whole number (`to_dec(9)`).
        // Match PY's repr(float) convention: always show at least one digit after the point.
        emitRaw(out, "static void _ac_dblprint(double d) {");
        emitRaw(out, "    char buf[64];");
        emitRaw(out, "    snprintf(buf, sizeof(buf), \"%.16g\", d);");
        emitRaw(out, "    if (!strpbrk(buf, \".eEnN\")) {");
        emitRaw(out, "        size_t l = strlen(buf);");
        emitRaw(out, "        snprintf(buf + l, sizeof(buf) - l, \".0\");");
        emitRaw(out, "    }");
        emitRaw(out, "    printf(\"%s\\n\", buf);");
        emitRaw(out, "}");
        if (needsSave_) {
            // `save as <file>` — fixed 1MB accumulator (native backends can afford far more
            // headroom than ASM's 64KB), appended to on every Term.display, written out in one
            // fwrite when the SAVE_FILE instruction fires.
            emitRaw(out, "static char _ac_saved_buf[1048576];");
            emitRaw(out, "static size_t _ac_saved_len = 0;");
            emitRaw(out, "static void ac_save_append(const char* s) {");
            emitRaw(out, "    size_t l = strlen(s);");
            emitRaw(out, "    if (_ac_saved_len + l + 2 < sizeof(_ac_saved_buf)) {");
            emitRaw(out, "        memcpy(_ac_saved_buf + _ac_saved_len, s, l);");
            emitRaw(out, "        _ac_saved_len += l;");
            emitRaw(out, "        _ac_saved_buf[_ac_saved_len++] = '\\n';");
            emitRaw(out, "    }");
            emitRaw(out, "}");
            emitRaw(out, "static void ac_save_append_int(long long v) {");
            emitRaw(out, "    char b[24];");
            emitRaw(out, "    snprintf(b, sizeof(b), \"%lld\", v);");
            emitRaw(out, "    ac_save_append(b);");
            emitRaw(out, "}");
            emitRaw(out, "static void ac_save_append_double(double d) {");
            emitRaw(out, "    char b[64];");
            emitRaw(out, "    snprintf(b, sizeof(b), \"%.16g\", d);");
            emitRaw(out, "    if (!strpbrk(b, \".eEnN\")) {");
            emitRaw(out, "        size_t l = strlen(b);");
            emitRaw(out, "        snprintf(b + l, sizeof(b) - l, \".0\");");
            emitRaw(out, "    }");
            emitRaw(out, "    ac_save_append(b);");
            emitRaw(out, "}");
        }
        // Dict runtime: string-keyed assoc (linear scan — fine for AC-scale dicts)
        emitRaw(out, "typedef struct { ac_int n, cap; const char** ks; ac_int* vs; } ac_dict;");
        emitRaw(out, "static ac_dict* ac_dict_new(void) {");
        emitRaw(out, "    ac_dict* d = (ac_dict*)malloc(sizeof(ac_dict));");
        emitRaw(out, "    d->n = 0;");
        emitRaw(out, "    d->cap = 8;");
        emitRaw(out, "    d->ks = (const char**)malloc(8 * sizeof(char*));");
        emitRaw(out, "    d->vs = (ac_int*)malloc(8 * sizeof(ac_int));");
        emitRaw(out, "    return d;");
        emitRaw(out, "}");
        emitRaw(out, "static void ac_dict_set(ac_dict* d, const char* k, ac_int v) {");
        emitRaw(out, "    for (ac_int i = 0; i < d->n; i++)");
        emitRaw(out, "        if (!strcmp(d->ks[i], k)) { d->vs[i] = v; return; }");
        emitRaw(out, "    if (d->n == d->cap) {");
        emitRaw(out, "        d->cap *= 2;");
        emitRaw(out, "        d->ks = (const char**)realloc(d->ks, d->cap * sizeof(char*));");
        emitRaw(out, "        d->vs = (ac_int*)realloc(d->vs, d->cap * sizeof(ac_int));");
        emitRaw(out, "    }");
        emitRaw(out, "    d->ks[d->n] = k;");
        emitRaw(out, "    d->vs[d->n] = v;");
        emitRaw(out, "    d->n++;");
        emitRaw(out, "}");
        emitRaw(out, "static ac_int ac_dict_get(const ac_dict* d, const char* k) {");
        emitRaw(out, "    for (ac_int i = 0; i < d->n; i++)");
        emitRaw(out, "        if (!strcmp(d->ks[i], k)) return d->vs[i];");
        emitRaw(out, "    fprintf(stderr, \"Preposterous: KeyError: %s\\n\", k);");
        emitRaw(out, "    exit(1);");
        emitRaw(out, "}");
        // String-valued sibling of ac_dict — a mixed-type dict literal (e.g. datac-imported
        // rows: {name:"Fido", age:"3"}) is homogenized to all-string by every backend, since
        // there's nowhere to record a second value type in a single flat map (see fmtDictValStr).
        emitRaw(out, "typedef struct { ac_int n, cap; const char** ks; const char** vs; } ac_sdict;");
        emitRaw(out, "static ac_sdict* ac_sdict_new(void) {");
        emitRaw(out, "    ac_sdict* d = (ac_sdict*)malloc(sizeof(ac_sdict));");
        emitRaw(out, "    d->n = 0;");
        emitRaw(out, "    d->cap = 8;");
        emitRaw(out, "    d->ks = (const char**)malloc(8 * sizeof(char*));");
        emitRaw(out, "    d->vs = (const char**)malloc(8 * sizeof(char*));");
        emitRaw(out, "    return d;");
        emitRaw(out, "}");
        emitRaw(out, "static void ac_sdict_set(ac_sdict* d, const char* k, const char* v) {");
        emitRaw(out, "    for (ac_int i = 0; i < d->n; i++)");
        emitRaw(out, "        if (!strcmp(d->ks[i], k)) { d->vs[i] = v; return; }");
        emitRaw(out, "    if (d->n == d->cap) {");
        emitRaw(out, "        d->cap *= 2;");
        emitRaw(out, "        d->ks = (const char**)realloc(d->ks, d->cap * sizeof(char*));");
        emitRaw(out, "        d->vs = (const char**)realloc(d->vs, d->cap * sizeof(char*));");
        emitRaw(out, "    }");
        emitRaw(out, "    d->ks[d->n] = k;");
        emitRaw(out, "    d->vs[d->n] = v;");
        emitRaw(out, "    d->n++;");
        emitRaw(out, "}");
        emitRaw(out, "static const char* ac_sdict_get(const ac_sdict* d, const char* k) {");
        emitRaw(out, "    for (ac_int i = 0; i < d->n; i++)");
        emitRaw(out, "        if (!strcmp(d->ks[i], k)) return d->vs[i];");
        emitRaw(out, "    fprintf(stderr, \"Preposterous: KeyError: %s\\n\", k);");
        emitRaw(out, "    exit(1);");
        emitRaw(out, "}");
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "static char* ac_iota(long long n) {");
        emitRaw(out, "    if (n < 0) n = 0;");
        emitRaw(out, "    char* b = (char*)malloc((size_t)n * 21 + 1);");
        emitRaw(out, "    if (!b) { b = (char*)malloc(1); if (b) b[0] = 0; return b; }");
        emitRaw(out, "    long p = 0;");
        emitRaw(out, "    for (long long i = 0; i < n; i++) p += sprintf(b + p, \"%lld\", i);");
        emitRaw(out, "    b[p] = 0;");
        emitRaw(out, "    return b;");
        emitRaw(out, "}");
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually uses it (`ac_ipow`/`ac_rand`/`ac_choice` are `static`, so an unused
        // one is usually dead-code-eliminated at compile time by the C compiler itself — but the
        // generated SOURCE is still bloated with it either way, hurting readability even when
        // it costs nothing at runtime).
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "static long long ac_ipow(long long b, long long e) {");
            emitRaw(out, "    long long r = 1;");
            emitRaw(out, "    while (e-- > 0) r *= b;");
            emitRaw(out, "    return r;");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            emitRaw(out, "#include <stdlib.h>\n#include <time.h>");
            emitRaw(out, "static int _ac_seeded = 0;");
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "static long long ac_rand(long long n) {");
            emitRaw(out, "    if (!_ac_seeded) { srand((unsigned)time(0)); _ac_seeded = 1; }");
            emitRaw(out, "    return n > 0 ? (long long)(rand() % n) : 0;");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "static long long ac_choice(const std::vector<long long>& xs) {");
            emitRaw(out, "    return xs[(size_t)ac_rand((long long)xs.size())];");
            emitRaw(out, "}");
            emitRaw(out, "#endif");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "static char* ac_stream(long long a, long long b, long long s) {");
        emitRaw(out, "    if (s == 0) s = 1;");
        emitRaw(out, "    long long cnt = 0;");
        emitRaw(out, "    if (s > 0) { if (b > a) cnt = (b - a + s - 1) / s; }");
        emitRaw(out, "    else { long long ns = -s; if (a > b) cnt = (a - b + ns - 1) / ns; }");
        emitRaw(out, "    char* o = (char*)malloc((size_t)cnt * 21 + 22);");
        emitRaw(out, "    if (!o) { o = (char*)malloc(1); if (o) o[0] = 0; return o; }");
        emitRaw(out, "    long p = 0;");
        emitRaw(out, "    for (long long i = a; (s > 0) ? (i < b) : (i > b); i += s)");
        emitRaw(out, "        p += sprintf(o + p, \"%lld\", i);");
        emitRaw(out, "    o[p] = 0;");
        emitRaw(out, "    return o;");
        emitRaw(out, "}");
        // eval(): self-contained arithmetic evaluator (+ - * / parens, unary +/-). Named distinctly
        // (NOT ac_eval) so it never clashes with the math ilib's extern `ac_eval` (math_c.h) when a
        // program `use ilib math` — that collision was "static declaration follows non-static".
        if (hasEvalOp_) {
            emitRaw(out, "static const char* _ac_ep;");
            emitRaw(out, "static double _ac_eexpr(void);");
            emitRaw(out, "static void _ac_ews(void) {");
            emitRaw(out, "    while (*_ac_ep == ' ' || *_ac_ep == '\\t') _ac_ep++;");
            emitRaw(out, "}");
            emitRaw(out, "static double _ac_efac(void) {");
            emitRaw(out, "    _ac_ews();");
            emitRaw(out, "    if (*_ac_ep == '(') {");
            emitRaw(out, "        _ac_ep++;");
            emitRaw(out, "        double v = _ac_eexpr();");
            emitRaw(out, "        _ac_ews();");
            emitRaw(out, "        if (*_ac_ep == ')') _ac_ep++;");
            emitRaw(out, "        return v;");
            emitRaw(out, "    }");
            emitRaw(out, "    if (*_ac_ep == '-') { _ac_ep++; return -_ac_efac(); }");
            emitRaw(out, "    if (*_ac_ep == '+') { _ac_ep++; return _ac_efac(); }");
            emitRaw(out, "    char* e;");
            emitRaw(out, "    double v = strtod(_ac_ep, &e);");
            emitRaw(out, "    if (e == _ac_ep) return 0.0;");
            emitRaw(out, "    _ac_ep = e;");
            emitRaw(out, "    return v;");
            emitRaw(out, "}");
            emitRaw(out, "static double _ac_eterm(void) {");
            emitRaw(out, "    double v = _ac_efac();");
            emitRaw(out, "    for (;;) {");
            emitRaw(out, "        _ac_ews();");
            emitRaw(out, "        char c = *_ac_ep;");
            emitRaw(out, "        if (c == '*') { _ac_ep++; v *= _ac_efac(); }");
            emitRaw(out, "        else if (c == '/') { _ac_ep++; double d = _ac_efac(); v = d != 0.0 ? v / d : 0.0; }");
            emitRaw(out, "        else break;");
            emitRaw(out, "    }");
            emitRaw(out, "    return v;");
            emitRaw(out, "}");
            emitRaw(out, "static double _ac_eexpr(void) {");
            emitRaw(out, "    double v = _ac_eterm();");
            emitRaw(out, "    for (;;) {");
            emitRaw(out, "        _ac_ews();");
            emitRaw(out, "        char c = *_ac_ep;");
            emitRaw(out, "        if (c == '+') { _ac_ep++; v += _ac_eterm(); }");
            emitRaw(out, "        else if (c == '-') { _ac_ep++; v -= _ac_eterm(); }");
            emitRaw(out, "        else break;");
            emitRaw(out, "    }");
            emitRaw(out, "    return v;");
            emitRaw(out, "}");
            emitRaw(out, "static double _ac_builtin_eval(const char* s) {");
            emitRaw(out, "    if (!s) return 0.0;");
            emitRaw(out, "    _ac_ep = s;");
            emitRaw(out, "    return _ac_eexpr();");
            emitRaw(out, "}");
        }
        emitRaw(out, "");
    }

    std::string formatCallName(const std::string& n) const override {
        std::string s = n; for (char& c : s) if (c == '.') c = '_'; return s;
    }
    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        // Bundle field READ (self.field as an operand): every OTHER var name gets dots
        // flattened to underscores here (preserveDots=false, an existing, unrelated
        // convention for namespaced ilib names) — that would turn a real field access into a
        // disconnected plain local `self_hp`, silently losing the field entirely. Must check
        // and translate to `self->field` BEFORE commonRef's flattening runs, not after.
        if (r.kind == IRRef::Kind::VAR) {
            std::string raw = r.toStringWithSymbols(sym);
            if (raw.rfind("self.", 0) == 0) return "self->" + raw.substr(5);
            // A bundle INSTANCE var read from outside its class (e.g. `c.hp` after `c =
            // Critter()`) is the SAME "one dotted symbol name" mechanism as self.field — must
            // also be excluded from the flatten-dots-to-underscore convention, or `c.hp`
            // silently becomes the disconnected bare identifier `c_hp` (never declared
            // anywhere — a real struct value's `.` member access, not an ilib namespace ref).
            auto dot = raw.find('.');
            if (dot != std::string::npos && classInstanceVars_.count(raw.substr(0, dot)))
                return raw.substr(0, dot) + "." + raw.substr(dot + 1);
        }
        return commonRef(r, sym, "1", "0", "NULL", "NULL", false);
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...) inside a method: a real struct member reached
        // through the `ClassName* self` pointer every method (including init) already receives
        // — never redeclared, and `->` not flattened to `_` (see formatRef's note above; `var`
        // here may arrive raw as "self.field" OR already as "self->field" from formatRef).
        if (var.rfind("self.", 0) == 0) return "self->" + var.substr(5) + " = " + val + ";";
        if (var.rfind("self->", 0) == 0) return var + " = " + val + ";";
        // Re-typing coercion (#retype): a variable that's EVER assigned a string is a string
        // everywhere (inference promoted it). Non-string values assigned to it get to_string'd,
        // so `x = 5; x = $hi$` becomes `ac_str x = ac_to_str(5); x = "hi";` instead of punning a
        // pointer through a 64-bit int. (String->int would need stoi and can't always succeed,
        // so string is the unifying type.)
        if (isStringVar(var)) {
            std::string rhs = (looksString(val) || strVars.count(val) || isStringVar(val))
                              ? val : "ac_to_str(" + val + ")";
            if (declared.insert(var).second) return "ac_str " + var + " = " + rhs + ";";
            return var + " = " + rhs + ";";
        }
        if (strVars.count(val) || isStringVar(val)) {
            strVars.insert(var);
            if (declared.insert(var).second) return "ac_str " + var + " = " + val + ";";
            return var + " = " + val + ";";
        }
        if (dictVars_.count(val)) {
            dictVars_.insert(var);
            bool strVal = dictStrVals_.count(val) > 0;
            if (strVal) dictStrVals_.insert(var);
            const char* ty = strVal ? "ac_sdict* " : "ac_dict* ";
            if (declared.insert(var).second) return ty + var + " = " + val + ";";
            return var + " = " + val + ";";
        }
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) return "ac_str " + var + " = " + val + ";";
            if (isNarrowInt(ct)) return std::string(acIntTy(ct)) + " " + var + " = " + val + ";";
            if (ct == IRType::INT || ct == IRType::BOOL || ct == IRType::ATOMIC) return "ac_int " + var + " = (ac_int)(" + val + ");";
            if (looksString(val)) return "ac_str " + var + " = " + val + ";";
            if (isFloatVal(val))  { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            return "ac_int " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            emit(out, indent, "pthread_mutex_lock(&_ac_atomic_lock);");
            emit(out, indent, decl(var, val));
            emit(out, indent, "pthread_mutex_unlock(&_ac_atomic_lock);");
        } else {
            emit(out, indent, decl(var, val));
        }
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
        // Floor-modulo (result has the sign of the divisor) to match Python's `%` oracle;
        // C's `%` truncates, so -7 mod 3 would give -1 instead of 2.
        std::string expr = "(((long long)(" + lhs + ") % (long long)(" + rhs + ")) + (long long)(" + rhs + ")) % (long long)(" + rhs + ")";
        emit(out, indent, decl(res, expr));
    }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as decl(): real struct field via `self->`, no redeclaration.
        if (var.rfind("self.", 0) == 0 || var.rfind("self->", 0) == 0) { emit(out, indent, decl(var, val)); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            bool isNew = declared.insert(var).second;
            emit(out, indent, "pthread_mutex_lock(&_ac_atomic_lock);");
            emit(out, indent, (isNew ? "ac_int " : "") + var + " = (ac_int)(" + val + ");");
            emit(out, indent, "pthread_mutex_unlock(&_ac_atomic_lock);");
            return;
        }
        // Re-typing coercion (#retype): a variable ever assigned a string is a string everywhere
        // (inference promoted it). A non-string value assigned to it gets ac_to_str'd — so
        // `x = 5; x = $hi$` is `ac_str x = ac_to_str(5); x = "hi";`, not a pointer punned through
        // a 64-bit int. String is the unifying type (int->str always works; str->int can't).
        if (isStringVar(var)) {
            bool valIsStr = looksString(val) || strVars.count(val) || isStringVar(val)
                            || t == IRType::STRING;
            std::string rhs = valIsStr ? val : "ac_to_str(" + val + ")";
            if (declared.insert(var).second) emit(out, indent, "ac_str " + var + " = " + rhs + ";");
            else                             emit(out, indent, var + " = " + rhs + ";");
            return;
        }
        // #retype numeric-unified: a re-typed var whose last assignment is numeric, receiving a
        // stringified-number literal → ac_unstring (compile-time strip) + type from the literal.
        if (strVars.count(val) || isStringVar(val)) {
            strVars.insert(var);
            if (declared.insert(var).second) emit(out, indent, "ac_str " + var + " = " + val + ";");
            else                             emit(out, indent, var + " = " + val + ";");
            return;
        }
        if (looksString(val)) {
            std::string bare = acUnstring(val);
            std::string ty = (bare.find('.') != std::string::npos) ? "double " : "ac_int ";
            if (declared.insert(var).second) emit(out, indent, ty + var + " = " + bare + ";");
            else                             emit(out, indent, var + " = " + bare + ";");
            return;
        }
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : t);
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "double " + var + " = (double)(" + val + ");"); }
            else if (declType == IRType::STRING) emit(out, indent, "ac_str " + var + " = " + val + ";");
            else if (isNarrowInt(declType))      emit(out, indent, std::string(acIntTy(declType)) + " " + var + " = " + val + ";");
            else if (declType == IRType::BOOL || declType == IRType::INT)
                                                  emit(out, indent, "ac_int " + var + " = (ac_int)(" + val + ");");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            emit(out, indent, var + " = " + val + ";");
        }
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // String concat (#6): `a + b` where either side is a string → ac_concat (malloc + copy).
        auto looksStr = [&](const std::string& v) {
            return (!v.empty() && v.front() == '"') || strVars.count(v) || isStringVar(v);
        };
        if (op == "+" && (isStringVar(res) || looksStr(lhs) || looksStr(rhs))) {
            strVars.insert(res);
            std::string expr = "ac_concat(" + lhs + ", " + rhs + ")";
            bool isNew = declared.insert(res).second;
            emit(out, indent, (isNew ? "ac_str " : "") + res + " = " + expr + ";");
            return;
        }
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
        // String equality is CONTENT (strcmp), not pointer compare (#6: `c is $l$` was `c=="l"`).
        auto looksStr = [&](const std::string& v) {
            return (!v.empty() && v.front() == '"') || strVars.count(v) || isStringVar(v);
        };
        if ((op == "==" || op == "!=") && (looksStr(lhs) || looksStr(rhs))) {
            std::string cmp = "strcmp(" + lhs + ", " + rhs + ")";
            emit(out, indent, decl(res, "(ac_int)(" + cmp + (op == "==" ? " == 0)" : " != 0)")));
            return;
        }
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
        if (isMLFloatReturningFunc(fn)) return true;
        if (fn.rfind("math.", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;   // stat_avg, stat_median, ...
        if (fn.rfind("ac_",   0) == 0) return true;   // ac_sin, ac_sqrt, ...
        return false;
    }
    static std::string trimCArg(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }
    static std::vector<std::string> splitArgs(const std::string& args) {
        std::vector<std::string> out;
        std::string cur;
        int depth = 0;
        bool inStr = false, esc = false;
        for (char ch : args) {
            if (inStr) {
                cur += ch;
                if (esc) esc = false;
                else if (ch == '\\') esc = true;
                else if (ch == '"') inStr = false;
                continue;
            }
            if (ch == '"') { inStr = true; cur += ch; continue; }
            if (ch == '(' || ch == '[' || ch == '{') depth++;
            else if (ch == ')' || ch == ']' || ch == '}') depth--;
            if (ch == ',' && depth == 0) {
                out.push_back(trimCArg(cur));
                cur.clear();
            } else {
                cur += ch;
            }
        }
        if (!cur.empty() || !args.empty()) out.push_back(trimCArg(cur));
        return out;
    }
    static std::string joinArgs(const std::vector<std::string>& args, size_t begin = 0) {
        std::string out;
        for (size_t i = begin; i < args.size(); ++i) {
            if (i > begin) out += ", ";
            out += args[i];
        }
        return out;
    }
    static bool hasLazyArg(const std::vector<std::string>& args) {
        for (const auto& a : args) if (a == "lazy" || a == "\"lazy\"") return true;
        return false;
    }
    static std::vector<std::string> withoutLazy(const std::vector<std::string>& args) {
        std::vector<std::string> out;
        for (const auto& a : args) if (a != "lazy" && a != "\"lazy\"") out.push_back(a);
        return out;
    }
    static bool isWidgetCtor(const std::string& func) {
        static const std::set<std::string> ctors = {
            "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
            "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch"
        };
        return ctors.count(func) > 0;
    }
    static std::string packFnFor(const std::string& kind) {
        if (kind == "Screen") return "";
        if (kind == "radbtn") return "ac_widgets_ckbtn_pack";
        return "ac_widgets_" + kind + "_pack";
    }
    static std::string getFnFor(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_get";
        return "ac_widgets_" + kind + "_get";
    }
    static std::string setFnFor(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_set";
        return "ac_widgets_" + kind + "_set";
    }
    bool cWidgetCtor(std::ostringstream &out, int &indent, const std::string &res,
                     const std::string &func, const std::string &args) {
        if (!isWidgetCtor(func) || res.empty()) return false;
        std::vector<std::string> raw = splitArgs(args);
        bool isLazy = hasLazyArg(raw);
        std::vector<std::string> a = withoutLazy(raw);
        std::string call;
        if (func == "Screen") {
            std::string title = a.size() > 0 ? a[0] : "\"AC App\"";
            std::string geom  = a.size() > 1 ? a[1] : "\"800x600\"";
            call = "ac_widgets_screen_new(" + title + ", " + geom + ")";
        } else if (func == "display") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string text   = a.size() > 1 ? a[1] : "\"\"";
            call = "ac_widgets_display_new(" + master + ", " + text + ")";
        } else if (func == "ask") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string width  = a.size() > 1 ? a[1] : "20";
            call = "ac_widgets_ask_new(" + master + ", (int)(" + width + "))";
        } else if (func == "btn") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string text   = a.size() > 1 ? a[1] : "\"Button\"";
            call = "ac_widgets_btn_new(" + master + ", " + text + ")";
        } else if (func == "ckbtn" || func == "radbtn") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string text   = a.size() > 1 ? a[1] : "\"\"";
            call = "ac_widgets_ckbtn_new(" + master + ", " + text + ")";
        } else if (func == "dropdown") {
            std::string master = a.size() > 0 ? a[0] : "0";
            call = "ac_widgets_dropdown_new(" + master + ")";
        } else if (func == "advance") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string len    = a.size() > 1 ? a[1] : "200";
            call = "ac_widgets_advance_new(" + master + ", (int)(" + len + "))";
        } else if (func == "slider") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string from   = a.size() > 1 ? a[1] : "0";
            std::string to     = a.size() > 2 ? a[2] : "100";
            std::string orient = a.size() > 3 ? a[3] : "\"horizontal\"";
            call = "ac_widgets_slider_new(" + master + ", (double)(" + from + "), (double)(" + to + "), " + orient + ")";
        } else if (func == "group") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string text   = a.size() > 1 ? a[1] : "\"\"";
            call = "ac_widgets_group_new(" + master + ", " + text + ")";
        } else if (func == "tabs") {
            std::string master = a.size() > 0 ? a[0] : "0";
            call = "ac_widgets_tabs_new(" + master + ")";
        } else if (func == "scroller") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string orient = a.size() > 1 ? a[1] : "\"vertical\"";
            call = "ac_widgets_scroller_new(" + master + ", " + orient + ")";
        } else if (func == "listbox") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string width  = a.size() > 1 ? a[1] : "20";
            std::string height = a.size() > 2 ? a[2] : "10";
            call = "ac_widgets_listbox_new(" + master + ", (int)(" + width + "), (int)(" + height + "))";
        } else if (func == "table") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string cols   = a.size() > 1 ? a[1] : "\"\"";
            std::string height = a.size() > 2 ? a[2] : "10";
            call = "ac_widgets_table_new(" + master + ", " + cols + ", (int)(" + height + "))";
        } else if (func == "sketch") {
            std::string master = a.size() > 0 ? a[0] : "0";
            std::string width  = a.size() > 1 ? a[1] : "300";
            std::string height = a.size() > 2 ? a[2] : "200";
            call = "ac_widgets_sketch_new(" + master + ", (int)(" + width + "), (int)(" + height + "))";
        }
        widgetVars_[res] = func;
        bool isNew = declared.insert(res).second;
        emit(out, indent, (isNew ? "ac_widget_t " : "") + res + " = " + call + ";");
        std::string packFn = packFnFor(func);
        if (!packFn.empty()) {
            if (isLazy) emit(out, indent, "ac_widgets_set_lazy(" + res + ");");
            else        emit(out, indent, packFn + "(" + res + ");");
        }
        if (func == "btn" && a.size() > 2) {
            std::string cb = a[2];
            auto ar = userFuncArity_.find(cb);
            std::string adapter = (ar != userFuncArity_.end() && ar->second > 0)
                                  ? "_ac_widget_call1" : "_ac_widget_call0";
            emit(out, indent, "ac_widgets_btn_on_click(" + res + ", " + adapter + ", (void*)" + cb + ");");
        }
        return true;
    }
    bool cWidgetMethod(std::ostringstream &out, int &indent, const std::string &res,
                       const std::string &func, const std::string &args) {
        auto dot = func.rfind('.');
        std::string recv, method;
        if (dot != std::string::npos && dot != 0) {
            recv = func.substr(0, dot);
            method = func.substr(dot + 1);
        } else {
            // formatCallName flattens dots to underscores on backends with dotCallSyntax()==
            // false (C/ASM), so `func` here is often e.g. "tabber_add_tab" — the LAST underscore
            // is NOT always the receiver/method boundary when the METHOD NAME itself contains an
            // underscore (add_tab, on_click). Try every underscore position, left to right, and
            // take the first one whose prefix is a KNOWN widget var — the split any ambiguous
            // name (like "canvas_line" where a var named "canvas_line" theoretically could also
            // exist) would still resolve wrong is the same ambiguity every other flattened-name
            // backend already lives with, not a new risk (verified: widgets_test2.ac's
            // `tabber.add_tab(...)` — the old rfind('_')-only split produced recv="tabber_add",
            // method="tab", neither matching anything, silently falling through to a hard
            // "undefined reference to `tabber_add_tab'" link error).
            bool found = false;
            for (size_t p = func.find('_'); p != std::string::npos && p != 0; p = func.find('_', p + 1)) {
                std::string cand = func.substr(0, p);
                if (widgetVars_.count(cand)) { recv = cand; method = func.substr(p + 1); found = true; break; }
            }
            if (!found) return false;
        }
        auto wit = widgetVars_.find(recv);
        if (wit == widgetVars_.end()) return false;
        const std::string& kind = wit->second;
        std::vector<std::string> a = splitArgs(args);
        if (method == "pack") {
            if (a.size() >= 2) emit(out, indent, "ac_widgets_pack_spaced(" + recv + ", (int)(" + a[0] + "), (int)(" + a[1] + "));");
            else              emit(out, indent, packFnFor(kind) + "(" + recv + ");");
            return true;
        }
        if (method == "mainloop" && kind == "Screen") { emit(out, indent, "ac_widgets_screen_mainloop(" + recv + ");"); return true; }
        if (method == "update" && kind == "Screen")   { emit(out, indent, "ac_widgets_screen_update(" + recv + ");"); return true; }
        if (method == "destroy" && kind == "Screen")  { emit(out, indent, "ac_widgets_screen_destroy(" + recv + ");"); return true; }
        if (method == "add") {
            std::string fn = (kind == "dropdown" ? "ac_widgets_dropdown_add" :
                              kind == "listbox"  ? "ac_widgets_listbox_add"  :
                              kind == "table"    ? "ac_widgets_table_add"    : "ac_widgets_add");
            emit(out, indent, fn + "(" + recv + (args.empty() ? "" : ", " + args) + ");");
            return true;
        }
        if (method == "set" || method == "config") {
            std::string val = a.empty() ? "\"\"" : a[0];
            emit(out, indent, setFnFor(kind) + "(" + recv + ", " + val + ");");
            return true;
        }
        if (method == "get") {
            std::string fn = getFnFor(kind);
            if (kind == "ask" || kind == "display" || kind == "dropdown") {
                if (!res.empty()) {
                    strVars.insert(res);
                    bool isNew = declared.insert(res).second;
                    emit(out, indent, (isNew ? "ac_str " : "") + res + " = " + fn + "(" + recv + ");");
                } else {
                    emit(out, indent, fn + "(" + recv + ");");
                }
            } else {
                if (!res.empty()) emit(out, indent, decl(res, fn + "(" + recv + ")"));
                else              emit(out, indent, fn + "(" + recv + ");");
            }
            return true;
        }
        if (method == "on_click" && kind == "btn" && !a.empty()) {
            std::string cb = a[0];
            auto ar = userFuncArity_.find(cb);
            std::string adapter = (ar != userFuncArity_.end() && ar->second > 0)
                                  ? "_ac_widget_call1" : "_ac_widget_call0";
            emit(out, indent, "ac_widgets_btn_on_click(" + recv + ", " + adapter + ", (void*)" + cb + ");");
            return true;
        }
        // tabs.add_tab / sketch's drawing methods — never had ANY handling here (verified:
        // widgets_test2.ac hard-linker-failed with "undefined reference to `tabber_add_tab'"
        // etc, even on C — the generic fallback flattened the dotted call to `recv_method` and
        // called that literally, same "undefined label" bug class as the ctor gap this whole
        // module fixes elsewhere).
        if (method == "add_tab" && kind == "tabs" && !a.empty()) {
            emit(out, indent, (res.empty() ? "" : (declared.insert(res).second ? "ac_widget_t " : "") + res + " = ")
                             + "ac_widgets_tabs_add_tab(" + recv + ", " + a[0] + ");");
            return true;
        }
        if (method == "clear" && kind == "sketch") {
            emit(out, indent, "ac_widgets_sketch_clear(" + recv + ");");
            return true;
        }
        if ((method == "line" || method == "rect") && kind == "sketch" && a.size() >= 7) {
            emit(out, indent, "ac_widgets_sketch_" + method + "(" + recv
                             + ", (double)(" + a[0] + "), (double)(" + a[1] + "), (double)(" + a[2] + "), (double)(" + a[3]
                             + "), (unsigned char)(" + a[4] + "), (unsigned char)(" + a[5] + "), (unsigned char)(" + a[6] + "));");
            return true;
        }
        if (method == "circle" && kind == "sketch" && a.size() >= 6) {
            emit(out, indent, "ac_widgets_sketch_circle(" + recv
                             + ", (double)(" + a[0] + "), (double)(" + a[1] + "), (double)(" + a[2]
                             + "), (unsigned char)(" + a[3] + "), (unsigned char)(" + a[4] + "), (unsigned char)(" + a[5] + "));");
            return true;
        }
        if (method == "text_at" && kind == "sketch" && a.size() >= 6) {
            emit(out, indent, "ac_widgets_sketch_text(" + recv
                             + ", (double)(" + a[0] + "), (double)(" + a[1] + "), " + a[2]
                             + ", (unsigned char)(" + a[3] + "), (unsigned char)(" + a[4] + "), (unsigned char)(" + a[5] + "));");
            return true;
        }
        return false;
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        if (cWidgetCtor(out, indent, res, func, args)) return;
        if (cWidgetMethod(out, indent, res, func, args)) return;
        if (func == "ac_length" && !res.empty()) {
            std::string pre = declared.insert(res).second ? "ac_int " : "";  // register! (a later
            // `res = res - 1` must NOT re-declare/shadow — that was UB + an infinite loop)
            // `length $hello$` (a STRING LITERAL, not a variable) needed `looksString(args)`
            // too — strVars/isStringVar only look ARGS up as a variable NAME, which a raw
            // literal like `"hello"` never matches, so this fell to the ac_arr_len (array)
            // branch and passed a `const char*` where `const ac_int*` was expected — a real
            // incompatible-pointer-type bug, not just a wrong answer (gcc caught it as a
            // warning, but it's UB: `a[-2]` reads two `ac_int`s, i.e. 16 bytes, before
            // wherever the string literal happens to live).
            if (strVars.count(args) || isStringVar(args) || looksString(args))  // char* → strlen, NOT sizeof(pointer)
                emit(out, indent, pre + res + " = (ac_int)strlen(" + args + ");");
            else  // lists are stretchy buffers — len rides at p[-2] (sizeof(ptr) was always 1)
                emit(out, indent, pre + res + " = ac_arr_len(" + args + ");");
            return; }
        if (func == "ac_choice" && !res.empty()) { std::string pre = declared.insert(res).second ? "ac_int " : ""; emit(out, indent, pre + res + " = (" + args + ")[ac_rand(ac_arr_len(" + args + "))];"); return; }
        {   // AC list.append → stretchy-buffer push (reassign: realloc may move the block).
            // Dots were already folded to underscores by formatCallName, so match both forms.
            for (const char* suf : {".append", "_append"}) {
                auto ap = func.rfind(suf);
                if (ap != std::string::npos && ap == func.size() - 7 && ap > 0) {
                    std::string recv = func.substr(0, ap);
                    if (listVars.count(recv) || listParams_.count(recv) || listGlobals_.count(recv)) {
                        emit(out, indent, recv + " = ac_arr_push(" + recv + ", " + args + ");");
                        return;
                    }
                }
            }
        }
        std::string call = func + "(" + args + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (isAcStrFunc(func) && declared.insert(res).second) {
            strVars.insert(res);
            emit(out, indent, "ac_str " + res + " = " + call + ";");
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            listVars.insert(res);
            emit(out, indent, "ac_int* " + res + " = " + call + ";");
        } else if (isUserStringReturningFunc(func) && declared.insert(res).second) {
            // #retstring: matches emitFunctionBegin's `ac_str` return type — without this the
            // call site defaulted to `ac_int res = ...`, printing the string's pointer VALUE
            // as a number instead of the text (examples/showcase.ac's `describe(13)`).
            strVars.insert(res);
            emit(out, indent, "ac_str " + res + " = " + call + ";");
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
    void emitIntDiv(std::ostringstream &out, int &indent, const std::string &res,
                    const std::string &lhs, const std::string &rhs) override
    {   // guarded: a literal /0 is UB (gcc emits ud2 → SIGILL); runtime 0 → clean error
        emit(out, indent, decl(res, "ac_idiv(" + lhs + ", " + rhs + ")"));
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (looksString(val) || strVars.count(val) || isStringVar(val))
            emit(out, indent, "printf(\"%s\\n\", " + val + ");");
        else if (listVars.count(val) || listParams_.count(val))
            emit(out, indent, "ac_arr_print(" + val + ");");  // [2, 3, 5] — matches PY
        else if (isFloatVal(val))
            emit(out, indent, "_ac_dblprint((double)(" + val + "));");
        else
            emit(out, indent, "printf(\"%lld\\n\", (long long)(" + val + "));");
    }
    // `sure $msg$` (browser confirm()) has no C UI to ask through — the base
    // BackendStrategy::emitConfirm default is a pure no-op that never assigns `res` at all,
    // fine when the result is discarded but a hard "'t_N' undeclared" compile error the moment
    // a caller captures it (`result = sure $x$`) — same gap PY already worked around (see its
    // own emitConfirm comment); mirrored here: print the prompt, default to false/0 (auto-decline).
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        emit(out, indent, "printf(\"%s\\n\", " + val + ");");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {   // flush first so buffered stdout survives the core dump (parity w/ PY's /kill)
        emit(out, indent, "fflush(stdout); abort();");
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
            emit(out, indent, "double " + res + " = _ac_builtin_eval(" + expr + ");");
        } else {
            emit(out, indent, res + " = _ac_builtin_eval(" + expr + ");");
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

    // Real try/catch via setjmp/longjmp (see the `_ac_try_stack`/`ac_idiv` note in emitHeader)
    // — this used to be `if (0) { <catch body> }`, i.e. genuinely unreachable dead code; a
    // runtime error inside `try` called exit() directly and NOTHING was ever caught. The
    // catch/after bodies are branches of ONE if/else driven by setjmp's return value, wrapped
    // in an outer `{ }` that owns the depth-counter bookkeeping both the normal-completion path
    // (falls out the bottom of the `if`) and the longjmp'd path (re-enters at the `else`) must
    // each run exactly once — see the two `_ac_try_depth--;` sites below.
    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "{");
        indent++;
        emit(out, indent, "_ac_try_depth++;");
        emit(out, indent, "if (setjmp(_ac_try_stack[_ac_try_depth-1]) == 0) {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        emit(out, indent, "_ac_try_depth--;");
        indent--;
        emit(out, indent, "} else {");
        indent++;
        emit(out, indent, "_ac_try_depth--;");
        emit(out, indent, "long long " + exVar + " = 0; (void)" + exVar + ";");
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");   // closes the catch- or after-body block
        indent--;
        emit(out, indent, "}");   // closes the outer wrapper opened by emitTryBegin
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        // `depth` names scopes by NESTING level, not by occurrence — two SIBLING (sequential,
        // not nested) loops both saving a same-named var (e.g. two separate `FOR i in ...`)
        // both land on "depth 0" and collide: C has no automatic per-block scoping here (the
        // save lives in the enclosing FUNCTION scope, not scoped to just one loop), so the
        // second loop's `__typeof__(a) _ac_s0_a = a;` was a hard "redefinition of _ac_s0_a"
        // compile error. Reuse the SAME slot on a second occurrence (correctness holds: strict
        // enter→loop-body→exit sequencing means nothing else observes it between uses) — just
        // stop re-declaring it, via the same `declared` bookkeeping this class uses everywhere.
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars) {
            bool isNew = declared.insert(pfx + v).second;
            emit(out, indent, (isNew ? "__typeof__(" + v + ") " : "") + pfx + v + " = " + v + ";");
        }
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
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (ac_int " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (isStringVar(collection)) {
            // AC iterates a string as 1-char strings; C: walk chars, make a 2-byte buffer.
            emit(out, indent, "for (size_t _fi = 0, _fn = strlen(" + collection + "); _fi < _fn; _fi++) {");
            emit(out, indent + 1, "char " + iterVar + "[2] = { " + collection + "[_fi], 0 };");
            strVars.insert(iterVar);
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        emit(out, indent, "for (ac_int _fi = 0; _fi < ac_arr_len(" + collection + "); _fi++) {");
        emit(out, indent + 1, "ac_int " + iterVar + " = " + collection + "[_fi];");
        declared.insert(iterVar); forVarStack_.push_back(iterVar);
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        if (!forVarStack_.empty()) { declared.erase(forVarStack_.back()); forVarStack_.pop_back(); }
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
        else if (type == "dict") {
            dictVars_.insert(var); declared.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (numeric) {
                emit(out, indent, "ac_dict* " + var + " = ac_dict_new();");
                for (auto& [k, v] : parseDictPairs(content))
                    emit(out, indent, "ac_dict_set(" + var + ", " + fmtDictKey(k) + ", " + fmtDictVal(v) + ");");
            } else {
                dictStrVals_.insert(var);
                emit(out, indent, "ac_sdict* " + var + " = ac_sdict_new();");
                for (auto& [k, v] : parseDictPairs(content))
                    emit(out, indent, "ac_sdict_set(" + var + ", " + fmtDictKey(k) + ", " + fmtDictValStr(v) + ");");
            }
        }
        else {
            // A list whose elements are themselves dict vars (e.g. datac-imported rows) —
            // needs an array of dict pointers, not the generic ac_int* stretchy buffer.
            std::vector<std::string> parts = splitCommaTrimmed(content);
            bool allDicts = !parts.empty();
            for (auto& p : parts) if (!dictVars_.count(p)) { allDicts = false; break; }
            if (allDicts) {
                bool strVal = dictStrVals_.count(parts[0]) > 0;
                listOfDictVars_.insert(var);
                if (strVal) listOfDictStrVals_.insert(var);
                declared.insert(var);
                std::string elemT = strVal ? "ac_sdict*" : "ac_dict*";
                std::string joined;
                for (size_t i = 0; i < parts.size(); i++) { if (i) joined += ", "; joined += parts[i]; }
                emit(out, indent, elemT + " " + var + "[" + std::to_string(parts.size()) + "] = {" + joined + "};");
                return;
            }
            listVars.insert(var); declared.insert(var);
            // Count elements to determine array size
            int n = content.empty() ? 0 : 1;
            for (char c : content) if (c == ',') n++;
            // Length-tracked heap array (stretchy buffer) — len rides at p[-2], safe to return.
            // A promoted list GLOBAL is already declared ac_int* at file scope — assign it (#C4).
            if (listGlobals_.count(var))
                emit(out, indent, var + " = ac_arr_new(" + std::to_string(n) + ");");
            else
                emit(out, indent, "ac_int* " + var + " = ac_arr_new(" + std::to_string(n) + ");");
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
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            declared.insert(result);
            emit(out, indent, std::string(strVal ? "ac_sdict* " : "ac_dict* ") + result + " = " + arr + "[" + idx + "];");
            return;
        }
        if (dictVars_.count(arr)) {
            declared.insert(result);
            if (dictStrVals_.count(arr)) {
                strVars.insert(result);
                emit(out, indent, "ac_str " + result + " = ac_sdict_get(" + arr + ", " + idx + ");");
            } else {
                emit(out, indent, "ac_int " + result + " = ac_dict_get(" + arr + ", " + idx + ");");
            }
            return;
        }
        if (isStringVar(arr) || strVars.count(arr)) {
            // s[i] on a string → 1-char string (AC semantics). Stack 2-byte buffer.
            strVars.insert(result); declared.insert(result);
            emit(out, indent, "char " + result + "[2] = { " + arr + "[" + idx + "], 0 };");
            return;
        }
        emit(out, indent, decl(result, arr + "[" + idx + "]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        if (dictVars_.count(arr)) {
            if (dictStrVals_.count(arr))
                emit(out, indent, "ac_sdict_set(" + arr + ", " + idx + ", " + val + ");");
            else
                emit(out, indent, "ac_dict_set(" + arr + ", " + idx + ", " + val + ");");
            return;
        }
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

    // Build a typed param list, using function-pointer type for func-typed params. Shared by
    // emitFunctionPrototype (forward declarations) and emitFunctionBegin (real definitions) —
    // previously only inlined in the latter, so C never got a forward-declaration mechanism at
    // all (unlike C++/Java, which both override emitFunctionPrototype): a mutually-recursive
    // pair (`is_even` calling `is_odd` defined LATER in the file) hit a real, pre-existing gcc
    // error — "conflicting types for 'is_odd'; have 'ac_int(ac_int)'" against an implicit
    // `int()` declaration synthesized from the first (undeclared) call site (verified:
    // examples/mutual_recursion.ac).
    std::string typedParamListC(const std::string& params) const {
        std::string tparams;
        if (!params.empty()) {
            std::istringstream ss(params);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                if (isStringVar(pname)) {
                    tparams += "const char* " + pname;   // #6: inferred string param (char-iterable)
                } else if (listParams_.count(pname)) {
                    tparams += "ac_int* " + pname;   // array parameter
                } else if (fit != funcTypedParams_.end()) {
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
        return tparams;
    }
    void emitFunctionPrototype(std::ostringstream &out, const std::string &name,
                               const std::string &params, int retKind) override
    {
        std::string ret = retKind == 4 ? "void " : retKind == 2 ? "ac_int* " : retKind == 3 ? "ac_str " : retKind == 1 ? "double " : "ac_int ";
        emit(out, 0, ret + name + "(" + typedParamListC(params) + ");");
    }
    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are file-scope globals
        std::string cName = name;
        std::string cParams = params;
        if (!classOwner.empty()) {
            cName = (name == "init") ? classOwner + "_init" : classOwner + "_" + name;
            // The generic "self" param gets a REAL `ClassName* self` prepended a few lines
            // down — without stripping the raw "self"/"self, " here first, it was ALSO
            // processed by the generic loop below as an ordinary `ac_int self` parameter,
            // producing a duplicate/redeclared `self` (a real compile error: every method
            // took a `ClassName* self` AND a plain `ac_int self`).
            if (cParams.rfind("self, ", 0) == 0) cParams = cParams.substr(6);
            else if (cParams == "self") cParams = "";
        }

        // Build typed param list, using function-pointer type for func-typed params
        std::string tparams = typedParamListC(cParams);
        if (!classOwner.empty()) {
            std::string selfParam = classOwner + "* self";
            tparams = tparams.empty() ? selfParam : selfParam + ", " + tparams;
        }
        if (tparams.empty()) tparams = "void";
        declareParams(cParams, declared);
        // #retvoid: a genuinely void function (e.g. a `configure event-listener` key-callback
        // body, or `init` — see emitConstructCall, which already discards init's return value
        // regardless of type) defaulted to `ac_int` — harmless for `init` (call site never
        // used it) but a real bug for event callbacks: `_ac_bind` takes `ac_evfn` = `void
        // (*)(void)`, and `ac_int (*)(void)` is an incompatible pointer type.
        std::string retT = returnIsVoid_ ? "void" : returnIsList_ ? "ac_int*" : baseReturnIsString_ ? "ac_str" : returnIsFloat_ ? "double" : "ac_int";
        returnIsFloat_ = false; returnIsList_ = false; baseReturnIsString_ = false; returnIsVoid_ = false;
        emit(out, indent, retT + " " + cName + "(" + tparams + ") {");
        indent++;
        // Hoist cross-block locals to the top (fixes #41). C has no `auto`, so concrete types only.
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "const char* " + v + " = 0;";
            else if (t == IRType::LIST)     line = "ac_int* " + v + " = 0;";
            else if (isNarrowInt(t))        line = std::string(acIntTy(t)) + " " + v + " = 0;";
            else                            line = "ac_int " + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
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
        if (isWidgetCtor(func) || func.find('.') != std::string::npos || func.find('_') != std::string::npos) {
            size_t sep = func.find('.');
            if (sep == std::string::npos) sep = func.find('_');
            if (isWidgetCtor(func) || (sep != std::string::npos && widgetVars_.count(func.substr(0, sep)))) {
                emitCall(out, indent, res, func, args);
                return;
            }
        }
        std::string call = func + "(" + args + ")";
        if (res.empty())
            emit(out, indent, call + ";");
        else
            emit(out, indent, decl(res, call));
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        // Field defaults can't be struct initialisers in C (suppressClassBody() stays true —
        // they're lowered into a real Name_init(Name* self) function instead, like every other
        // method); the struct itself DOES need its real fields now (previously a single dummy
        // `_tag` — field WRITES existed via self.field but nothing to write INTO).
        currentClass_ = name;
        emitRaw(out, "/* bundle " + name + " */");
        emit(out, indent, "typedef struct " + name + " {");
        indent++;
    }
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "ac_str" : t == IRType::FLOAT ? "double" : "ac_int";
        if (t == IRType::STRING) bundleStringFields_.insert(field);
        emit(out, indent, ty + " " + field + ";");
    }
    void emitFieldsEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "} " + currentClass_ + ";");
        emitRaw(out, "");
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        currentClass_ = "";
        (void)indent;
    }
    bool suppressClassBody() const override { return true; }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // C has no return-a-struct-by-value convention here (every OTHER method, including
        // init, already takes `ClassName* self` uniformly — see emitFunctionBegin) — so
        // `c = Critter()` becomes declare-then-init-by-pointer: `Critter c = {0}; Critter_init
        // (&c, args);`. init's own `ac_int` return value is discarded (nothing else uses it).
        classInstanceVars_.insert(res);   // see formatRef's note on `c.hp` vs dot-flattening
        bool isNewDecl = declared.insert(res).second;
        if (isNewDecl) emit(out, indent, className + " " + res + " = {0};");
        std::string call = className + "_init(&" + res + (args.empty() ? "" : ", " + args) + ");";
        emit(out, indent, call);
    }
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
        // Declare on first sight — a cast-only var (`to_int n = 42` / `short a = 5` then only read)
        // is not otherwise hoisted, so a bare assignment left it undeclared. Guarded by `declared`,
        // so hoisted vars (already in the set) get a plain assignment instead (no double-declare).
        bool isNew = declared.insert(var).second;
        // `to_int`/`to_dec`/`short`/`mini`/`atomic` on a STRING source (`to_int n = $42$`) must
        // PARSE the text, not reinterpret its pointer bit-pattern as a number — `(ac_int)("42")`
        // is the string literal's ADDRESS cast to an integer (a garbage huge number), not 42.
        // Every branch below except STRING's (already correct) had this bug — found via
        // `to_int n = $42$` / `to_int($9$)` in examples/keyword_catalog_core.ac both printing a
        // ~15-digit garbage number instead of 42/9.
        bool srcIsStringLiteral = looksString(src) || strVars.count(src) || isStringVar(src);
        std::string intSrc = srcIsStringLiteral ? "atoll(" + src + ")" : src;
        std::string fltSrc = srcIsStringLiteral ? "atof(" + src + ")" : src;
        if (floatVars.count(var) || t == IRType::FLOAT) {
            floatVars.insert(var);
            emit(out, indent, (isNew ? "double " : "") + var + " = (double)(" + fltSrc + ");");
        } else if (t == IRType::STRING) {
            // `to_string x = 99` / `to_string(9)`: `src` is a raw NUMBER, not a string, most of
            // the time — assigning it straight to an `ac_str` (const char*) either silently
            // reinterprets the integer bit pattern as a garbage pointer (a real, dangerous bug,
            // not just a wrong answer) or — as gcc caught here — just a compiler warning that
            // was actually a real bug (int-to-pointer). Every OTHER numeric-coercion branch in
            // this function already casts; this one just never did. Reuses the exact
            // "already-a-string?" heuristic decl()/emitTypedStoreVar use elsewhere in this class.
            std::string rhs = srcIsStringLiteral ? src : "ac_to_str(" + src + ")";
            emit(out, indent, (isNew ? "ac_str " : "") + var + " = " + rhs + ";");
            if (isNew) strVars.insert(var);
        } else if (isNarrowInt(t)) {
            // `short`/`mini`: the int32_t/int16_t declaration itself enforces the width — no cast.
            emit(out, indent, (isNew ? std::string(acIntTy(t)) + " " : "") + var + " = " + intSrc + ";");
        } else if (t == IRType::ATOMIC) {
            emit(out, indent, "pthread_mutex_lock(&_ac_atomic_lock);");
            emit(out, indent, (isNew ? "ac_int " : "") + var + " = (ac_int)(" + intSrc + ");");
            emit(out, indent, "pthread_mutex_unlock(&_ac_atomic_lock);");
        } else {
            emit(out, indent, (isNew ? "ac_int " : "") + var + " = (ac_int)(" + intSrc + ");");
        }
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are file-scope globals
        emit(out, indent, "int main()");
        emit(out, indent, "{");
        indent++;
        // Same hoist fix as emitFunctionBegin (see its comment) — the mainloop is block-scoped
        // C code too, and never got this treatment at all before: a var first assigned inside
        // one `if`/`else` arm of the mainloop and read from a sibling arm or after the `if`
        // failed with "'result' undeclared (first use in this function)" (verified:
        // examples/calculator.ac's REPL loop).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "const char* " + v + " = 0;";
            else if (t == IRType::LIST)     line = "ac_int* " + v + " = 0;";
            else if (isNarrowInt(t))        line = std::string(acIntTy(t)) + " " + v + " = 0;";
            else                            line = "ac_int " + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
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
    std::set<std::string> cppListVars_;
    // NA->free: mainloop vars a later-defined function reads/writes (e.g. a button callback
    // calling `name_inp.get()`) must share ONE variable with mainloop, not each get their own
    // local shadow. CStrategy already implements this (see its own promotedGlobals_); CppStrategy
    // never did — every promoted var was simply left undeclared (verified: `applicant_form.ac`'s
    // OnSubmit callback failed with "not declared in this scope" for every widget var it reads).
    std::set<std::string> promotedGlobals_;
    void setPromotedGlobals(const std::vector<std::string> &vars) override {
        for (auto& v : vars) promotedGlobals_.insert(v);
    }
    // Subset of promotedGlobals_ that are STRUCT-typed (ilib widget constructor results, e.g.
    // "name_inp" -> "ask") rather than scalar/list — these need a pointer-typed file-scope
    // global (no default constructor exists for `display`/`btn`/`ask`/... to plain-declare
    // one), constructed later via `new` at their first mainloop assignment, and accessed via
    // `->` instead of `.` everywhere else. See decl()'s and emitCall()'s own comments for the
    // two places this actually gets used.
    std::map<std::string, std::string> structGlobals_;
    void setStructGlobals(const std::map<std::string, std::string> &m) override {
        structGlobals_ = m;
    }
    std::set<std::string> floatVars;
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(v);   // #42: pre-inferred float locals
    }
    std::map<std::string, IRType> varCastTypes_;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string,std::set<std::string>> importSymbols_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::map<std::string, int> funcTypedParams_;
    bool returnIsFloat_ = false;
    bool returnIsList_ = false;
    bool returnIsVoid_ = false;
    bool curFuncReturnIsList_ = false;
    bool curFuncReturnIsString_ = false;   // persists through the body so emitReturn "" → std::string(), not 0
    bool curFuncReturnIsFloat_ = false;
    bool curFuncIsConstructor_ = false;    // classOwner.init → real C++ constructor; `return 0;` is ill-formed there
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;

    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    std::set<std::string> userStringFuncs_;
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
    bool isStringReturningFunc(const std::string& fn) const { return userStringFuncs_.count(fn) > 0; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasIdivOp_ = false, hasRandomOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool idiv, bool /*eval*/, bool /*etry*/) override {
        hasIpowOp_ = ip; hasIdivOp_ = idiv; hasRandomOp_ = r;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        emit(out, indent, "_ac_saved << ac_cat(" + val + ") << \"\\n\";");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "{ std::ofstream _f(" + filename + "); _f << _ac_saved.str(); }");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_ac_bind(" + key + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_ac_trigger(" + key + ");");
    }
    // #C5: ilib ABI takes const char* — pass std::string vars via .c_str() (also harmless
    // for user std::string params: implicit conversion back).
    std::string libArgRef(const std::string& name, bool isString) override {
        return isString ? name + ".c_str()" : name;
    }
    // #C5: .c_str() is applied by the dispatcher ONLY for namespaced ilib calls (see
    // valueArgRefFor). valueArgRef stays identity so ac_length/user-fn args keep std::string.

    // A value that must be wrapped in std::to_string to become a string is ONLY a bare numeric
    // literal (42, -3.5). A string literal "…", a concat expr with +, or another string var is
    // already a string — wrapping those in to_string was the #6 C++ break (to_string("Hello")).
    static bool needsToStringWrap(const std::string& v) {
        if (v.empty()) return false;
        size_t i = (v[0] == '-') ? 1 : 0;
        bool dot = false, digit = false;
        for (; i < v.size(); ++i) {
            if (v[i] == '.') { if (dot) return false; dot = true; }
            else if (v[i] >= '0' && v[i] <= '9') digit = true;
            else return false;
        }
        return digit;
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
        emitRaw(out, "#include <cstring>");
        emitRaw(out, "#include <thread>");
        emitRaw(out, "#include <chrono>");
        emitRaw(out, "#include <cstdint>");   // int32_t/int16_t for `short`/`mini`
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        if (anyAtomicVars()) {
            emitRaw(out, "#include <mutex>");
            emitRaw(out, "static std::mutex _ac_atomic_lock; // `atomic` vars: any op touching one is a global critical section");
        }
        if (needsEvents_) {
            emitRaw(out, "static std::map<std::string, std::function<void()>> _ac_events;");
            emitRaw(out, "static void _ac_bind(const std::string& key, std::function<void()> fn) {");
            emitRaw(out, "    _ac_events[key] = fn;");
            emitRaw(out, "}");
            emitRaw(out, "static void _ac_trigger(const std::string& key) {");
            emitRaw(out, "    auto it = _ac_events.find(key);");
            emitRaw(out, "    if (it != _ac_events.end()) it->second();");
            emitRaw(out, "}");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "static std::string ac_iota(long long n) {");
        emitRaw(out, "    std::string r;");
        emitRaw(out, "    for (long long i = 0; i < n; i++) r += std::to_string(i);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually uses it.
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "static long long ac_ipow(long long b, long long e) {");
            emitRaw(out, "    long long r = 1;");
            emitRaw(out, "    while (e-- > 0) r *= b;");
            emitRaw(out, "    return r;");
            emitRaw(out, "}");
        }
        if (hasIdivOp_) {
            // `//` truncating integer division
            emitRaw(out, "static long long ac_idiv(long long a, long long b) {");
            emitRaw(out, "    if (!b) throw std::runtime_error(\"3rd grade mathematics violated (ZeroDivisionError)\");");
            emitRaw(out, "    return a / b;");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            emitRaw(out, "#include <stdlib.h>\n#include <time.h>");
            emitRaw(out, "static int _ac_seeded = 0;");
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "static long long ac_rand(long long n) {");
            emitRaw(out, "    if (!_ac_seeded) { srand((unsigned)time(0)); _ac_seeded = 1; }");
            emitRaw(out, "    return n > 0 ? (long long)(rand() % n) : 0;");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "static long long ac_choice(const std::vector<long long>& xs) {");
            emitRaw(out, "    return xs[(size_t)ac_rand((long long)xs.size())];");
            emitRaw(out, "}");
            emitRaw(out, "#endif");
        }
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "static void ac_print_list(const std::vector<long long>& v) {");
        emitRaw(out, "    std::cout << \"[\";");
        emitRaw(out, "    for (size_t i = 0; i < v.size(); i++) {");
        emitRaw(out, "        if (i) std::cout << \", \";");
        emitRaw(out, "        std::cout << v[i];");
        emitRaw(out, "    }");
        emitRaw(out, "    std::cout << \"]\\n\";");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "static std::string ac_fstr(double x) {");
        emitRaw(out, "    long long xi = (long long)x;");
        emitRaw(out, "    if ((double)xi == x) return std::to_string(xi);");
        emitRaw(out, "    char b[32];");
        emitRaw(out, "    std::snprintf(b, sizeof(b), \"%.17g\", x);");
        emitRaw(out, "    return b;");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        // `%.16g` alone drops the trailing decimal point on a whole-number double (9.0 -> "9"),
        // silently making a float print indistinguishable from an int — matches the identical
        // fix in CStrategy; see its comment for the full rationale.
        emitRaw(out, "static void _ac_dblprint(double d) {");
        emitRaw(out, "    char buf[64];");
        emitRaw(out, "    std::snprintf(buf, sizeof(buf), \"%.16g\", d);");
        emitRaw(out, "    if (!strpbrk(buf, \".eEnN\")) {");
        emitRaw(out, "        size_t l = strlen(buf);");
        emitRaw(out, "        std::snprintf(buf + l, sizeof(buf) - l, \".0\");");
        emitRaw(out, "    }");
        emitRaw(out, "    printf(\"%s\\n\", buf);");
        emitRaw(out, "}");
        if (needsSave_) {
            emitRaw(out, "#include <sstream>");
            emitRaw(out, "#include <fstream>");
            // `save as`: accumulates everything printed so far
            emitRaw(out, "static std::ostringstream _ac_saved;");
        }
        // Overloaded stringify for `+` concat — overload resolution picks the right conversion from
        // each operand's ACTUAL C++ type, so codegen never has to guess (a std::string missed by the
        // string-var detector no longer becomes std::to_string(std::string) — a compile error).
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "#include <type_traits>");
        emitRaw(out, "static inline std::string ac_cat(const std::string& s) {");
        emitRaw(out, "    return s;");
        emitRaw(out, "}");
        emitRaw(out, "static inline std::string ac_cat(const char* s) {");
        emitRaw(out, "    return s ? std::string(s) : std::string();");
        emitRaw(out, "}");
        emitRaw(out, "template<class T> static inline std::string ac_cat(T v) {");
        emitRaw(out, "    if constexpr (std::is_floating_point<T>::value) return ac_fstr((double)v);");
        emitRaw(out, "    else return std::to_string(v);");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "static std::string ac_stream(long long a, long long b, long long s = 1) {");
        emitRaw(out, "    if (s == 0) s = 1;");
        emitRaw(out, "    std::string r;");
        emitRaw(out, "    for (long long i = a; (s > 0) ? (i < b) : (i > b); i += s) r += std::to_string(i);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
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
                        std::string _lnk;
                        if (ln == "machine-audio") _lnk = "acmachinaaudio";
                    else if (ln == "web-server") _lnk = "acserver";
                    else if (ln == "native-cpu") _lnk = "acncpu";
                        else if (ln == "os")       _lnk = "acoos";
                        else { _lnk = "ac"; for (char c : ln) if (c != '-') _lnk += c; }
                        emitRaw(out, "// Link: g++ out.cpp -I. -L\"" + absDir + "\" -l" + _lnk
                                     + " -Wl,-rpath,\"" + absDir + "\"");
                    }
                    // Selective import: emit using-declarations for requested symbols
                    std::string key = "ilib:" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty()) {
                        out << "// from " << ln << " use: ";
                        bool first = true;
                        for (auto& sym : it->second) { if (!first) out << ", "; out << sym; first = false; }
                        out << "\n";
                        // Names that collide with the C standard math library — <cmath>'s
                        // headers already inject a global, UNQUALIFIED `::sin`/`::cos`/etc (GCC
                        // puts these in both std:: and the global namespace), so `using
                        // ac_math::sin;` — introducing ANOTHER `double sin(double)` into the
                        // very same global scope — is a hard redeclaration conflict, not just
                        // ambiguity (verified: examples/keyword_catalog_modules.ac's `from ilib
                        // math use sin, cos` + `using math.sqrt`, g++: "conflicts with a
                        // previous declaration"). ac_math's own versions of these are thin
                        // wraps over the same std:: function, so simply not importing them
                        // un-qualified is safe — bare `sin(x)` in the generated body still
                        // resolves correctly via the already-visible global `::sin`.
                        static const std::set<std::string> cmathCollisions = {
                            "sin","cos","tan","asin","acos","atan","atan2",
                            "sqrt","cbrt","pow","exp","log","log10","log2",
                            "floor","ceil","round","abs","hypot",
                        };
                        for (auto& sym : it->second)
                            if (!cmathCollisions.count(sym))
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
            } else if (lt == "conglomer") {
                emitConglomer(out, ln);   // raw C header, dynamically linked (#conglomer)
            }
        }
        emitRaw(out, "");
    }
    // conglomer <foo.h>: include the header and DYNAMICALLY link its shared object.
    // `-l<stem>` links against lib<stem>.so at load time (dynamic); rpath finds it beside the header.
    void emitConglomer(std::ostringstream &out, const std::string &hdr)
    {
        // extern "C": a raw C header's symbols would otherwise be C++-name-mangled and fail to link.
        emitRaw(out, "extern \"C\" {");
        emitRaw(out, "#include \"" + hdr + "\"");
        emitRaw(out, "}");
        auto slash = hdr.rfind('/');
        std::string dir  = (slash == std::string::npos) ? "." : hdr.substr(0, slash);
        std::string base = (slash == std::string::npos) ? hdr : hdr.substr(slash + 1);
        std::string stem = base.substr(0, base.rfind('.'));      // foo.h → foo
        if (stem.rfind("lib", 0) == 0) stem = stem.substr(3);    // libfoo → foo
        emitRaw(out, "// CONGLOMER: " + hdr + " (dynamic)");
        // main.cpp's C++/LIB link-flag parser looks for "// Link: g++ <throwaway-token> <flags>"
        // (see the "// Link: g++ " prefix scan in main.cpp) — this used to emit "// Link: -I..."
        // with no "g++ " prefix at all, so the parser's rfind(prefix, 0)==0 check never matched
        // and these -I/-L/-l/-rpath flags were silently never passed to the real g++ invocation.
        emitRaw(out, "// Link: g++ out.cpp -I" + dir + " -L" + dir + " -l" + stem
                     + " -Wl,-rpath," + dir);
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym, "true", "false", "nullptr", "{}");
        // Bundle field READ (self.field as an operand, not just a STORE_VAR target): same
        // this->field translation as decl() applies to writes.
        if (s.rfind("self.", 0) == 0) return "this->" + s.substr(5);
        return s;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...) inside a method: the field is a real class
        // member, declared once by emitClassBegin's field-decl pass — never redeclared here.
        // Translate the AC-level dotted var name to the actual C++ member-access expression.
        // `var` may arrive either as the raw "self.field" (from callers that pass the bare
        // symbol name) or already as "this->field" (callers that go through formatRef() first,
        // which now does this same translation for READS) — handle both, never double-translate.
        if (var.rfind("self.", 0) == 0 || var.rfind("this->", 0) == 0) {
            std::string field = var.rfind("self.", 0) == 0 ? "this->" + var.substr(5) : var;
            std::string checkVar = var.rfind("this->", 0) == 0 ? ("self." + var.substr(6)) : var;
            if (!isStringVar(checkVar) && looksString(val)) return field + " = " + acUnstring(val) + ";";
            return field + " = " + val + ";";
        }
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, isStringVar(var) ? IRType::STRING : (floatVars.count(var) ? IRType::FLOAT : IRType::VOID));
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) {
                // string literals / concat exprs / string vars assign directly; only numbers convert
                bool quoted = !needsToStringWrap(val);
                return quoted ? "std::string " + var + " = " + val + ";"
                              : "std::string " + var + " = std::to_string(" + val + ");";
            }
            // #retype numeric-unified: a re-typed var (last assignment numeric) receiving a
            // stringified-number literal → ac_unstring (compile-time strip) + type from the literal.
            if (!isStringVar(var) && looksString(val)) {
                std::string bare = acUnstring(val);
                std::string ty = (bare.find('.') != std::string::npos) ? "double " : "long long ";
                if (ty == "double ") floatVars.insert(var);
                return ty + var + " = " + bare + ";";
            }
            if (ct == IRType::INT || ct == IRType::BOOL) return "long long " + var + " = (long long)(" + val + ");";
            if (isFloatVal(val)) { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            autoDeclCallee_[var] = val.substr(0, val.find('('));
            return "auto " + var + " = " + val + ";";
        }
        // Struct-typed promoted global (see setStructGlobals): pre-seeded into `declared` at
        // function/main begin, so its FIRST real assignment in the emitted source (mainloop
        // constructing the widget) lands here, not in the `declared.insert().second` branch
        // above. The var is a pointer at file scope (no default ctor to plain-declare against),
        // so this needs `new TypeName(args)`, not a bare `TypeName(args)` value-assignment.
        if (structGlobals_.count(var)) return var + " = new " + val + ";";
        // A var first declared via the generic `auto` fallback above (a struct-returning call —
        // ilib widget constructors like `display(...)`/`btn(...)`/`dropdown(...)` are the concrete
        // case, none of them AC bundle/scalar types the earlier branches already handle) holds a
        // C++ struct type inferred from ITS OWN first RHS. AC is dynamically typed, so reassigning
        // that same variable name to a DIFFERENT struct-returning call is completely ordinary AC
        // (verified pattern: every widgets example reuses one scratch var — `_h = display(...)`,
        // then later `_h = btn(...)`, discarding each result) — but a plain `_h = btn(...);` is a
        // hard C++ error (`operator=` doesn't exist between unrelated struct types). Detected via
        // the leading callee-name text differing from what was recorded at first declaration;
        // block-scoping (fresh shadowing `auto` inside `{ }`) sidesteps the type clash entirely —
        // safe here because none of these discard-pattern vars are ever read back afterward.
        {
            auto it = autoDeclCallee_.find(var);
            if (it != autoDeclCallee_.end()) {
                std::string callee = val.substr(0, val.find('('));
                if (callee != it->second) return "{ auto " + var + " = " + val + "; }";
            }
        }
        if (!isStringVar(var) && looksString(val)) return var + " = " + acUnstring(val) + ";";  // #retype reassign
        return var + " = " + val + ";";
    }
    std::map<std::string, std::string> autoDeclCallee_;

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
    void emitIntDiv(std::ostringstream &out, int &indent, const std::string &res,
                    const std::string &lhs, const std::string &rhs) override
    {   // guarded + THROWING — C++ try/catch actually catches division by zero now
        emit(out, indent, decl(res, "ac_idiv(" + lhs + ", " + rhs + ")"));
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "(((long long)(" + lhs + ") % (long long)(" + rhs + ")) + (long long)(" + rhs + ")) % (long long)(" + rhs + ")")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as decl(): real class member, no redeclaration, this->field not self.field.
        if (var.rfind("self.", 0) == 0 || var.rfind("this->", 0) == 0) { emit(out, indent, decl(var, val)); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            bool isNew = declared.insert(var).second;
            emit(out, indent, "_ac_atomic_lock.lock();");
            emit(out, indent, (isNew ? "long long " : "") + var + " = (long long)(" + val + ");");
            emit(out, indent, "_ac_atomic_lock.unlock();");
            return;
        }
        // #retype numeric-unified: a re-typed var (last assignment numeric) receiving a
        // stringified-number literal → ac_unstring (compile-time strip) + type from the literal.
        if (!isStringVar(var) && looksString(val)) {
            std::string bare = acUnstring(val);
            std::string ty = (bare.find('.') != std::string::npos) ? "double " : "long long ";
            if (ty == "double ") floatVars.insert(var);
            if (declared.insert(var).second) emit(out, indent, ty + var + " = " + bare + ";");
            else                             emit(out, indent, var + " = " + bare + ";");
            return;
        }
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, isStringVar(var) ? IRType::STRING : (floatVars.count(var) ? IRType::FLOAT : t));
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "double " + var + " = (double)(" + val + ");"); }
            else if (declType == IRType::STRING)  emit(out, indent, needsToStringWrap(val)
                                                       ? "std::string " + var + " = std::to_string(" + val + ");"
                                                       : "std::string " + var + " = " + val + ";");
            else if (declType == IRType::INT || declType == IRType::BOOL)
                                                  emit(out, indent, "long long " + var + " = (long long)(" + val + ");");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else {
            emit(out, indent, var + " = " + val + ";");
        }
    }
    void emitBinaryOp(std::ostringstream &out, int &indent, const std::string &res,
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // String concat: if either operand is a string (literal or inferred var), build a
        // std::string and coerce numeric operands via std::to_string / ac_fstr. Without this,
        // `"hi " + name` (name int) compiled as POINTER ARITHMETIC and `"a" + "b"` (two const
        // char*) didn't compile at all. Leading std::string() forces string-typed concat.
        if (op == "+") {
            bool lStr = looksString(lhs) || isStringVar(lhs);
            bool rStr = looksString(rhs) || isStringVar(rhs);
            if (lStr || rStr) {
                // ac_cat() overloads resolve each operand by its real type (string stays, number →
                // to_string/ac_fstr) — robust even when the string-var detector missed an operand.
                std::string expr = "ac_cat(" + lhs + ") + ac_cat(" + rhs + ")";
                bool isNewS = declared.insert(res).second;
                if (isNewS) { stringVars_.insert(res); emit(out, indent, "std::string " + res + " = " + expr + ";"); }
                else        emit(out, indent, res + " = " + expr + ";");
                return;
            }
        }
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
        // `length $literal$` (a raw string literal, e.g. `length $hello$`) is a `const char*` in
        // C++ — `.size()` only exists on an actual `std::string` object (what a string VARIABLE
        // is stored as here), so calling it directly on a literal is a hard compile error
        // ("request for member 'size' in ... which is of non-class type 'const char*'").
        // strlen() works on both forms, but a bare literal is the only case that was ever broken.
        if (func == "ac_length" && !res.empty()) {
            std::string lenExpr = looksString(args) ? ("strlen(" + args + ")") : ("(" + args + ").size()");
            emit(out, indent, decl(res, "(long long)" + lenExpr));
            return;
        }
        std::string cfunc = func;
        {   // AC list.append → std::vector push_back
            auto ap = cfunc.rfind(".append");
            if (ap != std::string::npos && ap == cfunc.size() - 7)
                cfunc = cfunc.substr(0, ap) + ".push_back";
        }
        // A method call whose receiver is a struct-typed promoted global (see setStructGlobals)
        // is a POINTER at file scope, not a value — `recv.method(...)` doesn't compile against a
        // pointer (verified: `name_inp.get()` inside applicant_form.ac's OnSubmit callback, once
        // `name_inp` became a real file-scope `ask*`, failed with "request for member 'get' in
        // ... which is of pointer type"). Rewrite the FIRST dot to `->` for exactly these.
        {
            auto dot = cfunc.find('.');
            if (dot != std::string::npos && structGlobals_.count(cfunc.substr(0, dot)))
                cfunc = cfunc.substr(0, dot) + "->" + cfunc.substr(dot + 1);
        }
        std::string call = cfunc + "(" + args + ")";
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if ((isAcStrFunc(func) || isStringReturningFunc(func)) && declared.insert(res).second) {
            emit(out, indent, "std::string " + res + " = " + call + ";");
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            cppListVars_.insert(res);
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
        // A constructor (classOwner.init → real C++ ctor) has no return type at all —
        // `return 0;`/`return std::string();` are ill-formed there. Bare `return;` (no value)
        // is the only legal early-return form in a constructor.
        if (curFuncIsConstructor_) { emit(out, indent, "return;"); return; }
        if (val.empty()) {
            // An empty return must match the declared type — `return 0;` for a std::string func
            // builds a string from a null const char* → UB/crash; use the right zero-value.
            const char* z = curFuncReturnIsList_   ? "return {};"
                          : curFuncReturnIsString_ ? "return std::string();"
                          : curFuncReturnIsFloat_  ? "return 0.0;"
                          :                          "return 0;";
            emit(out, indent, z);
        }
        else emit(out, indent, "return " + val + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // `null`/`nil` both map to `nullptr`/`{}` (formatRef's nullVal/nilVal) — neither is
        // directly streamable (`std::cout << nullptr` and `std::cout << {}` are both hard
        // compile errors: no `operator<<` overload takes `std::nullptr_t`, and a bare `{}` has
        // no type to even resolve to). `Term.display null`/`nil` have no real value to format
        // anyway; print the word, matching every other backend's null/nil text representation.
        if (val == "nullptr" || val == "{}")
            emit(out, indent, "std::cout << \"null\" << \"\\n\";");
        else if (cppListVars_.count(val))
            emit(out, indent, "ac_print_list(" + val + ");");
        else if (isFloatVal(val))
            emit(out, indent, "_ac_dblprint((double)(" + val + "));");
        else
            emit(out, indent, "std::cout << " + val + " << \"\\n\";");
    }
    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res`, crashing `result = sure $x$` with "'t_N' undeclared" on this backend too.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        emit(out, indent, "std::cout << " + val + " << \"\\n\";");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
    }
    void emitHalt(std::ostringstream &out, int &indent) override
    {
        emit(out, indent, "std::fflush(stdout); std::abort();");
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
        // `depth` names scopes by NESTING level, not occurrence — two SIBLING (sequential, not
        // nested) loops saving the same-named var both land on "depth 0" and collide: a second
        // `auto _ac_s0_n = n;` is a hard "redeclared as different type" error. Same fix as
        // CStrategy's emitScopeEnter (see its comment) — reuse the slot on a second occurrence.
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars) {
            bool isNew = declared.insert(pfx + v).second;
            emit(out, indent, (isNew ? "auto " : "") + pfx + v + " = " + v + ";");
        }
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
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (long long " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (isStringVar(collection)) {
            // AC iterates a string as 1-char STRINGS (Python-like), so `c == "1"` and Term.display c
            // work. The raw element is a char; wrap it in a std::string named as the iter var.
            std::string raw = iterVar + "__ch";
            emit(out, indent, "for (char " + raw + " : " + collection + ") {");
            indent++;
            emit(out, indent, "std::string " + iterVar + "(1, " + raw + ");");
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            return;
        }
        emit(out, indent, "for (long long " + iterVar + " : " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        if (!forVarStack_.empty()) { declared.erase(forVarStack_.back()); forVarStack_.pop_back(); }
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
            dictVars_.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (!numeric) dictStrVals_.insert(var);
            std::string vt = numeric ? "long long" : "std::string";
            std::string d = "std::map<std::string," + vt + "> " + var + " = {";
            bool first = true;
            for (auto& [k, v] : parseDictPairs(content)) {
                if (!first) d += ", ";
                d += "{" + fmtDictKey(k) + ", " + (numeric ? fmtDictVal(v) : fmtDictValStr(v)) + "}";
                first = false;
            }
            emit(out, indent, d + "};");
            declared.insert(var);
        } else {
            auto elems = splitCommaTrimmed(content);
            bool allDicts = !elems.empty();
            for (auto& e : elems) if (!dictVars_.count(e)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = dictStrVals_.count(elems[0]) > 0;
                if (strVal) listOfDictStrVals_.insert(var);
                std::string vt = strVal ? "std::string" : "long long";
                emit(out, indent, "std::vector<std::map<std::string," + vt + ">> " + var + " = {" + content + "};");
                declared.insert(var);
                return;
            }
            cppListVars_.insert(var);
            emit(out, indent, "std::vector<long long> " + var + " = {" + content + "};");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        // Reading an element OUT of a list-of-dicts (`p1 = pets[1]`) must declare `result` with
        // the real map type and propagate dict-ness onto it — decl()'s generic type inference
        // has no concept of maps at all and would otherwise default to `long long` (see
        // listOfDictVars_'s own comment for the full verified failure).
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            std::string vt = strVal ? "std::string" : "long long";
            emit(out, indent, "std::map<std::string," + vt + "> " + result + " = " + arr + "[" + idx + "];");
            declared.insert(result);
            return;
        }
        // Dict READ of a missing key: std::map::operator[] silently inserts a default and mutates
        // the map (wrong — AC/PY raise KeyError, C aborts). .at() throws std::out_of_range →
        // terminate, matching the crash-on-missing semantics of the other backends.
        if (dictVars_.count(arr))
            emit(out, indent, decl(result, arr + ".at(" + idx + ")"));
        else
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

    std::set<std::string> stringParams_;  // params typed as strings (GL object names)
    void setStringParams(const std::set<std::string>& sp) override { stringParams_ = sp; }
    // Shared param-typing for signatures AND forward declarations.
    std::string typedParamList(const std::string &cppParams)
    {
        std::string tparams;
        if (!cppParams.empty()) {
            std::istringstream ss(cppParams);
            std::string tok; bool first = true;
            while (std::getline(ss, tok, ',')) {
                size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                std::string pname = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                if (!first) tparams += ", ";
                auto fit = funcTypedParams_.find(pname);
                // isStringVar (whole-body string inference) MUST win when it also applies: the
                // BODY-level codegen unconditionally calls `.c_str()` on anything isStringVar
                // considers a string, regardless of how the PARAM got declared here — declaring
                // it `const char*` (stringParams_ alone, the narrower "passed to a known
                // string-arg ilib call" signal) left `.c_str()` called on a raw `const char*`, a
                // hard compile error (verified: examples/gd.ac's `hop(arg)` — "request for
                // member 'c_str' in 'arg', which is of non-class type 'const char*'").
                // stringParams_ still matters on its own as a FALLBACK, for a param isStringVar
                // never catches (see its own comment).
                if (isStringVar(pname) || stringParams_.count(pname)) {
                    tparams += "std::string " + pname;   // #6: inferred string param (iterable as chars)
                } else if (listParams_.count(pname)) {
                    tparams += "std::vector<long long>& " + pname;  // array parameter (by ref)
                    cppListVars_.insert(pname);
                } else if (fit != funcTypedParams_.end()) {
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) { if (k) argList += ", "; argList += "long long"; }
                    tparams += "std::function<long long(" + argList + ")> " + pname;
                } else {
                    tparams += "long long " + pname;
                }
                first = false;
            }
        }
        return tparams;
    }
    void emitFunctionPrototype(std::ostringstream &out, const std::string &name,
                               const std::string &params, int retKind) override
    {
        std::string ret = retKind == 4 ? "void "
                        : retKind == 2 ? "std::vector<long long> "
                        : retKind == 3 ? "std::string "
                        : retKind == 1 ? "double " : "long long ";
        int ind = 0;
        emit(out, ind, ret + name + "(" + typedParamList(params) + ");");
    }
    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are file-scope globals
        std::string cppParams = params;
        std::string cppName   = name;
        curFuncIsConstructor_ = false;
        if (!classOwner.empty()) {
            if (cppParams.rfind("self, ", 0) == 0) cppParams = cppParams.substr(6);
            else if (cppParams == "self") cppParams = "";
            cppName = (name == "init") ? classOwner : name;
            curFuncIsConstructor_ = (name == "init");
        }
        std::string tparams = typedParamList(cppParams);
        declareParams(cppParams, declared);
        curFuncReturnIsList_ = returnIsList_;
        curFuncReturnIsString_ = baseReturnIsString_;
        curFuncReturnIsFloat_ = returnIsFloat_;
        // #retvoid: CppStrategy never consulted returnIsVoid_ at all — a genuinely void
        // function (e.g. a `configure event-listener` key-callback body) always got the
        // `long long` default, an immediate prototype/definition mismatch once
        // emitFunctionPrototype (which DOES check it, via retKind==4/voidUserFuncs_) started
        // correctly declaring it void — "ambiguating new declaration" (verified: examples/
        // gd.ac's key-callback `hop`/`__keycb_space`). Same fix as C/Rust/Go already have.
        bool isVoidFn = returnIsVoid_ && !(!classOwner.empty() && name == "init");
        std::string retType = (!classOwner.empty() && name == "init") ? ""
            : isVoidFn ? "void "
            : returnIsList_ ? "std::vector<long long> "
            : baseReturnIsString_ ? "std::string "
            : returnIsFloat_ ? "double " : "long long ";
        returnIsFloat_ = false; returnIsList_ = false; baseReturnIsString_ = false; returnIsVoid_ = false;
        emit(out, indent, retType + cppName + "(" + tparams + ") {");
        indent++;
        // Hoist cross-block locals to the top so a later use in a sibling/outer block resolves (#41).
        // auto can't declare an uninitialized hoist, so use the concrete type (long long default).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double "  + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "std::string " + v + ";";
            else if (t == IRType::LIST)     line = "std::vector<long long> " + v + ";";
            else                            line = "long long "  + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
        funcTypedParams_.clear();
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
        curFuncReturnIsList_ = false;
        curFuncReturnIsString_ = false;
        curFuncReturnIsFloat_ = false;
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
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "std::string " : t == IRType::FLOAT ? "double " : "long long ";
        emit(out, indent, ty + field + ";");
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
        // Statement coercion `to_int/to_string/to_dec x = expr` DECLARES x with the target type on
        // first sight (#7 — was emitting a bare `x = …` assignment to an undeclared var), and parses
        // string↔number at the boundary (stoll/stod/to_string) instead of casting a char* pointer.
        bool isNew = declared.insert(var).second && !floatVars.count(var);
        bool srcIsStr = (!src.empty() && src.front() == '"') || isStringVar(src);
        if (t == IRType::STRING) {
            // ac_cat resolves by the src's real C++ type — a double the detector missed (e.g. a
            // math.mod result temp) formats as "1" via ac_fstr, not "1.000000" from to_string(double).
            std::string rhs = srcIsStr ? src : ("ac_cat(" + src + ")");
            emit(out, indent, (isNew ? "std::string " : "") + var + " = " + rhs + ";");
        } else if (t == IRType::FLOAT || floatVars.count(var)) {
            if (isNew) floatVars.insert(var);
            std::string rhs = srcIsStr ? "std::stod(" + src + ")" : "(double)(" + src + ")";
            emit(out, indent, (isNew ? "double " : "") + var + " = " + rhs + ";");
        } else if (irIntWidth(t)) {
            // `short`/`mini`: the fixed-width type (from the type include) enforces the width — no cast.
            const char* ty = acIntTypeCpp(irIntWidth(t));
            std::string rhs = srcIsStr ? ("std::stoll(" + src + ")") : src;
            emit(out, indent, (isNew ? std::string(ty) + " " : "") + var + " = " + rhs + ";");
        } else if (t == IRType::ATOMIC) {
            std::string rhs = srcIsStr ? "std::stoll(" + src + ")" : "(long long)(" + src + ")";
            emit(out, indent, "_ac_atomic_lock.lock();");
            emit(out, indent, (isNew ? "long long " : "") + var + " = " + rhs + ";");
            emit(out, indent, "_ac_atomic_lock.unlock();");
        } else {
            std::string rhs = srcIsStr ? "std::stoll(" + src + ")" : "(long long)(" + src + ")";
            emit(out, indent, (isNew ? "long long " : "") + var + " = " + rhs + ";");
        }
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are file-scope globals
        emit(out, indent, "int main() {");
        indent++;
        // Same hoist fix as emitFunctionBegin (see CStrategy's identical fix/comment) — the
        // mainloop is block-scoped C++ code too, and never got this treatment at all before: a
        // var first assigned inside one `if`/`else` arm and read from a sibling arm or after the
        // `if` failed with "'result' was not declared in this scope" (verified: examples/
        // calculator.ac's REPL loop).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "std::string " + v + ";";
            else if (t == IRType::LIST)     line = "std::vector<long long> " + v + ";";
            else                            line = "long long "  + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
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
public:
    // LIB wraps definitions in extern "C" — prototypes must match that linkage.
    void emitFunctionPrototype(std::ostringstream &out, const std::string &name,
                               const std::string &params, int retKind) override
    {
        std::string ret = retKind == 4 ? "void "
                        : retKind == 2 ? "std::vector<long long> "
                        : retKind == 3 ? "std::string "
                        : retKind == 1 ? "double " : "long long ";
        int ind = 0;
        emit(out, ind, "extern \"C\" " + ret + name + "(" + typedParamList(params) + ");");
    }

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
        emitRaw(out, "#include <cstring>");
        emitRaw(out, "typedef long long ac_int;");
        emitRaw(out, "typedef const char* ac_str;");
        // Same whole-number-float print fix as CppStrategy/CStrategy — see their comments.
        // LibStrategy has its own emitHeader (doesn't delegate to CppStrategy's despite the
        // comment above), so the helper needs its own copy here too.
        emitRaw(out, "static void _ac_dblprint(double d) {");
        emitRaw(out, "    char buf[64];");
        emitRaw(out, "    std::snprintf(buf, sizeof(buf), \"%.16g\", d);");
        emitRaw(out, "    if (!strpbrk(buf, \".eEnN\")) {");
        emitRaw(out, "        size_t l = strlen(buf);");
        emitRaw(out, "        std::snprintf(buf + l, sizeof(buf) - l, \".0\");");
        emitRaw(out, "    }");
        emitRaw(out, "    printf(\"%s\\n\", buf);");
        emitRaw(out, "}");
        // `ac_print_list`: LibStrategy inherits CppStrategy's emitPrint (calls `ac_print_list`
        // for a list value) but never defined it in its own separate header at all —
        // "ac_print_list was not declared in this scope" on any program printing a list on
        // `--target LIB` (verified: examples/sieve.ac). Same definition as CppStrategy's.
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "static void ac_print_list(const std::vector<long long>& v) {");
        emitRaw(out, "    std::cout << \"[\";");
        emitRaw(out, "    for (size_t i = 0; i < v.size(); i++) {");
        emitRaw(out, "        if (i) std::cout << \", \";");
        emitRaw(out, "        std::cout << v[i];");
        emitRaw(out, "    }");
        emitRaw(out, "    std::cout << \"]\\n\";");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        // `ac_cat`/`ac_fstr`: LibStrategy inherits CppStrategy's emitBinaryOp/emitTypeCast (which
        // call these for `+` string concat and to_string casts) but has its own separate
        // emitHeader that never defined them — "ac_cat was not declared in this scope" on any
        // program actually exercising string concat on `--target LIB`. Same definitions as
        // CppStrategy's emitHeader; see its comment for the overload-resolution rationale.
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "#include <cstdio>");
        emitRaw(out, "static std::string ac_fstr(double x) {");
        emitRaw(out, "    long long xi = (long long)x;");
        emitRaw(out, "    if ((double)xi == x) return std::to_string(xi);");
        emitRaw(out, "    char b[32];");
        emitRaw(out, "    std::snprintf(b, sizeof(b), \"%.17g\", x);");
        emitRaw(out, "    return b;");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        // ac_idiv: LibStrategy inherits CppStrategy's emitIntDiv (calls `ac_idiv(lhs, rhs)`,
        // native C++ `throw` — real exceptions, unlike C's setjmp/longjmp) but again never
        // defined it in its own separate header — "ac_idiv was not declared in this scope".
        // Gated on hasIdivOp_ — see PythonStrategy::setUsedBuiltinOps' comment.
        if (hasIdivOp_) {
            emitRaw(out, "static long long ac_idiv(long long a, long long b) {");
            emitRaw(out, "    if (!b) throw std::runtime_error(\"3rd grade mathematics violated (ZeroDivisionError)\");");
            emitRaw(out, "    return a / b;");
            emitRaw(out, "}");
        }
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "#include <type_traits>");
        emitRaw(out, "static inline std::string ac_cat(const std::string& s) {");
        emitRaw(out, "    return s;");
        emitRaw(out, "}");
        emitRaw(out, "static inline std::string ac_cat(const char* s) {");
        emitRaw(out, "    return s ? std::string(s) : std::string();");
        emitRaw(out, "}");
        emitRaw(out, "template<class T> static inline std::string ac_cat(T v) {");
        emitRaw(out, "    if constexpr (std::is_floating_point<T>::value) return ac_fstr((double)v);");
        emitRaw(out, "    else return std::to_string(v);");
        emitRaw(out, "}");
        emitRaw(out, "#endif");
        if (needsSave_) {
            // Same reason as ac_cat/ac_idiv above — LibStrategy's separate header needs its own
            // copy of whatever the inherited (CppStrategy) emitCapture/emitSaveFile reference.
            emitRaw(out, "#include <sstream>");
            emitRaw(out, "#include <fstream>");
            emitRaw(out, "static std::ostringstream _ac_saved;");
        }
        if (anyAtomicVars()) {
            emitRaw(out, "#include <mutex>");
            // `atomic` vars: any op touching one is a global critical section
            emitRaw(out, "static std::mutex _ac_atomic_lock;");
        }
        if (needsEvents_) {
            emitRaw(out, "static std::map<std::string, std::function<void()>> _ac_events;");
            emitRaw(out, "static void _ac_bind(const std::string& key, std::function<void()> fn) {");
            emitRaw(out, "    _ac_events[key] = fn;");
            emitRaw(out, "}");
            emitRaw(out, "static void _ac_trigger(const std::string& key) {");
            emitRaw(out, "    auto it = _ac_events.find(key);");
            emitRaw(out, "    if (it != _ac_events.end()) it->second();");
            emitRaw(out, "}");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "static std::string ac_iota(long long n) {");
        emitRaw(out, "    std::string r;");
        emitRaw(out, "    for (long long i = 0; i < n; i++) r += std::to_string(i);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "static long long ac_ipow(long long b, long long e) {");
            emitRaw(out, "    long long r = 1;");
            emitRaw(out, "    while (e-- > 0) r *= b;");
            emitRaw(out, "    return r;");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            emitRaw(out, "#include <stdlib.h>\n#include <time.h>");
            emitRaw(out, "static int _ac_seeded = 0;");
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "static long long ac_rand(long long n) {");
            emitRaw(out, "    if (!_ac_seeded) { srand((unsigned)time(0)); _ac_seeded = 1; }");
            emitRaw(out, "    return n > 0 ? (long long)(rand() % n) : 0;");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "static long long ac_choice(const std::vector<long long>& xs) {");
            emitRaw(out, "    return xs[(size_t)ac_rand((long long)xs.size())];");
            emitRaw(out, "}");
            emitRaw(out, "#endif");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "static std::string ac_stream(long long a, long long b, long long s = 1) {");
        emitRaw(out, "    if (s == 0) s = 1;");
        emitRaw(out, "    std::string r;");
        emitRaw(out, "    for (long long i = a; (s > 0) ? (i < b) : (i > b); i += s) r += std::to_string(i);");
        emitRaw(out, "    return r;");
        emitRaw(out, "}");
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
                        std::string _lnk;
                        if (ln == "machine-audio") _lnk = "acmachinaaudio";
                    else if (ln == "web-server") _lnk = "acserver";
                    else if (ln == "native-cpu") _lnk = "acncpu";
                        else if (ln == "os")       _lnk = "acoos";
                        else { _lnk = "ac"; for (char c : ln) if (c != '-') _lnk += c; }
                        emitRaw(out, "// Link: g++ out.cpp -I. -L\"" + absDir + "\" -l" + _lnk
                                     + " -Wl,-rpath,\"" + absDir + "\"");
                    }
                    std::string key = "ilib:" + ln;
                    auto it = importSymbols_.find(key);
                    if (it != importSymbols_.end() && !it->second.empty()) {
                        // Same cmath global-namespace collision as CppStrategy's copy of this
                        // code — see its comment.
                        static const std::set<std::string> cmathCollisions = {
                            "sin","cos","tan","asin","acos","atan","atan2",
                            "sqrt","cbrt","pow","exp","log","log10","log2",
                            "floor","ceil","round","abs","hypot",
                        };
                        for (auto& sym : it->second)
                            if (!cmathCollisions.count(sym))
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
                    std::string libname  = basename.substr(0, basename.rfind('.'));
                    if (libname.rfind("lib", 0) == 0) libname = libname.substr(3);
                    emitRaw(out, "// FLIB_SO_LINK: " + ln);
                    emitRaw(out, "// Link: g++ out.cpp -L" + libdir + " -l" + libname);
                    emitRaw(out, "#include \"" + ln.substr(0, ln.rfind('.')) + ".h\"");
                } else {
                    emitRaw(out, "#include \"" + ln + ".hpp\"");
                }
            } else if (lt == "conglomer") {
                emitConglomer(out, ln);   // raw C header, dynamically linked (#conglomer)
            }
        }
        emitRaw(out, "");
    }
    // conglomer <foo.h>: include the header and DYNAMICALLY link its shared object.
    // `-l<stem>` links against lib<stem>.so at load time (dynamic); rpath finds it beside the header.
    void emitConglomer(std::ostringstream &out, const std::string &hdr)
    {
        // extern "C": a raw C header's symbols would otherwise be C++-name-mangled and fail to link.
        emitRaw(out, "extern \"C\" {");
        emitRaw(out, "#include \"" + hdr + "\"");
        emitRaw(out, "}");
        auto slash = hdr.rfind('/');
        std::string dir  = (slash == std::string::npos) ? "." : hdr.substr(0, slash);
        std::string base = (slash == std::string::npos) ? hdr : hdr.substr(slash + 1);
        std::string stem = base.substr(0, base.rfind('.'));      // foo.h → foo
        if (stem.rfind("lib", 0) == 0) stem = stem.substr(3);    // libfoo → foo
        emitRaw(out, "// CONGLOMER: " + hdr + " (dynamic)");
        // main.cpp's C++/LIB link-flag parser looks for "// Link: g++ <throwaway-token> <flags>"
        // (see the "// Link: g++ " prefix scan in main.cpp) — this used to emit "// Link: -I..."
        // with no "g++ " prefix at all, so the parser's rfind(prefix, 0)==0 check never matched
        // and these -I/-L/-l/-rpath flags were silently never passed to the real g++ invocation.
        emitRaw(out, "// Link: g++ out.cpp -I" + dir + " -L" + dir + " -l" + stem
                     + " -Wl,-rpath," + dir);
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        // Wrap top-level statements in ac_lib_init() instead of main()
        hasMain_ = true;
        declared.clear(); floatVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are file-scope globals
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "extern \"C\" {");
        emitRaw(out, "#endif");
        emit(out, indent, "void ac_lib_init() {");
        indent++;
        // Same hoist fix as CppStrategy::emitMainBegin (see its comment) — LibStrategy has its
        // own separate emitMainBegin override, so it needs its own copy too (verified:
        // examples/calculator.ac's REPL loop, "'result' was not declared in this scope").
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "std::string " + v + ";";
            else if (t == IRType::LIST)     line = "std::vector<long long> " + v + ";";
            else                            line = "long long "  + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "#ifdef __cplusplus");
        emitRaw(out, "} // extern \"C\"");
        emitRaw(out, "#endif");
    }
    // Tracks, per open function, whether it opened an extern "C" block. Class/bundle
    // methods (classOwner non-empty) get C++ linkage and must NOT emit the closing
    // brace — otherwise the end always-closed and left an unbalanced `}` (#24).
    std::vector<bool> libExternStack_;
    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name,
                           const std::string &params,
                           const std::string &classOwner) override
    {
        // Export every top-level function as extern "C"; class methods keep C++ linkage.
        bool opensExtern = classOwner.empty();
        libExternStack_.push_back(opensExtern);
        if (opensExtern) {
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "extern \"C\" {");
            emitRaw(out, "#endif");
        }
        CppStrategy::emitFunctionBegin(out, indent, name, params, classOwner);
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        CppStrategy::emitFunctionEnd(out, indent);
        bool opensExtern = true;
        if (!libExternStack_.empty()) { opensExtern = libExternStack_.back(); libExternStack_.pop_back(); }
        if (opensExtern) {
            emitRaw(out, "#ifdef __cplusplus");
            emitRaw(out, "} // extern \"C\"");
            emitRaw(out, "#endif");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 7. JAVA  (AC->Java)
// ═══════════════════════════════════════════════════════════════════════════

class JavaStrategy : public BackendStrategy
{
    std::set<std::string> declared;
    std::set<std::string> userStringFuncs_;
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
    std::set<std::string> listVars;   // #23: lists are ArrayList<Long> (appends propagate)
    std::set<std::string> floatVars;
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(v);   // #42: pre-inferred float locals
    }
    // Same purpose as floatVars, but for booleans — was completely missing (JavaStrategy had
    // NO boolean-var tracking at all, unlike float/list/string). glBoolFuncs (see emitCall)
    // correctly declares a call's IMMEDIATE result temp `boolean`, but a following plain copy
    // (`tts_ready = t_0`) had no way to know `t_0` was boolean and defaulted to `long`
    // (verified: audio_test.ac, "incompatible types: boolean cannot be converted to long" —
    // this is also very likely the root of the long-standing gd.ac/geodeo.ac/gl_bounce.ac/
    // pong.ac Java gl-boolean gap, same shape).
    std::set<std::string> boolVars;
    // `v == "true"`/`"false"` are Java boolean LITERALS (not tracked variable names, so
    // boolVars.count alone misses them) — verified: geodeo.ac/gl_bounce.ac/audio_test.ac's
    // `t_4 == true`-shaped comparisons (a gl bool-returning call's result checked against a
    // literal) hit emitComparison's truthy() lambda, which wrapped the literal as `(true != 0)`
    // — comparing boolean to int, "incomparable types". Recognizing the literal directly avoids
    // wrapping it at all.
    bool isBoolVal(const std::string &v) const { return v == "true" || v == "false" || boolVars.count(v) > 0; }
    // widgets ilib: `use ilib widgets` exposes bare ctor calls (`display(root,...)`, real Java
    // wrapper classes emitted into the FFI shim — see widgets_ffi.java's "Wrapper classes"
    // section) and dot-method calls (`lang_drop.add(...)`) that Java's own dotCallSyntax()==true
    // handles for free ONCE the receiver var is correctly TYPED. Before this: the shared
    // dispatcher misclassified every bare widget ctor name as an "indirect call" (a call through
    // a function-typed PARAMETER — see the shared dispatcher's isIndirect comment), routing it
    // to emitIndirectCall's `.applyAsLong()` fallback, which doesn't exist on any of these names
    // at all (verified: widgets_test.ac — "cannot find symbol: variable display" etc, 17 errors).
    std::map<std::string, std::string> widgetVarClass_;   // var name -> Java wrapper class name
    std::set<std::string> widgetCallbacks_;                // AC fn names needing an upcall stub
    std::map<std::string, int> userFuncArity_;
    void setUserFuncArity(const std::map<std::string, int>& m) override { userFuncArity_ = m; }
    // NA->free: a var assigned in <mainloop> and read/written by a LATER-declared callback
    // function (e.g. applicant_form.ac's `status_lbl = display(...)` in mainloop, read via
    // `status_lbl.set(...)` inside `OnSubmit`, a function textually declared BEFORE mainloop
    // even runs) needs to be a Java field, not a local of either method — a plain local in
    // main() is invisible from OnSubmit ("cannot find symbol"). Every OTHER backend already
    // implements this (see CppStrategy's own promotedGlobals_/structGlobals_, the reference this
    // mirrors); JavaStrategy had NONE of it — the shared dispatcher already computes exactly the
    // data needed (setPromotedGlobals/setStructGlobals are called unconditionally for any
    // backend) and nothing was consuming it here.
    std::set<std::string> promotedGlobals_;
    void setPromotedGlobals(const std::vector<std::string>& vars) override {
        for (auto& v : vars) promotedGlobals_.insert(v);
    }
    // Subset of promotedGlobals_ built from a widget ctor call in <mainloop> (var -> AC ctor
    // name, e.g. "display") — see the shared dispatcher's setStructGlobals call site for exactly
    // how this gets computed (a scan of ir.globalInit for CALL results matching a promoted var).
    std::map<std::string, std::string> structGlobals_;
    void setStructGlobals(const std::map<std::string, std::string>& m) override {
        structGlobals_ = m;
        // Pre-seed widgetVarClass_ NOW (this runs once, before any function's codegen starts) so
        // javaWidgetCtor's typeChanged check sees the field's real type immediately when
        // <mainloop> constructs it, instead of treating the first assignment as a fresh local.
        for (auto& [v, ctorName] : m) {
            std::string cls = widgetJavaClass(ctorName);
            if (!cls.empty()) widgetVarClass_[v] = cls;
        }
    }
    static bool isWidgetCtor(const std::string& func) {
        static const std::set<std::string> ctors = {
            "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
            "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch"
        };
        return ctors.count(func) > 0;
    }
    static std::string widgetJavaClass(const std::string& func) {
        static const std::map<std::string,std::string> m = {
            {"Screen","Screen"}, {"display","AcDisplay"}, {"ask","AcAsk"}, {"btn","AcBtn"},
            {"ckbtn","AcCkbtn"}, {"radbtn","AcRadbtn"}, {"dropdown","AcDropdown"},
            {"advance","AcAdvance"}, {"slider","AcSlider"}, {"group","AcGroup"},
            {"tabs","AcTabs"}, {"scroller","AcScroller"}, {"listbox","AcListbox"},
            {"table","AcTable"}, {"sketch","AcSketch"},
        };
        auto it = m.find(func);
        return it == m.end() ? "" : it->second;
    }
    static std::vector<std::string> splitTopArgs(const std::string &args) {
        std::vector<std::string> out;
        std::string cur; int depth = 0; bool inStr = false;
        for (char c : args) {
            if (c == '"') inStr = !inStr;
            if (!inStr && (c == '(' || c == '[')) depth++;
            if (!inStr && (c == ')' || c == ']')) depth--;
            if (c == ',' && depth == 0 && !inStr) { out.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        for (auto &s : out) {
            size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
            s = (a != std::string::npos) ? s.substr(a, b - a + 1) : s;
        }
        return out;
    }
    // Widget ctor: `res = display(root, "text")` etc → `AcDisplay res = new AcDisplay(root, "text");`.
    // Java's own dotCallSyntax()==true means METHOD calls (`lang_drop.add(...)`) need no special
    // handling at all beyond this — they already emit as native `recv.method(args);` text via the
    // generic path, and resolve correctly once `recv` is declared with the real wrapper type here.
    bool javaWidgetCtor(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &func, const std::string &args) {
        if (!isWidgetCtor(func)) return false;
        std::string cls = widgetJavaClass(func);
        if (cls.empty()) return false;
        std::vector<std::string> a = splitTopArgs(args);
        // `btn(master, text, Callback)` — the 3rd arg is a user function passed BY NAME (already
        // formatted as a Java method reference `Class::Name` via funcArgRef — see the shared
        // dispatcher's userFuncNames_ check). Strip it from the ctor call (AcBtn's constructor
        // only takes master+text) and register it for a static upcall-stub bridge instead —
        // see emitFooter for where those get emitted.
        std::string cbName;
        if (func == "btn" && a.size() > 2) {
            std::string cbArg = a[2];
            auto cc = cbArg.rfind("::");
            cbName = (cc != std::string::npos) ? cbArg.substr(cc + 2) : cbArg;
            a.resize(2);
        }
        // `dropdown(root, lazy)` — the `lazy` sentinel defers auto-pack (see below) to a manual
        // caller-side pack later; strip it from the real constructor arg list (not a real ctor
        // param on any wrapper class — verified: applicant_form.ac, "cannot find symbol: variable
        // lazy" once it fell through to a plain identifier reference).
        bool isLazy = !a.empty() && (a.back() == "lazy" || a.back() == "\"lazy\"");
        if (isLazy) a.pop_back();
        // Positions whose Java constructor param is `int` — a bare AC integer arg always formats
        // as a `long` literal/expression (`30L`), which javac refuses to narrow implicitly
        // (verified: "incompatible types: possible lossy conversion from long to int" on
        // AcAsk(Screen,int)'s width and AcAdvance(Screen,int)'s length).
        static const std::map<std::string, std::set<int>> intParamPositions = {
            {"ask", {1}}, {"advance", {1}}, {"listbox", {1, 2}}, {"table", {2}}, {"sketch", {1, 2}},
        };
        auto ipIt = intParamPositions.find(func);
        if (ipIt != intParamPositions.end())
            for (int pos : ipIt->second)
                if (pos < (int)a.size()) a[pos] = "(int)(" + a[pos] + ")";
        std::string joined;
        for (size_t i = 0; i < a.size(); i++) { if (i) joined += ", "; joined += a[i]; }
        std::string call = "new " + cls + "(" + joined + ")";
        if (!cbName.empty()) widgetCallbacks_.insert(cbName);
        // AC reuses a single "throwaway" var name across ctor calls of DIFFERENT widget kinds
        // (verified: applicant_form.ac's `_h = display(...)` repeated, then finally `_h =
        // btn(...)`) — every other backend's widget handle is one untyped scalar, so this is a
        // harmless rebind there, but Java's `_h` would need to hold two INCOMPATIBLE class
        // types. A same-name reassignment to a DIFFERENT widget class still needs SOME Java
        // variable to chain the pack/on-click calls onto below (evaluating `new Cls(...)` a
        // second time would construct a SECOND, different widget instead) — declare it under a
        // throwaway counter-suffixed name instead of the real AC var name, which the ORIGINAL
        // (differently-typed) `res` still holds; these discard vars are, by construction, never
        // read back afterward (the value only matters as a side effect of construction).
        std::string recv = res;
        bool typeChanged = declared.count(res) > 0 && widgetVarClass_.count(res) && widgetVarClass_[res] != cls;
        bool alreadySameClass = declared.count(res) > 0 && !typeChanged;
        if (res.empty() || typeChanged) recv = "_ac_wtmp" + std::to_string(widgetTmpCounter_++);
        emit(out, indent, (alreadySameClass ? "" : cls + " ") + recv + " = " + call + ";");
        widgetVarClass_[recv] = cls;
        if (!typeChanged && !res.empty()) declared.insert(res);
        if (!cbName.empty()) emit(out, indent, recv + ".onClick(_ac_cb_" + cbName + "_stub);");
        // Every kind except Screen auto-packs unless `lazy` was passed (mirrors CStrategy's
        // packFnFor/isLazy handling exactly) — every wrapper class has a `.pack()` method.
        // Without this, a constructed widget was never shown at all (no other emission path
        // ever called it — verified against the actual FFI shim's constructors, none of which
        // pack internally).
        if (func != "Screen") {
            if (isLazy) emit(out, indent, "AcWidgets.setLazy(" + recv + "._h);");
            else        emit(out, indent, recv + ".pack();");
        }
        return true;
    }
    int widgetTmpCounter_ = 0;
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    std::string className = "Main";
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::set<std::string> stringParams_;  // function params that should be typed as String

    bool returnIsFloat_ = false;
    bool returnIsVoid_ = false;
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
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
    std::set<std::string> floatParams_;
    void setFloatParams(const std::set<std::string>& s) override { floatParams_ = s; }
    bool curFuncReturnIsString_ = false;
    std::string funcArgRef(const std::string &name) override { return className + "::" + name; }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false, hasEvalOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool /*idiv*/, bool ev, bool /*etry*/) override {
        hasIpowOp_ = ip; hasRandomOp_ = r; hasEvalOp_ = ev;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        emit(out, indent, "_acSaved.append(String.valueOf(" + val + ")).append(\"\\n\");");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "try (java.io.PrintWriter _acPw = new java.io.PrintWriter(" + filename + ")) { _acPw.print(_acSaved.toString()); } catch (java.io.IOException _acE) {}");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
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
        if (isMLFloatReturningFunc(fn)) return true;
        if (isIntReturningMathFunc(s)) return false;
        return strncmp(s, "math_", 5) == 0 || strncmp(s, "math.", 5) == 0 || strncmp(s, "stat_", 5) == 0;
    }
    bool isFloatVal(const std::string &v) const { return looksFloat(v) || floatVars.count(v) || isKnownFloatName(v); }

    void emit(std::ostringstream &out, int indent, const std::string &line) override
    {
        out << std::string(indent * 4, ' ') << line << "\n";
    }
    void emitRaw(std::ostringstream &out, const std::string &line) override { out << line << "\n"; }

    void setOutputStem(const std::string &stem) override {
        // Java class names must be valid identifiers — a file stem containing a hyphen (a
        // perfectly normal AC source filename, e.g. `hello-world.ac`) produced literally
        // `public class hello-world {`, a hard parse error ("'{' expected") on every single
        // program from such a file, unconditionally (verified: examples/hello-world.ac).
        className = stem;
        for (char& c : className)
            if (!std::isalnum((unsigned char)c) && c != '_') c = '_';
        if (!className.empty() && std::isdigit((unsigned char)className[0])) className = "_" + className;
    }
    void setPendingImports(const std::vector<std::pair<std::string,std::string>>& imp) override
    {
        pendingImports_ = imp;
    }

    void emitHeader(std::ostringstream &out) override
    {
        emitRaw(out, "// Generated by AC Compiler (AC->Java)");
        // Java only allows `import` statements before ANY type declaration in the file — but
        // each ilib FFI file's own `import java.io.*;`-style lines were being emitted INLINE,
        // wherever that file's content landed in the concatenation order, so importing a
        // SECOND ilib after a first one put its imports mid-file, after the first ilib's class
        // body already closed — "class, interface, enum, or record expected" (verified:
        // applicant_form.ac / jarvis.ac, both importing widgets + os together). Collect every
        // `import ...;` line from every ilib FFI file first (deduped) and emit them all up
        // front; each file's own body (with its import lines stripped) is emitted in place
        // exactly as before.
        std::set<std::string> javaImports_;
        std::vector<std::string> javaImportLines_;
        std::map<std::string, std::string> ffiBodies_;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt != "ilib" || ln == "math") continue;
            std::string ffi = readFFIFile(ln, "java");
            if (ffi.empty()) continue;
            std::istringstream ffiStream(ffi);
            std::string ffiLine, body;
            while (std::getline(ffiStream, ffiLine)) {
                if (ffiLine.rfind("import ", 0) == 0) {
                    if (javaImports_.insert(ffiLine).second) javaImportLines_.push_back(ffiLine);
                } else {
                    body += ffiLine; body += "\n";
                }
            }
            ffiBodies_[ln] = body;
        }
        for (auto& imp : javaImportLines_) emitRaw(out, imp);
        // ilib FFI classes emitted before main class (Java allows multiple non-public classes per file)
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib" && ln == "math") {
                // Java math = NATIVE java.lang.Math shim — the FFI file needed the preview
                // java.lang.foreign Linker (disabled by default) and shipped a public class
                // that collided with the program class. No FFI needed for math on the JVM.
                emitRaw(out, "class math {");
                emitRaw(out, "    static final double pi = Math.PI, e = Math.E, tau = 2*Math.PI, phi = 1.6180339887498949, em = 0.5772156649015329;");
                emitRaw(out, "    static double sin(double x){return Math.sin(x);} static double cos(double x){return Math.cos(x);} static double tan(double x){return Math.tan(x);}");
                emitRaw(out, "    static double asin(double x){return Math.asin(x);} static double acos(double x){return Math.acos(x);} static double atan(double x){return Math.atan(x);}");
                emitRaw(out, "    static double sqrt(double x){return Math.sqrt(x);} static double cbrt(double x){return Math.cbrt(x);} static double exp(double x){return Math.exp(x);}");
                emitRaw(out, "    static double ln(double x){return Math.log(x);} static double log10(double x){return Math.log10(x);} static double log2(double x){return Math.log(x)/Math.log(2);}");
                emitRaw(out, "    static double log(double b, double x){return Math.log(x)/Math.log(b);}");
                emitRaw(out, "    static double floor(double x){return Math.floor(x);} static double ceil(double x){return Math.ceil(x);} static double round(double x){return Math.round(x);}");
                emitRaw(out, "    static double abs(double x){return Math.abs(x);} static long abs_int(long x){return Math.abs(x);}");
                emitRaw(out, "    static double pow(double a,double b){return Math.pow(a,b);} static double hypot(double a,double b){return Math.hypot(a,b);}");
                emitRaw(out, "    static double min(double a,double b){return Math.min(a,b);} static double max(double a,double b){return Math.max(a,b);}");
                emitRaw(out, "    static long mod(long a,long b){ long r=a%b; if(r!=0&&((r<0)!=(b<0))) r+=b; return r; }");
                // `math.mod_int` can be called DIRECTLY from AC source (not only reached via the
                // `math.mod` dispatch, which already overloads to this same int path) — the
                // injected shim never defined it under this exact name at all (verified:
                // examples/math_number.ac's `math.mod_int(n, 2)` — "cannot find symbol: method
                // mod_int(long,long)").
                emitRaw(out, "    static long mod_int(long a,long b){ return mod(a,b); }");
                emitRaw(out, "    static double mod(double a,double b){ double r=a%b; if(r!=0&&((r<0)!=(b<0))) r+=b; long ri=(long)r; return r==ri?ri:r; }");
                emitRaw(out, "    static long is_prime(long n){ if(n<2) return 0; for(long d=2;d*d<=n;d++) if(n%d==0) return 0; return 1; }");
                emitRaw(out, "    static long to_int(double x){return (long)x;} static double deg2rad(double x){return Math.toRadians(x);} static double rad2deg(double x){return Math.toDegrees(x);}");
                emitRaw(out, "}");
                continue;
            }
            if (lt == "ilib") {
                auto it = ffiBodies_.find(ln);
                if (it != ffiBodies_.end() && !it->second.empty()) out << it->second << "\n";
            }
        }
        // eval(): self-contained arithmetic evaluator (+ - * / parens, unary +/-), ported
        // directly from CStrategy's `_ac_builtin_eval` (same grammar: factor→term→expr). Java
        // never had ANY implementation — `setUsedBuiltinOps`'s `eval` param was silently
        // discarded (`bool /*eval*/`), and emitEval unconditionally called `AcMath.math_eval(...)`
        // — a class ("AcMath") that doesn't exist anywhere in Java's output (the real math shim
        // is named `math`, lowercase, native java.lang.Math-backed — see its own comment above)
        // AND a method ("math_eval") that doesn't exist on it either, even when math IS imported.
        // Verified: calculator.ac's `result = eval(line)` — "cannot find symbol: variable AcMath".
        if (hasEvalOp_) {
            emitRaw(out, "final class _AcEval {");
            emitRaw(out, "    private final String s; private int p = 0;");
            emitRaw(out, "    private _AcEval(String s) { this.s = s; }");
            emitRaw(out, "    static double eval(String s) { return s == null ? 0.0 : new _AcEval(s).expr(); }");
            emitRaw(out, "    private void ws() { while (p < s.length() && (s.charAt(p) == ' ' || s.charAt(p) == '\\t')) p++; }");
            emitRaw(out, "    private double factor() {");
            emitRaw(out, "        ws();");
            emitRaw(out, "        if (p < s.length() && s.charAt(p) == '(') {");
            emitRaw(out, "            p++; double v = expr(); ws();");
            emitRaw(out, "            if (p < s.length() && s.charAt(p) == ')') p++;");
            emitRaw(out, "            return v;");
            emitRaw(out, "        }");
            emitRaw(out, "        if (p < s.length() && s.charAt(p) == '-') { p++; return -factor(); }");
            emitRaw(out, "        if (p < s.length() && s.charAt(p) == '+') { p++; return factor(); }");
            emitRaw(out, "        int start = p;");
            emitRaw(out, "        while (p < s.length() && (Character.isDigit(s.charAt(p)) || s.charAt(p) == '.')) p++;");
            emitRaw(out, "        if (p == start) return 0.0;");
            emitRaw(out, "        try { return Double.parseDouble(s.substring(start, p)); } catch (NumberFormatException e) { return 0.0; }");
            emitRaw(out, "    }");
            emitRaw(out, "    private double term() {");
            emitRaw(out, "        double v = factor();");
            emitRaw(out, "        for (;;) {");
            emitRaw(out, "            ws();");
            emitRaw(out, "            if (p >= s.length()) break;");
            emitRaw(out, "            char c = s.charAt(p);");
            emitRaw(out, "            if (c == '*') { p++; v *= factor(); }");
            emitRaw(out, "            else if (c == '/') { p++; double d = factor(); v = d != 0.0 ? v / d : 0.0; }");
            emitRaw(out, "            else break;");
            emitRaw(out, "        }");
            emitRaw(out, "        return v;");
            emitRaw(out, "    }");
            emitRaw(out, "    private double expr() {");
            emitRaw(out, "        double v = term();");
            emitRaw(out, "        for (;;) {");
            emitRaw(out, "            ws();");
            emitRaw(out, "            if (p >= s.length()) break;");
            emitRaw(out, "            char c = s.charAt(p);");
            emitRaw(out, "            if (c == '+') { p++; v += term(); }");
            emitRaw(out, "            else if (c == '-') { p++; v -= term(); }");
            emitRaw(out, "            else break;");
            emitRaw(out, "        }");
            emitRaw(out, "        return v;");
            emitRaw(out, "    }");
            emitRaw(out, "}");
        }
        emitRaw(out, "public class " + className + " {");
        if (needsSave_)
            emitRaw(out, "    static final StringBuilder _acSaved = new StringBuilder();  // `save as`: accumulates everything printed so far");
        if (anyAtomicVars())
            emitRaw(out, "    static final Object _ac_atomic_lock = new Object();  // `atomic` vars: any op touching one is a global critical section");
        if (needsEvents_) {
            emitRaw(out, "    static final java.util.Map<String, Runnable> _acEvents = new java.util.HashMap<>();");
            emitRaw(out, "    static void _acBind(String key, Runnable fn) {");
            emitRaw(out, "        _acEvents.put(key, fn);");
            emitRaw(out, "    }");
            emitRaw(out, "    static void _acTrigger(String key) {");
            emitRaw(out, "        Runnable f = _acEvents.get(key);");
            emitRaw(out, "        if (f != null) f.run();");
            emitRaw(out, "    }");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "    static String ac_iota(long n) {");
        emitRaw(out, "        StringBuilder b = new StringBuilder();");
        emitRaw(out, "        for (long i = 0; i < n; i++) b.append(i);");
        emitRaw(out, "        return b.toString();");
        emitRaw(out, "    }");
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually uses it.
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "    static long ac_ipow(long b, long e) {");
            emitRaw(out, "        long r = 1;");
            emitRaw(out, "        while (e-- > 0) r *= b;");
            emitRaw(out, "        return r;");
            emitRaw(out, "    }");
        }
        if (hasRandomOp_) {
            emitRaw(out, "    static java.util.Random _acr = new java.util.Random();");
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "    static long ac_rand(long n) {");
            emitRaw(out, "        return n > 0 ? (long)(_acr.nextDouble() * n) : 0;");
            emitRaw(out, "    }");
            // `random.choice(list)`
            emitRaw(out, "    static long ac_choice(java.util.ArrayList<Long> xs) {");
            emitRaw(out, "        return xs.get((int)ac_rand(xs.size()));");
            emitRaw(out, "    }");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "    static String ac_stream(long a, long b, long s) {");
        emitRaw(out, "        if (s == 0) s = 1;");
        emitRaw(out, "        StringBuilder o = new StringBuilder();");
        emitRaw(out, "        for (long i = a; (s > 0) ? (i < b) : (i > b); i += s) o.append(i);");
        emitRaw(out, "        return o.toString();");
        emitRaw(out, "    }");
        emitRaw(out, "    static String ac_stream(long a, long b) {");
        emitRaw(out, "        return ac_stream(a, b, 1);");
        emitRaw(out, "    }");
        emitRaw(out, "");
    }
    void emitFooter(std::ostringstream &out) override
    {
        // widgets ilib callback bridges (`btn(master, text, OnClick)` / `.onClick(...)`) — Java
        // static-field/method resolution doesn't care about textual order within a class, so
        // these can be emitted here (AFTER main() in the output text) even though main() already
        // references them by name — see widgetVarClass_'s header comment for the full mechanism.
        // Each AC callback gets its OWN upcall stub bound directly to it (no runtime userdata
        // dispatch needed, unlike C/ASM/BNY's single generic adapter — the callback identity is
        // already fully known at codegen time here).
        for (auto& name : widgetCallbacks_) {
            int arity = userFuncArity_.count(name) ? userFuncArity_.at(name) : 0;
            std::string callArgs = arity > 0 ? "0L" : "";
            emitRaw(out, "    private static void _ac_cb_" + name + "_bridge(java.lang.foreign.MemorySegment ud) { "
                       + name + "(" + callArgs + "); }");
            emitRaw(out, "    private static final java.lang.foreign.MemorySegment _ac_cb_" + name + "_stub;");
            emitRaw(out, "    static {");
            emitRaw(out, "        try {");
            emitRaw(out, "            java.lang.invoke.MethodHandle _mh = java.lang.invoke.MethodHandles.lookup().findStatic("
                       + className + ".class, \"_ac_cb_" + name + "_bridge\", "
                       + "java.lang.invoke.MethodType.methodType(void.class, java.lang.foreign.MemorySegment.class));");
            emitRaw(out, "            _ac_cb_" + name + "_stub = java.lang.foreign.Linker.nativeLinker().upcallStub(_mh, "
                       + "java.lang.foreign.FunctionDescriptor.ofVoid(java.lang.foreign.ValueLayout.ADDRESS), "
                       + "java.lang.foreign.Arena.global());");
            emitRaw(out, "        } catch (Throwable _t) { throw new RuntimeException(_t); }");
            emitRaw(out, "    }");
        }
        // NA->free promoted globals — see promotedGlobals_'s header comment. Widget-typed ones
        // (structGlobals_) are declared null and constructed later by javaWidgetCtor's first
        // real assignment in <mainloop> (mirrors CppStrategy's `TypeName* v = nullptr;` — no
        // default constructor exists on any of these wrapper classes either); non-widget ones
        // fall back to `long` (AC's own untyped-scalar default — matches every backend's
        // plainest promoted-global case, no observed example needs a promoted string/float here).
        for (auto& v : promotedGlobals_) {
            auto sit = structGlobals_.find(v);
            if (sit != structGlobals_.end()) {
                std::string cls = widgetJavaClass(sit->second);
                emitRaw(out, "    static " + (cls.empty() ? "long" : cls) + " " + v + (cls.empty() ? " = 0;" : " = null;"));
            } else {
                emitRaw(out, "    static long " + v + " = 0;");
            }
        }
        emitRaw(out, "}  // class " + className);
    }

    bool dotCallSyntax() const override { return true; }

    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::INT)
            return std::to_string(std::get<int64_t>(r.value.data)) + "L";
        std::string s = commonRef(r, sym, "true", "false", "null", "null");
        // Bundle field READ (self.field as an operand, not just a STORE_VAR target): same
        // this.field translation as decl() applies to writes.
        if (s.rfind("self.", 0) == 0) return "this." + s.substr(5);
        // Dot-style (math.pi, math.sin): go to the `math` namespace class directly. This
        // referenced "AcMath" — a class that doesn't exist anywhere in Java's output (the real
        // shim class, emitted in emitHeader, is named `math`, lowercase, native
        // java.lang.Math-backed) — matching the exact same stale-name bug emitEval had (see its
        // comment). Only reachable for a bare known-float-constant name with no dot at all
        // (`pi` used via `using math`-style bare import), so it likely never fired on any
        // CURRENTLY passing example — but any program hitting it would fail exactly like
        // calculator.ac did.
        if (r.kind == IRRef::Kind::VAR && isKnownFloatName(s) && s.find('.') == std::string::npos)
            return "math." + s;
        return s;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...) inside a method: the field is a real class
        // member, declared once by emitClassBegin's field-decl pass — never redeclared here.
        // `var` may arrive raw ("self.field") or already translated ("this.field", from
        // formatRef() on the READ side) — handle both, never double-translate.
        if (var.rfind("self.", 0) == 0) return "this." + var.substr(5) + " = " + val + ";";
        if (var.rfind("this.", 0) == 0) return var + " = " + val + ";";
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "double " + var + " = (double)(" + val + ");"; }
            if (ct == IRType::STRING) return "String " + var + " = String.valueOf(" + val + ");";
            if (ct == IRType::INT || ct == IRType::BOOL) return "long " + var + " = (long)(" + val + ");";
            // `looksString` alone only matches a literal `"..."` — a plain copy from a TRACKED
            // string variable (`n = t_0` where `t_0` was typed String by a widget `.get()`
            // call) fell through to the `long` default (verified: applicant_form.ac,
            // "incompatible types: String cannot be converted to long") — same plain-copy
            // propagation gap dictVars_ already needed a fix for below.
            if (looksString(val) || isStringVar(val)) { stringVars_.insert(var); return "String " + var + " = " + val + ";"; }
            if (isFloatVal(val))  { floatVars.insert(var); return "double " + var + " = " + val + ";"; }
            if (isBoolVal(val))   { boolVars.insert(var);  return "boolean " + var + " = " + val + ";"; }
            // Same plain-copy propagation gap float/bool/string already needed a check for —
            // `p1 = pets[1]` (a LOAD_INDEX into a TEMP, correctly typed Map<String,X> by
            // emitLoadIndex's listOfDictVars_ branch, THEN a plain STORE_VAR copying that temp
            // into `p1`) never propagated dict-ness to `p1` itself, since decl()'s fallback
            // chain never checked dictVars_ (verified: keyword_catalog_modules.ac — `long p1 =
            // t_10;` where t_10 IS a real Map, then `p1.get((int)("age"))` — "String cannot be
            // converted to int").
            if (dictVars_.count(val)) {
                dictVars_.insert(var);
                bool strVal = dictStrVals_.count(val) > 0;
                if (strVal) dictStrVals_.insert(var);
                return "java.util.Map<String," + std::string(strVal ? "String" : "Long") + "> " + var + " = " + val + ";";
            }
            return "long " + var + " = " + val + ";";
        }
        return var + " = " + val + ";";
    }

    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        bool valIsFloat = isFloatVal(val);
        // Reassigning a `short`/`mini` var: Java int-arith on a long literal widens to long, so the
        // store needs the (int)/(short) narrowing back into the fixed-width type from the type include.
        IRType ct = castDeclType(var, IRType::VOID);
        if (irIntWidth(ct) && declared.count(var) && !valIsFloat) {
            emit(out, indent, var + " = (" + acIntTypeJava(irIntWidth(ct)) + ")(" + val + ");");
            return;
        }
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
    { emit(out, indent, decl(res, "Math.floorMod((long)(" + lhs + "), (long)(" + rhs + "))")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as decl(): real class member, no redeclaration, this.field not self.field.
        if (var.rfind("self.", 0) == 0 || var.rfind("this.", 0) == 0) { emit(out, indent, decl(var, val)); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        // The declaration itself stays OUTSIDE the synchronized block (a Java local declared inside
        // a block is scoped to it — later reads of `var` would be "cannot find symbol").
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            if (declared.insert(var).second) emit(out, indent, "long " + var + " = 0;");
            emit(out, indent, "synchronized (_ac_atomic_lock) {");
            emit(out, indent + 1, var + " = (long)(" + val + ");");
            emit(out, indent, "}");
            return;
        }
        // Re-typing coercion (#retype): a var ever assigned a string is a String everywhere;
        // non-strings get String.valueOf'd — `x=5; x=$hi$` → `String x=String.valueOf(5); x="hi";`.
        if (isStringVar(var)) {
            std::string rhs = (looksString(val) || isStringVar(val)) ? val : "String.valueOf(" + val + ")";
            if (declared.insert(var).second) emit(out, indent, "String " + var + " = " + rhs + ";");
            else                             emit(out, indent, var + " = " + rhs + ";");
            return;
        }
        if (looksString(val)) {   // #retype numeric-unified ← stringified number: strip + type it
            std::string bare = acUnstring(val);
            std::string ty = (bare.find('.') != std::string::npos) ? "double " : "long ";
            if (declared.insert(var).second) emit(out, indent, ty + var + " = " + bare + ";");
            else                             emit(out, indent, var + " = " + bare + ";");
            return;
        }
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : t);
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        bool isFloat = isFloatVal(lhs) || isFloatVal(rhs);
        // String concat: Java's + works natively — but the RESULT must be String, not long.
        auto looksStr2 = [&](const std::string& v) {
            return (!v.empty() && v.front() == '"') || isStringVar(v);
        };
        if (op == "+" && (isStringVar(res) || looksStr2(lhs) || looksStr2(rhs))) {
            bool isNew2 = declared.insert(res).second;
            emit(out, indent, (isNew2 ? "String " : "") + res + " = " + lhs + " + " + rhs + ";");
            return;
        }
        std::string expr = lhs + " " + op + " " + rhs;
        bool isNew = declared.insert(res).second;
        // Result is a `short`/`mini` var: Java int-arith widens to long, so narrow back into the
        // fixed-width type from the type include (int/short) on every write.
        IRType rct = castDeclType(res, IRType::VOID);
        if (irIntWidth(rct) && !isFloat) {
            std::string ty = acIntTypeJava(irIntWidth(rct));
            emit(out, indent, (isNew ? ty + " " : "") + res + " = (" + ty + ")(" + expr + ");");
            return;
        }
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "double " + res + " = " + expr + ";"); }
        else if (isNew)        emit(out, indent, "long " + res + " = " + expr + ";");
        else if (isFloat && !floatVars.count(res))
                               emit(out, indent, res + " = (long)(" + expr + ");");
        else                   emit(out, indent, res + " = " + expr + ";");
    }
    void emitComparison(std::ostringstream &out, int &indent, const std::string &res,
                        const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // Java String equality is CONTENT via .equals — `==` compares references (#6:
        // `c == "1"` was always false, silently zeroing bin_to_dec).
        auto looksStr = [&](const std::string& v) {
            return (!v.empty() && v.front() == '"') || isStringVar(v);
        };
        if ((op == "==" || op == "!=") && (looksStr(lhs) || looksStr(rhs))) {
            std::string eq = "(" + lhs + ").equals(" + rhs + ")";
            std::string expr2 = (op == "==" ? eq : "!" + eq) + " ? 1L : 0L";
            emit(out, indent, decl(res, expr2));
            return;
        }
        // Same boolean-vs-long gap as emitIfBegin/emitJumpIfFalse (see their comments) — a
        // glBoolFuncs-typed operand is a real Java boolean; `== 0`/`!= 0` on one is a type
        // error, not just redundant (verified: geodeo.ac's `not is_obj(arg)` — "incomparable
        // types: boolean and int" — `not` lowers through this exact "not" branch below).
        auto truthy = [&](const std::string &v) { return isBoolVal(v) ? v : "(" + v + " != 0)"; };
        std::string expr;
        if (op == "xor")
            expr = "(" + truthy(lhs) + " != " + truthy(rhs) + ") ? 1L : 0L";
        else if (op == "xnor")
            expr = "(" + truthy(lhs) + " == " + truthy(rhs) + ") ? 1L : 0L";
        else if (op == "xsub")
            expr = "Math.abs((" + lhs + ") - (" + rhs + ")) + 1L";
        else if (op == "not")
            expr = isBoolVal(lhs) ? ("!" + lhs + " ? 1L : 0L") : ("(" + lhs + " == 0) ? 1L : 0L");
        else if (op == "==" || op == "!=")
            // Short-circuit `or`/`and` lowering (ir.cpp's `ac_sc_N` temps) explicitly compares
            // a possibly-boolean operand against 0 with `!=`/`==` — same gap, different shape
            // (verified: pong.ac's `(A overlap B) or (A overlap C)` — "incomparable types:
            // boolean and long" on `t_6 != 0L` where t_6 is AcGl.hitboxOverlap's boolean result).
            expr = "(" + truthy(lhs) + " " + op + " " + truthy(rhs) + ") ? 1L : 0L";
        else
            expr = "(" + lhs + " " + op + " " + rhs + ") ? 1L : 0L";
        emit(out, indent, decl(res, expr));
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        if (javaWidgetCtor(out, indent, res, func, args)) return;
        // sketch.{line,rect,circle,text_at}(...) — the trailing r,g,b args need an explicit
        // (byte) cast: an AC integer arg always formats as a `long` literal/expression, and
        // javac refuses to narrow that implicitly (verified: "incompatible types: possible
        // lossy conversion from long to byte" on all four methods). Java's own dotCallSyntax()
        // otherwise needs no special handling for widget methods at all — see javaWidgetCtor's
        // header comment — this is purely a numeric-narrowing fixup, not a dispatch gap.
        {
            auto dot = func.rfind('.');
            if (dot != std::string::npos) {
                std::string recv = func.substr(0, dot), method = func.substr(dot + 1);
                auto wit = widgetVarClass_.find(recv);
                if (wit != widgetVarClass_.end() && wit->second == "AcSketch"
                        && (method == "line" || method == "rect" || method == "circle" || method == "text_at")) {
                    std::vector<std::string> a = splitTopArgs(args);
                    for (size_t i = a.size() >= 3 ? a.size() - 3 : 0; i < a.size(); i++)
                        a[i] = "(byte)(" + a[i] + ")";
                    std::string joined;
                    for (size_t i = 0; i < a.size(); i++) { if (i) joined += ", "; joined += a[i]; }
                    emit(out, indent, func + "(" + joined + ");");
                    return;
                }
            }
        }
        // `x = recv.get()` where recv is a widget var — the generic dot-call fallback below
        // would declare `x` as `long` (its only default), but every wrapper class's `.get()`
        // returns something else entirely (verified: applicant_form.ac's `n = name_inp.get()` —
        // "incompatible types: String cannot be converted to long"). Needs its own type map
        // since Java has no other way to infer a method call's return type from its text.
        {
            auto dot = func.rfind('.');
            if (dot != std::string::npos && args.empty() && !res.empty()) {
                std::string recv = func.substr(0, dot), method = func.substr(dot + 1);
                auto wit = widgetVarClass_.find(recv);
                if (wit != widgetVarClass_.end() && method == "get") {
                    static const std::map<std::string,std::string> getReturnType = {
                        {"AcAsk","String"}, {"AcDisplay","String"}, {"AcDropdown","String"},
                        {"AcAdvance","double"}, {"AcSlider","double"},
                        {"AcCkbtn","boolean"}, {"AcRadbtn","boolean"},
                        {"AcListbox","java.util.List<String>"},
                    };
                    auto tIt = getReturnType.find(wit->second);
                    if (tIt != getReturnType.end()) {
                        std::string ty = tIt->second;
                        bool isNew = declared.insert(res).second;
                        emit(out, indent, (isNew ? ty + " " : "") + res + " = " + func + "();");
                        if (ty == "String") stringVars_.insert(res);
                        else if (ty == "boolean") boolVars.insert(res);
                        else if (ty == "double") floatVars.insert(res);
                        return;
                    }
                }
            }
        }
        if (func == "ac_length" && !res.empty()) { bool nw = declared.insert(res).second;
            // Java: String → .length(); ArrayList → .size() (#6/#23). `isStringVar` alone only
            // recognizes a tracked VARIABLE name — a raw literal (`length $hello$`) isn't one,
            // so it fell through to `.size()`, which String has no such method
            // ("cannot find symbol: method size(); location: class String").
            std::string lenExpr = (looksString(args) || isStringVar(args)) ? "(" + args + ").length()" : "(" + args + ").size()";
            emit(out, indent, (nw ? "long " : "") + res + " = " + lenExpr + ";"); return; }
        { auto ap = func.rfind(".append");
          if (ap != std::string::npos && ap == func.size() - 7) {
              std::string recv = func.substr(0, ap);
              emit(out, indent, recv + ".add((long)(" + args + "));");  // #23: mutates the SHARED list
              return; } }
        // Dot-chain string methods (`speech.lower()`) — Java's dotCallSyntax()==true means AC's
        // dotted method calls translate DIRECTLY to Java's native dot-call with no rewriting, an
        // assumption that breaks for these: `lower`/`upper`/`strip`/`trim` aren't real Java
        // String method names (verified: jarvis.ac's `speech.lower()` — "cannot find symbol:
        // method lower()"). Every OTHER backend already recognizes this exact dot-chain shape as
        // string-cheese sugar (see lowerExpr's `strMethods` set) — Java needs its own mapping to
        // the REAL String method names since it never rewrites the call text at all otherwise.
        {
            auto dot = func.rfind('.');
            if (dot != std::string::npos && args.empty()) {
                std::string recv = func.substr(0, dot), method = func.substr(dot + 1);
                static const std::unordered_map<std::string,std::string> strMethodMap = {
                    {"lower","toLowerCase"}, {"LOWER","toLowerCase"},
                    {"upper","toUpperCase"}, {"UPPER","toUpperCase"},
                    {"strip","strip"}, {"STRIP","strip"}, {"trim","trim"}, {"TRIM","trim"},
                };
                auto mit = strMethodMap.find(method);
                if (mit != strMethodMap.end() && (isStringVar(recv) || looksString(recv))) {
                    std::string call2 = recv + "." + mit->second + "()";
                    if (res.empty()) { emit(out, indent, call2 + ";"); return; }
                    stringVars_.insert(res);
                    // NOT decl(res, call2): decl()'s type heuristics only inspect the RHS text
                    // (looksString/isFloatVal/isBoolVal) — a method-call expression matches none
                    // of those, so it fell through to the `long` default despite `res` being
                    // freshly marked in stringVars_ one line above (that set is consulted by
                    // isStringVar() elsewhere, never by decl() for the var it's DECLARING).
                    // Declare directly, same as the glBoolFuncs branch above does for booleans.
                    if (declared.insert(res).second)
                        emit(out, indent, "String " + res + " = " + call2 + ";");
                    else
                        emit(out, indent, res + " = " + call2 + ";");
                    return;
                }
            }
        }
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
            // regex's `class regex {...}` dispatcher (see regex_ffi.java) declares `match`/`test`
            // as real `boolean` — same gap as gl's own bool functions above (verified:
            // examples/regex_demo.ac's `regex.match(...)`, "boolean cannot be converted to long").
            "regex.match", "regex.test",
            // machine-audio's `class maudio {...}` dispatcher (machine-audio_ffi.java) declares
            // tts_ok/stt_ok as real `boolean` too — same gap (verified: audio_test.ac's
            // `tts_ready = maudio.tts_ok()`, "incomparable types: long and boolean").
            "maudio.tts_ok", "maudio.stt_ok", "maudio.speak",
        };
        // native-cpu's carried-over pointer functions are called BARE (ptr_new, not
        // ncpu.ptr_new) — but Java has no free functions, so an unqualified call can't
        // resolve to AcNcpu's static methods without this rewrite (same shape as gl's
        // ac_gl_* → AcGl.* map above). The dotted ncpu.* surface doesn't need this: it
        // already resolves through the injected `class ncpu { ... }` receiver directly.
        static const std::unordered_map<std::string,std::string> ncpuMap = {
            {"ptr_new",     "AcNcpu.ptrNew"},
            {"ptr_deref",   "AcNcpu.ptrDeref"},
            {"ptr_null",    "AcNcpu.ptrNull"},
            {"ptr_is_null", "AcNcpu.ptrIsNull"},
            {"ptr_eq",      "AcNcpu.ptrEq"},
            {"ptr_copy",    "AcNcpu.ptrCopy"},
            {"ptr_update",  "AcNcpu.ptrUpdate"},
            {"ptr_free",    "AcNcpu.ptrFree"},
        };
        std::string actualFunc = func;
        auto git = glMap.find(func);
        if (git != glMap.end()) actualFunc = git->second;
        else { auto nit = ncpuMap.find(func); if (nit != ncpuMap.end()) actualFunc = nit->second; }

        // `math.mod_int` can also be called DIRECTLY (not just reached via the `math.mod`
        // dispatch below) — its signature is `mod_int(long, long)`, so an argument that's
        // float-typed on the CALLER side (e.g. a param the callee body treats as float — see
        // floatParams_'s own comment) needs an explicit `(long)` cast (verified: examples/
        // math_number.ac calls `math.mod_int(n, 2)` directly with `n` inferred double —
        // "cannot find symbol: method mod_int(double,long)"). Same fix as RustStrategy's.
        std::string castArgs = args;
        if (actualFunc == "math.mod_int") {
            std::vector<std::string> margs; { std::string cur; int depth = 0;
                for (char c : args) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { margs.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) margs.push_back(cur); }
            castArgs.clear();
            for (size_t k = 0; k < margs.size(); k++) {
                if (k) castArgs += ", ";
                castArgs += "(long)(" + margs[k] + ")";
            }
        }
        // AcGl's real FFI signature (gl_ffi.java) narrows most numeric params to `float`/`int`/
        // `byte` (SysV/Panama-FFI-friendly widths), but AC's own values are always long/double —
        // Java doesn't implicitly narrow ANY of double->float, long->int, or long->byte (unlike
        // the widening conversions Java DOES allow), so a bare AC numeric literal/var passed
        // straight through is a compile error at nearly every gl call with a non-String,
        // non-boolean param (verified: gd.ac/geodeo.ac/gl_bounce.ac/pong.ac — "possible lossy
        // conversion from double to float" / "from long to int", one call site at a time until
        // this table). Per-position type chars: s=String/no-op, f=float, i=int, b=byte.
        static const std::unordered_map<std::string,std::string> acGlArgTypes = {
            {"AcGl.screenCreate","iis"}, {"AcGl.screenSetBg","bbb"}, {"AcGl.screenSetFps","i"},
            {"AcGl.objGeometry","sii"}, {"AcGl.objSquare","si"}, {"AcGl.objPos","sii"},
            {"AcGl.objColor","sbbb"}, {"AcGl.objVelocity","sff"}, {"AcGl.objSetSpeed","sf"},
            {"AcGl.objSetDirection","sf"}, {"AcGl.objSpeedMult","sf"},
            {"AcGl.objMoveX","si"}, {"AcGl.objMoveY","si"}, {"AcGl.objVertex","sff"},
            {"AcGl.objCircleFall","sfs"}, {"AcGl.objCircleFell","sfs"}, {"AcGl.objAnimate","ssf"},
            {"AcGl.drawVertex","sff"}, {"AcGl.drawLine","sffffbbb"}, {"AcGl.drawCircle","sfffbbb"},
            {"AcGl.frameUpdate","f"}, {"AcGl.screenInit","iis"},
        };
        auto atIt = acGlArgTypes.find(actualFunc);
        if (atIt != acGlArgTypes.end()) {
            const std::string &types = atIt->second;
            std::vector<std::string> gargs; { std::string cur; int depth = 0;
                for (char c : castArgs) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { gargs.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) gargs.push_back(cur); }
            castArgs.clear();
            for (size_t k = 0; k < gargs.size(); k++) {
                if (k) castArgs += ", ";
                char t = k < types.size() ? types[k] : 's';
                if (t == 'f')      castArgs += "(float)(" + gargs[k] + ")";
                else if (t == 'i') castArgs += "(int)(" + gargs[k] + ")";
                else if (t == 'b') castArgs += "(byte)(" + gargs[k] + ")";
                else                castArgs += gargs[k];
            }
        }
        std::string call = actualFunc + "(" + castArgs + ")";
        if (glBoolFuncs.count(actualFunc)) {
            if (res.empty()) {
                emit(out, indent, call + ";");
            } else {
                boolVars.insert(res);
                if (declared.insert(res).second)
                    emit(out, indent, "boolean " + res + " = " + call + ";");
                else
                    emit(out, indent, res + " = " + call + ";");
            }
            return;
        }
        // `math.mod` is TYPE-PRESERVING (int args -> int result, matching PY's dynamic behavior
        // and Rust's own fix for the identical bug — see RustStrategy's emitCall comment on
        // this) — isFloatReturningFunc's blanket "math."-prefix rule below can't know that
        // (isIntReturningMathFunc only excludes `mod_int`, not bare `mod`), so a call with two
        // plain long args still got its result declared `double`, then failed passing that
        // wrongly-double value into a callee expecting `long` (verified: examples/
        // gcd_recursive.ac's `gcd(b, math.mod(a, b))` — "possible lossy conversion from double
        // to long"). Java's injected `math` class already overloads `mod(long,long)` AND
        // `mod(double,double)` — the dispatch itself was always correct; only this result-type
        // DECLARATION needed to actually check the args instead of assuming float.
        bool modIsInt = false;
        if (actualFunc == "math.mod") {
            std::vector<std::string> margs; { std::string cur; int depth = 0;
                for (char c : args) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { margs.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) margs.push_back(cur); }
            modIsInt = true;
            for (auto& a : margs) {
                std::string t = a; size_t s = t.find_first_not_of(' '), e = t.find_last_not_of(' ');
                if (s != std::string::npos) t = t.substr(s, e - s + 1);
                if (isFloatVal(t)) { modIsInt = false; break; }
            }
        }
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if ((isAcStrFunc(actualFunc) || userStringFuncs_.count(actualFunc)) && declared.insert(res).second) {
            emit(out, indent, "String " + res + " = " + call + ";");
        } else if (isListReturningFunc(actualFunc) && declared.insert(res).second) {
            listVars.insert(res);
            emit(out, indent, "java.util.ArrayList<Long> " + res + " = " + call + ";");
        } else if (modIsInt) {
            emit(out, indent, decl(res, call));
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
        if (val.empty()) return;
        // AC functions can genuinely mix return types across branches (dynamic typing —
        // `is_leap` returns a string on most paths, bare `0` on the fallthrough one); the
        // WHOLE-FUNCTION inference picks ONE Java return type (String, here, since most paths
        // are strings) — same fix as RustStrategy's emitReturn (see its comment). A numeric/
        // non-string branch's return value needs coercing via String.valueOf() (verified:
        // examples/leap_year.ac's trailing `return 0` — "incompatible types: long cannot be
        // converted to String").
        bool needsStringify = curFuncReturnIsString_ && !looksString(val) && !isStringVar(val);
        std::string v = needsStringify ? "String.valueOf(" + val + ")" : val;
        emit(out, indent, "return " + v + ";");
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // `null`/`nil` both map to the literal `null` (formatRef's nullVal/nilVal) — a bare
        // `System.out.println(null)` is a hard compile error ("reference to println is
        // ambiguous": println(char[]) and println(String) both match a literal null with no way
        // to pick). Cast disambiguates to the String overload, which prints "null" as text —
        // matches every other backend's null/nil text representation.
        if (val == "null") emit(out, indent, "System.out.println((String) null);");
        else emit(out, indent, "System.out.println(" + val + ");");
    }
    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res`, crashing `result = sure $x$` with "cannot find symbol" on this backend too.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        emit(out, indent, "System.out.println(" + val + ");");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
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
            emit(out, indent, "double " + res + " = _AcEval.eval(" + expr + ");");
        } else {
            emit(out, indent, res + " = _AcEval.eval(" + expr + ");");
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
        // `depth` names scopes by NESTING level, not occurrence — two SIBLING (sequential, not
        // nested) loops saving the same-named var both land on "depth 0" and collide: a second
        // `var _ac_s0_n = n;` is a hard "variable already defined" error (Java's block-scoped
        // `var` disallows redeclaration, unlike JS's function-scoped `var`). Same fix as
        // CStrategy's emitScopeEnter (see its comment) — reuse the slot on a second occurrence.
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars) {
            bool isNew = declared.insert(pfx + v).second;
            emit(out, indent, (isNew ? "var " : "") + pfx + v + " = " + v + ";");
        }
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
        // Java requires boolean; AC stores MOST comparisons as long (0/1), needing != 0 to become
        // a real boolean condition — but a glBoolFuncs-typed var (see its own comment) already
        // IS a real Java boolean, and `someBoolVar != 0` is itself a Java type error
        // ("incomparable types: boolean and int") — verified: gd.ac's `if (is_obj(arg))`.
        if (isBoolVal(cond)) { emit(out, indent, "if (" + cond + ") {"); indent++; return; }
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
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (seqOf_.count(collection)) {
            auto& [a, b] = seqOf_[collection];
            emit(out, indent, "for (long " + iterVar + " = (" + a + "); " + iterVar + " < (" + b + "); ++" + iterVar + ") {");
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            indent++;
            return;
        }
        if (isStringVar(collection)) {
            // Java strings aren't for-each iterable; walk the chars, wrap each as a 1-char String.
            std::string raw = iterVar + "__ch";
            emit(out, indent, "for (char " + raw + " : " + collection + ".toCharArray()) {");
            indent++;
            emit(out, indent, "String " + iterVar + " = String.valueOf(" + raw + ");");
            declared.insert(iterVar); forVarStack_.push_back(iterVar);
            return;
        }
        emit(out, indent, "for (long " + iterVar + " : " + collection + ") {");
        indent++;
    }
    void emitForEnd(std::ostringstream &out, int &indent) override
    {
        if (!forVarStack_.empty()) { declared.erase(forVarStack_.back()); forVarStack_.pop_back(); }
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
            dictVars_.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (!numeric) dictStrVals_.insert(var);
            std::string vt = numeric ? "Long" : "String";
            emit(out, indent, "java.util.Map<String," + vt + "> " + var + " = new java.util.HashMap<>();");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + ".put(" + fmtDictKey(k) + ", " + (numeric ? v + "L" : fmtDictValStr(v)) + ");");
            declared.insert(var);
        } else {
            auto dElems = splitCommaTrimmed(content);
            bool allDicts = !dElems.empty();
            for (auto& e : dElems) if (!dictVars_.count(e)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = dictStrVals_.count(dElems[0]) > 0;
                if (strVal) listOfDictStrVals_.insert(var);
                std::string vt = strVal ? "String" : "Long";
                std::string listOf;
                for (size_t k = 0; k < dElems.size(); k++) { if (k) listOf += ", "; listOf += dElems[k]; }
                emit(out, indent, "java.util.ArrayList<java.util.Map<String," + vt + ">> " + var
                    + " = new java.util.ArrayList<>(java.util.Arrays.asList(" + listOf + "));");
                declared.insert(var);
                return;
            }
            listVars.insert(var); declared.insert(var);
            std::string elems;
            {   std::istringstream iss(content); std::string tok; bool first = true;
                while (std::getline(iss, tok, ',')) {
                    size_t a = tok.find_first_not_of(' '), b = tok.find_last_not_of(' ');
                    std::string e = (a == std::string::npos) ? "" : tok.substr(a, b - a + 1);
                    if (e.empty()) continue;
                    if (!first) elems += ", ";
                    bool num = std::isdigit((unsigned char)e[0]) || (e[0]=='-' && e.size()>1);
                    elems += num ? e + "L" : e;
                    first = false;
                }
            }
            emit(out, indent, "java.util.ArrayList<Long> " + var + " = new java.util.ArrayList<>(java.util.Arrays.asList(" + (elems.empty() ? "new Long[0]" : elems) + "));");
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        if (dictVars_.count(arr)) {
            declared.insert(result);
            if (dictStrVals_.count(arr)) emit(out, indent, "String " + result + " = " + arr + ".get(" + idx + ");");
            else                         emit(out, indent, "long "   + result + " = " + arr + ".get(" + idx + ");");
            return;
        }
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            std::string vt = strVal ? "String" : "Long";
            declared.insert(result);
            emit(out, indent, "java.util.Map<String," + vt + "> " + result + " = "
                + arr + ".get((int)(" + idx + "));");
            return;
        }
        if (isStringVar(arr)) {
            declared.insert(result);
            emit(out, indent, "String " + result + " = String.valueOf(" + arr + ".charAt((int)(" + idx + ")));");
            return;
        }
        // #23: lists are ArrayList<Long> — .get() auto-unboxes to long
        emit(out, indent, decl(result, arr + ".get((int)(" + idx + "))"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        if (dictVars_.count(arr)) {
            emit(out, indent, arr + ".put(" + idx + ", " + (dictStrVals_.count(arr) ? val : "(long)(" + val + ")") + ");");
            return;
        }
        emit(out, indent, arr + ".set((int)(" + idx + "), (long)(" + val + "));");   // #23
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
        // Same boolean-vs-long gap as emitIfBegin (see its comment) — a glBoolFuncs-typed cond
        // is already a real Java boolean; `!= 0` on it is a type error, not just redundant.
        std::string test = isBoolVal(cond) ? cond : ("(" + cond + " != 0)");
        if (label == "__break__" || label == "\"__break__\"")
            emit(out, indent, "if (!" + test + ") break;");
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if (!" + test + ") continue;");
        else
            emit(out, indent, "// if (!(" + cond + ")) goto " + label);
    }

    void emitFunctionBegin(std::ostringstream &out, int &indent,
                           const std::string &name, const std::string &params,
                           const std::string &classOwner = "") override
    {
        declared.clear(); floatVars.clear(); boolVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are class-level fields
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
                if (isStringVar(pname)) {
                    tparams += "String " + pname;    // #6: inferred string param
                } else if (listParams_.count(pname)) {
                    tparams += "java.util.ArrayList<Long> " + pname;   // #23: shared reference
                    listVars.insert(pname);
                } else if (fit != funcTypedParams_.end()) {
                    // java.util.function: 1-arg → LongUnaryOperator, else LongFunction<Long>
                    tparams += (fit->second == 1 ? "java.util.function.LongUnaryOperator "
                                                 : "java.util.function.LongFunction<Long> ") + pname;
                } else if (floatParams_.count(pname)) {
                    // Same gap/fix as RustStrategy's floatParams_ (see its own comment) — a
                    // param the function's OWN body treats as float (`x / 2.0`) always
                    // defaulted to `long` here regardless, so calling it with a real double
                    // literal was "incompatible types: possible lossy conversion from double to
                    // long" (verified: examples/newton_sqrt.ac's `nsqrt(2.0)`).
                    floatVars.insert(pname);
                    tparams += "double " + pname;
                } else {
                    tparams += (stringParams_.count(pname) ? "String " : "long ") + pname;
                }
                pfirst = false;
            }
        }
        stringParams_.clear();
        funcTypedParams_.clear();
        std::string retT = returnIsList_ ? "java.util.ArrayList<Long>"
                         : baseReturnIsString_ ? "String"
                         : returnIsFloat_ ? "double"
                         : returnIsVoid_ ? "void" : "long";
        curFuncReturnIsString_ = baseReturnIsString_;
        returnIsList_ = false; baseReturnIsString_ = false;
        returnIsFloat_ = false; returnIsVoid_ = false;
        // A bundle method (classOwner non-empty, not the constructor) is a real INSTANCE method —
        // it needs `this` to reach fields (self.field), so it must not be `static` either. Only
        // genuinely free (non-method) functions get `static`.
        std::string sig = isConstructor
            ? classOwner + "(" + tparams + ")"
            : (classOwner.empty() ? "static " : "") + retT + " " + jName + "(" + tparams + ")";
        emit(out, indent, sig + " {");
        indent++;
        // #41: Java block-scopes declarations too — hoist cross-block locals to the top.
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "String " + v + " = \"\";";
            else if (t == IRType::LIST)   { listVars.insert(v); line = "java.util.ArrayList<Long> " + v + " = new java.util.ArrayList<>();"; }
            else                            line = "long " + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear(); boolVars.clear();
    }
    void emitIndirectCall(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &func,
                          const std::string &args) override
    {
        // The shared dispatcher's isIndirect check has no way to know a bare widget ctor name
        // (`display`, `btn`, ...) isn't a call through a function-typed parameter — see
        // widgetVarClass_'s header comment for the full story this closes.
        if (javaWidgetCtor(out, indent, res, func, args)) return;
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
        // `static` — bundles are instantiated from `main` (static context); a non-static
        // inner class would need an enclosing-instance reference Java can't synthesize there.
        emit(out, indent, "static class " + name + " {");
        indent++;
    }
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "String " : t == IRType::FLOAT ? "double " : "long ";
        emit(out, indent, ty + field + ";");
    }
    std::set<std::string> classInstanceVars_;   // vars known to hold a bundle instance (Java needs the real type, not `long`)
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        bool isNew = declared.insert(res).second;
        classInstanceVars_.insert(res);
        emit(out, indent, (isNew ? className + " " : "") + res + " = new " + className + "(" + args + ");");
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
        // #7 (Java): DECLARE on first sight; parse at the string↔number boundary.
        bool isNew = declared.insert(var).second;
        bool srcIsStr = (!src.empty() && src.front() == '"') || isStringVar(src);
        if (t == IRType::STRING) {
            std::string rhs = srcIsStr ? src
                : floatVars.count(src)
                    ? "((" + src + ") == (long)(" + src + ") ? String.valueOf((long)(" + src + ")) : String.valueOf(" + src + "))"
                    : "String.valueOf(" + src + ")";
            emit(out, indent, (isNew ? "String " : "") + var + " = " + rhs + ";");
        } else if (t == IRType::FLOAT || floatVars.count(var)) {
            if (isNew) floatVars.insert(var);
            std::string rhs = srcIsStr ? "Double.parseDouble(" + src + ")" : "(double)(" + src + ")";
            emit(out, indent, (isNew ? "double " : "") + var + " = " + rhs + ";");
        } else if (irIntWidth(t)) {
            // short/mini: fixed-width type from the type include. Java narrowing needs the (int)/(short) cast.
            std::string ty = acIntTypeJava(irIntWidth(t));   // int / short
            std::string inner = srcIsStr ? ("Long.parseLong(" + src + ")") : ("(" + src + ")");
            emit(out, indent, (isNew ? ty + " " : "") + var + " = (" + ty + ")" + inner + ";");
        } else if (t == IRType::ATOMIC) {
            // Declaration stays OUTSIDE the synchronized block — a Java local declared inside a
            // block is scoped to it, so later reads of `var` would be "cannot find symbol".
            std::string rhs = srcIsStr ? "Long.parseLong(" + src + ")" : "(long)(" + src + ")";
            if (isNew) emit(out, indent, "long " + var + " = 0;");
            emit(out, indent, "synchronized (_ac_atomic_lock) {");
            emit(out, indent + 1, var + " = " + rhs + ";");
            emit(out, indent, "}");
        } else {
            std::string rhs = srcIsStr ? "Long.parseLong(" + src + ")" : "(long)(" + src + ")";
            emit(out, indent, (isNew ? "long " : "") + var + " = " + rhs + ";");
        }
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear(); boolVars.clear();
        for (auto& v : promotedGlobals_) declared.insert(v); // free vars are class-level fields
        emit(out, indent, "public static void main(String[] args) {");
        indent++;
        // Same hoist fix as emitFunctionBegin (see CppStrategy's identical fix/comment) — the
        // mainloop is block-scoped Java code too, and never got this treatment at all before.
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "double " + v + " = 0;"; }
            else if (t == IRType::STRING)   line = "String " + v + " = \"\";";
            else if (t == IRType::LIST)   { listVars.insert(v); line = "java.util.ArrayList<Long> " + v + " = new java.util.ArrayList<>();"; }
            else                            line = "long " + v + " = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
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
    std::set<std::string> listVars;
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(v);   // #42: pre-inferred float locals
    }
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    std::map<std::string, int> userFuncArity_;
    bool lastWasReturn = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    std::set<std::string> promotedGlobals_;
    std::set<int> promotedGlobalSymIds_;
    void setPromotedGlobalSymIds(const std::set<int>& s) override { promotedGlobalSymIds_ = s; }
    // Vars known to hold a genuine native Rust `bool` (currently just the gl ilib's several
    // `-> bool` functions — see emitCall's glBoolFuncs). `emitIfBegin`'s default assumes an
    // int-typed condition and always appends `!= 0`; consulted there to skip that for these.
    std::set<std::string> rustBoolVars_;
    void setFuncTypedParams(const std::map<std::string, int>& m) override { funcTypedParams_ = m; }
    void setUserFuncArity(const std::map<std::string, int>& m) override { userFuncArity_ = m; }
    void setPromotedGlobals(const std::vector<std::string>& vars) override {
        for (const auto& v : vars) promotedGlobals_.insert(v);
    }
    std::set<std::string> floatParams_;
    void setFloatParams(const std::set<std::string>& s) override { floatParams_ = s; }
    std::map<std::string, std::set<int>> userFloatParamIdx_;
    void setUserFloatParams(const std::map<std::string, std::set<int>>& m) override { userFloatParamIdx_ = m; }
    std::set<std::string> stringParams_;
    void setStringParams(const std::set<std::string>& s) override { stringParams_ = s; }

    bool returnIsFloat_ = false;
    bool returnIsList_ = false;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> userListFuncs_;
    std::set<std::string> userStringFuncs_;   // #retstring: see emitFunctionBegin
    void setVarCastTypes(const std::map<std::string, IRType>& m) override { varCastTypes_ = m; }
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setReturnIsList(bool v) override { returnIsList_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
    bool isUserStringReturningFunc(const std::string& fn) const { return userStringFuncs_.count(fn) > 0; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    bool isListReturningFunc(const std::string& fn) const { return userListFuncs_.count(fn) > 0; }
    // Vec<i64> isn't Copy: passing a list var by value moves it, so reusing it after the
    // call is a use-after-move (#22). Clone list args for value semantics (matches V's .clone()).
    std::string valueArgRef(const std::string &name, IRType t) override {
        return t == IRType::LIST ? name + ".clone()" : name;
    }
    IRType castDeclType(const std::string& var, IRType def) const {
        auto it = varCastTypes_.find(var); return it != varCastTypes_.end() ? it->second : def;
    }
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        // A plain `static mut String` needs `unsafe` at every touch and a print can happen
        // inside ANY function, not just main — same OnceLock<Mutex<...>> pattern already used
        // for `atomic`/events globals (see their comments) gives a safe global without that.
        std::string fmt = isFloatVal(val) ? "{:?}" : "{}";
        emit(out, indent, "_ac_save().push_str(&format!(\"" + fmt + "\\n\", " + val + "));");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "std::fs::write(" + filename + ", _ac_save().as_str()).ok();");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_ac_bind(" + key + ", " + callback + ");");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_ac_trigger(" + key + ");");
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
        // unconditional_panic: AC's `try`/`catch` intentionally lets a divide-by-zero happen and
        // catches it at runtime — when AC's own constant folding can prove a division is by a
        // literal 0 (e.g. `5 // is_prime(4)` where is_prime folded to 0), rustc's default-deny
        // lint refuses to even COMPILE the file, even though the exact same code is reachable
        // and correctly caught at runtime. Found via `examples/showcase.ac`'s try/catch test.
        emitRaw(out, "#![allow(unused_variables, unused_mut, unused_assignments, non_snake_case, unconditional_panic)]");
        emitRaw(out, "use std::io::Write;");
        // std::sync::OnceLock is stable (1.70+, no external crate) — shared by `atomic` vars
        // AND the event-bind table below (both need a lazily-initialized global), so the
        // import must cover either needing it alone, not just anyAtomicVars().
        if (anyAtomicVars() || needsEvents_ || needsSave_)
            emitRaw(out, "use std::sync::{Mutex, OnceLock};");
        if (needsSave_) {
            emitRaw(out, "static _AC_SAVED: OnceLock<Mutex<String>> = OnceLock::new();");
            emitRaw(out, "fn _ac_save() -> std::sync::MutexGuard<'static, String> { _AC_SAVED.get_or_init(|| Mutex::new(String::new())).lock().unwrap() }");
        }
        if (anyAtomicVars()) {
            // `atomic` vars: any op touching one is a global critical section.
            emitRaw(out, "static _AC_ATOMIC_LOCK: OnceLock<Mutex<()>> = OnceLock::new();");
            emitRaw(out, "fn _ac_atomic_lock() -> &'static Mutex<()> { _AC_ATOMIC_LOCK.get_or_init(|| Mutex::new(())) }");
        }
        if (needsEvents_) {
            emitRaw(out, "use std::collections::HashMap;");
            emitRaw(out, "static _AC_EVENTS: OnceLock<Mutex<HashMap<String, fn()>>> = OnceLock::new();");
            emitRaw(out, "fn _ac_events() -> &'static Mutex<HashMap<String, fn()>> { _AC_EVENTS.get_or_init(|| Mutex::new(HashMap::new())) }");
            emitRaw(out, "fn _ac_bind(key: &str, f: fn()) { _ac_events().lock().unwrap().insert(key.to_string(), f); }");
            emitRaw(out, "fn _ac_trigger(key: &str) { let f = _ac_events().lock().unwrap().get(key).copied(); if let Some(f) = f { f(); } }");
        }
        for (const auto& v : promotedGlobals_)
            emitRaw(out, "static mut " + promotedGlobalName(v) + ": i64 = 0;");
        if (!promotedGlobals_.empty()) emitRaw(out, "");
        emitRaw(out, "fn ac_iota(n: i64) -> String { (0..n).map(|i| i.to_string()).collect() }");
        emitRaw(out, "fn ac_ipow(b: i64, e: i64) -> i64 { let mut r: i64 = 1; let mut k = e; while k > 0 { r *= b; k -= 1; } r }");
        emitRaw(out, "fn ac_rand(n: i64) -> i64 { if n <= 0 { return 0; } let t = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().subsec_nanos() as u64; let mut x = t | 1; x ^= x << 13; x ^= x >> 7; x ^= x << 17; (x % (n as u64)) as i64 }");
        emitRaw(out, "fn ac_choice(xs: Vec<i64>) -> i64 { xs[ac_rand(xs.len() as i64) as usize] }");
        emitRaw(out, "fn ac_stream(a: i64, b: i64, s: i64) -> String { let st = if s==0 {1} else {s}; let mut r=String::new(); let mut i=a; while (st>0 && i<b)||(st<0 && i>b) { r.push_str(&i.to_string()); i+=st; } r }");
        // Two or more ilib FFI files, concatenated verbatim, commonly `use` the same std
        // items (verified: gl_ffi.rs's `use std::ffi::{CString, c_char};` and math_ffi.rs's
        // plain `use std::ffi::CString;` — same path, same item, different SYNTACTIC FORM —
        // plus gl_ffi.rs/math_ffi.rs's overlapping-but-not-identical `use std::os::raw::{...}`
        // lists. rustc: "`CString` must be defined only once" / "`c_int` reimported here").
        // Every ilib FFI file is independent, self-contained source — none of them can know
        // what a DIFFERENT ilib the same program happens to also import will pull in — so
        // merge at the concatenation point instead of per-file. Both the plain `use path::Item;`
        // and braced `use path::{a, b};` forms are normalized into the SAME (path, items) model
        // (a plain single-item use is just a braced use with one item) and merged by path, so a
        // plain/braced collision on the identical item (this exact case) is caught too, not just
        // an exact braced-vs-braced overlap.
        std::vector<std::string> bodyLines;              // non-`use` lines, in order
        std::vector<std::string> usePaths;                // unique `use path::` prefixes, in order
        std::map<std::string, std::vector<std::string>> useItems; // path -> ordered unique items
        // `std::ffi::c_char` and `std::os::raw::c_char` are the SAME type re-exported under two
        // different paths (verified: gl_ffi.rs imports it via std::ffi, math_ffi.rs via
        // std::os::raw) — a per-path merge alone still collides on the bare name across paths
        // ("`c_char` is defined multiple times"), so also track every bare item name globally
        // and skip it under a SECOND path once any path has already claimed it.
        std::set<std::string> globalImportedNames;
        auto collectFfiLines = [&](const std::string& ffi) {
            std::istringstream ffiStream(ffi);
            std::string ffiLine;
            while (std::getline(ffiStream, ffiLine)) {
                // Only a TOP-LEVEL `use` (column 0, no leading whitespace) is a real module
                // import worth merging — an INDENTED `use` is scoped inside a function body
                // (verified: os_ffi.rs has one inside a fn, for a std::io::Write call it makes
                // locally) and must stay exactly where it is; treating it as mergeable would
                // hoist it out of its function into the global import list, which is simply
                // wrong regardless of any dedup logic (verified: os_demo.ac — the hoisted
                // `use std::io::Write;` collided with the always-present base-preamble one,
                // "`Write` is defined multiple times", a REGRESSION this exact fix caused).
                if (ffiLine.rfind("use ", 0) != 0) {
                    bodyLines.push_back(ffiLine);
                    continue;
                }
                std::string rest = ffiLine.substr(4); // after "use "
                if (rest.empty() || rest.back() != ';') { bodyLines.push_back(ffiLine); continue; }
                rest.pop_back(); // drop trailing ';'
                std::string path, itemsStr;
                auto brace = rest.find('{');
                if (brace != std::string::npos && rest.back() == '}') {
                    path = rest.substr(0, brace);
                    itemsStr = rest.substr(brace + 1, rest.size() - brace - 2); // strip "{...}"
                } else {
                    // Plain `use a::b::Item;` — split at the LAST "::" so path="a::b::",
                    // item="Item", same shape a braced single-item use would normalize to.
                    auto lastSep = rest.rfind("::");
                    if (lastSep == std::string::npos) { bodyLines.push_back(ffiLine); continue; }
                    path = rest.substr(0, lastSep + 2);
                    itemsStr = rest.substr(lastSep + 2);
                }
                if (std::find(usePaths.begin(), usePaths.end(), path) == usePaths.end())
                    usePaths.push_back(path);
                auto& items = useItems[path];
                std::string cur;
                std::istringstream itemStream(itemsStr);
                while (std::getline(itemStream, cur, ',')) {
                    size_t is = cur.find_first_not_of(" \t"), ie = cur.find_last_not_of(" \t");
                    if (is == std::string::npos) continue;
                    std::string item = cur.substr(is, ie - is + 1);
                    if (globalImportedNames.insert(item).second)
                        items.push_back(item);
                }
            }
        };
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib") {
                std::string ffi = readFFIFile(ln, "rs");
                if (!ffi.empty()) collectFfiLines(ffi);
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
        if (!usePaths.empty() || !bodyLines.empty()) {
            emitRaw(out, "");
            for (auto& path : usePaths) {
                auto& items = useItems[path];
                // Every item under this path may have already been claimed by a DIFFERENT path
                // via globalImportedNames (the c_char case) — nothing left to import here.
                if (items.empty()) continue;
                // A single-item merged path emits as plain `use path::Item;` (matches how it
                // would have looked if only one ilib had ever imported it); 2+ items use the
                // braced form — either is valid Rust, this just keeps single imports tidy.
                if (items.size() == 1) emitRaw(out, "use " + path + items[0] + ";");
                else {
                    std::string joined;
                    for (size_t k = 0; k < items.size(); k++) { if (k) joined += ", "; joined += items[k]; }
                    emitRaw(out, "use " + path + "{" + joined + "};");
                }
            }
            for (auto& b : bodyLines) out << b << "\n";
        }
        emitRaw(out, "");
    }

    bool dotCallSyntax() const override { return true; }

    // Rust reserves the identifier `self` for an actual `self`/`&self`/`&mut self` PARAMETER —
    // `let mut self = ...;` is a hard compile error ("expected value, found module `self`").
    // Regular methods get a real `&mut self` param, so `self.field` is valid there as-is. The
    // constructor (`new()`) has no such param (see emitFunctionBegin) and instead binds a local
    // called `_ac_self` — every `self.field` reference inside ITS body must use that name
    // instead, everywhere else must stay literally `self.field`.
    std::string selfLocal_(const std::string& var) const {
        return curFuncIsConstructor_ ? ("_ac_self." + var.substr(5)) : var;
    }
    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        std::string s = commonRef(r, sym, "true", "false", "None", "None");
        if (s.rfind("self.", 0) == 0) return selfLocal_(s);
        // AC identifiers are unreserved (no keyword list of its own), but a plain AC variable
        // NAME can collide with one of Rust's ~50 reserved words — `box` is a real one
        // (verified: widgets_test2.ac names a widget var "box", producing "expected pattern,
        // found `:`" — Rust parsed `let mut box: i64 = ...` as an attempt at the (unstable)
        // `box` placement-new syntax, not a plain `let` binding). Every other backend's target
        // language either doesn't reserve these words or already gets its own escaping
        // elsewhere; only VAR-kind refs need this (CONST text is quoted, TEMP/LABEL use a
        // "t_"/"L"-prefixed synthetic name, neither can collide).
        if (r.kind == IRRef::Kind::VAR && isRustKeyword(s)) return s + "_";
        // A promoted (NA->free) global is declared as a bare `static mut NAME` — but Rust
        // hard-errors ("E0530: let bindings cannot shadow statics") if ANY OTHER function
        // anywhere in the file declares a plain local with the exact same name, even though
        // that local has nothing to do with the promoted one (verified real regression:
        // keyword_catalog_core.ac's mainloop-scope `total`, genuinely promoted since a loop
        // there needs it, collided with a completely unrelated `bound total` local inside a
        // DIFFERENT function that merely happens to share the name). Prefixing every reference
        // to a promoted global keeps it in a name never chosen by plain AC source identifiers.
        // Symbol-ID gated, not just name-text — the DETECTION phase (which NAMES end up in
        // promotedGlobals_ at all) was already fixed this way (see globalVarSymIds_'s comment),
        // but this APPLICATION phase (which SPECIFIC references get the prefix) was still purely
        // name-based, so an unrelated local var sharing a genuinely-promoted global's name got
        // the prefix applied to IT too, inside its own unrelated function — turning "two
        // different locals, same spelling" into "one prefixed name declared twice" instead of
        // fixing the collision (verified real regression: sum_evens.ac/vowel_count.ac's `s`/`n`
        // locals, unrelated to the actual promoted mainloop var of the same name).
        if (r.kind == IRRef::Kind::VAR && promotedGlobals_.count(s)
            && (r.id < 0 || promotedGlobalSymIds_.count(r.id)))
            return promotedGlobalName(s);
        return s;
    }
    static std::string promotedGlobalName(const std::string &v) { return "_AC_FREE_" + v; }
    static bool isRustKeyword(const std::string &s) {
        static const std::set<std::string> kw = {
            "as","break","const","continue","crate","dyn","else","enum","extern","false","fn",
            "for","if","impl","in","let","loop","match","mod","move","mut","pub","ref","return",
            "self","Self","static","struct","super","trait","true","type","unsafe","use","where",
            "while","async","await","abstract","become","box","do","final","macro","override",
            "priv","typeof","unsized","virtual","yield","try",
        };
        return kw.count(s) > 0;
    }

    std::string decl(const std::string &var, const std::string &val)
    {
        // Bundle field write (self.field = ...): Rust structs already use plain `self.field`
        // for both reads and writes (no ->/this translation needed, unlike C++/Java) — the
        // ONLY fix needed is to skip the `let mut TYPE:` declaration wrapper, since `self` is a
        // real `Self`-typed local (see emitFunctionBegin's constructor prologue) and the field
        // already exists as a real struct member (see emitFieldDecl) — `let self.hp = ...` is
        // not valid Rust (can't `let`-declare a field path).
        if (var.rfind("self.", 0) == 0) {
            std::string target = selfLocal_(var);
            if (isStringVar(var) && looksString(val)) return target + " = " + val + ".to_string();";
            return target + " = " + val + ";";
        }
        if (var.rfind("_ac_self.", 0) == 0) {   // already translated (came in via formatRef on a READ)
            std::string checkVar = "self." + var.substr(9);
            if (isStringVar(checkVar) && looksString(val)) return var + " = " + val + ".to_string();";
            return var + " = " + val + ";";
        }
        // `var` arrives here ALREADY prefixed (formatRef's own promotedGlobals_ check ran
        // before this, via the shared dispatcher's ref() call) — match the prefix, not the
        // bare name, which promotedGlobals_ itself still stores.
        if (var.rfind("_AC_FREE_", 0) == 0) {
            return "unsafe { " + var + " = " + val + "; }";
        }
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "let mut " + var + ": f64 = " + (looksFloat(val) ? val : val + " as f64") + ";"; }
            if (ct == IRType::STRING || isStringVar(var) || isStringVar(val)) {
                stringVars_.insert(var);
                std::string rhs = looksString(val) ? val + ".to_string()" : val + ".clone()";
                return "let mut " + var + ": String = " + rhs + ";";
            }
            // Propagate a genuine native-bool value (see rustBoolVars_'s comment) through a
            // plain copy-through assignment — `tts_ready = t_0` where `t_0` was declared `bool`
            // (a glBoolFuncs call result) otherwise defaulted `tts_ready` to `i64`, and every
            // LATER comparison against it (`tts_ready == true`) inherited the same mismatch
            // (verified: examples/audio_test.ac's `tts_ready = maudio.tts_ok()`).
            if (rustBoolVars_.count(val)) { rustBoolVars_.insert(var); return "let mut " + var + ": bool = " + val + ";"; }
            // Same plain-copy propagation as rustBoolVars_ above, for dicts (`p1 = pets[1]` —
            // see listOfDictVars_'s comment for the full verified failure across every
            // statically-typed backend, this one included).
            if (dictVars_.count(val)) {
                dictVars_.insert(var);
                bool strVal = dictStrVals_.count(val) > 0;
                if (strVal) dictStrVals_.insert(var);
                std::string vt = strVal ? "String" : "i64";
                return "let mut " + var + ": std::collections::HashMap<String," + vt + "> = " + val + ".clone();";
            }
            if (ct == IRType::INT || ct == IRType::BOOL) return "let mut " + var + ": i64 = " + val + " as i64;";
            if (looksString(val)) return "let mut " + var + ": &str = " + val + ";";
            if (isFloatVal(val)) { floatVars.insert(var); return "let mut " + var + ": f64 = " + val + ";"; }
            return "let mut " + var + ": i64 = " + val + ";";
        }
        // A reassignment where `var`'s already-committed static type (f64 vs i64, tracked via
        // floatVars) disagrees with `val`'s: AC lets a variable flip between int/float
        // dynamically across statements (e.g. `n = n / 2` then `n = math.to_int(n)` in the same
        // loop — see #floatret/#floatparam), but Rust needs ONE fixed type per `let mut` binding
        // — cast the incoming value to whichever type `var` already has instead of emitting a
        // bare assignment (verified: examples/math_number.ac's `collatz`, `n = t_4;` where
        // `n: f64` but `t_4` (math.to_int's result) is i64).
        if (floatVars.count(var) && !isFloatVal(val)) return var + " = (" + val + ") as f64;";
        if (!floatVars.count(var) && isFloatVal(val) && !isStringVar(var)) return var + " = (" + val + ") as i64;";
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
    { emit(out, indent, decl(res, "(((" + lhs + " as i64) % (" + rhs + " as i64)) + (" + rhs + " as i64)) % (" + rhs + " as i64)")); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as decl(): real struct field, no `let`-redeclaration.
        if (var.rfind("self.", 0) == 0 || var.rfind("_ac_self.", 0) == 0) { emit(out, indent, decl(var, val)); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        // Declaration stays OUTSIDE the lock's `{ }` scope — Rust's definite-assignment analysis
        // is fine with `let mut x: i64; { x = 5; }`, and `var` needs to outlive that block anyway.
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            if (declared.insert(var).second) emit(out, indent, "let mut " + var + ": i64;");
            emit(out, indent, "{ let _ac_lg = _ac_atomic_lock().lock().unwrap(); " + var + " = (" + val + ") as i64; }");
            return;
        }
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : t);
            if (isStringVar(var)) {   // #retype: unify to String, coerce non-strings
                emit(out, indent, "let mut " + var + ": String = (" + val + ").to_string();"); return; }
            if (looksString(val)) {   // #retype numeric-unified decl ← stringified number
                std::string bare = acUnstring(val);
                std::string ty = (bare.find('.') != std::string::npos) ? ": f64" : ": i64";
                emit(out, indent, "let mut " + var + ty + " = " + bare + ";"); return; }
            if      (declType == IRType::FLOAT)  { floatVars.insert(var); emit(out, indent, "let mut " + var + ": f64 = " + (looksFloat(val) ? val : val + " as f64") + ";"); }
            else if (declType == IRType::STRING)  emit(out, indent, "let mut " + var + ": String = " + val + ".to_string();");
            else if (declType == IRType::BOOL)    emit(out, indent, "let mut " + var + ": bool = " + val + " != 0;");
            else if (declType == IRType::INT)     emit(out, indent, "let mut " + var + ": i64 = " + val + " as i64;");
            else { declared.erase(var); emit(out, indent, decl(var, val)); }
        } else if (isStringVar(var)) {
            emit(out, indent, var + " = (" + val + ").to_string();");   // #retype reassign
        } else if (looksString(val)) {   // #retype numeric-unified reassign ← stringified number
            emit(out, indent, var + " = " + acUnstring(val) + ";");
        } else if (floatVars.count(var) && !isFloatVal(val)) {
            // Same reconciliation as decl()'s equivalent branch (see its comment): `var`'s
            // already-committed f64 type disagrees with an int-typed `val` on reassignment.
            emit(out, indent, var + " = (" + val + ") as f64;");
        } else if (!floatVars.count(var) && isFloatVal(val) && !isStringVar(var)) {
            emit(out, indent, var + " = (" + val + ") as i64;");
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // Rust string concat: `String + String` is illegal (only String + &str). format! handles
        // every combination (String/&str/literal) cleanly. #6.
        // Checking isStringVar(RES) alone missed the common case: `res` is a freshly-minted
        // TEMP (e.g. `t_6`), which the string pre-scan has no reason to have registered yet —
        // only the OPERANDS (`label`, a real declared string var) carry the "this is a string
        // concat" signal. `label + "!"` (examples/keyword_catalog_core.ac) was declaring `t_6`
        // as `i64` and trying to add a `String` to it — a hard type error.
        if (op == "+" && (isStringVar(res) || looksString(lhs) || isStringVar(lhs) || looksString(rhs) || isStringVar(rhs))) {
            std::string expr = "format!(\"{}{}\", " + lhs + ", " + rhs + ")";
            bool isNew = declared.insert(res).second;
            stringVars_.insert(res);
            emit(out, indent, isNew ? "let mut " + res + ": String = " + expr + ";"
                                    : res + " = " + expr + ";");
            return;
        }
        bool lhsFloat = isFloatVal(lhs), rhsFloat = isFloatVal(rhs);
        bool isFloat = lhsFloat || rhsFloat;
        // In Rust, mixing integer and float types in arithmetic is a compile error.
        // When the result is float, ensure both operands are f64.
        std::string elhs = (isFloat && !lhsFloat) ? toRustF64(lhs) : lhs;
        std::string erhs = (isFloat && !rhsFloat) ? toRustF64(rhs) : rhs;
        std::string expr = elhs + " " + op + " " + erhs;
        bool isNew = declared.insert(res).second;
        IRType rct = castDeclType(res, IRType::VOID);   // short/mini result → declare at that width
        if (isFloat)
        {
            if (isNew) { floatVars.insert(res); emit(out, indent, "let mut " + res + ": f64 = " + expr + ";"); }
            else       emit(out, indent, res + " = " + expr + ";");
        }
        else if (irIntWidth(rct))
        {
            // a `short`/`mini` result: declare it i32/i16 and coerce the expression to that width so
            // it stores back into the narrow var cleanly (Rust rejects i32-into-i16 etc.).
            std::string ty = acIntTypeRs(irIntWidth(rct));
            std::string rhs = "(" + expr + ") as " + ty;
            emit(out, indent, isNew ? "let mut " + res + ": " + ty + " = " + rhs + ";"
                                    : res + " = " + rhs + ";");
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
            expr = rustBoolVars_.count(lhs) ? "(!" + lhs + ") as i64" : "(" + lhs + " == 0) as i64";
        else if ((op == "==" || op == "!=") && (rustBoolVars_.count(lhs) || rustBoolVars_.count(rhs))) {
            // A genuine native-bool var (see rustBoolVars_) compared against AC's usual `0`/`1`
            // int-literal truthiness convention — Rust's `==`/`!=` require matching types on
            // both sides, so `t_0 == 0` (t_0: bool) is a hard error. Recast the literal side to
            // its bool equivalent instead of falling through to the generic numeric path below.
            auto asBoolLit = [](const std::string &v) { return v == "0" ? "false" : v == "1" ? "true" : v; };
            std::string elhs = rustBoolVars_.count(lhs) ? lhs : asBoolLit(lhs);
            std::string erhs = rustBoolVars_.count(rhs) ? rhs : asBoolLit(rhs);
            expr = "(" + elhs + " " + op + " " + erhs + ") as i64";
        }
        else {
            // Rust forbids f64-vs-integer comparisons — coerce the int side to f64
            // when exactly one side is float-typed (e.g. `t_1 == 0` where t_1: f64).
            bool lf = isFloatVal(lhs), rf = isFloatVal(rhs);
            std::string elhs = (rf && !lf) ? toRustF64(lhs) : lhs;
            std::string erhs = (lf && !rf) ? toRustF64(rhs) : rhs;
            // Cast bool → i64 via `as i64`
            expr = "(" + elhs + " " + op + " " + erhs + ") as i64";
        }
        emit(out, indent, decl(res, expr));
    }
    static bool isFloatReturningFunc(const std::string &fn) {
        if (isIntReturningMathFunc(fn.c_str())) return false;
        if (isMLFloatReturningFunc(fn)) return true;
        if (fn.rfind("math.", 0) == 0) return true;
        if (fn.rfind("stat_", 0) == 0) return true;
        return false;
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &funcIn, const std::string &args) override
    {
        // A method-call receiver (`box.pack()`) is extracted from the raw IR symbol name by the
        // shared dispatcher, bypassing formatRef entirely — so formatRef's own keyword-escaping
        // (see its comment) never got a chance to fire here, and a receiver var whose name
        // collides with a Rust keyword reached this raw (verified: "box.pack()" — "expected
        // expression, found `.`", Rust parsing `box` as its own reserved-word token even mid
        // method-call-chain, not as an identifier at all).
        std::string func = funcIn;
        { auto dot = func.find('.');
          if (dot != std::string::npos && isRustKeyword(func.substr(0, dot)))
              func = func.substr(0, dot) + "_" + func.substr(dot); }
        // Same collision, METHOD side: `regex.match(...)` — the FFI wrapper (regex_ffi.rs)
        // already escapes its OWN method as `match_` (mirrors formatRef's receiver-side
        // escaping), but nothing on the CALL-SITE side matched that convention — verified:
        // regex_demo.ac, "expected identifier, found keyword `match`".
        // EXCEPT "mod": `math.mod` has its OWN dedicated dispatch just below (renaming to the
        // FFI's real `mod_int`/`mod_f` names) that pattern-matches the exact text "math.mod" —
        // escaping it here first to "math.mod_" made that dispatch's `func == "math.mod"` check
        // silently stop matching, falling through to a literal (nonexistent) "math.mod_" call
        // instead (verified regression: collatz.ac and everything else using `math.mod`, "no
        // method named `mod_` found for struct `AcMath`").
        { auto dot = func.find('.');
          if (dot != std::string::npos && isRustKeyword(func.substr(dot + 1)) && func.substr(dot + 1) != "mod")
              func = func.substr(0, dot + 1) + func.substr(dot + 1) + "_"; }
        { auto dot = func.find('.');
          if (dot != std::string::npos) {
              std::string recv = func.substr(0, dot);
              if (promotedGlobals_.count(recv))
                  func = "(unsafe { " + promotedGlobalName(recv) + " })" + func.substr(dot);
          } }
        if (func == "ac_length" && !res.empty()) { emit(out, indent, decl(res, "(" + args + ").len() as i64")); return; }
        { auto sp = func.rfind(".set");
          if (sp != std::string::npos && sp == func.size() - 4 && (looksString(args) || isStringVar(args))) {
              emit(out, indent, func.substr(0, sp) + ".set_str(" + args + ");");
              return;
          } }
        { auto ap = func.rfind(".append");
          if (ap != std::string::npos && ap == func.size() - 7) {
              emit(out, indent, func.substr(0, ap) + ".push(" + args + ");"); return; } }
        // `dropdown(master, lazy)` — AC's calling convention never populates dropdown's item
        // list at construction (always via `.add()` after — see every widgets example); the
        // 2-arg form only ever means "master + the `lazy` sentinel". Rust's dropdown(master)
        // takes exactly 1 arg (no default-arg support in Rust to fall back on), so a 2-arg call
        // was a hard "this function takes 1 argument but 2 arguments were supplied" (verified:
        // applicant_form.ac). Drop the sentinel arg — Rust's widget wrapper doesn't implement
        // lazy packing at all (see its own dropdown()), so there's nothing to forward anyway.
        if (func == "dropdown") {
            std::string firstArg = args;
            auto comma = args.find(',');
            if (comma != std::string::npos) firstArg = args.substr(0, comma);
            std::string callD = "dropdown(" + firstArg + ")";
            emit(out, indent, res.empty() ? callD + ";" : decl(res, callD));
            return;
        }
        // `btn(master, text, cmd)` — Rust's wrapper hard-codes cmd's type as `fn(i64) -> i64`
        // (a plain, non-capturing fn pointer), but AC callback functions compile to whatever
        // arity the AC source itself declared (`Make OnSubmit func()` -> 0 args, `Make OnClick
        // func(arg)` -> 1 arg — both patterns exist across the widgets examples) — passing a
        // 0-arg fn where `fn(i64) -> i64` is expected is a hard type mismatch (verified:
        // "incorrect number of function parameters" on applicant_form.ac's `OnSubmit`). Wrap in
        // a non-capturing closure adapting whichever real arity to the fixed signature — such a
        // closure still coerces to a plain `fn(i64) -> i64` pointer, so btn's own signature
        // doesn't need to change at all.
        if (func == "btn") {
            std::vector<std::string> parts; { std::string cur; int depth = 0;
                for (char c : args) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { parts.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) parts.push_back(cur); }
            if (parts.size() == 3) {
                std::string cbName = parts[2];
                size_t a = cbName.find_first_not_of(' '), b = cbName.find_last_not_of(' ');
                if (a != std::string::npos) cbName = cbName.substr(a, b - a + 1);
                auto pit = userFuncArity_.find(cbName);
                int arity = (pit != userFuncArity_.end()) ? pit->second : 0;
                // Always discard the callback's own return value explicitly (`; 0`) rather than
                // using it as the closure's tail expression — a `/kill`-style AC callback with
                // no `return` compiles to a void `fn(i64)` in Rust (unlike C's "everything
                // returns i64" convention), so treating its call as the tail expression is a
                // type error whenever the real callback happens to be void (verified:
                // "expected i64, found ()" on widgets_test.ac's OnClick).
                std::string wrapped = (arity >= 1)
                    ? "|_x: i64| -> i64 { " + cbName + "(_x); 0 }"
                    : "|_x: i64| -> i64 { " + cbName + "(); 0 }";
                std::string callB = "btn(" + parts[0] + "," + parts[1] + ", " + wrapped + ")";
                emit(out, indent, res.empty() ? callB + ";" : decl(res, callB));
                return;
            }
        }
        std::string args2 = args;
        // #floatparam (call-site half): cast an argument at a position the callee's own body
        // treats as float (see userFloatParamIdx_'s comment) — a bare int literal or
        // int-typed caller var doesn't implicitly convert to f64 on Rust (verified:
        // examples/math_number.ac's `collatz(6)`/`collatz(27)` calling `collatz(n: f64)`).
        { auto fpit = userFloatParamIdx_.find(func);
          if (fpit != userFloatParamIdx_.end() && !fpit->second.empty()) {
              std::vector<std::string> parts; { std::string cur; int depth = 0;
                  for (char c : args) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                      if (c==',' && depth==0) { parts.push_back(cur); cur.clear(); } else cur += c; }
                  if (!cur.empty()) parts.push_back(cur); }
              for (int idx : fpit->second) {
                  if (idx < 0 || (size_t)idx >= parts.size()) continue;
                  std::string &p = parts[(size_t)idx];
                  size_t s = p.find_first_not_of(' '), e = p.find_last_not_of(' ');
                  std::string t = (s == std::string::npos) ? p : p.substr(s, e - s + 1);
                  if (!isFloatVal(t)) p = "(" + t + ") as f64";
              }
              args2.clear();
              for (size_t k = 0; k < parts.size(); k++) { if (k) args2 += ","; args2 += parts[k]; }
          } }
        // User functions with a string parameter always declare it `&str` in Rust (owned
        // `String` args never implicitly convert) — a caller passing an already-`String`-typed
        // local (as opposed to a literal, which is natively `&str`) needs an explicit `&`
        // borrow (verified: jarvis.ac's `resolve(low)` where `low` is a `String` local and
        // `fn resolve(s: &str)` — "expected `&str`, found `String`"). Scoped to known USER
        // functions (userFuncArity_) so this never touches ilib/gl calls, which have their own
        // argument-marshaling rules already handled elsewhere in this method.
        if (userFuncArity_.count(func)) {
            std::vector<std::string> parts; { std::string cur; int depth = 0;
                for (char c : args2) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { parts.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) parts.push_back(cur); }
            bool any = false;
            for (auto &p : parts) {
                size_t s = p.find_first_not_of(' '), e = p.find_last_not_of(' ');
                if (s == std::string::npos) continue;
                std::string t = p.substr(s, e - s + 1);
                if (isStringVar(t) && !t.empty() && t[0] != '&' && t[0] != '"') { p = "&" + t; any = true; }
            }
            if (any) {
                args2.clear();
                for (size_t k = 0; k < parts.size(); k++) { if (k) args2 += ","; args2 += parts[k]; }
            }
        }
        // gl's `obj.move_x`/`obj.move_y` FFI params are i32 (pixel deltas), but AC's own
        // `expand:config-spec` mechanism (gl.acl) synthesizes this call directly by name — it
        // never goes through the general float-vs-int argument typing any OTHER call gets — and
        // the AC frontend renders every numeric literal it sees here as a float text (verified:
        // examples/geodeo.ac's `arg.move_y -180` IR-lowers to a FLOAT const, "-180.000000" — the
        // Rust call then reads `ac_gl_obj_move_y(&arg, -1.8e+02)` against an `i32` param).
        if (func == "ac_gl_obj_move_x" || func == "ac_gl_obj_move_y") {
            auto comma = args2.find(',');
            if (comma != std::string::npos) {
                std::string tail = args2.substr(comma + 1);
                size_t s = tail.find_first_not_of(' ');
                if (s != std::string::npos) tail = tail.substr(s);
                args2 = args2.substr(0, comma) + ", (" + tail + ") as i32";
            }
        }
        // `ac_gl_obj_set_direction`/`ac_gl_obj_speed_mult`'s FFI params are f32, and — unlike
        // velocity's args, which always arrive as float-formatted text ("-1.6e+02") because
        // that call site already treats them as float — a bare `DEG x.direction=45`/`x.speed@=-1`
        // integer literal reaches here as plain integer text. Rust never coerces a bare integer
        // literal to a float parameter type (unlike C's implicit numeric promotion), so this
        // needs the same explicit-cast treatment move_x/move_y already get above, just the other
        // direction (verified: pong.ac — "expected f32, found integer" on both calls).
        if (func == "ac_gl_obj_set_direction" || func == "ac_gl_obj_speed_mult") {
            auto comma = args2.find(',');
            if (comma != std::string::npos) {
                std::string tail = args2.substr(comma + 1);
                size_t s = tail.find_first_not_of(' ');
                if (s != std::string::npos) tail = tail.substr(s);
                if (!isFloatVal(tail))
                    args2 = args2.substr(0, comma) + ", (" + tail + ") as f32";
            }
        }
        // math_to_int takes f64 — cast integer args explicitly
        std::string actualArgs = (func == "math_to_int" || func == "math.to_int") ? args2 + " as f64" : args2;
        // `mod` is a Rust KEYWORD — math.mod(...) won't compile; the Rust FFI names it
        // mod_f(f64, f64) (always-float) or mod_int(i64, i64) (always-int). AC's own "math.mod"
        // is TYPE-PRESERVING (int args -> int result, matching PY's dynamic behavior and C's own
        // `math.mod` dispatch, which just casts to double and back without changing the AC-level
        // type) — unconditionally routing through mod_f (this code's ORIGINAL behavior) forced
        // every int-arg call to return f64, breaking any subsequent int-only use (verified:
        // lcm.ac's `gcd` — `b = math.mod(a, b)` — "expected i64, found f64" on the very next
        // statement). Pick mod_int specifically when NEITHER arg looks float-typed.
        std::string rfunc = func;
        // `math.mod_int` can also be called DIRECTLY from AC source (not just reached via the
        // `math.mod` dispatch below) — its FFI signature is `mod_int(a: i64, b: i64) -> i64`, so
        // an argument that's float-typed on the CALLER side (e.g. a param the callee body
        // treats as float — see #floatparam) needs an explicit int cast (verified: examples/
        // math_number.ac calls `math.mod_int(n, 2)` directly with `n: f64`).
        if (func == "math.mod_int" || func == "math_mod_int") {
            std::string cast; std::string cur; int depth = 0;
            for (char c : actualArgs) {
                if (c == '(' || c == '[') depth++;
                else if (c == ')' || c == ']') depth--;
                if (c == ',' && depth == 0) { cast += "(" + cur + ") as i64, "; cur.clear(); continue; }
                cur += c;
            }
            if (!cur.empty()) cast += "(" + cur + ") as i64";
            actualArgs = cast;
        }
        if (func == "math.mod") {
            std::vector<std::string> margs; { std::string cur; int depth = 0;
                for (char c : actualArgs) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { margs.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) margs.push_back(cur); }
            bool anyFloat = false;
            for (auto& a : margs) {
                std::string t = a; size_t s = t.find_first_not_of(' '); size_t e = t.find_last_not_of(' ');
                if (s != std::string::npos) t = t.substr(s, e - s + 1);
                if (isFloatVal(t)) anyFloat = true;
            }
            if (anyFloat) {
                rfunc = "math.mod_f";
                std::string cast; std::string cur; int depth = 0;
                for (char c : actualArgs) {
                    if (c == '(' || c == '[') depth++;
                    else if (c == ')' || c == ']') depth--;
                    if (c == ',' && depth == 0) { cast += "(" + cur + ") as f64, "; cur.clear(); continue; }
                    cur += c;
                }
                if (!cur.empty()) cast += "(" + cur + ") as f64";
                actualArgs = cast;
            } else {
                rfunc = "math.mod_int";
            }
        }
        std::string call = rfunc + "(" + actualArgs + ")";
        // `recv.get()` on a widgets instance (display/ask/dropdown all expose a String-returning
        // `.get()` in widgets_ffi.rs) — `recv` is an arbitrary AC variable name, not a fixed
        // namespace, so isAcStrFunc's namespace-prefix table can't recognize it the way it does
        // stringm./os./regex. calls. A receiver-agnostic suffix check is the pragmatic fit here
        // (verified: `n = name_inp.get()` defaulted to i64, then `n + e` — both widget .get()
        // results — failed as "expected i64, found String" on the following concatenation).
        bool isWidgetGet = func.size() > 4 && func.compare(func.size() - 4, 4, ".get") == 0;
        // The gl ilib's Rust FFI has several genuinely `-> bool`-returning functions (verified:
        // ac_gl_is_obj/frame_begin/hitbox_overlap/etc — see gl_ffi.rs) that RustStrategy had no
        // way to know about, so their call result always defaulted to `i64` — "expected i64,
        // found bool" the moment the call is actually used (assigned, compared, or fed into
        // another expression). Same class of gap as Go's maudio.tts_ok fix.
        static const std::set<std::string> glBoolFuncs = {
            "gl.init", "gl_init", "ac_gl_init",
            "gl.obj_circle_fell", "gl_obj_circle_fell", "ac_gl_obj_circle_fell",
            "gl.is_obj", "gl_is_obj", "ac_gl_is_obj",
            "gl.is_draw", "gl_is_draw", "ac_gl_is_draw",
            "gl.hitbox_overlap", "gl_hitbox_overlap", "ac_gl_hitbox_overlap",
            "gl.hitbox_many_overlap", "gl_hitbox_many_overlap", "ac_gl_hitbox_many_overlap",
            "gl.hitbox_overlap_boundary", "gl_hitbox_overlap_boundary", "ac_gl_hitbox_overlap_boundary",
            "gl.hitbox_overlap_pattern", "gl_hitbox_overlap_pattern", "ac_gl_hitbox_overlap_pattern",
            "gl.frame_begin", "gl_frame_begin", "ac_gl_frame_begin",
            "gl.key_pressed", "gl_key_pressed", "ac_gl_key_pressed",
            "gl.key_just_pressed", "gl_key_just_pressed", "ac_gl_key_just_pressed",
            // os ilib (os_ffi.rs) — same class of gap (verified: os_demo.ac's `os.exists(...)`
            // defaulted to i64, "expected i64, found bool").
            "os.exists", "os_exists", "ac_os_exists",
            // regex ilib (regex_ffi.rs) — same class of gap (verified: regex_demo.ac's
            // `regex.match(...)`, "expected i64, found bool"). `match` is ALSO a Rust keyword
            // (see the method-side escape above, which runs first and renames the call to
            // `regex.match_` before this set is consulted — hence the `_` form here).
            "regex.match_", "regex.match", "regex_match", "ac_regex_match",
            "regex.test", "regex_test", "ac_regex_test",
            // machine-audio ilib (machine-audio_ffi.rs) — same class of gap (verified:
            // audio_test.ac's `maudio.tts_ok()`, "expected i64, found bool").
            "maudio.tts_ok", "maudio_tts_ok", "ac_maudio_tts_ok",
        };
        if (res.empty()) {
            emit(out, indent, call + ";");
        } else if (glBoolFuncs.count(func) && declared.insert(res).second) {
            rustBoolVars_.insert(res);
            emit(out, indent, "let mut " + res + ": bool = " + call + ";");
        } else if ((isAcStrFunc(func) || isWidgetGet) && declared.insert(res).second) {
            stringVars_.insert(res);
            emit(out, indent, "let mut " + res + ": String = " + call + ";");
        } else if (isAcStrListFunc(func) && declared.insert(res).second) {
            emit(out, indent, "let mut " + res + ": Vec<String> = " + call + ";");
        } else if (isListReturningFunc(func) && declared.insert(res).second) {
            listVars.insert(res);
            emit(out, indent, "let mut " + res + ": Vec<i64> = " + call + ";");
        } else if (isUserStringReturningFunc(func) && declared.insert(res).second) {
            // #retstring: matches emitFunctionBegin's `-> String` for a user fn that returns a
            // string — without this the call site defaulted to `let mut res: i64 = ...`.
            stringVars_.insert(res);
            emit(out, indent, "let mut " + res + ": String = " + call + ";");
        } else if (rfunc == "math.mod_int" && declared.insert(res).second) {
            // See the math.mod dispatch above: routed through the int-returning FFI variant,
            // so the result must be declared i64 too — isFloatReturningFunc(func) below would
            // otherwise still see the ORIGINAL, unrenamed "math.mod" and wrongly declare f64
            // regardless of which actual function got called.
            emit(out, indent, "let mut " + res + ": i64 = " + call + ";");
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
            // A function declared `-> String` (see #retstring in emitFunctionBegin) can't
            // `return "literal";` as-is — `&str` doesn't coerce to `String` on return, needs
            // an explicit `.to_string()` (only for a bare literal; an already-`String` value —
            // e.g. a variable or concatenation result — must NOT be double-wrapped).
            // AC functions can genuinely mix return types across branches (dynamic typing —
            // `is_leap` returns a string on most paths, bare `0` on the fallthrough one); the
            // WHOLE-FUNCTION inference picks ONE Rust return type (String, here, since most
            // paths are strings), so a numeric-literal/int-typed branch needs the SAME
            // `.to_string()` treatment, not just an already-string literal (verified:
            // leap_year.ac's trailing `return 0` — "expected String, found integer").
            bool needsStringify = curFuncReturnIsString_
                && !isStringVar(val) && !stringVars_.count(val);
            std::string v = needsStringify ? "(" + val + ").to_string()" : val;
            emit(out, indent, "return " + v + ";");
            lastWasReturn = true;
        }
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        lastWasReturn = false;
        // `null`/`nil` both map to the literal text "None" (formatRef's nullVal/nilVal) — a
        // bare, untyped `None` doesn't implement `Display` (Rust can't infer WHICH `Option<T>`
        // to print), so `println!("{}", None)` is a hard type error. `Term.display null` has
        // no meaningful VALUE to format anyway; just print the word, matching what every other
        // backend's text-based null/nil representation already does (e.g. Python's `None`).
        if (val == "None") emit(out, indent, "println!(\"None\");");
        // A list (`Vec<i64>`) has no `Display` impl at all — only `Debug` (verified: sieve.ac's
        // `Term.display primes`, "`Vec<i64>` doesn't implement `std::fmt::Display`"). Must be
        // checked before the generic float/string fallthrough below.
        else if (listVars.count(val) || listParams_.count(val) || listGlobals_.count(val))
            emit(out, indent, "println!(\"{:?}\", " + val + ");");
        // A float-typed value must use `{:?}` (Debug), not `{}` (Display): Rust's Display impl
        // for f64 drops the trailing ".0" on whole numbers (`println!("{}", 9.0f64)` → "9"),
        // which silently turns a float back into what LOOKS like an int in the output — wrong
        // for any whole-number float (e.g. an expression-form `to_dec(9)` cast). `{:?}` always
        // keeps the decimal point. Only floats: Debug-formatting a String would add quotes.
        else if (isFloatVal(val))
            emit(out, indent, "println!(\"{:?}\", " + val + ");");
        else emit(out, indent, "println!(\"{}\", " + val + ");");
    }
    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res`, crashing `result = sure $x$` with "cannot find value" on this backend too.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        lastWasReturn = false;
        emit(out, indent, "println!(\"{}\", " + val + ");");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
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
            // Non-Copy types (String, Vec) would be MOVED by a plain save, leaving `v` unusable in
            // the loop body. .clone() keeps the original live (#6/#22 loop-sandbox on Rust).
            emit(out, indent, "let mut " + pfx + v + " = " + v
                              + ((isStringVar(v) || listVars.count(v) || listParams_.count(v) || listGlobals_.count(v)) ? ".clone();" : ";"));
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
        // Rust: booleans only; wrap integer conditions. A genuine native-bool var (see
        // rustBoolVars_) is already a real `bool` — appending `!= 0` unconditionally (the
        // long-standing default here, correct for AC's usual int-everywhere convention) is a
        // hard type error against an actual bool ("expected bool, found integer").
        std::string rcond = rustBoolVars_.count(cond) ? cond
                           : looksNumeric(cond) ? (cond + " != 0") : "(" + cond + ") != 0";
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
        if (isStringVar(collection)) {
            // AC iterates a string as 1-char strings; Rust chars() yields char → wrap to String.
            std::string raw = iterVar + "__ch";
            emit(out, indent, "for " + raw + " in " + collection + ".chars() {");
            indent++;
            emit(out, indent, "let " + iterVar + " = " + raw + ".to_string();");
            return;
        }
        // `.iter()` yields `&i64` (a reference), but every AC use of the loop var treats it as
        // a plain value (`v < lo`, `lo = v`, ...) — Rust won't compare/assign a `&i64` against a
        // bare `i64` without an explicit deref (verified: array_min_max.ac/running_max.ac/
        // second_largest.ac all failed this way). i64 is Copy, so dereferencing once up front
        // (same "raw ref name, then a real binding" shape the string-chars case above already
        // uses) is free and sidesteps the mismatch at every later use site instead of chasing it
        // down individually.
        std::string raw = iterVar + "__ref";
        emit(out, indent, "for " + raw + " in " + collection + ".iter() {");
        indent++;
        emit(out, indent, "let " + iterVar + " = *" + raw + ";");
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
            dictVars_.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (!numeric) dictStrVals_.insert(var);
            std::string vt = numeric ? "i64" : "String";
            emit(out, indent, "let mut " + var + ": std::collections::HashMap<String," + vt + "> = std::collections::HashMap::new();");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + ".insert(" + fmtDictKey(k) + ".to_string(), "
                                  + (numeric ? v : fmtDictValStr(v) + ".to_string()") + ");");
            declared.insert(var);
        } else {
            auto dElems = splitCommaTrimmed(content);
            bool allDicts = !dElems.empty();
            for (auto& e : dElems) if (!dictVars_.count(e)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = dictStrVals_.count(dElems[0]) > 0;
                if (strVal) listOfDictStrVals_.insert(var);
                std::string vt = strVal ? "String" : "i64";
                std::string listOf;
                for (size_t k = 0; k < dElems.size(); k++) { if (k) listOf += ", "; listOf += dElems[k]; }
                emit(out, indent, "let mut " + var + ": Vec<std::collections::HashMap<String," + vt + ">> = vec![" + listOf + "];");
                declared.insert(var);
                return;
            }
            emit(out, indent, "let mut " + var + " = vec![" + content + "];");
            listVars.insert(var);
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        if (dictVars_.count(arr)) {
            declared.insert(result);
            // AsRef trick: works for BOTH &str literals and String vars as the key
            std::string key = "AsRef::<str>::as_ref(&(" + idx + "))";
            if (dictStrVals_.count(arr))
                emit(out, indent, "let " + result + " = " + arr + "[" + key + "].clone();");
            else
                emit(out, indent, "let " + result + " = " + arr + "[" + key + "];");
            return;
        }
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            declared.insert(result);
            emit(out, indent, "let " + result + " = " + arr + "[(" + idx + ") as usize].clone();");
            return;
        }
        if (isStringVar(arr)) {
            declared.insert(result);
            emit(out, indent, "let " + result + " = (" + arr + ".as_bytes()[(" + idx + ") as usize] as char).to_string();");
            return;
        }
        emit(out, indent, decl(result, arr + "[(" + idx + ") as usize]"));
    }
    void emitStoreIndex(std::ostringstream &out, int &indent,
                        const std::string &arr, const std::string &idx,
                        const std::string &val) override
    {
        if (dictVars_.count(arr)) {
            emit(out, indent, arr + ".insert((" + idx + ").to_string(), " + val + ");");
            return;
        }
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
        // Without this, a later plain copy-through (`name = t_0`, a separate STORE_VAR after
        // this input call) had no way to know t_0 was a String and defaulted to i64 (verified:
        // ask_input.ac — "expected i64, found String").
        stringVars_.insert(result);
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
        // Same native-bool exception as emitIfBegin (see its comment) — a genuine `bool` var
        // (currently gl's several `-> bool` functions) can't be `!= 0`-wrapped a second time.
        std::string rcond = rustBoolVars_.count(cond) ? cond : "(" + cond + ") != 0";
        if (label == "__break__" || label == "\"__break__\"")
        {
            emit(out, indent, "if !(" + rcond + ") { break; }");
        }
        else if (label == "__continue__" || label == "\"__continue__\"")
        {
            emit(out, indent, "if !(" + rcond + ") { continue; }");
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
                // stringParams_ (see its own comment) — RustStrategy never wired
                // setStringParams at all before this (base class no-op default), so
                // detectStringParams' whole-program result was silently discarded; isStringVar
                // alone (the general whole-body string fixpoint) doesn't catch a param whose
                // ONLY string evidence is being passed to a specific known-string-arg ilib call
                // (verified: gl_bounce.ac's `reset_ball(arg)` — `arg` only ever appears as the
                // argument to `gl:obj.is`/`gl:obj.regen`, never itself assigned a string literal
                // or compared to one — declared `i64`, then failed passing a `&str` key name).
                if (isStringVar(pname) || stringParams_.count(pname)) {
                    stringVars_.insert(pname);
                    tparams += pname + ": &str";   // #6: read-only string param (literals pass free)
                } else if (listParams_.count(pname)) {
                    tparams += "mut " + pname + ": Vec<i64>";  // array parameter (moved in)
                } else if (fit != funcTypedParams_.end()) {
                    std::string argList;
                    for (int k = 0; k < fit->second; ++k) { if (k) argList += ", "; argList += "i64"; }
                    tparams += "mut " + pname + ": fn(" + argList + ") -> i64";
                } else if (floatParams_.count(pname)) {
                    floatVars.insert(pname);
                    tparams += "mut " + pname + ": f64";
                } else {
                    tparams += "mut " + pname + ": i64";
                }
                first = false;
            }
        }
        declareParams(rParams, declared);
        curFuncReturnIsList_ = returnIsList_;
        // #retstring: RustStrategy never consulted baseReturnIsString_ at all — a user function
        // returning a bare string literal (e.g. via `cond`/`return $prime$`) always got the
        // numeric default `-> i64`, an immediate type mismatch. Found via bundle regression
        // testing (examples/showcase.ac's `describe`), pre-existing, unrelated to bundles.
        curFuncReturnIsString_ = baseReturnIsString_;
        // #retvoid: a genuinely void function (no `return <value>` anywhere — e.g. a
        // `configure event-listener` key-callback body) defaulted to `-> i64` here, same bug
        // class as #retstring — found via `bind`/event-listener regression testing. A real
        // Rust void fn omits the `-> T` clause entirely (unit return `()`), which matters
        // beyond cosmetics: `_ac_bind` needs an exact `fn()` value, and `fn() -> i64` is a
        // different, incompatible type — `-> i64` wasn't just wrong, it broke passing the
        // callback as a value at all.
        // The constructor (isNew) ALWAYS returns Self via its own dedicated signature line
        // below — never treated as void, regardless of what the generic "no explicit return
        // anywhere" scan concluded (the synthetic field-init body has no `return <value>`
        // either, same as any other void function, but this one isn't one).
        bool isNew = !classOwner.empty() && name == "init";
        curFuncIsConstructor_ = isNew;
        curFuncReturnIsVoid_ = returnIsVoid_ && !isNew;
        std::string retT = curFuncReturnIsVoid_ ? "" : returnIsList_ ? "-> Vec<i64> " : baseReturnIsString_ ? "-> String " : returnIsFloat_ ? "-> f64 " : "-> i64 ";
        returnIsList_ = false;
        returnIsFloat_ = false;
        baseReturnIsString_ = false;
        returnIsVoid_ = false;
        if (isNew) {
            emit(out, indent, "pub fn new(" + tparams + ") -> Self {");
            // `new()` has no `self` receiver — bind one from Self::default() (see
            // emitClassBegin's #[derive(Default)] note) so field-init statements can use plain
            // `self.field = val;` like every other method, instead of building one big struct
            // literal that would need buffering every field statement up front.
            indent++;
            emit(out, indent, "let mut _ac_self = Self::default();");
            indent--;
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "pub fn " + rName + "(&mut self" + sep + tparams + ") " + retT + "{");
        } else {
            emit(out, indent, "fn " + rName + "(" + tparams + ") " + retT + "{");
        }
        indent++;
        // Hoist cross-block locals to the top so a later use in a sibling/outer block resolves
        // (#41 — same fix already applied to C/C++/Java/V/JS; RustStrategy never consumed
        // hoistVars_ at all before this, a real gap: a var whose first assignment sits inside an
        // `if`/`match` arm got declared via `let` INSIDE that arm's block, invisible to any
        // sibling branch or code after the `if` — verified: examples/calculator.ac's `result`,
        // "cannot find value `result` in this scope" the moment it's read outside the arm that
        // first assigned it).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "let mut " + v + ": f64 = 0.0;"; }
            else if (t == IRType::STRING) { stringVars_.insert(v); line = "let mut " + v + ": String = String::new();"; }
            else if (t == IRType::LIST)     line = "let mut " + v + ": Vec<i64> = Vec::new();";
            else                             line = "let mut " + v + ": i64 = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
        funcTypedParams_.clear();
    }
    bool curFuncReturnIsList_ = false;
    bool curFuncReturnIsString_ = false;
    bool curFuncReturnIsVoid_ = false;
    bool curFuncIsConstructor_ = false;
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
    bool returnIsVoid_ = false;
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        // A void fn (unit return `()`) must NOT get a trailing bare value expression — `0` as
        // the last expression in a function with no `-> T` is itself a type error (Rust infers
        // `()` and `0` doesn't fit), unlike every OTHER case here which needs SOME fallthrough
        // value to match its declared non-void return type.
        if (!lastWasReturn && !curFuncReturnIsVoid_)
            emit(out, indent, curFuncIsConstructor_ ? "_ac_self"
                             : curFuncReturnIsList_ ? "vec![]"
                             : curFuncReturnIsString_ ? "String::new()" : "0");
        lastWasReturn = false; curFuncReturnIsList_ = false;
        curFuncReturnIsString_ = false; curFuncIsConstructor_ = false; curFuncReturnIsVoid_ = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear();
    }
    std::string curClassName_;
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        // #[derive(Default)]: the constructor prologue (see emitFunctionBegin's `isNew` branch)
        // builds the instance via `Self::default()` then assigns fields one at a time (Rust's
        // `new()` associated function has no `self` receiver to sequentially mutate otherwise —
        // a struct LITERAL with every field listed at once is the only other option, which would
        // need buffering every field-init statement instead of emitting them as normal
        // statements — Default sidesteps that with a two-line prologue).
        curClassName_ = name;
        emit(out, indent, "#[derive(Default)]");
        emit(out, indent, "struct " + name + " {");
        indent++;
    }
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "String" : t == IRType::FLOAT ? "f64" : "i64";
        emit(out, indent, field + ": " + ty + ",");
    }
    // Called once after all of a class's fields are declared, before its methods are emitted —
    // closes the struct body and opens the `impl` block the methods live in.
    void emitFieldsEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        emit(out, indent, "impl " + curClassName_ + " {");
        indent++;
    }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // `init` is emitted as an associated function `ClassName::new(args)`, not a bare
        // `ClassName(args)` call (Rust has no implicit constructors at all). decl()'s generic
        // type inference would default `res` to `i64` — it has no way to know the value is a
        // struct instance, so declare it with the real class type directly instead.
        bool isNew = declared.insert(res).second;
        emit(out, indent, (isNew ? "let mut " + res + ": " + className + " = " : res + " = ")
                          + className + "::new(" + args + ");");
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
    std::string libArgRef(const std::string& name, bool isString) override
    {
        return isString ? "&" + name : name;
    }
    void emitTypeCast(std::ostringstream &out, int &indent,
                      const std::string &var, const std::string &src, IRType t) override
    {
        // Declare on first sight (a cast-only var — `to_int/short/mini x = e` — was never
        // otherwise declared → "cannot find value" on Rust). short/mini are advisory i64 here.
        bool isNew = declared.insert(var).second;
        // A string source parses at the boundary (`"42".parse()`), not `as` (which won't compile
        // from &str). Fixes `to_int n = $42$` on Rust.
        bool srcIsStr = (!src.empty() && src.front() == '"') || isStringVar(src);
        if (floatVars.count(var) || t == IRType::FLOAT) {
            floatVars.insert(var);
            std::string rhs = srcIsStr ? (src + ".parse::<f64>().unwrap_or(0.0)") : (src + " as f64");
            emit(out, indent, (isNew ? "let mut " + var + ": f64 = " : var + " = ") + rhs + ";");
        } else if (t == IRType::STRING) {
            emit(out, indent, (isNew ? "let mut " + var + ": String = " : var + " = ") + src + ".to_string();");
        } else if (irIntWidth(t)) {
            std::string ty = acIntTypeRs(irIntWidth(t));   // i32 / i16 from the type include
            std::string rhs = srcIsStr ? (src + ".parse::<" + ty + ">().unwrap_or(0)") : (src + " as " + ty);
            emit(out, indent, (isNew ? "let mut " + var + ": " + ty + " = " : var + " = ") + rhs + ";");
        } else if (t == IRType::ATOMIC) {
            // Declaration stays OUTSIDE the lock's `{ }` scope — see emitTypedStoreVar's note.
            std::string rhs = srcIsStr ? (src + ".parse::<i64>().unwrap_or(0)") : (src + " as i64");
            if (isNew) emit(out, indent, "let mut " + var + ": i64;");
            emit(out, indent, "{ let _ac_lg = _ac_atomic_lock().lock().unwrap(); " + var + " = " + rhs + "; }");
        } else {
            std::string rhs = srcIsStr ? (src + ".parse::<i64>().unwrap_or(0)") : (src + " as i64");
            emit(out, indent, (isNew ? "let mut " + var + ": i64 = " : var + " = ") + rhs + ";");
        }
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear();
        lastWasReturn = false;
        emit(out, indent, "fn main() {");
        indent++;
        // Same hoist fix as emitFunctionBegin (see its comment) — the mainloop itself is
        // block-scoped code too (verified: examples/calculator.ac's `result`, first assigned
        // inside one `if`/`else` arm of the REPL loop and read from sibling arms afterward).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "let mut " + v + ": f64 = 0.0;"; }
            else if (t == IRType::STRING) { stringVars_.insert(v); line = "let mut " + v + ": String = String::new();"; }
            else if (t == IRType::LIST)     line = "let mut " + v + ": Vec<i64> = Vec::new();";
            else                             line = "let mut " + v + ": i64 = 0;";
            emit(out, indent, line);
            declared.insert(v);
        }
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
    bool curFuncRetString_ = false;
    std::set<std::string> userStringFuncs_;
public:
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
private:
    std::set<std::string> declared;
    std::set<std::string> floatVars;
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(v);   // #42: pre-inferred float locals
    }
    std::set<std::string> stringVars;
    // Native Go `bool` vars — currently just maudio.tts_ok's result (see emitCall). Tracked the
    // same way stringVars is, so a plain copy-through (`tts_ready = t_0`, a separate STORE_VAR
    // after the CALL) propagates the type instead of defaulting to int64 and then mismatching
    // the `x == true` comparison AC's "is True" lowering always emits for Go.
    std::set<std::string> boolVars;
    std::map<std::string, IRType> varCastTypes_;
    std::map<std::string, int> funcTypedParams_;
    bool needsOS_ = false;
    void setNeedsOS(bool v) override { if (v) needsOS_ = true; }
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
        if (isMLFloatReturningFunc(fn)) return true;
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
    bool anyAtomicVars() const {
        for (auto& [k, v] : varCastTypes_) if (v == IRType::ATOMIC) return true;
        return false;
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool /*idiv*/, bool /*eval*/, bool /*etry*/) override {
        hasIpowOp_ = ip; hasRandomOp_ = r;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        // Route floats through the same string-returning twin of _ac_dblprint (see
        // emitHeader) — plain fmt.Sprintf("%v", ...) has the identical whole-number-drops-
        // decimal bug already fixed for the print path, and the saved text must match.
        std::string expr = floatVars.count(val) ? ("ac_fmt_double(" + val + ")") : ("fmt.Sprintf(\"%v\", " + val + ")");
        emit(out, indent, "_acSaved.WriteString(" + expr + " + \"\\n\")");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "os.WriteFile(" + filename + ", []byte(_acSaved.String()), 0644)");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "_acBind(" + key + ", " + callback + ")");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "_acTrigger(" + key + ")");
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
        bool needsUnsafeImport = false;
        std::vector<std::string> wrapperBlocks;
        // Go requires ALL imports before any other top-level declaration (same class of bug
        // already fixed for V, same session) — an ilib FFI wrapper block can carry its OWN
        // `import "X"` lines (verified: widgets' Go FFI emits `import "strings"` / `import
        // "fmt"`), but wrapperBlocks is only ever printed much later, after several other
        // function definitions — those imports landed after declarations, a hard Go compile
        // error ("imports must appear before other declarations"). Hoist them out here instead.
        std::set<std::string> hoistedImports;

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
        bool wantsNativeMath = false;
        for (auto& [lt, ln] : pendingImports_)
            if (lt == "ilib" && ln == "math") wantsNativeMath = true;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt == "ilib" && ln == "math") continue;   // native Go math shim (no cgo) below
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
                    out << "\n" << cgoBlock;
                }
                // Not every ilib needs cgo (see parseGoFFI's own comment) — wrappers can be
                // non-empty even when cgoBlock is empty, and previously only got pushed/hoisted
                // inside the `!cgoBlock.empty()` branch above, silently DROPPING every non-cgo
                // ilib's functions entirely (verified: machine-audio's maudio_say/listen/stop
                // never appeared in the output at all before this was split out).
                if (!wrappers.empty()) {
                    if (wrappers.find("unsafe.") != std::string::npos)
                        needsUnsafeImport = true;
                    // Extract any import lines from the wrapper body — both single-line
                    // (`import "X"`) and grouped-block (`import (\n\t"X"\n\t"Y"\n)`, the shape a
                    // plain non-cgo FFI file's own top-level imports naturally take, e.g.
                    // machine-audio's) — so they land in the main import(...) block instead of
                    // scattered after other declarations.
                    std::istringstream ws(wrappers);
                    std::ostringstream stripped;
                    std::string line;
                    bool inImportBlock = false;
                    while (std::getline(ws, line)) {
                        size_t a = line.find_first_not_of(" \t");
                        std::string trimmed = (a == std::string::npos) ? "" : line.substr(a);
                        if (inImportBlock) {
                            if (trimmed == ")") inImportBlock = false;
                            else if (!trimmed.empty()) hoistedImports.insert(trimmed);
                            continue;
                        }
                        if (trimmed == "import (") { inImportBlock = true; continue; }
                        if (trimmed.rfind("import ", 0) == 0) {
                            hoistedImports.insert(trimmed.substr(7));
                            continue;
                        }
                        stripped << line << "\n";
                    }
                    wrapperBlocks.push_back(stripped.str());
                }
            }
        }
        // If flib .so exists but no ilib CGO block was emitted, emit a standalone CGO preamble
        if (!flibLDFlags.empty() && !flibInjected) {
            out << "\n/*\n" << flibLDFlags << "*/\nimport \"C\"\n";
        }
        emitRaw(out, "");
        emitRaw(out, "import (");
        if (needsInput_) emitRaw(out, "    \"bufio\"");
        emitRaw(out, "    \"fmt\"");
        emitRaw(out, "    \"strconv\"");
        if (needsOS_ || needsSave_)   emitRaw(out, "    \"os\"");
        if (needsInput_ || needsSave_) emitRaw(out, "    \"strings\"");
        if (needsUnsafeImport) emitRaw(out, "    \"unsafe\"");
        if (wantsNativeMath)   emitRaw(out, "    gomath \"math\"");
        if (anyAtomicVars())   emitRaw(out, "    \"sync\"");
        emitRaw(out, "    \"syscall\"");
        {
            // Already-emitted-above imports would double-import if re-added — Go rejects that.
            std::set<std::string> already = {"\"fmt\"", "\"strconv\"", "\"syscall\""};
            if (needsInput_) already.insert("\"bufio\"");
            if (needsOS_ || needsSave_) already.insert("\"os\"");
            if (needsInput_ || needsSave_) already.insert("\"strings\"");
            if (needsUnsafeImport) already.insert("\"unsafe\"");
            if (anyAtomicVars()) already.insert("\"sync\"");
            for (auto& imp : hoistedImports)
                if (!already.count(imp)) emitRaw(out, "    " + imp);
        }
        emitRaw(out, ")\n");
        emitRaw(out, "func _b(b bool) int64 { if b { return 1 }; return 0 }");
        emitRaw(out, "func _ac_abort() { syscall.Kill(syscall.Getpid(), syscall.SIGABRT) }\n");
        // `to_int`/`to_dec`/`to_bool`/`short`/`mini`/`atomic` on a STRING source (`to_int n =
        // $42$`) must PARSE the text — Go's `int64("42")` is a hard compile error (no such
        // conversion exists at all, unlike C's silent-garbage pointer-cast bug).
        emitRaw(out, "func ac_atoi(s string) int64 { v, _ := strconv.ParseInt(s, 10, 64); return v }");
        emitRaw(out, "func ac_atof(s string) float64 { v, _ := strconv.ParseFloat(s, 64); return v }");
        if (anyAtomicVars())
            emitRaw(out, "var _acAtomicLock sync.Mutex // `atomic` vars: any op touching one is a global critical section\n");
        if (needsEvents_) {
            emitRaw(out, "var _acEvents = map[string]func(){}");
            emitRaw(out, "func _acBind(key string, fn func()) { _acEvents[key] = fn }");
            emitRaw(out, "func _acTrigger(key string) { if fn, ok := _acEvents[key]; ok { fn() } }");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "func ac_iota(n int64) string {");
        emitRaw(out, "    r := \"\"");
        emitRaw(out, "    for i := int64(0); i < n; i++ {");
        emitRaw(out, "        r += fmt.Sprint(i)");
        emitRaw(out, "    }");
        emitRaw(out, "    return r");
        emitRaw(out, "}");
        if (wantsNativeMath) {
            // ilib math on Go = the native math package (cgo needed header paths that broke
            // outside the repo). Call sites pass int64s; args are float64()-cast at emitCall.
            emitRaw(out, "func math_mod(a, b float64) float64 { r := gomath.Mod(a, b); if r != 0 && (r < 0) != (b < 0) { r += b }; return r }");
            emitRaw(out, "func math_sin(x float64) float64 { return gomath.Sin(x) }");
            emitRaw(out, "func math_cos(x float64) float64 { return gomath.Cos(x) }");
            emitRaw(out, "func math_tan(x float64) float64 { return gomath.Tan(x) }");
            emitRaw(out, "func math_sqrt(x float64) float64 { return gomath.Sqrt(x) }");
            emitRaw(out, "func math_cbrt(x float64) float64 { return gomath.Cbrt(x) }");
            emitRaw(out, "func math_abs(x float64) float64 { return gomath.Abs(x) }");
            emitRaw(out, "func math_floor(x float64) float64 { return gomath.Floor(x) }");
            emitRaw(out, "func math_ceil(x float64) float64 { return gomath.Ceil(x) }");
            emitRaw(out, "func math_round(x float64) float64 { return gomath.Round(x) }");
            emitRaw(out, "func math_ln(x float64) float64 { return gomath.Log(x) }");
            emitRaw(out, "func math_log2(x float64) float64 { return gomath.Log2(x) }");
            emitRaw(out, "func math_log10(x float64) float64 { return gomath.Log10(x) }");
            emitRaw(out, "func math_exp(x float64) float64 { return gomath.Exp(x) }");
            emitRaw(out, "func math_pow(a, b float64) float64 { return gomath.Pow(a, b) }");
            emitRaw(out, "func math_hypot(a, b float64) float64 { return gomath.Hypot(a, b) }");
            emitRaw(out, "func math_min(a, b float64) float64 { return gomath.Min(a, b) }");
            emitRaw(out, "func math_max(a, b float64) float64 { return gomath.Max(a, b) }");
            emitRaw(out, "func math_is_prime(n float64) float64 { m := int64(n); if m < 2 { return 0 }; for d := int64(2); d*d <= m; d++ { if m%d == 0 { return 0 } }; return 1 }");
            // Missing entirely before (verified: examples/calculator.ac — "undefined: math_rad2deg").
            emitRaw(out, "func math_rad2deg(x float64) float64 { return x * 180 / gomath.Pi }");
            emitRaw(out, "func math_deg2rad(x float64) float64 { return x * gomath.Pi / 180 }");
            // Missing entirely before (verified: examples/math_number.ac — "undefined: math_to_int").
            emitRaw(out, "func math_to_int(x float64) float64 { return gomath.Trunc(x) }");
            emitRaw(out, "var math_pi = gomath.Pi");
            emitRaw(out, "var math_e = gomath.E");
        }
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually uses it (Go's linker strips an unused top-level func from the final
        // binary regardless, but the generated SOURCE is still bloated with it either way).
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "func ac_ipow(b int64, e int64) int64 {");
            emitRaw(out, "    r := int64(1)");
            emitRaw(out, "    for i := int64(0); i < e; i++ {");
            emitRaw(out, "        r *= b");
            emitRaw(out, "    }");
            emitRaw(out, "    return r");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "func ac_rand(n int64) int64 {");
            emitRaw(out, "    if n <= 0 {");
            emitRaw(out, "        return 0");
            emitRaw(out, "    }");
            emitRaw(out, "    var tv syscall.Timeval");
            emitRaw(out, "    syscall.Gettimeofday(&tv)");
            emitRaw(out, "    x := uint64(tv.Sec*1000000+tv.Usec) | 1");
            emitRaw(out, "    x ^= x << 13");
            emitRaw(out, "    x ^= x >> 7");
            emitRaw(out, "    x ^= x << 17");
            emitRaw(out, "    return int64(x % uint64(n))");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "func ac_choice(xs []int64) int64 {");
            emitRaw(out, "    return xs[ac_rand(int64(len(xs)))]");
            emitRaw(out, "}");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "func ac_stream(a int64, b int64, s int64) string {");
        emitRaw(out, "    if s == 0 {");
        emitRaw(out, "        s = 1");
        emitRaw(out, "    }");
        emitRaw(out, "    r := \"\"");
        emitRaw(out, "    for i := a; (s > 0 && i < b) || (s < 0 && i > b); i += s {");
        emitRaw(out, "        r += fmt.Sprint(i)");
        emitRaw(out, "    }");
        emitRaw(out, "    return r");
        emitRaw(out, "}\n");
        // Go's fmt.Println/%v for a float64 drops the trailing decimal point on a whole number
        // (fmt.Println(9.0) -> "9"), same class of bug as C's bare %.16g — matches PY's repr(float)
        // convention (always at least one digit after the point) instead.
        emitRaw(out, "func _ac_dblprint(d float64) { if d == float64(int64(d)) { fmt.Printf(\"%.1f\\n\", d) } else { fmt.Println(d) } }");
        if (needsSave_) {
            emitRaw(out, "func ac_fmt_double(d float64) string { if d == float64(int64(d)) { return fmt.Sprintf(\"%.1f\", d) }; return fmt.Sprintf(\"%v\", d) }");
            emitRaw(out, "var _acSaved strings.Builder  // `save as`: accumulates everything printed so far");
        }
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
        // Bundle field write (self.field = ...): Go structs already use plain `self.field` for
        // both reads and writes (Go auto-derefs a pointer receiver for `.` access, same as
        // Rust) — `self` isn't a reserved word in Go either (unlike Rust), so the constructor
        // can bind a real local literally named `self` (see emitFunctionBegin's `isNew`
        // prologue) and every method — including the constructor — uses the exact same
        // `self.field` text. The only fix needed: skip the `var TYPE` declaration wrapper.
        if (var.rfind("self.", 0) == 0) {
            if (floatVars.count(var) && !looksFloat(val) && !isKnownFloatName(val))
                return var + " = float64(" + val + ")";
            if (stringVars.count(var) && !looksString(val))
                return var + " = fmt.Sprintf(\"%v\", " + val + ")";
            return var + " = " + val;
        }
        if (declared.insert(var).second)
        {
            IRType ct = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : IRType::VOID);
            if (ct == IRType::FLOAT) { floatVars.insert(var); return "var " + var + " float64 = " + goWrap(val, IRType::FLOAT); }
            if (ct == IRType::STRING) { stringVars.insert(var); return "var " + var + " string = fmt.Sprintf(\"%v\", " + val + ")"; }
            if (ct == IRType::INT || ct == IRType::BOOL) { return "var " + var + " int64 = " + goWrap(val, IRType::INT); }
            // No cast override: use value heuristic. `stringVars.count(val)` (is the RHS ITSELF
            // a var already tracked as string, e.g. `heard = t_2` where t_2 came from a
            // string-returning ilib call) matters here just as much as `looksString` (a literal
            // "..." pattern) — without it, any copy-through of a string via a plain var
            // reference silently defaulted to int64 (verified: `heard = maudio.listen(4000)`
            // lowers as `t_2 = call ...` (correctly typed string) then `heard = stv t_2`, and
            // this second step never consulted stringVars at all).
            if (looksString(val) || stringVars.count(val)) { stringVars.insert(var); return "var " + var + " string = " + val; }
            if (boolVars.count(val)) { boolVars.insert(var); return "var " + var + " bool = " + val; }
            if (isFloatVal(val)) { floatVars.insert(var); return "var " + var + " float64 = " + val; }
            // Same plain-copy propagation as stringVars/boolVars above, for dicts (`p1 =
            // pets[1]` — see listOfDictVars_'s comment for the full verified failure across
            // every statically-typed backend, this one included).
            if (dictVars_.count(val)) {
                dictVars_.insert(var);
                bool strVal = dictStrVals_.count(val) > 0;
                if (strVal) dictStrVals_.insert(var);
                return "var " + var + " map[string]" + std::string(strVal ? "string" : "int64") + " = " + val;
            }
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
        declared.insert(var);
        if (isFloat) { floatVars.insert(var); emit(out, indent, "var " + var + " float64 = " + val + " /* const */"); }
        else emit(out, indent, "var " + var + " int64 = " + val + " /* const */");
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
    { declared.insert(res); emit(out, indent, res + " := ((int64(" + lhs + ") % int64(" + rhs + ")) + int64(" + rhs + ")) % int64(" + rhs + ")"); }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as decl(): real struct field, no `var`-redeclaration.
        if (var.rfind("self.", 0) == 0) { emit(out, indent, decl(var, val)); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (castDeclType(var, IRType::VOID) == IRType::ATOMIC) {
            if (declared.insert(var).second) emit(out, indent, "var " + var + " int64");
            emit(out, indent, "_acAtomicLock.Lock()");
            emit(out, indent, var + " = " + goWrap(val, IRType::INT));
            emit(out, indent, "_acAtomicLock.Unlock()");
            return;
        }
        // Re-typing coercion (#retype): a var ever assigned a string is a string everywhere;
        // non-strings get fmt.Sprint'd. `x=5; x=$hi$` → `var x string = fmt.Sprintf("%v",5); x="hi"`.
        if (isStringVar(var)) {
            std::string rhs = (looksString(val) || stringVars.count(val)) ? val
                              : "fmt.Sprintf(\"%v\", " + val + ")";
            if (declared.insert(var).second) { stringVars.insert(var); emit(out, indent, "var " + var + " string = " + rhs); }
            else                             emit(out, indent, var + " = " + rhs);
            return;
        }
        // A bare var reference already tracked as string (e.g. `heard = t_2`, t_2 being a
        // string-returning ilib call's result — see emitCall's isAcStrFunc branch) is a GENUINE
        // string value to propagate as-is — fundamentally different from the literal-text case
        // right below (a stringified NUMBER to unwrap back to numeric). Conflating the two by
        // checking `stringVars.count(val)` alongside `looksString(val)` in one branch sent a
        // real string through acUnstring's numeric-literal unwrapping, which no-ops on a bare
        // identifier and defaults to `int64` — wrong type entirely (verified: `heard` declared
        // `int64 = t_2` where t_2 is a `string`, a hard Go compile error).
        if (!looksString(val) && stringVars.count(val)) {
            if (declared.insert(var).second) { stringVars.insert(var); emit(out, indent, "var " + var + " string = " + val); }
            else                             emit(out, indent, var + " = " + val);
            return;
        }
        if (looksString(val)) {   // #retype numeric-unified ← stringified number literal
            std::string bare = acUnstring(val);
            std::string ty = (bare.find('.') != std::string::npos) ? " float64 = " : " int64 = ";
            if (declared.insert(var).second) emit(out, indent, "var " + var + ty + bare);
            else                             emit(out, indent, var + " = " + bare);
            return;
        }
        if (declared.insert(var).second) {
            IRType declType = castDeclType(var, floatVars.count(var) ? IRType::FLOAT : t);
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // Go string concat: `+` on two strings is legal Go, but this function otherwise always
        // treats `+` numerically (declares `res` as int64/float64) — with no string check at
        // all, `label + "!"` declared `t_6 int64 = label + "!"`, a hard type mismatch (Go has
        // no implicit string-to-int64). Mirrors the same fix already made for Rust.
        if (op == "+" && (isStringVar(res) || looksString(lhs) || isStringVar(lhs) || looksString(rhs) || isStringVar(rhs))) {
            std::string expr = "fmt.Sprintf(\"%v%v\", " + lhs + ", " + rhs + ")";
            bool isNewS = declared.insert(res).second;
            stringVars.insert(res);
            emit(out, indent, isNewS ? "var " + res + " string = " + expr : res + " = " + expr);
            return;
        }
        bool lhsFloat = isFloatVal(lhs), rhsFloat = isFloatVal(rhs);
        bool isFloat = lhsFloat || rhsFloat || floatVars.count(res);
        // Go has NO implicit numeric conversion — in float context cast BOTH operands
        // (float64(x) on a float64 is a legal no-op conversion). #42
        std::string elhs = isFloat ? "float64(" + lhs + ")" : lhs;
        std::string erhs = isFloat ? "float64(" + rhs + ")" : rhs;
        std::string expr = elhs + " " + op + " " + erhs;
        bool isNew = declared.insert(res).second;
        IRType rct = castDeclType(res, IRType::VOID);   // short/mini result → declare at that width
        if (isNew && isFloat)  { floatVars.insert(res); emit(out, indent, "var " + res + " float64 = " + expr); }
        else if (irIntWidth(rct)) {
            // short/mini result: Go has no implicit conversion, so wrap the expr in int32()/int16()
            // so it declares/stores at the narrow width (`a = a * 4` with a `short a`).
            std::string ty = acIntTypeGo(irIntWidth(rct));
            std::string rhs = std::string(ty) + "(" + expr + ")";
            emit(out, indent, isNew ? "var " + res + " " + ty + " = " + rhs : res + " = " + rhs);
        }
        else if (isNew)        emit(out, indent, "var " + res + " int64 = " + expr);
        else if (isFloat && !floatVars.count(res))
                               emit(out, indent, res + " = int64(" + expr + ")");
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
    // quickthread — Go is the one backend with native lightweight threads: a goroutine.
    void emitQuickThread(std::ostringstream &out, int &indent,
                         const std::string &func, const std::string &args) override
    {
        emit(out, indent, "go " + func + "(" + args + ")");
    }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        if (func == "ac_length" && !res.empty()) { if (declared.insert(res).second) emit(out, indent, "var " + res + " int64 = int64(len(" + args + "))"); else emit(out, indent, res + " = int64(len(" + args + "))"); return; }
        { auto ap = func.rfind(".append");
          if (ap != std::string::npos && ap == func.size() - 7) {
              std::string recv = func.substr(0, ap);
              emit(out, indent, recv + " = append(" + recv + ", " + args + ")"); return; } }
        // Native math shim takes float64 — Go has no implicit conversion, cast each arg.
        if (func.rfind("math_", 0) == 0 || func.rfind("math.", 0) == 0) {
            // `math.mod_int` can be called DIRECTLY from AC source (not only reached via the
            // `math.mod` dispatch below) — Go's native math shim has no separate `mod_int`
            // function at all (only `math_mod`), so route it there too, forced int (verified:
            // examples/math_number.ac's `math.mod_int(n, 2)` — "undefined: math_mod_int").
            std::string routedFunc = (func == "math.mod_int" || func == "math_mod_int") ? "math.mod" : func;
            bool forceInt = routedFunc != func;
            std::vector<std::string> margs; { std::string cur; int depth = 0;
                for (char c : args) { if (c=='('||c=='[') depth++; else if (c==')'||c==']') depth--;
                    if (c==',' && depth==0) { margs.push_back(cur); cur.clear(); } else cur += c; }
                if (!cur.empty()) margs.push_back(cur); }
            std::string cast; for (size_t k = 0; k < margs.size(); k++) { if (k) cast += ", "; cast += "float64(" + margs[k] + ")"; }
            std::string fn2 = routedFunc; for (auto& ch : fn2) if (ch == '.') ch = '_';
            std::string call2 = fn2 + "(" + cast + ")";
            // `math.mod` is TYPE-PRESERVING (int args -> int result, matching PY's dynamic
            // behavior and the identical fix already made for Rust/Java — see their comments).
            // Go's native math shim has no int overload at all (unlike Rust/Java's dual
            // mod/mod_int) — it's ALWAYS float64 by design, which is fine for sin/cos/sqrt/etc
            // but broke `math.mod` specifically: the underlying division is always exact when
            // both original args are int, so wrapping the whole call in `int64(...)` recovers
            // the right type without needing a second native function (verified: examples/
            // armstrong.ac / gcd_recursive.ac — "cannot use t_4 (float64) as int64 value").
            bool modIsInt = forceInt;
            if (!forceInt && (routedFunc == "math.mod" || routedFunc == "math_mod")) {
                modIsInt = true;
                for (auto& a : margs) {
                    std::string t = a; size_t s = t.find_first_not_of(' '), e = t.find_last_not_of(' ');
                    if (s != std::string::npos) t = t.substr(s, e - s + 1);
                    if (isFloatVal(t)) { modIsInt = false; break; }
                }
            }
            if (modIsInt) call2 = "int64(" + call2 + ")";
            if (res.empty()) { emit(out, indent, call2); return; }
            bool nw = declared.insert(res).second;
            if (!modIsInt) floatVars.insert(res);
            emit(out, indent, (nw ? "var " + res + " " + (modIsInt ? "int64" : "float64") + " = " : res + " = ") + call2);
            return;
        }
        // math_to_int takes float64 — cast integer args explicitly
        std::string actualArgs = (func == "math_to_int") ? "float64(" + args + ")" : args;
        std::string call = func + "(" + actualArgs + ")";
        if (res.empty()) {
            emit(out, indent, call);
        } else if ((func == "maudio.tts_ok" || func == "maudio_tts_ok") && declared.insert(res).second) {
            // Native Go bool (not AC's usual int64-everywhere convention) — `IF x is True`
            // lowers to a literal `x == true` comparison (see formatRef's trueVal/falseVal),
            // which only type-checks against an actual Go bool var, not int64. Declaring this
            // one result as `bool` matches that comparison instead of fighting it.
            boolVars.insert(res);
            emit(out, indent, "var " + res + " bool = " + call);
        } else if ((isAcStrFunc(func) || userStringFuncs_.count(func)) && declared.insert(res).second) {
            // stringVars must record `res` here — decl()'s own fresh-declaration path checks
            // `stringVars.count(val)` to propagate string-ness through a later plain copy
            // (`heard = t_2`, a separate STORE_VAR after this CALL); without this insert, that
            // check always missed the call result and silently re-defaulted to int64.
            stringVars.insert(res);
            emit(out, indent, "var " + res + " string = " + call);
        } else if (isAcStrListFunc(func) && declared.insert(res).second) {
            emit(out, indent, "var " + res + " []string = " + call);
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
        if (val.empty()) return;
        // AC functions can genuinely mix return types across branches (dynamic typing —
        // `is_leap` returns a string on most paths, bare `0` on the fallthrough one); the
        // WHOLE-FUNCTION inference picks ONE Go return type (string, here) — same fix as
        // RustStrategy/JavaStrategy's emitReturn (see their comments). `curFuncRetString_` was
        // already tracked (used by the synthetic fallthrough return at function end) but never
        // consulted here, for a REAL `return <value>` statement (verified: examples/
        // leap_year.ac's trailing `return 0` — "cannot use 0 (untyped int constant) as string
        // value in return statement").
        bool needsStringify = curFuncRetString_ && !looksString(val) && !isStringVar(val);
        std::string v = needsStringify ? "fmt.Sprint(" + val + ")" : val;
        emit(out, indent, "return " + v);
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (floatVars.count(val)) emit(out, indent, "_ac_dblprint(" + val + ")");
        else emit(out, indent, "fmt.Println(" + val + ")");
    }
    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res`, crashing `result = sure $x$` with "undefined: t_N" on this backend too.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        emit(out, indent, "fmt.Println(" + val + ")");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
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
        emit(out, indent, "{ _acs := float64(" + secs + "); syscall.Nanosleep(&syscall.Timespec{Sec: int64(_acs), Nsec: int64((_acs-float64(int64(_acs)))*1e9)}, nil) }");
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
        // `emitRaise` (bare `raise ERR(...)`, right above) sets needsOS_ before using
        // os.Stderr — this sibling method (`raise ClauseName(...)` / `raise hint(...)` /
        // `raise toxic(...)`) uses the exact same os.Stderr call but never set the flag,
        // so the "os" import was missing → "undefined: os" the moment a program used any
        // named/hint/toxic raise clause without ALSO using a bare `raise ERR(...)` elsewhere
        // to incidentally pull the import in.
        needsOS_ = true;
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
        // Go: wrap try body in an immediately-invoked closure with defer/recover.
        // The recover sets a flag so the CATCH body actually executes (it was emitted
        // under `if false` before — panics were swallowed and the handler never ran).
        emit(out, indent, "{");
        indent++;
        emit(out, indent, "_ac_caught := false; _ac_msg := \"\"; _ = _ac_msg");
        emit(out, indent, "func() {");
        indent++;
        emit(out, indent, "defer func() {");
        indent++;
        emit(out, indent, "if _ac_r := recover(); _ac_r != nil { _ac_caught = true; _ac_msg = fmt.Sprint(_ac_r) }");
        indent--;
        emit(out, indent, "}()");
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        indent--;
        emit(out, indent, "}()");
        emit(out, indent, "if _ac_caught {");
        indent++;
        emit(out, indent, "var " + exVar + " interface{} = _ac_msg; _ = " + exVar);
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
        indent--;
        emit(out, indent, "} // end try scope");
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        // `depth` names scopes by NESTING level, not occurrence — two SIBLING (sequential, not
        // nested) loops saving a same-named var both land on "depth 0". Go's `:=` requires AT
        // LEAST ONE new variable on the left — reusing it a second time is "no new variables on
        // left side of :=", same root cause (and same fix: reuse the slot, plain `=` after the
        // first declare) as the analogous C bug found via examples/keyword_catalog_core.ac.
        std::string pfx = "_ac_s" + std::to_string(depth) + "_";
        for (const auto &v : vars) {
            bool isNew = declared.insert(pfx + v).second;
            emit(out, indent, pfx + v + (isNew ? " := " : " = ") + v);
        }
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
        if (isStringVar(collection)) {
            // range over a Go string yields runes; wrap each as a 1-char string (AC semantics).
            std::string raw = iterVar + "__r";
            emit(out, indent, "for _, " + raw + " := range " + collection + " {");
            indent++;
            emit(out, indent, iterVar + " := string(" + raw + ")");
            emit(out, indent, "_ = " + iterVar);
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
            dictVars_.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (!numeric) dictStrVals_.insert(var);
            emit(out, indent, var + " := map[string]" + std::string(numeric ? "int64" : "string") + "{}");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + "[" + fmtDictKey(k) + "] = " + (numeric ? v : fmtDictValStr(v)));
            declared.insert(var);
        } else {
            auto dElems = splitCommaTrimmed(content);
            bool allDicts = !dElems.empty();
            for (auto& e : dElems) if (!dictVars_.count(e)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = dictStrVals_.count(dElems[0]) > 0;
                if (strVal) listOfDictStrVals_.insert(var);
                std::string vt = strVal ? "string" : "int64";
                std::string listOf;
                for (size_t k = 0; k < dElems.size(); k++) { if (k) listOf += ", "; listOf += dElems[k]; }
                emit(out, indent, var + " := []map[string]" + vt + "{" + listOf + "}");
                declared.insert(var);
                return;
            }
            emit(out, indent, var + " := []int64{" + content + "}");
            declared.insert(var);
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        if (isStringVar(arr)) {
            declared.insert(result)/*known*/;
            emit(out, indent, result + " := string(" + arr + "[" + idx + "])");
            emit(out, indent, "_ = " + result);
            return;
        }
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            declared.insert(result);
            emit(out, indent, result + " := " + arr + "[" + idx + "]");
            return;
        }
        // A VALUE read out of a dict (`p1["name"]`) — decl()'s fallback can't see the map's
        // value type from raw text, so it defaulted to int64 even for a string-valued dict;
        // Go's `:=` infers the correct type directly from the map-index expression itself.
        if (dictVars_.count(arr)) {
            if (dictStrVals_.count(arr)) stringVars.insert(result); else /* int64 */ {}
            declared.insert(result);
            emit(out, indent, result + " := " + arr + "[" + idx + "]");
            return;
        }
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
                if (isStringVar(pname)) {
                    tparams += pname + " string";    // #6: inferred string param
                } else if (listParams_.count(pname)) {
                    tparams += pname + " []int64";   // array parameter (slice: mutations visible)
                } else if (fit != funcTypedParams_.end()) {
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
        bool isNew = !classOwner.empty() && name == "init";
        curFuncIsConstructor_ = isNew;
        // #retvoid: a genuinely void function (e.g. a `configure event-listener` key-callback
        // body — no `return <value>` anywhere) defaulted to `int64` here, same bug class as
        // Rust's #retvoid fix. A Go void func has NO return-type clause at all, and — unlike
        // every other case here — must never get a fallback `return 0`: Go rejects `return`
        // with a value in a function declared with zero return values as a compile error.
        // Never treat the constructor as void even though its synthetic body has no explicit
        // `return <value>` either — it always returns `self` via its own dedicated line below.
        curFuncReturnIsVoid_ = returnIsVoid_ && !isNew;
        std::string retT = curFuncReturnIsVoid_ ? ""
                         : returnIsList_ ? "[]int64"
                         : baseReturnIsString_ ? "string"
                         : returnIsFloat_ ? "float64" : "int64";
        curFuncRetString_ = baseReturnIsString_;
        baseReturnIsString_ = false;
        returnIsList_ = false; returnIsFloat_ = false; returnIsVoid_ = false;
        if (isNew) {
            emit(out, indent, "func New" + classOwner + "(" + tparams + ") *" + classOwner + " {");
            // Unlike Rust, `self` isn't a reserved word in Go — a real local named `self` is
            // fine, so the constructor can use the exact same `self.field = val` statements
            // every other method already does, no renaming trick needed.
            indent++;
            emit(out, indent, "self := &" + classOwner + "{}");
            indent--;
        } else if (!classOwner.empty()) {
            std::string sep = tparams.empty() ? "" : ", ";
            emit(out, indent, "func (self *" + classOwner + ") " + gName + "(" + tparams + ") " + retT + " {");
        } else {
            emit(out, indent, "func " + gName + "(" + tparams + ") " + retT + " {");
        }
        indent++;
        // Hoist cross-block locals to the top so a later use in a sibling/outer block resolves
        // (#41 — same fix already applied to C/C++/Java/Rust/V/JS; GoStrategy never consumed
        // hoistVars_ at all before this): a var first assigned inside one `if`/`else` arm and
        // read from a sibling arm or after the `if` failed with "undefined: result" (verified:
        // examples/calculator.ac's REPL loop, on the mainloop side — see emitMainBegin's
        // identical fix below for that half).
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "var " + v + " float64"; }
            else if (t == IRType::STRING) { stringVars.insert(v); line = "var " + v + " string"; }
            else if (t == IRType::LIST)     line = "var " + v + " []int64";
            else                            line = "var " + v + " int64";
            emit(out, indent, line);
            declared.insert(v);
        }
        funcTypedParams_.clear();
    }
    bool curFuncReturnIsList_ = false;
    bool curFuncIsConstructor_ = false;
    bool curFuncReturnIsVoid_ = false;
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
    bool returnIsVoid_ = false;
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        if (!curFuncReturnIsVoid_)
            emit(out, indent, curFuncIsConstructor_ ? "return self"
                            : curFuncReturnIsList_   ? "return nil"
                            : curFuncRetString_      ? "return \"\"" : "return 0");
        curFuncReturnIsList_ = false; curFuncRetString_ = false;
        curFuncIsConstructor_ = false; curFuncReturnIsVoid_ = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear(); floatVars.clear(); stringVars.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "type " + name + " struct {");
        indent++;
    }
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "string" : t == IRType::FLOAT ? "float64" : "int64";
        emit(out, indent, field + " " + ty);
    }
    // Go methods (`func (self *X) name()`) are standalone top-level funcs, not nested inside
    // the struct like Rust's `impl` block — this only needs to close the struct's brace.
    void emitFieldsEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
    }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // `init` is emitted as the free function `NewClassName(args) *ClassName`, not a bare
        // `ClassName(args)` call — Go structs have no implicit constructor call syntax at all.
        // decl()'s generic type inference would default `res` to `int64` — declare it with the
        // real pointer type directly instead, same reasoning as Rust's emitConstructCall.
        bool isNew = declared.insert(res).second;
        emit(out, indent, (isNew ? "var " + res + " *" + className + " = " : res + " = ")
                          + "New" + className + "(" + args + ")");
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
        // Was: "the var was already declared via pre-scan, just assign" — true for SOME cast
        // targets (matched a separate pre-declaration pass) but not others: the FLOAT/STRING/
        // BOOL branches below never declared anything themselves and relied entirely on that
        // external pre-scan, which doesn't cover every case (a plain `to_bool flag = 1`, or an
        // EXPRESSION-form cast like `to_bool(0)` producing a fresh temp the pre-scan has no
        // name to have registered ahead of time) — "undefined: flag" / "undefined: t_N".
        // Every branch now declares on first sight, matching the two branches that already did.
        // ALSO: a STRING source (`to_int n = $42$`) needs real PARSING — Go's `int64("42")`/
        // `float64("42")` don't exist as conversions at all (hard compile errors, not even the
        // silent-garbage-pointer bug C had) — route through the new `ac_atoi`/`ac_atof` helpers.
        bool srcIsStr = looksString(src) || stringVars.count(src) || isStringVar(src);
        std::string intSrc = srcIsStr ? "ac_atoi(" + src + ")" : "int64(" + src + ")";
        std::string fltSrc = srcIsStr ? "ac_atof(" + src + ")" : "float64(" + src + ")";
        // `floatVars.count(var)` alone only catches a pre-scanned DECLARED variable (`to_dec d
        // = 7`) — an expression-form cast (`to_dec(9)`) targets a fresh temp the pre-scan never
        // saw, so it must also check the cast's own resultType `t` or it silently falls through
        // to the plain-int64 branch below (produces `9`, not `9.0` — a real type/value bug, not
        // just cosmetic: the value is stored and returned as an int, not a float).
        if (t == IRType::FLOAT || floatVars.count(var)) {
            floatVars.insert(var);
            if (declared.insert(var).second) emit(out, indent, "var " + var + " float64 = " + fltSrc);
            else emit(out, indent, var + " = " + fltSrc);
        }
        else if (stringVars.count(var)) {
            std::string rhs = srcIsStr ? src : "fmt.Sprintf(\"%v\", " + src + ")";
            if (declared.insert(var).second) emit(out, indent, "var " + var + " string = " + rhs);
            else emit(out, indent, var + " = " + rhs);
        }
        else if (t == IRType::BOOL) {
            std::string rhs = "func() int64 { if " + intSrc + " != 0 { return 1 }; return 0 }()";
            if (declared.insert(var).second) emit(out, indent, "var " + var + " int64 = " + rhs);
            else emit(out, indent, var + " = " + rhs);
        }
        else if (irIntWidth(t)) {
            // short/mini: fixed-width int from the type include. Declare on first sight — a cast-only
            // var (`short a = e`) isn't otherwise pre-declared, unlike plain-int casts.
            std::string ty = acIntTypeGo(irIntWidth(t));   // int32 / int16
            if (declared.insert(var).second)
                emit(out, indent, "var " + var + " " + ty + " = " + ty + "(" + intSrc + ")");
            else
                emit(out, indent, var + " = " + ty + "(" + intSrc + ")");
        }
        else if (t == IRType::ATOMIC) {
            if (declared.insert(var).second) emit(out, indent, "var " + var + " int64");
            emit(out, indent, "_acAtomicLock.Lock()");
            emit(out, indent, var + " = " + intSrc);
            emit(out, indent, "_acAtomicLock.Unlock()");
        }
        else {
            // Declare on first sight — a cast-only var isn't otherwise pre-declared → "undefined".
            if (declared.insert(var).second)
                emit(out, indent, "var " + var + " int64 = " + intSrc);
            else
                emit(out, indent, var + " = " + intSrc);
        }
    }

    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        declared.clear(); floatVars.clear(); stringVars.clear();
        // Pre-populate promoted globals so they get bare assignment inside main
        for (auto& v : promotedGlobals_) declared.insert(v);
        emit(out, indent, "func main() {");
        indent++;
        // Same hoist fix as emitFunctionBegin (see its comment) — the mainloop is block-scoped
        // Go code too, and never got this treatment at all before (verified: examples/
        // calculator.ac's REPL loop, "undefined: result").
        for (const auto& [v, t] : hoistVars_) {
            if (declared.count(v)) continue;
            std::string line;
            if      (t == IRType::FLOAT)  { floatVars.insert(v); line = "var " + v + " float64"; }
            else if (t == IRType::STRING) { stringVars.insert(v); line = "var " + v + " string"; }
            else if (t == IRType::LIST)     line = "var " + v + " []int64";
            else                            line = "var " + v + " int64";
            emit(out, indent, line);
            declared.insert(v);
        }
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
    void setFloatVarsFull(const std::set<std::string>& s) override {
        for (const auto& v : s) floatVars.insert(vName(v));   // #42 (V lowercases names)
    }
    bool lastWasReturn = false;
    bool needsInput_ = false;
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::unordered_map<std::string, std::string> rangeOf_;
    std::unordered_map<std::string, std::pair<std::string,std::string>> seqOf_;
    bool anyAtomicVars_ = false;
    void setVarCastTypes(const std::map<std::string, IRType>& m) override {
        for (auto& [k, v] : m) if (v == IRType::ATOMIC) { anyAtomicVars_ = true; break; }
    }
    bool anyAtomicVars() const { return anyAtomicVars_; }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    bool hasIpowOp_ = false, hasRandomOp_ = false, hasIdivOp_ = false, hasTryOp_ = false;
    void setUsedBuiltinOps(bool /*div*/, bool ip, bool /*len*/, bool /*add*/, bool r, bool idiv, bool /*eval*/, bool etry) override {
        hasIpowOp_ = ip; hasRandomOp_ = r; hasIdivOp_ = idiv; hasTryOp_ = etry;
    }
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        if (!needsSave_) return;
        // V's `${}` string interpolation already matches its own println formatting exactly
        // (verified: the whole-number-float fix applies identically to both), so no separate
        // float-vs-int dispatch is needed here the way C/Go/Rust's emitCapture needs one.
        emit(out, indent, "ac_save_append('${" + val + "}')");
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        emit(out, indent, "os.write_file(" + filename + ", ac_save_buf) or {}");
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    void emitEventBind(std::ostringstream &out, int &indent,
                       const std::string &key, const std::string &callback) override
    {
        emit(out, indent, "ac_bind(" + key + ", " + callback + ")");
    }
    void emitEventTrigger(std::ostringstream &out, int &indent,
                          const std::string &key) override
    {
        emit(out, indent, "ac_trigger(" + key + ")");
    }

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
        // `try`/`catch`: real via C's setjmp/longjmp through V's C-interop (V compiles to C —
        // this is the same technique CStrategy uses, just reached through `C.foo()`/`#include`
        // instead of writing raw C directly). V's own native runtime panic on division-by-zero
        // is NOT catchable (no Go-style panic/recover in V) — `ac_idiv` below checks for zero
        // itself, BEFORE V's runtime ever gets a chance to panic, and longjmps out instead when
        // a `try` is active. `[32]C.jmp_buf` warns "unknown type, all virtual C types must be
        // defined" but still compiles and works correctly — verified live.
        // NOTE: V requires ALL `import` statements at the very top of the file, before any
        // other top-level declaration — so imports must come first, setjmp/longjmp infra after.
        emitRaw(out, "import time as actime");
        if (needsInput_ || needsSave_) emitRaw(out, "import os");
        if (anyAtomicVars()) emitRaw(out, "import sync");
        // ilib FFI content (web/machine-audio/string-cheese/web-server/...) can carry its own
        // `import X` lines (net.http, io, etc). Each vendored .v ffi file is internally valid
        // V (imports before decls WITHIN that one file), but concatenating several such files
        // after our own atomic/try-catch/event globals breaks V's whole-file "imports first"
        // rule — so resolve+preprocess every ilib's content ONCE here, hoist its `import ` lines
        // up front (grouped with our own), and remember the import-stripped body to emit later,
        // after all the non-import declarations below.
        std::vector<std::string> ffiBodies;
        for (auto& [lt, ln] : pendingImports_) {
            if (lt != "ilib") continue;
            std::string libDir;
            std::string content = readFFIFile(ln, "v", &libDir);
            if (content.empty()) continue;
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
            // Skip any leading "module" line — we're already in module main. Hoist `import `
            // lines now; keep everything else for the deferred body pass below.
            std::istringstream ss(content);
            std::string line2;
            bool first = true;
            std::ostringstream body;
            while (std::getline(ss, line2)) {
                if (first && line2.rfind("module ", 0) == 0) { first = false; continue; }
                first = false;
                if (line2.rfind("import ", 0) == 0) emitRaw(out, line2);
                else body << line2 << "\n";
            }
            ffiBodies.push_back(body.str());
        }
        // See PythonStrategy::setUsedBuiltinOps' comment — only emit each builtin when the
        // program actually needs it. Gated on hasTryOp_ (a real `try`/`catch` block) OR
        // hasIdivOp_ (`//`, the only other caller of this setjmp stack) — see CStrategy's
        // identical fix/comment for the full rationale.
        if (hasTryOp_ || hasIdivOp_) {
            emitRaw(out, "#include <setjmp.h>");
            emitRaw(out, "fn C.setjmp(voidptr) int");
            emitRaw(out, "fn C.longjmp(voidptr, int)");
            emitRaw(out, "__global ( ac_try_stack [32]C.jmp_buf )");
            emitRaw(out, "__global ( ac_try_depth = 0 )");
        }
        if (hasIdivOp_) {
            // `//` truncating integer division
            emitRaw(out, "fn ac_idiv(a i64, b i64) i64 {");
            emitRaw(out, "    if b == 0 {");
            emitRaw(out, "        if ac_try_depth > 0 { C.longjmp(&ac_try_stack[ac_try_depth-1], 1) }");
            emitRaw(out, "        eprintln('Preposterous: 3rd grade mathematics violated (ZeroDivisionError)')");
            emitRaw(out, "        exit(1)");
            emitRaw(out, "    }");
            emitRaw(out, "    return a / b");
            emitRaw(out, "}");
        }
        if (anyAtomicVars()) {
            emitRaw(out, "__global ( ac_atomic_lock = sync.new_mutex() )  // `atomic` vars: any op touching one is a global critical section");
        }
        if (needsEvents_) {
            emitRaw(out, "__global ( ac_events map[string]fn () )");
            emitRaw(out, "fn ac_bind(key string, f fn ()) {");
            emitRaw(out, "    ac_events[key] = f");
            emitRaw(out, "}");
            emitRaw(out, "fn ac_trigger(key string) {");
            emitRaw(out, "    if key in ac_events { ac_events[key]() }");
            emitRaw(out, "}");
        }
        if (needsSave_) {
            emitRaw(out, "__global ( ac_save_buf = '' )");
            emitRaw(out, "fn ac_save_append(s string) { ac_save_buf += s + '\\n' }");
        }
        for (auto& body : ffiBodies) {
            out << "\n" << body;
        }
        for (auto& [lt, ln] : pendingImports_) {
            if (lt != "flib") continue;
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
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator
        emitRaw(out, "fn ac_iota(n i64) string {");
        emitRaw(out, "    mut r := ''");
        emitRaw(out, "    for i := i64(0); i < n; i++ { r += i.str() }");
        emitRaw(out, "    return r");
        emitRaw(out, "}");
        if (hasIpowOp_) {
            // `^` operator: integer power
            emitRaw(out, "fn ac_ipow(b i64, e i64) i64 {");
            emitRaw(out, "    mut r := i64(1)");
            emitRaw(out, "    for i := i64(0); i < e; i++ { r *= b }");
            emitRaw(out, "    return r");
            emitRaw(out, "}");
        }
        if (hasRandomOp_) {
            // `random.number(n)`: a uniform int in [0, n)
            emitRaw(out, "fn ac_rand(n i64) i64 {");
            emitRaw(out, "    if n <= 0 { return 0 }");
            emitRaw(out, "    mut x := u64(actime.now().unix_nano()) | 1");
            emitRaw(out, "    x ^= x << 13");
            emitRaw(out, "    x ^= x >> 7");
            emitRaw(out, "    x ^= x << 17");
            emitRaw(out, "    return i64(x % u64(n))");
            emitRaw(out, "}");
            // `random.choice(list)`
            emitRaw(out, "fn ac_choice(xs []i64) i64 {");
            emitRaw(out, "    return xs[ac_rand(i64(xs.len))]");
            emitRaw(out, "}");
        }
        // `stream(a, b, step)`: lazy sequence, displayed as its digits concatenated with no separator
        emitRaw(out, "fn ac_stream(a i64, b i64, s i64) string {");
        emitRaw(out, "    mut st := s");
        emitRaw(out, "    if st == 0 { st = 1 }");
        emitRaw(out, "    mut r := ''");
        emitRaw(out, "    mut i := a");
        emitRaw(out, "    for (st > 0 && i < b) || (st < 0 && i > b) { r += i.str(); i += st }");
        emitRaw(out, "    return r");
        emitRaw(out, "}");
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
    // A bare function passed AS A VALUE (event-bind callback) needs the SAME name transform
    // `emitFunctionBegin` applied at the function's own definition — `vName()` doesn't just
    // lowercase, it also rewrites a leading `_` to `a` (V forbids identifiers starting with
    // `_`). `formatRef` above only lowercases, so a callback like `__keycb_space` (defined as
    // `a_keycb_space` via vName) was referenced at the call site as the untransformed
    // `__keycb_space` — two different names for the same function, "no value" (undefined).
    std::string funcArgRef(const std::string &name) override { return vName(name); }

    std::string decl(const std::string &var, const std::string &val)
    {
        if (declared.insert(var).second)
        {
            if (floatVars.count(var)) return "mut " + var + " := f64(" + val + ")";  // #42
            if (looksString(val))
                return "mut " + var + " := " + val;
            // Same plain-copy propagation the other backends needed for dicts (`p1 = pets[1]`
            // — see listOfDictVars_'s comment) — wrapping a map value in `i64(...)` is invalid;
            // V's `:=` infers the correct map type directly from `val` with no wrapper at all.
            if (dictVars_.count(val)) {
                dictVars_.insert(var);
                if (dictStrVals_.count(val)) dictStrVals_.insert(var);
                return "mut " + var + " := " + val;
            }
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
    // A bare float LITERAL (e.g. a constant-folded "3.5"): all digits + exactly one dot
    // (+ optional leading '-'). Not "x.str()" / "obj.field" — those have non-digits.
    static bool isFloatLiteral(const std::string &v) {
        if (v.empty()) return false;
        size_t i = 0; if (v[0] == '-') i = 1;
        bool dot = false, digit = false;
        for (; i < v.size(); ++i) {
            if (v[i] == '.') { if (dot) return false; dot = true; }
            else if (v[i] >= '0' && v[i] <= '9') digit = true;
            else return false;
        }
        return dot && digit;
    }
    void emitStoreVar(std::ostringstream &out, int &indent, const std::string &var, const std::string &val) override
    {
        std::string vn = vName(var);
        // Bundle field write (self.field = ...): a real struct field (see emitFieldDecl), never
        // `mut TYPE :=`-redeclared. `vName()` only lowercases — "self.hp" stays "self.hp",
        // already valid V for both reads and writes (same as Go/Rust's non-constructor case).
        if (vn.rfind("self.", 0) == 0) { emit(out, indent, vn + " = " + val); return; }
        // #42: pre-inferred float locals declare as f64 (V has no implicit int/float mixing)
        if (floatVars.count(vn)) {
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := f64(" + val + ")");
            else                            emit(out, indent, vn + " = f64(" + val + ")");
            return;
        }
        // Preserve folded float constants (`i64(3.5)` would truncate to 3, #21).
        if (isFloatLiteral(val)) {
            floatVars.insert(vn);
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := f64(" + val + ")");
            else                            emit(out, indent, vn + " = " + val);
            return;
        }
        // A constant-folded string result (`iota 3` with a compile-time-known bound folds to
        // the literal `"012"`, arriving here as a plain LOAD_CONST — no `ac_iota()` call left
        // for the `isAcStrFunc` check in emitCall to catch) unconditionally fell to the numeric
        // `i64(...)` wrap below — "cannot cast string to i64". This function never called the
        // shared `decl()` (which DOES check `looksString`) at all; check it here too.
        if (looksString(val) || stringVars_.count(val) || isStringVar(val)) {
            stringVars_.insert(vn);
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := " + val);
            else                            emit(out, indent, vn + " = " + val);
            return;
        }
        // Same gap the string check right above already needed a fix for — this function never
        // calls decl() (which DOES have the equivalent dict check), so `p1 = t_10` (t_10 a real
        // map from a list-of-dicts read) unconditionally wrapped `i64(...)` around a map value
        // (see listOfDictVars_'s comment for the full verified failure).
        if (dictVars_.count(val)) {
            dictVars_.insert(vn);
            if (dictStrVals_.count(val)) dictStrVals_.insert(vn);
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := " + val);
            else                            emit(out, indent, vn + " = " + val);
            return;
        }
        if (declared.insert(vn).second)
            emit(out, indent, "mut " + vn + " := i64(" + val + ")");
        else
            emit(out, indent, vn + " = " + val);
    }
    void emitTypedStoreVar(std::ostringstream &out, int &indent,
                           const std::string &var, const std::string &val, IRType t) override
    {
        std::string vn = vName(var);
        // Bundle field write via a typed decl (e.g. `atomic hp = 5` as a field default) — same
        // translation as emitStoreVar: real struct field, no `mut`-redeclaration.
        if (vn.rfind("self.", 0) == 0) { emit(out, indent, vn + " = " + val); return; }
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in the
        // global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible RMW.
        if (t == IRType::ATOMIC) {
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := i64(0)");
            emit(out, indent, "ac_atomic_lock.lock()");
            emit(out, indent, vn + " = i64(" + val + ")");
            emit(out, indent, "ac_atomic_lock.unlock()");
            return;
        }
        bool isFloat = (t == IRType::FLOAT) || floatVars.count(vn);   // #42: pre-inferred floats
        // Re-typing coercion (#retype): unify to string, coerce non-strings via .str().
        if (isStringVar(var)) {
            std::string rhs = (looksString(val) || isStringVar(val)) ? val : "(" + val + ").str()";
            if (declared.insert(vn).second) { stringVars_.insert(vn); emit(out, indent, "mut " + vn + " := " + rhs); }
            else                            emit(out, indent, vn + " = " + rhs);
            return;
        }
        if (looksString(val) || isStringVar(val)) {   // #retype numeric-unified ← stringified number
            std::string bare = acUnstring(val);
            std::string wrap = (bare.find('.') != std::string::npos) ? "f64(" : "i64(";
            if (declared.insert(vn).second) emit(out, indent, "mut " + vn + " := " + wrap + bare + ")");
            else                            emit(out, indent, vn + " = " + wrap + bare + ")");
            return;
        }
        // STORE_VAR always dispatches through THIS function (not emitStoreVar — see the shared
        // dispatcher's case IROpcode::STORE_VAR), so this is the ACTUAL fix point for `p1 =
        // pets[1]` (see listOfDictVars_'s comment) — `i.resultType`/`t` has no "dict" case at
        // all (dicts aren't a real IRType, only tracked via dictVars_), so it always fell to
        // the plain-int default and wrapped a map value in `i64(...)`.
        if (declared.insert(vn).second) {
            if (isFloat) { floatVars.insert(vn); emit(out, indent, "mut " + vn + " := f64(" + val + ")"); }
            else if (t == IRType::STRING) emit(out, indent, "mut " + vn + " := "      + val);
            else if (t == IRType::BOOL)   emit(out, indent, "mut " + vn + " := bool(" + val + ")");
            else if (looksString(val))    emit(out, indent, "mut " + vn + " := "      + val);
            else if (dictVars_.count(val)) {
                dictVars_.insert(vn);
                if (dictStrVals_.count(val)) dictStrVals_.insert(vn);
                // V requires an explicit .clone() to copy a map value (not just a bit-copy like
                // scalars — "cannot copy map: call `move` or `clone` method").
                emit(out, indent, "mut " + vn + " := " + val + ".clone()");
            }
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
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // V string concat: `+` on two strings is legal V, but `decl()`'s `looksString(val)`
        // check (the generic fallback this used to fall through to) only recognizes a RAW
        // quoted literal — `looksString("label + \"!\"")` (the whole COMBINED expression
        // text) is false, since it's an expression, not a literal — so `label + "!"` fell to
        // the numeric default, wrapping the whole thing in `i64(...)`: `i64(label + "!")`, a
        // hard "cannot cast string to i64" error. Check the OPERANDS instead, same fix already
        // made for Rust/Go.
        if (op == "+" && (isStringVar(res) || looksString(lhs) || isStringVar(lhs) || looksString(rhs) || isStringVar(rhs))) {
            std::string expr = lhs + " + " + rhs;
            bool isNewS = declared.insert(res).second;
            stringVars_.insert(res);
            emit(out, indent, (isNewS ? "mut " : "") + res + (isNewS ? " := " : " = ") + expr);
            return;
        }
        // #42: V has no implicit int/float mixing — in float context cast BOTH operands
        // (f64(x) on an f64 is a legal no-op) and keep the result f64 through the chain.
        bool isFloat = floatVars.count(res) || floatVars.count(lhs) || floatVars.count(rhs)
                    || isFloatLiteral(lhs) || isFloatLiteral(rhs);
        if (isFloat) {
            floatVars.insert(res);
            std::string expr = "f64(" + lhs + ") " + op + " f64(" + rhs + ")";
            if (declared.insert(res).second) emit(out, indent, "mut " + res + " := " + expr);
            else                             emit(out, indent, res + " = " + expr);
            return;
        }
        emit(out, indent, decl(res, lhs + " " + op + " " + rhs));
    }
    void emitTrueDivision(std::ostringstream &out, int &indent,
                          const std::string &res, const std::string &lhs, const std::string &rhs) override
    {
        std::string expr = "f64(" + lhs + ") / f64(" + rhs + ")";
        floatVars.insert(res);   // division result is f64 — keep the chain float-typed
        if (declared.insert(res).second) emit(out, indent, "mut " + res + " := " + expr);
        else                             emit(out, indent, res + " = " + expr);
    }
    void emitIntDiv(std::ostringstream &out, int &indent, const std::string &res,
                    const std::string &lhs, const std::string &rhs) override
    {
        // Route through the guarded `ac_idiv` (see emitHeader) instead of native `/` — V's own
        // runtime division-by-zero panic is NOT catchable, so the check has to happen before V
        // ever gets a chance to panic.
        emit(out, indent, decl(res, "ac_idiv(" + lhs + ", " + rhs + ")"));
    }
    void emitMod(std::ostringstream &out, int &indent,
                 const std::string &res, const std::string &lhs, const std::string &rhs) override
    { emit(out, indent, decl(res, "((i64(" + lhs + ") % i64(" + rhs + ")) + i64(" + rhs + ")) % i64(" + rhs + ")")); }
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
    std::set<std::string> userListFuncs_;
    void setListReturnFuncs(const std::set<std::string>& s) override { userListFuncs_ = s; }
    // Float-return support (#21): without it, a float-returning fn is declared `i64`, so
    // `return f64(..)` is a hard V type error ("cannot use f64 as type i64 in return argument").
    bool returnIsFloat_ = false;
    bool curFuncReturnIsFloat_ = false;
    bool curFuncReturnIsString_ = false;
    std::set<std::string> userFloatFuncs_;
    std::set<std::string> floatParams_;
    void setFloatParams(const std::set<std::string>& s) override { floatParams_ = s; }
    // Vars known to hold a list (populated at emitAlloc's "list" case) — V arrays are
    // value-typed with explicit-copy semantics; assigning one bare list var to another needs
    // `.clone()` or V refuses to compile ("use `x = y.clone()` instead of `x = y`"). Consulted
    // by emitScopeEnter/emitScopeExit's loop save/restore (see their own comment).
    std::set<std::string> listVars_;
    void setReturnIsFloat(bool v) override { returnIsFloat_ = v; }
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string& fn) const { return userFloatFuncs_.count(fn) > 0; }
    // #retstring (same pattern as C/C++/Java/Rust/Go): a user function marked #retstring
    // returns a real string, not the i64 the default `decl()` wraps every call result in —
    // `decl()`'s own `looksString(val)` check only sees the literal call-expression text
    // ("describe(13)"), which never looks like a string, so without this a call to a
    // string-returning user function hard-fails V's type checker ("cannot cast string to i64").
    std::set<std::string> userStringFuncs_;
    void setStringReturnFuncs(const std::set<std::string>& s) override { userStringFuncs_ = s; }
    bool isUserStringReturningFunc(const std::string& fn) const { return userStringFuncs_.count(fn) > 0; }
    void emitCall(std::ostringstream &out, int &indent, const std::string &res,
                  const std::string &func, const std::string &args) override
    {
        if (func == "ac_length" && !res.empty()) { bool nw = declared.insert(res).second; if (nw) emit(out, indent, "mut " + res + " := i64((" + args + ").len)"); else emit(out, indent, res + " = i64((" + args + ").len)"); return; }
        {   // AC list.append → V's << operator
            auto ap = func.rfind(".append");
            if (ap != std::string::npos && ap == func.size() - 7) {
                emit(out, indent, func.substr(0, ap) + " << (" + args + ")");
                return;
            }
        }
        std::string call = func + "(" + args + ")";
        if (!res.empty() && isAcStrFunc(func) && declared.insert(res).second) {
            emit(out, indent, "mut " + res + " := " + call); // string result, no i64() wrap
            return;
        }
        if (!res.empty() && isAcStrListFunc(func) && declared.insert(res).second) {
            emit(out, indent, "mut " + res + " := " + call); // []string result, type-inferred
            return;
        }
        if (!res.empty() && userListFuncs_.count(func) && declared.insert(res).second) {
            emit(out, indent, "mut " + res + " := " + call); // []i64 result, no i64() wrap
            return;
        }
        if (!res.empty() && isUserFloatReturningFunc(func)) {
            floatVars.insert(res);
            if (declared.insert(res).second) emit(out, indent, "mut " + res + " := " + call); // f64 result
            else                             emit(out, indent, res + " = " + call);
            return;
        }
        if (!res.empty() && isUserStringReturningFunc(func)) {
            if (declared.insert(res).second) emit(out, indent, "mut " + res + " := " + call); // string result, no i64() wrap
            else                             emit(out, indent, res + " = " + call);
            return;
        }
        emit(out, indent, res.empty() ? call : decl(res, call));
    }
    void emitReturn(std::ostringstream &out, int &indent, const std::string &val) override
    {
        // Suppress bare empty return; emitFunctionEnd provides the fallthrough
        if (!val.empty())
        {
            // A float-returning fn must return f64. Wrap unless the value is already an f64
            // expression (avoids i64/f64 mismatch when the body returns an int local/literal).
            std::string rv = val;
            if (curFuncReturnIsFloat_ && !floatVars.count(val) && val.rfind("f64(", 0) != 0)
                rv = "f64(" + val + ")";
            // AC functions can genuinely mix return types across branches (dynamic typing —
            // `is_leap` returns a string on most paths, bare `0` on the fallthrough one); the
            // WHOLE-FUNCTION inference picks ONE V return type (string, here) — same fix as
            // RustStrategy/JavaStrategy/GoStrategy's emitReturn (see their comments). A numeric/
            // non-string branch's return value needs coercing via `.str()` (verified: examples/
            // leap_year.ac's trailing `return 0` — "cannot use `int literal` as type `string`").
            else if (curFuncReturnIsString_ && !looksString(val) && !isStringVar(val))
                rv = val + ".str()";
            emit(out, indent, "return " + rv);
            lastWasReturn = true;
        }
    }
    void emitPrint(std::ostringstream &out, int &indent, const std::string &val) override
    {
        lastWasReturn = false;
        emit(out, indent, "println(" + val + ")");
    }
    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res`, crashing `result = sure $x$` with "undefined ident" on this backend too.
    void emitConfirm(std::ostringstream &out, int &indent,
                     const std::string &res, const std::string &val) override
    {
        lastWasReturn = false;
        emit(out, indent, "println(" + val + ")");
        if (!res.empty()) emit(out, indent, decl(res, "0"));
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
        emit(out, indent, "actime.sleep(actime.second * int(" + secs + "))");
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

    // Real try/catch via setjmp/longjmp (see the `ac_try_stack`/`ac_idiv` note in emitHeader) —
    // same structure as CStrategy's fix: one if/else driven by setjmp's return value, inside an
    // outer wrapper that owns the depth-counter bookkeeping (both the normal-completion path
    // and the longjmp'd path need it decremented exactly once — see CStrategy's longer note).
    void emitTryBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        emit(out, indent, "{");
        indent++;
        emit(out, indent, "ac_try_depth++");
        emit(out, indent, "if C.setjmp(&ac_try_stack[ac_try_depth-1]) == 0 {");
        indent++;
    }
    void emitCatchBegin(std::ostringstream &out, int &indent,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        lastWasReturn = false;
        // V forbids identifiers starting with '_'; vName rewrites the leading underscore
        // (default catch var is `_exc`, which V rejects outright).
        std::string ev = vName(exVar);
        emit(out, indent, "ac_try_depth--");
        indent--;
        emit(out, indent, "} else {");
        indent++;
        emit(out, indent, "ac_try_depth--");
        emit(out, indent, "mut " + ev + " := '' ; _ = " + ev);
    }
    void emitAfterBegin(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
        emit(out, indent, "{");
        indent++;
    }
    void emitTryEnd(std::ostringstream &out, int &indent) override
    {
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");   // closes the catch- or after-body block
        indent--;
        emit(out, indent, "}");   // closes the outer wrapper opened by emitTryBegin
    }

    void emitScopeEnter(std::ostringstream &out, int &indent,
                        const std::vector<std::string> &vars, int depth) override
    {
        // `depth` names scopes by NESTING level, not occurrence — two SIBLING (sequential, not
        // nested) loops saving a same-named var both land on "depth 0", and V's `:=` rejects
        // redeclaring the same name a second time ("redefinition of `acs0_a`") — same root
        // cause (and fix: reuse the slot, plain `=` after the first declare) as the analogous
        // C/Go bugs found via examples/keyword_catalog_core.ac.
        // NOTE: a list-typed `v` here would need `.clone()` to satisfy V's copy-semantics rule
        // (see listVars_'s comment) — NOT added: verified that doing so, while it compiles,
        // changes RUNTIME behavior for a genuine loop accumulator (examples/array_append.ac's
        // `squares.append(...)` inside a FOR loop) — the save/restore pair silently clobbers the
        // accumulated appends back to the pre-loop snapshot instead of erroring, which is worse
        // than the original compile error. This needs a real investigation into why this shared
        // (cross-backend) save/restore mechanism is even firing around a plain accumulator
        // loop — left as a known, separate, deeper pre-existing bug, not fixed here.
        std::string pfx = "acs" + std::to_string(depth) + "_";  // V: no leading underscore
        for (const auto &v : vars) {
            bool isNew = declared.insert(pfx + v).second;
            emit(out, indent, (isNew ? "mut " : "") + pfx + v + (isNew ? " := " : " = ") + v);
        }
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
        // FOR over a STRING: V iterates a string as u8 bytes — bind a 1-char STRING instead
        // (AC semantics: `c` is a 1-char string, comparable/concatenable). #6
        if (isStringVar(collection)) {
            std::string bidx = "bi_" + iterVar;   // V forbids leading-underscore identifiers
            emit(out, indent, "for " + bidx + " in 0.." + collection + ".len {");
            indent++;
            emit(out, indent, "mut " + iterVar + " := " + collection + "[" + bidx + "..(" + bidx + "+1)]");
            stringVars_.insert(iterVar);
            declared.insert(iterVar);
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
            dictVars_.insert(var);
            bool numeric = dictValsAllNumeric(content);
            if (!numeric) dictStrVals_.insert(var);
            emit(out, indent, "mut " + var + " := map[string]" + std::string(numeric ? "i64" : "string") + "{}");
            for (auto& [k, v] : parseDictPairs(content))
                emit(out, indent, var + "[" + fmtDictKey(k) + "] = " + (numeric ? "i64(" + v + ")" : fmtDictValStr(v)));
            declared.insert(var);
        } else {
            auto dElems = splitCommaTrimmed(content);
            bool allDicts = !dElems.empty();
            for (auto& e : dElems) if (!dictVars_.count(e)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = dictStrVals_.count(dElems[0]) > 0;
                if (strVal) listOfDictStrVals_.insert(var);
                std::string listOf;
                for (size_t k = 0; k < dElems.size(); k++) { if (k) listOf += ", "; listOf += dElems[k]; }
                // V infers the element type (map[string]string/i64) directly from the literal's
                // own elements, same as Go's `:=` — no explicit annotation needed here.
                emit(out, indent, "mut " + var + " := [" + listOf + "]");
                declared.insert(var);
                return;
            }
            listVars_.insert(var);
            // V requires explicit type on first element; EMPTY list is []i64{}
            std::string c = content;
            {   // trim
                size_t a2 = c.find_first_not_of(" \t");
                c = (a2 == std::string::npos) ? "" : c.substr(a2, c.find_last_not_of(" \t") - a2 + 1);
            }
            if (c.empty()) {
                emit(out, indent, "mut " + var + " := []i64{}");
                declared.insert(var);
            } else {
                size_t comma = c.find(',');
                std::string vContent = comma != std::string::npos
                                           ? "i64(" + c.substr(0, comma) + ")" + c.substr(comma)
                                           : "i64(" + c + ")";
                emit(out, indent, "mut " + var + " := [" + vContent + "]");
                declared.insert(var);
            }
        }
    }
    void emitLoadIndex(std::ostringstream &out, int &indent,
                       const std::string &result, const std::string &arr,
                       const std::string &idx) override
    {
        if (isStringVar(arr)) {
            declared.insert(result);
            emit(out, indent, "mut " + result + " := " + arr + "[" + idx + "].ascii_str()");
            return;
        }
        if (listOfDictVars_.count(arr)) {
            bool strVal = listOfDictStrVals_.count(arr) > 0;
            dictVars_.insert(result);
            if (strVal) dictStrVals_.insert(result);
            declared.insert(result);
            // Same map-copy rule as emitTypedStoreVar: V errors "cannot copy map"
            // without an explicit .clone() here, since arr[idx] is itself a map value.
            emit(out, indent, "mut " + result + " := " + arr + "[" + idx + "].clone()");
            return;
        }
        // A VALUE read out of a dict (`p1["name"]`) — V's `:=` infers the type from the
        // map-index expression itself, same reasoning as Go's identical fix.
        if (dictVars_.count(arr)) {
            declared.insert(result);
            emit(out, indent, "mut " + result + " := " + arr + "[" + idx + "]");
            return;
        }
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
        else if (label == "__continue__" || label == "\"__continue__\"")
            emit(out, indent, "if " + cond + " == 0 { continue }");
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
        bool isNew = !classOwner.empty() && name == "init";
        curFuncIsConstructor_ = isNew;
        if (!classOwner.empty()) {
            if (vParams.rfind("self, ", 0) == 0) vParams = vParams.substr(6);
            else if (vParams == "self") vParams = "";
            // A top-level V function literally named `init()` is a reserved special form (like
            // Go's `func init()`) — runs automatically before main, no params/return allowed.
            // Our constructor needs both, so it can't be named that; `new_ClassName` avoids the
            // collision and matches this codebase's Go-backend naming for the same concept.
            // V function names must be snake_case (no uppercase at all) — lowercase just the
            // function name here, the STRUCT/return type stays PascalCase (`classOwner` as-is).
            funcName = isNew ? "new_" + vName(classOwner) : name;
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
        // Build signature with renamed params (name + "_p"); list params are []i64
        std::string tparams;
        for (size_t i = 0; i < paramNames.size(); i++) {
            if (i > 0) tparams += ", ";
            bool isStr  = isStringVar(paramNames[i]);
            bool isList = !isStr && listParams_.count(paramNames[i]) > 0;
            // Same gap/fix as RustStrategy/JavaStrategy's floatParams_ (see their comments) — a
            // param the function's OWN body treats as float (`x / 2.0`) always defaulted to
            // `i64` here regardless (verified: examples/newton_sqrt.ac's `nsqrt(2.0)` —
            // "cannot use `float literal` as `i64` in argument 1").
            bool isFloat = !isStr && !isList && floatParams_.count(paramNames[i]) > 0;
            tparams += paramNames[i] + "_p " + (isStr ? "string" : isList ? "[]i64" : isFloat ? "f64" : "i64");
        }
        // #retvoid: a genuinely void function (e.g. a `configure event-listener` key-callback
        // body) defaulted to `i64` — same bug class already fixed for Rust/Go/C. V's void
        // syntax omits the return type entirely: `fn name(params) { ... }`. Never treat the
        // constructor as void (its own dedicated `ClassName` return line below always applies).
        curFuncReturnIsVoid_ = returnIsVoid_ && !isNew;
        std::string vret = curFuncReturnIsVoid_ ? ""
                         : baseReturnIsString_ ? "string"
                         : baseReturnIsList_ ? "[]i64" : returnIsFloat_ ? "f64" : "i64";
        curFuncReturnIsString_ = baseReturnIsString_;
        baseReturnIsString_ = false;
        curFuncReturnIsFloat_ = returnIsFloat_;
        baseReturnIsList_ = false;
        returnIsFloat_ = false; returnIsVoid_ = false;
        if (isNew) {
            // Free function, not a method — `new_ClassName(args) ClassName { mut self := ... }`.
            emit(out, indent, "fn " + funcName + "(" + tparams + ") " + classOwner + " {");
            indent++;
            emit(out, indent, "mut self := " + classOwner + "{}");
            indent--;
        } else if (!classOwner.empty()) {
            // The receiver was previously UNNAMED (`fn (ClassName) name()`) — `self` was not
            // bound to anything, so `self.field` inside the body was a hard "undefined ident"
            // error. `mut self ClassName` both names it and allows field mutation.
            emit(out, indent, "fn (mut self " + classOwner + ") " + funcName + "(" + tparams + ") " + vret + " {");
        } else {
            emit(out, indent, "fn " + funcName + "(" + tparams + ") " + vret + " {");
        }
        indent++;
        // Emit mut local copies and add to declared (lists clone for V value semantics)
        for (const auto& p : paramNames) {
            bool isStr  = isStringVar(p);
            bool isList = !isStr && listParams_.count(p) > 0;
            bool isFloat = !isStr && !isList && floatParams_.count(p) > 0;
            emit(out, indent, "mut " + p + " := " + p + "_p" + (isList ? ".clone()" : ""));
            if (isStr) stringVars_.insert(vName(p));   // keep string-ness for body codegen
            if (isFloat) floatVars.insert(p);          // keep float-ness for body codegen
            declared.insert(p);
        }
        // Hoist cross-block locals (fixes #41: V scopes `:=` per block). Cross-block vars are
        // mutated ≥2 places by definition, so `mut` is legit; typed-zero init.
        for (const auto& [v, t] : hoistVars_) {
            std::string vn = VStrategy::vName(v);
            if (declared.count(vn)) continue;
            std::string init;
            if      (t == IRType::FLOAT)  { floatVars.insert(vn); init = "f64(0)"; }
            else if (t == IRType::STRING)   init = "''";
            else if (t == IRType::LIST)     init = "[]i64{}";
            else                            init = "i64(0)";
            emit(out, indent, "mut " + vn + " := " + init);
            declared.insert(vn);
        }
    }
    bool curFuncIsConstructor_ = false;
    bool curFuncReturnIsVoid_ = false;
    void setReturnIsVoid(bool v) override { returnIsVoid_ = v; }
    bool returnIsVoid_ = false;
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        if (!lastWasReturn && !curFuncReturnIsVoid_)
            emit(out, indent, curFuncIsConstructor_ ? "return self"
                             : curFuncReturnIsFloat_ ? "return f64(0)" : "return i64(0)");
        curFuncReturnIsFloat_ = false; curFuncIsConstructor_ = false; curFuncReturnIsVoid_ = false;
        lastWasReturn = false;
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
        declared.clear();
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        emit(out, indent, "struct " + name + " {");
        indent++;
        emit(out, indent, "mut:");   // fields need to be in a `mut:` block to be assignable
    }
    void emitFieldDecl(std::ostringstream &out, int &indent,
                       const std::string &field, IRType t) override
    {
        std::string ty = t == IRType::STRING ? "string" : t == IRType::FLOAT ? "f64" : "i64";
        emit(out, indent, vName(field) + " " + ty);
    }
    void emitFieldsEnd(std::ostringstream &out, int &indent) override
    {
        indent--;
        emit(out, indent, "}");
        emitRaw(out, "");
    }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        // `init` is emitted as the free function `new_ClassName(args) ClassName` (see
        // emitFunctionBegin) — not a bare `ClassName(args)` call, which V has no syntax for at
        // all (there's no implicit constructor / no bare-struct-name-as-callable in V).
        std::string vn = vName(res);
        bool isNewDecl = declared.insert(vn).second;
        emit(out, indent, (isNewDecl ? "mut " + vn + " := " : vn + " = ")
                          + "new_" + vName(className) + "(" + args + ")");
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
        // A string source parses at the boundary (`'42'.i64()`), not a numeric conversion (which V
        // rejects from a string). Fixes `to_int n = $42$` / `to_dec` on V.
        bool srcIsStr = (!src.empty() && (src.front() == '\'' || src.front() == '"')) || isStringVar(src);
        if      (t == IRType::FLOAT)  expr = srcIsStr ? (src + ".f64()") : ("f64(" + src + ")");
        // V has no cast-type tracking (its decl() wraps every int in i64()), so it can't carry a
        // fixed width through arithmetic — short/mini are advisory i64 on V (as on Python/JS/BNY).
        else if (t == IRType::INT || irIntWidth(t))
                                      expr = srcIsStr ? (src + ".i64()") : ("i64(" + src + ")");
        else if (t == IRType::STRING) expr = src + ".str()";
        else if (t == IRType::BOOL)   expr = "if " + src + " != 0 { i64(1) } else { i64(0) }";
        else if (t == IRType::ATOMIC) {
            expr = srcIsStr ? (src + ".i64()") : ("i64(" + src + ")");
            if (!declared.count(var)) { declared.insert(var); emit(out, indent, "mut " + var + " := i64(0)"); }
            emit(out, indent, "ac_atomic_lock.lock()");
            emit(out, indent, var + " = " + expr);
            emit(out, indent, "ac_atomic_lock.unlock()");
            return;
        }
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
    // AC's IR formats string constants with C-style backslash escapes (`\"`, `\n`, ...) — every
    // other backend's target language (Python/C/JS/Java/...) shares that convention natively, so
    // the quoted text is emitted as-is. NASM does NOT: its plain `"..."`/`'...'` string delimiters
    // are LITERAL (a `\` is just a `\` character, meaning a `\"` inside one actually terminates the
    // string early) — only the backtick `` `...` `` delimiter enables C-style escape processing.
    // Emitting these into `.data` with the original double-quote delimiter (as every db-emitting
    // call site here originally did) is therefore a straight syntax error the instant a string
    // contains a quote/backslash/newline (verified: applicant_form.ac's JSON-building strings like
    // `{\"name\":\"` — NASM: "unterminated string" / "comma expected after operand"). Swapping the
    // delimiter is a content-preserving fix: same escape syntax, just interpreted correctly.
    static std::string toNasmDbLit(const std::string &quoted) {
        if (quoted.size() < 2 || quoted.front() != '"' || quoted.back() != '"') return quoted;
        return "`" + quoted.substr(1, quoted.size() - 2) + "`";
    }
    std::vector<std::pair<std::string,std::string>> pendingImports_;
    std::set<std::string> definedFuncs_;   // labels we emit a body for (fib, main, …)
    std::set<std::string> calledFuncs_;     // every `call X` target — auto-externed if undefined
    // widgets ilib: `use ilib widgets` exposes bare-name constructor calls (`display(root, ...)`,
    // `btn(root, ...)`) and dot-method calls on the returned handle (`lang_drop.add(...)`) — every
    // other backend (CStrategy's cWidgetCtor/cWidgetMethod is the reference this ports) expands
    // these into the real `ac_widgets_*_new`/`_pack`/`_set`/`_get`/`_add` FFI calls. ASM had NONE
    // of this: emitCall's generic fallback emitted a bare `call display`/`call lang_drop_add` —
    // symbols that exist nowhere (not even in the .so, which only exports `ac_widgets_display_new`
    // etc) — a hard link error for every widgets_test*.ac file, previously undetected because
    // `--all`/`--no-run` never actually assembles+links ASM output (see BNY's identical
    // "undefined label 'display'" — same root cause, different backend).
    std::map<std::string, std::string> widgetVars_;   // var name -> widget constructor kind
    std::map<std::string, int> userFuncArity_;
    void setUserFuncArity(const std::map<std::string, int>& m) override { userFuncArity_ = m; }
    static bool isWidgetCtor(const std::string& func) {
        static const std::set<std::string> ctors = {
            "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
            "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch"
        };
        return ctors.count(func) > 0;
    }
    static std::string packFnFor(const std::string& kind) {
        if (kind == "Screen") return "";
        if (kind == "radbtn") return "ac_widgets_ckbtn_pack";
        return "ac_widgets_" + kind + "_pack";
    }
    static std::string getFnFor(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_get";
        return "ac_widgets_" + kind + "_get";
    }
    static std::string setFnFor(const std::string& kind) {
        if (kind == "radbtn") return "ac_widgets_ckbtn_set";
        return "ac_widgets_" + kind + "_set";
    }
    std::vector<std::string> splitCallArgs(const std::string &args) {
        std::vector<std::string> out;
        std::string cur; int depth = 0; bool inStr = false;
        for (char c : args) {
            if (c == '"') inStr = !inStr;
            if (!inStr && (c == '(' || c == '[')) depth++;
            if (!inStr && (c == ')' || c == ']')) depth--;
            if (c == ',' && depth == 0 && !inStr) { out.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        for (auto &s : out) {
            size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
            s = (a != std::string::npos) ? s.substr(a, b - a + 1) : s;
        }
        return out;
    }
    // Load a resolved arg list into rdi/rsi/rdx/rcx/r8/r9 and call `fn`, storing rax in `res`
    // (unless res is empty). Shared by the ctor-new call and every method-call variant below.
    void asmCallSimple(std::ostringstream &out, const std::string &fn,
                        const std::vector<std::string> &callArgs, const std::string &res) {
        static const char *argRegs[6] = {"rdi","rsi","rdx","rcx","r8","r9"};
        for (size_t i = 0; i < callArgs.size() && i < 6; i++) {
            loadRAX(out, callArgs[i]);
            out << "    mov " << argRegs[i] << ", rax\n";
        }
        calledFuncs_.insert(fn);
        out << "    call " << fn << "\n";
        if (!res.empty()) storeRAX(out, res);
    }
    bool asmWidgetCtor(std::ostringstream &out, const std::string &res,
                       const std::string &func, const std::vector<std::string> &a) {
        if (!isWidgetCtor(func) || res.empty()) return false;
        auto arg = [&](size_t i, const std::string &def) { return i < a.size() ? a[i] : def; };
        std::string newFn;
        std::vector<std::string> callArgs;
        if (func == "Screen") { newFn = "ac_widgets_screen_new"; callArgs = { arg(0, "\"AC App\""), arg(1, "\"800x600\"") }; }
        else if (func == "display") { newFn = "ac_widgets_display_new"; callArgs = { arg(0, "0"), arg(1, "\"\"") }; }
        else if (func == "ask") { newFn = "ac_widgets_ask_new"; callArgs = { arg(0, "0"), arg(1, "20") }; }
        else if (func == "btn") { newFn = "ac_widgets_btn_new"; callArgs = { arg(0, "0"), arg(1, "\"Button\"") }; }
        else if (func == "ckbtn" || func == "radbtn") { newFn = "ac_widgets_ckbtn_new"; callArgs = { arg(0, "0"), arg(1, "\"\"") }; }
        else if (func == "dropdown") { newFn = "ac_widgets_dropdown_new"; callArgs = { arg(0, "0") }; }
        else if (func == "advance") { newFn = "ac_widgets_advance_new"; callArgs = { arg(0, "0"), arg(1, "200") }; }
        else if (func == "slider") { newFn = "ac_widgets_slider_new"; callArgs = { arg(0, "0"), arg(1, "0"), arg(2, "100"), arg(3, "\"horizontal\"") }; }
        else if (func == "group") { newFn = "ac_widgets_group_new"; callArgs = { arg(0, "0"), arg(1, "\"\"") }; }
        else if (func == "tabs") { newFn = "ac_widgets_tabs_new"; callArgs = { arg(0, "0") }; }
        else if (func == "scroller") { newFn = "ac_widgets_scroller_new"; callArgs = { arg(0, "0"), arg(1, "\"vertical\"") }; }
        else if (func == "listbox") { newFn = "ac_widgets_listbox_new"; callArgs = { arg(0, "0"), arg(1, "20"), arg(2, "10") }; }
        else if (func == "table") { newFn = "ac_widgets_table_new"; callArgs = { arg(0, "0"), arg(1, "\"\""), arg(2, "10") }; }
        else if (func == "sketch") { newFn = "ac_widgets_sketch_new"; callArgs = { arg(0, "0"), arg(1, "300"), arg(2, "200") }; }
        else return false;
        asmCallSimple(out, newFn, callArgs, res);
        widgetVars_[res] = func;
        std::string packFn = packFnFor(func);
        if (!packFn.empty()) asmCallSimple(out, packFn, {res}, "");
        if (func == "btn" && a.size() > 2) {
            std::string cb = a[2];
            auto ar = userFuncArity_.find(cb);
            std::string adapter = (ar != userFuncArity_.end() && ar->second > 0)
                                  ? "_ac_widget_call1" : "_ac_widget_call0";
            loadRAX(out, res);
            out << "    mov rdi, rax\n";
            out << "    lea rsi, [rel " << adapter << "]\n";
            out << "    lea rdx, [rel " << cb << "]\n";
            calledFuncs_.insert("ac_widgets_btn_on_click");
            out << "    call ac_widgets_btn_on_click\n";
        }
        return true;
    }
    bool asmWidgetMethod(std::ostringstream &out, const std::string &res,
                         const std::string &func, const std::string &argsRaw) {
        // The last underscore is NOT always the receiver/method boundary when the method name
        // itself contains one (add_tab, text_at) — try every underscore left to right, taking
        // the first prefix that's a known widget var (mirrors CStrategy's cWidgetMethod fix;
        // see its comment for the "undefined reference to `tabber_add_tab'" bug this closes).
        std::string recv, method; bool found = false;
        for (size_t p = func.find('_'); p != std::string::npos && p != 0; p = func.find('_', p + 1)) {
            std::string cand = func.substr(0, p);
            if (widgetVars_.count(cand)) { recv = cand; method = func.substr(p + 1); found = true; break; }
        }
        if (!found) return false;
        auto wit = widgetVars_.find(recv);
        if (wit == widgetVars_.end()) return false;
        const std::string& kind = wit->second;
        std::vector<std::string> a = splitCallArgs(argsRaw);
        if (method == "pack") {
            if (a.size() >= 2) asmCallSimple(out, "ac_widgets_pack_spaced", {recv, a[0], a[1]}, "");
            else asmCallSimple(out, packFnFor(kind), {recv}, "");
            return true;
        }
        if (method == "mainloop" && kind == "Screen") { asmCallSimple(out, "ac_widgets_screen_mainloop", {recv}, ""); return true; }
        if (method == "update" && kind == "Screen")   { asmCallSimple(out, "ac_widgets_screen_update", {recv}, ""); return true; }
        if (method == "destroy" && kind == "Screen")  { asmCallSimple(out, "ac_widgets_screen_destroy", {recv}, ""); return true; }
        if (method == "add") {
            std::string fn = (kind == "dropdown" ? "ac_widgets_dropdown_add" :
                              kind == "listbox"  ? "ac_widgets_listbox_add"  :
                              kind == "table"    ? "ac_widgets_table_add"    : "ac_widgets_add");
            std::vector<std::string> callArgs = {recv};
            for (auto &x : a) callArgs.push_back(x);
            asmCallSimple(out, fn, callArgs, "");
            return true;
        }
        if (method == "set" || method == "config") {
            std::string val = a.empty() ? "\"\"" : a[0];
            asmCallSimple(out, setFnFor(kind), {recv, val}, "");
            return true;
        }
        if (method == "get") {
            std::string fn = getFnFor(kind);
            asmCallSimple(out, fn, {recv}, res);
            if (!res.empty() && (kind == "ask" || kind == "display" || kind == "dropdown"))
                strVars_.insert(res);
            return true;
        }
        if (method == "on_click" && kind == "btn" && !a.empty()) {
            std::string cb = a[0];
            auto ar = userFuncArity_.find(cb);
            std::string adapter = (ar != userFuncArity_.end() && ar->second > 0)
                                  ? "_ac_widget_call1" : "_ac_widget_call0";
            loadRAX(out, recv);
            out << "    mov rdi, rax\n";
            out << "    lea rsi, [rel " << adapter << "]\n";
            out << "    lea rdx, [rel " << cb << "]\n";
            calledFuncs_.insert("ac_widgets_btn_on_click");
            out << "    call ac_widgets_btn_on_click\n";
            return true;
        }
        // tabs.add_tab / sketch's drawing methods — mirrors CStrategy's cWidgetMethod (see its
        // matching comment for the "undefined reference to `tabber_add_tab'" etc bug this closes).
        if (method == "add_tab" && kind == "tabs" && !a.empty()) {
            asmCallSimple(out, "ac_widgets_tabs_add_tab", {recv, a[0]}, res);
            return true;
        }
        if (method == "clear" && kind == "sketch") {
            asmCallSimple(out, "ac_widgets_sketch_clear", {recv}, "");
            return true;
        }
        // ac_widgets_sketch_{line,rect,circle,text}(h, doubles..., ints...) — SysV assigns
        // float-class (XMM) and int-class (RDI/RSI/...) args independently; h always takes RDI.
        if ((method == "line" || method == "rect") && kind == "sketch" && a.size() >= 7) {
            loadRAX(out, recv); out << "    mov rdi, rax\n";
            static const char* xmm[4] = {"xmm0","xmm1","xmm2","xmm3"};
            for (int k = 0; k < 4; k++) loadDouble(out, a[k], xmm[k]);
            static const char* ir[3] = {"rsi","rdx","rcx"};
            for (int k = 0; k < 3; k++) { loadRAX(out, a[4 + k]); out << "    mov " << ir[k] << ", rax\n"; }
            std::string fn = method == "line" ? "ac_widgets_sketch_line" : "ac_widgets_sketch_rect";
            calledFuncs_.insert(fn); out << "    call " << fn << "\n";
            return true;
        }
        if (method == "circle" && kind == "sketch" && a.size() >= 6) {
            loadRAX(out, recv); out << "    mov rdi, rax\n";
            static const char* xmm[3] = {"xmm0","xmm1","xmm2"};
            for (int k = 0; k < 3; k++) loadDouble(out, a[k], xmm[k]);
            static const char* ir[3] = {"rsi","rdx","rcx"};
            for (int k = 0; k < 3; k++) { loadRAX(out, a[3 + k]); out << "    mov " << ir[k] << ", rax\n"; }
            calledFuncs_.insert("ac_widgets_sketch_circle"); out << "    call ac_widgets_sketch_circle\n";
            return true;
        }
        if (method == "text_at" && kind == "sketch" && a.size() >= 6) {
            loadRAX(out, recv); out << "    mov rdi, rax\n";
            loadDouble(out, a[0], "xmm0"); loadDouble(out, a[1], "xmm1");
            static const char* ir[4] = {"rsi","rdx","rcx","r8"};
            for (int k = 0; k < 4; k++) { loadRAX(out, a[2 + k]); out << "    mov " << ir[k] << ", rax\n"; }
            calledFuncs_.insert("ac_widgets_sketch_text"); out << "    call ac_widgets_sketch_text\n";
            return true;
        }
        return false;
    }
    // Exact per-function stack-frame sizing (see emitFunctionBegin's comment for the full
    // rationale). A placeholder token is emitted in place of the literal `sub rsp, N` at
    // prologue time (the real size isn't known until the body finishes); recordFrameSize()
    // captures nextSlot's final value at emitFunctionEnd/emitMainEnd, and postProcess() swaps
    // every placeholder for its real value in one pass over the fully-assembled output text.
    std::string frameLabel_;
    std::map<std::string, int> funcFrameSize_;   // label -> final byte size
    static std::string frameToken(const std::string &label) { return "$$ASMFRAME$$" + label + "$$"; }
    void recordFrameSize(const std::string &label) {
        int bytes = ((nextSlot + 15) / 16) * 16;   // 16-byte align (SysV ABI stack alignment)
        if (bytes < 64) bytes = 64;                // safety margin for any slots this pass missed
        funcFrameSize_[label] = bytes;
    }
    // Parses a line of the exact shape `    mov [rbp-N], REG` (a storeRAX/storeXMM0 spill) into
    // (N, REG). Returns false for anything else (including the mirror-image load shape below —
    // deliberately strict, since the peephole below only ever needs to recognize this ONE exact
    // spill shape to find its matching reload).
    static bool parseSpillStore(const std::string &line, std::string &slot, std::string &reg) {
        const std::string pfx = "    mov [rbp-";
        if (line.rfind(pfx, 0) != 0) return false;
        size_t p = pfx.size();
        size_t bracketEnd = line.find("], ", p);
        if (bracketEnd == std::string::npos) return false;
        slot = line.substr(p, bracketEnd - p);
        if (slot.empty() || slot.find_first_not_of("0123456789") != std::string::npos) return false;
        reg = line.substr(bracketEnd + 3);
        return !reg.empty();
    }
    // A peephole eliminating a REAL, verified redundancy: storeRAX/storeXMM0 immediately
    // followed by loadRAX/loadXMM0 reloading the EXACT SAME value from the EXACT SAME slot into
    // the EXACT SAME register it was just spilled from — a pure no-op round-trip through memory
    // (verified: examples/keyword_catalog_core.ac and any WHILST loop body had several of these
    // per iteration — every "store this temp's result, then immediately need it again for the
    // next operation" step in the generic dispatcher does exactly this). Only ever folds two
    // TEXTUALLY ADJACENT lines with an EXACT slot+register match — no cross-label or
    // cross-jump reasoning, so there's no risk of assuming a value survived past a point where
    // control flow could have changed it.
    static std::string foldDeadReloads(const std::string &s) {
        std::vector<std::string> lines;
        { std::string cur; for (char c : s) { if (c == '\n') { lines.push_back(cur); cur.clear(); } else cur += c; }
          if (!cur.empty()) lines.push_back(cur); }
        std::ostringstream out;
        for (size_t i = 0; i < lines.size(); i++) {
            std::string slot, reg;
            if (i + 1 < lines.size() && parseSpillStore(lines[i], slot, reg)) {
                std::string reloadLine = "    mov " + reg + ", [rbp-" + slot + "]";
                if (lines[i + 1] == reloadLine) {
                    out << lines[i] << "\n";
                    i++;   // skip the redundant reload
                    continue;
                }
            }
            out << lines[i] << "\n";
        }
        std::string r = out.str();
        if (!r.empty() && r.back() == '\n' && (s.empty() || s.back() != '\n')) r.pop_back();
        return r;
    }
    std::string postProcess(const std::string &s) override {
        std::string out = s;
        for (auto &[label, bytes] : funcFrameSize_) {
            std::string token = frameToken(label);
            std::string val = std::to_string(bytes);
            size_t pos = 0;
            while ((pos = out.find(token, pos)) != std::string::npos) {
                out.replace(pos, token.size(), val);
                pos += val.size();
            }
        }
        return foldDeadReloads(out);
    }
    std::set<std::string> atomicVars_;      // vars declared `atomic` — real pthread_mutex-guarded
    void setVarCastTypes(const std::map<std::string, IRType>& m) override {
        for (auto& [k, v] : m) if (v == IRType::ATOMIC) atomicVars_.insert(k);
    }
    bool anyAtomicVars() const { return !atomicVars_.empty(); }
    // Every other backend gets this via the shared detectFloatVars() fixpoint pass (see its
    // definition below) and consumes it in setFloatVarsFull — ASM never overrode the hook, so
    // the analysis ran and its result was silently discarded: floats were completely
    // unimplemented here (a bare float literal like `Term.display 3.5` didn't even ASSEMBLE —
    // `mov rax, 3.5` isn't valid NASM for a general-purpose register, since GPRs can't hold a
    // non-integer immediate). Fixed below: float values are stored as their raw 8-byte IEEE754
    // bit pattern in the SAME rbp-relative stack slots every other value uses (loadRAX/storeRAX
    // move the bits around unexamined), and reinterpreted through XMM0/XMM1 only at the point an
    // actual float OPERATION (arithmetic, compare, cast, print) needs to happen — this fits the
    // existing pure-GPR/stack-slot architecture without restructuring it.
    std::set<std::string> floatVars_;
    void setFloatVarsFull(const std::set<std::string>& s) override { floatVars_ = s; }
    bool isFloatVal(const std::string &v) const { return floatVars_.count(v) > 0 || looksFloat(v); }
    // Same gap as floatVars_ above, one level up: the shared pre-pass already knows WHICH
    // user-defined functions return a float (e.g. `mean` after emitTrueDivision's fix below makes
    // `/` a real float op) and hands it to every backend via setFloatReturnFuncs — every other
    // strategy consults it in emitCall to tag the call's result temp as float. ASM never
    // implemented either half, so a float-returning function's result printed as the raw double
    // bit pattern reinterpreted as a huge integer (verified: 5.0 printed as 4617315517961601024)
    // — the VALUE was already correct (storeXMM0/loadRAX move the same 8 bytes through RAX either
    // way), only the caller-side "this temp is a double" bookkeeping was missing.
    std::set<std::string> userFloatFuncs_;
    void setFloatReturnFuncs(const std::set<std::string>& s) override { userFloatFuncs_ = s; }
    bool isUserFloatReturningFunc(const std::string &fn) const { return userFloatFuncs_.count(fn) > 0; }
    // The shared stringVars_ (base class, populated externally via setStringVars) only tracks
    // NAMED variables — its detectStringVars ADD-case requires `Kind::VAR`, so a concat result
    // that's never assigned to a name (`Term.display label + "!"` — an anonymous print-expression
    // TEMP) is never in it. Mirrors CStrategy's own local `strVars` for exactly this reason:
    // tracked inline, at the point a string VALUE is actually produced (emitStrConcat's result,
    // emitTypeCast's STRING branch), so a later isStr(...) check on that temp's name succeeds.
    std::set<std::string> strVars_;
    bool isStr(const std::string &v) const { return looksString(v) || strVars_.count(v) > 0 || isStringVar(v); }
    // Arrays: NO codegen existed at all before this (emitAlloc/emitLoadIndex/emitStoreIndex all
    // silently no-op'd via BackendStrategy's base defaults) — `xs = [1,2,3]` compiled to nothing,
    // `xs[1]` read uninitialized stack garbage, and a `FOR c in $string$` loop (which lowers
    // through this SAME LOAD_INDEX/"__len__" mechanism on low-level backends) hung forever
    // printing "0" (verified: 5GB of output before being killed — `lenT` never got a real value
    // since emitLoadIndex did nothing). Layout: a malloc'd block, 8-byte length at [0], elements
    // at [8], [16], ... — simple, matches BNY's own array layout. Unlike BNY's raw single-register
    // byte emitter, ASM generates NASM TEXT, so real `[base + 8 + idx*8]` indexed addressing is
    // available directly — no manual pointer-walking loops needed here.
    std::set<std::string> listVars_;
    // Per-dict-var set of keys whose literal value is a `$..$` string — a dict block is
    // heterogeneous per-slot at runtime (see _ac_dict_new's comment), so a LOAD_INDEX with a
    // compile-time-constant string key is the only place codegen can know whether to mark its
    // result string-typed (strVars_) for correct Term.display formatting.
    std::map<std::string, std::set<std::string>> dictStrKeys_;
    // listVars_ only ever grows from a LOCAL `xs = [1,2,3]` literal (emitAlloc). A list-typed
    // function PARAMETER never goes through emitAlloc, so `length arr`/`arr.append(v)`/print on
    // a bare array param fell through to the string/scalar default and produced wrong output
    // (verified: `find(arr, target)` calling `length arr` emitted `call strlen` on a pointer that
    // was never a C string, returning garbage). listParams_/listGlobals_ are the base class's own
    // already-correct pre-scanned answer to "is this name a list" for params/globals (every other
    // strategy already consults them, e.g. line ~2950) — ASM just never checked them.
    bool isListVar(const std::string &v) const {
        return listVars_.count(v) > 0 || listParams_.count(v) > 0 || listGlobals_.count(v) > 0;
    }
    // Bundle/class: like arrays, NO codegen existed before this. What WAS generated (method
    // labels `ClassName_method`, self passed as a real pointer param) looked plausible but was
    // fundamentally broken underneath: `self.field = x` inside a method just wrote to a LOCAL
    // stack slot named "self.field" (via the default flatten-dots getSlot() path) — completely
    // disconnected from the actual object the caller allocated, discarded the instant the
    // function returned. `c = Critter()` called a nonexistent `Critter` label (only
    // `Critter_init` existed — a hard link error). `c.hp` read random uninitialized stack.
    // Fixed by mirroring CStrategy's own proven approach (see its `formatRef`/`decl` comments)
    // for the free-function/self-pointer method-call convention ASM already shares with C
    // (`dotCallSyntax() == false`): a real malloc'd object (8 bytes/field, in field-declaration
    // order), `self.field`/`instance.field` resolved to `[pointer + offset]` via `formatRef`
    // preserving the dot (instead of flattening it to a disconnected `self_field`) plus
    // loadRAX/storeRAX interception that turns that dotted name into real pointer+offset
    // addressing — NASM text generation makes `[reg + offset]` trivial, unlike BNY's raw
    // single-register byte emitter.
    std::string currentClass_;
    std::map<std::string, std::vector<std::string>> classFields_;   // className -> ordered field names
    std::map<std::string, std::string> instanceClass_;              // instance var name -> className
    int fieldOffset(const std::string &className, const std::string &field) const
    {
        auto it = classFields_.find(className);
        if (it == classFields_.end()) return -1;
        for (size_t i = 0; i < it->second.size(); i++)
            if (it->second[i] == field) return (int)(8 * i);
        return -1;
    }
    // Resolves a possibly-dotted name to (basePtrVarName, byteOffset) if it's a recognized
    // self.field / instance.field access; returns false (leave `val` alone) otherwise.
    bool resolveFieldAccess(const std::string &val, std::string &base, int &offset) const
    {
        auto dot = val.find('.');
        if (dot == std::string::npos) return false;
        base = val.substr(0, dot);
        std::string field = val.substr(dot + 1);
        std::string cls = (base == "self") ? currentClass_
                         : (instanceClass_.count(base) ? instanceClass_.at(base) : std::string());
        if (cls.empty()) return false;
        offset = fieldOffset(cls, field);
        return offset >= 0;
    }
    // try/catch label bookkeeping (see emitHeader's `_ac_try_stack` comment for the full design).
    std::vector<int> tryStack_;
    int tryIdx_ = 0;
    int lastTryIdx_ = -1;
    bool afterPending_ = false;
    int divGuardIdx_ = 0;
    // Shared by both `/` and `%` (both use `idiv`, both can trap on a zero divisor). Assumes RBX
    // already holds the divisor — matches every idiv call site's existing convention.
    void emitDivZeroGuard(std::ostringstream &out)
    {
        int g = divGuardIdx_++;
        out << "    test rbx, rbx\n    jnz .divok" << g << "\n";
        out << "    mov rax, [rel _ac_try_depth]\n    test rax, rax\n    jz .divfatal" << g << "\n";
        out << "    dec rax\n    imul rax, rax, 200\n";
        out << "    lea rdi, [rel _ac_try_stack]\n    add rdi, rax\n    mov esi, 1\n    call longjmp\n";
        calledFuncs_.insert("longjmp");
        out << ".divfatal" << g << ":\n";
        out << "    lea rdi, [rel _msg_divzero]\n    xor eax, eax\n    call printf\n";
        out << "    mov edi, 1\n    call exit\n";
        out << ".divok" << g << ":\n";
    }
    bool needsEvents_ = false;
    bool needsSave_ = false;
    void setNeedsSave(bool v) override { needsSave_ = v; }
    // Fixed 64KB buffer (matches this backend's other fixed-size choices — the 64-slot event
    // table, 32-slot try stack — plenty for AC's toy-scale programs; a real growable buffer
    // would need realloc bookkeeping this backend has no precedent for elsewhere).
    // `_ac_save_append(char* text)`: strcpy's onto the end of the buffer, then a newline byte,
    // updating `_ac_save_len` — this IS the "append" logic; every emitCapture call site converts
    // its value to a C-string first, then jumps here.
    void emitCapture(std::ostringstream &out, int &indent, const std::string &val) override
    {
        (void)indent;
        if (!needsSave_) return;
        if (isStr(val)) {
            loadRAX(out, val);
        } else if (isFloatVal(val)) {
            // Reuse the exact same %g+dot-fix buffer logic as emitPrint's float branch.
            out << "    mov rdi, 32\n    call malloc\n"; calledFuncs_.insert("malloc");
            out << "    mov rbx, rax\n";
            out << "    mov rdi, rbx\n    mov rsi, 32\n    lea rdx, [rel _fmt_fbare]\n";
            loadXMM0(out, val);
            out << "    mov al, 1\n    call snprintf\n"; calledFuncs_.insert("snprintf");
            out << "    mov r12, rax\n";
            out << "    mov rdi, rbx\n    lea rsi, [rel _fmt_dotcheck]\n    call strpbrk\n"; calledFuncs_.insert("strpbrk");
            int idx = fltPrintIdx_++;
            out << "    test rax, rax\n    jnz .savehavedot" << idx << "\n";
            out << "    mov byte [rbx + r12], '.'\n    mov byte [rbx + r12 + 1], '0'\n    mov byte [rbx + r12 + 2], 0\n";
            out << ".savehavedot" << idx << ":\n";
            out << "    mov rax, rbx\n";
        } else {
            out << "    mov rdi, 24\n    call malloc\n"; calledFuncs_.insert("malloc");
            out << "    mov rbx, rax\n";
            loadRAX(out, val);
            out << "    mov rdi, rbx\n    mov rsi, 24\n    lea rdx, [rel _fmt_dbare]\n    mov rcx, rax\n    xor eax, eax\n    call snprintf\n";
            calledFuncs_.insert("snprintf");
            out << "    mov rax, rbx\n";
        }
        out << "    mov rdi, rax\n    call _ac_save_append\n";
    }
    void emitSaveFile(std::ostringstream &out, int &indent, const std::string &filename) override
    {
        (void)indent;
        // `filename` is the raw quoted-literal text (e.g. `"saved_out.txt"`) — NOT a NASM label;
        // it can't go directly into `[rel ...]`. Every other string use in this backend
        // materializes a literal into `.data` first via `loadRAX` (which already does exactly
        // that for a `looksString` operand) — using it here too instead of a bare `[rel filename]`
        // fixes a hard NASM syntax error ("character constant too long").
        loadRAX(out, filename);
        out << "    mov rdi, rax\n    lea rsi, [rel _fmt_wmode]\n    call fopen\n";
        calledFuncs_.insert("fopen");
        out << "    test rax, rax\n    jz .savedone" << fltPrintIdx_ << "\n";
        out << "    mov rbx, rax\n";
        out << "    lea rdi, [rel _ac_save_buf]\n    mov rsi, 1\n    mov rdx, [rel _ac_save_len]\n    mov rcx, rbx\n    call fwrite\n";
        calledFuncs_.insert("fwrite");
        out << "    mov rdi, rbx\n    call fclose\n";
        calledFuncs_.insert("fclose");
        out << ".savedone" << fltPrintIdx_++ << ":\n";
    }
    void setNeedsEvents(bool v) override { needsEvents_ = v; }
    // `key` arrives as a quoted literal (e.g. "space") — same on-demand .data-label pattern
    // emitPrint already uses for string literals (strIdx/dataSec), reused here so both call
    // sites (bind + trigger, possibly the same key string more than once) get real labels.
    std::string internKeyString(const std::string &key) {
        if (!looksString(key)) return key;   // already a label/identifier — pass through
        std::string lbl = "_str" + std::to_string(strIdx++);
        dataSec.push_back(lbl + " db " + toNasmDbLit(key) + ", 0");
        return lbl;
    }
    void emitEventBind(std::ostringstream &out, int & /*indent*/,
                       const std::string &key, const std::string &callback) override
    {
        out << "    lea rdi, [rel " << internKeyString(key) << "]\n";
        out << "    lea rsi, [rel " << callback << "]\n";
        out << "    call _ac_bind\n";
    }
    void emitEventTrigger(std::ostringstream &out, int & /*indent*/,
                          const std::string &key) override
    {
        out << "    lea rdi, [rel " << internKeyString(key) << "]\n";
        out << "    call _ac_trigger\n";
    }

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
        // The shared dispatcher builds instance-method call args as "&" + instVar (correct for
        // CStrategy's model, where an instance is a real stack-allocated VALUE and `&c` takes
        // its address to get the pointer `ClassName_method` expects) — ASM's bundle instances
        // are already malloc'd pointers (see emitConstructCall), so `c`'s own slot ALREADY holds
        // the pointer `Critter_greet` needs; treating "&c" as a NEW, never-declared variable
        // name (silently allocating an unrelated fresh/uninitialized slot) is what caused a
        // segfault — `c.greet()` read garbage instead of the real object pointer.
        if (val.size() > 1 && val[0] == '&') { loadRAX(out, val.substr(1)); return; }
        std::string fbase; int foff;
        if (resolveFieldAccess(val, fbase, foff)) {
            out << "    mov rax, [rbp-" << getSlot(fbase) << "]\n";  // rax = object pointer
            out << "    mov rax, [rax+" << foff << "]\n";
            return;
        }
        if (looksFloat(val))
        {
            // A float literal can't be a `mov reg, imm` operand at all (NASM rejects a
            // non-integer immediate into a GPR) — stash its raw IEEE754 bits in .data (NASM's
            // `dq` directive accepts a float literal directly and computes the bit pattern) and
            // load those 8 bytes as-is; nothing downstream cares that they're "really" a double
            // until an actual float op reinterprets them via movq into XMM.
            std::string lbl = "_flt" + std::to_string(strIdx++);
            dataSec.push_back(lbl + " dq " + val);
            out << "    mov rax, [rel " << lbl << "]\n";
        }
        else if (looksNumeric(val))
        {
            out << "    mov rax, " << val << "\n";
        }
        else if (looksString(val))
        {
            std::string lbl = "_str" + std::to_string(strIdx++);
            dataSec.push_back(lbl + " db " + toNasmDbLit(val) + ", 0");
            out << "    lea rax, [rel " << lbl << "]\n";
        }
        else
        {
            out << "    mov rax, [rbp-" << getSlot(val) << "]\n";
        }
    }

    void storeRAX(std::ostringstream &out, const std::string &dst)
    {
        std::string fbase; int foff;
        if (resolveFieldAccess(dst, fbase, foff)) {
            out << "    mov rbx, rax\n";                          // rbx = value to store (rax about to be reused)
            out << "    mov rax, [rbp-" << getSlot(fbase) << "]\n"; // rax = object pointer
            out << "    mov [rax+" << foff << "], rbx\n";
            return;
        }
        out << "    mov [rbp-" << getSlot(dst) << "], rax\n";
    }

    // Reinterpret a value's raw bit-slot as a double in XMM0/XMM1 for an actual float op. Only
    // valid when `val` is ALREADY known to hold real double bits (e.g. after emitTypeCast's
    // FLOAT branch, or a var already in floatVars_) — for a value that might still be a genuine
    // int (mixed-type arithmetic operand), use loadDouble() below instead, which converts.
    void loadXMM0(std::ostringstream &out, const std::string &val) { loadRAX(out, val); out << "    movq xmm0, rax\n"; }
    void loadXMM1(std::ostringstream &out, const std::string &val) { loadRAX(out, val); out << "    movq xmm1, rax\n"; }
    // Store XMM0's bits back into a slot via the same RAX bit-slot convention.
    void storeXMM0(std::ostringstream &out, const std::string &dst) { out << "    movq rax, xmm0\n"; storeRAX(out, dst); }
    // Get a proper double VALUE into an XMM register from any operand — an int literal/var used
    // in a mixed-type float expression (`d + 3`, float var + int var) needs a real int-to-double
    // conversion (cvtsi2sd), NOT a bit-reinterpret (movq) — those bits aren't a double at all.
    void loadDouble(std::ostringstream &out, const std::string &val, const std::string &xmmReg)
    {
        if (looksFloat(val)) {
            std::string lbl = "_flt" + std::to_string(strIdx++);
            dataSec.push_back(lbl + " dq " + val);
            out << "    movsd " << xmmReg << ", [rel " << lbl << "]\n";
        } else if (looksNumeric(val)) {
            out << "    mov rax, " << val << "\n    cvtsi2sd " << xmmReg << ", rax\n";
        } else if (isFloatVal(val)) {
            out << "    mov rax, [rbp-" << getSlot(val) << "]\n    movq " << xmmReg << ", rax\n";
        } else {
            out << "    mov rax, [rbp-" << getSlot(val) << "]\n    cvtsi2sd " << xmmReg << ", rax\n";
        }
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
        emitRaw(out, "    extern printf, exit, abort, fflush, stderr, setjmp, longjmp");
        if (anyAtomicVars())
            emitRaw(out, "    extern pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock");
        if (needsEvents_)
            emitRaw(out, "    extern strcmp");
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
        // %.16g, not bare %g (6 sig figs) — matches every other backend's _ac_dblprint/ac_fstr
        // convention (CStrategy etc.); plain %g was truncating (verified: 7.0/3.0 printed as
        // "2.33333" instead of "2.333333333333333").
        emitRaw(out, "    _fmt_f db \"%.16g\", 10, 0");     // float print (Term.display of a float var)
        emitRaw(out, "    _fmt_dbare db \"%lld\", 0");      // int -> string (to_string), no trailing newline
        emitRaw(out, "    _fmt_fbare db \"%.16g\", 0");     // float -> string (to_string), no trailing newline
        emitRaw(out, "    _fmt_dotcheck db \".eEnN\", 0");  // strpbrk charset: is a %g result already float-shaped?
        emitRaw(out, "    _msg_divzero db \"Preposterous: 3rd grade mathematics violated (ZeroDivisionError)\", 10, 0");
        if (needsSave_) emitRaw(out, "    _fmt_wmode db \"w\", 0");
        emitRaw(out, "");
        // Real try/catch via setjmp/longjmp (mirrors CStrategy's proven `_ac_try_stack`/
        // `_ac_try_depth` design exactly — see its header comment). Previously ASM's
        // emitTryBegin/emitCatchBegin/emitAfterBegin were pure comments ("ASM: no native
        // exception support", an honestly-labeled but total no-op) AND, separately, integer
        // divide-by-zero had NO check at all anywhere — not even the "print message and exit"
        // fallback every other backend has — so `10 // 0` was a raw hardware SIGFPE crash
        // (verified: `try Term.display 10 // 0 catch ...` core-dumped instead of printing
        // "caught..."). The div-by-zero the try was meant to catch usually happens inside a
        // CALLED FUNCTION (e.g. `risky(0)` → `10 // d`), not the same function as the `try` —
        // a compile-time-known local jump can't cross that function-call boundary, so this
        // needs the real cross-frame unwind setjmp/longjmp provides, not a shortcut.
        emitRaw(out, "section .bss");
        emitRaw(out, "    _ac_try_stack resb 6400");   // 32 slots * 200 bytes/jmp_buf (glibc x86-64)
        emitRaw(out, "    _ac_try_depth resq 1");
        emitRaw(out, "");
        if (anyAtomicVars()) {
            // `atomic` vars: a real pthread_mutex_t (sizeof == 40 on x86-64 Linux glibc),
            // initialized via a real pthread_mutex_init() call in main's prologue rather than
            // relying on PTHREAD_MUTEX_INITIALIZER's exact zero-byte layout (glibc-version-
            // dependent) — any op touching an atomic var is wrapped lock/unlock around it.
            emitRaw(out, "section .bss");
            emitRaw(out, "    _ac_atomic_lock resb 40");
            emitRaw(out, "");
        }
        if (needsSave_) {
            // `save as <file>` — fixed 64KB accumulator buffer (same fixed-size-is-fine choice
            // as the try-stack/event-table above), appended to by `_ac_save_append` on every
            // Term.display (see emitCapture), written out by emitSaveFile via fopen/fwrite/fclose.
            emitRaw(out, "section .bss");
            emitRaw(out, "    _ac_save_buf resb 65536");
            emitRaw(out, "    _ac_save_len resq 1");
            emitRaw(out, "");
            emitRaw(out, "section .text");
            definedFuncs_.insert("_ac_save_append");
            calledFuncs_.insert("strlen"); calledFuncs_.insert("strcpy");
            calledFuncs_.insert("fopen"); calledFuncs_.insert("fwrite"); calledFuncs_.insert("fclose");
            emitRaw(out, "_ac_save_append:");            // rdi = C-string to append
            emitRaw(out, "    push rbx");
            emitRaw(out, "    push r12");
            emitRaw(out, "    mov r12, rdi");
            emitRaw(out, "    call strlen");
            emitRaw(out, "    mov rbx, rax");
            emitRaw(out, "    lea rdi, [rel _ac_save_buf]");
            emitRaw(out, "    add rdi, [rel _ac_save_len]");
            emitRaw(out, "    mov rsi, r12");
            emitRaw(out, "    call strcpy");
            emitRaw(out, "    add qword [rel _ac_save_len], rbx");
            emitRaw(out, "    lea rdi, [rel _ac_save_buf]");
            emitRaw(out, "    add rdi, [rel _ac_save_len]");
            emitRaw(out, "    mov byte [rdi], 10");
            emitRaw(out, "    mov byte [rdi+1], 0");
            emitRaw(out, "    inc qword [rel _ac_save_len]");
            emitRaw(out, "    pop r12");
            emitRaw(out, "    pop rbx");
            emitRaw(out, "    ret");
            emitRaw(out, "");
        }
        if (needsEvents_) {
            // `configure event-listener` / `bind KEY to FUNC`: a fixed-size parallel-array
            // table (key-string-ptr, callback-ptr) — 64 slots is far past any realistic
            // keybinding count, and a linear scan avoids hand-rolling a hash map in raw asm.
            // strcmp (libc, already linked) does the key comparison — no need to reimplement
            // string equality by hand the way BNY's zero-dependency pipeline has to.
            emitRaw(out, "section .bss");
            emitRaw(out, "    _ac_ev_keys resq 64");
            emitRaw(out, "    _ac_ev_fns  resq 64");
            emitRaw(out, "    _ac_ev_n    resq 1");
            emitRaw(out, "");
        }
        emitRaw(out, "section .text");
        if (needsEvents_) {
            definedFuncs_.insert("_ac_bind");
            definedFuncs_.insert("_ac_trigger");
            emitRaw(out, "_ac_bind:");                  // rdi=key ptr, rsi=fn ptr
            emitRaw(out, "    mov rax, [rel _ac_ev_n]");
            emitRaw(out, "    lea rcx, [rel _ac_ev_keys]");
            emitRaw(out, "    mov [rcx + rax*8], rdi");
            emitRaw(out, "    lea rcx, [rel _ac_ev_fns]");
            emitRaw(out, "    mov [rcx + rax*8], rsi");
            emitRaw(out, "    inc rax");
            emitRaw(out, "    mov [rel _ac_ev_n], rax");
            emitRaw(out, "    ret");
            emitRaw(out, "");
            emitRaw(out, "_ac_trigger:");                // rdi=key ptr
            emitRaw(out, "    push rbx");
            emitRaw(out, "    push r12");
            emitRaw(out, "    push r13");
            emitRaw(out, "    mov r12, rdi");             // save key ptr (callee-saved across strcmp)
            emitRaw(out, "    xor r13, r13");             // i = 0
            emitRaw(out, "._ac_trig_loop:");
            emitRaw(out, "    mov rax, [rel _ac_ev_n]");
            emitRaw(out, "    cmp r13, rax");
            emitRaw(out, "    jge ._ac_trig_done");
            emitRaw(out, "    lea rcx, [rel _ac_ev_keys]");
            emitRaw(out, "    mov rdi, [rcx + r13*8]");
            emitRaw(out, "    mov rsi, r12");
            emitRaw(out, "    call strcmp");
            emitRaw(out, "    test eax, eax");
            emitRaw(out, "    jnz ._ac_trig_next");
            emitRaw(out, "    lea rcx, [rel _ac_ev_fns]");
            emitRaw(out, "    mov rax, [rcx + r13*8]");
            emitRaw(out, "    call rax");
            emitRaw(out, "    jmp ._ac_trig_done");
            emitRaw(out, "._ac_trig_next:");
            emitRaw(out, "    inc r13");
            emitRaw(out, "    jmp ._ac_trig_loop");
            emitRaw(out, "._ac_trig_done:");
            emitRaw(out, "    pop r13");
            emitRaw(out, "    pop r12");
            emitRaw(out, "    pop rbx");
            emitRaw(out, "    ret");
            emitRaw(out, "");
        }
        // `iota N`: lazy 0..N-1, displayed as its digits concatenated with no separator.
        // Mirrors CStrategy's `ac_iota` exactly (malloc a worst-case buffer, sprintf each
        // digit-run in place, NUL-terminate) — emitted unconditionally like C's copy (see
        // its comment: only ac_ipow/ac_rand/ac_choice are usage-gated, iota/dict/etc. just
        // always emit as a small static-equivalent helper). Was previously not emitted at
        // all on ASM — any `iota`/`stream` use hard-linker-failed ("undefined reference to
        // `ac_iota'"), never even reaching the earlier `.after0` bug (verified: showcase.ac).
        definedFuncs_.insert("ac_iota");
        calledFuncs_.insert("malloc");
        calledFuncs_.insert("sprintf");
        emitRaw(out, "ac_iota:");                      // rdi = n (int64) -> rax = malloc'd C string
        emitRaw(out, "    push rbx");
        emitRaw(out, "    push r12");
        emitRaw(out, "    push r13");
        emitRaw(out, "    push r14");
        // SysV ABI: rsp must be 16-aligned at every `call`. Entry rsp is 8-mod-16 (the
        // `call ac_iota` that brought us here pushed an 8-byte return address onto a
        // 16-aligned caller rsp); 4 pushes above is a multiple of 16, so alignment is
        // UNCHANGED — still 8-mod-16 — right when we're about to `call malloc`/`call
        // sprintf` below. Without this padding slot, glibc's internal SSE (`movaps`,
        // 16-byte-aligned-only) code in vsprintf faults intermittently depending on
        // which internal code path malloc's heap state happens to route through
        // (verified: crashed inside showcase.ac's fuller call graph, NOT in an
        // isolated single-call repro — a misalignment bug is exactly this flaky-by-
        // context signature, not a data bug).
        emitRaw(out, "    sub rsp, 8");
        emitRaw(out, "    mov r12, rdi");               // r12 = n
        emitRaw(out, "    cmp r12, 0");
        emitRaw(out, "    jge .ac_iota_noneg");
        emitRaw(out, "    xor r12, r12");
        emitRaw(out, ".ac_iota_noneg:");
        emitRaw(out, "    mov rax, r12");
        emitRaw(out, "    imul rax, rax, 21");          // worst case: 21 bytes per int64 (incl. sign+NUL slack)
        emitRaw(out, "    inc rax");
        emitRaw(out, "    mov rdi, rax");
        emitRaw(out, "    call malloc");
        emitRaw(out, "    test rax, rax");
        emitRaw(out, "    jnz .ac_iota_gotbuf");
        emitRaw(out, "    mov rdi, 1");
        emitRaw(out, "    call malloc");
        emitRaw(out, "    test rax, rax");
        emitRaw(out, "    jz .ac_iota_done");
        emitRaw(out, "    mov byte [rax], 0");
        emitRaw(out, "    jmp .ac_iota_done");
        emitRaw(out, ".ac_iota_gotbuf:");
        emitRaw(out, "    mov rbx, rax");               // rbx = buffer base
        emitRaw(out, "    mov r13, rbx");               // r13 = write cursor
        emitRaw(out, "    xor r14, r14");                // r14 = i
        emitRaw(out, ".ac_iota_loop:");
        emitRaw(out, "    cmp r14, r12");
        emitRaw(out, "    jge .ac_iota_finish");
        emitRaw(out, "    mov rdi, r13");
        emitRaw(out, "    lea rsi, [rel _fmt_dbare]");   // \"%lld\", no trailing newline
        emitRaw(out, "    mov rdx, r14");
        emitRaw(out, "    xor eax, eax");                // SysV varargs: al = vector regs used = 0
        emitRaw(out, "    call sprintf");
        emitRaw(out, "    add r13, rax");                // advance cursor by chars written
        emitRaw(out, "    inc r14");
        emitRaw(out, "    jmp .ac_iota_loop");
        emitRaw(out, ".ac_iota_finish:");
        emitRaw(out, "    mov byte [r13], 0");
        emitRaw(out, "    mov rax, rbx");
        emitRaw(out, ".ac_iota_done:");
        emitRaw(out, "    add rsp, 8");
        emitRaw(out, "    pop r14");
        emitRaw(out, "    pop r13");
        emitRaw(out, "    pop r12");
        emitRaw(out, "    pop rbx");
        emitRaw(out, "    ret");
        emitRaw(out, "");
        // Dict runtime: string-keyed assoc, fixed 64-pair capacity (same "toy-scale, linear
        // scan, no realloc" choice already made for the event-listener table above) — a
        // malloc'd block [n(8)][k0(8)][v0(8)]...[k63(8)][v63(8)]. Values are raw 8-byte
        // payloads (an int64 OR a string-pool pointer) — mirrors exp_bny.cpp's BNY dict
        // runtime and CStrategy's ac_dict, both of which this was missing entirely before
        // (emitAlloc unconditionally `return`ed for type=="dict" — any `d = {...}` literal
        // left its var slot uninitialized garbage; `pets = [dc_pets_0, dc_pets_1]` (a
        // datac-imported list of dict rows) then indexing it segfaulted on the garbage read).
        definedFuncs_.insert("_ac_dict_new");
        definedFuncs_.insert("_ac_dict_get");
        definedFuncs_.insert("_ac_dict_set");
        calledFuncs_.insert("malloc"); calledFuncs_.insert("strcmp");
        calledFuncs_.insert("printf"); calledFuncs_.insert("exit");
        emitRaw(out, "_ac_dict_new:");                 // -> rax = block ptr
        emitRaw(out, "    mov rdi, " + std::to_string(8 + 64 * 16));
        emitRaw(out, "    call malloc");
        emitRaw(out, "    mov qword [rax], 0");
        emitRaw(out, "    ret");
        emitRaw(out, "");
        emitRaw(out, "_ac_dict_get:");                 // rdi=block, rsi=key -> rax=value
        emitRaw(out, "    push rbx");
        emitRaw(out, "    push r12");
        emitRaw(out, "    push r13");
        emitRaw(out, "    mov r12, rdi");
        emitRaw(out, "    mov r13, rsi");
        emitRaw(out, "    xor rbx, rbx");
        emitRaw(out, ".ac_dg_loop:");
        emitRaw(out, "    mov rax, [r12]");
        emitRaw(out, "    cmp rbx, rax");
        emitRaw(out, "    jge .ac_dg_miss");
        emitRaw(out, "    lea rcx, [r12+8]");
        emitRaw(out, "    mov rax, rbx");
        emitRaw(out, "    imul rax, rax, 16");
        emitRaw(out, "    add rcx, rax");
        emitRaw(out, "    mov rdi, [rcx]");
        emitRaw(out, "    mov rsi, r13");
        emitRaw(out, "    push rcx");
        emitRaw(out, "    call strcmp");
        emitRaw(out, "    pop rcx");
        emitRaw(out, "    test eax, eax");
        emitRaw(out, "    jnz .ac_dg_next");
        emitRaw(out, "    mov rax, [rcx+8]");
        emitRaw(out, "    pop r13");
        emitRaw(out, "    pop r12");
        emitRaw(out, "    pop rbx");
        emitRaw(out, "    ret");
        emitRaw(out, ".ac_dg_next:");
        emitRaw(out, "    inc rbx");
        emitRaw(out, "    jmp .ac_dg_loop");
        emitRaw(out, ".ac_dg_miss:");
        emitRaw(out, "    lea rdi, [rel _msg_keyerror]");
        emitRaw(out, "    mov rsi, r13");
        emitRaw(out, "    xor eax, eax");
        emitRaw(out, "    call printf");
        emitRaw(out, "    mov edi, 1");
        emitRaw(out, "    call exit");
        emitRaw(out, "");
        emitRaw(out, "_ac_dict_set:");                 // rdi=block, rsi=key, rdx=val -> rax=block
        emitRaw(out, "    push rbx");
        emitRaw(out, "    push r12");
        emitRaw(out, "    push r13");
        emitRaw(out, "    push r14");
        emitRaw(out, "    mov r12, rdi");
        emitRaw(out, "    mov r13, rsi");
        emitRaw(out, "    mov r14, rdx");
        emitRaw(out, "    xor rbx, rbx");
        emitRaw(out, ".ac_ds_loop:");
        emitRaw(out, "    mov rax, [r12]");
        emitRaw(out, "    cmp rbx, rax");
        emitRaw(out, "    jge .ac_ds_append");
        emitRaw(out, "    lea rcx, [r12+8]");
        emitRaw(out, "    mov rax, rbx");
        emitRaw(out, "    imul rax, rax, 16");
        emitRaw(out, "    add rcx, rax");
        emitRaw(out, "    mov rdi, [rcx]");
        emitRaw(out, "    mov rsi, r13");
        emitRaw(out, "    push rcx");
        emitRaw(out, "    call strcmp");
        emitRaw(out, "    pop rcx");
        emitRaw(out, "    test eax, eax");
        emitRaw(out, "    jnz .ac_ds_next");
        emitRaw(out, "    mov qword [rcx+8], r14");
        emitRaw(out, "    mov rax, r12");
        emitRaw(out, "    jmp .ac_ds_done");
        emitRaw(out, ".ac_ds_next:");
        emitRaw(out, "    inc rbx");
        emitRaw(out, "    jmp .ac_ds_loop");
        emitRaw(out, ".ac_ds_append:");
        emitRaw(out, "    lea rcx, [r12+8]");
        emitRaw(out, "    mov rax, rbx");
        emitRaw(out, "    imul rax, rax, 16");
        emitRaw(out, "    add rcx, rax");
        emitRaw(out, "    mov [rcx], r13");
        emitRaw(out, "    mov [rcx+8], r14");
        emitRaw(out, "    inc qword [r12]");
        emitRaw(out, "    mov rax, r12");
        emitRaw(out, ".ac_ds_done:");
        emitRaw(out, "    pop r14");
        emitRaw(out, "    pop r13");
        emitRaw(out, "    pop r12");
        emitRaw(out, "    pop rbx");
        emitRaw(out, "    ret");
        emitRaw(out, "");
        dataSec.push_back("_msg_keyerror db \"Preposterous: KeyError: %s\", 10, 0");
        // widgets ilib callback trampolines (see asmWidgetCtor/asmWidgetMethod's `on_click`
        // handling) — GTK's C callback signature is `void (*)(void*)`; the AC user function
        // being bridged to is either 0-arg or 1-arg (arity from userFuncArity_). Mirrors
        // CStrategy's `_ac_widget_call0`/`_ac_widget_call1` (`((ac_int(*)(void))fn)()` /
        // `((ac_int(*)(ac_int))fn)(0)`) exactly, just as NASM instead of C.
        definedFuncs_.insert("_ac_widget_call0");
        definedFuncs_.insert("_ac_widget_call1");
        emitRaw(out, "_ac_widget_call0:");    // rdi = fn ptr
        emitRaw(out, "    call rdi");
        emitRaw(out, "    ret");
        emitRaw(out, "");
        emitRaw(out, "_ac_widget_call1:");    // rdi = fn ptr
        emitRaw(out, "    mov rax, rdi");
        emitRaw(out, "    xor edi, edi");
        emitRaw(out, "    call rax");
        emitRaw(out, "    ret");
        emitRaw(out, "");
    }

    void emitFooter(std::ostringstream &out) override
    {
        // Auto-extern every call target we never defined a body for (runtime helpers like
        // ac_length / math_mod, ilib functions). nasm is two-pass, so a trailing extern
        // still resolves references earlier in the file. printf/exit/abort/fflush are in
        // the header's fixed extern line already.
        static const std::set<std::string> builtins = {"printf","exit","abort","fflush"};
        std::vector<std::string> autoExt;
        for (auto &c : calledFuncs_)
            if (!definedFuncs_.count(c) && !builtins.count(c))
                autoExt.push_back(c);
        if (!autoExt.empty()) {
            emitRaw(out, "");
            for (auto &e : autoExt)
                emitRaw(out, "    extern " + e);
        }
        if (dataSec.empty())
            return;
        // Extra string literals go after the code
        emitRaw(out, "");
        emitRaw(out, "section .data");
        for (auto &d : dataSec)
            emitRaw(out, "    " + d);
    }

    // Every other backend can lean on a compile-time trick to bridge the AC-level
    // dotted name to the ilib's real exported symbol (C's math_c.h #defines
    // `math_mod` to `ac_mod`, C++/Java/Go/Rust/V each hand-write a same-named
    // wrapper function). ASM has no such indirection — whatever a `call`/`extern`
    // targets must resolve to the REAL exported name directly. Mirrors
    // exp_bny.cpp's normalizeExtSym table (BNY has the identical no-preprocessor
    // problem); kept as its own copy since the two are separate translation units.
    // Keyed on the DOTTED AC-level name (`math.mod`) — used directly by
    // formatCallName (fed a dot-preserved name by LIB_CALL's CONST-kind path,
    // e.g. `maudio.stop`) and, flattened once below, by emitCall (fed an
    // already dot-to-underscore-flattened name by the CALL opcode path, since
    // that path's `func` comes from ref()/commonRef, not formatCallName).
    static const std::map<std::string,std::string>& asmIlibSymbolMap() {
        static const std::map<std::string,std::string> tbl = {
            {"math.sin","ac_sin"},{"math.cos","ac_cos"},{"math.tan","ac_tan"},
            {"math.csc","ac_csc"},{"math.sec","ac_sec"},{"math.cot","ac_cot"},
            {"math.asin","ac_asin"},{"math.acos","ac_acos"},{"math.atan","ac_atan"},
            {"math.acsc","ac_acsc"},{"math.asec","ac_asec"},{"math.acot","ac_acot"},
            {"math.atan2","ac_atan2"},
            {"math.deg2rad","ac_deg2rad"},{"math.rad2deg","ac_rad2deg"},
            {"math.sqrt","ac_sqrt"},{"math.pow","ac_pow"},{"math.cbrt","ac_cbrt"},
            {"math.abs","ac_abs"},{"math.abs_int","ac_abs_int"},
            {"math.floor","ac_floor"},{"math.ceil","ac_ceil"},{"math.round","ac_round"},
            {"math.hypot","ac_hypot"},{"math.clamp","ac_clamp"},
            {"math.ln","ac_ln"},{"math.log","ac_log_base"},
            {"math.log2","ac_log2"},{"math.log10","ac_log10"},
            {"math.mod","ac_mod"},{"math.mod_int","ac_mod_int"},
            {"math.to_int","ac_to_int"},{"math.to_dec","ac_to_dec"},
            {"math.gcd","ac_gcd"},{"math.lcm","ac_lcm"},{"math.is_prime","ac_is_prime"},
            {"math.pi","ac_math_pi_const"},{"math.e","ac_math_e_const"},
            {"math.phi","ac_math_phi_const"},{"math.tau","ac_math_tau_const"},
            {"math.em","ac_math_em_const"},{"math.inf","ac_math_inf"},
            {"camera.init","ac_camera_init"},{"camera.capture","ac_camera_capture"},
            {"camera.capture_latest","ac_camera_capture_latest"},
            {"camera.capture_first","ac_camera_capture_first"},
            {"camera.release","ac_camera_release"},
            {"sidebar.config","ac_sidebar_config"},{"sidebar.setregion","ac_sidebar_setregion"},
            {"sidebar.setinteractive","ac_sidebar_setinteractive"},
            {"sidebar.display","ac_sidebar_display"},{"sidebar.ask","ac_sidebar_ask"},
            {"sidebar.getinput","ac_sidebar_getinput"},
            {"screen.setmode","ac_screen_setmode"},{"screen.update","ac_screen_update"},
            {"web.open","ac_web_open"},{"web.file_open","ac_web_file_open"},
            {"web.popen","ac_web_popen"},{"web.ropen","ac_web_ropen"},
            {"web.browser","ac_web_browser"},{"web.pdf","ac_web_pdf"},
            {"web.text","ac_web_text"},{"web.inspect","ac_web_inspect"},
            {"web.ac_page","ac_web_ac_page"},{"web.page_get","ac_web_page_get"},
            {"web.help","ac_web_help"},
            {"server.db_run","ac_server_db_run"},{"server.db_run_p","ac_server_db_run_p"},
            {"server.db_import","ac_server_db_import"},{"server.db_reset","ac_server_db_reset"},
            {"server.db_stop","ac_server_db_stop"},{"server.listen","ac_server_listen"},
            {"server.accept","ac_server_accept"},{"server.req_method","ac_server_req_method"},
            {"server.req_path","ac_server_req_path"},{"server.req_query","ac_server_req_query"},
            {"server.req_body","ac_server_req_body"},{"server.req_header","ac_server_req_header"},
            {"server.respond","ac_server_respond"},{"server.respond_json","ac_server_respond_json"},
            {"server.close","ac_server_close"},{"server.help","ac_server_help"},
            {"maudio.stop","ac_maudio_stop_all"},
            // AC's `stringm.strip(s)` is a 1-arg whitespace trim, NOT the underlying 2-arg
            // char-set `ac_stringm_strip(s, chars)` the generic "ac_" + flattened-name fallback
            // below would otherwise wrongly route it to (passing whatever garbage register
            // happened to hold a 2nd arg AC source never supplied). Mirrors string_cheese_c.h's
            // own `#define stringm_strip ac_stringm_trim /* AC strip = 1-arg trim */` — C gets
            // this via textual macro inclusion, ASM needs it here explicitly (verified:
            // stringm_demo.ac's `stringm.strip($  padded  $)` silently no-op'd on ASM before).
            {"stringm.strip","ac_stringm_trim"},
            // string-cheese.acl aliases BOTH "length" and "len" input keywords to the SAME
            // "stringm.length" call name (only "length" is ever actually emitted) — but the real
            // exported symbol is the short `ac_stringm_len` (string_cheese_c.h's C/C++ shims
            // needed the identical fix: a missing `length` alias, verified failing even on C).
            {"stringm.length","ac_stringm_len"},
        };
        return tbl;
    }
    // Same table, keyed on the flattened (dots→underscores) form — what emitCall
    // actually receives via the CALL-opcode dispatch path's ref()/commonRef flatten.
    static const std::map<std::string,std::string>& asmIlibSymbolMapFlattened() {
        static const std::map<std::string,std::string> flat = [] {
            std::map<std::string,std::string> m;
            for (auto& [dotted, real] : asmIlibSymbolMap()) {
                std::string key = dotted;
                for (char& c : key) if (c == '.') c = '_';
                m[key] = real;
            }
            return m;
        }();
        return flat;
    }
    std::string formatCallName(const std::string& n) const override {
        auto& tbl = asmIlibSymbolMap();
        auto it = tbl.find(n);
        if (it != tbl.end()) return it->second;
        for (const char* ns : {"os.", "regex.", "stringm.", "ncpu.", "maudio."}) {
            if (n.rfind(ns, 0) == 0) {
                std::string s = "ac_" + n;
                for (auto& c : s) if (c == '.') c = '_';
                return s;
            }
        }
        std::string s = n; for (char& c : s) if (c == '.') c = '_'; return s;
    }
    std::string formatRef(const IRRef &r, SymbolTable *sym) override
    {
        // Bundle field access (self.field / instance.field): every other dotted name gets
        // flattened to underscores below (an existing, unrelated convention for namespaced ilib
        // names) — that would turn a real field access into a disconnected plain local
        // `self_hp`/`c_hp`, silently losing the field. Mirrors CStrategy's own formatRef (see
        // its comment) — preserve the dot here, un-flattened, so loadRAX/storeRAX's
        // resolveFieldAccess can recognize and translate it to real pointer+offset addressing.
        if (r.kind == IRRef::Kind::VAR) {
            std::string raw = r.toStringWithSymbols(sym);
            std::string base; int offset;
            if (resolveFieldAccess(raw, base, offset)) return raw;
        }
        return commonRef(r, sym, "1", "0", "0", "0", false);
    }

    void emitStoreVar(std::ostringstream &out, int & /*indent*/, const std::string &var, const std::string &val) override
    {
        // `atomic` var: wrap the WHOLE statement (read-of-current-value via `val` + write) in
        // the global lock, so a compound update like `x = x + 1` is a genuine, uninterruptible
        // RMW — matches every other backend's `atomic` codegen.
        if (atomicVars_.count(var)) {
            out << "    lea rdi, [_ac_atomic_lock]\n    call pthread_mutex_lock\n";
            calledFuncs_.insert("pthread_mutex_lock");
            loadRAX(out, val);
            storeRAX(out, var);
            out << "    lea rdi, [_ac_atomic_lock]\n    call pthread_mutex_unlock\n";
            calledFuncs_.insert("pthread_mutex_unlock");
            return;
        }
        // Propagate string/list-ness through a plain copy (`LOAD_VAR iterT, collRef` — how the
        // low-level FOR-loop lowering seeds its loop-carried iterator temp) — without this, a
        // FOR loop over a string/array collection loses track of what `iterT` actually holds,
        // and emitLoadIndex has no way to tell a `FOR c in $string$` apart from array indexing.
        if (isStr(val) && !isListVar(val)) strVars_.insert(var);
        if (isListVar(val)) listVars_.insert(var);
        if (dictVars_.count(val)) {
            dictVars_.insert(var);
            if (dictStrKeys_.count(val)) dictStrKeys_[var] = dictStrKeys_[val];
        }
        if (listOfDictVars_.count(val)) {
            listOfDictVars_.insert(var);
            if (listOfDictStrVals_.count(val)) listOfDictStrVals_.insert(var);
            if (dictStrKeys_.count(val)) dictStrKeys_[var] = dictStrKeys_[val];
        }
        // Same gap, float-ness: a double-returning call's result TEMP gets correctly marked in
        // floatVars_ at the call site (see emitCall's floatSig handling), but `before = <that
        // temp>`'s plain STORE_VAR never propagated it to `before` itself — the bit pattern
        // copies over correctly (loadRAX/storeRAX below is a raw 8-byte copy either way), but
        // `before` then "looks" like a plain int to every later isFloatVal() check, so
        // Term.display picked the integer format and printed the double's raw bits as a huge
        // decimal (verified: ml_scalar.ac's `before = ml.take(w)` printed
        // "4611686018427387904" — exactly 2.0's IEEE-754 bit pattern reinterpreted as int64 —
        // instead of "2.0").
        if (isFloatVal(val)) floatVars_.insert(var);
        loadRAX(out, val);
        storeRAX(out, var);
    }
    // `to_int`/`to_string`/`to_dec`/`to_bool`/`short`/`mini`/`atomic` — previously this function
    // returned immediately for anything but ATOMIC, meaning EVERY other cast was a silent no-op:
    // the destination slot was never written at all, so `to_int n = $42$; Term.display n + 1`
    // read raw uninitialized stack garbage instead of 43. A real, verified bug independent of
    // (and larger than) the float-specific gap that led here — fixed for every target type, not
    // just the one that was being investigated.
    void emitTypeCast(std::ostringstream &out, int & /*indent*/,
                      const std::string &var, const std::string &src, IRType t) override
    {
        bool srcIsStr = isStr(src);
        bool srcIsFloat = isFloatVal(src);
        if (t == IRType::ATOMIC) {
            out << "    lea rdi, [_ac_atomic_lock]\n    call pthread_mutex_lock\n";
            calledFuncs_.insert("pthread_mutex_lock");
            loadRAX(out, src);
            storeRAX(out, var);
            out << "    lea rdi, [_ac_atomic_lock]\n    call pthread_mutex_unlock\n";
            calledFuncs_.insert("pthread_mutex_unlock");
        }
        else if (t == IRType::FLOAT) {
            floatVars_.insert(var);
            if (srcIsStr) {
                loadRAX(out, src);
                out << "    mov rdi, rax\n    xor rsi, rsi\n    call strtod\n"; // double result in xmm0
                calledFuncs_.insert("strtod");
                storeXMM0(out, var);
            } else if (srcIsFloat) {
                loadRAX(out, src);   // already a double bit-pattern — no conversion needed
                storeRAX(out, var);
            } else {
                loadRAX(out, src);   // integer VALUE (not bits) — real int-to-double conversion
                out << "    cvtsi2sd xmm0, rax\n";
                storeXMM0(out, var);
            }
        }
        else if (t == IRType::STRING) {
            strVars_.insert(var);
            if (srcIsStr) {
                loadRAX(out, src);
                storeRAX(out, var);
            } else {
                emitIntToStr(out, src, srcIsFloat);
                storeRAX(out, var);
            }
        }
        else if (t == IRType::BOOL) {
            loadRAX(out, src);
            out << "    test rax, rax\n    setne al\n    movzx rax, al\n";
            storeRAX(out, var);
        }
        else {   // INT / SHORT / MINI — plain integer family, with width truncation for short/mini
            if (srcIsStr) {
                loadRAX(out, src);
                out << "    mov rdi, rax\n    call atoll\n";
                calledFuncs_.insert("atoll");
            } else if (srcIsFloat) {
                loadXMM0(out, src);
                out << "    cvttsd2si rax, xmm0\n";   // truncate-toward-zero, real value conversion
            } else {
                loadRAX(out, src);
            }
            if (t == IRType::SHORT) out << "    movsx rax, eax\n";        // wrap to 32-bit signed
            else if (t == IRType::MINI) out << "    movsx rax, ax\n";     // wrap to 16-bit signed
            storeRAX(out, var);
        }
    }
    // Int/double -> string: malloc(24) + snprintf, mirrors CStrategy's ac_to_str/ac_fstr but
    // inline (ASM has no shared runtime-helper mechanism to hang a reusable function off of).
    void emitIntToStr(std::ostringstream &out, const std::string &src, bool srcIsFloat)
    {
        // For the int case, capture the value in r12 (callee-saved) BEFORE malloc — malloc clobbers
        // rax. For the float case, load xmm0 AFTER malloc instead of before it — XMM registers
        // aren't callee-saved across a call either, so loading early would just get clobbered.
        if (!srcIsFloat) { loadRAX(out, src); out << "    mov r12, rax\n"; }
        out << "    mov rdi, 24\n    call malloc\n";
        calledFuncs_.insert("malloc");
        out << "    mov rbx, rax\n";   // buffer ptr, callee-saved across the snprintf call below
        out << "    mov rdi, rbx\n    mov rsi, 24\n";
        if (srcIsFloat) {
            out << "    lea rdx, [rel _fmt_fbare]\n";
            loadXMM0(out, src);
            out << "    mov al, 1\n"; // 1 vector reg used
        } else {
            out << "    lea rdx, [rel _fmt_dbare]\n    mov rcx, r12\n    xor eax, eax\n";
        }
        out << "    call snprintf\n";
        calledFuncs_.insert("snprintf");
        out << "    mov rax, rbx\n";
    }
    // String `+` concat: malloc(strlen(a)+strlen(b)+1) + strcpy + strcat, mirrors CStrategy's
    // ac_concat. ASM's `+` previously did a plain integer `add rax, rbx` UNCONDITIONALLY — for
    // string operands that adds the two POINTER VALUES together (garbage address), which then
    // printed as a nonsense integer instead of the concatenated string.
    void emitStrConcat(std::ostringstream &out, const std::string &lhs, const std::string &rhs)
    {
        loadRAX(out, lhs); out << "    mov r12, rax\n";
        loadRAX(out, rhs); out << "    mov r13, rax\n";
        out << "    mov rdi, r12\n    call strlen\n"; calledFuncs_.insert("strlen");
        out << "    mov r14, rax\n";
        out << "    mov rdi, r13\n    call strlen\n";
        out << "    add r14, rax\n    inc r14\n";
        out << "    mov rdi, r14\n    call malloc\n"; calledFuncs_.insert("malloc");
        out << "    mov rbx, rax\n";
        out << "    mov rdi, rbx\n    mov rsi, r12\n    call strcpy\n"; calledFuncs_.insert("strcpy");
        out << "    mov rdi, rbx\n    mov rsi, r13\n    call strcat\n"; calledFuncs_.insert("strcat");
        out << "    mov rax, rbx\n";
    }

    // `/` (true division): the base class default just calls emitBinaryOp("/") — plain `idiv`,
    // truncating (`7 / 3` → 2 instead of 2.333...). Every other typed backend (see CStrategy's
    // own emitTrueDivision) instead ALWAYS promotes both operands to float here, matching the
    // established statically-typed-backend convention (differs from PY's dynamic "int when
    // clean, else float" `ac_div`, same class of accepted per-backend idiom gap as bool
    // True/False vs 1/0). ASM had no override at all, so `s / length arr` on non-evenly-divisible
    // input (verified: mean([1,2,4]) = 7/3) silently truncated to "2" instead of "2.333...3".
    void emitTrueDivision(std::ostringstream &out, int & /*indent*/, const std::string &res,
                          const std::string &lhs, const std::string &rhs) override
    {
        floatVars_.insert(res);
        loadDouble(out, lhs, "xmm0");
        loadDouble(out, rhs, "xmm1");
        out << "    divsd xmm0, xmm1\n";
        storeXMM0(out, res);
    }
    void emitBinaryOp(std::ostringstream &out, int & /*indent*/, const std::string &res,
                      const std::string &lhs, const std::string &rhs, const std::string &op) override
    {
        // Float arithmetic: real SSE2 ops (addsd/subsd/mulsd/divsd), not the integer path below —
        // this is the other half of the float-support gap (see floatVars_'s comment): even once
        // values could be stored/printed as floats, `d + 1` or `7 /// 2` still silently ran
        // through `add rax, rbx` / `idiv` (bit-pattern-as-integer arithmetic — nonsense results).
        if (op == "+" && (isStr(lhs) || isStr(rhs))) {
            strVars_.insert(res);
            emitStrConcat(out, lhs, rhs);
            storeRAX(out, res);
            return;
        }
        if ((op == "+" || op == "-" || op == "*" || op == "/") && (isFloatVal(lhs) || isFloatVal(rhs))) {
            floatVars_.insert(res);
            loadDouble(out, lhs, "xmm0");
            loadDouble(out, rhs, "xmm1");
            const char* insn = (op == "+") ? "addsd" : (op == "-") ? "subsd" : (op == "*") ? "mulsd" : "divsd";
            out << "    " << insn << " xmm0, xmm1\n";
            storeXMM0(out, res);
            return;
        }
        loadRAX(out, lhs);
        if (op == "+")
        {
            if (looksNumeric(rhs))
                out << "    add rax, " << rhs << "\n";
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
                out << "    add rax, rbx\n";
            }
        }
        else if (op == "-")
        {
            if (looksNumeric(rhs))
                out << "    sub rax, " << rhs << "\n";
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
                out << "    sub rax, rbx\n";
            }
        }
        else if (op == "*")
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
        else if (op == "/")
        {
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            emitDivZeroGuard(out);
            out << "    cqo\n    idiv rbx\n"; // quotient in RAX
        }
        else if (op == "%")
        {
            if (looksNumeric(rhs))
            {
                out << "    mov rbx, " << rhs << "\n";
            }
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
            }
            emitDivZeroGuard(out);
            out << "    cqo\n    idiv rbx\n    mov rax, rdx\n"; // remainder in RDX
        }
        else if (op == "&" || op == "|" || op == "^")
        {
            // band/bor/bxor(/bnot, lowered by the shared dispatcher as `x ^ -1`) were falling
            // through every branch above with NO match at all, silently reaching storeRAX with
            // whatever loadRAX(lhs) left in RAX — i.e. a no-op that just echoes the left
            // operand back out unchanged. Only ever "worked" for constant operands, which the
            // compiler's own constant-folding pass evaluates before codegen ever sees a BAND/
            // BOR/BXOR instruction — variables (or non-foldable expressions) were genuinely
            // broken. Found via `examples/bitwise_words.ac`'s `~5`/`bnot 0` (folding doesn't
            // reach the synthesized `x ^ -1` form the same way it folds a literal `a band b`).
            const char* insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
            if (looksNumeric(rhs))
                out << "    " << insn << " rax, " << rhs << "\n";
            else
            {
                out << "    mov rbx, [rbp-" << getSlot(rhs) << "]\n";
                out << "    " << insn << " rax, rbx\n";
            }
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
        if ((op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=")
            && (isFloatVal(lhs) || isFloatVal(rhs)))
        {
            // ucomisd sets ZF/PF/CF the way integer `cmp` sets ZF/SF/OF — different flags, so
            // the SETcc mnemonics below aren't reusable as-is; PF=1 means "unordered" (a NaN
            // operand), which AC has no real concept of, so we don't special-case it — an
            // unordered result just falls out as whichever flag combination it naturally sets.
            loadDouble(out, lhs, "xmm0");
            loadDouble(out, rhs, "xmm1");
            out << "    ucomisd xmm0, xmm1\n";
            std::string setcc =
                (op == "==") ? "sete" : (op == "!=") ? "setne"
                                    : (op == "<")    ? "setb"
                                    : (op == ">")    ? "seta"
                                    : (op == "<=")   ? "setbe"
                                                     : "setae";
            out << "    " << setcc << " al\n";
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

    // Splits a top-level-comma-separated element list, trimming surrounding spaces (matches how
    // ir.cpp's ListLiteral lowering formats a list's ALLOC content — "1, 2, 3").
    static std::vector<std::string> splitElems(const std::string &content) {
        std::vector<std::string> out;
        std::string cur; int depth = 0; bool inStr = false;
        for (char c : content) {
            if (c == '"') inStr = !inStr;
            if (!inStr && (c == '(' || c == '[')) depth++;
            if (!inStr && (c == ')' || c == ']')) depth--;
            if (c == ',' && depth == 0 && !inStr) { out.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!content.empty()) out.push_back(cur);
        for (auto &s : out) {
            size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
            s = (a != std::string::npos) ? s.substr(a, b - a + 1) : s;
        }
        return out;
    }
    void emitAlloc(std::ostringstream &out, int & /*indent*/,
                  const std::string &var, const std::string &type,
                  const std::string &content, const std::string & /*content2*/) override
    {
        // `range`/`sequence` (and iota/stream, which alias to them) NEVER reach here on ASM —
        // ir.cpp's low-level FOR-loop path (useHighLevelIR==false) handles those entirely via
        // its own compact counted WHILST loop and `break`s before ever emitting an ALLOC. This
        // only ever fires for a genuine list literal (`xs = [1, 2, 3]`) or a dict literal.
        if (type == "dict") {
            dictVars_.insert(var);
            out << "    call _ac_dict_new\n";
            storeRAX(out, var);
            for (auto &[k, v] : parseDictPairs(content)) {
                std::string key = k;
                if (key.size() >= 2 && key.front() == '$' && key.back() == '$')
                    key = key.substr(1, key.size() - 2);
                loadRAX(out, "\"" + key + "\"");
                out << "    mov rsi, rax\n";
                bool isStrVal = v.size() >= 2 && v.front() == '$' && v.back() == '$';
                if (isStrVal) {
                    dictStrKeys_[var].insert(key);
                    loadRAX(out, "\"" + v.substr(1, v.size() - 2) + "\"");
                } else {
                    loadRAX(out, v);
                }
                out << "    mov rdx, rax\n";
                loadRAX(out, var);
                out << "    mov rdi, rax\n";
                out << "    call _ac_dict_set\n";
            }
            return;
        }
        if (type != "list") return;
        listVars_.insert(var);
        // A list whose elements are themselves dict vars (e.g. datac-imported rows) — the
        // generic loop below already loads each element by plain variable-slot lookup
        // (loadRAX falls through to `mov rax, [rbp-slot]` for any bare name), so it already
        // stores the right dict-block pointers; this just needs to record the list itself as
        // "elements are dicts" so a later `pets[1]` LOAD_INDEX knows to mark its result dict.
        {
            auto parts = splitCommaTrimmed(content);
            bool allDicts = !parts.empty();
            for (auto &p : parts) if (!dictVars_.count(p)) { allDicts = false; break; }
            if (allDicts) {
                listOfDictVars_.insert(var);
                bool strVal = false;
                for (auto &p : parts) if (dictStrKeys_.count(p) && !dictStrKeys_[p].empty()) { strVal = true; break; }
                if (strVal) {
                    listOfDictStrVals_.insert(var);
                    for (auto &p : parts) dictStrKeys_[var].insert(dictStrKeys_[p].begin(), dictStrKeys_[p].end());
                }
            }
        }
        auto elems = splitElems(content);
        int n = (int)elems.size();
        out << "    mov rdi, " << (8 * (n + 1)) << "\n    call malloc\n";
        calledFuncs_.insert("malloc");
        out << "    mov rbx, rax\n";
        out << "    mov qword [rbx], " << n << "\n";
        for (int idx = 0; idx < n; idx++) {
            loadRAX(out, elems[idx]);
            out << "    mov qword [rbx + " << (8 * (idx + 1)) << "], rax\n";
        }
        out << "    mov rax, rbx\n";
        storeRAX(out, var);
    }
    void emitLoadIndex(std::ostringstream &out, int & /*indent*/, const std::string &result,
                       const std::string &arr, const std::string &idx) override
    {
        std::string idxUnq = idx;
        if (idxUnq.size() >= 2 && idxUnq.front() == '"') idxUnq = idxUnq.substr(1, idxUnq.size() - 2);
        if (isStr(arr) && !isListVar(arr)) {
            // s[i] on a string → real 1-char string (AC semantics: iterating/indexing a string
            // yields characters), and s's own `__len__` is a real strlen — this is the exact
            // mechanism `FOR c in $string$` rides on low-level backends (LOAD_INDEX with a
            // "__len__" sentinel then per-index reads); with no ASM implementation at all this
            // hung forever (verified: 5GB of output before being killed — `lenT` was never set).
            if (idxUnq == "__len__") {
                loadRAX(out, arr);
                out << "    mov rdi, rax\n    call strlen\n";
                calledFuncs_.insert("strlen");
                storeRAX(out, result);
                return;
            }
            loadRAX(out, arr);
            out << "    mov rbx, rax\n";
            loadRAX(out, idx);
            out << "    add rbx, rax\n";
            out << "    movzx rbx, byte [rbx]\n";   // rbx (callee-saved) survives the malloc call below —
                                                      // rcx is caller-saved and gets clobbered by it
            out << "    mov rdi, 2\n    call malloc\n";
            calledFuncs_.insert("malloc");
            out << "    mov byte [rax], bl\n";
            out << "    mov byte [rax+1], 0\n";
            strVars_.insert(result);
            storeRAX(out, result);
            return;
        }
        if (dictVars_.count(arr)) {
            loadRAX(out, arr);
            out << "    mov rdi, rax\n";
            loadRAX(out, idx);
            out << "    mov rsi, rax\n";
            out << "    call _ac_dict_get\n";
            storeRAX(out, result);
            if (dictStrKeys_.count(arr) && dictStrKeys_[arr].count(idxUnq)) strVars_.insert(result);
            return;
        }
        loadRAX(out, arr);
        out << "    mov rbx, rax\n";
        if (idxUnq == "__len__") {
            out << "    mov rax, qword [rbx]\n";
            storeRAX(out, result);
            return;
        }
        loadRAX(out, idx);
        out << "    mov rcx, rax\n";
        out << "    mov rax, qword [rbx + 8 + rcx*8]\n";
        storeRAX(out, result);
        // A list-of-dicts element read (e.g. `p1 = pets[1]`, datac multi-row import) yields a
        // dict-block pointer — the value itself was already stored correctly by the generic
        // list ALLOC above (loadRAX resolves a plain var name like any other), this just
        // propagates dict-ness onto the result so a later `p1[$name$]` dict-get is recognized.
        if (listOfDictVars_.count(arr)) {
            dictVars_.insert(result);
            if (listOfDictStrVals_.count(arr) && dictStrKeys_.count(arr))
                dictStrKeys_[result] = dictStrKeys_[arr];
        }
    }
    void emitStoreIndex(std::ostringstream &out, int & /*indent*/, const std::string &arr,
                        const std::string &idx, const std::string &val) override
    {
        if (dictVars_.count(arr)) {
            loadRAX(out, arr);
            out << "    mov rdi, rax\n";
            loadRAX(out, idx);
            out << "    mov rsi, rax\n";
            std::string idxUnq = idx;
            if (idxUnq.size() >= 2 && idxUnq.front() == '"') idxUnq = idxUnq.substr(1, idxUnq.size() - 2);
            if (isStr(val)) dictStrKeys_[arr].insert(idxUnq);
            loadRAX(out, val);
            out << "    mov rdx, rax\n";
            out << "    call _ac_dict_set\n";
            return;
        }
        loadRAX(out, arr);
        out << "    mov rbx, rax\n";
        loadRAX(out, idx);
        out << "    mov rcx, rax\n";
        loadRAX(out, val);
        out << "    mov qword [rbx + 8 + rcx*8], rax\n";
    }
    void emitCall(std::ostringstream &out, int & /*indent*/, const std::string &res,
                  const std::string &func_in, const std::string &args) override
    {
        if (asmWidgetCtor(out, res, func_in, splitCallArgs(args))) return;
        if (asmWidgetMethod(out, res, func_in, args)) return;
        // `func_in` arrives already dot→underscore-flattened (e.g. `math_mod`) via the CALL
        // opcode dispatch's ref()/commonRef path, which never goes through formatCallName —
        // remap it to the ilib's REAL exported symbol here (mirrors formatCallName's own
        // remap of the dot-preserved LIB_CALL/CONST-kind path; see asmIlibSymbolMap's comment).
        std::string func = func_in;
        {
            auto &tbl = asmIlibSymbolMapFlattened();
            auto it = tbl.find(func);
            if (it != tbl.end()) func = it->second;
            else {
                // Generic namespace fallback — mirrors formatCallName's own fallback for
                // os./regex./stringm./ncpu./maudio. (see its comment): those ilibs export
                // ac_os_cwd/ac_regex_match/ac_stringm_upper/... directly (no per-function
                // remap-table entry needed, it's a uniform "ac_" + flattened-name prefix).
                // Missing this here specifically broke every os.*/stringm.*/regex.* call on
                // ASM (verified: os_demo.ac, stringm_demo.ac, regex_demo.ac all hard-linker-
                // failed — "undefined reference to `os_cwd'" etc, calling the flattened name
                // literally instead of the real exported symbol).
                for (const char* ns : {"os_", "regex_", "stringm_", "ncpu_", "maudio_"}) {
                    if (func.rfind(ns, 0) == 0) { func = "ac_" + func; break; }
                }
            }
        }
        // `length $str$` — every other backend special-cases this to its native string-length
        // call (strlen/.length/len()); ASM had none at all, so it emitted a plain `call
        // ac_length` with no matching symbol anywhere — a hard link error ("undefined reference
        // to `ac_length'"), not even a wrong-answer bug, just never worked at all.
        if (func == "ac_length" && !res.empty()) {
            if (isListVar(args)) {
                loadRAX(out, args);
                out << "    mov rax, [rax]\n";   // length lives at [base+0]
                storeRAX(out, res);
                return;
            }
            loadRAX(out, args);
            out << "    mov rdi, rax\n    call strlen\n";
            calledFuncs_.insert("strlen");
            storeRAX(out, res);
            return;
        }
        // `arr.append(v)` — realloc-grow-by-one (mirrors CStrategy's ac_arr_push): malloc a
        // block one element bigger, copy the old length-prefixed contents, append the new
        // value, and reassign `recv` to the NEW pointer (the old block may have moved).
        for (const char* suf : {".append", "_append"}) {
            auto ap = func.rfind(suf);
            if (ap != std::string::npos && ap == func.size() - 7 && ap > 0) {
                std::string recv = func.substr(0, ap);
                if (isListVar(recv)) {
                    loadRAX(out, recv);
                    out << "    mov rbx, rax\n";           // rbx = old block ptr
                    out << "    mov r13, qword [rbx]\n";   // r13 = old length n
                    out << "    mov rax, r13\n";
                    out << "    add rax, 2\n";              // n+2 words: header + (n+1) elements
                    out << "    imul rax, rax, 8\n";
                    out << "    mov rdi, rax\n";
                    out << "    call malloc\n";
                    calledFuncs_.insert("malloc");
                    out << "    mov r12, rax\n";            // r12 = new block ptr
                    out << "    mov rsi, rbx\n";
                    out << "    mov rdi, r12\n";
                    out << "    mov rcx, r13\n";
                    out << "    inc rcx\n";                  // copy header + n old elements = n+1 words
                    out << "    rep movsq\n";
                    loadRAX(out, args);                       // rax = value being appended
                    out << "    mov rbx, rax\n";
                    out << "    mov rax, r13\n";
                    out << "    mov qword [r12 + 8 + rax*8], rbx\n";
                    out << "    mov rax, r13\n";
                    out << "    inc rax\n";
                    out << "    mov [r12], rax\n";            // update length
                    out << "    mov rax, r12\n";
                    storeRAX(out, recv);                       // reassign — the block may have moved
                    return;
                }
            }
        }
        out << "    ; call " << func << "(" << args << ")\n";
        std::vector<std::string> parsed;
        {
            std::string cur; int depth = 0; bool inStr = false;
            for (char c : args) {
                if (c == '"') inStr = !inStr;
                if (!inStr && (c == '(' || c == '[')) depth++;
                if (!inStr && (c == ')' || c == ']')) depth--;
                if (c == ',' && depth == 0 && !inStr) { parsed.push_back(cur); cur.clear(); }
                else cur += c;
            }
            if (!cur.empty()) parsed.push_back(cur);
        }
        // Ilib functions with `double` args/return (math.sin/sqrt/mod/pow/...) — every one of
        // these is documented in math_ffi.asm as taking its args in xmm0/xmm1/... per the real
        // SysV ABI, but nothing was ever routing them there: this whole generic path always
        // loaded args into rdi/rsi/... and read the result out of rax, so ANY math ilib float
        // call (verified: showcase.ac's `math.mod(n, d)`) silently passed garbage int-register
        // args to a function reading xmm0/xmm1 — not even a link error, a runtime stack-
        // corrupting crash. `loadDouble`/`storeXMM0` (used by binary float ops) already exist;
        // this just routes ilib calls through them too, per a per-symbol signature table sourced
        // from math_ffi.asm's documented real calling convention (see asmIlibSymbolMap above for
        // where these exported names come from).
        static const std::map<std::string, std::pair<std::string,char>> floatSig = {
            {"ac_math_pi_const", {"", 'd'}}, {"ac_math_e_const", {"", 'd'}},
            {"ac_math_tau_const", {"", 'd'}}, {"ac_math_inf", {"", 'd'}},
            {"ac_math_pi", {"i", 'd'}}, {"ac_math_e", {"i", 'd'}},
            {"ac_sin", {"d", 'd'}}, {"ac_cos", {"d", 'd'}}, {"ac_tan", {"d", 'd'}},
            {"ac_csc", {"d", 'd'}}, {"ac_sec", {"d", 'd'}}, {"ac_cot", {"d", 'd'}},
            {"ac_asin", {"d", 'd'}}, {"ac_acos", {"d", 'd'}}, {"ac_atan", {"d", 'd'}},
            {"ac_acsc", {"d", 'd'}}, {"ac_asec", {"d", 'd'}}, {"ac_acot", {"d", 'd'}},
            {"ac_deg2rad", {"d", 'd'}}, {"ac_rad2deg", {"d", 'd'}},
            {"ac_sqrt", {"d", 'd'}}, {"ac_cbrt", {"d", 'd'}}, {"ac_abs", {"d", 'd'}},
            {"ac_floor", {"d", 'd'}}, {"ac_ceil", {"d", 'd'}}, {"ac_round", {"d", 'd'}},
            {"ac_log2", {"d", 'd'}}, {"ac_log10", {"d", 'd'}},
            {"ac_atan2", {"dd", 'd'}}, {"ac_pow", {"dd", 'd'}}, {"ac_hypot", {"dd", 'd'}},
            {"ac_mod", {"dd", 'd'}}, {"ac_log_base", {"dd", 'd'}},
            {"ac_clamp", {"ddd", 'd'}},
            {"ac_to_int", {"d", 'i'}}, {"ac_to_dec", {"i", 'd'}},
            // ml ilib (libacml.h's "AC-native API" surface) — same calling-convention gap,
            // different library: `ml.tensor(2)` passed its double arg via rdi (int reg) instead
            // of xmm0, and `ml.take(w)` read its double RESULT out of rax instead of xmm0 —
            // verified: ml_scalar.ac printed garbage ("544166712" twice) instead of "2.0"/
            // "1.64...". ml's exported names are already flat (no ac_/ilib-name remap needed,
            // unlike math) — this table entry alone is the whole fix.
            {"ml_tensor", {"d", 'i'}}, {"ml_grid", {"iid", 'i'}},
            {"ml_gradient_track", {"ii", 'i'}}, {"ml_grad_wipe", {"i", 'i'}},
            {"ml_weights", {"di", 'i'}}, {"ml_take", {"i", 'd'}},
            {"ml_add", {"ii", 'i'}}, {"ml_multiply", {"ii", 'i'}}, {"ml_relu", {"i", 'i'}},
            {"ml_backward", {"i", 'i'}}, {"ml_get_grad", {"i", 'i'}},
        };
        auto sigIt = floatSig.find(func);
        if (sigIt != floatSig.end()) {
            const std::string &argTypes = sigIt->second.first;
            char retType = sigIt->second.second;
            int nextInt = 0, nextXmm = 0;
            static const char *intRegs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            static const char *xmmRegs[8] = {"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"};
            for (size_t k = 0; k < argTypes.size() && k < parsed.size(); k++) {
                std::string a = parsed[k];
                size_t s = a.find_first_not_of(' '); size_t e = a.find_last_not_of(' ');
                if (s == std::string::npos) continue;
                a = a.substr(s, e - s + 1);
                if (argTypes[k] == 'd') {
                    loadDouble(out, a, xmmRegs[nextXmm++]);
                } else {
                    loadRAX(out, a);
                    out << "    mov " << intRegs[nextInt++] << ", rax\n";
                }
            }
            calledFuncs_.insert(func);
            out << "    call " << func << "\n";
            if (!res.empty()) {
                if (retType == 'd') { floatVars_.insert(res); storeXMM0(out, res); }
                else storeRAX(out, res);
            }
            return;
        }
        // Load each argument into its System V ABI register (rdi, rsi, rdx, rcx, r8, r9).
        // Values are stack slots / literals, and loadRAX only touches rax, so loading a
        // later arg can't clobber an earlier arg register.
        static const char *argRegs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        for (size_t i = 0; i < parsed.size() && i < 6; i++) {
            std::string a = parsed[i];
            size_t s = a.find_first_not_of(' '); size_t e = a.find_last_not_of(' ');
            if (s == std::string::npos) continue;
            a = a.substr(s, e - s + 1);
            loadRAX(out, a);
            out << "    mov " << argRegs[i] << ", rax\n";
        }
        calledFuncs_.insert(func);
        out << "    call " << func << "\n";
        if (!res.empty()) {
            if (isUserFloatReturningFunc(func)) floatVars_.insert(res);
            storeRAX(out, res);
        }
    }

    void emitReturn(std::ostringstream &out, int & /*indent*/, const std::string &val) override
    {
        if (!val.empty())
            loadRAX(out, val);
        else
            out << "    xor eax, eax\n";
        out << "    mov rbx, [rbp-8]\n";   // restore caller's RBX (see emitFunctionBegin's note)
        out << "    leave\n    ret\n";
    }

    int fltPrintIdx_ = 0;
    bool emitPrintNullText(std::ostringstream &out, int & /*indent*/, bool isNil) override
    {
        std::string lbl = "_str" + std::to_string(strIdx++);
        dataSec.push_back(lbl + " db \"" + (isNil ? "nil" : "null") + "\", 0");
        out << "    lea rsi, [rel " << lbl << "]\n";
        out << "    lea rdi, [rel _fmt_s]\n";
        out << "    xor eax, eax\n    call printf\n";
        return true;
    }
    void emitPrint(std::ostringstream &out, int & /*indent*/, const std::string &val) override
    {
        if (isListVar(val))
        {
            // "[e0, e1, ...]" — matches PY's list repr. Elements are plain ints (same scope as
            // CStrategy's own ac_arr_print — nested/mixed-type arrays aren't a demonstrated case).
            int idx = strIdx++;
            std::string loopL = "_arrprint_loop" + std::to_string(idx);
            std::string skipL = "_arrprint_noskip" + std::to_string(idx);
            std::string doneL = "_arrprint_done" + std::to_string(idx);
            loadRAX(out, val);
            out << "    mov rbx, rax\n";              // rbx = array base
            out << "    mov r12, qword [rbx]\n";       // r12 = length
            out << "    mov rdi, 4096\n    call malloc\n";
            calledFuncs_.insert("malloc");
            out << "    mov r13, rax\n";               // r13 = output buffer
            out << "    mov byte [r13], '['\n";
            out << "    mov r14, r13\n    inc r14\n";  // r14 = write cursor
            out << "    xor r15, r15\n";
            out << loopL << ":\n";
            out << "    cmp r15, r12\n    jge " << doneL << "\n";
            out << "    test r15, r15\n    jz " << skipL << "\n";
            out << "    mov byte [r14], ','\n    inc r14\n";
            out << "    mov byte [r14], ' '\n    inc r14\n";
            out << skipL << ":\n";
            out << "    mov rdi, r14\n    mov rsi, 24\n    lea rdx, [rel _fmt_dbare]\n";
            out << "    mov rcx, qword [rbx + 8 + r15*8]\n";
            out << "    xor eax, eax\n    call snprintf\n";
            calledFuncs_.insert("snprintf");
            out << "    add r14, rax\n";
            out << "    inc r15\n    jmp " << loopL << "\n";
            out << doneL << ":\n";
            out << "    mov byte [r14], ']'\n    inc r14\n";
            out << "    mov byte [r14], 0\n";
            out << "    mov rsi, r13\n";
            out << "    lea rdi, [rel _fmt_s]\n";
            out << "    xor eax, eax\n    call printf\n";
            return;
        }
        if (isStr(val))
        {
            if (looksString(val))
            {
                std::string lbl = "_str" + std::to_string(strIdx++);
                dataSec.push_back(lbl + " db " + toNasmDbLit(val) + ", 0");
                out << "    lea rsi, [rel " << lbl << "]\n";
            }
            else
            {
                loadRAX(out, val);   // var/temp already holds a char* pointer
                out << "    mov rsi, rax\n";
            }
            out << "    lea rdi, [rel _fmt_s]\n";
        }
        else if (isFloatVal(val))
        {
            // Plain `%g` via printf drops the trailing decimal point on a whole-number double
            // (9.0 -> "9"), same class of bug fixed identically for C/C++/Rust/Go/JS this
            // session — format into a buffer first and append ".0" if nothing marks it as a
            // float already, then print the buffer as a string.
            int idx = fltPrintIdx_++;
            std::string haveDot = "_flt_havedot" + std::to_string(idx);
            out << "    mov rdi, 32\n    call malloc\n";
            calledFuncs_.insert("malloc");
            out << "    mov rbx, rax\n";
            out << "    mov rdi, rbx\n    mov rsi, 32\n    lea rdx, [rel _fmt_fbare]\n";
            loadXMM0(out, val);   // reload AFTER malloc — XMM isn't callee-saved across the call
            out << "    mov al, 1\n    call snprintf\n";
            calledFuncs_.insert("snprintf");
            out << "    mov r12, rax\n";   // chars written (excludes null terminator)
            out << "    mov rdi, rbx\n    lea rsi, [rel _fmt_dotcheck]\n    call strpbrk\n";
            calledFuncs_.insert("strpbrk");
            out << "    test rax, rax\n    jnz " << haveDot << "\n";
            out << "    mov byte [rbx + r12], '.'\n";
            out << "    mov byte [rbx + r12 + 1], '0'\n";
            out << "    mov byte [rbx + r12 + 2], 0\n";
            out << haveDot << ":\n";
            out << "    mov rsi, rbx\n";
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

    // Same gap+fix as CStrategy's own emitConfirm (see its comment) — base default never
    // assigns `res` at all, and unlike a text backend that at least fails at COMPILE time,
    // ASM would read whatever garbage bits happened to be in that never-written stack slot.
    void emitConfirm(std::ostringstream &out, int &indent, const std::string &res,
                     const std::string &val) override
    {
        emitPrint(out, indent, val);
        if (!res.empty()) out << "    mov qword [rbp-" << getSlot(res) << "], 0\n";
    }
    void emitHalt(std::ostringstream &out, int & /*indent*/) override
    {
        out << "    xor edi, edi\n    call fflush\n";   // flush stdout so /kill output survives abort
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
    // `raise hint(...)`/`raise toxic(...)`/`raise MyClause(...)` — non-fatal, prints "Prefix: msg"
    // to stderr on every other backend. ASM had no override at all, so it fell to
    // BackendStrategy's base default — which uses C-STYLE `/* ... */` comments as a placeholder,
    // invalid NASM syntax (a hard assemble-time error: "label or instruction expected"), AND
    // semantically a pure no-op even where it happened not to break the build (a comment prints
    // nothing) — `raise hint($x$)` silently did nothing on ASM while every other backend really
    // wrote to stderr. Fixed for real via fprintf(stderr, ...), matching CStrategy's approach.
    void emitRaiseClause(std::ostringstream &out, int & /*indent*/,
                         const std::string &clause, const std::string &msg) override
    {
        std::string prefix = clause == "hint" ? "Suggestion" : clause == "toxic" ? "Toxic" : clause;
        std::string lbl = "_raisefmt" + std::to_string(strIdx++);
        dataSec.push_back(lbl + " db \"" + prefix + ": %s\", 10, 0");
        loadRAX(out, msg.empty() ? std::string("\"\"") : msg);
        out << "    mov rdx, rax\n";
        out << "    mov rdi, [rel stderr]\n";
        out << "    lea rsi, [rel " << lbl << "]\n";
        out << "    xor eax, eax\n";
        out << "    call fprintf\n";
        calledFuncs_.insert("fprintf");
    }

    // Real try/catch via setjmp/longjmp — see emitHeader's `_ac_try_stack` comment for the full
    // rationale. `_ac_try_depth` is decremented on BOTH exit paths (normal fallthrough out of
    // the try body, and re-entry via longjmp) — same two-decrement-sites shape as CStrategy's
    // proven C version, translated from its `if/else` into explicit labels + jumps.
    void emitTryBegin(std::ostringstream &out, int & /*indent*/) override
    {
        int idx = tryIdx_++;
        tryStack_.push_back(idx);
        out << "    ; try begin\n";
        out << "    mov rax, [rel _ac_try_depth]\n    inc rax\n    mov [rel _ac_try_depth], rax\n";
        out << "    dec rax\n    imul rax, rax, 200\n";
        out << "    lea rdi, [rel _ac_try_stack]\n    add rdi, rax\n    call setjmp\n";
        calledFuncs_.insert("setjmp");
        out << "    test eax, eax\n    jnz .catch" << idx << "\n";
    }
    void emitCatchBegin(std::ostringstream &out, int & /*indent*/,
                        const std::string &exVar, const std::string &typeName) override
    {
        (void)typeName;
        int idx = tryStack_.back(); tryStack_.pop_back(); lastTryIdx_ = idx;
        afterPending_ = true;
        out << "    mov rax, [rel _ac_try_depth]\n    dec rax\n    mov [rel _ac_try_depth], rax\n";
        out << "    jmp .after" << idx << "\n";
        out << ".catch" << idx << ":\n";
        out << "    mov rax, [rel _ac_try_depth]\n    dec rax\n    mov [rel _ac_try_depth], rax\n";
        // Mirrors C's `long long exVar = 0;` — the exception var's stack slot
        // must be zero-initialized, else it holds whatever garbage was left
        // on the stack from before the try (matches C, no real message
        // propagation across the setjmp/longjmp model on either backend).
        if (!exVar.empty())
            out << "    mov qword [rbp-" << getSlot(exVar) << "], 0\n";
    }
    void emitAfterBegin(std::ostringstream &out, int & /*indent*/) override
    {
        out << ".after" << lastTryIdx_ << ":\n";
        afterPending_ = false;
    }
    void emitTryEnd(std::ostringstream &out, int & /*indent*/) override
    {
        // A plain try/catch with no explicit "after"/"always" clause never
        // triggers emitAfterBegin, but emitCatchBegin unconditionally jumps
        // to .after<idx> to skip the catch body on the success path — emit
        // the label here as the fallback definition so it's never dangling.
        if (afterPending_) {
            out << ".after" << lastTryIdx_ << ":\n";
            afterPending_ = false;
        }
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

    // Structured-if label stack: each frame is {elseLabel, endLabel, hasElse}.
    struct IfFrame { std::string elseLbl, endLbl; bool hasElse; };
    std::vector<IfFrame> ifStack_;
    void emitIfBegin(std::ostringstream &out, int & /*indent*/, const std::string &cond) override
    {
        // cond is a variable holding 0 or 1. Jump to the else/end label when false.
        loadRAX(out, cond);
        IfFrame fr;
        fr.elseLbl = "_Lelse" + std::to_string(strIdx++);
        fr.endLbl  = "_Lend"  + std::to_string(strIdx++);
        fr.hasElse = false;
        out << "    test rax, rax\n";
        out << "    jz " << fr.elseLbl << "\n";
        ifStack_.push_back(fr);
    }
    void emitIfElse(std::ostringstream &out, int & /*indent*/) override
    {
        if (ifStack_.empty()) return;
        IfFrame &fr = ifStack_.back();
        fr.hasElse = true;
        // then-branch falls through to here → skip the else body, then land the else label
        out << "    jmp " << fr.endLbl << "\n";
        out << fr.elseLbl << ":\n";
    }
    void emitIfEnd(std::ostringstream &out, int & /*indent*/) override
    {
        if (ifStack_.empty()) return;
        IfFrame fr = ifStack_.back();
        ifStack_.pop_back();
        // With no else, the false-jump target is the end; place elseLbl here too.
        if (!fr.hasElse)
            out << fr.elseLbl << ":\n";
        out << fr.endLbl << ":\n";
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
    // Tag markers as NASM comments — the inherited `// <tag>` is a NASM syntax error.
    void emitTagBegin(std::ostringstream &out, int & /*indent*/, const std::string &tag) override
    { out << "    ; <" << tag << ">\n"; }
    void emitTagEnd(std::ostringstream &out, int & /*indent*/, const std::string &tag) override
    { out << "    ; <" << tag << ">\n"; }
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
        // [rbp-8] is reserved for the caller's RBX (see emitFunctionEnd's matching restore) —
        // user locals/params start at [rbp-16]. RBX is callee-saved per the SysV ABI, but this
        // backend uses it pervasively as a short-lived scratch register (array/string/bundle
        // codegen especially) — without explicitly saving/restoring it, any such use silently
        // clobbers whatever the CALLER was keeping in RBX across this call. Concretely: a bundle
        // constructor holds the freshly malloc'd object pointer in RBX across `call
        // ClassName_init` (matches ABI convention: caller trusts RBX to survive) — `..._init`'s
        // own OWN field-store codegen ALSO used RBX as scratch, stomping the caller's pointer
        // before it returned. Segfaulted on the very next field read (verified with gdb: crashed
        // reading a field through a pointer that had turned into unrelated garbage).
        nextSlot = 8;
        (void)indent;
        std::string label = classOwner.empty() ? name : classOwner + "_" + name;
        if (!classOwner.empty() && name == "init") label = classOwner + "_init";
        definedFuncs_.insert(label);
        emitRaw(out, "");
        emitRaw(out, label + ":");
        // Was a blanket 32768 bytes (4096 slots) for EVERY function regardless of how many
        // slots it actually uses — chosen (see the prior fix's own comment, preserved below)
        // because a fixed-but-too-small reservation was a real, exploitable stack-corruption
        // bug: `getSlot()` hands out ever-larger offsets with no bound check against whatever
        // was reserved, so under-provisioning silently writes PAST the frame into whatever's
        // below. 32KB safely avoided that, but pays for it on EVERY call: touching 32KB of
        // fresh stack is far more cache/TLB pressure than a typical function (a handful of
        // locals, tens of bytes) needs, and in a hot/recursive call path that's real, avoidable
        // memory traffic. Exact size isn't known until the whole body is generated (nextSlot's
        // final value), so a placeholder is emitted here and swapped for the real, 16-byte-
        // aligned, minimum-64-byte size in postProcess() once emitFunctionEnd has it — same
        // safety property as the 32KB fix (still can't be too small, since it's computed from
        // the SAME getSlot() calls that would otherwise overflow it), zero waste otherwise.
        frameLabel_ = label;
        out << "    push rbp\n    mov rbp, rsp\n    sub rsp, " << frameToken(label) << "\n";
        out << "    mov [rbp-8], rbx\n";
        // Save incoming arguments into their stack slots. Without this the params read as garbage
        // (fibonacci returned all 0). System V ABI: first 6 integer args in rdi,rsi,rdx,rcx,r8,r9.
        static const char* argRegs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        std::istringstream ps(params); std::string pname; int ai = 0;
        while (std::getline(ps, pname, ',') && ai < 6) {
            size_t a = pname.find_first_not_of(' ');
            if (a == std::string::npos) continue;
            size_t z = pname.find_last_not_of(' ');
            pname = pname.substr(a, z - a + 1);
            out << "    mov [rbp-" << getSlot(pname) << "], " << argRegs[ai++] << "\n";
        }
        // Authoritative per-function, not just per-class: methods of DIFFERENT classes are
        // emitted sequentially and each needs `self.field` inside its OWN body resolved against
        // ITS class, not whatever emitClassBegin last set. Plain functions clear it (empty
        // classOwner) since `self` has no meaning outside a method.
        currentClass_ = classOwner;
    }
    void emitFunctionEnd(std::ostringstream &out, int &indent) override
    {
        (void)indent;
        out << "    xor eax, eax\n";
        out << "    mov rbx, [rbp-8]\n";   // restore caller's RBX (see emitFunctionBegin's note)
        out << "    leave\n    ret\n";
        recordFrameSize(frameLabel_);
        slot.clear();
        nextSlot = 8;
    }
    void emitClassBegin(std::ostringstream &out, int &indent,
                        const std::string &name) override
    {
        (void)indent;
        emitRaw(out, "; bundle " + name);
        currentClass_ = name;
        classFields_[name];   // ensure an entry exists even for a zero-field bundle
    }
    void emitFieldDecl(std::ostringstream & /*out*/, int & /*indent*/,
                       const std::string &field, IRType /*t*/) override
    {
        classFields_[currentClass_].push_back(field);
    }
    void emitClassEnd(std::ostringstream &out, int &indent) override
    {
        (void)out; (void)indent;
        currentClass_.clear();
    }
    bool suppressClassBody() const override { return true; }
    void emitConstructCall(std::ostringstream &out, int &indent, const std::string &res,
                           const std::string &className, const std::string &args) override
    {
        (void)indent;
        instanceClass_[res] = className;
        int n = (int)classFields_[className].size();
        out << "    mov rdi, " << (8 * (n > 0 ? n : 1)) << "\n    call malloc\n";
        calledFuncs_.insert("malloc");
        out << "    mov rbx, rax\n";              // rbx = new object ptr (callee-saved, survives init call)
        out << "    mov rdi, rbx\n";               // self = first arg
        if (!args.empty()) {
            static const char* argRegs[5] = {"rsi", "rdx", "rcx", "r8", "r9"};
            auto elems = splitElems(args);
            for (size_t k = 0; k < elems.size() && k < 5; k++) {
                loadRAX(out, elems[k]);
                out << "    mov " << argRegs[k] << ", rax\n";
            }
        }
        out << "    call " << className << "_init\n";
        calledFuncs_.insert(className + "_init");
        out << "    mov rax, rbx\n";
        storeRAX(out, res);
    }
    void emitMainBegin(std::ostringstream &out, int &indent) override
    {
        slot.clear();
        nextSlot = 0;
        emitRaw(out, "\nmain:");
        // Same exact-sizing fix as emitFunctionBegin (see its comment) — `main` is where the
        // original fixed-reservation bug actually manifested (the mainloop tends to accumulate
        // the most distinct temps/vars of any single scope in a program), so it's also the
        // scope with the most to gain from not reserving a blanket 32KB regardless.
        frameLabel_ = "main";
        out << "    push rbp\n    mov rbp, rsp\n    sub rsp, " << frameToken("main") << "\n";
        if (anyAtomicVars()) {
            out << "    lea rdi, [_ac_atomic_lock]\n    xor esi, esi\n    call pthread_mutex_init\n";
            calledFuncs_.insert("pthread_mutex_init");
        }
    }
    void emitMainEnd(std::ostringstream &out, int &indent) override
    {
        out << "    xor edi, edi\n    call exit\n";
        recordFrameSize("main");
        slot.clear();
        nextSlot = 0;
    }
};

// Detect params passed to GL calls (object names → string-typed on typed backends).
static std::set<std::string> detectStringParams(const AC_IR::IRFunction& fn,
                                                const AC_IR::SymbolTable& symbols) {
    using namespace AC_IR;
    std::set<std::string> pset(fn.parameters.begin(), fn.parameters.end());
    std::set<std::string> out;
    auto& S = const_cast<SymbolTable&>(symbols);
    for (const auto& ins : fn.instructions) {
        if (ins.opcode != IROpcode::CALL && ins.opcode != IROpcode::LIB_CALL) continue;
        if (ins.typedOperands.empty()) continue;
        std::string fnm;
        const auto& f0 = ins.typedOperands[0];
        if (f0.kind == IRRef::Kind::VAR && f0.id >= 0) fnm = S.getName(f0.id);
        else if (f0.kind == IRRef::Kind::CONST && f0.value.type == IRType::STRING)
            fnm = std::get<std::string>(f0.value.data);
        // The raw AC-level callee name is dotted ("gl.is_obj"), never colon- or underscore-form
        // at this pre-codegen stage — the old "gl:"/"gl_" checks never matched anything real,
        // so this detector was silently a no-op for every gl-taking-a-string-arg call (verified:
        // `reset_ball(arg)` — an event-callback whose body calls `ac_gl_is_obj(arg)` — declared
        // `arg: i64`, then failed passing a `&str` literal key name to it at the call site).
        // By the time this runs, the callee name is ALREADY normalized/renamed past the raw
        // AC-level dotted form — empirically "ac_gl_is_obj", not "gl.is_obj"/"gl:obj.is" (found
        // via a temporary debug print, since the earlier "gl:"/"gl_"-only check, itself already
        // a fix for the ORIGINAL "gl.is_obj"-shaped guess, ALSO never matched anything real).
        bool isGl = fnm.rfind("gl.", 0) == 0 || fnm.rfind("gl:", 0) == 0 || fnm.rfind("gl_", 0) == 0
                    || fnm.rfind("ac_gl_", 0) == 0 || fnm == "is_obj" || fnm == "is_draw";
        if (!isGl) continue;
        for (size_t ai = 1; ai < ins.typedOperands.size(); ai++) {
            const auto& r = ins.typedOperands[ai];
            if (r.kind == IRRef::Kind::VAR && r.id >= 0) {
                std::string n = S.getName(r.id);
                if (pset.count(n)) out.insert(n);
            }
        }
    }
    return out;
}

// Infer which locals/params are STRINGS from usage (fixpoint). Signals: assigned a string literal
// or a string-returning call; produced by string concat; compared to a string literal; passed to a
// stringm.* ilib; copied from a known string var. Lets block backends declare std::string/String,
// iterate as chars, etc. — the #6 foundation. Runs per-function over a snapshot of its instructions.
// Infer which locals/params are FLOATS (fixpoint) — #42 generalized. A var is float if assigned
// a float const, a DIV/FDIV result, a float-returning call, or arithmetic touching a float var.
// Typed backends declare these double/f64/float64 up front and cast int operands in mixed ops.
static std::set<std::string> detectFloatVars(const std::vector<AC_IR::IRInstruction>& insns,
                                             const AC_IR::SymbolTable& symbols,
                                             const std::set<std::string>& floatFuncs,
                                             const std::set<std::string>& seedFloat = {}) {
    using namespace AC_IR;
    auto& S = const_cast<SymbolTable&>(symbols);
    auto nm = [&](const IRRef& r) -> std::string {
        return (r.kind == IRRef::Kind::VAR && r.id >= 0) ? S.getName(r.id) : std::string();
    };
    std::set<std::string> flt = seedFloat;
    std::set<int> fltTemps;
    auto isF = [&](const IRRef& r) {
        if (r.kind == IRRef::Kind::CONST) return r.value.type == IRType::FLOAT;
        if (r.kind == IRRef::Kind::TEMP)  return fltTemps.count(r.id) > 0;
        if (r.kind == IRRef::Kind::VAR)   { std::string n = nm(r); return !n.empty() && flt.count(n) > 0; }
        return false;
    };
    bool changed = true;
    while (changed) {
        changed = false;
        auto mark = [&](const IRRef& r) {
            if (r.kind == IRRef::Kind::TEMP) { if (fltTemps.insert(r.id).second) changed = true; }
            else if (r.kind == IRRef::Kind::VAR) {
                std::string n = nm(r);
                if (!n.empty() && flt.insert(n).second) changed = true;
            }
        };
        for (const auto& ins : insns) {
            switch (ins.opcode) {
                case IROpcode::DIV: case IROpcode::FDIV:
                    if (ins.result.isValid()) mark(ins.result);
                    break;
                case IROpcode::ADD: case IROpcode::SUB:
                case IROpcode::MUL: case IROpcode::PMUL:
                    if (ins.result.isValid() && ins.typedOperands.size() >= 2
                        && (isF(ins.typedOperands[0]) || isF(ins.typedOperands[1])))
                        mark(ins.result);
                    break;
                case IROpcode::STORE_VAR:
                case IROpcode::LOAD_CONST: {   // folded call results arrive as LOAD_CONST
                    const IRRef* tgt = nullptr; const IRRef* val = nullptr;
                    if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) { tgt = &ins.typedOperands[0]; val = &ins.typedOperands[1]; }
                    else if (ins.result.isValid() && !ins.typedOperands.empty()) { tgt = &ins.result; val = &ins.typedOperands[0]; }
                    if (tgt && val && isF(*val)) mark(*tgt);
                    break;
                }
                case IROpcode::CALL:
                    if (ins.result.isValid() && !ins.typedOperands.empty()) {
                        const auto& c = ins.typedOperands[0];
                        std::string callee = (c.kind == IRRef::Kind::VAR || c.kind == IRRef::Kind::FUNCTION)
                            ? nm(c) : std::string();
                        if (callee.empty() && c.kind == IRRef::Kind::CONST && c.value.type == IRType::STRING)
                            callee = std::get<std::string>(c.value.data);
                        if (!callee.empty() && floatFuncs.count(callee)) mark(ins.result);
                    }
                    break;
                default: break;
            }
        }
    }
    // `mark()` tracks TEMP-kind results in `fltTemps` (by id) separately from VAR-kind results in
    // `flt` (by name) — but only `flt` is ever returned, so a float value that lands in a bare
    // TEMP (never assigned to a named variable — e.g. `Term.display math.pi(5)`, whose
    // constexpr-folded LOAD_CONST result is a TEMP printed directly, with no intervening STORE_VAR)
    // was silently invisible to every consumer of this function. Verified: `t_0 = ldc 3.14159;
    // print t_0` printed the raw double bit pattern as an integer on ASM (the one backend relying
    // purely on this string-keyed set with no fallback to `ins.resultType` at the print site).
    // Merge them in using the same "t_" + id naming convention this file uses everywhere else a
    // TEMP needs a string name (see IRRef::toStringWithSymbols / commonRef's own TEMP case).
    for (int id : fltTemps) flt.insert("t_" + std::to_string(id));
    return flt;
}

static std::set<std::string> detectStringVars(const std::vector<AC_IR::IRInstruction>& insns,
                                              const AC_IR::SymbolTable& symbols,
                                              const std::set<std::string>& seedStrings = {},
                                              const std::set<std::string>& strFuncs = {}) {
    using namespace AC_IR;
    auto& S = const_cast<SymbolTable&>(symbols);
    auto nm = [&](const IRRef& r) -> std::string {
        return (r.kind == IRRef::Kind::VAR && r.id >= 0) ? S.getName(r.id) : std::string();
    };
    auto nm2 = [&](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::TEMP) return "t_" + std::to_string(r.id);
        return nm(r);
    };
    auto isStrConst = [](const IRRef& r) {
        return r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING;
    };
    auto constFuncName = [&](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::CONST && r.value.type == IRType::STRING)
            return std::get<std::string>(r.value.data);
        return nm(r);
    };
    std::set<std::string> str = seedStrings;
    bool changed = true;
    while (changed) {
        changed = false;
        auto add = [&](const std::string& n) { if (!n.empty() && str.insert(n).second) changed = true; };
        auto known = [&](const IRRef& r) { return isStrConst(r) || (nm2(r).size() && str.count(nm2(r))); };
        for (const auto& ins : insns) {
            switch (ins.opcode) {
                case IROpcode::STORE_VAR:
                case IROpcode::LOAD_CONST: {   // folded results (iota, concat) arrive as LOAD_CONST
                    IRRef tgt, val;
                    if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) { tgt = ins.typedOperands[0]; val = ins.typedOperands[1]; }
                    else if (ins.result.isValid() && !ins.typedOperands.empty()) { tgt = ins.result; val = ins.typedOperands[0]; }
                    if ((tgt.kind == IRRef::Kind::VAR || tgt.kind == IRRef::Kind::TEMP)
                        && (known(val) || ins.resultType == IRType::STRING))
                        add(nm2(tgt));
                    break;
                }
                case IROpcode::ADD: // string concat rides ADD
                    // `x += "str"` lowers to `t_N = add x, "str"` (a TEMP result) THEN
                    // `x = stv t_N` — the old `result.kind == VAR` guard here only ever caught a
                    // concat stored DIRECTLY into a named var in one step, never one that landed
                    // in a TEMP first (i.e. every compound-assignment concat) — that TEMP was
                    // invisible to this whole fixpoint, so the SUBSEQUENT `stv x, t_N` couldn't
                    // see `t_N` as string either (`known(t_N)` false), and `x` never got marked
                    // (verified: examples/string_repeat.ac's `line += $ab$` in a loop — `line`
                    // silently defaulted to `i64`, empty-string init literal(`""`)and all).
                    if ((ins.result.kind == IRRef::Kind::VAR || ins.result.kind == IRRef::Kind::TEMP)
                        && ins.typedOperands.size() >= 2)
                        if (known(ins.typedOperands[0]) || known(ins.typedOperands[1]) || ins.resultType == IRType::STRING)
                            add(nm2(ins.result));
                    break;
                case IROpcode::EQ: case IROpcode::NEQ:
                    if (ins.typedOperands.size() >= 2 && (known(ins.typedOperands[0]) || known(ins.typedOperands[1]))) {
                        add(nm(ins.typedOperands[0])); add(nm(ins.typedOperands[1]));
                    }
                    break;
                case IROpcode::FOR_BEGIN: // iterating a string yields string chars, and vice-versa
                    if (ins.typedOperands.size() >= 2) {
                        std::string iv = nm(ins.typedOperands[0]), coll = nm(ins.typedOperands[1]);
                        if (iv.size() && str.count(iv))   add(coll);
                        if (coll.size() && str.count(coll)) add(iv);
                    }
                    break;
                case IROpcode::INPUT: // Term.ask always reads and returns text — unconditional,
                    // no `known(...)` gate needed (unlike STORE_VAR/CALL above, which only add
                    // when the SOURCE is already known-string). Missing this meant `name =
                    // Term.ask $...$` never propagated string-ness past the immediate result
                    // temp — Java declared the temp itself as String (a separate, local
                    // codegen-time check in emitInput), but the FOLLOWING `name = t_0` copy saw
                    // `known(t_0)` false here and declared `name` as `long`, "incompatible
                    // types: String cannot be converted to long" (verified: ask_input.ac).
                    if (ins.result.kind == IRRef::Kind::VAR || ins.result.kind == IRRef::Kind::TEMP)
                        add(nm2(ins.result));
                    break;
                case IROpcode::CALL:
                    if ((ins.result.kind == IRRef::Kind::VAR || ins.result.kind == IRRef::Kind::TEMP)
                        && !ins.typedOperands.empty()) {
                        std::string cf = constFuncName(ins.typedOperands[0]);
                        if (BackendStrategy::isAcStrFunc(cf) || strFuncs.count(cf))
                            add(nm2(ins.result));
                    }
                    break;
                case IROpcode::LIB_CALL: {
                    if (ins.typedOperands.empty()) break;
                    std::string f = constFuncName(ins.typedOperands[0]);
                    if (f.find("stringm") != std::string::npos)   // stringm.* args are strings
                        for (size_t k = 1; k < ins.typedOperands.size(); k++) add(nm(ins.typedOperands[k]));
                    // Same VAR-only gap the CALL case above already had fixed (see its comment) —
                    // this one's result lands in a TEMP just as often (e.g. `low = speech.lower()`
                    // — the string-cheese rewrite emits a LIB_CALL result into a TEMP, not a named
                    // var) and `nm()` returns empty for TEMP, silently skipping it; use `nm2()`
                    // and accept TEMP too (verified: jarvis.ac's `low = speech.lower()` then
                    // `resolve(low)` — Java declared `low` as `long`, "incompatible types: String
                    // cannot be converted to long", the immediate LIB_CALL result temp itself WAS
                    // correctly string-typed by a separate, Java-local mechanism, but that never
                    // propagated to `low` because this whole-program pre-pass never marked the
                    // temp as string in the first place).
                    if ((ins.result.kind == IRRef::Kind::VAR || ins.result.kind == IRRef::Kind::TEMP)
                        && BackendStrategy::isAcStrFunc(f))
                        add(nm2(ins.result));
                    break;
                }
                default: break;
            }
        }
    }
    return str;
}

// Last-type-wins re-typing (#retype): among vars the string inference marked, find those whose
// LAST assignment is actually NUMERIC. `x=5; x=$hi$` → last is string → stays string-unified
// (numbers get ac_to_str'd). `x=$5$; x=5` → last is numeric → the var is NUMERIC-unified: it's
// removed from the string set, and the earlier string literal gets ac_unstring'd (compile-time
// strip + auto). Dead earlier assignments are DCE'd first, so ac_unstring only ever sees a LIVE
// stringified number — never a genuine string (that would be mangled). Returns the numeric-unified set.
static std::set<std::string> detectNumericRetype(const std::vector<AC_IR::IRInstruction>& insns,
                                                 const AC_IR::SymbolTable& symbols,
                                                 const std::set<std::string>& stringVars) {
    using namespace AC_IR;
    auto& S = const_cast<SymbolTable&>(symbols);
    auto nm = [&](const IRRef& r) -> std::string {
        return (r.kind == IRRef::Kind::VAR && r.id >= 0) ? S.getName(r.id) : std::string();
    };
    // See detectStringVars' own nm2 (same purpose): a value living in a TEMP (e.g. the `t_N` in
    // `x += "str"`'s `t_N = add x, "str"; x = stv t_N`) needs the same "t_"+id naming to be
    // looked up in `stringVars` at all — checking `val.kind == VAR` alone (the old behavior)
    // silently treated every TEMP-held value as "not a string", so a var whose ONLY assignment
    // came through a temp (any compound-assignment concat) got wrongly numeric-retyped even
    // though detectStringVars (once ITS OWN matching TEMP gap above is fixed) correctly knows
    // it's a string.
    auto nm2 = [&](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::TEMP) return "t_" + std::to_string(r.id);
        return nm(r);
    };
    std::map<std::string, bool> lastIsString;   // var → was its most-recent assignment a string?
    for (const auto& ins : insns) {
        IRRef tgt, val; bool have = false;
        if (ins.opcode == IROpcode::STORE_VAR && ins.typedOperands.size() >= 2) {
            tgt = ins.typedOperands[0]; val = ins.typedOperands[1]; have = true;
        } else if ((ins.opcode == IROpcode::STORE_VAR || ins.opcode == IROpcode::TYPE_CAST
                 || ins.opcode == IROpcode::LOAD_CONST) && ins.result.isValid() && !ins.typedOperands.empty()) {
            tgt = ins.result; val = ins.typedOperands[0]; have = true;
        }
        if (!have) continue;
        std::string v = nm(tgt);
        if (v.empty() || !stringVars.count(v)) continue;
        bool valStr = ins.resultType == IRType::STRING
                    || (val.kind == IRRef::Kind::CONST && val.value.type == IRType::STRING)
                    || ((val.kind == IRRef::Kind::VAR || val.kind == IRRef::Kind::TEMP) && stringVars.count(nm2(val)));
        lastIsString[v] = valStr;
    }
    std::set<std::string> numeric;
    for (auto& [v, lastStr] : lastIsString) if (!lastStr) numeric.insert(v);
    return numeric;
}

// Detect which parameters of a function hold LISTS, from usage: indexed (arr[i]),
// measured (length arr), iterated (FOR v in arr), or appended (arr.append).
// Infer which PARAMETERS are floats from local usage — detectFloatVars' fixpoint only ever marks
// a var as the RESULT of a float-producing instruction (a DIV/FDIV's target, arithmetic against a
// known-float operand, etc.); a parameter is never assigned inside its own function (it arrives
// pre-set from the caller), so it never becomes a "result" and the fixpoint can never mark it —
// even though `x / 2.0` inside the function proves `x` must hold real float bits at the call site.
// Mirrors detectListParams' same "infer from local usage, not caller analysis" shape. Needed
// concretely by AsmStrategy: unlike the typed backends (which paper over this by casting `(double)
// (x)` at every use regardless of x's declared type), ASM's raw-bit-slot model must know BEFORE
// emitting code whether a slot holds int bits (needs cvtsi2sd) or already-double bits (needs a
// plain movq reinterpret) — get it wrong and a real double bit pattern gets int-to-double
// "converted" as if it were a huge integer (verified: nsqrt(2.0) printed 2199023954602.623).
static std::set<std::string> detectFloatParams(const AC_IR::IRFunction& fn,
                                                const AC_IR::SymbolTable& symbols) {
    using namespace AC_IR;
    std::set<std::string> paramSet(fn.parameters.begin(), fn.parameters.end());
    std::set<std::string> out;
    auto opName = [&](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::VAR && r.id >= 0)
            return const_cast<SymbolTable&>(symbols).getName(r.id);
        return "";
    };
    auto isFloatConst = [](const IRRef& r) {
        return r.kind == IRRef::Kind::CONST && r.value.type == IRType::FLOAT;
    };
    for (const auto& ins : fn.instructions) {
        if ((ins.opcode == IROpcode::DIV || ins.opcode == IROpcode::FDIV
          || ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
          || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::PMUL)
            && ins.typedOperands.size() >= 2) {
            for (int side = 0; side < 2; side++) {
                std::string n = opName(ins.typedOperands[side]);
                if (paramSet.count(n) && isFloatConst(ins.typedOperands[1 - side]))
                    out.insert(n);
            }
        }
    }
    return out;
}
static std::set<std::string> detectListParams(const AC_IR::IRFunction& fn,
                                              const AC_IR::SymbolTable& symbols) {
    using namespace AC_IR;
    std::set<std::string> paramSet(fn.parameters.begin(), fn.parameters.end());
    std::set<std::string> out;
    auto opName = [&](const IRRef& r) -> std::string {
        if (r.kind == IRRef::Kind::VAR && r.id >= 0)
            return const_cast<SymbolTable&>(symbols).getName(r.id);
        return "";
    };
    for (const auto& ins : fn.instructions) {
        switch (ins.opcode) {
        case IROpcode::LOAD_INDEX:
        case IROpcode::STORE_INDEX:
            if (!ins.typedOperands.empty()) {
                std::string n = opName(ins.typedOperands[0]);
                if (paramSet.count(n)) out.insert(n);
            }
            break;
        case IROpcode::FOR_BEGIN:
            if (ins.typedOperands.size() >= 2) {
                std::string n = opName(ins.typedOperands[1]);
                if (paramSet.count(n)) out.insert(n);
            }
            break;
        case IROpcode::CALL:
            if (ins.typedOperands.size() >= 2 && opName(ins.typedOperands[0]) == "ac_length") {
                std::string n = opName(ins.typedOperands[1]);
                if (paramSet.count(n)) out.insert(n);
            }
            break;
        case IROpcode::LIB_CALL:
            if (!ins.typedOperands.empty()) {
                const auto& m = ins.typedOperands[0];
                std::string mname;
                if (m.kind == IRRef::Kind::VAR && m.id >= 0)
                    mname = const_cast<SymbolTable&>(symbols).getName(m.id);
                else if (m.kind == IRRef::Kind::CONST && m.value.type == IRType::STRING)
                    mname = std::get<std::string>(m.value.data);
                auto dot = mname.find(".append");
                if (dot != std::string::npos && paramSet.count(mname.substr(0, dot)))
                    out.insert(mname.substr(0, dot));
            }
            break;
        default: break;
        }
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// UNIFIED IR CODE GENERATOR
// ═══════════════════════════════════════════════════════════════════════════

// Compiler-synthesized var names (list-repeat, short-circuit, cond scrutinee) must be
// invisible to the loop save/restore + free-var machinery, like _ac_-prefixed ones.
static bool isSyntheticVar(const std::string& n) {
    return n.rfind("_ac_", 0) == 0 || n.rfind("ac_rep", 0) == 0 ||
           n.rfind("ac_sc_", 0) == 0 || n.rfind("ac_cond_", 0) == 0;
}

class UnifiedIRCodeGen
{
    const IRProgram &ir;
    std::unique_ptr<BackendStrategy> strategy;
    std::ostringstream out;
    int indentLevel = 0;
    int loopDepth_ = 0;
    std::set<std::string> globalVarNames_;
    // Symbol IDs backing globalVarNames_ (see freeVarSymIds' own comment for why name-only
    // matching isn't safe for the cross-function promotion checks below).
    std::set<int> globalVarSymIds_;
    std::set<std::string> globalStringVars_; // string vars promoted to C global scope (before main)
    std::vector<std::string> freeVarNames_; // vars assigned at global scope (depth 0)
    std::set<std::string> definedSoFar_;    // global vars assigned before the current instruction
    std::vector<std::vector<std::string>> scopeSaveStack_; // per-loop saved-var sets (enter/exit must match)
    std::vector<std::string> promotedGlobalsList_; // free vars promoted to true globals (NA→free)
    std::map<std::string, std::string> structGlobalsList_; // subset of the above that are struct-typed (var -> ctor name)
    bool inFunctionBody_ = false;           // true while emitting a function body (not mainloop)
    std::vector<std::pair<std::string,std::string>> libImports_; // populated in generate() before genInstr
    std::vector<std::string> usingHeaders_; // namespaces declared with "using header X" (insertion order)
    std::unordered_map<std::string,std::string> usingSymbolNS_; // bare symbol → namespace (from ACL)
    bool usingNamespaceIlib_ = false;    // "using namespace ilib" — promotes all ilib libs into usingHeaders_
    std::set<std::string> userFuncNames_; // user-defined function names; unqualified calls not in this set get namespace prefix
    std::set<std::string> classNames_;    // bundle/class names — `c = ClassName()` is a construct-call
    std::map<std::string,std::string> instanceVarClass_;  // var -> class name, for backends without dotCallSyntax()
    std::set<std::string> protoFloatFuncs_, protoListFuncs_, protoStringFuncs_; // fwd-decl return types
    std::set<std::string> voidUserFuncs_;   // user functions with no `return <value>;` anywhere
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
        // Track which global vars exist at this point in the mainloop, so loop
        // save/restore only touches vars that are actually defined before the loop
        // (saving a not-yet-assigned var was a NameError/compile error on every backend).
        if (!inFunctionBody_ && i.result.kind == IRRef::Kind::VAR) {
            std::string vn = ref(i.result);
            if (!vn.empty() && !isSyntheticVar(vn)) definedSoFar_.insert(vn);
        }
        if (!inFunctionBody_ && i.opcode == IROpcode::STORE_VAR
            && !i.typedOperands.empty() && i.typedOperands[0].kind == IRRef::Kind::VAR) {
            std::string vn = ref(i.typedOperands[0]);
            if (!vn.empty() && !isSyntheticVar(vn)) definedSoFar_.insert(vn);
        }

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
        case IROpcode::FDIV:
        case IROpcode::IDIV:
        case IROpcode::MOD:
        {
            if (i.typedOperands.size() >= 2) {
                if (i.opcode == IROpcode::FDIV) {
                    strategy->emitFloatDivision(out, indentLevel, ref(i.result),
                                                ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else if (i.opcode == IROpcode::DIV) {
                    strategy->emitTrueDivision(out, indentLevel, ref(i.result),
                                               ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else if (i.opcode == IROpcode::IDIV) {
                    strategy->emitIntDiv(out, indentLevel, ref(i.result),
                                         ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else if (i.opcode == IROpcode::MOD) {
                    strategy->emitMod(out, indentLevel, ref(i.result),
                                      ref(i.typedOperands[0]), ref(i.typedOperands[1]));
                } else {
                    std::string op = (i.opcode == IROpcode::ADD) ? "+"
                            : (i.opcode == IROpcode::SUB) ? "-"
                            : "*";
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
            std::string op = (i.opcode == IROpcode::AND) ? "&" : "|";
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

        // ── bitwise band / bor / bxor / bnot ───────────────────────────
        // &, | and ^ are the native bitwise operators in every text target
        // (PY/JS/C/C++/Java/Go/Rust/V), so emitBinaryOp works for all of them.
        case IROpcode::BAND:
        case IROpcode::BOR:
        case IROpcode::BXOR:
            if (i.typedOperands.size() >= 2) {
                std::string op = (i.opcode == IROpcode::BAND) ? "&"
                        : (i.opcode == IROpcode::BOR)  ? "|" : "^";
                strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]), op);
            }
            break;
        case IROpcode::BNOT:
            // bnot x == x ^ -1 in two's complement — true in every text target.
            if (!i.typedOperands.empty())
                strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                       ref(i.typedOperands[0]), "-1", "^");
            break;

        // ── ptm / ptd: LITERAL bit shifts (for speed) ──────────────────
        case IROpcode::PTM:
        case IROpcode::PTD:
            if (i.typedOperands.size() >= 2)
                strategy->emitBinaryOp(out, indentLevel, ref(i.result),
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]),
                                       i.opcode == IROpcode::PTM ? "<<" : ">>");
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
            // Mainloop free vars are only in scope in the global section; a loop inside a
            // function body must NOT save/restore them (they aren't visible there).
            // Save only vars that are DEFINED at this point (saving a var first assigned
            // after the loop referenced it before assignment). Stack keeps enter/exit paired.
            if (!inFunctionBody_) {
                std::vector<std::string> saveSet;
                for (const auto &v : freeVarNames_)
                    if (definedSoFar_.count(v)) saveSet.push_back(v);
                scopeSaveStack_.push_back(saveSet);
                strategy->emitScopeEnter(out, indentLevel, saveSet, loopDepth_);
            }
            loopDepth_++;
            strategy->emitWhileBegin(out, indentLevel,
                                     i.typedOperands.empty() ? "" : ref(i.typedOperands[0]));
            break;

        case IROpcode::WHILE_END:
            strategy->emitWhileEnd(out, indentLevel);
            loopDepth_--;
            if (!inFunctionBody_) {
                std::vector<std::string> saveSet;
                if (!scopeSaveStack_.empty()) { saveSet = scopeSaveStack_.back(); scopeSaveStack_.pop_back(); }
                strategy->emitScopeExit(out, indentLevel, saveSet, loopDepth_);
            }
            break;

        case IROpcode::FOR_BEGIN:
            if (i.typedOperands.size() >= 2)
            {
                if (!inFunctionBody_) {
                    std::vector<std::string> saveSet;
                    for (const auto &v : freeVarNames_)
                        if (definedSoFar_.count(v)) saveSet.push_back(v);
                    scopeSaveStack_.push_back(saveSet);
                    strategy->emitScopeEnter(out, indentLevel, saveSet, loopDepth_);
                }
                loopDepth_++;
                strategy->emitForBegin(out, indentLevel,
                                       ref(i.typedOperands[0]), ref(i.typedOperands[1]));
            }
            break;

        case IROpcode::FOR_END:
            strategy->emitForEnd(out, indentLevel);
            loopDepth_--;
            if (!inFunctionBody_) {
                std::vector<std::string> saveSet;
                if (!scopeSaveStack_.empty()) { saveSet = scopeSaveStack_.back(); scopeSaveStack_.pop_back(); }
                strategy->emitScopeExit(out, indentLevel, saveSet, loopDepth_);
            }
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

            // random builtins: random.number(n) → ac_rand(n); random.choice(list) → ac_choice(list)
            if (rawName == "random.number") func = "ac_rand";
            else if (rawName == "random.choice") func = "ac_choice";

            // INDIRECT call: operand[0] is a TEMP holding a callable value
            // (funcs[i](x) — the callee was computed, not named).
            if (!i.typedOperands.empty() && i.typedOperands[0].kind == IRRef::Kind::TEMP)
            {
                std::string iargs;
                for (size_t ai = 1; ai < i.typedOperands.size(); ai++) {
                    if (ai > 1) iargs += ", ";
                    iargs += ref(i.typedOperands[ai]);
                }
                std::string ires = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
                strategy->emitIndirectCall(out, indentLevel, ires, func, iargs);
                break;
            }

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
                else if (aop.kind == IRRef::Kind::VAR && aop.id >= 0) {
                    // Namespaced ilib call (e.g. stringm.upper) → const char* ABI needs .c_str()
                    bool ilibCall = rawName.find('.') != std::string::npos;
                    std::string a = ref(aop);
                    bool isStr = ir.symbols.getType(aop.id) == IRType::STRING
                                 || strategy->isStringVar(a);
                    args += ilibCall ? strategy->libArgRef(a, isStr)
                                     : strategy->valueArgRef(a, ir.symbols.getType(aop.id));
                }
                else
                    args += ref(aop);
            }
            std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
            // See voidUserFuncs_'s comment: a call to a genuinely void user function still
            // carries a result temp/var in the IR — discard it so backends take their existing
            // bare-statement path instead of declaring a variable with no value to hold.
            if (!res.empty() && voidUserFuncs_.count(rawName)) res = "";
            // native-cpu's carried-over pointer functions (ptr_new, ptr_deref, ...) are bare
            // ilib calls, not calls through a function-typed parameter — misclassifying them as
            // indirect made Java route them through emitIndirectCall's `.applyAsLong()` fallback
            // (meant for genuine functional-parameter calls), which can't resolve since no such
            // local/param exists. Same bare names on every backend, so excluding them here is safe
            // universally: the other backends' emitCall/emitIndirectCall already produce identical
            // output for a bare name either way.
            static const std::set<std::string> nativeCpuBareFuncs = {
                "ptr_new", "ptr_deref", "ptr_null", "ptr_is_null",
                "ptr_eq", "ptr_copy", "ptr_update", "ptr_free",
            };
            // Indirect call: callee is a parameter variable, not a known user function
            bool isIndirect = !rawName.empty()
                              && !userFuncNames_.count(rawName)
                              && !BackendStrategy::isAcStrFunc(rawName)  // real string builtins
                              && !nativeCpuBareFuncs.count(rawName)
                              && rawName != "ac_iota" && rawName != "ac_stream"
                              && rawName != "ac_ipow" && rawName != "ac_length"
                              && rawName.find('.') == std::string::npos
                              && !i.typedOperands.empty()
                              && i.typedOperands[0].kind == IRRef::Kind::VAR;
            if (!rawName.empty() && classNames_.count(rawName)) {
                strategy->emitConstructCall(out, indentLevel, res, func, args);
                if (!res.empty()) instanceVarClass_[res] = rawName;
            }
            else if (isIndirect)
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
            if (!i.typedOperands.empty()) {
                const auto& pop = i.typedOperands[0];
                bool handled = false;
                if (pop.kind == IRRef::Kind::CONST && pop.value.type == IRType::STRING) {
                    std::string s = std::get<std::string>(pop.value.data);
                    if (s.size() >= 2 && s.front() == '$' && s.back() == '$') s = s.substr(1, s.size() - 2);
                    if (s == "null" || s == "nil") {
                        handled = strategy->emitPrintNullText(out, indentLevel, s == "nil");
                        // Feed `save as`'s capture buffer the same real text (a quoted string
                        // literal satisfies isStr()'s looksString check directly, so the existing
                        // capture path materializes it exactly like any other string constant).
                        if (handled) strategy->emitCapture(out, indentLevel, "\"" + s + "\"");
                    }
                }
                if (!handled) {
                    strategy->emitPrint(out, indentLevel, ref(pop));
                    strategy->emitCapture(out, indentLevel, ref(pop));
                }
            }
            break;

        case IROpcode::SAVE_FILE: {
            std::string fname = i.typedOperands.empty() ? "\"ac_save.txt\"" : ref(i.typedOperands[0]);
            strategy->emitSaveFile(out, indentLevel, fname);
            break;
        }

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
            else if (methodRaw == "quickthread" && i.typedOperands.size() > 1)
            {
                std::string fn = ref(i.typedOperands[1]);
                std::string args;
                for (size_t ai = 2; ai < i.typedOperands.size(); ai++) {
                    if (ai > 2) args += ", ";
                    args += ref(i.typedOperands[ai]);
                }
                strategy->emitQuickThread(out, indentLevel, fn, args);
            }
            else if ((methodRaw == "foreign") &&
                     i.typedOperands.size() > 1)
            {
                // Pass the RAW code — ref() formats it as a string literal, escaping
                // newlines to "\n" and producing a one-line syntax error in the output.
                const auto& fop = i.typedOperands[1];
                std::string rawCode = (fop.kind == IRRef::Kind::CONST && fop.value.type == IRType::STRING)
                                      ? std::get<std::string>(fop.value.data) : ref(fop);
                strategy->emitForeign(out, indentLevel, rawCode);
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
                // Generic lib/object call — ref() already handles dot→underscore via
                // preserveDots in commonRef() for a VAR/FUNCTION-kind callee (the normal case,
                // from real AC source). A CONST-kind callee (e.g. ir.cpp's injectAutoShutoff,
                // which builds its `maudio.stop` callee as a bare string constant, not a symbol
                // reference) skips that path entirely — commonRef's CONST branch just quotes the
                // text verbatim, dot intact — so stripQuotes alone left a literal, un-flattened
                // "maudio.stop" reaching emitCall, invalid syntax on any backend without real
                // dot-call support (verified: C, "maudio.stop();" — undeclared identifier).
                // formatCallName is a no-op on every backend that doesn't need flattening.
                std::string func = strategy->formatCallName(stripQuotes(method));
                std::string args;
                for (size_t j = 1; j < i.typedOperands.size(); j++)
                {
                    if (j > 1)
                        args += ", ";
                    std::string a = ref(i.typedOperands[j]);
                    bool isStr = i.typedOperands[j].kind == IRRef::Kind::VAR
                                 && strategy->isStringVar(a);
                    args += strategy->libArgRef(a, isStr);
                }
                std::string res = (i.result.kind == IRRef::Kind::NONE) ? "" : ref(i.result);
                // Bundle method call on a known instance (`c.greet()`), on a backend with no
                // real dot-call syntax (only C today — every other backend natively supports
                // `instance.method()`): `methodRaw` (raw, undot-flattened) is "c.greet" — look
                // up "c"'s class and call the free function directly: `Critter_greet(&c, ...)`.
                // `func`/`method` above went through ref()/formatRef(), which for C would have
                // already flattened this same "c.greet" to a disconnected `c_greet` — bypass
                // that path entirely here instead of trying to unpick it after the fact.
                if (!strategy->dotCallSyntax()) {
                    auto dot = methodRaw.find('.');
                    if (dot != std::string::npos) {
                        std::string instVar = methodRaw.substr(0, dot);
                        auto cit = instanceVarClass_.find(instVar);
                        if (cit != instanceVarClass_.end()) {
                            std::string methodName = methodRaw.substr(dot + 1);
                            std::string callArgs = "&" + instVar + (args.empty() ? "" : ", " + args);
                            strategy->emitCall(out, indentLevel, res, cit->second + "_" + methodName, callArgs);
                            break;
                        }
                    }
                }
                strategy->emitCall(out, indentLevel, res, func, args);
            }
            break;
        }

        case IROpcode::EVENT_BIND:
            if (i.typedOperands.size() >= 2)
                // funcArgRef (not plain ref()): the callback is a FUNCTION VALUE, not a call —
                // backends that need a qualified/method-reference form to pass a bare function
                // by value (Java: `Main::__keycb_space`) need that here too, same as any other
                // "pass a user function as a value" site (see the CALL-argument path above).
                strategy->emitEventBind(out, indentLevel,
                                        ref(i.typedOperands[0]),
                                        strategy->funcArgRef(ref(i.typedOperands[1])));
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
                    // `bound` variant: function-scoped persistence — must NOT become a global
                    if (!instr.attrs.empty() && instr.attrs[0] == "bound") continue;
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
                    // Any instruction that writes a *named* var (STORE_VAR, or an arithmetic
                    // op like `counter = counter + 1` whose result is the var) counts as a
                    // write that must promote to the free scope.
                    if (instr.result.kind == IRRef::Kind::VAR)
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
        // Pre-scan: LIST parameters (usage-detected). Without this, typed backends
        // declared array params as plain integers and the target compiler rejected
        // every function that receives an array.
        std::set<std::string> fnListParams = detectListParams(func, ir.symbols);
        strategy->setListParams(fnListParams);

        std::set<std::string> fnStringParams = detectStringParams(func, ir.symbols);
        strategy->setStringParams(fnStringParams);
        // #6: infer string-typed locals/params (seeded with string params) so backends declare and
        // iterate them as strings.
        std::set<std::string> fnStringVars = detectStringVars(func.instructions, ir.symbols, fnStringParams, protoStringFuncs_);
        for (const auto& nv : detectNumericRetype(func.instructions, ir.symbols, fnStringVars))
            fnStringVars.erase(nv);   // #retype: last-assign numeric → NOT string-unified
        strategy->setStringVars(fnStringVars);

        // Pre-scan: detect function return type (float or list)
        // Note: list literals use mkTemp() → Kind::TEMP; named vars use Kind::VAR. Check both.
        // #floatret: which vars/temps this function computes as float (DIV/FDIV results and
        // anything assigned from one). Declared here (outside the block below) so it can ALSO
        // drive setFloatParams (before emitFunctionBegin, so a param's signature type agrees
        // with how the body treats it) and setFloatVarsFull (below, replacing a redundant
        // second computation of the exact same thing) — see the #floatret comment further down
        // for the bug this fixes (examples/math_number.ac's `collatz`).
        std::set<std::string> fnFloatVars = detectFloatVars(func.instructions, ir.symbols, protoFloatFuncs_,
                                                              detectFloatParams(func, ir.symbols));
        {
            bool retFloat = false, retList = false, retString = false;
            bool hasValueReturn = false;
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
            // The old rule here (any FDIV instruction anywhere in the function -> whole function
            // is float-returning) was a real, confirmed bug: `collatz(n)` (examples/
            // math_number.ac) uses `n / 2` internally (AC's `/` on a non-constant operand always
            // lowers to FDIV — it can't be proven to divide evenly at compile time) but RETURNS
            // `steps`, a plain int accumulator wholly unrelated to that division — the blanket
            // rule still declared `-> f64` and then `return steps;` (an i64) was a hard type
            // mismatch on Rust. Now traced via fnFloatVars (see its own comment above): only the
            // ACTUAL returned value's float-ness matters.
            for (const auto &instr : func.instructions) {
                // Same DIV-vs-FDIV gap as the whole-program floatFuncs scan below (see its
                // comment): a `/` that provably needs float gets rewritten to FDIV by an earlier
                // ir.cpp pass, so checking only DIV here missed it — the function's OWN return
                // type declaration (e.g. C's `ac_int nsqrt(...)`) stayed int while the caller-side
                // fix (setFloatReturnFuncs) correctly upgraded the CALL SITE to double, silently
                // truncating the real double value on the implicit double->int return conversion.
                // (See #floatret above: this now only fires when the RETURNED value itself is
                // float-derived, traced via fnFloatVars, not for any unrelated FDIV in the body.)
                if (instr.opcode == IROpcode::RETURN && !instr.typedOperands.empty()) {
                    hasValueReturn = true;
                    const auto &op = instr.typedOperands[0];
                    if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::FLOAT) retFloat = true;
                    if (isVarLike(op) && fnFloatVars.count(refName(op))) retFloat = true; // #floatret
                    if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::STRING) retString = true;
                    if (op.kind == IRRef::Kind::VAR && fnStringVars.count(refName(op))) retString = true; // #6
                    if (isVarLike(op)) {
                        std::string rname = refName(op);
                        if (fnListParams.count(rname)) retList = true; // returning a list param
                        for (const auto& ai : func.instructions) {
                            if (!isVarLike(ai.result) || refName(ai.result) != rname) continue;
                            // returning a computed STRING temp/var (e.g. `return $hi $ + name`) → string fn (#6)
                            if (ai.resultType == IRType::STRING) retString = true;
                            if (ai.opcode == IROpcode::ALLOC
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
            if (retString) strategy->setReturnIsString(true);
            if (!hasValueReturn) strategy->setReturnIsVoid(true);
        }

        // Pre-scan: cross-block locals → hoist set (fixes #41 on block-scoped backends C/C++/V).
        // A var "escapes" if its first-reference block-path is NOT a prefix of a later reference's
        // path (used in a sibling/outer scope than where it'd be declared). Then/else are distinct
        // scopes, so IF_ELSE opens a new block id. Hoisted vars are declared at the function top.
        {
            std::map<std::string, IRType> hoist;
            std::map<std::string, std::vector<int>> firstPath;
            std::map<std::string, IRType> firstType;
            std::vector<int> stk = {0};
            int nextId = 1;
            // Function names are NOT hoistable locals — a CALL's callee rides in the operands as a
            // VAR ref, so a recursive fn (`ack` calling `ack`) would otherwise hoist `long long ack`
            // inside `ack`, shadowing the function → "ack cannot be used as a function".
            std::set<std::string> funcNames;
            for (const auto& fn : ir.functions) funcNames.insert(fn.name);
            auto isPrefix = [](const std::vector<int>& a, const std::vector<int>& b) {
                if (a.size() > b.size()) return false;
                for (size_t i = 0; i < a.size(); i++) if (a[i] != b[i]) return false;
                return true;
            };
            auto note = [&](const std::string& v, IRType t) {
                // A CALL's callee ref can be a dotted namespace name (`maudio.speak`, `regex.match`)
                // resolved as a single VAR-kind symbol, not a real AC variable — no genuine AC
                // identifier contains '.'. Without this exclusion it could get flagged as a
                // cross-block "hoistable local" and emitted as a syntactically invalid
                // declaration (verified regression: examples/audio_test.ac, Rust's
                // "let mut maudio.speak: i64 = 0;" — "expected one of `:`, `;`, `=`, `@`, or `|`").
                if (v.empty() || isSyntheticVar(v) || paramSet.count(v) || funcNames.count(v)
                    || v.find('.') != std::string::npos) return;
                auto it = firstPath.find(v);
                if (it == firstPath.end()) { firstPath[v] = stk; firstType[v] = t; }
                else if (!isPrefix(it->second, stk)) {
                    IRType ht = firstType[v];
                    hoist[v] = (ht == IRType::VOID && t != IRType::VOID) ? t : ht;
                }
            };
            for (const auto& ins : func.instructions) {
                // record every VAR referenced (result + operands) at the current block path
                if (ins.result.kind == IRRef::Kind::VAR) note(ref(ins.result), ins.resultType);
                if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                    note(ref(ins.typedOperands[0]), ins.resultType);
                // A CALL/LIB_CALL's operand[0] is the CALLEE, not a data reference — a VAR-kind
                // callee ref is a real, in-scope function/namespace name (`maudio.speak`, which
                // C's own formatCallName flattens to `maudio_speak` — a name that can ALSO
                // collide with a real C preprocessor macro from an ilib header), never a
                // hoistable local. The '.'-based exclusion elsewhere in this file only helps
                // backends that haven't already flattened the dot by the time `ref()` resolves
                // it; skipping the callee slot outright is the actually-robust fix (verified
                // regression: examples/audio_test.ac, C's "ac_int maudio_speak = 0;" colliding
                // with `#define maudio_speak ac_maudio_speak` from machine_audio_c.h).
                bool isCallOpcode = ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL;
                for (size_t oi = 0; oi < ins.typedOperands.size(); oi++) {
                    if (isCallOpcode && oi == 0) continue;
                    const auto& op = ins.typedOperands[oi];
                    if (op.kind == IRRef::Kind::VAR) note(ref(op), IRType::VOID);
                }
                // then adjust the block path
                switch (ins.opcode) {
                    case IROpcode::WHILE_BEGIN: case IROpcode::FOR_BEGIN: case IROpcode::IF_BEGIN:
                        stk.push_back(nextId++); break;
                    case IROpcode::IF_ELSE:
                        if (stk.size() > 1) stk.back() = nextId++; break;
                    case IROpcode::WHILE_END: case IROpcode::FOR_END: case IROpcode::IF_END:
                        if (stk.size() > 1) stk.pop_back(); break;
                    default: break;
                }
            }
            strategy->setHoistVars(hoist);
        }

        // #floatparam: a param's SIGNATURE type must agree with how the body treats it (see the
        // #floatret comment above) — restrict fnFloatVars to just the parameter names and set it
        // BEFORE emitFunctionBegin (which is what actually emits the typed param list), then
        // reuse the SAME set for setFloatVarsFull just after (previously recomputed separately,
        // AFTER emitFunctionBegin had already run with only the narrower detectFloatParams()
        // signal — verified: examples/math_number.ac's `collatz(n)`, `n / 2` treated `n` as
        // float everywhere in the body but the signature still said `n: i64`).
        {
            std::set<std::string> fnFloatParams;
            for (const auto& p : func.parameters) if (fnFloatVars.count(p)) fnFloatParams.insert(p);
            strategy->setFloatParams(fnFloatParams);
        }
        strategy->emitFunctionBegin(out, indentLevel, func.name, params, func.classOwner);
        strategy->setFloatVarsFull(fnFloatVars);
        inFunctionBody_ = true;
        for (const auto &instr : func.instructions)
            genInstr(instr);
        inFunctionBody_ = false;
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
        bool hasSave   = false;
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
                // conglomer <foo.h> is native-only: a raw C header can't be honored portably.
                // Error LOUDLY on non-native backends instead of silently emitting broken output
                // (the AC program would call C functions the target can't provide). Wrap it as an
                // ilib/flib for portable use.
                if (libType == "conglomer" && ir.backend != "C" && ir.backend != "CPP" && ir.backend != "C++"
                    && ir.backend != "LIB") {
                    throw ACError::conglomerNativeOnly(libName, ir.backend);
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

        bool hasOS = false;
        auto checkOS = [&](const IRInstruction& ins) {
            if (ins.opcode == IROpcode::RAISE_CLAUSE || ins.opcode == IROpcode::SOFT_HALT)
                hasOS = true;
            if (ins.opcode == IROpcode::LIB_CALL && !ins.typedOperands.empty()) {
                const auto& op0 = ins.typedOperands[0];
                if (op0.kind == IRRef::Kind::CONST && op0.value.type == IRType::STRING
                    && std::get<std::string>(op0.value.data) == "raise")
                    hasOS = true;
            }
        };
        // See setUsedBuiltinOps' comment: which misc runtime-helper builtins this program
        // actually invokes, so a backend like Python that injects one function per builtin can
        // gate each rather than emitting every one into every generated file unconditionally.
        bool hasDivOp = false, hasIpowOp = false, hasLengthOp = false, hasAddOp = false, hasRandomOp = false, hasIdivOp = false, hasEvalOp = false, hasTryOp = false;
        auto checkBuiltinOps = [&](const IRInstruction& ins) {
            if (ins.opcode == IROpcode::DIV) hasDivOp = true;
            if (ins.opcode == IROpcode::IDIV) hasIdivOp = true;
            if (ins.opcode == IROpcode::ADD) hasAddOp = true;
            if (ins.opcode == IROpcode::EVAL || ins.opcode == IROpcode::LAZY_EVAL) hasEvalOp = true;
            if (ins.opcode == IROpcode::TRY_BEGIN) hasTryOp = true;
            if (ins.opcode == IROpcode::CALL && !ins.typedOperands.empty()) {
                const auto& c = ins.typedOperands[0];
                std::string cn;
                if (c.kind == IRRef::Kind::VAR && c.id >= 0) cn = ir.symbols.getName(c.id);
                else if (c.kind == IRRef::Kind::CONST && c.value.type == IRType::STRING)
                    cn = std::get<std::string>(c.value.data);
                if (cn == "ac_ipow") hasIpowOp = true;
                else if (cn == "ac_length") hasLengthOp = true;
                else if (cn == "random.number" || cn == "random.choice") hasRandomOp = true;
            }
        };
        for (auto& fn : ir.functions)
            for (auto& ins : fn.instructions) {
                if (ins.opcode == IROpcode::INPUT)      hasInput  = true;
                if (ins.opcode == IROpcode::EVENT_BIND) hasEvents = true;
                if (ins.opcode == IROpcode::SAVE_FILE)  hasSave   = true;
                checkOS(ins);
                checkBuiltinOps(ins);
                scanImport(ins);
            }
        {
            int scanDepth = 0;
            std::set<std::string> freeVarSet;
            // Parallel symbol-ID set for the SAME depth-0 mainloop writes as freeVarSet, keyed
            // by ir.symbols ID rather than resolved name text. Needed because AC allows a
            // completely unrelated function to declare its OWN local using the exact same NAME
            // as a mainloop free var (verified real bug: keyword_catalog_core.ac's mainloop
            // `total` — genuinely promoted, a loop there needs it — got conflated with an
            // unrelated `bound total` local inside a DIFFERENT function purely because the
            // cross-function "does this function reference a free var" check below matched by
            // NAME STRING alone; AC's symbol table gives each DECLARATION its own ID even when
            // names collide, so ID equality is the only way to tell "same variable" from
            // "different variable, same spelling" apart).
            std::set<int> freeVarSymIds;
            std::set<std::string> loopMutatedVars;   // depth-0 vars WRITTEN inside a loop = accumulators
            int lastStoredVarSymId = -1;   // set by storedVarName() as an out-of-band side channel
            auto storedVarName = [&](const IRInstruction& ins) -> std::string {
                lastStoredVarSymId = -1;
                if (ins.opcode != IROpcode::STORE_VAR) return "";
                if (ins.typedOperands.size() >= 2 && ins.typedOperands[0].kind == IRRef::Kind::VAR) {
                    lastStoredVarSymId = ins.typedOperands[0].id;
                    return ref(ins.typedOperands[0]);
                }
                if (ins.result.isValid() && !ins.typedOperands.empty() && ins.result.kind == IRRef::Kind::VAR) {
                    lastStoredVarSymId = ins.result.id;
                    return ref(ins.result);
                }
                return "";
            };
            for (auto& ins : ir.globalInit) {
                if (ins.opcode == IROpcode::INPUT)      hasInput  = true;
                if (ins.opcode == IROpcode::EVENT_BIND) hasEvents = true;
                if (ins.opcode == IROpcode::SAVE_FILE)  hasSave   = true;
                checkOS(ins);
                checkBuiltinOps(ins);
                // NOTE: Do NOT mark mainloop variables as globals — they're local to mainloop!
                // Track scope depth and collect free vars (assigned at depth 0, not in loops)
                if (ins.opcode == IROpcode::WHILE_BEGIN || ins.opcode == IROpcode::FOR_BEGIN)
                    scanDepth++;
                else if (ins.opcode == IROpcode::WHILE_END || ins.opcode == IROpcode::FOR_END)
                    scanDepth--;
                else if (scanDepth == 0) {
                    // A plain STORE_VAR is the common case, but a CALL (or LIB_CALL/CONSTRUCT
                    // etc.) can also write a named var DIRECTLY via its own `.result` — e.g.
                    // `name_inp = call ask, root, 45` has no separate STORE_VAR at all (confirmed
                    // via --stop-after-ir). storedVarName() only recognizes STORE_VAR, so this
                    // shape was silently invisible to the whole free-var scan below it — any
                    // mainloop var assigned this way (every ilib widget constructor call: ask/
                    // display/dropdown/...) never became a "free" candidate, so a callback
                    // function referencing it later (see the READ-side promotion fix a few dozen
                    // lines down) had nothing to promote even after that fix. Catch both shapes.
                    // CONST_DECL's `.result` is ALSO a VAR (e.g. `const limit = 10`), but a const
                    // is by definition never written again — treating it as a "write" wrongly
                    // pulled it into loop save/restore (which needs to WRITE the var to restore
                    // it), producing a hard C++ "assignment of read-only variable" error on a
                    // program that never actually mutates it anywhere (verified: const_demo.ac's
                    // `const limit = 10` used only as a `FOR i in range limit` bound).
                    std::string varName = storedVarName(ins);
                    int symId = lastStoredVarSymId;
                    if (varName.empty() && ins.opcode != IROpcode::CONST_DECL
                            && ins.result.kind == IRRef::Kind::VAR && ins.result.id >= 0) {
                        varName = ref(ins.result);
                        symId = ins.result.id;
                    }
                    if (!varName.empty() && !isSyntheticVar(varName)) {
                        freeVarSet.insert(varName);
                        if (symId >= 0) freeVarSymIds.insert(symId);
                    }
                }
                else if (scanDepth > 0) {
                    // A depth-0 var also written inside a loop is an accumulator (`line += …`, which
                    // lowers to `line = add line, …` — an ADD result, NOT a STORE_VAR). The loop
                    // save/restore would reset it after the loop; matching the text backends to BNY
                    // (which accumulates), such vars are exempt from save/restore. Catch ANY write:
                    // a STORE_VAR target OR any instruction whose result is a VAR.
                    std::string varName = storedVarName(ins);
                    if (varName.empty() && ins.result.kind == IRRef::Kind::VAR && ins.result.id >= 0)
                        varName = ref(ins.result);
                    if (!varName.empty() && !isSyntheticVar(varName))
                        loopMutatedVars.insert(varName);
                }
                scanImport(ins);
            }
            freeVarNames_.assign(freeVarSet.begin(), freeVarSet.end());
            // NA→free promotion: a function that *writes* one of these mainloop free vars
            // mutates the free scope (spec rule 2). genFunction emits the backend's global
            // declaration only for vars a function actually assigns (reference-aware), so this
            // clean depth-0 set is safe — unlike the old all-stores set that caused corruption.
            globalVarNames_.insert(freeVarSet.begin(), freeVarSet.end());
            globalVarSymIds_.insert(freeVarSymIds.begin(), freeVarSymIds.end());
            // …but a loop-accumulator is exempt from the loop SAVE/RESTORE (not from global decl),
            // so its in-loop mutation persists (text backends now match BNY).
            if (!loopMutatedVars.empty())
                freeVarNames_.erase(
                    std::remove_if(freeVarNames_.begin(), freeVarNames_.end(),
                        [&](const std::string& v){ return loopMutatedVars.count(v); }),
                    freeVarNames_.end());
            // A mainloop STRING var is separately hoisted to a real (immutable, non-`mut`)
            // global string constant elsewhere (see stringOnlyGlobals a bit further down, and
            // globalStringVars_) — it never needs loop save/restore at all (its value never
            // changes), and on Rust specifically, emitScopeExit's restore assignment is a hard
            // compile error against a plain (non-`mut`) `static` (verified: gl_bounce.ac's
            // `Paddle`/`Ball` object-name string consts, used inside a loop, produced "cannot
            // assign" — `static Paddle: &str` has no mutability to assign into at all).
            {
                std::set<std::string> earlyStringGlobals;
                for (const auto& ins : ir.globalInit)
                    if (ins.opcode == IROpcode::ALLOC && ins.result.kind == IRRef::Kind::VAR
                        && ins.typedOperands.size() >= 1 && ins.typedOperands[0].kind == IRRef::Kind::CONST
                        && ins.typedOperands[0].value.type == IRType::STRING
                        && std::get<std::string>(ins.typedOperands[0].value.data) == "string")
                        earlyStringGlobals.insert(ref(ins.result));
                if (!earlyStringGlobals.empty())
                    freeVarNames_.erase(
                        std::remove_if(freeVarNames_.begin(), freeVarNames_.end(),
                            [&](const std::string& v){ return earlyStringGlobals.count(v); }),
                        freeVarNames_.end());
            }
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
        // Collect vars to promote to true module/file globals (for JS/HTML/Go). Two sources:
        //  (a) explicit `free` declarations inside functions, and
        //  (b) NA→free: mainloop free vars (globalVarNames_) that a function *writes* — the
        //      same reference-aware set genFunction uses for backend `global` decls. Without
        //      (b), such a var becomes a function-local shadow instead of mutating free scope.
        {
            std::vector<std::string> promoted;
            std::set<std::string> seen;
            // A mainloop var that's a STRING (`Ball = $Ball$`) is ALREADY separately hoisted to
            // a real global string constant by a later, unrelated pass (populates
            // globalStringVars_ — too late to consult here, it hasn't run yet). Promoting it a
            // SECOND time as an i64 `static mut` (the only type this promotion path has ever
            // supported) collides with that first declaration under the exact same name and, on
            // Rust, is also a straight type error (declaring the same string var as i64) —
            // verified real bug: gl_bounce.ac's `Ball` string const conflicted with itself once
            // promoted, "the name `_AC_FREE_Ball` is defined multiple times". Scan for these
            // ourselves, early, rather than depend on the later pass's ordering.
            std::set<std::string> stringOnlyGlobals;
            for (const auto& ins : ir.globalInit)
                if (ins.opcode == IROpcode::ALLOC && ins.result.kind == IRRef::Kind::VAR
                    && ins.typedOperands.size() >= 1 && ins.typedOperands[0].kind == IRRef::Kind::CONST
                    && ins.typedOperands[0].value.type == IRType::STRING
                    && std::get<std::string>(ins.typedOperands[0].value.data) == "string")
                    stringOnlyGlobals.insert(ref(ins.result));
            for (const auto& fn : ir.functions)
                for (const auto& ins : fn.instructions)
                    if (ins.opcode == IROpcode::FREE_DECL && !ins.typedOperands.empty())
                    {
                        // `bound` = function-scoped; never promote to a module global
                        if (!ins.attrs.empty() && ins.attrs[0] == "bound") continue;
                        std::string name = ref(ins.typedOperands[0]);
                        if (!stringOnlyGlobals.count(name) && seen.insert(name).second)
                            promoted.push_back(name);
                    }
            for (const auto& fn : ir.functions) {
                std::set<std::string> paramSet(fn.parameters.begin(), fn.parameters.end());
                for (const auto& ins : fn.instructions) {
                    if (ins.result.kind == IRRef::Kind::VAR) {
                        std::string name = ref(ins.result);
                        // Match by SYMBOL ID, not just resolved name text — an unrelated
                        // function can declare its own local using the exact same name as a
                        // genuinely-promoted mainloop var (verified real bug, see
                        // globalVarSymIds_' own comment); a name-only match wrongly promotes —
                        // really, wrongly CONFLATES — that unrelated local too.
                        if (!paramSet.count(name) && globalVarNames_.count(name)
                                && globalVarSymIds_.count(ins.result.id)
                                && !stringOnlyGlobals.count(name)
                                && seen.insert(name).second)
                            promoted.push_back(name);
                    }
                    // Same NA->free inference, extended to READS: a function that only ever
                    // CALLS a method on / passes / reads a mainloop-scope var (never assigns to
                    // it — e.g. a button callback doing `name_inp.get()`) never hit the
                    // ins.result branch above at all, since the var only ever appears as an
                    // OPERAND, never as a write target. Confirmed real: `applicant_form.ac`'s
                    // OnSubmit callback reads 6 mainloop-declared widget vars (name_inp,
                    // email_inp, ..., status_lbl) purely via .get()/.set() calls and never
                    // assigns any of them — they were silently never promoted, so every
                    // statically-scoped backend (C/C++/Rust/Java/Go/V) failed to compile with
                    // "not declared in this scope" while PY/JS's own always-global mainloop
                    // vars masked the same gap by accident.
                    for (const auto& op : ins.typedOperands) {
                        std::string name;
                        bool haveSymId = false;
                        if (op.kind == IRRef::Kind::VAR) {
                            name = (op.id >= 0) ? ir.symbols.getName(op.id) : ref(op);
                            haveSymId = op.id >= 0;
                        } else if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::STRING) {
                            name = std::get<std::string>(op.value.data);
                        } else {
                            continue;
                        }
                        // Symbol-ID match required when we have one (a real VAR operand) — see
                        // globalVarSymIds_'s comment. A CONST-STRING operand (the "receiver.method"
                        // combined-name shape just below) has no symbol ID of its own to check;
                        // that narrower case is left name-only, same as before.
                        if (!paramSet.count(name) && globalVarNames_.count(name)
                                && (!haveSymId || globalVarSymIds_.count(op.id))
                                && !stringOnlyGlobals.count(name)
                                && seen.insert(name).second)
                            promoted.push_back(name);
                        // A method call's callee arrives as ONE combined "receiver.method" VAR
                        // name (e.g. `t_0 = call name_inp.get` — confirmed via --stop-after-ir),
                        // not as a separate receiver operand — so the plain lookup above never
                        // matches "name_inp" itself. Extract the receiver portion and check it too.
                        auto dot = name.find('.');
                        if (dot != std::string::npos) {
                            std::string recv = name.substr(0, dot);
                            if (!paramSet.count(recv) && globalVarNames_.count(recv)
                                    && !stringOnlyGlobals.count(recv)
                                    && seen.insert(recv).second)
                                promoted.push_back(recv);
                        }
                    }
                }
            }
            if (!promoted.empty()) {
                strategy->setPromotedGlobals(promoted);
                strategy->setPromotedGlobalSymIds(globalVarSymIds_);
            }
            promotedGlobalsList_ = promoted; // reused for C/compiled file-scope global emission

            // Which of these promoted vars are STRUCT-typed (ilib widget constructor calls,
            // e.g. `name_inp = call ask, root, 45` in mainloop) rather than scalar/list — needed
            // by CppStrategy (see setStructGlobals) to declare a pointer-typed file-scope global
            // instead of a plain one, since these struct types have no default constructor.
            // Only mainloop (ir.globalInit) assignments are checked: every observed case is a
            // widget constructed once at the top of <mainloop> and referenced later by a
            // callback function, never constructed inside a user function.
            if (!promoted.empty()) {
                static const std::set<std::string> widgetCtors = {
                    "Screen", "display", "ask", "btn", "ckbtn", "radbtn", "dropdown",
                    "advance", "slider", "group", "tabs", "scroller", "listbox", "table", "sketch"
                };
                std::set<std::string> promotedSet(promoted.begin(), promoted.end());
                std::map<std::string, std::string> structGlobals;
                for (const auto& ins : ir.globalInit) {
                    if (ins.opcode != IROpcode::CALL || ins.result.kind != IRRef::Kind::VAR
                        || ins.typedOperands.empty())
                        continue;
                    std::string var = ref(ins.result);
                    if (!promotedSet.count(var) || structGlobals.count(var)) continue;
                    std::string callee = ref(ins.typedOperands[0]);
                    auto dot = callee.rfind('.');
                    std::string bareCallee = (dot != std::string::npos) ? callee.substr(dot + 1) : callee;
                    if (widgetCtors.count(bareCallee)) structGlobals[var] = bareCallee;
                }
                if (!structGlobals.empty()) strategy->setStructGlobals(structGlobals);
                structGlobalsList_ = structGlobals;
            }
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
                for (const auto& ins : insns) {
                    if (ins.opcode == IROpcode::TYPE_CAST && ins.result.kind == IRRef::Kind::VAR)
                        varCastTypes[ref(ins.result)] = ins.resultType;
                    // A `short`/`mini`-typed arithmetic result (temp OR var) must be declared at its
                    // real width too — else a `long`/`i64` temp assigned back into a `short` var is a
                    // lossy-conversion error on the strict backends (`short a = a * 4`).
                    else if (irIntWidth(ins.resultType) && ins.result.isValid()
                             && (ins.result.kind == IRRef::Kind::VAR || ins.result.kind == IRRef::Kind::TEMP)
                             && (ins.opcode == IROpcode::ADD || ins.opcode == IROpcode::SUB
                              || ins.opcode == IROpcode::MUL || ins.opcode == IROpcode::PMUL
                              || ins.opcode == IROpcode::IDIV || ins.opcode == IROpcode::MOD))
                        varCastTypes[ref(ins.result)] = ins.resultType;
                }
            };
            scanCasts(ir.globalInit);
            for (const auto& fn : ir.functions) scanCasts(fn.instructions);
            if (!varCastTypes.empty())
                strategy->setVarCastTypes(varCastTypes);
        }

        // Build user-defined function name set for unqualified-call resolution
        userFuncNames_.clear();
        classNames_.clear();
        for (const auto& fn : ir.functions) {
            userFuncNames_.insert(fn.name);
            // `c = Critter()` (bare bundle instantiation) is a CALL whose callee is the class
            // name, not the "init" method name — without this, the shared isIndirect heuristic
            // (below) can't tell it apart from a genuine call through a function-typed
            // parameter, and routes it through emitIndirectCall's `.applyAsLong()`-style
            // fallback (Java) or otherwise-wrong output on any backend that treats "unknown
            // bare callee" as indirect.
            if (fn.name == "init" && !fn.classOwner.empty()) {
                userFuncNames_.insert(fn.classOwner);
                classNames_.insert(fn.classOwner);
            }
        }
        {
            std::map<std::string, int> arity;
            for (const auto& fn : ir.functions) arity[fn.name] = (int)fn.parameters.size();
            if (!arity.empty()) strategy->setUserFuncArity(arity);
        }
        // A CALL instruction ALWAYS allocates a result temp/var in the IR, even when the callee
        // is genuinely void (no `return <value>;` anywhere in its body) — every OTHER call site
        // (indirect calls, calls whose result is a `t_N` nobody reads) tolerates this fine, but a
        // statically-typed backend trying to declare that temp with a real type against a truly
        // void callee is a hard compile error (verified on BOTH C and Rust: examples/geodeo.ac's
        // `<terrain>` tag lowers to `fn spawn_terrain() -> void`/`void spawn_terrain()`, called as
        // `t_8 = call spawn_terrain` — C: "void value not ignored as it ought to be"; Rust:
        // "expected i64, found ()" — a genuine, pre-existing, cross-backend bug, not something
        // introduced this session). voidUserFuncs_ lets the CALL case below null out `res` for
        // exactly these callees, so every backend's existing `res.empty()` bare-statement path
        // (already there for indirect/void-result calls) handles it correctly.
        for (const auto& fn : ir.functions) {
            bool hasValueReturn = false;
            for (const auto& ins : fn.instructions)
                if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()) { hasValueReturn = true; break; }
            if (!hasValueReturn) voidUserFuncs_.insert(fn.name);
        }

        // Build set of user-defined functions that return float (contain a DIV instruction)
        {
            std::set<std::string> floatFuncs;
            std::set<std::string> listFuncs;
            std::set<std::string> stringFuncs;
            std::map<std::string, std::set<int>> userFloatParamIdx;
            for (const auto& fn : ir.functions) {
                std::set<std::string> fnSV; bool fnSVdone = false;
                auto fnStringVars = [&]() -> const std::set<std::string>& {
                    if (!fnSVdone) { fnSV = detectStringVars(fn.instructions, ir.symbols, detectStringParams(fn, ir.symbols)); fnSVdone = true; }
                    return fnSV;
                };
                // #floatret (caller-facing half — see the identical fix + comment at the
                // per-function emitFunctionBegin call site above for the callee-facing half):
                // trace the ACTUAL returned value's float-ness instead of "does a float division
                // happen ANYWHERE in this function" — the old blanket rule marked a function
                // float-returning from any unrelated internal FDIV (verified: examples/
                // math_number.ac's `collatz` uses `n / 2` internally but returns `steps`, a
                // plain int — callers still declared `let t: f64 = collatz(6);`, a hard type
                // mismatch once the callee's OWN signature was correctly fixed to `-> i64`).
                std::set<std::string> fnFV; bool fnFVdone = false;
                auto fnFloatVars = [&]() -> const std::set<std::string>& {
                    if (!fnFVdone) { fnFV = detectFloatVars(fn.instructions, ir.symbols, {}, detectFloatParams(fn, ir.symbols)); fnFVdone = true; }
                    return fnFV;
                };
                // #floatparam (call-site half): a param this function's OWN body treats as float
                // (see #floatparam at the emitFunctionBegin call site) needs its CALLERS to pass
                // an f64-typed argument too — a bare int literal (`collatz(6)`) or an
                // int-typed caller-side var doesn't implicitly convert on Rust.
                for (size_t pi = 0; pi < fn.parameters.size(); pi++)
                    if (fnFloatVars().count(fn.parameters[pi]))
                        userFloatParamIdx[fn.name].insert((int)pi);
                for (const auto& ins : fn.instructions) {
                    if (ins.opcode == IROpcode::RETURN && !ins.typedOperands.empty()) {
                        const auto& rv = ins.typedOperands[0];
                        auto gRefName = [&](const IRRef& r) -> std::string {
                            if (r.kind == IRRef::Kind::VAR && r.id >= 0) return ir.symbols.getName(r.id);
                            if (r.kind == IRRef::Kind::TEMP) return "t_" + std::to_string(r.id);
                            return "";
                        };
                        if (rv.kind == IRRef::Kind::CONST && rv.value.type == IRType::FLOAT)
                            floatFuncs.insert(fn.name);
                        if (rv.kind == IRRef::Kind::CONST && rv.value.type == IRType::STRING)
                            stringFuncs.insert(fn.name);   // returns a string literal
                        bool rvVarLike = rv.kind == IRRef::Kind::VAR || rv.kind == IRRef::Kind::TEMP;
                        if (rvVarLike) {
                            std::string rname = gRefName(rv);
                            if (fnFloatVars().count(rname)) floatFuncs.insert(fn.name);   // #floatret
                            if (fnStringVars().count(rname)) stringFuncs.insert(fn.name);  // #6: returns a string var
                            if (detectListParams(fn, ir.symbols).count(rname))
                                listFuncs.insert(fn.name);   // returns a list param
                            for (const auto& ai : fn.instructions) {
                                bool aiVarLike = ai.result.kind == IRRef::Kind::VAR || ai.result.kind == IRRef::Kind::TEMP;
                                if (!aiVarLike || gRefName(ai.result) != rname) continue;
                                // returning a computed STRING temp/var (e.g. `return $hi $ + name`) → string fn (#6)
                                if (ai.resultType == IRType::STRING) stringFuncs.insert(fn.name);
                                if (ai.opcode == IROpcode::ALLOC
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
            if (!userFloatParamIdx.empty()) strategy->setUserFloatParams(userFloatParamIdx);
            if (!floatFuncs.empty()) strategy->setFloatReturnFuncs(floatFuncs);
            if (!listFuncs.empty()) strategy->setListReturnFuncs(listFuncs);
            if (!stringFuncs.empty()) strategy->setStringReturnFuncs(stringFuncs);
            protoFloatFuncs_ = floatFuncs;
            protoListFuncs_  = listFuncs;
            protoStringFuncs_ = stringFuncs;
        }

        strategy->setNeedsInput(hasInput);
        strategy->setNeedsEvents(hasEvents);
        strategy->setNeedsOS(hasOS);
        strategy->setNeedsSave(hasSave);
        strategy->setUsedBuiltinOps(hasDivOp, hasIpowOp, hasLengthOp, hasAddOp, hasRandomOp, hasIdivOp, hasEvalOp, hasTryOp);
        strategy->setPendingImports(libImports_);
        strategy->setImportSymbols(importSymbols);

        strategy->emitHeader(out);

        // For C/C++/LIB/RS/Java backends: emit string ALLOC from globalInit as global variables
        // so they are visible inside event callback functions defined before main().
        if (ir.backend == "C" || ir.backend == "CPP" || ir.backend == "C++" || ir.backend == "LIB" ||
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

        // NA→free for C: emit numeric free vars as file-scope globals so a function and main
        // share one variable instead of each declaring a local shadow. (Other compiled
        // backends — C++/Rust/Java — still pending; see scoping_implementation_plan.md.)
        if (ir.backend == "C" && !promotedGlobalsList_.empty()) {
            // Infer double if any writer of v carries a FLOAT-typed result/operand; else ac_int.
            // (Float free vars in C are a rare case; default int covers counters/accumulators.)
            auto isFloatGlobal = [&](const std::string& v) {
                auto scan = [&](const std::vector<IRInstruction>& code) {
                    for (const auto& ins : code) {
                        if (ins.result.kind == IRRef::Kind::VAR && ref(ins.result) == v) {
                            if (ins.result.value.type == IRType::FLOAT) return true;
                            for (const auto& op : ins.typedOperands)
                                if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::FLOAT)
                                    return true;
                        }
                    }
                    return false;
                };
                if (scan(ir.globalInit)) return true;
                for (const auto& fn : ir.functions) if (scan(fn.instructions)) return true;
                return false;
            };
            // C4: a promoted global that holds a LIST must be declared ac_int* at file scope
            // (and the mainloop ALLOC must ASSIGN it, not shadow it with a local).
            auto isListGlobal = [&](const std::string& v) {
                auto scan = [&](const std::vector<IRInstruction>& code) {
                    for (const auto& ins : code)
                        if (ins.opcode == IROpcode::ALLOC && ins.result.kind == IRRef::Kind::VAR
                            && ref(ins.result) == v && !ins.typedOperands.empty()
                            && ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[0].value.type == IRType::STRING
                            && std::get<std::string>(ins.typedOperands[0].value.data) == "list")
                            return true;
                    return false;
                };
                if (scan(ir.globalInit)) return true;
                for (const auto& fn : ir.functions) if (scan(fn.instructions)) return true;
                return false;
            };
            std::set<std::string> listGlobals;
            for (const auto &v : promotedGlobalsList_) {
                if (globalStringVars_.count(v)) continue; // already emitted as string global
                if (structGlobalsList_.count(v)) out << "ac_widget_t " << v << ";\n";
                else if (isListGlobal(v)) { listGlobals.insert(v); out << "ac_int* " << v << ";\n"; }
                else out << (isFloatGlobal(v) ? "double " : "ac_int ") << v << ";\n";
            }
            strategy->setListGlobals(listGlobals);
            out << "\n";
        }

        // NA->free for C++: same idea as the C block above, plus struct-typed globals (ilib
        // widget constructor results — display/btn/dropdown/...). Those have no default
        // constructor, so a plain `TypeName v;` file-scope declaration won't compile; declared
        // as `TypeName* v = nullptr;` instead, actually constructed later at the var's first
        // mainloop assignment via `new` (see decl()'s own comment on that half), and accessed
        // through `->` everywhere else (see emitCall()'s comment on that half). Emitted BEFORE
        // widgets.hpp's own struct definitions are included isn't a problem here since this
        // whole file-scope block already runs after emitHeader's #include block (same insertion
        // point the C block above already uses).
        if ((ir.backend == "CPP" || ir.backend == "C++" || ir.backend == "LIB") && !promotedGlobalsList_.empty()) {
            auto isFloatGlobal = [&](const std::string& v) {
                auto scan = [&](const std::vector<IRInstruction>& code) {
                    for (const auto& ins : code) {
                        if (ins.result.kind == IRRef::Kind::VAR && ref(ins.result) == v) {
                            if (ins.result.value.type == IRType::FLOAT) return true;
                            for (const auto& op : ins.typedOperands)
                                if (op.kind == IRRef::Kind::CONST && op.value.type == IRType::FLOAT)
                                    return true;
                        }
                    }
                    return false;
                };
                return scan(ir.globalInit) ||
                       std::any_of(ir.functions.begin(), ir.functions.end(),
                                   [&](const auto& fn){ return scan(fn.instructions); });
            };
            auto isListGlobal = [&](const std::string& v) {
                auto scan = [&](const std::vector<IRInstruction>& code) {
                    for (const auto& ins : code)
                        if (ins.opcode == IROpcode::ALLOC && ins.result.kind == IRRef::Kind::VAR
                            && ref(ins.result) == v && !ins.typedOperands.empty()
                            && ins.typedOperands[0].kind == IRRef::Kind::CONST
                            && ins.typedOperands[0].value.type == IRType::STRING
                            && std::get<std::string>(ins.typedOperands[0].value.data) == "list")
                            return true;
                    return false;
                };
                return scan(ir.globalInit) ||
                       std::any_of(ir.functions.begin(), ir.functions.end(),
                                   [&](const auto& fn){ return scan(fn.instructions); });
            };
            for (const auto &v : promotedGlobalsList_) {
                auto sit = structGlobalsList_.find(v);
                if (sit != structGlobalsList_.end()) {
                    out << sit->second << "* " << v << " = nullptr;\n";
                } else if (globalStringVars_.count(v)) {
                    continue; // already emitted as a string global elsewhere
                } else if (isListGlobal(v)) {
                    out << "std::vector<long long> " << v << ";\n";
                } else {
                    out << (isFloatGlobal(v) ? "double " : "long long ") << v << ";\n";
                }
            }
            out << "\n";
        }

        // Forward declarations first (C++ requires them for mutual recursion —
        // is_even calling is_odd defined later otherwise fails to compile).
        {
            // reuse the program-level list/float return sets computed above
            for (const auto &func : ir.functions) {
                if (!func.classOwner.empty()) continue;
                std::string params;
                for (size_t pi = 0; pi < func.parameters.size(); pi++) {
                    if (pi) params += ", ";
                    params += func.parameters[pi];
                }
                // per-function param hints for correct prototype types
                std::set<std::string> protoListParams = detectListParams(func, ir.symbols);
                std::set<std::string> protoStringParams = detectStringParams(func, ir.symbols);
                strategy->setListParams(protoListParams);
                strategy->setStringParams(protoStringParams);
                // #protostrvars: `isStringVar` (consulted by CStrategy's typedParamListC and
                // likely other backends' prototype-typing) checks the whole-body `stringVars_`
                // set, NOT `stringParams_` above — but stringVars_ was only ever set later, by
                // genFunction right before emitFunctionBegin, never before THIS prototype pass.
                // A param whose only string evidence is body usage (not an explicit literal)
                // stayed mis-typed as a bare list/int in the FORWARD DECLARATION while the real
                // definition (built after genFunction's own setStringVars ran) correctly saw it
                // as a string — a prototype/definition mismatch, which C's stricter type
                // matching turns into a hard compile error (verified: examples/vowel_count.ac's
                // `vowels(s)` — prototype declared `ac_int* s`, definition declared
                // `const char* s`, "conflicting types for 'vowels'"). Compute and set the same
                // way genFunction does, so both passes agree.
                {
                    std::set<std::string> protoStrVars = detectStringVars(func.instructions, ir.symbols, protoStringParams, protoStringFuncs_);
                    for (const auto& nv : detectNumericRetype(func.instructions, ir.symbols, protoStrVars)) protoStrVars.erase(nv);
                    strategy->setStringVars(protoStrVars);
                }
                // detectFloatParams (built for ASM's untyped bit-slot model — see its own
                // comment) is equally the right "infer from local usage" signal for a
                // STATICALLY-TYPED param whose type is never explicit in the AC source
                // (verified real bug on Rust: `nsqrt(x) { guess = x / 2.0; ... }` declared `x:
                // i64` unconditionally, then failed to compile calling `nsqrt(2.0)` — "expected
                // i64, found floating-point number". C/C++ never needed this because they cast
                // `(double)(x)` at every USE regardless of x's declared type; Rust has no
                // implicit numeric coercion, so it must know the real type up front.)
                strategy->setFloatParams(detectFloatParams(func, ir.symbols));
                std::set<std::string> pset(func.parameters.begin(), func.parameters.end());
                std::map<std::string, int> ftp;
                for (const auto &instr : func.instructions) {
                    if (instr.opcode != IROpcode::CALL || instr.typedOperands.empty()) continue;
                    const auto &callee = instr.typedOperands[0];
                    if (callee.kind != IRRef::Kind::VAR || callee.id < 0) continue;
                    std::string cname = ir.symbols.getName(callee.id);
                    if (pset.count(cname)) ftp[cname] = (int)instr.typedOperands.size() - 1;
                }
                strategy->setFuncTypedParams(ftp);
                int retKind = 0;
                if (protoListFuncs_.count(func.name))  retKind = 2;
                else if (protoFloatFuncs_.count(func.name)) retKind = 1;
                else if (protoStringFuncs_.count(func.name)) retKind = 3;
                // #protovoid: same prototype/definition mismatch as #protostrvars above, but for
                // the RETURN type — a genuinely void function (an event callback, most commonly)
                // has no retKind case here at all, so it always prototyped as the int default
                // while its real definition (emitFunctionBegin, which DOES check
                // returnIsVoid_/voidUserFuncs_) correctly emitted `void` — hard mismatch
                // (verified: examples/gd.ac's key-callback `hop`/`__keycb_space`, "conflicting
                // types... have 'void(...)'").
                else if (voidUserFuncs_.count(func.name)) retKind = 4;
                strategy->emitFunctionPrototype(out, func.name, params, retKind);
            }
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
                    // Real member/struct-field declarations for backends that need them (C++,
                    // Java, ...): scan every method of this class for `self.<field>` STORE_VAR/
                    // TYPE_CAST targets and emit one declaration each, in first-seen order.
                    // Backends without explicit field decls (Python, JS) leave emitFieldDecl a
                    // no-op, so this scan is harmless there.
                    {
                        std::set<std::string> seenFields;
                        for (const auto &func : ir.functions) {
                            if (func.classOwner != curClass) continue;
                            for (const auto &fi : func.instructions) {
                                if (fi.opcode != IROpcode::STORE_VAR && fi.opcode != IROpcode::TYPE_CAST) continue;
                                IRRef tgt;
                                if (fi.opcode == IROpcode::TYPE_CAST) tgt = fi.result;
                                else if (fi.typedOperands.size() >= 2) tgt = fi.typedOperands[0];
                                else if (fi.result.isValid()) tgt = fi.result;
                                if (tgt.kind != IRRef::Kind::VAR || tgt.id < 0) continue;
                                std::string nm = ir.symbols.getName(tgt.id);
                                if (nm.rfind("self.", 0) != 0) continue;
                                std::string field = nm.substr(5);
                                if (!seenFields.insert(field).second) continue;
                                IRType ft = fi.resultType != IRType::VOID ? fi.resultType : IRType::INT;
                                strategy->emitFieldDecl(out, indentLevel, field, ft);
                            }
                        }
                        strategy->emitFieldsEnd(out, indentLevel);
                    }
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

        // Second pass: emit main body (skip class blocks).
        // #6: the mainloop (globalInit) needs its own string inference — it's not a function, so
        // genFunction's setStringVars never ran for it. Without this, mainloop string vars are
        // mis-declared (e.g. `.length` vs `.length()`, non-string FOR).
        {
            std::set<std::string> mlStr = detectStringVars(ir.globalInit, ir.symbols, {}, protoStringFuncs_);
            for (const auto& nv : detectNumericRetype(ir.globalInit, ir.symbols, mlStr)) mlStr.erase(nv);
            strategy->setStringVars(mlStr);   // #retype
        }
        // Mainloop cross-block hoist (block-scoped backends: JS/C/C++/V/Java/Rust) — the mainloop
        // is not a function, so the per-function hoist pass above never covered it (#41 in main).
        {
            std::map<std::string, IRType> hoist;
            std::map<std::string, std::vector<int>> firstPath;
            std::map<std::string, IRType> firstType;
            std::vector<int> stk = {0};
            int nextId = 1;
            auto isPrefix = [](const std::vector<int>& a, const std::vector<int>& b) {
                if (a.size() > b.size()) return false;
                for (size_t i = 0; i < a.size(); i++) if (a[i] != b[i]) return false;
                return true;
            };
            std::set<std::string> funcNames;   // callee names aren't hoistable locals (see per-fn scan)
            for (const auto& fn : ir.functions) funcNames.insert(fn.name);
            auto note = [&](const std::string& v, IRType t) {
                // Same exclusion as the per-function hoist scan above — see its comment.
                if (v.empty() || isSyntheticVar(v) || funcNames.count(v)
                    || v.find('.') != std::string::npos) return;
                auto it = firstPath.find(v);
                if (it == firstPath.end()) { firstPath[v] = stk; firstType[v] = t; }
                else if (!isPrefix(it->second, stk)) {
                    IRType ht = firstType[v];
                    hoist[v] = (ht == IRType::VOID && t != IRType::VOID) ? t : ht;
                }
            };
            bool inClassScan = false;
            for (const auto& ins : ir.globalInit) {
                if (ins.opcode == IROpcode::CLASS_BEGIN) { inClassScan = true; continue; }
                if (ins.opcode == IROpcode::CLASS_END)   { inClassScan = false; continue; }
                if (inClassScan) continue;
                if (ins.result.kind == IRRef::Kind::VAR) note(ref(ins.result), ins.resultType);
                if (ins.opcode == IROpcode::STORE_VAR && !ins.typedOperands.empty()
                    && ins.typedOperands[0].kind == IRRef::Kind::VAR)
                    note(ref(ins.typedOperands[0]), ins.resultType);
                // See the identical fix + comment in the per-function hoist scan above.
                {
                    bool isCallOpcode = ins.opcode == IROpcode::CALL || ins.opcode == IROpcode::LIB_CALL;
                    for (size_t oi = 0; oi < ins.typedOperands.size(); oi++) {
                        if (isCallOpcode && oi == 0) continue;
                        const auto& op = ins.typedOperands[oi];
                        if (op.kind == IRRef::Kind::VAR) note(ref(op), IRType::VOID);
                    }
                }
                switch (ins.opcode) {
                    case IROpcode::WHILE_BEGIN: case IROpcode::FOR_BEGIN: case IROpcode::IF_BEGIN:
                        stk.push_back(nextId++); break;
                    case IROpcode::IF_ELSE:
                        if (stk.size() > 1) stk.back() = nextId++; break;
                    case IROpcode::WHILE_END: case IROpcode::FOR_END: case IROpcode::IF_END:
                        if (stk.size() > 1) stk.pop_back(); break;
                    default: break;
                }
            }
            strategy->setHoistVars(hoist);
        }
        strategy->emitMainBegin(out, indentLevel);
        strategy->setFloatVarsFull(detectFloatVars(ir.globalInit, ir.symbols, protoFloatFuncs_));
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
        return strategy->postProcess(out.str());
    }
};

// ─── public API ──────────────────────────────────────────────────────────────

std::string generateFromIR(const IRProgram &ir, const std::string &stem,
                           const std::string &outputBase)
{
    UnifiedIRCodeGen gen(ir, stem, outputBase);
    return gen.generate();
}
