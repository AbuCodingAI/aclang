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
            // Skip "event-listener" tokens without calling parseStatement
            // Consume: event, -, listener
            while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                advance();
            }
            skipNewlines();
            
            auto node = std::make_unique<ASTNode>(NodeType::EventListener);
            
            // Parse the nested block: use listener to establish rule
            if (at(TokenType::INDENT)) {
                advance();
                // Skip "use listener to establish rule" line
                while (!at(TokenType::NEWLINE) && !at(TokenType::END_OF_FILE)) {
                    advance();
                }
                skipNewlines();
                
                // Skip the INDENT token before the key bindings
                if (at(TokenType::INDENT)) {
                    advance();
                }
                
                // Now parse the key bindings - directly handle them here
                while (at(TokenType::KW_ON) && !at(TokenType::DEDENT)) {
                    advance(); // consume ON
                    std::string key;
                    if (at(TokenType::KW_VALUE)) {
                        advance();
                        if (at(TokenType::ASSIGN)) advance();
                        if (at(TokenType::IDENTIFIER)) key = advance().value;
                    }
                    skipNewlines();
                    auto keyBinding = std::make_unique<ASTNode>(NodeType::KeyBinding, key);
                    
                    // Parse the function call on the next line (indented)
                    if (at(TokenType::INDENT)) {
                        advance();
                        skipNewlines();
                        if (at(TokenType::IDENTIFIER)) {
                            std::string funcName = advance().value;
                            std::string args;
                            if (at(TokenType::LPAREN)) {
                                advance();
                                while (!at(TokenType::RPAREN) && !at(TokenType::END_OF_FILE)) {
                                    args += advance().value;
                                }
                                if (at(TokenType::RPAREN)) advance();
                            }
                            keyBinding->attrs.push_back(funcName + "(" + args + ")");
                        }
                        skipNewlines();
                        if (at(TokenType::DEDENT)) advance();
                    }
                    node->children.push_back(std::move(keyBinding));
                    skipNewlines();
                }
                
                if (at(TokenType::DEDENT)) advance();
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

        // Bare identifier - unidentified syntax, throw error
        auto t = tokens[pos - 1];  // Get the identifier token we just consumed
        throw SYNTAX_ERROR("Unidentified syntax detected", t.line, t.col);
    }
};

NodePtr parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse();
}
