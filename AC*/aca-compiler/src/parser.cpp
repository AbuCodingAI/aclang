#include "parser.hpp"
#include "types.hpp"
#include <stdexcept>

namespace aca {

Parser::Parser(std::vector<Token> tokens) : toks(std::move(tokens)) {}

const Token& Parser::cur() const { return toks[p]; }
const Token& Parser::peek(size_t off) const {
    size_t idx = p + off;
    if (idx >= toks.size()) return toks.back();
    return toks[idx];
}
bool Parser::at(TokenType t) const { return cur().type == t; }
const Token& Parser::adv() { return toks[p++]; }

[[noreturn]] void Parser::errHere(const std::string& msg) const {
    throw std::runtime_error("Line " + std::to_string(cur().line) + ":" + std::to_string(cur().col) + " " + msg);
}

void Parser::skipNewlines() {
    while (at(TokenType::NEWLINE)) adv();
}

void Parser::skipLayout() {
    while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT) || at(TokenType::HEADER)) adv();
}

Program Parser::parse() {
    Program prog;
    while (!at(TokenType::END)) {
        if (at(TokenType::HEADER)) {
            prog.header = cur().lexeme;
            adv();
            continue;
        }
        skipLayout();
        if (at(TokenType::END)) break;

        if (at(TokenType::KW_MAKE)) {
            prog.funcs.push_back(parseFuncDef());
            continue;
        }

        if (at(TokenType::TAG) && cur().lexeme == "<mainloop>") {
            adv();
            skipLayout();
            while (!at(TokenType::END)) {
                skipLayout();
                if (at(TokenType::TAG) && cur().lexeme == "<mainloop>") {
                    adv();
                    break;
                }
                if (at(TokenType::END)) break;
                prog.mainloop.push_back(parseStmt());
            }
            continue;
        }

        prog.mainloop.push_back(parseStmt());
    }
    return prog;
}

FuncDef Parser::parseFuncDef() {
    Token mk = adv(); // Make
    if (!at(TokenType::IDENT)) errHere("Expected function name after Make");
    Token name = adv();
    if (!at(TokenType::KW_FUNC)) errHere("Expected func(...) after function name");
    adv();
    if (!at(TokenType::LPAREN)) errHere("Expected '(' after func");
    adv();
    std::vector<std::string> params;
    skipNewlines();
    if (!at(TokenType::RPAREN)) {
        while (true) {
            if (!at(TokenType::IDENT)) errHere("Expected parameter name");
            params.push_back(adv().lexeme);
            skipNewlines();
            if (at(TokenType::COMMA)) {
                adv();
                skipNewlines();
                continue;
            }
            break;
        }
    }
    if (!at(TokenType::RPAREN)) errHere("Expected ')'");
    adv();

    skipNewlines();
    if (!at(TokenType::INDENT)) errHere("Expected indentation for function body");
    adv();
    std::vector<StmtPtr> body;
    while (!at(TokenType::END) && !at(TokenType::DEDENT)) {
        skipNewlines();
        if (at(TokenType::DEDENT) || at(TokenType::END)) break;
        body.push_back(parseStmt());
    }
    if (at(TokenType::DEDENT)) adv();

    FuncDef f;
    f.name = name.lexeme;
    f.params = std::move(params);
    f.body = std::move(body);
    f.line = mk.line;
    f.col = mk.col;
    return f;
}

StmtPtr Parser::parseStmt() {
    skipNewlines();

    if (at(TokenType::KW_RETURN)) {
        Token r = adv();
        auto e = parseExpr();
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Return;
        s->line = r.line;
        s->col = r.col;
        s->data = ReturnPayload{std::move(e)};
        return s;
    }

    if (at(TokenType::KW_IF)) return parseIfStmt();

    if (at(TokenType::IDENT) &&
        (peek().type == TokenType::EQ || peek().type == TokenType::PLUS_EQ || peek().type == TokenType::MINUS_EQ ||
         peek().type == TokenType::AT_EQ)) {
        Token name = adv();
        Token op = adv();
        auto rhs = parseExpr();
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->line = name.line;
        s->col = name.col;
        s->data = AssignPayload{name.lexeme, op.lexeme, std::move(rhs)};
        return s;
    }

    auto e = parseExpr();
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ExprOnly;
    s->line = e->line;
    s->col = e->col;
    s->data = std::move(e);
    return s;
}

StmtPtr Parser::parseIfStmt() {
    Token kw = adv();
    auto cond = parseExpr();
    skipNewlines();
    if (!at(TokenType::INDENT)) errHere("Expected indentation after IF condition");
    adv();
    std::vector<StmtPtr> thenBody;
    while (!at(TokenType::END) && !at(TokenType::DEDENT)) {
        skipNewlines();
        if (at(TokenType::DEDENT) || at(TokenType::END)) break;
        thenBody.push_back(parseStmt());
    }
    if (at(TokenType::DEDENT)) adv();

    std::vector<StmtPtr> elseBody;
    skipNewlines();
    if (at(TokenType::KW_OTHER)) {
        adv();
        skipNewlines();
        if (!at(TokenType::INDENT)) errHere("Expected indentation after OTHER");
        adv();
        while (!at(TokenType::END) && !at(TokenType::DEDENT)) {
            skipNewlines();
            if (at(TokenType::DEDENT) || at(TokenType::END)) break;
            elseBody.push_back(parseStmt());
        }
        if (at(TokenType::DEDENT)) adv();
    }

    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::If;
    s->line = kw.line;
    s->col = kw.col;
    s->data = IfPayload{std::move(cond), std::move(thenBody), std::move(elseBody)};
    return s;
}

ExprPtr Parser::parseExpr(int minPrec) {
    auto lhs = parsePrimary();
    while (true) {
        std::string op;
        int prec = precedenceOf(op);
        if (prec < minPrec) break;
        Token optok = adv();
        op = optok.lexeme;
        auto rhs = parseExpr(prec + 1);
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Binary;
        e->line = optok.line;
        e->col = optok.col;
        e->data = std::make_tuple(op, std::move(lhs), std::move(rhs));
        lhs = std::move(e);
    }
    return lhs;
}

int Parser::precedenceOf(std::string& opOut) const {
    TokenType t = cur().type;
    if (t == TokenType::KW_IS || t == TokenType::NOT_EQ) {
        opOut = "is";
        return 5;
    }
    if (t == TokenType::LT) {
        opOut = "<";
        return 6;
    }
    if (t == TokenType::GT) {
        opOut = ">";
        return 6;
    }
    if (t == TokenType::NOT_GT) {
        opOut = "#>";
        return 6;
    }
    if (t == TokenType::NOT_LT) {
        opOut = "#<";
        return 6;
    }
    if (t == TokenType::PLUS) {
        opOut = "+";
        return 10;
    }
    if (t == TokenType::MINUS) {
        opOut = "-";
        return 10;
    }
    if (t == TokenType::KW_MOD) {
        opOut = "mod";
        return 20;
    }
    if (t == TokenType::STAR) {
        opOut = "*";
        return 20;
    }
    if (t == TokenType::SLASH) {
        opOut = "/";
        return 20;
    }
    if (t == TokenType::AT) {
        opOut = "@";
        return 30;
    }
    return -1;
}

ExprPtr Parser::parsePrimary() {
    while (at(TokenType::NEWLINE)) adv();
    const Token& t = cur();

    if (at(TokenType::INT_LIT)) {
        adv();
        LitValue v;
        v.t = Type::Numeral(NumeralSubtype::NumInt);
        v.v = (int64_t)std::stoll(t.lexeme);
        return Expr::lit(std::move(v), t.line, t.col);
    }
    if (at(TokenType::DEC_LIT)) {
        adv();
        LitValue v;
        v.t = Type::Numeral(NumeralSubtype::NumDec);
        v.v = decimalLiteralToMap(t.lexeme);
        return Expr::lit(std::move(v), t.line, t.col);
    }
    if (at(TokenType::STRING_LIT)) {
        adv();
        LitValue v;
        v.t = Type::String();
        v.v = t.lexeme;
        return Expr::lit(std::move(v), t.line, t.col);
    }
    if (at(TokenType::BOOL_LIT)) {
        adv();
        bool b = (t.lexeme == "True" || t.lexeme == "true" || t.lexeme == "TRUE");
        LitValue v;
        v.t = Type::Boolean(b ? BooleanSubtype::True : BooleanSubtype::False);
        v.v = b;
        return Expr::lit(std::move(v), t.line, t.col);
    }

    if (at(TokenType::IDENT)) {
        Token name = adv();
        std::string qname = name.lexeme;
        while (at(TokenType::DOT) && peek().type == TokenType::IDENT) {
            adv();
            qname += ".";
            qname += adv().lexeme;
        }

        // Juxtaposition call: `Term.display x` / `foo 123` / `print $hi$`
        // If the next token begins a primary expression on the same line, treat as a 1-arg call.
        if (cur().line == name.line && !at(TokenType::LPAREN)) {
            TokenType nt = cur().type;
            bool beginsPrimary =
                (nt == TokenType::IDENT || nt == TokenType::INT_LIT || nt == TokenType::DEC_LIT || nt == TokenType::STRING_LIT ||
                 nt == TokenType::BOOL_LIT || nt == TokenType::LBRACK);
            if (beginsPrimary) {
                std::vector<ExprPtr> args;
                args.push_back(parseExpr(100)); // bind tight: consume only the argument expression
                auto e = std::make_unique<Expr>();
                e->kind = ExprKind::Call;
                e->line = name.line;
                e->col = name.col;
                e->data = std::make_tuple(qname, std::move(args));
                return parsePostfix(std::move(e));
            }
        }

        if (at(TokenType::LPAREN)) {
            adv();
            std::vector<ExprPtr> args;
            while (at(TokenType::NEWLINE)) adv();
            if (!at(TokenType::RPAREN)) {
                while (true) {
                    args.push_back(parseExpr());
                    while (at(TokenType::NEWLINE)) adv();
                    if (at(TokenType::COMMA)) {
                        adv();
                        continue;
                    }
                    break;
                }
            }
            if (!at(TokenType::RPAREN)) errHere("Expected ')'");
            adv();
            auto e = std::make_unique<Expr>();
            e->kind = ExprKind::Call;
            e->line = name.line;
            e->col = name.col;
            e->data = std::make_tuple(qname, std::move(args));
            return parsePostfix(std::move(e));
        }

        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Var;
        e->line = name.line;
        e->col = name.col;
        e->data = qname;
        return parsePostfix(std::move(e));
    }

    if (at(TokenType::LBRACK)) {
        Token lb = adv();
        while (at(TokenType::NEWLINE)) adv();
        auto first = parseExpr();
        while (at(TokenType::NEWLINE)) adv();
        if (at(TokenType::ARROW_FAT)) {
            adv();
            auto second = parseExpr();
            while (at(TokenType::NEWLINE)) adv();
            if (!at(TokenType::RBRACK)) errHere("Expected ']'");
            adv();
            auto e = std::make_unique<Expr>();
            e->kind = ExprKind::Range;
            e->line = lb.line;
            e->col = lb.col;
            e->data = std::make_pair(std::move(first), std::move(second));
            return parsePostfix(std::move(e));
        }
        std::vector<ExprPtr> items;
        items.push_back(std::move(first));
        while (at(TokenType::COMMA)) {
            adv();
            items.push_back(parseExpr());
            while (at(TokenType::NEWLINE)) adv();
        }
        if (!at(TokenType::RBRACK)) errHere("Expected ']'");
        adv();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::List;
        e->line = lb.line;
        e->col = lb.col;
        e->data = std::move(items);
        return parsePostfix(std::move(e));
    }

    if (at(TokenType::LPAREN)) {
        adv();
        while (at(TokenType::NEWLINE)) adv();
        if (at(TokenType::IDENT) && peek().type == TokenType::RPAREN && peek(2).type == TokenType::ARROW_FAT) {
            Token param = adv();
            adv();
            adv();
            auto body = parseExpr();
            auto lam = std::make_unique<Expr>();
            lam->kind = ExprKind::Call;
            lam->line = t.line;
            lam->col = t.col;
            std::vector<ExprPtr> args;
            auto pexpr = std::make_unique<Expr>();
            pexpr->kind = ExprKind::Var;
            pexpr->line = param.line;
            pexpr->col = param.col;
            pexpr->data = param.lexeme;
            args.push_back(std::move(pexpr));
            args.push_back(std::move(body));
            lam->data = std::make_tuple(std::string("__lambda__"), std::move(args));
            return parsePostfix(std::move(lam));
        }

        auto inner = parseExpr();
        while (at(TokenType::NEWLINE)) adv();
        if (!at(TokenType::RPAREN)) errHere("Expected ')'");
        adv();
        return parsePostfix(std::move(inner));
    }

    errHere("Unexpected token in expression: '" + t.lexeme + "'");
}

ExprPtr Parser::parsePostfix(ExprPtr base) {
    while (true) {
        while (at(TokenType::NEWLINE)) adv();
        if (!at(TokenType::STREAM_TO)) break;
        Token st = adv();
        while (at(TokenType::NEWLINE)) adv();

        if (!(at(TokenType::PLUS) || at(TokenType::STAR))) errHere("Expected stream reducer after >>");
        Token red = adv();

        std::vector<ExprPtr> payload;
        while (at(TokenType::NEWLINE)) adv();
        if (at(TokenType::LPAREN)) {
            adv();
            while (at(TokenType::NEWLINE)) adv();
            if (!at(TokenType::RPAREN)) {
                while (true) {
                    payload.push_back(parseExpr());
                    while (at(TokenType::NEWLINE)) adv();
                    if (at(TokenType::COMMA)) {
                        adv();
                        continue;
                    }
                    break;
                }
            }
            if (!at(TokenType::RPAREN)) errHere("Expected ')'");
            adv();
            while (at(TokenType::RPAREN)) adv(); // tolerate +((x)=>...))
        }

        auto call = std::make_unique<Expr>();
        call->kind = ExprKind::Call;
        call->line = st.line;
        call->col = st.col;
        std::vector<ExprPtr> args;
        args.push_back(std::move(base));
        auto op = std::make_unique<Expr>();
        op->kind = ExprKind::Var;
        op->line = red.line;
        op->col = red.col;
        op->data = red.lexeme; // "+" or "*"
        args.push_back(std::move(op));
        for (auto& a : payload) args.push_back(std::move(a));
        call->data = std::make_tuple(std::string("__stream__"), std::move(args));
        base = std::move(call);
    }
    return base;
}

} // namespace aca
