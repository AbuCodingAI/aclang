#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace aca {

Lexer::Lexer(std::string source) : src(std::move(source)) {}

bool Lexer::eof(size_t lookahead) const { return i + lookahead >= src.size(); }
char Lexer::peek() const { return eof() ? '\0' : src[i]; }
char Lexer::peek2() const { return eof(1) ? '\0' : src[i + 1]; }

void Lexer::adv() {
    if (eof()) return;
    i++;
    col++;
}

std::string Lexer::loc() const { return "Line " + std::to_string(line) + ":" + std::to_string(col) + " "; }

Token Lexer::tok(TokenType t, std::string lexeme) const { return Token{t, std::move(lexeme), line, col}; }

bool Lexer::isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
bool Lexer::isIdent(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '-'; }

bool Lexer::ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); k++) {
        if (std::tolower((unsigned char)a[k]) != std::tolower((unsigned char)b[k])) return false;
    }
    return true;
}

void Lexer::lexBlockComment() {
    adv();
    adv();
    while (!eof()) {
        if (peek() == '*' && peek2() == '/') {
            adv();
            adv();
            return;
        }
        if (peek() == '\n') {
            adv();
            line++;
            col = 1;
            continue;
        }
        adv();
    }
    throw std::runtime_error(loc() + "Unterminated block comment");
}

Token Lexer::lexNumber() {
    int startCol = col;
    size_t start = i;
    while (!eof() && std::isdigit((unsigned char)peek())) adv();
    bool isDec = false;
    if (!eof() && peek() == '.' && !eof(1) && std::isdigit((unsigned char)peek2())) {
        isDec = true;
        adv();
        while (!eof() && std::isdigit((unsigned char)peek())) adv();
    }
    std::string s = src.substr(start, i - start);
    return Token{isDec ? TokenType::DEC_LIT : TokenType::INT_LIT, std::move(s), line, startCol};
}

Token Lexer::lexTag() {
    int startCol = col;
    size_t start = i;
    adv(); // <
    while (!eof() && peek() != '>') adv();
    if (eof()) throw std::runtime_error(loc() + "Unterminated tag");
    adv(); // >
    std::string s = src.substr(start, i - start);
    return Token{TokenType::TAG, std::move(s), line, startCol};
}

Token Lexer::lexIdentLike() {
    int startCol = col;
    size_t start = i;
    adv();
    while (!eof() && isIdent(peek())) adv();
    std::string s = src.substr(start, i - start);

    if (ieq(s, "true") || ieq(s, "false")) return Token{TokenType::BOOL_LIT, s, line, startCol};
    if (s == "Make" || s == "make") return Token{TokenType::KW_MAKE, s, line, startCol};
    if (s == "func") return Token{TokenType::KW_FUNC, s, line, startCol};
    if (s == "IF") return Token{TokenType::KW_IF, s, line, startCol};
    if (s == "OTHER") return Token{TokenType::KW_OTHER, s, line, startCol};
    if (s == "WHILST") return Token{TokenType::KW_WHILST, s, line, startCol};
    if (s == "return") return Token{TokenType::KW_RETURN, s, line, startCol};
    if (s == "is") return Token{TokenType::KW_IS, s, line, startCol};
    if (s == "mod") return Token{TokenType::KW_MOD, s, line, startCol};

    return Token{TokenType::IDENT, std::move(s), line, startCol};
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> out;
    std::vector<int> indents{0};

    while (!eof()) {
        // Header: AC*->TARGET ...
        if (line == 1 && col == 1) {
            const std::string prefix = "AC*->";
            if (src.size() >= prefix.size() && src.compare(0, prefix.size(), prefix) == 0) {
                int startCol = col;
                size_t start = i;
                while (!eof() && peek() != '\n') adv();
                std::string s = src.substr(start, i - start);
                out.push_back(Token{TokenType::HEADER, std::move(s), 1, startCol});
                continue;
            }
        }

        if (peek() == '\n') {
            out.push_back(tok(TokenType::NEWLINE, "\n"));
            adv();
            line++;
            col = 1;

            int spaces = 0;
            while (!eof() && (peek() == ' ' || peek() == '\t')) {
                spaces += (peek() == ' ') ? 1 : 4;
                adv();
            }
            if (!eof() && peek() == '\n') continue;

            int cur = indents.back();
            if (spaces > cur) {
                indents.push_back(spaces);
                out.push_back(tok(TokenType::INDENT, ""));
            } else {
                while (spaces < indents.back()) {
                    indents.pop_back();
                    out.push_back(tok(TokenType::DEDENT, ""));
                }
            }
            continue;
        }

        if (std::isspace((unsigned char)peek())) {
            adv();
            continue;
        }

        if (peek() == '/' && peek2() == '*') {
            lexBlockComment();
            continue;
        }

        // r$...$ raw string (no escapes)
        if (peek() == 'r' && peek2() == '$') {
            int sc = col;
            adv();
            adv();
            std::string s;
            while (!eof() && peek() != '$') {
                if (peek() == '\n') {
                    line++;
                    col = 1;
                    s.push_back('\n');
                    adv();
                    continue;
                }
                s.push_back(peek());
                adv();
            }
            if (!eof() && peek() == '$') adv();
            out.push_back(Token{TokenType::STRING_LIT, std::move(s), line, sc});
            continue;
        }

        // $...$ string (escapes)
        if (peek() == '$') {
            int sc = col;
            adv();
            std::string s;
            while (!eof() && peek() != '$') {
                if (peek() == '\\' && !eof(1)) {
                    adv();
                    char esc = peek();
                    if (esc == 'n') s.push_back('\n');
                    else if (esc == 't') s.push_back('\t');
                    else if (esc == 'r') s.push_back('\r');
                    else if (esc == '\\') s.push_back('\\');
                    else if (esc == '$') s.push_back('$');
                    else {
                        s.push_back('\\');
                        s.push_back(esc);
                    }
                    adv();
                    continue;
                }
                if (peek() == '\n') {
                    line++;
                    col = 1;
                    s.push_back('\n');
                    adv();
                    continue;
                }
                s.push_back(peek());
                adv();
            }
            if (!eof() && peek() == '$') adv();
            out.push_back(Token{TokenType::STRING_LIT, std::move(s), line, sc});
            continue;
        }

        if (peek() == '<') {
            out.push_back(lexTag());
            continue;
        }

        // Multi-char ops
        if (peek() == '>' && peek2() == '>') {
            out.push_back(tok(TokenType::STREAM_TO, ">>"));
            adv();
            adv();
            continue;
        }
        if (peek() == '=' && peek2() == '>') {
            out.push_back(tok(TokenType::ARROW_FAT, "=>"));
            adv();
            adv();
            continue;
        }
        if (peek() == '+' && peek2() == '=') {
            out.push_back(tok(TokenType::PLUS_EQ, "+="));
            adv();
            adv();
            continue;
        }
        if (peek() == '-' && peek2() == '=') {
            out.push_back(tok(TokenType::MINUS_EQ, "-="));
            adv();
            adv();
            continue;
        }
        if (peek() == '@' && peek2() == '=') {
            out.push_back(tok(TokenType::AT_EQ, "@="));
            adv();
            adv();
            continue;
        }
        if (peek() == '#' && peek2() == '>') {
            out.push_back(tok(TokenType::NOT_GT, "#>"));
            adv();
            adv();
            continue;
        }
        if (peek() == '#' && peek2() == '<') {
            out.push_back(tok(TokenType::NOT_LT, "#<"));
            adv();
            adv();
            continue;
        }
        if (peek() == '#' && peek2() == '=') {
            out.push_back(tok(TokenType::NOT_EQ, "#="));
            adv();
            adv();
            continue;
        }

        switch (peek()) {
            case '=': out.push_back(tok(TokenType::EQ, "=")); adv(); continue;
            case '(': out.push_back(tok(TokenType::LPAREN, "(")); adv(); continue;
            case ')': out.push_back(tok(TokenType::RPAREN, ")")); adv(); continue;
            case '[': out.push_back(tok(TokenType::LBRACK, "[")); adv(); continue;
            case ']': out.push_back(tok(TokenType::RBRACK, "]")); adv(); continue;
            case ',': out.push_back(tok(TokenType::COMMA, ",")); adv(); continue;
            case '.': out.push_back(tok(TokenType::DOT, ".")); adv(); continue;
            case '+': out.push_back(tok(TokenType::PLUS, "+")); adv(); continue;
            case '-': out.push_back(tok(TokenType::MINUS, "-")); adv(); continue;
            case '*': out.push_back(tok(TokenType::STAR, "*")); adv(); continue;
            case '/': out.push_back(tok(TokenType::SLASH, "/")); adv(); continue;
            case '@': out.push_back(tok(TokenType::AT, "@")); adv(); continue;
            case '<': out.push_back(tok(TokenType::LT, "<")); adv(); continue;
            case '>': out.push_back(tok(TokenType::GT, ">")); adv(); continue;
        }

        if (std::isdigit((unsigned char)peek())) {
            out.push_back(lexNumber());
            continue;
        }
        if (isIdentStart(peek())) {
            out.push_back(lexIdentLike());
            continue;
        }

        throw std::runtime_error(loc() + "Unexpected character: '" + std::string(1, peek()) + "'");
    }

    while ((int)indents.size() > 1) {
        indents.pop_back();
        out.push_back(tok(TokenType::DEDENT, ""));
    }
    out.push_back(tok(TokenType::END, ""));
    return out;
}

} // namespace aca

