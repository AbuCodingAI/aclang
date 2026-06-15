#pragma once
#include <map>
#include <string>
#include <utility>
#include <variant>

namespace aca {

// Exact decimal literal representation: base-10 exponent -> digit coefficient.
// Example "123.45" => {2:1,1:2,0:3,-1:4,-2:5}
using DecMap = std::map<int, int>;

enum class TypeKind { Numeral, String, Boolean, Unknown };
enum class NumeralSubtype { NumInt, NumDec };
enum class BooleanSubtype { True, False };

struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::variant<std::monostate, NumeralSubtype, BooleanSubtype> sub;

    static Type Numeral(NumeralSubtype s) {
        Type t;
        t.kind = TypeKind::Numeral;
        t.sub = s;
        return t;
    }
    static Type String() {
        Type t;
        t.kind = TypeKind::String;
        return t;
    }
    static Type Boolean(BooleanSubtype s) {
        Type t;
        t.kind = TypeKind::Boolean;
        t.sub = s;
        return t;
    }
    static Type Unknown() { return Type{}; }
};

inline bool isNumeral(const Type& t) { return t.kind == TypeKind::Numeral; }
inline bool isBoolean(const Type& t) { return t.kind == TypeKind::Boolean; }
inline bool isUnknown(const Type& t) { return t.kind == TypeKind::Unknown; }

inline std::string typeToString(const Type& t) {
    switch (t.kind) {
        case TypeKind::Numeral:
            return std::get<NumeralSubtype>(t.sub) == NumeralSubtype::NumInt ? "Numeral.NumInt" : "Numeral.NumDec";
        case TypeKind::String:  return "String";
        case TypeKind::Boolean:
            return std::get<BooleanSubtype>(t.sub) == BooleanSubtype::True ? "Boolean.true" : "Boolean.false";
        case TypeKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

inline std::string decMapToDebug(const DecMap& m) {
    std::string out = "{";
    bool first = true;
    for (auto& [exp, coeff] : m) {
        if (!first) out += ", ";
        first = false;
        out += std::to_string(exp) + ":" + std::to_string(coeff);
    }
    out += "}";
    return out;
}

struct LitValue {
    std::variant<int64_t, DecMap, std::string, bool> v;
    Type t = Type::Unknown();
};

DecMap decimalLiteralToMap(const std::string& lit);

} // namespace aca

