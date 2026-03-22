#pragma once
#include <string>
#include <string_view>

enum class TokenType {
    // Literals
    STRING,         // $...$  or "..."
    NUMBER,
    IDENTIFIER,

    // Operators
    ASSIGN,         // =
    NOT_EQUAL,      // #=
    LT,             // <
    GT,             // >
    LTE,            // <=
    GTE,            // >=
    MULTIPLY,       // * inside fn...fn context
    AMPERSAND,      // &
    DOT,            // .
    SLASH,          // /
    ARROW,          // ->
    LPAREN,         // (
    RPAREN,         // )
    LBRACKET,       // [
    RBRACKET,       // ]
    LBRACE,         // {
    RBRACE,         // }
    COMMA,          // ,
    KW_FN,          // fn (multiply prefix)
    KW_DISPLAY,     // display $string$ (screen/GUI output)
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    MULTIPLY_EQUAL, // *=
    DIVIDE_EQUAL,   // /=
    AT_EQUAL,       // @= (compound multiplication since * is comment)

    // Keywords
    KW_IF,
    KW_ELSEIF,
    KW_OTHER,
    KW_FOR,
    KW_IN,
    KW_WHILST,
    KW_RETURN,
    KW_USE,
    KW_SAVE,
    KW_AS,
    KW_MAKE,
    KW_DEF,
    KW_TAG,
    KW_RAISE,
    KW_ERR,
    KW_NOT,
    KW_OF,
    KW_TYPE,
    KW_OVERLAP,
    KW_TRUE,
    KW_FALSE,
    KW_ON,
    KW_WHEN,
    KW_MANY,
    KW_TEMP,
    KW_FUNC,
    KW_AT,
    KW_ILIB,        // ilib (import library)
    KW_RANGE,       // range N  → [0..N], Numeral Pos only
    KW_SEQUENCE,    // sequence(x,y) → [x..y], breaks if x > y
    KW_IS,          // is  → equality comparison (==)
    KW_PASS,        // pass → no-op placeholder
    KW_SKIP,        // skip → stop rest of if/elseif/other chain
    KW_BREAK,       // break → exit loop
    KW_CONTINUE,    // continue → next loop iteration
    KW_DESTROY,     // destroy x → remove variable from existence
    KW_PROGRAM_LOOP,// programLoop → controls mainloop continuation

    // Tags (block delimiters)
    TAG_OPEN,       // <tagname>
    TAG_CLOSE,      // second instance of same tag

    // Backend declaration
    BACKEND,        // AC->XX

    // Comment (stripped by lexer)
    COMMENT,

    // Misc
    NEWLINE,
    INDENT,
    DEDENT,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;

    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), col(c) {}
};
