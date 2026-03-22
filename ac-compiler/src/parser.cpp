#include "../include/token.hpp"
#include "../include/ast.hpp"
#include "../include/tags.hpp"
#include "../include/error.hpp"
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    // tag open/close tracking: name -> open count
    std::unordered_map<std::string, int> tagCount;
    // custom tags defined by `def tag <name>`
    std::unordered_set<std::string> customTags;

    Token& peek() { return tokens[pos]; }

    bool isRecognizedTag(const std::string& name) const {
        return Tags::isKnownTag(name) || customTags.count(name) > 0;
    }
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
        if (!at(t)) throw SYNTAX_ERROR(msg, peek().line, peek().col);
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
            std::string tagName = peek().value;
            if (!isRecognizedTag(tagName)) {
                throw SEMANTIC_ERROR("Unknown tag <" + std::string(tagName) + ">", peek().line, peek().col);
            }
            return parseTagBlock();
        }

        // /kill
        if (at(TokenType::SLASH)) {
            auto t = advance();
            if (t.value == "kill") return std::make_unique<ASTNode>(NodeType::KillStmt);
            return nullptr;
        }

        // pass
        if (at(TokenType::KW_PASS)) {
            advance();
            return std::make_unique<ASTNode>(NodeType::PassStmt);
        }

        // skip
        if (at(TokenType::KW_SKIP)) {
            advance();
            return std::make_unique<ASTNode>(NodeType::SkipStmt);
        }

        // break
        if (at(TokenType::KW_BREAK)) {
            advance();
            return std::make_unique<ASTNode>(NodeType::BreakStmt);
        }

        // continue
        if (at(TokenType::KW_CONTINUE)) {
            advance();
            return std::make_unique<ASTNode>(NodeType::ContinueStmt);
        }

        // destroy x
        if (at(TokenType::KW_DESTROY)) {
            advance();
            std::string name;
            if (at(TokenType::IDENTIFIER)) name = advance().value;
            return std::make_unique<ASTNode>(NodeType::DestroyStmt, name);
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
            customTags.insert(tagName.value);
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
            std::string configTarget;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                configTarget += advance().value;
                if (!at(TokenType::NEWLINE)) configTarget += " ";
            }
            // trim whitespace
            while (!configTarget.empty() && configTarget.back() == ' ') configTarget.pop_back();
            auto node = std::make_unique<ASTNode>(NodeType::EventListener, configTarget);
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

        // on value=space / on value=escape etc
        if (at(TokenType::KW_ON)) {
            advance();
            std::string key;
            if (at(TokenType::IDENTIFIER) && peek().value == "value") {
                advance();
                if (at(TokenType::ASSIGN)) advance();
                if (at(TokenType::IDENTIFIER)) key = advance().value;
            }
            skipNewlines();
            auto node = std::make_unique<ASTNode>(NodeType::EventListener, key);
            if (at(TokenType::INDENT)) {
                node->children.push_back(parseBlock());
            } else {
                auto stmt = parseStatement();
                if (stmt) node->children.push_back(std::move(stmt));
            }
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

        // range N — Numeral Pos only
        if (at(TokenType::KW_RANGE)) {
            advance();
            std::string n;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                n += advance().value;
            auto node = std::make_unique<ASTNode>(NodeType::RangeExpr, n);
            node->inferredType = std::make_shared<Type>(Type::makeRange());
            return node;
        }

        // sequence(x, y)
        if (at(TokenType::KW_SEQUENCE)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after sequence");
            std::string x, y;
            while (!at(TokenType::COMMA) && !at(TokenType::END_OF_FILE)) x += advance().value;
            if (at(TokenType::COMMA)) advance();
            while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) y += advance().value;
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::SequenceExpr, x + "," + y);
            node->inferredType = std::make_shared<Type>(Type::makeSequence());
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

        // True / False boolean literals as standalone statements or assignments
        if (at(TokenType::KW_TRUE) || at(TokenType::KW_FALSE)) {
            // bare boolean — treat as expression
            std::string val = advance().value;
            return std::make_unique<ASTNode>(NodeType::BinaryExpr, val);
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
        if (at(TokenType::IDENTIFIER) || at(TokenType::KW_PROGRAM_LOOP)) {
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

        // Collect ELSEIF / OTHER chain
        while (true) {
            skipNewlines();
            if (at(TokenType::KW_ELSEIF)) {
                advance();
                std::string elseifCond;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::STRING)) elseifCond += "$" + advance().value + "$ ";
                    else elseifCond += advance().value + " ";
                }
                auto elseifNode = std::make_unique<ASTNode>(NodeType::ElseIfStmt, elseifCond);
                skipNewlines();
                elseifNode->children.push_back(parseBlock());
                node->children.push_back(std::move(elseifNode));
                continue;
            }
            if (at(TokenType::KW_OTHER)) {
                advance();
                auto otherNode = std::make_unique<ASTNode>(NodeType::IfStmt, "OTHER");
                skipNewlines();
                otherNode->children.push_back(parseBlock());
                node->children.push_back(std::move(otherNode));
                continue;
            }
            break;
        }

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
        // Collect else-if/other chains in the same level
        while (true) {
            skipNewlines();
            if (at(TokenType::KW_ELSEIF)) {
                advance();
                std::string cond2;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::STRING)) cond2 += "$" + advance().value + "$ ";
                    else cond2 += advance().value + " ";
                }
                auto elseifNode = std::make_unique<ASTNode>(NodeType::ElseIfStmt, cond2);
                skipNewlines();
                elseifNode->children.push_back(parseBlock());
                node->children.push_back(std::move(elseifNode));
                continue;
            }
            if (at(TokenType::KW_OTHER)) {
                advance();
                auto otherNode = std::make_unique<ASTNode>(NodeType::IfStmt, "OTHER");
                skipNewlines();
                otherNode->children.push_back(parseBlock());
                node->children.push_back(std::move(otherNode));
                continue;
            }
            break;
        }
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
        // use ilib <libname>  — import a built-in library
        if (at(TokenType::KW_ILIB)) {
            advance(); // consume ilib
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, libname);
        }
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
            // collect all args: func(a, b, c)
            std::vector<std::string> args;
            while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::IDENTIFIER)) args.push_back(advance().value);
                if (at(TokenType::COMMA)) advance();
            }
            expect(TokenType::RPAREN, "Expected )");
            auto node = std::make_unique<ASTNode>(NodeType::FuncDef, name);
            for (auto& a : args) node->attrs.push_back(a);
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

        // Function call without object prefix: jump(Character)
        if (at(TokenType::LPAREN)) {
            advance();
            std::string args;
            int depth = 1;
            while (!at(TokenType::END_OF_FILE) && depth > 0) {
                if (at(TokenType::LPAREN)) { depth++; args += advance().value; continue; }
                if (at(TokenType::RPAREN)) { depth--; if (depth == 0) break; args += advance().value; continue; }
                if (at(TokenType::STRING)) { args += "$" + advance().value + "$"; continue; }
                args += advance().value;
            }
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name);
            node->attrs.push_back(args);
            return node;
        }

        // Obj.Name
        if (at(TokenType::DOT)) {
            advance();
            std::string prop = advance().value; // property or method name

            // chained: Name.prop.method
            while (at(TokenType::DOT)) {
                advance();
                prop += "." + advance().value;
            }

            // Obj.Name as object declaration
            if (name == "Obj") {
                auto node = std::make_unique<ASTNode>(NodeType::ObjDecl, "Obj." + prop);
                return node;
            }

            // method call: Name.method(args)
            if (at(TokenType::LPAREN)) {
                advance();
                std::string args;
                int depth = 1;
                while (!at(TokenType::END_OF_FILE) && depth > 0) {
                    if (at(TokenType::LPAREN)) depth++;
                    if (at(TokenType::RPAREN)) { depth--; if (depth == 0) break; }
                    if (at(TokenType::STRING)) { args += "$" + advance().value + "$"; continue; }
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
            std::string tail;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                tail += advance().value;
                if (!at(TokenType::NEWLINE)) tail += " ";
            }
            if (!tail.empty()) node->attrs.push_back(tail);
            return node;
        }

        // Indexing / assignment: name[expr] = value or pure index expression
        if (at(TokenType::LBRACKET)) {
            advance();
            std::string indexExpr;
            while (!at(TokenType::RBRACKET) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::STRING)) indexExpr += "$" + advance().value + "$";
                else indexExpr += advance().value;
            }
            if (at(TokenType::RBRACKET)) advance();

            // List/index access as expression
            if (at(TokenType::ASSIGN)) {
                advance();
                std::string val;
                if (at(TokenType::STRING)) { val = "$" + advance().value + "$"; }
                else if (at(TokenType::KW_TRUE) || at(TokenType::KW_FALSE)) {
                    std::string b = advance().value;
                    for (auto &ch : b) ch = std::tolower(ch);
                    val = b;
                } else {
                    while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                        if (at(TokenType::STRING)) val += "$" + advance().value + "$";
                        else val += advance().value;
                    }
                }
                auto node = std::make_unique<ASTNode>(NodeType::IndexExpr, name);
                node->attrs.push_back(indexExpr);
                node->attrs.push_back(val);
                return node;
            } else {
                auto node = std::make_unique<ASTNode>(NodeType::IndexExpr, name);
                node->attrs.push_back(indexExpr);
                return node;
            }
        }

        // Compound assignment: name += value, etc.
        if (at(TokenType::PLUS_EQUAL) || at(TokenType::MINUS_EQUAL) || at(TokenType::MULTIPLY_EQUAL) || at(TokenType::DIVIDE_EQUAL) || at(TokenType::AT_EQUAL)) {
            TokenType opType = advance().type;
            NodeType nodeType;
            switch (opType) {
                case TokenType::PLUS_EQUAL: nodeType = NodeType::PlusEqualStmt; break;
                case TokenType::MINUS_EQUAL: nodeType = NodeType::MinusEqualStmt; break;
                case TokenType::MULTIPLY_EQUAL: nodeType = NodeType::MultiplyEqualStmt; break;
                case TokenType::DIVIDE_EQUAL: nodeType = NodeType::DivideEqualStmt; break;
                case TokenType::AT_EQUAL: nodeType = NodeType::AtEqualStmt; break;
                default: nodeType = NodeType::AssignStmt; break;
            }
            
            std::string val;
            if (at(TokenType::STRING)) {
                val = "$" + advance().value + "$";
            }
            else {
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                    val += advance().value;
            }
            auto node = std::make_unique<ASTNode>(nodeType, name);
            node->attrs.push_back(val);
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
            // range N
            if (at(TokenType::KW_RANGE)) {
                advance();
                std::string n;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) n += advance().value;
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__range__" + n);
                return node;
            }
            // sequence(x, y)
            if (at(TokenType::KW_SEQUENCE)) {
                advance();
                expect(TokenType::LPAREN, "Expected ( after sequence");
                std::string x, y;
                while (!at(TokenType::COMMA) && !at(TokenType::END_OF_FILE)) x += advance().value;
                if (at(TokenType::COMMA)) advance();
                while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) y += advance().value;
                if (at(TokenType::RPAREN)) advance();
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__sequence__" + x + "," + y);
                return node;
            }
            std::string val;
            if (at(TokenType::STRING)) { val = "$" + advance().value + "$"; }
            else if (at(TokenType::KW_TRUE) || at(TokenType::KW_FALSE)) {
                // boolean assignment
                std::string bval = advance().value;
                std::string lower = bval;
                for (auto& ch : lower) ch = std::tolower(ch);
                val = lower; // normalize to lowercase
            } else {
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
