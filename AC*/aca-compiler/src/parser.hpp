#pragma once
#include "ast.hpp"
#include "token.hpp"
#include <string>
#include <vector>

namespace aca {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> toks;
    size_t p = 0;

    const Token& cur() const;
    const Token& peek(size_t off = 1) const;
    bool at(TokenType t) const;
    const Token& adv();
    [[noreturn]] void errHere(const std::string& msg) const;

    void skipNewlines();
    void skipLayout();

    FuncDef parseFuncDef();
    StmtPtr parseStmt();
    StmtPtr parseIfStmt();

    ExprPtr parseExpr(int minPrec = 0);
    int precedenceOf(std::string& opOut) const;
    ExprPtr parsePrimary();
    ExprPtr parsePostfix(ExprPtr base);
};

} // namespace aca

