#include "../include/ac.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"IF",       TokenType::KW_IF},
    {"ELSEIF",   TokenType::KW_ELSEIF},
    {"OTHER",    TokenType::KW_OTHER},
    {"FOR",      TokenType::KW_FOR},
    {"in",       TokenType::KW_IN},
    {"WHILST",   TokenType::KW_WHILST},
    {"return",   TokenType::KW_RETURN},
    {"fn",       TokenType::KW_FN},
    // display is NOT a keyword — it's the widget label constructor; Term.display is the I/O keyword
    {"use",      TokenType::KW_USE},
    {"save",     TokenType::KW_SAVE},
    {"as",       TokenType::KW_AS},
    {"Make",     TokenType::KW_MAKE},
    {"make",     TokenType::KW_MAKE},
    {"def",      TokenType::KW_DEF},
    {"tag",      TokenType::KW_TAG},
    {"raise",    TokenType::KW_RAISE},
    {"ERR",      TokenType::KW_ERR},
    {"not",      TokenType::KW_NOT},
    {"and",      TokenType::KW_AND},
    {"or",       TokenType::KW_OR},
    {"xor",      TokenType::KW_XOR},
    {"bor",      TokenType::KW_BOR},
    {"of",       TokenType::KW_OF},
    {"type",     TokenType::KW_TYPE},
    {"Overlap",  TokenType::KW_OVERLAP},
    {"overlap",  TokenType::KW_OVERLAP},
    {"True",     TokenType::KW_TRUE},
    {"False",    TokenType::KW_FALSE},
    {"null",     TokenType::KW_NULL},
    {"nil",      TokenType::KW_NIL},
    {"on",       TokenType::KW_ON},
    {"many",     TokenType::KW_MANY},
    {"temp",     TokenType::KW_TEMP},
    {"func",     TokenType::KW_FUNC},
    {"at",       TokenType::KW_AT},
    {"ilib",     TokenType::KW_ILIB},
    {"elib",     TokenType::KW_ELIB},
    {"clib",     TokenType::KW_CLIB},
    {"flib",     TokenType::KW_FLIB},
    {"datac",    TokenType::KW_DATAC},
    {"from",     TokenType::KW_FROM},
    {"range",    TokenType::KW_RANGE},
    {"sequence", TokenType::KW_SEQUENCE},
    {"is",       TokenType::KW_IS},
    {"xsub",     TokenType::KW_XSUB},
    {"pass",     TokenType::KW_PASS},
    {"skip",     TokenType::KW_SKIP},
    // "break" removed — use /end to exit a loop in AC
    {"continue", TokenType::KW_CONTINUE},
    {"destroy",  TokenType::KW_DESTROY},
    {"programLoop", TokenType::KW_PROGRAM_LOOP},
    {"configure", TokenType::KW_CONFIGURE},
    {"event-listener", TokenType::KW_EVENT_LISTENER},
    {"listener", TokenType::KW_LISTENER},
    {"establish", TokenType::KW_ESTABLISH},
    {"rule", TokenType::KW_RULE},
    {"value", TokenType::KW_VALUE},
    {"eval",        TokenType::KW_EVAL},
    {"input",       TokenType::KW_INPUT},
    {"bind",        TokenType::KW_BIND},
    {"to",          TokenType::KW_TO},
    {"bundle",      TokenType::KW_BUNDLE},
    {"private",     TokenType::KW_PRIVATE},
    {"public",      TokenType::KW_PUBLIC},
    {"free",        TokenType::KW_FREE},
    {"alias",       TokenType::KW_ALIAS},
    {"lazy_eval",   TokenType::KW_LAZY_EVAL},
    {"to_dec",      TokenType::KW_DEC},
    {"to_int",      TokenType::KW_INT},
    {"to_string",   TokenType::KW_STRING},
    {"to_bool",     TokenType::KW_BOOL},
    {"print_page",  TokenType::KW_PRINT_PAGE},
    {"alert",       TokenType::KW_ALERT},
    {"sure",        TokenType::KW_SURE},
    {"try",    TokenType::KW_TRY},
    {"catch",  TokenType::KW_CATCH},
    {"report", TokenType::KW_REPORT},
    {"after",  TokenType::KW_AFTER},
    {"using",  TokenType::KW_USING},
    {"const",  TokenType::KW_CONST},
    {"cp",     TokenType::KW_CP},
    {"length", TokenType::KW_LENGTH},
    {"LineUp", TokenType::KW_LINEUP},
    {"export", TokenType::KW_EXPORT},
};

// First-byte fast-path: if a word's first byte is NOT in kwFirstByte, it can't be a keyword.
// Initialized after KEYWORDS so the order-of-initialization is well-defined.
static bool kwFirstByte[256];
static const bool _kwFbInit = [] {
    for (auto& [kw, _] : KEYWORDS)
        if (!kw.empty()) kwFirstByte[(uint8_t)kw[0]] = true;
    return true;
}();

class Lexer {
public:
    std::string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    explicit Lexer(std::string source) : src(std::move(source)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        tokens.reserve(src.size() / 4);
        std::vector<int> indentStack = {0};
        indentStack.reserve(64);

        while (pos < src.size()) {
            // Handle newlines and indentation
            if (src[pos] == '\n') {
                tokens.emplace_back(TokenType::NEWLINE, "\n", line, col);
                pos++; line++; col = 1;

                // Count leading spaces for indent tracking
                int spaces = 0;
                while (pos < src.size() && src[pos] == ' ') { spaces++; pos++; col++; }
                while (pos < src.size() && src[pos] == '\t') { spaces += 4; pos++; col++; }

                // Skip blank lines
                if (pos < src.size() && src[pos] == '\n') continue;

                int current = indentStack.back();
                if (spaces > current) {
                    indentStack.push_back(spaces);
                    tokens.emplace_back(TokenType::INDENT, "", line, col);
                } else {
                    while (spaces < indentStack.back()) {
                        indentStack.pop_back();
                        tokens.emplace_back(TokenType::DEDENT, "", line, col);
                    }
                }
                continue;
            }

            // Skip spaces mid-line
            if (src[pos] == ' ' || src[pos] == '\t') { pos++; col++; continue; }

            // * — always numeric multiply (or *=)
            if (src[pos] == '*') {
                if (pos + 1 < src.size() && src[pos+1] == '=') {
                    tokens.emplace_back(TokenType::MULTIPLY_EQUAL, "*=", line, col);
                    pos += 2; col += 2;
                } else {
                    tokens.emplace_back(TokenType::MULTIPLY, "*", line, col);
                    pos++; col++;
                }
                continue;
            }

            // String: $...$ (with escape sequences) and r$...$ (raw, no escapes)
            if (src[pos] == '$') {
                // Check if previous token was IDENTIFIER "r" → raw string
                bool rawStr = !tokens.empty() &&
                              tokens.back().type == TokenType::IDENTIFIER &&
                              tokens.back().value == "r";
                if (rawStr) tokens.pop_back(); // consume the 'r' prefix token

                int sc = col; pos++; col++;
                std::string s;
                if (rawStr) {
                    // Raw string: copy bytes until closing $, no escape processing
                    while (pos < src.size() && src[pos] != '$') {
                        if (src[pos] == '\n') { line++; col = 1; } else col++;
                        s += src[pos++];
                    }
                } else {
                    // Normal string: process escape sequences
                    while (pos < src.size() && src[pos] != '$') {
                        if (src[pos] == '\\' && pos + 1 < src.size()) {
                            pos++; col++;
                            char esc = src[pos];
                            if      (esc == 'n')  s += '\n';
                            else if (esc == 't')  s += '\t';
                            else if (esc == 'r')  s += '\r';
                            else if (esc == '\\') s += '\\';
                            else if (esc == '$')  s += '$';
                            else { s += '\\'; s += esc; }
                        } else {
                            if (src[pos] == '\n') { line++; col = 1; } else col++;
                            s += src[pos];
                        }
                        pos++; col++;
                    }
                }
                if (pos < src.size()) { pos++; col++; } // skip closing $
                tokens.emplace_back(TokenType::STRING, std::move(s), line, sc);
                continue;
            }

            // String: "..." (supports escape sequences with fn prefix)
            if (src[pos] == '"') {
                int sc = col; pos++; col++;
                std::string s;
                while (pos < src.size() && src[pos] != '"') {
                    if (src[pos] == '\\' && pos + 1 < src.size()) {
                        // Handle escape sequences
                        pos++; col++;
                        char next = src[pos];
                        if (next == '"') s += '"';
                        else if (next == '\\') s += '\\';
                        else if (next == 'n') s += '\n';
                        else if (next == 't') s += '\t';
                        else s += next;  // Unknown escape, keep as-is
                        pos++; col++;
                    } else {
                        s += src[pos++]; col++;
                    }
                }
                if (pos < src.size()) { pos++; col++; }
                tokens.emplace_back(TokenType::DQUOTE_STRING, s, line, sc);
                continue;
            }

            // Tag: <tagname> — only if < is followed by a letter
            if (src[pos] == '<' && pos+1 < src.size() && std::isalpha(src[pos+1])) {
                int sc = col; pos++; col++;
                std::string name;
                while (pos < src.size() && src[pos] != '>') { name += src[pos++]; col++; }
                if (pos < src.size()) { pos++; col++; }

                // Foreign block: capture raw text until closing <Foreign>
                if (name == "Foreign") {
                    // skip the newline right after <Foreign>
                    if (pos < src.size() && src[pos] == '\n') { pos++; line++; col = 1; }
                    std::string raw;
                    const std::string closeTag = "<Foreign>";
                    while (pos < src.size()) {
                        if (src.substr(pos, closeTag.size()) == closeTag) {
                            pos += closeTag.size(); col += closeTag.size();
                            break;
                        }
                        if (src[pos] == '\n') { line++; col = 1; }
                        raw += src[pos++];
                    }
                    // trim trailing newline
                    if (!raw.empty() && raw.back() == '\n') raw.pop_back();

                    // detect base indent from first non-empty line and strip it
                    std::istringstream ss(raw);
                    std::string ln;
                    size_t baseIndent = std::string::npos;
                    std::vector<std::string> lines;
                    while (std::getline(ss, ln)) lines.push_back(ln);
                    for (auto& l : lines) {
                        size_t first = l.find_first_not_of(" \t");
                        if (first != std::string::npos && first < baseIndent)
                            baseIndent = first;
                    }
                    if (baseIndent == std::string::npos) baseIndent = 0;
                    std::string stripped;
                    for (auto& l : lines) {
                        if (!stripped.empty()) stripped += '\n';
                        std::string trimmed = l;
                        // strip trailing whitespace
                        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
                            trimmed.pop_back();
                        if (trimmed.size() >= baseIndent) stripped += trimmed.substr(baseIndent);
                        else stripped += trimmed;
                    }

                    tokens.emplace_back(TokenType::TAG_OPEN, "__foreign__" + stripped, line, sc);
                    continue;
                }

                tokens.emplace_back(TokenType::TAG_OPEN, name, line, sc);
                continue;
            }

            // Backend: AC->XX or AC LIB (library header, no mainloop)
            if (src.substr(pos, 3) == "AC-") {
                int sc = col;
                pos += 3; col += 3;
                if (pos < src.size() && src[pos] == '>') { pos++; col++; }
                std::string backend;
                while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '+')) {
                    backend += src[pos++]; col++;
                }
                tokens.emplace_back(TokenType::BACKEND, backend, line, sc);
                continue;
            }
            if (src.size() > pos + 5 && src.substr(pos, 6) == "AC LIB") {
                // Check it's followed by end-of-line or EOF (not "AC LIBRARY" etc.)
                size_t eol = pos + 6;
                if (eol >= src.size() || src[eol] == '\n' || src[eol] == '\r' || src[eol] == ' ' || src[eol] == '\t') {
                    int sc = col;
                    pos += 6; col += 6;
                    tokens.emplace_back(TokenType::BACKEND, std::string("LIB"), line, sc);
                    continue;
                }
            }

            // / — block comment /*...*/, compound /=, or slash command
            if (src[pos] == '/') {
                int sc = col; pos++; col++;
                // Block comment: /* ... */
                if (pos < src.size() && src[pos] == '*') {
                    pos++; col++;
                    while (pos + 1 < src.size()) {
                        if (src[pos] == '*' && src[pos+1] == '/') { pos += 2; col += 2; break; }
                        if (src[pos] == '\n') { line++; col = 1; } else col++;
                        pos++;
                    }
                    continue;
                }
                // Check for // (integer division) before /=
                if (pos < src.size() && src[pos] == '/') {
                    pos++; col++;
                    tokens.emplace_back(TokenType::DOUBLE_SLASH, "//", line, sc);
                } else if (pos < src.size() && src[pos] == '=') {
                    pos++; col++;
                    tokens.emplace_back(TokenType::DIVIDE_EQUAL, "/=", line, sc);
                } else {
                    // Regular slash command like /kill, or bare division operator /
                    std::string word;
                    while (pos < src.size() && std::isalpha(src[pos])) { word += src[pos++]; col++; }
                    tokens.emplace_back(TokenType::SLASH, word.empty() ? "/" : word, line, sc);
                }
                continue;
            }

            // # family: #=, #>, #<, #|, #  (not-equal, not-gt, not-lt, xnor, unary-not)
            if (src[pos] == '#') {
                int sc = col;
                if (pos+1 < src.size() && src[pos+1] == '=') {
                    tokens.emplace_back(TokenType::NOT_EQUAL, "#=", line, sc);
                    pos += 2; col += 2;
                } else if (pos+1 < src.size() && src[pos+1] == '>') {
                    tokens.emplace_back(TokenType::HASH_GT, "#>", line, sc);
                    pos += 2; col += 2;
                } else if (pos+1 < src.size() && src[pos+1] == '<') {
                    tokens.emplace_back(TokenType::HASH_LT, "#<", line, sc);
                    pos += 2; col += 2;
                } else if (pos+1 < src.size() && src[pos+1] == '|') {
                    tokens.emplace_back(TokenType::HASH_PIPE, "#|", line, sc);
                    pos += 2; col += 2;
                } else {
                    tokens.emplace_back(TokenType::HASH, "#", line, sc);
                    pos += 1; col += 1;
                }
                continue;
            }

            // Numbers
            if (std::isdigit(src[pos])) {
                int sc = col;
                std::string num;
                while (pos < src.size() && (std::isdigit(src[pos]) || src[pos] == '.' || src[pos] == 'x')) {
                    num += src[pos++]; col++;
                }
                tokens.emplace_back(TokenType::NUMBER, num, line, sc);
                continue;
            }

            // Identifiers and keywords
            if (std::isalpha(src[pos]) || src[pos] == '_') {
                int sc = col;
                std::string word;
                while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_' || src[pos] == '-')) {
                    word += src[pos++]; col++;
                }
                // First-byte fast-path: skip hash lookup if word can't be a keyword
                if (!word.empty() && !kwFirstByte[(uint8_t)word[0]]) {
                    tokens.emplace_back(TokenType::IDENTIFIER, word, line, sc);
                    continue;
                }
                auto it = KEYWORDS.find(word);
                if (it != KEYWORDS.end()) {
                    tokens.emplace_back(it->second, word, line, sc);
                } else {
                    // Case-insensitive boolean check
                    std::string lower = word;
                    for (auto& ch : lower) ch = std::tolower(ch);
                    if (lower == "true")
                        tokens.emplace_back(TokenType::KW_TRUE, word, line, sc);
                    else if (lower == "false")
                        tokens.emplace_back(TokenType::KW_FALSE, word, line, sc);
                    else
                        tokens.emplace_back(TokenType::IDENTIFIER, word, line, sc);
                }
                continue;
            }

            // Single char tokens
            int sc = col;
            char c = src[pos++]; col++;
            switch (c) {
                case '+':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::PLUS_EQUAL, "+=", line, sc); }
                    else tokens.emplace_back(TokenType::IDENTIFIER, "+", line, sc);
                    break;
                case '-':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::MINUS_EQUAL, "-=", line, sc); }
                    else tokens.emplace_back(TokenType::IDENTIFIER, "-", line, sc);
                    break;
                case '*':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::MULTIPLY_EQUAL, "*=", line, sc); }
                    else tokens.emplace_back(TokenType::MULTIPLY, "*", line, sc);
                    break;
                case '@':
                    if (pos < src.size() && src[pos] == '=') { 
                        pos++; col++; 
                        tokens.emplace_back(TokenType::AT_EQUAL, "@=", line, sc); 
                    } else {
                        tokens.emplace_back(TokenType::AT, "@", line, sc);
                    }
                    break;
                case '=': tokens.emplace_back(TokenType::ASSIGN,    "=",  line, sc); break;
                case '.': tokens.emplace_back(TokenType::DOT,       ".",  line, sc); break;
                case '(': tokens.emplace_back(TokenType::LPAREN,    "(",  line, sc); break;
                case ')': tokens.emplace_back(TokenType::RPAREN,    ")",  line, sc); break;
                case ',': tokens.emplace_back(TokenType::COMMA,     ",",  line, sc); break;
                case ':': tokens.emplace_back(TokenType::COLON,     ":",  line, sc); break;
                case '&':
                    if (pos < src.size() && src[pos] == '&') { 
                        pos++; col++; 
                        tokens.emplace_back(TokenType::DOUBLE_AMPERSAND, "&&", line, sc); 
                    } else {
                        tokens.emplace_back(TokenType::AMPERSAND, "&",  line, sc);
                    }
                    break;
                case '|':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::PIPE_EQUAL, "|=", line, sc); }
                    else tokens.emplace_back(TokenType::PIPE, "|", line, sc);
                    break;
                case '^': tokens.emplace_back(TokenType::CARET, "^", line, sc); break;
                case '~': tokens.emplace_back(TokenType::TILDE, "~", line, sc); break;
                case '<':
                    tokens.emplace_back(TokenType::LT, "<", line, sc);
                    break;
                case '>':
                    tokens.emplace_back(TokenType::GT, ">", line, sc);
                    break;
                case '[': tokens.emplace_back(TokenType::LBRACKET,  "[",  line, sc); break;
                case ']': tokens.emplace_back(TokenType::RBRACKET,  "]",  line, sc); break;
                case '{': tokens.emplace_back(TokenType::LBRACE,    "{",  line, sc); break;
                case '}': tokens.emplace_back(TokenType::RBRACE,    "}",  line, sc); break;
                case '\\':
                    // \ws → whitespace sentinel literal
                    if (pos < src.size() && src[pos] == 'w' &&
                        pos + 1 < src.size() && src[pos+1] == 's' &&
                        (pos + 2 >= src.size() || !std::isalnum((unsigned char)src[pos+2]))) {
                        pos += 2;
                        tokens.emplace_back(TokenType::STRING, "__WS__", line, sc);
                    }
                    // otherwise skip bare backslash
                    break;
                default: break; // skip unknown
            }
        }

        // Emit remaining DEDENTs
        tokens.emplace_back(TokenType::END_OF_FILE, "", line, col);
        return tokens;
    }
};

// Expose for linking
std::vector<Token> lex(const std::string& source) {
    Lexer l(source);
    return l.tokenize();
}
