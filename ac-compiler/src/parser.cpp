#include "../include/token.hpp"
#include "../include/ast.hpp"
#include <vector>
#include <stdexcept>
#include <unordered_map>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    // tag open/close tracking: name -> open count
    std::unordered_map<std::string, int> tagCount;

    Token& peek() { return tokens[pos]; }
    Token& advance() { return tokens[pos++]; }

    bool at(TokenType t) { return peek().type == t; }

    void skipNewlines() {
        while (at(TokenType::NEWLINE)) advance();
    }

    void skipAll() {
        while (at(TokenType::NEWLINE) || at(TokenType::INDENT) || at(TokenType::DEDENT))
            advance();
    }

    Token expect(TokenType t, const std::string& msg) {
        if (!at(t)) throw std::runtime_error(msg + " at line " + std::to_string(peek().line));
        return advance();
    }

public:
    explicit Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    NodePtr parse() {
        auto program = std::make_unique<ASTNode>(NodeType::Program);
        skipAll();
        while (!at(TokenType::END_OF_FILE)) {
            auto node = parseStatement();
            if (node) program->children.push_back(std::move(node));
            skipAll();
        }
        return program;
    }

private:
    NodePtr parseStatement() {
        skipAll();
        if (at(TokenType::END_OF_FILE)) return nullptr;

        // Backend declaration
        if (at(TokenType::BACKEND)) {
            auto t = advance();
            return std::make_unique<ASTNode>(NodeType::BackendDecl, t.value);
        }

        // Tag block
        if (at(TokenType::TAG_OPEN)) {
            // Foreign passthrough block
            if (peek().value.substr(0, 11) == "__foreign__") {
                std::string raw = advance().value.substr(11);
                return std::make_unique<ASTNode>(NodeType::ForeignBlock, raw);
            }
            return parseTagBlock();
        }

        // /kill
        if (at(TokenType::SLASH)) {
            auto t = advance();
            if (t.value == "kill") return std::make_unique<ASTNode>(NodeType::KillStmt);
            return nullptr;
        }

        // use X
        if (at(TokenType::KW_USE)) {
            advance();
            return parseUse();
        }

        // save as name
        if (at(TokenType::KW_SAVE)) {
            advance();
            expect(TokenType::KW_AS, "Expected 'as' after 'save'");
            auto name = expect(TokenType::IDENTIFIER, "Expected filename after 'save as'");
            std::string full = name.value;
            // handle file.ext
            if (at(TokenType::DOT)) {
                advance();
                auto ext = advance();
                full += "." + ext.value;
            }
            return std::make_unique<ASTNode>(NodeType::SaveStmt, full);
        }

        // IF condition
        if (at(TokenType::KW_IF)) {
            return parseIf();
        }

        // FOR item in list
        if (at(TokenType::KW_FOR)) {
            return parseFor();
        }

        // WHILST condition
        if (at(TokenType::KW_WHILST)) {
            return parseWhilst();
        }

        // ELSEIF condition
        if (at(TokenType::KW_ELSEIF)) {
            advance();
            std::string cond;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::STRING)) cond += "$" + advance().value + "$ ";
                else cond += advance().value + " ";
            }
            auto node = std::make_unique<ASTNode>(NodeType::ElseIfStmt, cond);
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // OTHER (else)
        if (at(TokenType::KW_OTHER)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::IfStmt, "OTHER");
            node->children.push_back(parseBlock());
            return node;
        }

        // raise ERR(msg)
        if (at(TokenType::KW_RAISE)) {
            advance();
            expect(TokenType::KW_ERR, "Expected ERR after raise");
            expect(TokenType::LPAREN, "Expected ( after ERR");
            std::string msg;
            if (at(TokenType::STRING)) msg = advance().value;
            expect(TokenType::RPAREN, "Expected ) after ERR message");
            return std::make_unique<ASTNode>(NodeType::RaiseStmt, msg);
        }

        // def tag <name>
        if (at(TokenType::KW_DEF)) {
            advance();
            expect(TokenType::KW_TAG, "Expected 'tag' after 'def'");
            auto tagName = expect(TokenType::TAG_OPEN, "Expected <tagname> after 'def tag'");
            auto node = std::make_unique<ASTNode>(NodeType::CustomTagDef, tagName.value);
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // Make X func(arg)
        if (at(TokenType::KW_MAKE)) {
            advance();
            return parseMake();
        }

        // configure event-listener
        if (at(TokenType::IDENTIFIER) && peek().value == "configure") {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::EventListener);
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // when many ...
        if (at(TokenType::KW_WHEN)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::WhenBlock);
            // collect condition tokens until newline
            std::string cond;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                cond += advance().value + " ";
            }
            node->value = cond;
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // display $string$ (screen output)
        if (at(TokenType::KW_DISPLAY)) {
            advance();
            std::string val;
            if (at(TokenType::STRING)) val = "$" + advance().value + "$";
            else if (at(TokenType::IDENTIFIER)) val = advance().value;
            return std::make_unique<ASTNode>(NodeType::DisplayStmt, val);
        }

        // return expr
        if (at(TokenType::KW_RETURN)) {
            advance();
            // if next is fn, it's a multiply expression
            if (at(TokenType::KW_FN)) {
                auto fnNode = parseFnExpr();
                auto ret = std::make_unique<ASTNode>(NodeType::ReturnStmt, fnNode->value);
                return ret;
            }
            std::string val;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::STRING)) val += "$" + advance().value + "$";
                else val += advance().value;
            }
            return std::make_unique<ASTNode>(NodeType::ReturnStmt, val);
        }

        // fn expr*(expr)fn  — multiply expression as a statement/value
        if (at(TokenType::KW_FN)) {
            return parseFnExpr();
        }

        // list literal [$...$]
        if (at(TokenType::LBRACKET)) {
            return parseListLiteral();
        }

        // tuple literal {$...$}
        if (at(TokenType::LBRACE)) {
            return parseTupleLiteral();
        }

        // Identifier-based statements: Obj.X, X.prop = val, X.method(args), X = Y
        if (at(TokenType::IDENTIFIER)) {
            return parseIdentifierStatement();
        }

        // Skip unknown tokens
        advance();
        return nullptr;
    }

    NodePtr parseTagBlock() {
        auto tagTok = advance(); // consume <tagname>
        std::string name = tagTok.value;

        tagCount[name]++;
        bool isClose = (tagCount[name] % 2 == 0);

        if (isClose) {
            // This is a closing tag, return a sentinel
            auto node = std::make_unique<ASTNode>(NodeType::Block, "__close__" + name);
            return node;
        }

        // Opening tag — parse body until matching close OR EndHere if StartHere
        auto block = std::make_unique<ASTNode>(NodeType::TagBlock, name);
        skipAll();

        std::string closeWith = (name == "StartHere") ? "EndHere" : name;

        while (!at(TokenType::END_OF_FILE)) {
            if (at(TokenType::TAG_OPEN) && tokens[pos].value == closeWith) {
                if (name == "StartHere") {
                    advance(); // consume EndHere
                    break;
                }
                int nextCount = tagCount[name] + 1;
                if (nextCount % 2 == 0) {
                    tagCount[name]++;
                    advance();
                    break;
                }
            }
            auto stmt = parseStatement();
            if (stmt) {
                if (stmt->type == NodeType::Block &&
                    stmt->value == "__close__" + name) break;
                block->children.push_back(std::move(stmt));
            }
            skipAll();
        }
        return block;
    }

    NodePtr parseIf() {
        advance(); // consume IF
        std::string cond;
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::STRING))
                cond += "$" + advance().value + "$ ";
            else
                cond += advance().value + " ";
        }
        auto node = std::make_unique<ASTNode>(NodeType::IfStmt, cond);
        skipNewlines();
        node->children.push_back(parseBlock());
        return node;
    }

    NodePtr parseFor() {
        advance(); // consume FOR
        // FOR item in list  ->  value = "item", attrs[0] = "list"
        std::string item;
        if (at(TokenType::IDENTIFIER)) item = advance().value;
        if (at(TokenType::KW_IN)) advance(); // consume 'in'
        std::string list;
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::STRING)) list += "$" + advance().value + "$";
            else list += advance().value;
        }
        auto node = std::make_unique<ASTNode>(NodeType::ForLoop, item);
        node->attrs.push_back(list);
        skipNewlines();
        node->children.push_back(parseBlock());
        return node;
    }

    NodePtr parseWhilst() {
        advance(); // consume WHILST
        std::string cond;
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::STRING)) cond += "$" + advance().value + "$ ";
            else cond += advance().value + " ";
        }
        auto node = std::make_unique<ASTNode>(NodeType::WhilstLoop, cond);
        skipNewlines();
        node->children.push_back(parseBlock());
        return node;
    }

    // fn expr*(expr) — fn applies to the whole line, no closing fn needed
    NodePtr parseFnExpr() {
        advance(); // consume fn
        std::string expr;
        while (!at(TokenType::END_OF_FILE) && !at(TokenType::NEWLINE)) {
            expr += advance().value;
        }
        return std::make_unique<ASTNode>(NodeType::BinaryExpr, expr);
    }

    // [$item1, item2$]  — contents between [ and ] as a comma-split string
    NodePtr parseListLiteral() {
        advance(); // consume [
        std::string contents;
        while (!at(TokenType::RBRACKET) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::STRING)) contents += advance().value;
            else contents += advance().value;
        }
        if (at(TokenType::RBRACKET)) advance();
        auto node = std::make_unique<ASTNode>(NodeType::ListLiteral, contents);
        return node;
    }

    // {$item1, item2$}
    NodePtr parseTupleLiteral() {
        advance(); // consume {
        std::string contents;
        while (!at(TokenType::RBRACE) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::STRING)) contents += advance().value;
            else contents += advance().value;
        }
        if (at(TokenType::RBRACE)) advance();
        auto node = std::make_unique<ASTNode>(NodeType::TupleLiteral, contents);
        return node;
    }

    NodePtr parseBlock() {
        auto block = std::make_unique<ASTNode>(NodeType::Block);
        skipNewlines(); // skip newlines but NOT INDENT
        if (at(TokenType::INDENT)) {
            advance(); // consume INDENT
            while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                auto stmt = parseStatement();
                if (stmt) block->children.push_back(std::move(stmt));
                skipNewlines();
            }
            if (at(TokenType::DEDENT)) advance();
        }
        return block;
    }

    NodePtr parseUse() {
        std::string target;
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            target += advance().value + " ";
        }
        return std::make_unique<ASTNode>(NodeType::UseStmt, target);
    }

    NodePtr parseMake() {
        // Make funcname func(arg) or Make Screen object
        std::string name = expect(TokenType::IDENTIFIER, "Expected name after Make").value;

        if (at(TokenType::KW_FUNC)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after func");
            std::string arg;
            if (at(TokenType::IDENTIFIER)) arg = advance().value;
            expect(TokenType::RPAREN, "Expected )");
            auto node = std::make_unique<ASTNode>(NodeType::FuncDef, name);
            node->attrs.push_back(arg);
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // Make X object / Make Screen object
        auto node = std::make_unique<ASTNode>(NodeType::ObjDecl, name);
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
        return node;
    }

    NodePtr parseIdentifierStatement() {
        std::string name = advance().value;

        // Obj.Name
        if (at(TokenType::DOT)) {
            advance();
            std::string prop = advance().value; // property or method name

            // chained: Name.prop.method
            while (at(TokenType::DOT)) {
                advance();
                prop += "." + advance().value;
            }

            // method call: Name.method(args)
            if (at(TokenType::LPAREN)) {
                advance();
                std::string args;
                int depth = 1;
                while (!at(TokenType::END_OF_FILE) && depth > 0) {
                    if (at(TokenType::LPAREN)) depth++;
                    if (at(TokenType::RPAREN)) { depth--; if (depth == 0) break; }
                    args += advance().value;
                }
                if (at(TokenType::RPAREN)) advance();
                auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
                node->attrs.push_back(args);
                return node;
            }

            // no-paren string call: Name.method $string$ [extra tokens...]
            if (at(TokenType::STRING)) {
                std::string strVal = advance().value;
                auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
                node->attrs.push_back("$" + strVal + "$");
                // collect any trailing tokens on the same line (e.g. "Hierarchy 3")
                std::string extra;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                    extra += advance().value + " ";
                if (!extra.empty()) node->attrs.push_back(extra);
                return node;
            }

            // config: Name.config key=val ...
            if (prop == "config") {
                auto node = std::make_unique<ASTNode>(NodeType::ConfigCall, name);
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    std::string key = advance().value;
                    if (at(TokenType::ASSIGN)) { advance(); }
                    std::string val;
                    while (!at(TokenType::NEWLINE) && !at(TokenType::COMMA) && !at(TokenType::END_OF_FILE)) {
                        val += advance().value;
                    }
                    node->attrs.push_back(key + "=" + val);
                }
                return node;
            }

            // assignment: Name.prop = value
            if (at(TokenType::ASSIGN)) {
                advance();
                std::string val;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    val += advance().value + " ";
                }
                auto node = std::make_unique<ASTNode>(NodeType::PropAssign, name + "." + prop);
                node->attrs.push_back(val);
                return node;
            }

            // bare property access / call without parens
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
            return node;
        }

        // Simple assignment: name = value
        if (at(TokenType::ASSIGN)) {
            advance();
            // list literal
            if (at(TokenType::LBRACKET)) {
                auto listNode = parseListLiteral();
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__list__" + listNode->value);
                return node;
            }
            // tuple literal
            if (at(TokenType::LBRACE)) {
                auto tupleNode = parseTupleLiteral();
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__tuple__" + tupleNode->value);
                return node;
            }
            // fn multiply expr
            if (at(TokenType::KW_FN)) {
                auto fnNode = parseFnExpr();
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__fn__" + fnNode->value);
                return node;
            }
            std::string val;
            if (at(TokenType::STRING)) { val = "$" + advance().value + "$"; }
            else {
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                    val += advance().value;
            }
            auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
            node->attrs.push_back(val);
            return node;
        }

        // Bare identifier (SpawnTerrain, etc.)
        auto node = std::make_unique<ASTNode>(NodeType::SpawnStmt, name);
        // consume rest of line as args
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
            node->attrs.push_back(advance().value);
        }
        return node;
    }
};

NodePtr parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse();
}
