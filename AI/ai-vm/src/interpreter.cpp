#include "../include/interpreter.hpp"
#include "../include/dec.hpp"
#include "dec_internal.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#ifndef _WIN32
#  include <dlfcn.h>
#  include <unistd.h>
#endif

// ── AIVal: tagged register value ──────────────────────────────────────────
// Matches the AC runtime model: int64, string, or dec (via DecRef handle).

enum class AITag : uint8_t { INT = 0, STR = 1, DEC = 2, LIST = 3 };

struct AIVal {
    AITag   tag  = AITag::INT;
    int64_t ival = 0;
    std::string sval;
    DecRef  dval = DEC_NULL;
    std::vector<AIVal> lval; // used when tag == LIST

    AIVal() = default;
    explicit AIVal(int64_t v)    : tag(AITag::INT), ival(v) {}
    explicit AIVal(std::string s): tag(AITag::STR), sval(std::move(s)) {}
    explicit AIVal(DecRef d)     : tag(AITag::DEC), dval(d) {}   // takes ownership
    explicit AIVal(std::vector<AIVal> v): tag(AITag::LIST), lval(std::move(v)) {}

    AIVal(const AIVal& o) : tag(o.tag), ival(o.ival), sval(o.sval), dval(o.dval), lval(o.lval) {
        if (tag == AITag::DEC && dval) dec_retain(dval);
    }
    AIVal& operator=(const AIVal& o) {
        if (this == &o) return *this;
        if (tag == AITag::DEC && dval) dec_release(dval);
        tag = o.tag; ival = o.ival; sval = o.sval; dval = o.dval; lval = o.lval;
        if (tag == AITag::DEC && dval) dec_retain(dval);
        return *this;
    }
    AIVal(AIVal&& o) noexcept : tag(o.tag), ival(o.ival), sval(std::move(o.sval)),
                                 dval(o.dval), lval(std::move(o.lval)) {
        o.dval = DEC_NULL; o.tag = AITag::INT;
    }
    AIVal& operator=(AIVal&& o) noexcept {
        if (this == &o) return *this;
        if (tag == AITag::DEC && dval) dec_release(dval);
        tag = o.tag; ival = o.ival; sval = std::move(o.sval);
        dval = o.dval; lval = std::move(o.lval);
        o.dval = DEC_NULL; o.tag = AITag::INT;
        return *this;
    }
    ~AIVal() { if (tag == AITag::DEC && dval) dec_release(dval); }

    bool isDec()  const { return tag == AITag::DEC;  }
    bool isStr()  const { return tag == AITag::STR;  }
    bool isInt()  const { return tag == AITag::INT;  }
    bool isList() const { return tag == AITag::LIST; }
};

// ── Helpers ────────────────────────────────────────────────────────────────

static std::string aiToStr(const AIVal& v) {
    if (v.isStr()) return v.sval;
    if (v.isInt()) return std::to_string(v.ival);
    char buf[256]; dec_to_cstr(v.dval, buf, sizeof(buf)); return buf;
}

// Promote any AIVal to a new DecRef (caller must release)
static DecRef asDecRef(const AIVal& v) {
    if (v.isDec()) return dec_retain(v.dval);
    if (v.isInt()) return dec_from_int(v.ival);
    return dec_from_literal(v.sval.c_str());
}

static AIVal aiAdd(const AIVal& a, const AIVal& b) {
    if (a.isStr() || b.isStr()) return AIVal(aiToStr(a) + aiToStr(b));
    if (a.isDec() || b.isDec()) {
        DecRef ra = asDecRef(a), rb = asDecRef(b);
        DecRef r  = dec_add(ra, rb);
        dec_release(ra); dec_release(rb);
        return AIVal(r);
    }
    return AIVal(a.ival + b.ival);
}
static AIVal aiSub(const AIVal& a, const AIVal& b) {
    if (a.isDec() || b.isDec()) {
        DecRef ra = asDecRef(a), rb = asDecRef(b);
        DecRef r  = dec_sub(ra, rb);
        dec_release(ra); dec_release(rb);
        return AIVal(r);
    }
    return AIVal(a.ival - b.ival);
}
static AIVal aiMul(const AIVal& a, const AIVal& b) {
    if (a.isDec() || b.isDec()) {
        DecRef ra = asDecRef(a), rb = asDecRef(b);
        DecRef r  = dec_mul(ra, rb);
        dec_release(ra); dec_release(rb);
        return AIVal(r);
    }
    return AIVal(a.ival * b.ival);
}
static AIVal aiAtMul(const AIVal& a, const AIVal& b) {
    // string × int repetition
    if (a.isStr() && b.isInt()) {
        std::string r; for (int64_t i = 0; i < b.ival; i++) r += a.sval; return AIVal(r);
    }
    if (b.isStr() && a.isInt()) {
        std::string r; for (int64_t i = 0; i < a.ival; i++) r += b.sval; return AIVal(r);
    }
    return aiMul(a, b);
}
static AIVal aiDiv(const AIVal& a, const AIVal& b) {
    if (a.isDec() || b.isDec()) {
        DecRef ra = asDecRef(a), rb = asDecRef(b);
        DecRef r  = dec_div(ra, rb, 20);
        dec_release(ra); dec_release(rb);
        return AIVal(r);
    }
    if (b.ival == 0) { fprintf(stderr, "Preposterous: division by zero\n"); return AIVal((int64_t)0); }
    return AIVal(a.ival / b.ival);
}
static AIVal aiMod(const AIVal& a, const AIVal& b) {
    int64_t divisor = b.isInt() ? b.ival : 1;
    if (divisor == 0) return AIVal((int64_t)0);
    return AIVal(a.ival % divisor);
}
static AIVal aiXsub(const AIVal& a, const AIVal& b) {
    int64_t ia = a.isInt() ? a.ival : 0, ib = b.isInt() ? b.ival : 0;
    int64_t d  = ia - ib; if (d < 0) d = -d;
    return AIVal(d + 1);
}

static void displayVal(const AIVal& v) {
    if (v.isInt()) printf("%lld\n", (long long)v.ival);
    else if (v.isStr()) printf("%s\n", v.sval.c_str());
    else if (v.isList()) {
        printf("[");
        for (size_t i = 0; i < v.lval.size(); i++) {
            if (i) printf(", ");
            const AIVal& e = v.lval[i];
            if (e.isInt()) printf("%lld", (long long)e.ival);
            else if (e.isStr()) printf("%s", e.sval.c_str());
            else if (e.isList()) printf("[...]");
            else { char buf[256]; dec_to_cstr(e.dval, buf, sizeof(buf)); printf("%s", buf); }
        }
        printf("]\n");
    }
    else dec_display(v.dval);
}

static int64_t asTruth(const AIVal& v) {
    if (v.isInt())  return v.ival != 0 ? 1 : 0;
    if (v.isStr())  return v.sval.empty() ? 0 : 1;
    if (v.isList()) return v.lval.empty() ? 0 : 1;
    return dec_is_zero(v.dval) ? 0 : 1;
}

static int cmpVals(const AIVal& a, const AIVal& b) {
    if (a.isDec() || b.isDec()) {
        DecRef ra = asDecRef(a), rb = asDecRef(b);
        int c = dec_cmp(ra, rb);
        dec_release(ra); dec_release(rb);
        return c;
    }
    return (a.ival < b.ival) ? -1 : (a.ival > b.ival) ? 1 : 0;
}

// ── Native library dispatch ────────────────────────────────────────────────

static std::unordered_map<std::string, void*> g_soCache;

static void* getNativeSo(const std::string& libName) {
#ifndef _WIN32
    auto it = g_soCache.find(libName);
    if (it != g_soCache.end()) return it->second;

    std::string soName = "libac" + libName + ".so";
    std::vector<std::string> candidates;
    char buf[4096] = {};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        std::string bd(buf, (size_t)n);
        auto sl = bd.rfind('/'); if (sl != std::string::npos) bd = bd.substr(0, sl);
        candidates.push_back(bd + "/../library/" + libName + "/" + soName);
        candidates.push_back(bd + "/../../AC/library/" + libName + "/" + soName);
        candidates.push_back(bd + "/" + soName);
    }
    candidates.push_back(soName);

    void* h = nullptr;
    for (auto& p : candidates) { h = dlopen(p.c_str(), RTLD_LAZY | RTLD_GLOBAL); if (h) break; }
    g_soCache[libName] = h;
    return h;
#else
    (void)libName; return nullptr;
#endif
}

static AIVal callNative(const AiBcModule& mod, uint16_t pidx, const std::vector<AIVal>& args) {
    if (pidx >= (uint16_t)mod.pool.size() || mod.pool[pidx].type != AIBC_CONST_STR)
        return AIVal((int64_t)0);
    const std::string& name = mod.pool[pidx].sval;
    auto dot = name.find('.');
    if (dot == std::string::npos) return AIVal((int64_t)0);
    std::string lib = name.substr(0, dot), fn = name.substr(dot + 1);

    auto toD = [&](size_t i) -> double {
        if (i >= args.size()) return 0.0;
        const AIVal& v = args[i];
        if (v.isInt()) return (double)v.ival;
        if (v.isDec()) { char b[128]; dec_to_cstr(v.dval,b,sizeof(b)); return atof(b); }
        return atof(v.sval.c_str());
    };
    auto toI = [&](size_t i) -> int64_t {
        if (i >= args.size()) return 0;
        return args[i].isInt() ? args[i].ival : (int64_t)toD(i);
    };
    auto toS = [&](size_t i) -> std::string {
        return i < args.size() ? aiToStr(args[i]) : "";
    };
    auto fromD = [](double v) -> AIVal {
        char b[64]; snprintf(b, sizeof(b), "%.17g", v);
        return AIVal(dec_from_literal(b));
    };

#ifndef _WIN32
    void* so = getNativeSo(lib);
    if (!so) {
        fprintf(stderr, "Preposterous: NativeLibError: could not load libac%s.so\n", lib.c_str());
        return AIVal((int64_t)0);
    }
    if (lib == "math") {
        if (fn == "pi" || fn == "e" || fn == "phi" || fn == "tau" || fn == "inf") {
            std::string sym = (fn == "inf") ? "ac_math_inf" : "ac_math_" + fn + "_const";
            auto f = (double(*)())dlsym(so, sym.c_str());
            return f ? fromD(f()) : AIVal((int64_t)0);
        }
        static const char* D1[] = {
            "sin","cos","tan","csc","sec","cot","asin","acos","atan",
            "sqrt","cbrt","abs","floor","ceil","round","ln","log2","log10",
            "deg2rad","rad2deg", nullptr
        };
        for (int i = 0; D1[i]; i++) if (fn == D1[i]) {
            auto f = (double(*)(double))dlsym(so, ("ac_" + fn).c_str());
            return f ? fromD(f(toD(0))) : AIVal((int64_t)0);
        }
        if (fn=="atan2"||fn=="hypot"||fn=="pow"||fn=="mod") {
            auto f = (double(*)(double,double))dlsym(so, ("ac_" + fn).c_str());
            return f ? fromD(f(toD(0), toD(1))) : AIVal((int64_t)0);
        }
        if (fn == "log") {
            auto f = (double(*)(double,double))dlsym(so, "ac_log_base");
            return f ? fromD(f(toD(0), toD(1))) : AIVal((int64_t)0);
        }
        if (fn == "clamp") {
            auto f = (double(*)(double,double,double))dlsym(so, "ac_clamp");
            return f ? fromD(f(toD(0), toD(1), toD(2))) : AIVal((int64_t)0);
        }
        if (fn == "gcd" || fn == "lcm") {
            auto f = (int64_t(*)(int64_t,int64_t))dlsym(so, ("ac_" + fn).c_str());
            return f ? AIVal(f(toI(0), toI(1))) : AIVal((int64_t)0);
        }
        if (fn == "is_prime") {
            auto f = (int(*)(int64_t))dlsym(so, "ac_is_prime");
            return f ? AIVal((int64_t)f(toI(0))) : AIVal((int64_t)0);
        }
        if (fn == "eval") {
            auto f = (double(*)(const char*))dlsym(so, "ac_eval");
            std::string s = toS(0);
            return f ? fromD(f(s.c_str())) : AIVal((int64_t)0);
        }
    }
    if (lib == "camera") {
        if (fn=="init")    { auto f=(int(*)())dlsym(so,"ac_camera_init"); return f?AIVal((int64_t)f()):AIVal((int64_t)0); }
        if (fn=="release") { auto f=(void(*)())dlsym(so,"ac_camera_release"); if(f)f(); return AIVal((int64_t)0); }
        if (fn=="capture"||fn=="capture_latest"||fn=="capture_first") {
            std::string s=toS(0), sym="ac_camera_"+fn;
            auto f=(int(*)(const char*))dlsym(so,sym.c_str());
            return f?AIVal((int64_t)f(s.c_str())):AIVal((int64_t)0);
        }
        if (fn=="sidebar_config")  { std::string k=toS(0),v2=toS(1); auto f=(void(*)(const char*,const char*))dlsym(so,"ac_sidebar_config"); if(f)f(k.c_str(),v2.c_str()); return AIVal((int64_t)0); }
        if (fn=="sidebar_setregion"||fn=="sidebar_display"||fn=="screen_setmode") {
            std::string s=toS(0); auto f=(void(*)(const char*))dlsym(so,("ac_"+fn).c_str()); if(f)f(s.c_str()); return AIVal((int64_t)0);
        }
        if (fn=="sidebar_setinteractive") { auto f=(void(*)(int))dlsym(so,"ac_sidebar_setinteractive"); if(f)f((int)toI(0)); return AIVal((int64_t)0); }
        if (fn=="sidebar_ask") { std::string s=toS(0); auto f=(const char*(*)(const char*))dlsym(so,"ac_sidebar_ask"); const char* r=f?f(s.c_str()):nullptr; return AIVal(std::string(r?r:"")); }
        if (fn=="sidebar_getinput") { auto f=(const char*(*)())dlsym(so,"ac_sidebar_getinput"); const char* r=f?f():nullptr; return AIVal(std::string(r?r:"")); }
        if (fn=="screen_update") { auto f=(void(*)())dlsym(so,"ac_screen_update"); if(f)f(); return AIVal((int64_t)0); }
    }
#else
    (void)toD; (void)toI; (void)toS; (void)fromD; (void)lib; (void)fn;
#endif
    return AIVal((int64_t)0);
}

// ── Interpreter ────────────────────────────────────────────────────────────

void aivm_run(const AiBcModule& mod) {
    if (mod.funcs.empty()) return;

    std::function<AIVal(uint32_t, std::vector<AIVal>)> callFunc;
    callFunc = [&](uint32_t fnIdx, std::vector<AIVal> args) -> AIVal {
        if (fnIdx >= mod.funcs.size()) return AIVal((int64_t)0);
        const AiBcFunc& fn = mod.funcs[fnIdx];
        std::vector<AIVal> regs(fn.local_count, AIVal((int64_t)0));
        for (size_t i = 0; i < args.size() && i < regs.size(); i++)
            regs[i] = args[i];

        auto loadConst = [&](uint16_t idx) -> AIVal {
            if (idx >= mod.pool.size()) return AIVal((int64_t)0);
            const AiBcConst& c = mod.pool[idx];
            if (c.type == AIBC_CONST_INT) return AIVal(c.ival);
            if (c.type == AIBC_CONST_STR) return AIVal(std::string(c.sval));
            if (c.type == AIBC_CONST_DEC) return AIVal(dec_from_literal(c.sval.c_str()));
            return AIVal((int64_t)0);
        };

        auto R = [&](uint8_t r) -> AIVal& { return regs[r % regs.size()]; };

        uint32_t pc = fn.bc_offset;
        const uint32_t end = fn.bc_offset + fn.bc_len;

        while (pc < end) {
            AiBcInstr ins = decodeInstr(mod.bytecode[pc++]);

            switch (ins.op) {
            case AIOp::NOP:  break;
            case AIOp::LOAD_CONST: R(ins.A) = loadConst((uint16_t)(ins.B|(ins.C<<8))); break;
            case AIOp::LOAD_VAR:   R(ins.A) = regs[ins.B]; break;
            case AIOp::STORE_VAR:  regs[ins.A] = R(ins.B); break;

            case AIOp::ADD:    R(ins.A) = aiAdd(R(ins.B),  R(ins.C)); break;
            case AIOp::SUB:    R(ins.A) = aiSub(R(ins.B),  R(ins.C)); break;
            case AIOp::MUL:    R(ins.A) = aiMul(R(ins.B),  R(ins.C)); break;
            case AIOp::AT_MUL: R(ins.A) = aiAtMul(R(ins.B),R(ins.C)); break;
            case AIOp::DIV:    R(ins.A) = aiDiv(R(ins.B),  R(ins.C)); break;
            case AIOp::MOD:    R(ins.A) = aiMod(R(ins.B),  R(ins.C)); break;
            case AIOp::XSUB:   R(ins.A) = aiXsub(R(ins.B), R(ins.C)); break;
            case AIOp::XOR: {
                int64_t ia = R(ins.B).isInt() ? R(ins.B).ival : 0;
                int64_t ib = R(ins.C).isInt() ? R(ins.C).ival : 0;
                R(ins.A) = AIVal(ia ^ ib); break;
            }
            case AIOp::NEG: R(ins.A) = aiSub(AIVal((int64_t)0), R(ins.B)); break;
            case AIOp::NOT: R(ins.A) = AIVal((int64_t)(asTruth(R(ins.B)) == 0 ? 1 : 0)); break;

            case AIOp::CMP_EQ: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))==0?1:0)); break;
            case AIOp::CMP_NE: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))!=0?1:0)); break;
            case AIOp::CMP_LT: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))< 0?1:0)); break;
            case AIOp::CMP_GT: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))> 0?1:0)); break;
            case AIOp::CMP_LE: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))<=0?1:0)); break;
            case AIOp::CMP_GE: R(ins.A) = AIVal((int64_t)(cmpVals(R(ins.B),R(ins.C))>=0?1:0)); break;
            case AIOp::AND: R(ins.A) = AIVal((int64_t)(asTruth(R(ins.B))&&asTruth(R(ins.C))?1:0)); break;
            case AIOp::OR:  R(ins.A) = AIVal((int64_t)(asTruth(R(ins.B))||asTruth(R(ins.C))?1:0)); break;

            case AIOp::JMP: {
                int32_t off = (int32_t)((ins.A)|((int32_t)ins.B<<8)|((int32_t)ins.C<<16));
                if (off & (1<<23)) off |= 0xFF000000;
                pc = (uint32_t)((int32_t)pc + off - 1); break;
            }
            case AIOp::JZ: {
                if (asTruth(R(ins.A)) == 0) {
                    int32_t off = (int16_t)(ins.B|(ins.C<<8));
                    pc = (uint32_t)((int32_t)pc + off - 1);
                } break;
            }
            case AIOp::JNZ: {
                if (asTruth(R(ins.A)) != 0) {
                    int32_t off = (int16_t)(ins.B|(ins.C<<8));
                    pc = (uint32_t)((int32_t)pc + off - 1);
                } break;
            }

            case AIOp::CALL: {
                std::vector<AIVal> ca;
                for (int i = 0; i < ins.B; i++) ca.push_back(regs[i]);
                R(0) = callFunc(ins.A, ca); break;
            }
            case AIOp::RETURN: return R(ins.A);
            case AIOp::HALT:   exit(0);

            case AIOp::DISPLAY:     displayVal(R(ins.A)); break;
            case AIOp::DEC_DISPLAY: displayVal(R(ins.A)); break;  // same
            case AIOp::INPUT: {
                std::string line; std::getline(std::cin, line);
                R(ins.A) = AIVal(line); break;
            }

            case AIOp::CALL_NATIVE: {
                uint16_t pidx = (uint16_t)(ins.B|(ins.C<<8));
                std::vector<AIVal> ca(ins.A);
                for (uint8_t i = 0; i < ins.A; i++) ca[i] = regs[i % regs.size()];
                regs[0] = callNative(mod, pidx, ca); break;
            }

            case AIOp::DEC_NEW: {
                uint16_t idx = (uint16_t)(ins.B|(ins.C<<8));
                const std::string& s = idx < mod.pool.size() ? mod.pool[idx].sval : "0";
                R(ins.A) = AIVal(dec_from_literal(s.c_str())); break;
            }
            case AIOp::DEC_ADD: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal(dec_add(ra,rb)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_SUB: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal(dec_sub(ra,rb)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_MUL: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal(dec_mul(ra,rb)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_DIV: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal(dec_div(ra,rb,20)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_CMP_EQ: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal((int64_t)(dec_cmp(ra,rb)==0?1:0)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_CMP_LT: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal((int64_t)(dec_cmp(ra,rb)< 0?1:0)); dec_release(ra);dec_release(rb); break; }
            case AIOp::DEC_CMP_GT: { DecRef ra=asDecRef(R(ins.B)),rb=asDecRef(R(ins.C)); R(ins.A)=AIVal((int64_t)(dec_cmp(ra,rb)> 0?1:0)); dec_release(ra);dec_release(rb); break; }

            case AIOp::CAST_DEC: {
                const AIVal& s = R(ins.B);
                if (s.isDec()) R(ins.A) = s;
                else R(ins.A) = AIVal(asDecRef(s));  // takes ownership
                break;
            }
            case AIOp::INT_TO_DEC: R(ins.A) = AIVal(dec_from_int(R(ins.B).ival)); break;
            case AIOp::DEC_TO_INT: case AIOp::CAST_INT: {
                const AIVal& s = R(ins.B);
                if (s.isInt()) R(ins.A) = s;
                else if (s.isDec()) { char b[128]; dec_to_cstr(s.dval,b,sizeof(b)); R(ins.A)=AIVal((int64_t)atoll(b)); }
                else { try { R(ins.A)=AIVal((int64_t)std::stoll(s.sval)); } catch(...){ R(ins.A)=AIVal((int64_t)0); } }
                break;
            }
            case AIOp::CAST_STR:  R(ins.A) = AIVal(aiToStr(R(ins.B))); break;
            case AIOp::CAST_BOOL: R(ins.A) = AIVal((int64_t)asTruth(R(ins.B))); break;

            case AIOp::MAKE_LIST: {
                std::vector<AIVal> items;
                uint8_t start = ins.B, count = ins.C;
                items.reserve(count);
                for (uint8_t i = 0; i < count; i++)
                    items.push_back(regs[(start + i) % regs.size()]);
                R(ins.A) = AIVal(std::move(items));
                break;
            }
            case AIOp::LIST_GET: {
                const AIVal& lst = R(ins.B);
                int64_t idx = R(ins.C).isInt() ? R(ins.C).ival : 0;
                if (lst.isList() && idx >= 0 && (size_t)idx < lst.lval.size())
                    R(ins.A) = lst.lval[(size_t)idx];
                else
                    R(ins.A) = AIVal((int64_t)0);
                break;
            }
            case AIOp::LIST_SET: {
                AIVal& lst = regs[ins.A % regs.size()];
                int64_t idx = R(ins.B).isInt() ? R(ins.B).ival : 0;
                if (lst.isList() && idx >= 0 && (size_t)idx < lst.lval.size())
                    lst.lval[(size_t)idx] = R(ins.C);
                break;
            }
            case AIOp::LIST_SIZE: {
                const AIVal& lst = R(ins.B);
                R(ins.A) = AIVal(lst.isList() ? (int64_t)lst.lval.size() : (int64_t)0);
                break;
            }

            default: break;
            }
        }
        return AIVal((int64_t)0);
    };

    callFunc(0, {});
}
