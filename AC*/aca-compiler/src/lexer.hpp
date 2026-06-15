#pragma once
#include "token.hpp"
#include <string>
#include <vector>

namespace aca {

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> lex();

private:
    std::string src;
    size_t i = 0;
    int line = 1;
    int col = 1;

    bool eof(size_t lookahead = 0) const;
    char peek() const;
    char peek2() const;
    void adv();
    std::string loc() const;

    Token tok(TokenType t, std::string lexeme) const;
    static bool isIdentStart(char c);
    static bool isIdent(char c);
    static bool ieq(const std::string& a, const std::string& b);

    void lexBlockComment();
    Token lexNumber();
    Token lexIdentLike();
    Token lexTag();
};

} // namespace aca

