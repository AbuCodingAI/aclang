#include "../include/token.hpp"
#include "../include/ast.hpp"
#include <vector>
#include <stdexcept>
#include <string>

// ── Helpers ────────────────────────────────────────────────────────────────

struct Parser {
    const std::vector<Token>& toks;
    size_t pos = 0;

    explicit Parser(const std::vector<Token>& t) : toks(t) {}

    const Token& peek(int offset = 0) const {
        size_t i = pos + offset;
        if (i >= toks.size()) return toks.back();
        return toks[i];
    }
    bool at(TokenType t, int offset = 0) const { return peek(offset).type == t; }
    Token advance() {
        Token t = peek();
        if (pos < toks.size()) pos++;
        return t;
    }
    Token expect(TokenType t, const char* msg) {
        if (!at(t)) throw std::runtime_error(
            std::string("ParseError at line ") + std::to_string(peek().line) +
            ": expected " + msg + ", got '" + peek().value + "'");
        return advance();
    }

    void skipNewlines() {
        while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
            advance();
    }
    void skipLine() {
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
        if (at(TokenType::NEWLINE)) advance();
    }

    // ── Register name (e.g. "64box1"): NUMBER token followed by IDENTIFIER ─
    // Returns the full register name or empty if not a reg name
    std::string tryRegName() {
        if (at(TokenType::NUMBER) && at(TokenType::IDENTIFIER, 1)) {
            std::string bits = peek().value;
            if (bits == "64" || bits == "32" || bits == "16" || bits == "8") {
                std::string name = bits + peek(1).value;
                advance(); advance();
                return name;
            }
        }
        return "";
    }

    // ── Expression ─────────────────────────────────────────────────────────
    NodePtr parseExpr(int minPrec = 0);
    NodePtr parsePrimary();
    NodePtr parseMemTarget();  // addr expr usable as lhs of <<

    // ── Statements ─────────────────────────────────────────────────────────
    NodePtr parseStmt();
    NodePtr parseBlock();
    NodePtr parseIfStmt();
    NodePtr parseWhilstStmt();
    NodePtr parseForStmt();
    NodePtr parseFuncDef();
    NodePtr parseUse();

    NodePtr parse() {
        auto prog = std::make_unique<ASTNode>(NodeType::Program, "", 1);
        skipNewlines();
        while (!at(TokenType::END_OF_FILE)) {
            // AC+ LIB header
            if (at(TokenType::BACKEND)) {
                auto n = std::make_unique<ASTNode>(NodeType::BackendDecl, advance().value);
                prog->children.push_back(std::move(n));
                skipNewlines();
                continue;
            }
            // flib imports
            if (at(TokenType::KW_USE)) {
                prog->children.push_back(parseUse());
                skipNewlines();
                continue;
            }
            // Function definitions
            if (at(TokenType::KW_MAKE)) {
                prog->children.push_back(parseFuncDef());
                skipNewlines();
                continue;
            }
            // Main loop entry
            if (at(TokenType::TAG) && peek().value == "mainloop") {
                advance();
                skipNewlines();
                auto ml = std::make_unique<ASTNode>(NodeType::MainLoop, "mainloop");
                auto body = parseBlock();
                ml->children.push_back(std::move(body));
                // consume closing <mainloop>
                if (at(TokenType::TAG) && peek().value == "mainloop") advance();
                prog->children.push_back(std::move(ml));
                skipNewlines();
                continue;
            }
            skipNewlines();
            if (!at(TokenType::END_OF_FILE)) advance(); // skip unknown
        }
        return prog;
    }
};

// ── Primary expressions ────────────────────────────────────────────────────

NodePtr Parser::parsePrimary() {
    int ln = peek().line;

    // nil
    if (at(TokenType::KW_NIL)) {
        advance();
        return std::make_unique<ASTNode>(NodeType::NilLit, "nil", ln);
    }

    // locate varname
    if (at(TokenType::KW_LOCATE)) {
        advance();
        std::string name = expect(TokenType::IDENTIFIER, "variable name after locate").value;
        auto n = std::make_unique<ASTNode>(NodeType::LocateExpr, name, ln);
        return n;
    }

    // Register name: 64box1, 32vault, etc.
    std::string reg = tryRegName();
    if (!reg.empty()) {
        // Could be a function call? Unlikely for registers.
        // Could be part of addr + offset << value — handled in parseMemTarget
        return std::make_unique<ASTNode>(NodeType::IdentExpr, reg, ln);
    }

    // Number
    if (at(TokenType::NUMBER)) {
        std::string v = advance().value;
        return std::make_unique<ASTNode>(NodeType::NumberLit, v, ln);
    }

    // String
    if (at(TokenType::STRING)) {
        std::string v = advance().value;
        return std::make_unique<ASTNode>(NodeType::StringLit, v, ln);
    }

    // Identifier (possibly a function call)
    if (at(TokenType::IDENTIFIER)) {
        std::string name = advance().value;
        if (at(TokenType::LPAREN)) {
            // Function call
            advance();
            auto call = std::make_unique<ASTNode>(NodeType::CallExpr, name, ln);
            while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                call->children.push_back(parseExpr(0));
                if (at(TokenType::COMMA)) advance();
            }
            if (at(TokenType::RPAREN)) advance();
            return call;
        }
        return std::make_unique<ASTNode>(NodeType::IdentExpr, name, ln);
    }

    // Parenthesised expression
    if (at(TokenType::LPAREN)) {
        advance();
        auto e = parseExpr(0);
        if (at(TokenType::RPAREN)) advance();
        return e;
    }

    // Hash = NOT prefix
    if (at(TokenType::HASH)) {
        advance();
        auto operand = parsePrimary();
        auto n = std::make_unique<ASTNode>(NodeType::UnaryExpr, "#", ln);
        n->children.push_back(std::move(operand));
        return n;
    }

    // Unary minus
    if (at(TokenType::MINUS)) {
        advance();
        auto operand = parsePrimary();
        auto n = std::make_unique<ASTNode>(NodeType::UnaryExpr, "-", ln);
        n->children.push_back(std::move(operand));
        return n;
    }

    return nullptr;
}

static int opPrec(TokenType t) {
    switch (t) {
    case TokenType::IS: case TokenType::NOT_EQUAL:
    case TokenType::LESS: case TokenType::GREATER:
    case TokenType::LESS_EQUAL: case TokenType::GREATER_EQUAL: return 2;
    case TokenType::PLUS: case TokenType::MINUS: return 4;
    case TokenType::MULTIPLY: case TokenType::DIVIDE:
    case TokenType::AT: return 6;
    default: return -1;
    }
}

NodePtr Parser::parseExpr(int minPrec) {
    auto lhs = parsePrimary();
    if (!lhs) return nullptr;

    while (true) {
        int prec = opPrec(peek().type);
        if (prec < minPrec) break;
        std::string op = advance().value;
        auto rhs = parseExpr(prec + 1);
        if (!rhs) break;
        auto bin = std::make_unique<ASTNode>(NodeType::BinaryExpr, op, lhs->line);
        bin->children.push_back(std::move(lhs));
        bin->children.push_back(std::move(rhs));
        lhs = std::move(bin);
    }
    return lhs;
}

// ── Block parsing ─────────────────────────────────────────────────────────

NodePtr Parser::parseBlock() {
    auto block = std::make_unique<ASTNode>(NodeType::Block, "");
    skipNewlines();
    if (at(TokenType::INDENT)) advance();
    while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
        if (at(TokenType::NEWLINE)) { advance(); continue; }
        auto stmt = parseStmt();
        if (stmt) block->children.push_back(std::move(stmt));
    }
    if (at(TokenType::DEDENT)) advance();
    return block;
}

// ── Statement parsing ─────────────────────────────────────────────────────

NodePtr Parser::parseIfStmt() {
    // IF cond\n body [ELSEIF cond body]* [OTHER body]
    advance(); // consume IF
    auto node = std::make_unique<ASTNode>(NodeType::IfStmt, "if", peek().line);

    // condition
    auto cond = parseExpr(0);
    node->children.push_back(std::move(cond));
    skipNewlines();
    node->children.push_back(parseBlock());

    while (at(TokenType::KW_ELSEIF)) {
        advance();
        auto eiCond = parseExpr(0);
        auto elseif = std::make_unique<ASTNode>(NodeType::IfStmt, "elseif");
        elseif->children.push_back(std::move(eiCond));
        skipNewlines();
        elseif->children.push_back(parseBlock());
        node->children.push_back(std::move(elseif));
    }
    if (at(TokenType::KW_OTHER)) {
        advance();
        skipNewlines();
        auto other = std::make_unique<ASTNode>(NodeType::Block, "other");
        other->children.push_back(parseBlock());
        node->children.push_back(std::move(other));
    }
    return node;
}

NodePtr Parser::parseWhilstStmt() {
    advance(); // consume WHILST
    auto node = std::make_unique<ASTNode>(NodeType::WhilstStmt, "whilst", peek().line);
    node->children.push_back(parseExpr(0));
    skipNewlines();
    node->children.push_back(parseBlock());
    return node;
}

NodePtr Parser::parseForStmt() {
    advance(); // FOR
    std::string var = expect(TokenType::IDENTIFIER, "loop variable").value;
    expect(TokenType::KW_IN, "in");
    auto node = std::make_unique<ASTNode>(NodeType::ForStmt, var, peek().line);
    if (at(TokenType::KW_RANGE)) {
        advance();
        node->attrs.push_back("range");
        node->children.push_back(parseExpr(0));
    } else {
        node->children.push_back(parseExpr(0));
    }
    skipNewlines();
    node->children.push_back(parseBlock());
    return node;
}

NodePtr Parser::parseFuncDef() {
    advance(); // Make
    std::string name = expect(TokenType::IDENTIFIER, "function name").value;
    expect(TokenType::KW_FUNC, "func");
    expect(TokenType::LPAREN, "(");
    auto node = std::make_unique<ASTNode>(NodeType::FuncDef, name, peek().line);
    while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
        if (at(TokenType::IDENTIFIER))
            node->attrs.push_back(advance().value);
        else if (at(TokenType::COMMA))
            advance();
        else
            break;
    }
    expect(TokenType::RPAREN, ")");
    skipNewlines();
    node->children.push_back(parseBlock());
    return node;
}

NodePtr Parser::parseUse() {
    advance(); // use
    if (at(TokenType::KW_FLIB)) {
        advance();
        std::string path;
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            auto& t = peek();
            if (t.type == TokenType::DOT) { path += "."; advance(); }
            else                          { path += t.value; advance(); }
        }
        // Trim
        auto ns = path.find_first_not_of(' ');
        if (ns != std::string::npos) path = path.substr(ns);
        auto ne = path.find_last_not_of(' ');
        if (ne != std::string::npos) path = path.substr(0, ne + 1);
        auto n = std::make_unique<ASTNode>(NodeType::UseFlibStmt, path);
        skipLine();
        return n;
    }
    skipLine();
    return nullptr;
}

NodePtr Parser::parseStmt() {
    int ln = peek().line;

    // /kill /stop /end
    if (at(TokenType::SLASH_CMD)) {
        std::string cmd = advance().value;
        skipLine();
        return std::make_unique<ASTNode>(NodeType::HaltStmt, cmd, ln);
    }

    // return expr
    if (at(TokenType::KW_RETURN)) {
        advance();
        auto node = std::make_unique<ASTNode>(NodeType::ReturnStmt, "return", ln);
        if (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
            node->children.push_back(parseExpr(0));
        skipLine();
        return node;
    }

    // IF
    if (at(TokenType::KW_IF)) return parseIfStmt();

    // WHILST
    if (at(TokenType::KW_WHILST)) return parseWhilstStmt();

    // FOR
    if (at(TokenType::KW_FOR)) return parseForStmt();

    // Make (nested function)
    if (at(TokenType::KW_MAKE)) return parseFuncDef();

    // reg 64box1 = expr
    if (at(TokenType::KW_REG)) {
        advance();
        std::string regName = tryRegName();
        if (regName.empty() && at(TokenType::IDENTIFIER))
            regName = advance().value;
        auto node = std::make_unique<ASTNode>(NodeType::RegDecl, regName, ln);
        if (at(TokenType::ASSIGN)) {
            advance();
            node->children.push_back(parseExpr(0));
        }
        skipLine();
        return node;
    }

    // stack varname type
    if (at(TokenType::KW_STACK)) {
        advance();
        std::string varname;
        if (at(TokenType::IDENTIFIER)) varname = advance().value;
        std::string type;
        if (at(TokenType::IDENTIFIER)) type = advance().value;
        auto node = std::make_unique<ASTNode>(NodeType::StackDecl, varname, ln);
        node->attrs.push_back(type);
        skipLine();
        return node;
    }

    // Register name (64box1, 32vault) as statement LHS
    {
        std::string reg = tryRegName();
        if (!reg.empty()) {
            // Possible: compound assign, memwrite, binary lhs
            //   64box1 += offset
            //   64box1 << value
            //   64box1 + offset << value  (OffsetExpr)

            // Check for << (memory write) with possible + offset
            if (at(TokenType::PLUS)) {
                // addr + offset << value
                advance();
                auto offset = parseExpr(6); // higher prec
                if (at(TokenType::LSHIFT)) {
                    advance();
                    auto value = parseExpr(0);
                    auto mw = std::make_unique<ASTNode>(NodeType::MemWrite, reg, ln);
                    mw->attrs.push_back("offset");
                    mw->children.push_back(std::move(offset));
                    mw->children.push_back(std::move(value));
                    skipLine();
                    return mw;
                }
            }
            if (at(TokenType::LSHIFT)) {
                advance();
                auto value = parseExpr(0);
                auto mw = std::make_unique<ASTNode>(NodeType::MemWrite, reg, ln);
                mw->children.push_back(std::move(value));
                skipLine();
                return mw;
            }
            // Compound assignment
            TokenType ct = peek().type;
            if (ct == TokenType::PLUS_EQUAL  || ct == TokenType::MINUS_EQUAL ||
                ct == TokenType::MULTIPLY_EQUAL || ct == TokenType::DIVIDE_EQUAL ||
                ct == TokenType::AT_EQUAL) {
                std::string op = advance().value;
                auto rhs = parseExpr(0);
                auto node = std::make_unique<ASTNode>(NodeType::CompoundAssign, reg, ln);
                node->attrs.push_back(op);
                node->children.push_back(std::move(rhs));
                skipLine();
                return node;
            }
            // Plain assign
            if (at(TokenType::ASSIGN)) {
                advance();
                auto rhs = parseExpr(0);
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, reg, ln);
                node->children.push_back(std::move(rhs));
                skipLine();
                return node;
            }
            // Just reg expr as statement (unusual, skip)
            skipLine();
            return nullptr;
        }
    }

    // Plain identifier
    if (at(TokenType::IDENTIFIER)) {
        std::string name = advance().value;
        int sline = ln;

        // function call as statement: name(args)
        if (at(TokenType::LPAREN)) {
            advance();
            auto call = std::make_unique<ASTNode>(NodeType::CallExpr, name, sline);
            while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                call->children.push_back(parseExpr(0));
                if (at(TokenType::COMMA)) advance();
            }
            if (at(TokenType::RPAREN)) advance();
            skipLine();
            return call;
        }

        // assignment
        if (at(TokenType::ASSIGN)) {
            advance();
            auto rhs = parseExpr(0);
            auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name, sline);
            node->children.push_back(std::move(rhs));
            skipLine();
            return node;
        }

        // compound assign
        TokenType ct = peek().type;
        if (ct == TokenType::PLUS_EQUAL  || ct == TokenType::MINUS_EQUAL ||
            ct == TokenType::MULTIPLY_EQUAL || ct == TokenType::DIVIDE_EQUAL) {
            std::string op = advance().value;
            auto rhs = parseExpr(0);
            auto node = std::make_unique<ASTNode>(NodeType::CompoundAssign, name, sline);
            node->attrs.push_back(op);
            node->children.push_back(std::move(rhs));
            skipLine();
            return node;
        }

        skipLine();
        return nullptr;
    }

    advance(); // skip unknown token
    return nullptr;
}

// ── Public API ────────────────────────────────────────────────────────────

std::vector<Token>  lex(const std::string& src);   // defined in lexer.cpp

NodePtr parseACP(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse();
}
