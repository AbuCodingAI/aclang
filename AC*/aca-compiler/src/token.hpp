#pragma once
#include <string>

namespace aca {

enum class TokenType {
    HEADER, // AC*->TARGET ...

    IDENT,
    INT_LIT,
    DEC_LIT,
    STRING_LIT,
    BOOL_LIT,

    NEWLINE,
    INDENT,
    DEDENT,

    EQ,
    LT,
    GT,
    LPAREN,
    RPAREN,
    LBRACK,
    RBRACK,
    COMMA,
    DOT,

    STREAM_TO, // >>
    ARROW_FAT, // =>

    PLUS,
    MINUS,
    STAR,
    SLASH,
    AT,      // @
    PLUS_EQ, // +=
    MINUS_EQ,// -=
    AT_EQ,   // @=

    NOT_GT, // #>   (NOT_GT)
    NOT_LT, // #<   (NOT_LT)
    NOT_EQ, // #=   (NOT_EQ)

    KW_MAKE,
    KW_FUNC,
    KW_IF,
    KW_OTHER,
    KW_WHILST,
    KW_RETURN,
    KW_IS,
    KW_MOD,

    TAG, // <mainloop>

    END,
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
};

} // namespace aca

