#pragma once
#include <string>
#include <memory>

enum class TypeKind {
    Boolean,
    Numeral,
    String,
    Unknown
};

enum class BooleanSubtype {
    True,
    False
};

enum class NumeralSubtype {
    Positive,
    Negative
};

struct Type {
    TypeKind kind;
    BooleanSubtype boolSubtype;
    NumeralSubtype numSubtype;
    
    Type() : kind(TypeKind::Unknown), boolSubtype(BooleanSubtype::True), numSubtype(NumeralSubtype::Positive) {}
    
    explicit Type(TypeKind k) : kind(k), boolSubtype(BooleanSubtype::True), numSubtype(NumeralSubtype::Positive) {}
    
    Type(TypeKind k, BooleanSubtype bs) : kind(k), boolSubtype(bs), numSubtype(NumeralSubtype::Positive) {}
    
    Type(TypeKind k, NumeralSubtype ns) : kind(k), boolSubtype(BooleanSubtype::True), numSubtype(ns) {}
    
    static Type Boolean(BooleanSubtype subtype = BooleanSubtype::True) {
        return Type(TypeKind::Boolean, subtype);
    }
    
    static Type Numeral(NumeralSubtype subtype = NumeralSubtype::Positive) {
        return Type(TypeKind::Numeral, subtype);
    }
    
    static Type String() {
        return Type(TypeKind::String);
    }
    
    static Type Unknown() {
        return Type(TypeKind::Unknown);
    }
    
    bool isNumeral() const { return kind == TypeKind::Numeral; }
    bool isString() const { return kind == TypeKind::String; }
    bool isBoolean() const { return kind == TypeKind::Boolean; }
    bool isUnknown() const { return kind == TypeKind::Unknown; }
    
    std::string toString() const {
        switch (kind) {
            case TypeKind::Boolean:
                return boolSubtype == BooleanSubtype::True ? "boolean(True)" : "boolean(False)";
            case TypeKind::Numeral:
                return numSubtype == NumeralSubtype::Positive ? "numeral(Pos)" : "numeral(Neg)";
            case TypeKind::String:
                return "string";
            case TypeKind::Unknown:
                return "unknown";
        }
        return "unknown";
    }
};

using TypePtr = std::shared_ptr<Type>;
