#include "../include/ac.hpp"
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

class Parser {
public:
    struct ParseError {
        int line, col;
        std::string message, context;
    };

private:
    std::vector<Token> tokens;
    size_t pos = 0;
    // tag open/close tracking: stack of open tag names
    std::vector<std::string> tagStack;
    // custom tags defined by `def tag <name>`
    std::unordered_set<std::string> customTags;

    // Error recovery support
    std::vector<ParseError> errors;
    int maxErrors = 10;

    Token& peek() {
        if (pos < tokens.size()) return tokens[pos];
        return tokens.back();  // Return EOF token if out of bounds
    }

    Token& peekAhead(size_t offset) {
        if (pos + offset < tokens.size()) return tokens[pos + offset];
        return tokens.back();
    }

    bool isRecognizedTag(const std::string& name) const {
        return Tags::isKnownTag(name) || customTags.count(name) > 0;
    }
    Token& advance() {
        if (pos < tokens.size()) return tokens[pos++];
        return tokens.back();
    }

    bool at(TokenType t) { return peek().type == t; }

    void reportError(const std::string& msg, const std::string& ctx = "") {
        errors.push_back({peek().line, peek().col, msg, ctx});
        if ((int)errors.size() >= maxErrors) {
            throw std::runtime_error("Too many parse errors");
        }
    }

    // Advance until a safe synchronization point so parsing can resume.
    void synchronize() {
        while (!at(TokenType::END_OF_FILE)) {
            TokenType t = peek().type;
            // Stop AT a newline/dedent so the caller's skipNewlines() can consume it
            if (t == TokenType::NEWLINE || t == TokenType::DEDENT) return;
            // Stop AT statement-starting keywords
            if (t == TokenType::KW_IF     || t == TokenType::KW_FOR    ||
                t == TokenType::KW_WHILST  || t == TokenType::KW_MAKE   ||
                t == TokenType::KW_BUNDLE  || t == TokenType::KW_RETURN ||
                t == TokenType::TAG_OPEN) return;
            advance();
        }
    }

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
        if (!at(t)) {
            reportError(msg, "got '" + peek().value + "'");
            synchronize();
            // Return a dummy token so callers can continue
            return Token(t, "", peek().line, peek().col);
        }
        return advance();
    }

    // Pratt Parser for expressions
    // Precedence table:
    // 1: OR
    // 2: XOR, XNOR  (|, #|)
    // 3: AND  (&, keyword AND)
    // 4: is, #=, <, >, #>(≤), #<(≥)
    // 5: +, -
    // 6: *, /, %, @
    // 7: unary -, #(NOT)
    // 8: function calls, indexing

    int getPrecedence(TokenType op) {
        switch (op) {
            // Precedence 1: OR
            case TokenType::IDENTIFIER:
                if (peek().value == "OR" || peek().value == "or") return 1;
                if (peek().value == "and") return 3;
                // Check for other operators
                {
                    const std::string& val = peek().value;
                    if (val == "+" || val == "-") return 5;
                    if (val == "*" || val == "/") return 6;
                }
                return 0;  // Not an operator

            // Precedence 2: XOR / XNOR
            case TokenType::PIPE:
            case TokenType::HASH_PIPE:
                return 2;

            // Precedence 3: AND
            case TokenType::AMPERSAND:
                return 3;
            case TokenType::KW_NOT:
                if (peek().value == "AND" || peek().value == "and") return 3;
                return 0;

            // Precedence 4: Comparisons (#> = ≤, #< = ≥) and overlap/is infix
            case TokenType::KW_IS:
            case TokenType::NOT_EQUAL:
            case TokenType::LT:
            case TokenType::GT:
            case TokenType::HASH_GT:
            case TokenType::HASH_LT:
            case TokenType::KW_OVERLAP:
                return 4;

            // Precedence 5: Addition/Subtraction/xsub
            case TokenType::PLUS_EQUAL:  // When used as + in expression context
            case TokenType::MINUS_EQUAL: // When used as - in expression context
            case TokenType::KW_XSUB:
                return 5;

            // Precedence 6: Multiplication/Division
            case TokenType::MULTIPLY:
            case TokenType::SLASH:
            case TokenType::DOUBLE_SLASH:
            case TokenType::AT:
                return 6;

            // Precedence 7: Exponentiation (right-associative)
            case TokenType::CARET:
                return 7;

            default:
                return 0;
        }
    }
    
    bool isRightAssociative(TokenType op) {
        // Exponentiation is right-associative; all others are left-associative
        return op == TokenType::CARET;
    }
    
    // Parse prefix expression (literals, identifiers, unary operators, parenthesized expressions)
    NodePtr parsePrefix() {
        // Unary minus
        if (at(TokenType::IDENTIFIER) && peek().value == "-") {
            advance();
            auto operand = parseExpression(7); // Precedence 7 for unary
            auto node = std::make_unique<ASTNode>(NodeType::UnaryExpr, "-");
            node->children.push_back(std::move(operand));
            return node;
        }
        
        // Unary NOT (keyword)
        if (at(TokenType::KW_NOT)) {
            advance();
            auto operand = parseExpression(7); // Precedence 7 for unary
            auto node = std::make_unique<ASTNode>(NodeType::UnaryExpr, "NOT");
            node->children.push_back(std::move(operand));
            return node;
        }

        // Unary # (boolean NOT)
        if (at(TokenType::HASH)) {
            advance();
            auto operand = parseExpression(7); // Precedence 7 for unary
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

        // "..." outside fn — hard error
        if (at(TokenType::DQUOTE_STRING)) {
            auto t = peek();
            throw SYNTAX_ERROR("\"...\" strings are only valid inside fn expressions; use $...$ here", t.line, t.col);
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

        // Null literal
        if (at(TokenType::KW_NULL)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, "null");
            node->attrs.push_back("NULL");
            return node;
        }

        // Nil literal — empty set ∅; [nil] = {∅}
        if (at(TokenType::KW_NIL)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::LiteralExpr, "nil");
            node->attrs.push_back("NIL");
            return node;
        }
        
        // Identifier (variable or function call)
        // Also allow event-system keywords (value, on, rule, listener) as variable names
        if (at(TokenType::IDENTIFIER) || at(TokenType::KW_VALUE) || at(TokenType::KW_RULE)) {
            auto tok = advance();

            // Method call in expression: obj.method or obj.ns.method(args) etc.
            if (at(TokenType::DOT)) {
                advance(); // consume "."
                // accept any token after dot (keywords like "overlap" are valid method names)
                std::string methodName = !at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE) ? advance().value : "";
                // allow chained dots: gl.obj.x, gl.hitbox.overlap, etc.
                while (at(TokenType::DOT)) {
                    advance();
                    if (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE) && !at(TokenType::LPAREN))
                        methodName += "." + advance().value;
                }
                std::string fullName = tok.value + "." + methodName;
                auto node = std::make_unique<ASTNode>(NodeType::MethodCall, fullName);
                if (at(TokenType::LPAREN)) {
                    advance();
                    node->attrs.push_back("__called__"); // marks explicit () call syntax
                    while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                        node->children.push_back(parseExpression(0));
                        if (at(TokenType::COMMA)) advance();
                    }
                    if (at(TokenType::RPAREN)) advance();
                } else if (at(TokenType::STRING)) {
                    auto lit = std::make_unique<ASTNode>(NodeType::LiteralExpr, advance().value);
                    lit->attrs.push_back("STRING");
                    node->children.push_back(std::move(lit));
                } else if (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE) &&
                           !at(TokenType::RPAREN) && !at(TokenType::RBRACKET) &&
                           getPrecedence(peek().type) == 0) {
                    auto argExpr = parseExpression(0);
                    if (argExpr) node->children.push_back(std::move(argExpr));
                }
                return node;
            }

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

        // eval(expr) — native AC expression evaluator; usable in any expression context
        if (at(TokenType::KW_EVAL)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after eval");
            auto arg = parseExpression(0);
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::EvalExpr, "");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        if (at(TokenType::KW_LAZY_EVAL)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after lazy_eval");
            auto arg = parseExpression(0);
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::LazyEvalExpr, "");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        // sure $msg$ — browser confirm(); usable as expression: result = sure $Delete?$
        if (at(TokenType::KW_SURE)) {
            advance();
            auto arg = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, "sure");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        // range N — usable in expression position, e.g. FOR i in range 5
        if (at(TokenType::KW_RANGE)) {
            advance();
            auto boundExpr = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::RangeExpr, "");
            if (boundExpr) node->children.push_back(std::move(boundExpr));
            node->inferredType = std::make_shared<Type>(Type::makeList());
            return node;
        }

        // sequence(a, b) — usable in expression position, e.g. FOR n in sequence(3, 7)
        if (at(TokenType::KW_SEQUENCE)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after sequence");
            auto xExpr = parseExpression(0);
            expect(TokenType::COMMA, "Expected , in sequence(a, b)");
            auto yExpr = parseExpression(0);
            expect(TokenType::RPAREN, "Expected ) after sequence");
            auto node = std::make_unique<ASTNode>(NodeType::SequenceExpr, "");
            if (xExpr) node->children.push_back(std::move(xExpr));
            if (yExpr) node->children.push_back(std::move(yExpr));
            node->inferredType = std::make_shared<Type>(Type::makeList());
            return node;
        }

        // length(x) — get array/string length
        if (at(TokenType::KW_LENGTH)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after length");
            auto arg = parseExpression(0);
            expect(TokenType::RPAREN, "Expected ) after length argument");
            auto node = std::make_unique<ASTNode>(NodeType::CallExpr, "length");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        // If we hit a natural expression terminator, there is simply no expression here.
        // Return nullptr to let callers decide if this is an error.
        {
            TokenType t = peek().type;
            if (t == TokenType::NEWLINE   || t == TokenType::DEDENT     ||
                t == TokenType::END_OF_FILE || t == TokenType::RPAREN   ||
                t == TokenType::RBRACKET  || t == TokenType::RBRACE     ||
                t == TokenType::COMMA     || t == TokenType::COLON) {
                return nullptr;
            }
        }

        // An unexpected token in the middle of an expression: report error and return nullptr
        // This allows callers (like binary operators) to detect the error
        reportError("Unexpected token '" + peek().value + "' in expression");
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
            if (at(TokenType::KW_NOT)) {
                auto t = peek();
                throw SYNTAX_ERROR("Use #= for not-equal; 'is not' is not AC syntax", t.line, t.col);
            }
        } else if (op == TokenType::NOT_EQUAL) {
            opStr = "#=";
            advance();
        } else if (op == TokenType::LT) {
            opStr = "<";
            advance();
        } else if (op == TokenType::GT) {
            opStr = ">";
            advance();
        } else if (op == TokenType::MULTIPLY) {
            opStr = "*";
            advance();
        } else if (op == TokenType::DOUBLE_SLASH) {
            opStr = "//";
            advance();
        } else if (op == TokenType::SLASH) {
            opStr = "/";
            advance();
        } else if (op == TokenType::AT) {
            opStr = "@";
            advance();
        } else if (op == TokenType::PIPE) {
            opStr = "|";
            advance();
        } else if (op == TokenType::HASH_PIPE) {
            opStr = "#|";
            advance();
        } else if (op == TokenType::HASH_GT) {
            opStr = "#>";
            advance();
        } else if (op == TokenType::HASH_LT) {
            opStr = "#<";
            advance();
        } else if (op == TokenType::AMPERSAND) {
            opStr = "&";
            advance();
        } else if (op == TokenType::CARET) {
            opStr = "^";
            advance();
        } else if (op == TokenType::KW_OVERLAP) {
            // "a.hitbox.coords overlap b.hitbox.coords" → hitbox overlap check
            opStr = "overlap";
            advance();
        } else if (op == TokenType::KW_XSUB) {
            opStr = "xsub";
            advance();
        } else {
            // Unknown operator
            return left;
        }
        
        // Parse right operand with appropriate precedence
        // Left-assoc: pass opPrec so right grabs only strictly-higher-prec ops (nextPrec > opPrec)
        // Right-assoc: pass opPrec-1 so right also grabs same-prec ops (nextPrec >= opPrec)
        auto right = parseExpression(prec - (isRightAssociative(op) ? 1 : 0));

        // Validate right operand exists
        if (!right) {
            auto opToken = peek();  // The operator token for error reporting
            throw SYNTAX_ERROR(std::string("Expected expression after operator '") + opStr + "'", opToken.line, opToken.col);
        }

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
        try {
            while (!at(TokenType::END_OF_FILE)) {
                auto node = parseStatement();
                if (node) program->children.push_back(std::move(node));
                skipAll();
            }
        } catch (const std::runtime_error& e) {
            // "Too many parse errors" — stop parsing and return partial AST
            std::string msg = e.what();
            if (msg != "Too many parse errors") {
                errors.push_back({peek().line, peek().col, msg, ""});
            }
        }
        return program;
    }

    bool hasErrors() const { return !errors.empty(); }

    const std::vector<ParseError>& getErrors() const { return errors; }

private:
    NodePtr parseStatement() {
        skipNewlines(); // Changed from skipAll() - only skip newlines, not indents
        if (at(TokenType::END_OF_FILE)) return nullptr;
        try {
        return parseStatementInner();
        } catch (const std::runtime_error& e) {
            // "Too many parse errors" — rethrow so the outer parse() loop can stop
            throw;
        } catch (const ACError& e) {
            // ACError from throws inside parsers that weren't converted yet
            errors.push_back({peek().line, peek().col, e.what(), ""});
            if ((int)errors.size() >= maxErrors) {
                throw std::runtime_error("Too many parse errors");
            }
            synchronize();
            return nullptr;
        }
    }

    NodePtr parseStatementInner() {
        if (at(TokenType::END_OF_FILE)) return nullptr;

        // 'catch' / 'after' are only valid as part of 'try ... catch ... after ...'
        if (at(TokenType::KW_CATCH) || at(TokenType::KW_AFTER)) {
            auto t = peek();
            throw SYNTAX_ERROR("Unexpected '" + t.value + "' without preceding 'try'", t.line, t.col);
        }

        // Orphan INDENT — consume nested block, attach as sub-block to parent
        if (at(TokenType::INDENT)) {
            auto block = std::make_unique<ASTNode>(NodeType::Block, "__nested__");
            advance(); // consume INDENT
            while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                auto s = parseStatement();
                if (s) block->children.push_back(std::move(s));
                skipNewlines();
            }
            if (at(TokenType::DEDENT)) advance();
            return block;
        }

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

        // /kill  /stop  /end  /restart
        if (at(TokenType::SLASH)) {
            auto t = advance();
            if (t.value == "kill")
                return std::make_unique<ASTNode>(NodeType::KillStmt);
            if (t.value == "restart")
                return std::make_unique<ASTNode>(NodeType::RestartStmt);
            if (t.value == "stop")
                return std::make_unique<ASTNode>(NodeType::StopStmt);
            if (t.value == "end")
                return std::make_unique<ASTNode>(NodeType::EndStmt);
            if (t.value == "halt") {
                // collect duration: /halt 1  /halt 2.5  /halt math.inf
                std::string arg;
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                    arg += advance().value;
                // strip leading/trailing spaces
                while (!arg.empty() && arg.front() == ' ') arg.erase(arg.begin());
                while (!arg.empty() && arg.back()  == ' ') arg.pop_back();
                if (arg == "math.inf" || arg == "inf")
                    return std::make_unique<ASTNode>(NodeType::StopStmt);
                if (arg.empty())
                    return std::make_unique<ASTNode>(NodeType::PassStmt);
                return std::make_unique<ASTNode>(NodeType::HaltStmt, arg);
            }
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

        // using header <libname> — bring library namespace into flat scope
        if (at(TokenType::KW_USING)) {
            advance(); // consume 'using'
            if (at(TokenType::IDENTIFIER) && peek().value == "header") advance(); // optional 'header'

            // Support single-symbol import sugar:
            //   using math.sin
            //   using math.sqrt
            // Expands to:
            //   using header math      (enables unqualified calls)
            //   from ilib math use sin (restricts imported surface to the symbol)
            if (at(TokenType::IDENTIFIER)) {
                std::string lib = advance().value;
                if (at(TokenType::DOT)) {
                    advance(); // consume '.'
                    std::string sym = at(TokenType::IDENTIFIER) ? advance().value : "";
                    // consume any remaining tokens as additional ".sym" segments (reject for now)
                    if (!sym.empty() && !at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                        // Allow multiple "lib.sym" tokens on one line: using math.sin math.sqrt
                        std::vector<std::string> syms;
                        syms.push_back(sym);
                        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                            if (at(TokenType::COMMA)) { advance(); continue; }
                            if (!at(TokenType::IDENTIFIER)) { advance(); continue; }
                            std::string lib2 = advance().value;
                            if (!at(TokenType::DOT)) break;
                            advance();
                            if (!at(TokenType::IDENTIFIER)) break;
                            std::string sym2 = advance().value;
                            if (lib2 == lib) syms.push_back(sym2);
                        }

                        auto chain = std::make_unique<ASTNode>(NodeType::MethodChain);
                        chain->children.push_back(std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:" + lib));
                        auto selective = std::make_unique<ASTNode>(NodeType::UseLibStmt, "ilib:" + lib);
                        for (auto& s : syms) selective->attrs.push_back(s);
                        chain->children.push_back(std::move(selective));
                        return chain;
                    }

                    auto chain = std::make_unique<ASTNode>(NodeType::MethodChain);
                    chain->children.push_back(std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:" + lib));
                    auto selective = std::make_unique<ASTNode>(NodeType::UseLibStmt, "ilib:" + lib);
                    if (!sym.empty()) selective->attrs.push_back(sym);
                    chain->children.push_back(std::move(selective));
                    // Consume the rest of the line (if any)
                    while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
                    return chain;
                } else {
                    // Fallback: original behavior, treat remainder as lib name.
                    std::string libname = lib;
                    while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                        libname += advance().value;
                    // trim
                    auto notSpace = [](char c){ return !std::isspace((unsigned char)c); };
                    libname.erase(libname.begin(), std::find_if(libname.begin(), libname.end(), notSpace));
                    libname.erase(std::find_if(libname.rbegin(), libname.rend(), notSpace).base(), libname.end());
                    return std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:" + libname);
                }
            }

            // No identifier after 'using' — treat as empty.
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:");
        }

        // from ilib/elib/clib <name> use <symbol>[, <symbol>, ...]
        // e.g.: from ilib math use sin, cos, sqrt
        if (at(TokenType::KW_FROM)) {
            advance(); // consume 'from'
            std::string libType = "ilib";
            if (at(TokenType::KW_ILIB))      { advance(); libType = "ilib"; }
            else if (at(TokenType::KW_ELIB)) { advance(); libType = "elib"; }
            else if (at(TokenType::KW_CLIB)) { advance(); libType = "clib"; }
            else if (at(TokenType::KW_FLIB)) { advance(); libType = "flib"; }
            std::string libName;
            if (at(TokenType::IDENTIFIER)) libName = advance().value;
            auto node = std::make_unique<ASTNode>(NodeType::UseLibStmt, libType + ":" + libName);
            // Collect symbol list after 'use': sym1, sym2, sym3 ...
            if (at(TokenType::KW_USE)) {
                advance(); // consume 'use'
                while (at(TokenType::IDENTIFIER)) {
                    node->attrs.push_back(advance().value); // each requested symbol
                    if (at(TokenType::COMMA)) advance(); else break;
                }
            }
            return node;
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

        // try ... catch ... after ...
        if (at(TokenType::KW_TRY)) {
            return parseTry();
        }

        // cond x ...  (multi-branch dispatch sugar)
        if (at(TokenType::IDENTIFIER) && peek().value == "cond") {
            advance(); // consume 'cond'
            auto baseExpr = parseExpression(0);
            skipNewlines();
            auto node = std::make_unique<ASTNode>(NodeType::CondStmt);
            if (baseExpr) node->children.push_back(std::move(baseExpr));
            expectIndent();
            skipNewlines();
            while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::KW_IS)) {
                    advance();
                    auto caseExpr = parseExpression(0);
                    skipNewlines();
                    auto block = parseBlock();
                    auto c = std::make_unique<ASTNode>(NodeType::CondCase);
                    if (caseExpr) c->children.push_back(std::move(caseExpr));
                    c->children.push_back(std::move(block));
                    node->children.push_back(std::move(c));
                    skipNewlines();
                    continue;
                }
                if (at(TokenType::KW_OTHER)) {
                    advance();
                    skipNewlines();
                    auto block = parseBlock();
                    auto o = std::make_unique<ASTNode>(NodeType::CondOther);
                    o->children.push_back(std::move(block));
                    node->children.push_back(std::move(o));
                    skipNewlines();
                    continue;
                }
                // Unknown line: parse as statement (allows comments or nested things)
                auto s = parseStatement();
                if (s) node->children.push_back(std::move(s));
                skipNewlines();
            }
            expectDedent();
            return node;
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

            // raise ERR (by itself, no parens) → Preposterous: Fatality occurred
            if (at(TokenType::KW_ERR) && peekAhead(1).type != TokenType::LPAREN) {
                advance(); // consume ERR
                return std::make_unique<ASTNode>(NodeType::RaiseStmt, "");
            }
            // raise ERR($msg$) → Preposterous: msg + abort
            if (at(TokenType::KW_ERR)) {
                advance();
                expect(TokenType::LPAREN, "Expected ( after ERR");
                std::string msg;
                if (at(TokenType::STRING)) msg = advance().value;
                expect(TokenType::RPAREN, "Expected ) after ERR message");
                return std::make_unique<ASTNode>(NodeType::RaiseStmt, msg);
            }

            // Read clause name (identifier: hint / toxic / Hint / Err / Toxic / Praise / any)
            std::string clauseName;
            if (at(TokenType::IDENTIFIER)) clauseName = advance().value;

            // No parens: bare `raise ClauseName` → "ClauseName: (no message)"
            if (!at(TokenType::LPAREN)) {
                auto node = std::make_unique<ASTNode>(NodeType::RaiseClauseStmt, clauseName);
                return node;
            }
            advance(); // consume (
            std::string msg;
            if (at(TokenType::STRING)) msg = advance().value;
            if (at(TokenType::RPAREN)) advance();

            // Reserved lowercase specials:
            // hint(msg)  → Suggestion: msg  (printed to stderr, not fatal)
            // toxic(msg) → Toxic: msg  (warning to stderr, not fatal)
            // All others (Hint, Err, Toxic, Praise, MyClause...) → "ClauseName: msg"
            auto node = std::make_unique<ASTNode>(NodeType::RaiseClauseStmt, clauseName);
            node->attrs.push_back(msg);
            return node;
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

        // bundle X — class/struct definition
        if (at(TokenType::KW_BUNDLE)) {
            advance();
            return parseBundle();
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

        // eval(expr) — evaluate string as AC expression, returns value
        if (at(TokenType::KW_EVAL)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after eval");
            auto arg = parseExpression(0);
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::EvalExpr, "");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        // lazy_eval(expr) — safe deferred evaluation (wraps in try/catch per backend)
        if (at(TokenType::KW_LAZY_EVAL)) {
            advance();
            expect(TokenType::LPAREN, "Expected ( after lazy_eval");
            auto arg = parseExpression(0);
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::LazyEvalExpr, "");
            if (arg) node->children.push_back(std::move(arg));
            return node;
        }

        // math.GoodDec x = expr — explicit decimal declaration (unscaled, scale)
        if (at(TokenType::IDENTIFIER) && peek().value == "math"
            && peekAhead(1).type == TokenType::DOT
            && peekAhead(2).type == TokenType::IDENTIFIER
            && peekAhead(2).value == "GoodDec") {
            advance(); // math
            advance(); // .
            advance(); // GoodDec
            std::string varName = expect(TokenType::IDENTIFIER, "Expected variable name after math.GoodDec").value;
            NodePtr valExpr;
            if (at(TokenType::ASSIGN)) {
                advance();
                valExpr = parseExpression();
                if (valExpr && valExpr->type == NodeType::TupleLiteral) {
                    auto t = peek();
                    throw SYNTAX_ERROR("math.GoodDec does not accept {unscaled, scale}; write decimals like 1.23 or expressions like 0.1 + 0.2", t.line, t.col);
                }
            }
            auto node = std::make_unique<ASTNode>(NodeType::TypeCoerceStmt, varName);
            node->attrs.push_back("GOODDEC");
            if (valExpr) node->children.push_back(std::move(valExpr));
            return node;
        }

        // math.LongInt x = expr — explicit signed 96-bit integer declaration (folds when constant)
        if (at(TokenType::IDENTIFIER) && peek().value == "math"
            && peekAhead(1).type == TokenType::DOT
            && peekAhead(2).type == TokenType::IDENTIFIER
            && peekAhead(2).value == "LongInt") {
            advance(); // math
            advance(); // .
            advance(); // LongInt
            std::string varName = expect(TokenType::IDENTIFIER, "Expected variable name after math.LongInt").value;
            NodePtr valExpr;
            if (at(TokenType::ASSIGN)) {
                advance();
                valExpr = parseExpression();
            }
            auto node = std::make_unique<ASTNode>(NodeType::TypeCoerceStmt, varName);
            node->attrs.push_back("LONGINT");
            if (valExpr) node->children.push_back(std::move(valExpr));
            return node;
        }

        // dec/int/string/bool x [= expr]  →  coerce x to target type
        {
            std::string coerceType;
            if      (at(TokenType::KW_DEC))    coerceType = "DEC";
            else if (at(TokenType::KW_INT))    coerceType = "INT";
            else if (at(TokenType::KW_STRING)) coerceType = "STRING";
            else if (at(TokenType::KW_BOOL))   coerceType = "BOOL";

            if (!coerceType.empty()) {
                advance();
                std::string varName;
                if (at(TokenType::IDENTIFIER)) varName = advance().value;
                NodePtr valExpr;
                if (at(TokenType::ASSIGN)) {
                    advance();
                    valExpr = parseExpression();
                }
                // no valExpr → coerce current value of varName
                auto node = std::make_unique<ASTNode>(NodeType::TypeCoerceStmt, varName);
                node->attrs.push_back(coerceType);
                if (valExpr) node->children.push_back(std::move(valExpr));
                return node;
            }
        }

        // free x, y, z  →  declare as globally scoped (like Python's global)
        // alias x = y — bidirectional live binding
        if (at(TokenType::KW_ALIAS)) {
            advance();
            std::string lhs = expect(TokenType::IDENTIFIER, "Expected variable name after alias").value;
            expect(TokenType::ASSIGN, "Expected = in alias declaration");
            std::string rhs = expect(TokenType::IDENTIFIER, "Expected variable name after alias =").value;
            auto node = std::make_unique<ASTNode>(NodeType::AliasDecl, lhs);
            node->attrs.push_back(rhs);
            return node;
        }

        // const x = expr — immutable binding
        if (at(TokenType::KW_CONST)) {
            advance();
            std::string name = expect(TokenType::IDENTIFIER, "Expected variable name after const").value;
            expect(TokenType::ASSIGN, "Expected = after const variable name");
            auto val = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::ConstDecl, name);
            if (val) node->children.push_back(std::move(val));
            return node;
        }

        // cp x = y — explicit value copy
        if (at(TokenType::KW_CP)) {
            advance();
            std::string lhs = expect(TokenType::IDENTIFIER, "Expected variable name after cp").value;
            expect(TokenType::ASSIGN, "Expected = after cp variable name");
            auto rhs = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::CopyStmt, lhs);
            if (rhs) node->children.push_back(std::move(rhs));
            return node;
        }

        if (at(TokenType::KW_FREE)) {
            advance();
            auto node = std::make_unique<ASTNode>(NodeType::FreeDecl, "");
            // collect comma-separated identifiers on the same line
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                if (at(TokenType::IDENTIFIER))
                    node->attrs.push_back(advance().value);
                else if (at(TokenType::COMMA))
                    advance();
                else
                    break;
            }
            return node;
        }

        // print_page  →  browser window.print() — no argument
        if (at(TokenType::KW_PRINT_PAGE)) {
            advance();
            return std::make_unique<ASTNode>(NodeType::MethodCall, "print_page");
        }

        // alert $msg$ / alert(msg)  →  browser window.alert()
        if (at(TokenType::KW_ALERT)) {
            advance();
            auto argExpr = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, "alert");
            if (argExpr) node->children.push_back(std::move(argExpr));
            return node;
        }

        // sure $msg$  →  browser window.confirm() — also handled in parsePrefix as expression
        if (at(TokenType::KW_SURE)) {
            advance();
            auto argExpr = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, "sure");
            if (argExpr) node->children.push_back(std::move(argExpr));
            return node;
        }

        // return expr
        if (at(TokenType::KW_RETURN)) {
            advance();

            auto returnExpr = parseExpression(0);
            auto node = std::make_unique<ASTNode>(NodeType::ReturnStmt, "");

            if (returnExpr) {
                // return x, y  — collect comma-separated values into a list
                if (at(TokenType::COMMA)) {
                    auto list = std::make_unique<ASTNode>(NodeType::ListLiteral, "");
                    list->children.push_back(std::move(returnExpr));
                    while (at(TokenType::COMMA)) {
                        advance();
                        auto next = parseExpression(0);
                        if (next) list->children.push_back(std::move(next));
                    }
                    node->children.push_back(std::move(list));
                } else {
                    node->children.push_back(std::move(returnExpr));
                }
            }
            return node;
        }

        // True / False / null literals as standalone statements or assignments
        if (at(TokenType::KW_TRUE) || at(TokenType::KW_FALSE) || at(TokenType::KW_NULL) || at(TokenType::KW_NIL)) {
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
        // Also allow event-system keywords as variable names when used in statement position
        if (at(TokenType::IDENTIFIER) || at(TokenType::KW_PROGRAM_LOOP) ||
            at(TokenType::KW_VALUE)   || at(TokenType::KW_RULE)) {
            return parseIdentifierStatement();
        }

        // Skip unknown tokens
        advance();
        return nullptr;
    }

    NodePtr parseTagBlock() {
        auto tagTok = advance(); // consume <tagname>
        std::string name = tagTok.value;

        // For StartHere, the matching close tag is EndHere; all others close with themselves
        std::string closeWith = (name == "StartHere") ? "EndHere" : name;

        // If the tag stack has an open tag that this name closes, treat it as a close
        if (!tagStack.empty()) {
            std::string expectedClose = (tagStack.back() == "StartHere") ? "EndHere" : tagStack.back();
            if (name == expectedClose) {
                tagStack.pop_back();
                return std::make_unique<ASTNode>(NodeType::Block, "__close__" + name);
            }
        }

        // Opening tag — push and parse body
        tagStack.push_back(name);
        auto block = std::make_unique<ASTNode>(NodeType::TagBlock, name);

        expectIndent();

        bool closed = false;
        while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
            auto stmt = parseStatement();
            if (stmt) {
                if (stmt->type == NodeType::Block && stmt->value.rfind("__close__", 0) == 0) {
                    std::string closedName = stmt->value.substr(9);
                    if (closedName == name) {
                        // Our own closing tag was found inside the indented body (mis-indented close)
                        closed = true;
                        break;
                    }
                    // Stale __close__ from a child — discard it
                    continue;
                }
                block->children.push_back(std::move(stmt));
            }
            skipNewlines();
        }

        if (closed) {
            // Closed inside the loop (body not yet dedented) — consume DEDENT if present
            if (at(TokenType::DEDENT)) advance();
        } else {
            // Normal case: loop exited on DEDENT — consume it, then consume the closing tag
            if (at(TokenType::DEDENT)) advance();
            skipNewlines();
            if (at(TokenType::TAG_OPEN) && peek().value == closeWith) {
                advance(); // consume matching closing tag
                if (!tagStack.empty() && tagStack.back() == name) tagStack.pop_back();
            } else if (!tagStack.empty() && tagStack.back() == name) {
                tagStack.pop_back(); // unclosed block — pop anyway
            }
        }

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

        // Special: "WHILST many hitbox overlap" → while gl_hitbox_many_overlap()
        if (at(TokenType::KW_MANY)) {
            advance(); // consume "many"
            // consume "hitbox overlap" if present
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
            auto manyCall = std::make_unique<ASTNode>(NodeType::FunctionCall, "gl_hitbox_many_overlap");
            auto node = std::make_unique<ASTNode>(NodeType::WhilstLoop, "");
            node->children.push_back(std::move(manyCall));
            skipNewlines();
            node->children.push_back(parseBlock());
            return node;
        }

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

    NodePtr parseTry() {
        advance(); // consume 'try'
        skipNewlines();

        // Parse try body
        auto tryBody = parseBlock();

        // Parse one or more catch clauses, then optional after.
        auto node = std::make_unique<ASTNode>(NodeType::TryCatchStmt);
        node->children.push_back(std::move(tryBody));

        skipNewlines();
        while (at(TokenType::KW_CATCH)) {
            advance(); // consume 'catch'
            skipNewlines();

            // Optional type name on the catch line: catch TypeName
            std::string typeName;
            if (at(TokenType::IDENTIFIER)) typeName = advance().value;
            skipNewlines();

            // Parse catch block manually to extract optional 'report <var>'
            std::string exceptionVar = "_exc"; // default exception variable name
            auto catchBody = std::make_unique<ASTNode>(NodeType::Block);
            expectIndent();
            skipNewlines();

            // 'report <identifier>' on the first line of catch sets the exception variable
            if (at(TokenType::KW_REPORT)) {
                advance(); // consume 'report'
                if (at(TokenType::IDENTIFIER)) exceptionVar = advance().value;
                skipNewlines();
            }

            // Parse remaining catch body
            while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
                auto stmt = parseStatement();
                if (stmt) catchBody->children.push_back(std::move(stmt));
                skipNewlines();
            }
            expectDedent();

            auto clause = std::make_unique<ASTNode>(NodeType::CatchClause, typeName);
            clause->attrs.push_back(exceptionVar);
            clause->children.push_back(std::move(catchBody));
            node->children.push_back(std::move(clause));

            skipNewlines();
        }

        if (node->children.size() < 2) {
            throw SYNTAX_ERROR("Expected at least one 'catch' after try block", peek().line, peek().col);
        }

        // Optional 'after' block (finally)
        if (at(TokenType::KW_AFTER)) {
            advance(); // consume 'after'
            skipNewlines();
            auto after = std::make_unique<ASTNode>(NodeType::AfterClause);
            after->children.push_back(parseBlock());
            node->children.push_back(std::move(after));
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
                    if (at(TokenType::STRING) || at(TokenType::DQUOTE_STRING)) {
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
                                        if (at(TokenType::STRING) || at(TokenType::DQUOTE_STRING)) {
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
                    if (at(TokenType::STRING)) key += "$" + advance().value + "$";
                    else key += advance().value;
                }
                
                // Expect colon
                if (!at(TokenType::COLON)) {
                    throw SYNTAX_ERROR("Expected ':' after dict key", peek().line, peek().col);
                }
                advance(); // consume :
                
                // Parse value (stops at comma, newline, } or dedent)
                std::string value;
                while (!at(TokenType::NEWLINE) && !at(TokenType::COMMA) &&
                       !at(TokenType::RBRACE) && !at(TokenType::DEDENT) &&
                       !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::STRING)) value += "$" + advance().value + "$";
                    else value += advance().value;
                }
                // consume trailing comma between entries
                if (at(TokenType::COMMA)) advance();
                
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
        // use clib <libname>  — import a computer library (globally installed on the system)
        // use header <libname> — same as "using header": flat namespace import
        // use <libname>       — import a library from library/ folder

        // use header <libname> — alias for "using header <libname>"
        if (at(TokenType::IDENTIFIER) && peek().value == "header") {
            advance(); // consume "header"
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            auto notSpace = [](char c){ return !std::isspace((unsigned char)c); };
            libname.erase(libname.begin(), std::find_if(libname.begin(), libname.end(), notSpace));
            libname.erase(std::find_if(libname.rbegin(), libname.rend(), notSpace).base(), libname.end());
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:" + libname);
        }

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
        if (at(TokenType::KW_FLIB)) {
            advance(); // consume flib
            // Collect rest of line as a path, reconstructing slashes.
            // The lexer tokenizes "/" as SLASH with the following alpha chars as value,
            // so "/tmp/test_lib" → SLASH("tmp") SLASH("test") IDENT("_lib").
            // We reconstruct by prepending "/" for each SLASH token.
            std::string libpath;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                auto& t = peek();
                if (t.type == TokenType::SLASH) {
                    advance();
                    libpath += "/" + t.value;
                } else if (t.type == TokenType::DOT) {
                    advance();
                    libpath += ".";
                } else {
                    libpath += advance().value;
                }
            }
            // strip spaces
            auto ns = libpath.find_first_not_of(' ');
            auto ne = libpath.find_last_not_of(' ');
            libpath = (ns == std::string::npos) ? "" : libpath.substr(ns, ne - ns + 1);
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "flib:" + libpath);
        }

        if (at(TokenType::KW_DATAC)) {
            advance(); // consume datac
            // collect filename (may be a quoted STRING or bare tokens up to 'as')
            std::string filepath;
            if (at(TokenType::STRING)) {
                filepath = advance().value; // already stripped of quotes by lexer
            } else {
                while (!at(TokenType::KW_AS) && !at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    auto& t = peek();
                    if (t.type == TokenType::SLASH) { advance(); filepath += "/" + t.value; }
                    else if (t.type == TokenType::DOT) { advance(); filepath += "."; }
                    else filepath += advance().value;
                }
            }
            auto ns = filepath.find_first_not_of(' ');
            auto ne = filepath.find_last_not_of(' ');
            filepath = (ns == std::string::npos) ? "" : filepath.substr(ns, ne - ns + 1);
            // consume 'as'
            if (at(TokenType::KW_AS)) advance();
            std::string alias;
            if (at(TokenType::IDENTIFIER)) alias = advance().value;
            if (alias.empty()) {
                // derive alias from filename stem
                auto sl = filepath.find_last_of("/\\");
                alias = (sl == std::string::npos) ? filepath : filepath.substr(sl + 1);
                auto dot = alias.rfind('.');
                if (dot != std::string::npos) alias = alias.substr(0, dot);
            }
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "datac:" + filepath + ":" + alias);
        }

        // Regular use statement (not ilib/elib/clib/flib/datac)
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

        // Make Screen object — screen initialisation; capture optional "resize WxH"
        if (name == "Screen") {
            auto node = std::make_unique<ASTNode>(NodeType::ObjDecl, "Screen");
            std::string dims = "1720x1080"; // default
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                std::string tv = advance().value;
                // "resize" followed by NUMBERxNUMBER e.g. "1720x1080"
                if (tv == "resize" && !at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    std::string d;
                    // collect: NUMBER("1720") + IDENTIFIER("x1080") or similar
                    while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                        d += advance().value;
                    if (!d.empty()) dims = d;
                }
            }
            node->attrs.push_back("resize:" + dims);
            return node;
        }

        // Make X object (generic)
        auto node = std::make_unique<ASTNode>(NodeType::ObjDecl, name);
        while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
        return node;
    }

    NodePtr parseBundle() {
        // bundle ClassName
        //     [private|public] field = value       ← default is public (struct-like)
        //     [private|public] Make method func()  ← once any modifier used, all must be explicit
        std::string name = expect(TokenType::IDENTIFIER, "Expected class name after bundle").value;

        auto node = std::make_unique<ASTNode>(NodeType::BundleDef, name);
        expectIndent();
        bool anyModifier = false;

        struct PendingMember { std::string access; bool hadMod; NodePtr member; };
        std::vector<PendingMember> pending;

        while (!at(TokenType::DEDENT) && !at(TokenType::END_OF_FILE)) {
            skipNewlines();
            if (at(TokenType::DEDENT) || at(TokenType::END_OF_FILE)) break;

            std::string access = "public";
            bool hadMod = false;
            if (at(TokenType::KW_PRIVATE))     { advance(); access = "private"; hadMod = true; anyModifier = true; }
            else if (at(TokenType::KW_PUBLIC)) { advance(); access = "public";  hadMod = true; anyModifier = true; }

            auto member = parseStatement();
            if (!member) { skipNewlines(); continue; }
            pending.push_back({access, hadMod, std::move(member)});
            skipNewlines();
        }
        expectDedent();

        // Validate: once any modifier is used, every member must be explicit
        for (auto& pm : pending) {
            if (anyModifier && !pm.hadMod)
                throw SEMANTIC_ERROR(
                    "Preposterous: bundle '" + name + "' — when using private/public, all members must be explicitly annotated",
                    0, 0);
        }

        for (auto& pm : pending) {
            auto wrapper = std::make_unique<ASTNode>(NodeType::BundleMember, pm.access);
            wrapper->children.push_back(std::move(pm.member));
            node->children.push_back(std::move(wrapper));
        }
        return node;
    }

    NodePtr parseIdentifierStatement() {
        std::string name = advance().value;

        // using header <libname> — flat import; marks namespace for unqualified calls
        if (name == "using" && at(TokenType::IDENTIFIER) && peek().value == "header") {
            advance(); // consume "header"
            std::string libname;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                libname += advance().value;
            auto notSpace = [](char c){ return !std::isspace((unsigned char)c); };
            libname.erase(libname.begin(), std::find_if(libname.begin(), libname.end(), notSpace));
            libname.erase(std::find_if(libname.rbegin(), libname.rend(), notSpace).base(), libname.end());
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:" + libname);
        }
        // using namespace ilib — all imported ilib symbols accessible without prefix
        if (name == "using" && at(TokenType::IDENTIFIER) && peek().value == "namespace") {
            advance(); // consume "namespace"
            std::string ns;
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE))
                ns += advance().value;
            auto notSpace2 = [](char c){ return !std::isspace((unsigned char)c); };
            ns.erase(ns.begin(), std::find_if(ns.begin(), ns.end(), notSpace2));
            ns.erase(std::find_if(ns.rbegin(), ns.rend(), notSpace2).base(), ns.end());
            return std::make_unique<ASTNode>(NodeType::UseLibStmt, "using:namespace:" + ns);
        }

        // Function call without object prefix: jump(Character)
        if (at(TokenType::LPAREN)) {
            advance();
            // Collect args split at top-level commas so each arg is a separate attr
            std::vector<std::string> argList;
            std::string cur;
            int depth = 1;
            while (!at(TokenType::END_OF_FILE) && depth > 0) {
                if (at(TokenType::COMMA) && depth == 1) { advance(); argList.push_back(cur); cur = ""; continue; }
                if (at(TokenType::LPAREN)) { depth++; cur += advance().value; continue; }
                if (at(TokenType::RPAREN)) { depth--; if (depth == 0) break; cur += advance().value; continue; }
                if (at(TokenType::STRING)) { cur += "$" + advance().value + "$"; continue; }
                cur += advance().value;
            }
            argList.push_back(cur);
            if (at(TokenType::RPAREN)) advance();
            auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name);
            for (auto& a : argList) {
                // trim surrounding spaces
                size_t s = a.find_first_not_of(' '), e = a.find_last_not_of(' ');
                if (s != std::string::npos) node->attrs.push_back(a.substr(s, e - s + 1));
            }
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

            // Obj.Name or obj.Name as object declaration
            if (name == "Obj" || name == "obj") {
                auto node = std::make_unique<ASTNode>(NodeType::ObjDecl, "Obj." + prop);
                return node;
            }

            // display/print-style methods: use Pratt parser for the full argument expression
            // (e.g. "Term.display (2+3)*4" must not stop at the inner ")")
            if (prop == "display" || prop == "print" || prop == "log" || prop == "write") {
                auto argExpr = parseExpression(0);
                if (!argExpr) {
                    throw SYNTAX_ERROR(std::string("Expected argument for ") + name + "." + prop, peek().line, peek().col);
                }
                auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
                node->children.push_back(std::move(argExpr));
                return node;
            }

            // method call: Name.method(args)
            if (at(TokenType::LPAREN)) {
                advance();
                // Split at top-level commas so each arg becomes a separate attr
                std::vector<std::string> argList;
                std::string cur;
                int depth = 1;
                while (!at(TokenType::END_OF_FILE) && depth > 0) {
                    if (at(TokenType::COMMA) && depth == 1) { advance(); argList.push_back(cur); cur = ""; continue; }
                    if (at(TokenType::LPAREN)) { depth++; cur += advance().value; continue; }
                    if (at(TokenType::RPAREN)) { depth--; if (depth == 0) break; cur += advance().value; continue; }
                    if (at(TokenType::STRING)) { cur += "$" + advance().value + "$"; continue; }
                    cur += advance().value;
                }
                argList.push_back(cur);
                if (at(TokenType::RPAREN)) advance();
                auto node = std::make_unique<ASTNode>(NodeType::MethodCall, name + "." + prop);
                for (auto& a : argList) {
                    size_t s = a.find_first_not_of(' '), e = a.find_last_not_of(' ');
                    if (s != std::string::npos) {
                        std::string val = a.substr(s, e - s + 1);
                        // curveshape arg is a math formula — pass as string literal
                        if (prop == "curveshape") node->attrs.push_back("$" + val + "$");
                        else node->attrs.push_back(val);
                    }
                }
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

            // config: Name.config key=val - key=val - keyword
            // attrs separated by " - " (dash at paren depth 0)
            if (prop == "config") {
                auto node = std::make_unique<ASTNode>(NodeType::ConfigCall, name);
                std::string cur;
                int depth = 0;
                auto pushAttr = [&]() {
                    auto s = cur.find_first_not_of(' '), e = cur.find_last_not_of(' ');
                    if (s != std::string::npos) node->attrs.push_back(cur.substr(s, e-s+1));
                    cur.clear();
                };
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    if (at(TokenType::LPAREN))  { depth++; cur += "("; advance(); continue; }
                    if (at(TokenType::RPAREN))  { depth--; cur += ")"; advance(); continue; }
                    if (at(TokenType::ASSIGN))  { cur += "="; advance(); continue; }
                    if (at(TokenType::COMMA))   { cur += ","; advance(); continue; }
                    if (at(TokenType::STRING))  {
                        std::string sv = advance().value;
                        if (!cur.empty() && cur.back() != '(' && cur.back() != '=') cur += " ";
                        cur += "$" + sv + "$"; continue;
                    }
                    std::string tv = advance().value;
                    if (tv == "-" && depth == 0) { pushAttr(); continue; } // attr separator
                    if (!cur.empty() && cur.back() != '(' && cur.back() != '=') cur += " ";
                    cur += tv;
                }
                pushAttr();
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

            // compound assignment on chained prop: Name.prop.sub /= val, *= val, etc.
            if (at(TokenType::DIVIDE_EQUAL) || at(TokenType::MULTIPLY_EQUAL) ||
                at(TokenType::PLUS_EQUAL)   || at(TokenType::MINUS_EQUAL)) {
                std::string op = advance().value; // "/=", "*=", etc.
                auto rhsExpr = parseExpression(0);
                auto node = std::make_unique<ASTNode>(NodeType::PropAssign, name + "." + prop);
                node->attrs.push_back(op); // store compound op
                if (rhsExpr) node->children.push_back(std::move(rhsExpr));
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
        if (at(TokenType::PLUS_EQUAL) || at(TokenType::MINUS_EQUAL) || at(TokenType::MULTIPLY_EQUAL) || at(TokenType::DIVIDE_EQUAL) || at(TokenType::AT_EQUAL) || at(TokenType::PIPE_EQUAL)) {
            TokenType opType = advance().type;
            NodeType nodeType;
            switch (opType) {
                case TokenType::PLUS_EQUAL: nodeType = NodeType::PlusEqualStmt; break;
                case TokenType::MINUS_EQUAL: nodeType = NodeType::MinusEqualStmt; break;
                case TokenType::MULTIPLY_EQUAL: nodeType = NodeType::MultiplyEqualStmt; break;
                case TokenType::DIVIDE_EQUAL: nodeType = NodeType::DivideEqualStmt; break;
                case TokenType::AT_EQUAL: nodeType = NodeType::AtEqualStmt; break;
                case TokenType::PIPE_EQUAL: nodeType = NodeType::XorEqualStmt; break;
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
            if (at(TokenType::IDENTIFIER) && peekAhead(1).type == TokenType::LPAREN) {
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
                        // Positional argument — track nesting depth so inner parens are included
                        std::string val;
                        int argDepth = 0;
                        while (!at(TokenType::END_OF_FILE)) {
                            if (at(TokenType::LPAREN))  { argDepth++; val += advance().value; continue; }
                            if (at(TokenType::RPAREN)) {
                                if (argDepth > 0) { argDepth--; val += advance().value; continue; }
                                break; // depth 0 RPAREN = end of outer arg list
                            }
                            if (at(TokenType::COMMA) && argDepth == 0) break;
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
            
            // dict-{ key:value\n... } syntax
            if (at(TokenType::IDENTIFIER) && peek().value == "dict" &&
                peekAhead(1).type == TokenType::IDENTIFIER && peekAhead(1).value == "-" &&
                peekAhead(2).type == TokenType::LBRACE) {
                advance(); advance(); // consume "dict" and "-"
                auto dictNode = parseTupleLiteral();
                auto assignNode = std::make_unique<ASTNode>(NodeType::AssignStmt, name);
                assignNode->attrs.push_back("__dict__" + dictNode->value);
                return assignNode;
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

        // "Name at <rate>" — animation rate/direction hint, skip as no-op
        if (at(TokenType::KW_AT)) {
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) advance();
            return nullptr;
        }

        // Bare identifier - unidentified syntax, throw error
        Token& t = (pos > 0) ? tokens[pos - 1] : tokens[0];  // Safe previous token access
        throw SYNTAX_ERROR("Unidentified syntax detected", t.line, t.col);
    }
};

// Global storage for parse errors from the last parse() call.
// main.cpp reads this after calling parse().
struct ParseErrorRecord {
    int line, col;
    std::string message, context;
};
std::vector<ParseErrorRecord> g_parseErrors;

NodePtr parse(const std::vector<Token>& tokens) {
    g_parseErrors.clear();
    Parser p(tokens);
    auto ast = p.parse();
    for (const auto& e : p.getErrors()) {
        g_parseErrors.push_back({e.line, e.col, e.message, e.context});
    }
    return ast;
}
