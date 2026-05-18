#pragma once
#include <string>
#include <variant>
#include <optional>
#include <memory>

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
    NegDec
};

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
               numSub == NumeralSubtype::NegInt);
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
        return isDec() ? "double" : "int";
    }

    std::string toC() const { return toCpp(); }

    std::string toPy() const {
        return isDec() ? "float" : "int";
    }

    std::string toJs() const {
        return "number";
    }

    std::string toJava() const {
        return isDec() ? "double" : "int";
    }

    std::string toGo() const {
        return isDec() ? "float64" : "int";
    }

    std::string toRs() const {
        if (!isNumeral()) return "i64";
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
                }
            case TypeKind::String:  return "String";
            case TypeKind::List:    return "List";
            case TypeKind::Tuple:   return "Tuple";
            case TypeKind::Unknown: return "Unknown";
        }
        return "Unknown";
    }
};

using TypePtr = std::shared_ptr<Type>;