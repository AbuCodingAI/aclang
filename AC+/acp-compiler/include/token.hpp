#pragma once
#include <string>

// AC+ token types
enum class TokenType {
    // Literals
    IDENTIFIER,
    NUMBER,         // integer or hex (0x...)
    STRING,         // $...$

    // Symbols
    ASSIGN,         // =
    PLUS,           // +
    MINUS,          // -
    MULTIPLY,       // *
    DIVIDE,         // /
    AT,             // @  (polymorphic multiply)
    LSHIFT,         // <<  (memory write)
    LPAREN,         // (
    RPAREN,         // )
    COMMA,          // ,
    DOT,            // .

    // Compound assignment
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    MULTIPLY_EQUAL, // *=
    DIVIDE_EQUAL,   // /=
    AT_EQUAL,       // @=

    // Comparison
    IS,             // is  (==)
    NOT_EQUAL,      // #=
    LESS,           // <
    GREATER,        // >
    LESS_EQUAL,     // #<  (less-or-equal alternate: # negates)
    GREATER_EQUAL,  // #>  (greater-or-equal alternate)
    HASH,           // #   (NOT prefix)

    // Tags and delimiters
    TAG,            // <tagname>
    NEWLINE,
    INDENT,
    DEDENT,
    END_OF_FILE,

    // Keywords
    KW_MAKE,        // Make
    KW_FUNC,        // func
    KW_RETURN,      // return
    KW_IF,          // IF
    KW_ELSEIF,      // ELSEIF
    KW_OTHER,       // OTHER
    KW_WHILST,      // WHILST
    KW_FOR,         // FOR
    KW_IN,          // in
    KW_RANGE,       // range
    KW_USE,         // use
    KW_FLIB,        // flib
    KW_REG,         // reg      (register variable)
    KW_STACK,       // stack    (stack variable)
    KW_LOCATE,      // locate   (address-of)
    KW_NIL,         // nil
    KW_BACKEND,     // AC+ LIB header token (value = "LIB" or empty)
    BACKEND,        // backend declaration line (AC+ LIB)
    SLASH_CMD,      // /kill /stop /end
};

struct Token {
    TokenType   type;
    std::string value;
    int         line = 0;
    int         col  = 0;
    Token() = default;
    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), col(c) {}
};
