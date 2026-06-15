#pragma once
#include <string>
#include <string_view>

enum class TokenType {
    // Literals
    STRING,         // $...$
    DQUOTE_STRING,  // "..." — only valid inside fn expressions
    NUMBER,
    IDENTIFIER,

    // Operators
    ASSIGN,         // =
    NOT_EQUAL,      // #=
    LT,             // <
    GT,             // >
    MULTIPLY,       // * inside fn...fn context
    AT,             // @ (default multiplication operator)
    AMPERSAND,      // &
    DOUBLE_AMPERSAND, // && (method chaining with different args)
    DOT,            // .
    SLASH,          // /
    DOUBLE_SLASH,   // // integer division
    ARROW,          // ->
    LPAREN,         // (
    RPAREN,         // )
    LBRACKET,       // [
    RBRACKET,       // ]
    LBRACE,         // {
    RBRACE,         // }
    COMMA,          // ,
    COLON,          // : (for dict key:value pairs)
    KW_FN,          // fn (enables *, &, &&, and "..." with quotes)
    KW_DISPLAY,     // display $string$ (screen/GUI output)
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    MULTIPLY_EQUAL, // *=
    DIVIDE_EQUAL,   // /=
    AT_EQUAL,       // @= (compound multiplication)
    PIPE,           // |  (XOR)
    PIPE_EQUAL,     // |= (XOR-assign)
    HASH,           // #  (unary NOT)
    HASH_GT,        // #> (NOT greater → <=)
    HASH_LT,        // #< (NOT less → >=)
    HASH_PIPE,      // #| (XNOR)
    CARET,          // ^ (exponentiation)
    KW_XSUB,        // xsub (inclusive range count: |a-b|+1)

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
    KW_NULL,        // null → backend-specific null (None/null/nullptr/nil/none)
    KW_NIL,         // nil  → empty set ∅; [nil] = set containing empty set {∅}
    KW_ON,
    KW_MANY,
    KW_TEMP,
    KW_FUNC,
    KW_AT,
    KW_ILIB,        // ilib — AC built-in/standard libraries  (resolved from ilib/ folder)
    KW_ELIB,        // elib — packages installed via atar (AC's package manager) (resolved from elib/ folder)
    KW_CLIB,        // clib — custom libraries you made      (subfolders inside clib/ folder)
    KW_FLIB,        // flib (foreign/compiled AC library: use flib /path/to/lib)
    KW_DATAC,       // datac — import a .datac data file (baked in at compile time)
    KW_FROM,        // from (submodule import: from ilib math use statistics)
    KW_RANGE,       // range N  → [0..N], Numeral Pos only
    KW_SEQUENCE,    // sequence(x,y) → [x..y], breaks if x > y
    KW_IS,          // is  → equality comparison
    KW_PASS,        // pass → no-op placeholder
    KW_SKIP,        // skip → stop rest of if/elseif/other chain
    KW_BREAK,       // break → exit loop
    KW_CONTINUE,    // continue → next loop iteration
    KW_DESTROY,     // destroy x → remove variable from existence
    KW_PROGRAM_LOOP,// programLoop → controls mainloop continuation
    KW_CONFIGURE,   // configure
    KW_EVENT_LISTENER, // event-listener
    KW_LISTENER,    // listener
    KW_ESTABLISH,   // establish
    KW_RULE,        // rule
    KW_VALUE,       // value
    KW_INPUT,       // input (send ghost/simulated input)
    KW_BIND,        // bind (for keybinds: bind KEY_W to function)
    KW_TO,          // to (for keybinds: bind KEY_W to function)
    KW_EVAL,        // eval(expr) → evaluate string as AC expression
    KW_BUNDLE,      // bundle X — class/struct definition
    KW_PRIVATE,     // private  — bundle member access modifier
    KW_PUBLIC,      // public   — bundle member access modifier (required when mixed)
    KW_FREE,        // free x, y — declare vars as globally scoped (like Python's global)
    KW_ALIAS,       // alias x = y — bidirectional live binding
    KW_LAZY_EVAL,   // lazy_eval(expr) — deferred safe evaluation
    KW_DEC,         // dec x [= expr]  — coerce x to decimal/float
    KW_INT,         // int x [= expr]  — coerce x to integer
    KW_STRING,      // string x [= expr] — coerce x to string
    KW_BOOL,        // bool x [= expr]   — coerce x to boolean
    KW_PRINT_PAGE,  // print_page → browser window.print()
    KW_ALERT,       // alert $msg$ → browser window.alert()
    KW_SURE,        // sure $msg$ → browser window.confirm() (returns bool)
    KW_TRY,         // try block
    KW_CATCH,       // catch block
    KW_REPORT,      // report <var> — bind caught exception
    KW_AFTER,       // after block (finally)
    KW_USING,       // using <lib> — bring library namespace into scope
    KW_CONST,       // const x = expr — immutable binding (JS/C++ semantics)
    KW_CP,          // cp x = y — explicit value copy
    KW_LENGTH,      // length(x) — array/string length

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
