#pragma once
#include <string>
#include <variant>
#include <optional>
#include <memory>
#include <set>

// ── Assumed result type of a built-in / ilib AC call ─────────────────────────────
// The IR carries no per-function signatures for ilib calls, so AC infers their return type BY NAME.
// This is the ONE authority every stage must consult — the IR float pre-scan, the text backends,
// AND the native BNY backend — so they can never disagree. (They used to: BNY's float pre-scan only
// recognized *user* float functions, so `total += math.mod(m,10)` never promoted `total` to float
// and a double's raw bits were summed into an int accumulator → garbage. Centralizing it here fixes
// that whole class of "type-mangled" bugs instead of patching each backend's private guesswork.)
inline bool acCallReturnsFloat(const std::string& irName) {
    if (irName == "ml.take" || irName == "ml_take") return true;
    // aczip's compression-ratio percentage — same "known float-returning ilib call"
    // pattern as ml.take above.
    if (irName == "aczip.get_ratio" || irName == "aczip_get_ratio") return true;
    bool isMath = irName.rfind("math.", 0) == 0 || irName.rfind("math_", 0) == 0;
    if (!isMath) return false;
    // math.* return a double EXCEPT this small set of genuinely integer-valued ones.
    static const std::set<std::string> intValued = {
        "math.to_int", "math.abs_int", "math.mod_int", "math.gcd", "math.lcm", "math.is_prime",
        "math_to_int", "math_abs_int", "math_mod_int", "math_gcd", "math_lcm", "math_is_prime",
    };
    return intValued.find(irName) == intValued.end();
}

// ─────────────────────────────────────────────────────────────
// Core AC type system (semantic level)
// ─────────────────────────────────────────────────────────────

enum class TypeKind {
    Boolean,
    Numeral,
    String,
    List,
    Tuple,
    Unknown
};

// AC does NOT treat keywords like "range" or "sequence" as types.
// They are language constructs, not type-level entities.

enum class NumeralSubtype {
    PosInt,
    PosDec,
    NegInt,
    NegDec,
    Short,   // 32-bit signed integer  (`short x = e`)
    Mini     // 16-bit signed integer  (`mini x = e`)
};

// ── Fixed-width integer backend type names ─────────────────────────────────
// `short` = 32-bit, `mini` = 16-bit. This is the ONE place the width→type-name
// mapping lives, so every backend reads the same fixed-width type from the type
// include instead of scattering literals (or truncating values) through codegen.
// bits: 32 → short, 16 → mini; any other value → the backend's default 64-bit int.
inline const char* acIntTypeCpp (int bits) { return bits==32 ? "int32_t" : bits==16 ? "int16_t" : "long long"; }
inline const char* acIntTypeC   (int bits) { return acIntTypeCpp(bits); }
inline const char* acIntTypeRs  (int bits) { return bits==32 ? "i32"     : bits==16 ? "i16"     : "i64"; }
inline const char* acIntTypeGo  (int bits) { return bits==32 ? "int32"   : bits==16 ? "int16"   : "int64"; }
inline const char* acIntTypeJava(int bits) { return bits==32 ? "int"     : bits==16 ? "short"   : "long"; }
inline const char* acIntTypeV   (int bits) { return bits==32 ? "int"     : bits==16 ? "i16"     : "i64"; }

struct Type {
    TypeKind kind = TypeKind::Unknown;

    // Only meaningful when kind == Numeral
    NumeralSubtype numSub = NumeralSubtype::PosInt;

    bool immutable = false;

    // ─────────────────────────────────────────────────────────────
    // Constructors
    // ─────────────────────────────────────────────────────────────

    Type() = default;

    explicit Type(TypeKind k) : kind(k) {}

    Type(TypeKind k, NumeralSubtype n)
        : kind(k), numSub(n) {}

    // ─────────────────────────────────────────────────────────────
    // Factories
    // ─────────────────────────────────────────────────────────────

    static Type Boolean() { return Type(TypeKind::Boolean); }

    static Type Numeral(NumeralSubtype s = NumeralSubtype::PosInt) {
        return Type(TypeKind::Numeral, s);
    }

    static Type String() { return Type(TypeKind::String); }

    static Type List()     { return Type(TypeKind::List); }
    static Type makeList() { return List(); }   // alias used by parser

    static Type Tuple() {
        Type t(TypeKind::Tuple);
        t.immutable = true;
        return t;
    }

    static Type Unknown() { return Type(TypeKind::Unknown); }

    // ─────────────────────────────────────────────────────────────
    // Literal inference (frontend responsibility)
    // ─────────────────────────────────────────────────────────────

    static Type inferNumeral(const std::string& val) {
        bool neg = !val.empty() && val[0] == '-';
        bool dec = val.find('.') != std::string::npos;

        if (!neg && !dec) return Numeral(NumeralSubtype::PosInt);
        if (!neg &&  dec) return Numeral(NumeralSubtype::PosDec);
        if ( neg && !dec) return Numeral(NumeralSubtype::NegInt);
        return Numeral(NumeralSubtype::NegDec);
    }

    // ─────────────────────────────────────────────────────────────
    // Predicates
    // ─────────────────────────────────────────────────────────────

    bool isBoolean() const { return kind == TypeKind::Boolean; }
    bool isNumeral() const { return kind == TypeKind::Numeral; }
    bool isString()  const { return kind == TypeKind::String; }
    bool isList()    const { return kind == TypeKind::List; }
    bool isTuple()   const { return kind == TypeKind::Tuple; }
    bool isUnknown() const { return kind == TypeKind::Unknown; }

    bool isInt() const {
        return isNumeral() &&
              (numSub == NumeralSubtype::PosInt ||
               numSub == NumeralSubtype::NegInt ||
               numSub == NumeralSubtype::Short  ||
               numSub == NumeralSubtype::Mini);
    }

    // Fixed-width bit count for `short`/`mini` (0 = default 64-bit int / not applicable).
    int intWidth() const {
        if (numSub == NumeralSubtype::Short) return 32;
        if (numSub == NumeralSubtype::Mini)  return 16;
        return 0;
    }

    bool isDec() const {
        return isNumeral() &&
              (numSub == NumeralSubtype::PosDec ||
               numSub == NumeralSubtype::NegDec);
    }

    // ─────────────────────────────────────────────────────────────
    // Backend type mapping (kept simple + deterministic)
    // NOTE: no semantics here, only representation hints
    // ─────────────────────────────────────────────────────────────

    std::string toCpp() const {
        if (!isNumeral()) return "auto";
        if (intWidth()) return acIntTypeCpp(intWidth());
        return isDec() ? "double" : "int";
    }

    std::string toC() const { return toCpp(); }

    std::string toPy() const {
        return isDec() ? "float" : "int";   // Python has no fixed-width int
    }

    std::string toJs() const {
        return "number";
    }

    std::string toJava() const {
        if (intWidth()) return acIntTypeJava(intWidth());
        return isDec() ? "double" : "int";
    }

    std::string toGo() const {
        if (intWidth()) return acIntTypeGo(intWidth());
        return isDec() ? "float64" : "int";
    }

    std::string toRs() const {
        if (!isNumeral()) return "i64";
        if (intWidth()) return acIntTypeRs(intWidth());
        return isDec() ? "f64" : "i64";
    }

    std::string toV() const {
        if (intWidth()) return acIntTypeV(intWidth());
        return isDec() ? "f64" : "i64";
    }

    // ─────────────────────────────────────────────────────────────
    // Debug string
    // ─────────────────────────────────────────────────────────────

    std::string toString() const {
        switch (kind) {
            case TypeKind::Boolean: return "Boolean";
            case TypeKind::Numeral:
                switch (numSub) {
                    case NumeralSubtype::PosInt: return "Numeral(PosInt)";
                    case NumeralSubtype::PosDec: return "Numeral(PosDec)";
                    case NumeralSubtype::NegInt: return "Numeral(NegInt)";
                    case NumeralSubtype::NegDec: return "Numeral(NegDec)";
                    case NumeralSubtype::Short:  return "Numeral(Short/32)";
                    case NumeralSubtype::Mini:   return "Numeral(Mini/16)";
                }
                return "Numeral";   // guard: don't fall through into the String case
            case TypeKind::String:  return "String";
            case TypeKind::List:    return "List";
            case TypeKind::Tuple:   return "Tuple";
            case TypeKind::Unknown: return "Unknown";
        }
        return "Unknown";
    }
};

using TypePtr = std::shared_ptr<Type>;