#include "../include/token.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <cctype>
#include <cstring>
#include <stdexcept>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"Make",    TokenType::KW_MAKE},
    {"func",    TokenType::KW_FUNC},
    {"return",  TokenType::KW_RETURN},
    {"IF",      TokenType::KW_IF},
    {"ELSEIF",  TokenType::KW_ELSEIF},
    {"OTHER",   TokenType::KW_OTHER},
    {"WHILST",  TokenType::KW_WHILST},
    {"FOR",     TokenType::KW_FOR},
    {"in",      TokenType::KW_IN},
    {"range",   TokenType::KW_RANGE},
    {"use",     TokenType::KW_USE},
    {"flib",    TokenType::KW_FLIB},
    {"reg",     TokenType::KW_REG},
    {"stack",   TokenType::KW_STACK},
    {"locate",  TokenType::KW_LOCATE},
    {"nil",     TokenType::KW_NIL},
    {"is",      TokenType::IS},
};

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> tokens;
    size_t pos  = 0;
    int    line = 1;
    int    col  = 1;

    // Indentation stack: track column of each indent level
    std::vector<int> indentStack = {0};

    auto emit = [&](TokenType t, std::string v, int l = 0, int c = 0) {
        tokens.emplace_back(t, std::move(v), l ? l : line, c ? c : col);
    };

    while (pos < src.size()) {
        // ── Handle "AC+ LIB" or "AC+ ..." header ──────────────────────────
        if (src.size() > pos + 2 && src.substr(pos, 3) == "AC+") {
            int sc = col;
            pos += 3; col += 3;
            while (pos < src.size() && src[pos] == ' ') { pos++; col++; }
            std::string rest;
            while (pos < src.size() && src[pos] != '\n' && src[pos] != '\r') {
                rest += src[pos++]; col++;
            }
            emit(TokenType::BACKEND, rest, line, sc);
            continue;
        }

        // ── Newline + indentation ──────────────────────────────────────────
        if (src[pos] == '\n' || src[pos] == '\r') {
            emit(TokenType::NEWLINE, "");
            if (src[pos] == '\r' && pos + 1 < src.size() && src[pos+1] == '\n') pos++;
            pos++; line++; col = 1;

            // Count leading spaces on next line
            int spaces = 0;
            while (pos < src.size() && src[pos] == ' ') { spaces++; pos++; }
            col = spaces + 1;

            // Skip blank lines
            if (pos < src.size() && (src[pos] == '\n' || src[pos] == '\r')) continue;
            if (pos >= src.size()) break;

            int cur_indent = indentStack.back();
            if (spaces > cur_indent) {
                indentStack.push_back(spaces);
                emit(TokenType::INDENT, "");
            } else {
                while (spaces < indentStack.back()) {
                    indentStack.pop_back();
                    emit(TokenType::DEDENT, "");
                }
            }
            continue;
        }

        // ── Skip whitespace (non-newline) ──────────────────────────────────
        if (src[pos] == ' ' || src[pos] == '\t') {
            pos++; col++;
            continue;
        }

        // ── Block comment /* ... */ ────────────────────────────────────────
        if (src[pos] == '/' && pos + 1 < src.size() && src[pos+1] == '*') {
            pos += 2; col += 2;
            while (pos + 1 < src.size()) {
                if (src[pos] == '*' && src[pos+1] == '/') { pos += 2; col += 2; break; }
                if (src[pos] == '\n') { line++; col = 1; } else col++;
                pos++;
            }
            continue;
        }

        // ── Line comment  -- ──────────────────────────────────────────────
        if (src[pos] == '-' && pos + 1 < src.size() && src[pos+1] == '-') {
            while (pos < src.size() && src[pos] != '\n') { pos++; col++; }
            continue;
        }

        // ── Slash commands /kill /stop /end ───────────────────────────────
        if (src[pos] == '/') {
            int sc = col; pos++; col++;
            std::string word;
            while (pos < src.size() && std::isalpha(src[pos])) { word += src[pos++]; col++; }
            emit(TokenType::SLASH_CMD, word, line, sc);
            continue;
        }

        // ── String $...$ ──────────────────────────────────────────────────
        if (src[pos] == '$') {
            bool raw = !tokens.empty() &&
                       tokens.back().type == TokenType::IDENTIFIER &&
                       tokens.back().value == "r";
            if (raw) tokens.pop_back();

            int sc = col; pos++; col++;
            std::string s;
            if (raw) {
                while (pos < src.size() && src[pos] != '$') { s += src[pos++]; col++; }
            } else {
                while (pos < src.size() && src[pos] != '$') {
                    if (src[pos] == '\\' && pos + 1 < src.size()) {
                        pos++; col++;
                        char esc = src[pos];
                        if      (esc == 'n')  s += '\n';
                        else if (esc == 't')  s += '\t';
                        else if (esc == '\\') s += '\\';
                        else if (esc == '$')  s += '$';
                        else { s += '\\'; s += esc; }
                    } else {
                        s += src[pos];
                    }
                    pos++; col++;
                }
            }
            if (pos < src.size()) { pos++; col++; }
            emit(TokenType::STRING, s, line, sc);
            continue;
        }

        // ── Hex / decimal numbers ──────────────────────────────────────────
        if (std::isdigit(src[pos]) || (src[pos] == '0' && pos+1 < src.size() && src[pos+1] == 'x')) {
            int sc = col;
            std::string num;
            if (src[pos] == '0' && pos+1 < src.size() && (src[pos+1] == 'x' || src[pos+1] == 'X')) {
                num += src[pos++]; col++;
                num += src[pos++]; col++;
                while (pos < src.size() && std::isxdigit(src[pos])) { num += src[pos++]; col++; }
            } else {
                while (pos < src.size() && (std::isdigit(src[pos]) || src[pos] == '.')) { num += src[pos++]; col++; }
            }
            emit(TokenType::NUMBER, num, line, sc);
            continue;
        }

        // ── Tags <tagname> ────────────────────────────────────────────────
        if (src[pos] == '<' && pos+1 < src.size() && std::isalpha(src[pos+1])) {
            int sc = col; pos++; col++;
            std::string name;
            while (pos < src.size() && src[pos] != '>') { name += src[pos++]; col++; }
            if (pos < src.size()) { pos++; col++; }
            emit(TokenType::TAG, name, line, sc);
            continue;
        }

        // ── Identifiers and keywords ──────────────────────────────────────
        if (std::isalpha(src[pos]) || src[pos] == '_') {
            int sc = col;
            std::string ident;
            // AC+ register names can start with digit (e.g. 64box1) — handled by number path
            while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_')) {
                ident += src[pos++]; col++;
            }
            auto it = KEYWORDS.find(ident);
            if (it != KEYWORDS.end()) {
                emit(it->second, ident, line, sc);
            } else {
                emit(TokenType::IDENTIFIER, ident, line, sc);
            }
            continue;
        }

        // ── Register names starting with digit: 64box1, 32vault, etc. ────
        // Already handled by number path for the leading digits, then identifier continues.
        // Re-lex: if NUMBER immediately followed by alpha → register name
        // (This is handled in parser by checking NUMBER+IDENTIFIER adjacency.)

        // ── Operators ────────────────────────────────────────────────────
        int sc = col;
        char c = src[pos++]; col++;

        switch (c) {
        case '=': emit(TokenType::ASSIGN,   "=",  line, sc); break;
        case ',': emit(TokenType::COMMA,    ",",  line, sc); break;
        case '.': emit(TokenType::DOT,      ".",  line, sc); break;
        case '(': emit(TokenType::LPAREN,   "(",  line, sc); break;
        case ')': emit(TokenType::RPAREN,   ")",  line, sc); break;
        case '+':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::PLUS_EQUAL,  "+=", line, sc); }
            else                                                        emit(TokenType::PLUS,        "+",  line, sc);
            break;
        case '-':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::MINUS_EQUAL, "-=", line, sc); }
            else                                                        emit(TokenType::MINUS,       "-",  line, sc);
            break;
        case '*':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::MULTIPLY_EQUAL, "*=", line, sc); }
            else                                                        emit(TokenType::MULTIPLY,      "*",  line, sc);
            break;
        case '/':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::DIVIDE_EQUAL, "/=", line, sc); }
            // else: already handled as slash command above
            break;
        case '@':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::AT_EQUAL, "@=", line, sc); }
            else                                                        emit(TokenType::AT,       "@",  line, sc);
            break;
        case '<':
            if (pos < src.size() && src[pos] == '<') { pos++; col++; emit(TokenType::LSHIFT, "<<", line, sc); }
            else                                                        emit(TokenType::LESS,   "<",  line, sc);
            break;
        case '>': emit(TokenType::GREATER, ">", line, sc); break;
        case '#':
            if (pos < src.size() && src[pos] == '=') { pos++; col++; emit(TokenType::NOT_EQUAL,      "#=", line, sc); }
            else if (pos < src.size() && src[pos] == '>') { pos++; col++; emit(TokenType::LESS_EQUAL, "#>", line, sc); }
            else if (pos < src.size() && src[pos] == '<') { pos++; col++; emit(TokenType::GREATER_EQUAL, "#<", line, sc); }
            else                                                             emit(TokenType::HASH,        "#",  line, sc);
            break;
        default:
            break; // skip unknown chars
        }
    }

    // Emit remaining DEDENTs
    while (indentStack.size() > 1) {
        indentStack.pop_back();
        tokens.emplace_back(TokenType::DEDENT, "", line, col);
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "", line, col);
    return tokens;
}
