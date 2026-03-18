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
enum class NumeralSubtype { Positive, Negative };

struct Type {
    TypeKind kind;
    BooleanSubtype boolSubtype = BooleanSubtype::True;
    NumeralSubtype numSubtype  = NumeralSubtype::Positive;
    bool immutable = false; // tuples are immutable

    Type() : kind(TypeKind::Unknown) {}
    explicit Type(TypeKind k) : kind(k) {}
    Type(TypeKind k, BooleanSubtype bs) : kind(k), boolSubtype(bs) {}
    Type(TypeKind k, NumeralSubtype ns) : kind(k), numSubtype(ns) {}

    static Type makeBoolean(BooleanSubtype s = BooleanSubtype::True) { return Type(TypeKind::Boolean, s); }
    static Type makeNumeral(NumeralSubtype s = NumeralSubtype::Positive) { return Type(TypeKind::Numeral, s); }
    static Type makeString()   { return Type(TypeKind::String); }
    static Type makeList()     { return Type(TypeKind::List); }
    static Type makeTuple()    { Type t(TypeKind::Tuple); t.immutable = true; return t; }
    static Type makeRange()    { return Type(TypeKind::Range); }    // range N  → Numeral Pos
    static Type makeSequence() { return Type(TypeKind::Sequence); } // sequence(x,y)
    static Type makeUnknown()  { return Type(TypeKind::Unknown); }

    bool isNumeral()  const { return kind == TypeKind::Numeral; }
    bool isString()   const { return kind == TypeKind::String; }
    bool isBoolean()  const { return kind == TypeKind::Boolean; }
    bool isList()     const { return kind == TypeKind::List; }
    bool isTuple()    const { return kind == TypeKind::Tuple; }
    bool isRange()    const { return kind == TypeKind::Range; }
    bool isSequence() const { return kind == TypeKind::Sequence; }
    bool isUnknown()  const { return kind == TypeKind::Unknown; }
    bool isPos()      const { return kind == TypeKind::Numeral && numSubtype == NumeralSubtype::Positive; }

    std::string toString() const {
        switch (kind) {
            case TypeKind::Boolean:  return boolSubtype == BooleanSubtype::True ? "Boolean(True)" : "Boolean(False)";
            case TypeKind::Numeral:  return numSubtype  == NumeralSubtype::Positive ? "Numeral(Pos)" : "Numeral(Neg)";
            case TypeKind::String:   return "String";
            case TypeKind::List:     return "List";
            case TypeKind::Tuple:    return "Tuple(immutable)";
            case TypeKind::Range:    return "Range(Numeral Pos)";
            case TypeKind::Sequence: return "Sequence(Numeral Pos)";
            case TypeKind::Unknown:  return "Unknown";
        }
        return "Unknown";
    }
};

using TypePtr = std::shared_ptr<Type>;
