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
    // tag open/close tracking: stack of open tag names
    std::vector<std::string> tagStack;
    // custom tags defined by `def tag <name>`
    std::unordered_set<std::string> customTags;

    Token& peek() { return tokens[pos]; }

    Token& peekAhead(size_t offset) { 
        if (pos + offset < tokens.size()) return tokens[pos + offset];
        return tokens.back();
    }

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
    
    void expectIndent() {
        skipNewlines();
        if (!at(TokenType::INDENT)) {
            throw SYNTAX_ERROR("Expected indentation", peek().line, peek().col);
        }
        advance();
    }
    
    void expectDedent() {
        skipNewlines();
        if (!at(TokenType::DEDENT)) {
            throw SYNTAX_ERROR("Expected dedent (unindent)", peek().line, peek().col);
        }
        advance();
    }

    Token expect(TokenType t, const std::string& msg) {
        if (!at(t)) throw SYNTAX_ERROR(msg, peek().line, peek().col);
        return advance();
    }

    // Pratt Parser for expressions
    // Precedence table:
    // 1: OR
    // 2: AND
    // 3: ==, !=, <, >, <=, >=, is
    // 4: +, -
    // 5: *, /, %, @
    // 6: unary -, NOT
    // 7: function calls, indexing
    
    int getPrecedence(TokenType op) {
        switch (op) {
            // Precedence 1: OR
            case TokenType::IDENTIFIER:
                if (peek().value == "OR" || peek().value == "or") return 1;
                // Check for other operators
                {
                    const std::string& val = peek().value;
                    if (val == "+" || val == "-") return 4;
                    if (val == "*" || val == "/" || val == "%") return 5;
                }
                return 0;  // Not an operator
            
            // Precedence 2: AND
            case TokenType::KW_NOT:
                if (peek().value == "AND" || peek().value == "and") return 2;
                return 0;
            
            // Precedence 3: Comparisons
            case TokenType::KW_IS:
            case TokenType::NOT_EQUAL:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::LTE:
            case TokenType::GTE:
                return 3;
            
            // Precedence 4: Addition/Subtraction
            case TokenType::PLUS_EQUAL:  // When used as + in expression context
            case TokenType::MINUS_EQUAL: // When used as - in expression context
                return 4;
            
            // Precedence 5: Multiplication/Division
            case TokenType::MULTIPLY:
            case TokenType::SLASH:
            case TokenType::AT:
                return 5;
            
            default:
                return 0;
        }
    }
    
    bool isRightAssociative(TokenType op) {
        // All operators are left-associative in AC
        return false;
    }
    
    // Parse prefix expression (literals, identifiers, unary operators, parenthesized expressions)
    NodePtr parsePrefix() {
        // Unary minus
        if (at(TokenType::IDENTIFIER) && peek().value == "-") {
            advance();
            auto operand = parseExpression(6); // Precedence 6 for unary
            auto node = std::make_unique<ASTNode>(NodeType::UnaryExpr, "-");
            node->children.push_back(std::move(operand));
            return node;
        }
        
        // Unary NOT
        if (at(TokenType::KW_NOT)) {
            advance();
            auto operand = parseExpression(6); // Precedence 6 for unary
            auto node = std::make_unique<ASTNode>(NodeType::UnaryExpr, "NOT");
            node->children.push_back(std::move(operand));
            return node;
        }
        
        // Parenthesized expression
        if (at(TokenType::LPAREN)) {
            advance();
            auto expr = parseExpression(0);
            expect(TokenType::RPAREN, "Expected ')' after expression");
            return expr;
        }
        
        // Number literal
        if (at(TokenType::NUMBER)) {
            auto tok = advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, tok.value);
            // Determine if INT or FLOAT
            if (tok.value.find('.') != std::string::npos) {
                node->attrs.push_back("FLOAT");
            } else {
                node->attrs.push_back("INT");
            }
            return node;
        }
        
        // String literal
        if (at(TokenType::STRING)) {
            auto tok = advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, tok.value);
            node->attrs.push_back("STRING");
            return node;
        }
        
        // Boolean literals
        if (at(TokenType::KW_TRUE)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, "true");
            node->attrs.push_back("BOOL");
            return node;
        }
        
        if (at(TokenType::KW_FALSE)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, "false");
            node->attrs.push_back("BOOL");
            return node;
        }
        
        // Identifier (variable or function call)
        if (at(TokenType::IDENTIFIER)) {
            auto tok = advance();
            
            // Function call: func(args)
            if (at(TokenType::LPAREN)) {
                advance();
                auto node = std::make_unique<ASTNode>(NodeType::CallExpr, tok.value);
                
                // Parse arguments
                while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                    node->children.push_back(parseExpression(0));
                    if (at(TokenType::COMMA)) advance();
                }
                
                expect(TokenType::RPAREN, "Expected ')' after function arguments");
                return node;
            }
            
            // Array indexing: arr[index]
            if (at(TokenType::LBRACKET)) {
                advance();
                auto arrayNode = std::make_unique<ASTNode>(NodeType::Identifier, tok.value);
                auto indexExpr = parseExpression(0);
                expect(TokenType::RBRACKET, "Expected ']' after array index");
                
                auto node = std::make_unique<ASTNode>(NodeType::IndexExpr, tok.value);
                node->children.push_back(std::move(arrayNode));
                node->children.push_back(std::move(indexExpr));
                return node;
            }
            
            // Simple identifier
            auto node = std::make_unique<ASTNode>(NodeType::Identifier, tok.value);
            return node;
        }
        
        // List literal
        if (at(TokenType::LBRACKET)) {
            return parseListLiteral();
        }
        
        // Tuple/Dict literal
        if (at(TokenType::LBRACE)) {
            return parseTupleLiteral();
        }
        
        // If we can't parse a prefix, return nullptr
        return nullptr;
    }
    
    // Parse infix expression (binary operators)
    NodePtr parseInfix(NodePtr left, TokenType op) {
        int prec = getPrecedence(op);
        std::string opStr;
        
        // Get operator string
        if (op == TokenType::IDENTIFIER) {
            opStr = peek().value;
            advance();
        } else if (op == TokenType::KW_IS) {
            opStr = "is";
            advance();
        } else if (op == TokenType::NOT_EQUAL) {
            opStr = "#=";
            advance();
        } else if (op == TokenType::LT) {
            opStr = "<";
            advance();
        } else if (op == TokenType::GT) {
            opStr = ">";
            advance();
        } else if (op == TokenType::LTE) {
            opStr = "<=";
            advance();
        } else if (op == TokenType::GTE) {
            opStr = ">=";
            advance();
        } else if (op == TokenType::MULTIPLY) {
            opStr = "*";
            advance();
        } else if (op == TokenType::SLASH) {
            opStr = "/";
            advance();
        } else if (op == TokenType::AT) {
            opStr = "@";
            advance();
        } else {
            // Unknown operator
            return left;
        }
        
        // Parse right operand with appropriate precedence
        auto right = parseExpression(prec + (isRightAssociative(op) ? 0 : 1));
        
        // Create binary expression node
        auto node = std::make_unique<ASTNode>(NodeType::BinaryExpr, opStr);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        return node;
    }
    
    // Main Pratt parser entry point
    NodePtr parseExpression(int precedence = 0) {
        // Parse prefix
        auto left = parsePrefix();
        if (!left) return nullptr;
        
        // Parse infix operators
        while (true) {
            TokenType op = peek().type;
            int prec = getPrecedence(op);
            
            // Stop if precedence is too low or we hit a terminator
            if (prec <= precedence || 
                at(TokenType::NEWLINE) || 
                at(TokenType::END_OF_FILE) ||
                at(TokenType::RPAREN) ||
                at(TokenType::RBRACKET) ||
                at(TokenType::COMMA)) {
                break;
            }
            
            left = parseInfix(std::move(left), op);
        }
        
        return left;
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
        skipNewlines(); // Changed from skipAll() - only skip newlines, not indents
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

        // input <keybind>
        if (at(TokenType::KW_INPUT)) {
            advance();
            std::string keybind;
            if (at(TokenType::IDENTIFIER)) {
                keybind = advance().value;
            }
            return std::make_unique<ASTNode>(NodeType::InputStmt, keybind);
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
        if (at(TokenType::KW_CONFIGURE)) {
            advance(); // consume configure
            expect(TokenType::KW_EVENT_LISTENER, "Expected 'event-listener'");
            skipNewlines();
            
            auto node = std::make_unique<ASTNode>(NodeType::EventListener);
            
            // Parse the nested block: use listener to establish rule
            if (!at(TokenType::INDENT)) {
                throw SYNTAX_ERROR("Expected indentation after 'configure event-listener'", peek().line, peek().col);
            }
            advance(); // consume INDENT
            
            // Skip "use listener to establish rule" line
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                advance();
            }
            skipNewlines();
            
            // Expect INDENT before key bindings
            if (!at(TokenType::INDENT)) {
                throw SYNTAX_ERROR("Expected indentation before key bindings", peek().line, peek().col);
            }
            advance(); // consume INDENT
            
            // Now parse the key bindings - must have at least one
            int bindingCount = 0;
            while (at(TokenType::KW_ON) && !at(TokenType::DEDENT)) {
                advance(); // consume ON
                
                // Strict: on value is <key>
                expect(TokenType::KW_VALUE, "Expected 'value' after 'on'");
                expect(TokenType::KW_IS, "Expected 'is' after 'value'");
                std::string key = expect(TokenType::IDENTIFIER, "Expected key identifier").value;
                
                skipNewlines();
                auto keyBinding = std::make_unique<ASTNode>(NodeType::KeyBinding, key);
                
                // Parse the block on the next line (indented) - can contain any statements
                // Don't manually advance INDENT - let parseBlock() handle it via expectIndent()
                if (at(TokenType::INDENT)) {
                    // Parse a full block of statements (not just a function call)
                    auto block = parseBlock();
                    // Move all children from the parsed block to the keyBinding
                    for (auto& child : block->children) {
                        keyBinding->children.push_back(std::move(child));
                    }
                }
                node->children.push_back(std::move(keyBinding));
                bindingCount++;
                skipNewlines();
            }
            
            if (bindingCount == 0) {
                throw SYNTAX_ERROR("Event listener must contain at least one 'on' binding", peek().line, peek().col);
            }
            
            // Consume the DEDENT after key bindings
            if (!at(TokenType::DEDENT)) {
                throw SYNTAX_ERROR("Expected dedent after key bindings", peek().line, peek().col);
            }
            advance();
            // Consume the second DEDENT (from "use listener to establish rule" level)
            if (!at(TokenType::DEDENT)) {
                throw SYNTAX_ERROR("Expected dedent after event listener block", peek().line, peek().col);
            }
            advance();
            
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
            node->inferredType = std::make_shared<Type>(Type::makeList());
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
            node->inferredType = std::make_shared<Type>(Type::makeList());
            return node;
        }

        // display $string$ (screen output)
        if (at(TokenType::KW_DISPLAY)) {
            advance();
            
            // Parse display expression using Pratt parser
            auto displayExpr = parseExpression(0);
            
            auto node = std::make_unique<ASTNode>(NodeType::DisplayStmt, "");
            if (displayExpr) {
                node->children.push_back(std::move(displayExpr));
            }
            return node;
        }

        // return expr
        if (at(TokenType::KW_RETURN)) {
            advance();
            
            // Parse return expression using Pratt parser
            auto returnExpr = parseExpression(0);
            
            auto node = std::make_unique<ASTNode>(NodeType::ReturnStmt, "");
            if (returnExpr) {
                node->children.push_back(std::move(returnExpr));
            }
            return node;
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

        // Check if this is a closing tag
        // For StartHere, the closing tag is EndHere
        std::string closeWith = (name == "StartHere") ? "EndHere" : name;
        
        // Check if this tag matches the closing tag of the top of stack
        if (!tagStack.empty()) {
            std::string expectedClose = (tagStack.back() == "StartHere") ? "EndHere" : tagStack.back();
            if (name == expectedClose) {
                // This is a closing tag
                tagStack.pop_back();
                auto node = std::make_unique<ASTNode>(NodeType::Block, "__close__" + name);
                return node;
            }
        }

        // This is an opening tag
        tagStack.push_back(name);
        auto block = std::make_unique<ASTNode>(NodeType::TagBlock, name);
        
        // Tag blocks require indentation for their content
        expectIndent();

        while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
            if (at(TokenType::TAG_OPEN) && tokens[pos].value == closeWith) {
                if (name == "StartHere") {
                    advance(); // consume EndHere
                    tagStack.pop_back();
                    break;
                }
                // Check if this is the matching closing tag
                if (!tagStack.empty() && tagStack.back() == name) {
                    advance(); // consume closing tag
                    tagStack.pop_back();
                    break;
                }
            }
            auto stmt = parseStatement();
            if (stmt) {
                if (stmt->type == NodeType::Block &&
                    stmt->value == "__close__" + name) {
                    tagStack.pop_back();
                    break;
                }
                block->children.push_back(std::move(stmt));
            }
            skipNewlines();
        }
        
        expectDedent();
        return block;
    }

    NodePtr parseIf() {
        advance(); // consume IF
        
        // Parse condition using Pratt parser
        auto condExpr = parseExpression(0);
        if (!condExpr) {
            throw SYNTAX_ERROR("Expected condition after IF", peek().line, peek().col);
        }
        
        auto node = std::make_unique<ASTNode>(NodeType::IfStmt, "");
        node->children.push_back(std::move(condExpr)); // Condition as first child
        skipNewlines();
        node->children.push_back(parseBlock()); // Body as second child

        // Collect ELSEIF / OTHER chain
        while (true) {
            skipNewlines();
            if (at(TokenType::KW_ELSEIF)) {
                advance();
                
                // Parse ELSEIF condition using Pratt parser
                auto elseifCondExpr = parseExpression(0);
                if (!elseifCondExpr) {
                    throw SYNTAX_ERROR("Expected condition after ELSEIF", peek().line, peek().col);
                }
                
                auto elseifNode = std::make_unique<ASTNode>(NodeType::ElseIfStmt, "");
                elseifNode->children.push_back(std::move(elseifCondExpr)); // Condition as first child
                skipNewlines();
                elseifNode->children.push_back(parseBlock()); // Body as second child
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
        // FOR item in list  ->  value = "item", children[0] = list expression
        std::string item;
        if (at(TokenType::IDENTIFIER)) item = advance().value;
        if (at(TokenType::KW_IN)) advance(); // consume 'in'
        
        // Parse list expression using Pratt parser
        auto listExpr = parseExpression(0);
        if (!listExpr) {
            throw SYNTAX_ERROR("Expected collection expression after 'in'", peek().line, peek().col);
        }
        
        auto node = std::make_unique<ASTNode>(NodeType::ForLoop, item);
        node->children.push_back(std::move(listExpr)); // Collection expression as first child
        skipNewlines();
        node->children.push_back(parseBlock()); // Body as second child
        return node;
    }

    NodePtr parseWhilst() {
        advance(); // consume WHILST
        
        // Parse condition using Pratt parser
        auto condExpr = parseExpression(0);
        if (!condExpr) {
            throw SYNTAX_ERROR("Expected condition after WHILST", peek().line, peek().col);
        }
        
        auto node = std::make_unique<ASTNode>(NodeType::WhilstLoop, "");
        node->children.push_back(std::move(condExpr)); // Condition as first child
        skipNewlines();
        node->children.push_back(parseBlock()); // Body as second child
        
        // Collect else-if/other chains in the same level
        while (true) {
            skipNewlines();
            if (at(TokenType::KW_ELSEIF)) {
                advance();
                
                // Parse ELSEIF condition using Pratt parser
                auto elseifCondExpr = parseExpression(0);
                if (!elseifCondExpr) {
                    throw SYNTAX_ERROR("Expected condition after ELSEIF", peek().line, peek().col);
                }
                
                auto elseifNode = std::make_unique<ASTNode>(NodeType::ElseIfStmt, "");
                elseifNode->children.push_back(std::move(elseifCondExpr)); // Condition as first child
                skipNewlines();
                elseifNode->children.push_back(parseBlock()); // Body as second child
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
        
        // Check if this is a method chain (Term.method ... & Term.method ...)
        if (at(TokenType::IDENTIFIER)) {
            size_t savedPos = pos;
            
            // Try to parse as method chain
            std::string name = advance().value;
            if (at(TokenType::DOT)) {
                advance();
                std::string prop = advance().value;
                
                // Collect arguments first
                std::string tail;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE) && 
                       !at(TokenType::AMPERSAND) && !at(TokenType::DOUBLE_AMPERSAND)) {
                    if (at(TokenType::STRING)) {
                        tail += "$" + advance().value + "$";
                    } else {
                        tail += advance().value;
                    }
                    if (!at(TokenType::NEWLINE) && !at(TokenType::AMPERSAND) && !at(TokenType::DOUBLE_AMPERSAND)) 
                        tail += " ";
                }
                
                // Check if there's a method chain
                if (at(TokenType::AMPERSAND) || at(TokenType::DOUBLE_AMPERSAND)) {
                    // This is a method chain - parse it properly
                    auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
                    if (!tail.empty()) node->attrs.push_back(tail);
                    
                    // Create chain node
                    auto chainNode = std::make_unique<ASTNode>(NodeType::MethodChain);
                    chainNode->children.push_back(std::move(node));
                    
                    // Parse remaining chained calls
                    while (at(TokenType::AMPERSAND) || at(TokenType::DOUBLE_AMPERSAND)) {
                        bool isSameArg = at(TokenType::DOUBLE_AMPERSAND);
                        advance(); // consume & or &&
                        
                        if (at(TokenType::IDENTIFIER)) {
                            std::string nextName = advance().value;
                            if (at(TokenType::DOT)) {
                                advance();
                                std::string nextProp = advance().value;
                                auto nextNode = std::make_unique<ASTNode>(NodeType::MethodCall, nextName + "." + nextProp);
                                
                                // For &&, use the same argument as the first call
                                if (isSameArg) {
                                    nextNode->attrs.push_back(tail);
                                } else {
                                    // For &, collect different arguments
                                    std::string nextTail;
                                    while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE) && 
                                           !at(TokenType::AMPERSAND) && !at(TokenType::DOUBLE_AMPERSAND)) {
                                        if (at(TokenType::STRING)) {
                                            nextTail += "$" + advance().value + "$";
                                        } else {
                                            nextTail += advance().value;
                                        }
                                        if (!at(TokenType::NEWLINE) && !at(TokenType::AMPERSAND) && !at(TokenType::DOUBLE_AMPERSAND)) 
                                            nextTail += " ";
                                    }
                                    if (!nextTail.empty()) nextNode->attrs.push_back(nextTail);
                                }
                                
                                chainNode->children.push_back(std::move(nextNode));
                            }
                        }
                    }
                    return chainNode;
                }
            }
            
            // Not a method chain, reset and capture as string
            pos = savedPos;
        }
        
        // Fallback: capture entire line as string
        std::string expr;
        while (!at(TokenType::END_OF_FILE) && !at(TokenType::NEWLINE)) {
            if (at(TokenType::STRING)) {
                expr += "$" + advance().value + "$";
            } else {
                expr += advance().value;
            }
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

    // {$item1, item2$} or {key: value\nkey2: value2}
    NodePtr parseTupleLiteral() {
        advance(); // consume {
        
        // Peek ahead to determine if this is a dict or tuple
        // Dict has : before first comma/newline or }
        bool isDict = false;
        size_t lookAhead = pos;
        while (lookAhead < tokens.size() && 
               tokens[lookAhead].type != TokenType::RBRACE &&
               tokens[lookAhead].type != TokenType::END_OF_FILE) {
            // Skip newlines and indents when looking for colon
            if (tokens[lookAhead].type == TokenType::NEWLINE || 
                tokens[lookAhead].type == TokenType::INDENT ||
                tokens[lookAhead].type == TokenType::DEDENT) {
                lookAhead++;
                continue;
            }
            if (tokens[lookAhead].type == TokenType::COLON) {
                isDict = true;
                break;
            }
            lookAhead++;
        }
        
        if (isDict) {
            // Parse as dictionary: {key: value\nkey2: value2}
            std::string contents;
            skipAll(); // Skip newlines and indents after opening {
            
            while (!at(TokenType::RBRACE) && !at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                // Skip any leading newlines/indents
                while (at(TokenType::NEWLINE) || at(TokenType::INDENT)) advance();
                
                if (at(TokenType::RBRACE) || at(TokenType::DEDENT)) break;
                
                // Parse key
                std::string key;
                while (!at(TokenType::COLON) && !at(TokenType::NEWLINE) && 
                       !at(TokenType::RBRACE) && !at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::STRING)) key += advance().value;
                    else key += advance().value;
                }
                
                // Expect colon
                if (!at(TokenType::COLON)) {
                    throw SYNTAX_ERROR("Expected ':' after dict key", peek().line, peek().col);
                }
                advance(); // consume :
                
                // Parse value
                std::string value;
                while (!at(TokenType::NEWLINE) && !at(TokenType::RBRACE) && 
                       !at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::STRING)) value += advance().value;
                    else value += advance().value;
                }
                
                // Add to contents
                if (!key.empty()) {
                    if (!contents.empty()) contents += ",";
                    contents += key + ":" + value;
                }
                
                // Skip newlines between entries
                while (at(TokenType::NEWLINE)) advance();
            }
            
            // Skip closing dedent and brace
            if (at(TokenType::DEDENT)) advance();
            if (at(TokenType::RBRACE)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::DictLiteral, contents);
            return node;
        } else {
            // Parse as tuple: {$item1, item2$}
            std::string contents;
            while (!at(TokenType::RBRACE) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::STRING)) contents += advance().value;
                else contents += advance().value;
            }
            if (at(TokenType::RBRACE)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::TupleLiteral, contents);
            return node;
        }
    }

    NodePtr parseBlock() {
        auto block = std::make_unique<ASTNode>(NodeType::Block);
        expectIndent(); // REQUIRE indentation
        while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
            auto stmt = parseStatement();
            if (stmt) block->children.push_back(std::move(stmt));
            skipNewlines();
        }
        expectDedent(); // REQUIRE dedent
        return block;
    }

    NodePtr parseUse() {
        // use ilib <libname>  — import a built-in library
        // use elib <libname>  — import an external library
        // use clib <libname>  — import a C library
        // use <libname>       — import a library from library/ folder
        
        if (at(TokenType::KW_ILIB)) {
            advance(); // consume ilib
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "ilib:" + libname);
        }
        if (at(TokenType::KW_ELIB)) {
            advance(); // consume elib
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "elib:" + libname);
        }
        if (at(TokenType::KW_CLIB)) {
            advance(); // consume clib
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "clib:" + libname);
        }
        
        // Regular use statement (not ilib/elib/clib)
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
                if (!at(TokenType::NEWLINE)) 
                    tail += " ";
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
            
            // Parse right-hand side expression using Pratt parser
            auto rhsExpr = parseExpression(0);
            if (!rhsExpr) {
                throw SYNTAX_ERROR("Expected expression after compound assignment operator", peek().line, peek().col);
            }
            
            auto node = std::make_unique<ASTNode>(nodeType, name);
            node->children.push_back(std::move(rhsExpr));
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
            // tuple or dict literal
            if (at(TokenType::LBRACE)) {
                auto literalNode = parseTupleLiteral();
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                if (literalNode->type == NodeType::DictLiteral) {
                    node->attrs.push_back("__dict__" + literalNode->value);
                } else {
                    node->attrs.push_back("__tuple__" + literalNode->value);
                }
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
                auto rangeExpr = parseExpression(0);
                if (!rangeExpr) {
                    throw SYNTAX_ERROR("Expected expression after range", peek().line, peek().col);
                }
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__range__");
                node->children.push_back(std::move(rangeExpr));
                return node;
            }
            // sequence(x, y)
            if (at(TokenType::KW_SEQUENCE)) {
                advance();
                expect(TokenType::LPAREN, "Expected ( after sequence");
                auto xExpr = parseExpression(0);
                if (!xExpr) {
                    throw SYNTAX_ERROR("Expected first argument in sequence", peek().line, peek().col);
                }
                expect(TokenType::COMMA, "Expected comma in sequence");
                auto yExpr = parseExpression(0);
                if (!yExpr) {
                    throw SYNTAX_ERROR("Expected second argument in sequence", peek().line, peek().col);
                }
                expect(TokenType::RPAREN, "Expected ) after sequence");
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__sequence__");
                node->children.push_back(std::move(xExpr));
                node->children.push_back(std::move(yExpr));
                return node;
            }
            // function call with keyword args: name = func(key=val, key=val) or func(arg1, arg2)
            if ((at(TokenType::IDENTIFIER) || at(TokenType::KW_DISPLAY)) && peekAhead(1).type == TokenType::LPAREN) {
                std::string funcName = advance().value;
                advance(); // consume (
                std::vector<std::string> kwargs;
                while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                    // Check if this is a keyword argument (has = after identifier)
                    bool isKeywordArg = false;
                    if (at(TokenType::IDENTIFIER)) {
                        // Look ahead to see if there's an = sign
                        size_t savedPos = pos;
                        advance(); // consume identifier
                        if (at(TokenType::ASSIGN)) {
                            isKeywordArg = true;
                        }
                        pos = savedPos; // restore position
                    }
                    
                    if (isKeywordArg) {
                        // Keyword argument: key=value
                        std::string key = advance().value;
                        if (at(TokenType::ASSIGN)) advance();
                        std::string val;
                        while (!at(TokenType::COMMA) && !at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                            if (at(TokenType::STRING)) {
                                val += "$" + advance().value + "$";
                            } else {
                                val += advance().value;
                            }
                        }
                        kwargs.push_back(key + "=" + val);
                    } else {
                        // Positional argument
                        std::string val;
                        while (!at(TokenType::COMMA) && !at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                            if (at(TokenType::STRING)) {
                                val += "$" + advance().value + "$";
                            } else {
                                val += advance().value;
                            }
                        }
                        kwargs.push_back(val); // No key= prefix for positional args
                    }
                    
                    if (at(TokenType::COMMA)) advance();
                }
                if (at(TokenType::RPAREN)) advance();
                auto funcCall = std::make_unique<ASTNode>(NodeType::FunctionCall, funcName);
                for (auto& kwarg : kwargs) funcCall->attrs.push_back(kwarg);
                auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                node->attrs.push_back("__funcall__" + funcName);
                node->children.push_back(std::move(funcCall));
                return node;
            }
            
            // General expression - use Pratt parser
            auto rhsExpr = parseExpression(0);
            if (!rhsExpr) {
                throw SYNTAX_ERROR("Expected expression after assignment", peek().line, peek().col);
            }
            
            auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
            node->children.push_back(std::move(rhsExpr));
            return node;
        }

        // Bare identifier - unidentified syntax, throw error
        auto t = tokens[pos - 1];  // Get the identifier token we just consumed
        throw SYNTAX_ERROR("Unidentified syntax detected", t.line, t.col);
    }
};

NodePtr parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse();
}
