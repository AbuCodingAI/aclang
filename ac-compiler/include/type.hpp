#pragma once
#include <string>
#include <memory>

enum class TypeKind {
    Boolean,
    Numeral,
    String,
    List,
    Tuple,
    Range,
    Sequence,
    Unknown
};

enum class BooleanSubtype { True, False };

enum class NumeralSubtype {
    PosInt,   // positive integer  e.g. 5
    PosDec,   // positive decimal  e.g. 3.14
    NegInt,   // negative integer  e.g. -5
    NegDec,   // negative decimal  e.g. -3.14
};

struct Type {
    TypeKind      kind       = TypeKind::Unknown;
    BooleanSubtype boolSub   = BooleanSubtype::True;
    NumeralSubtype numSub    = NumeralSubtype::PosInt;
    bool immutable           = false;

    Type() = default;
    explicit Type(TypeKind k) : kind(k) {}
    Type(TypeKind k, BooleanSubtype bs) : kind(k), boolSub(bs) {}
    Type(TypeKind k, NumeralSubtype ns) : kind(k), numSub(ns) {}

    // ── factories ────────────────────────────────────────────────────────────
    static Type makeBoolean(BooleanSubtype s = BooleanSubtype::True) { return Type(TypeKind::Boolean, s); }

    static Type makeNumeral(NumeralSubtype s = NumeralSubtype::PosInt) { return Type(TypeKind::Numeral, s); }

    // Infer numeral subtype from a string literal value
    static Type inferNumeral(const std::string& val) {
        bool neg = !val.empty() && val[0] == '-';
        bool dec = val.find('.') != std::string::npos;
        if (!neg && !dec) return makeNumeral(NumeralSubtype::PosInt);
        if (!neg &&  dec) return makeNumeral(NumeralSubtype::PosDec);
        if ( neg && !dec) return makeNumeral(NumeralSubtype::NegInt);
        return makeNumeral(NumeralSubtype::NegDec);
    }

    static Type makeString()   { return Type(TypeKind::String); }
    static Type makeList()     { return Type(TypeKind::List); }
    static Type makeTuple()    { Type t(TypeKind::Tuple); t.immutable = true; return t; }
    static Type makeRange()    { return Type(TypeKind::Range); }
    static Type makeSequence() { return Type(TypeKind::Sequence); }
    static Type makeUnknown()  { return Type(TypeKind::Unknown); }

    // ── predicates ───────────────────────────────────────────────────────────
    bool isNumeral()  const { return kind == TypeKind::Numeral; }
    bool isString()   const { return kind == TypeKind::String; }
    bool isBoolean()  const { return kind == TypeKind::Boolean; }
    bool isList()     const { return kind == TypeKind::List; }
    bool isTuple()    const { return kind == TypeKind::Tuple; }
    bool isRange()    const { return kind == TypeKind::Range; }
    bool isSequence() const { return kind == TypeKind::Sequence; }
    bool isUnknown()  const { return kind == TypeKind::Unknown; }

    bool isPosInt()   const { return isNumeral() && numSub == NumeralSubtype::PosInt; }
    bool isPosDec()   const { return isNumeral() && numSub == NumeralSubtype::PosDec; }
    bool isNegInt()   const { return isNumeral() && numSub == NumeralSubtype::NegInt; }
    bool isNegDec()   const { return isNumeral() && numSub == NumeralSubtype::NegDec; }
    bool isPos()      const { return isPosInt() || isPosDec(); }
    bool isNeg()      const { return isNegInt() || isNegDec(); }
    bool isInt()      const { return isPosInt() || isNegInt(); }
    bool isDec()      const { return isPosDec() || isNegDec(); }

    // ── native type name per backend ─────────────────────────────────────────
    std::string toCpp()  const {
        if (!isNumeral()) return "auto";
        switch (numSub) {
            case NumeralSubtype::PosInt: return "int";
            case NumeralSubtype::PosDec: return "double";
            case NumeralSubtype::NegInt: return "int";
            case NumeralSubtype::NegDec: return "double";
        }
        return "int";
    }
    std::string toC()    const { return toCpp(); } // same as C++
    std::string toPy()   const { return isDec() ? "float" : "int"; }
    std::string toJs()   const { return "number"; } // JS has one number type
    std::string toJava() const { return isDec() ? "double" : "int"; }
    std::string toGo()   const { return isDec() ? "float64" : "int"; }
    std::string toRs()   const {
        switch (numSub) {
            case NumeralSubtype::PosInt: return "u64";
            case NumeralSubtype::PosDec: return "f64";
            case NumeralSubtype::NegInt: return "i64";
            case NumeralSubtype::NegDec: return "f64";
        }
        return "i64";
    }

    std::string toString() const {
        switch (kind) {
            case TypeKind::Boolean:
                return boolSub == BooleanSubtype::True ? "Boolean(True)" : "Boolean(False)";
            case TypeKind::Numeral:
                switch (numSub) {
                    case NumeralSubtype::PosInt: return "Numeral(Pos.Int)";
                    case NumeralSubtype::PosDec: return "Numeral(Pos.Dec)";
                    case NumeralSubtype::NegInt: return "Numeral(Neg.Int)";
                    case NumeralSubtype::NegDec: return "Numeral(Neg.Dec)";
                }
            case TypeKind::String:   return "String";
            case TypeKind::List:     return "List";
            case TypeKind::Tuple:    return "Tuple(immutable)";
            case TypeKind::Range:    return "Range(Numeral Pos.Int)";
            case TypeKind::Sequence: return "Sequence(Numeral Pos.Int)";
            case TypeKind::Unknown:  return "Unknown";
        }
        return "Unknown";
    }
};

using TypePtr = std::shared_ptr<Type>;
