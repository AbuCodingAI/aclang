#include "../include/token.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>
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
    {"display",  TokenType::KW_DISPLAY},
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
    {"of",       TokenType::KW_OF},
    {"type",     TokenType::KW_TYPE},
    {"Overlap",  TokenType::KW_OVERLAP},
    {"overlap",  TokenType::KW_OVERLAP},
    {"True",     TokenType::KW_TRUE},
    {"False",    TokenType::KW_FALSE},
    {"on",       TokenType::KW_ON},
    {"when",     TokenType::KW_WHEN},
    {"When",     TokenType::KW_WHEN},
    {"many",     TokenType::KW_MANY},
    {"temp",     TokenType::KW_TEMP},
    {"func",     TokenType::KW_FUNC},
    {"at",       TokenType::KW_AT},
};

class Lexer {
public:
    std::string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    explicit Lexer(std::string source) : src(std::move(source)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        std::vector<int> indentStack = {0};

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

            // Comments: *...* — only when * appears as standalone token (not inside fn expr)
            // We treat * as comment only when preceded by whitespace/newline
            if (src[pos] == '*') {
                // peek ahead: if there's a closing * before a newline, it's a comment
                size_t closePos = src.find('*', pos + 1);
                size_t nlPos = src.find('\n', pos + 1);
                if (closePos != std::string::npos && (nlPos == std::string::npos || closePos < nlPos)) {
                    pos++; col++;
                    while (pos < src.size() && src[pos] != '*') { pos++; col++; }
                    if (pos < src.size()) { pos++; col++; }
                    continue;
                }
                // otherwise it's a multiply star
                tokens.emplace_back(TokenType::MULTIPLY, "*", line, col);
                pos++; col++;
                continue;
            }

            // String: $...$
            if (src[pos] == '$') {
                int sc = col; pos++; col++;
                std::string s;
                while (pos < src.size() && src[pos] != '$') { s += src[pos++]; col++; }
                if (pos < src.size()) { pos++; col++; }
                tokens.emplace_back(TokenType::STRING, s, line, sc);
                continue;
            }

            // String: "..."
            if (src[pos] == '"') {
                int sc = col; pos++; col++;
                std::string s;
                while (pos < src.size() && src[pos] != '"') { s += src[pos++]; col++; }
                if (pos < src.size()) { pos++; col++; }
                tokens.emplace_back(TokenType::STRING, s, line, sc);
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

            // Backend: AC->XX
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

            // /kill
            if (src[pos] == '/') {
                int sc = col; pos++; col++;
                std::string word;
                while (pos < src.size() && std::isalpha(src[pos])) { word += src[pos++]; col++; }
                tokens.emplace_back(TokenType::SLASH, word, line, sc);
                continue;
            }

            // #= (not equal)
            if (src[pos] == '#' && pos+1 < src.size() && src[pos+1] == '=') {
                tokens.emplace_back(TokenType::NOT_EQUAL, "#=", line, col);
                pos += 2; col += 2;
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
                while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_')) {
                    word += src[pos++]; col++;
                }
                auto it = KEYWORDS.find(word);
                if (it != KEYWORDS.end()) {
                    tokens.emplace_back(it->second, word, line, sc);
                } else {
                    tokens.emplace_back(TokenType::IDENTIFIER, word, line, sc);
                }
                continue;
            }

            // Single char tokens
            int sc = col;
            char c = src[pos++]; col++;
            switch (c) {
                case '=': tokens.emplace_back(TokenType::ASSIGN,    "=",  line, sc); break;
                case '.': tokens.emplace_back(TokenType::DOT,       ".",  line, sc); break;
                case '(': tokens.emplace_back(TokenType::LPAREN,    "(",  line, sc); break;
                case ')': tokens.emplace_back(TokenType::RPAREN,    ")",  line, sc); break;
                case ',': tokens.emplace_back(TokenType::COMMA,     ",",  line, sc); break;
                case '&': tokens.emplace_back(TokenType::AMPERSAND, "&",  line, sc); break;
                case '-': tokens.emplace_back(TokenType::IDENTIFIER,"-",  line, sc); break;
                case '^': tokens.emplace_back(TokenType::IDENTIFIER,"^",  line, sc); break;
                case '%': tokens.emplace_back(TokenType::IDENTIFIER,"%",  line, sc); break;
                case '<':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::LTE, "<=", line, sc); }
                    else tokens.emplace_back(TokenType::LT, "<", line, sc);
                    break;
                case '>':
                    if (pos < src.size() && src[pos] == '=') { pos++; col++; tokens.emplace_back(TokenType::GTE, ">=", line, sc); }
                    else tokens.emplace_back(TokenType::GT, ">", line, sc);
                    break;
                case '[': tokens.emplace_back(TokenType::LBRACKET,  "[",  line, sc); break;
                case ']': tokens.emplace_back(TokenType::RBRACKET,  "]",  line, sc); break;
                case '{': tokens.emplace_back(TokenType::LBRACE,    "{",  line, sc); break;
                case '}': tokens.emplace_back(TokenType::RBRACE,    "}",  line, sc); break;
                case '+': tokens.emplace_back(TokenType::IDENTIFIER,"+",  line, sc); break;
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
