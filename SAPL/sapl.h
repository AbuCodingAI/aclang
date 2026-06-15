/**
 * SAPL - Standard APL
 * C++ Interpreter/Parser Implementation
 * 
 * APL-inspired, ASCII-friendly array-oriented language
 */

#ifndef SAPL_H
#define SAPL_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <numeric>
#include <functional>
#include <cstdint>

namespace SAPL {

// Forward declarations
class Value;
class Environment;
class ASTNode;

// ============================================================================
// VALUE TYPES
// ============================================================================

/**
 * Value types supported by SAPL
 * - Numbers (double for decimal support)
 * - Arrays (vectors of values)
 * - Strings
 * - Booleans
 * - Dictionaries
 */
enum class ValueType {
    NUMBER,
    ARRAY,
    STRING,
    BOOLEAN,
    DICTIONARY,
    NIL
};

/**
 * Main Value class representing SAPL runtime values
 * Supports automatic broadcasting and array operations
 */
class Value {
public:
    ValueType type;
    double number;
    std::string str;
    std::vector<Value> arr;
    std::map<std::string, Value> dict;
    bool boolean;

    Value() : type(ValueType::NIL), number(0), boolean(false) {}
    
    Value(double n) : type(ValueType::NUMBER), number(n), boolean(false) {}
    
    Value(const std::string& s) : type(ValueType::STRING), str(s), number(0), boolean(false) {}
    
    Value(bool b) : type(ValueType::BOOLEAN), boolean(b), number(0) {}
    
    Value(const std::vector<Value>& a) : type(ValueType::ARRAY), arr(a), number(0), boolean(false) {}
    
    Value(const std::map<std::string, Value>& d) : type(ValueType::DICTIONARY), dict(d), number(0), boolean(false) {}

    static Value makeArray(const std::vector<Value>& v) {
        return Value(v);
    }

    bool isNumber() const { return type == ValueType::NUMBER; }
    bool isArray() const { return type == ValueType::ARRAY; }
    bool isString() const { return type == ValueType::STRING; }
    bool isBoolean() const { return type == ValueType::BOOLEAN; }
    bool isDictionary() const { return type == ValueType::DICTIONARY; }
    bool isNil() const { return type == ValueType::NIL; }

    /** Get the shape/dimensions of the value */
    Value shape() const {
        if (type == ValueType::ARRAY) {
            return Value(static_cast<double>(arr.size()));
        }
        // Scalars have no shape
        return Value();
    }

    /** Convert to string representation */
    std::string toString() const {
        std::ostringstream oss;
        switch (type) {
            case ValueType::NUMBER:
                if (number == static_cast<int64_t>(number)) {
                    oss << static_cast<int64_t>(number);
                } else {
                    oss << number;
                }
                break;
            case ValueType::STRING:
                oss << str;
                break;
            case ValueType::BOOLEAN:
                oss << (boolean ? "true" : "false");
                break;
            case ValueType::ARRAY:
                oss << "[";
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) oss << " ";
                    oss << arr[i].toString();
                }
                oss << "]";
                break;
            case ValueType::DICTIONARY:
                oss << "{";
                for (auto it = dict.begin(); it != dict.end(); ++it) {
                    if (it != dict.begin()) oss << ", ";
                    oss << "\"" << it->first << "\": " << it->second.toString();
                }
                oss << "}";
                break;
            case ValueType::NIL:
                oss << "nil";
                break;
        }
        return oss.str();
    }
};

// ============================================================================
// TOKENS
// ============================================================================

enum class TokenType {
    // Literals
    NUMBER,
    STRING,
    IDENTIFIER,
    
    // Keywords
    E,          // Sum reduction
    PI,         // Product reduction
    I,          // Interval/Range
    REV,        // Reverse
    T,          // Transpose
    PW,         // Shape
    ASC,        // Ascending grade
    DESC,       // Descending grade
    CMP,        // Compare
    ECHO,       // Echo/bind
    DICT,       // Dictionary declaration
    // FN removed — AC syntax not SAPL
    
    // Operators
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    PERCENT,    // %
    CARET,      // ^ (power)
    LT,         // <
    GT,         // >
    LTE,        // <=
    GTE,        // >=
    EQ,         // = (assignment)
    NEQ,        // !=
    ARROW,      // <- (output)
    
    // Delimiters
    LPAREN,     // (
    RPAREN,     // )
    LBRACKET,   // [
    RBRACKET,   // ]
    LBRACE,     // {
    RBRACE,     // }
    COMMA,      // ,
    COLON,      // :
    SEMICOLON,  // ; (comment)
    DOT,        // .
    
    // Special
    NEWLINE,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    
    Token(TokenType t, const std::string& l, int ln, int col)
        : type(t), lexeme(l), line(ln), column(col) {}
};

// ============================================================================
// LEXER
// ============================================================================

class Lexer {
public:
    Lexer(const std::string& source) : source(source), pos(0), line(1), column(1) {}
    
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        
        while (!isAtEnd()) {
            skipWhitespace();
            if (isAtEnd()) break;
            
            lineStart = column;
            Token token = scanToken();
            tokens.push_back(token);
        }
        
        tokens.emplace_back(TokenType::END_OF_FILE, "", line, column);
        return tokens;
    }
    
private:
    std::string source;
    size_t pos;
    int line;
    int column;
    int lineStart;
    
    bool isAtEnd() const { return pos >= source.length(); }
    
    char peek() const {
        if (isAtEnd()) return '\0';
        return source[pos];
    }
    
    char peekNext() const {
        if (pos + 1 >= source.length()) return '\0';
        return source[pos + 1];
    }
    
    char advance() {
        if (isAtEnd()) return '\0';
        char c = source[pos++];
        column++;
        return c;
    }
    
    void skipWhitespace() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
            } else if (c == ';') {
                // Comment - skip to end of line
                while (!isAtEnd() && peek() != '\n') {
                    advance();
                }
            } else {
                break;
            }
        }
    }
    
    Token scanToken() {
        int startLine = line;
        int startCol = column;
        
        char c = advance();

        // Newline
        if (c == '\n') {
            line++;
            column = 1;
            return Token(TokenType::NEWLINE, "\n", startLine, startCol);
        }
        
        // Number
        if (std::isdigit(c) || (c == '.' && std::isdigit(peekNext()))) {
            return scanNumber(startLine, startCol);
        }
        
        // String
        if (c == '"') {
            return scanString(startLine, startCol);
        }
        
        // Identifier or keyword
        if (std::isalpha(c) || c == '_') {
            return scanIdentifier(startLine, startCol);
        }
        
        // Operators and delimiters
        switch (c) {
            case '+': return Token(TokenType::PLUS, "+", startLine, startCol);
            case '-': return Token(TokenType::MINUS, "-", startLine, startCol);
            case '*': return Token(TokenType::STAR, "*", startLine, startCol);
            case '/': return Token(TokenType::SLASH, "/", startLine, startCol);
            case '%': return Token(TokenType::PERCENT, "%", startLine, startCol);
            case '^': return Token(TokenType::CARET, "^", startLine, startCol);
            case '(': return Token(TokenType::LPAREN, "(", startLine, startCol);
            case ')': return Token(TokenType::RPAREN, ")", startLine, startCol);
            case '[': return Token(TokenType::LBRACKET, "[", startLine, startCol);
            case ']': return Token(TokenType::RBRACKET, "]", startLine, startCol);
            case '{': return Token(TokenType::LBRACE, "{", startLine, startCol);
            case '}': return Token(TokenType::RBRACE, "}", startLine, startCol);
            case ',': return Token(TokenType::COMMA, ",", startLine, startCol);
            case ':': return Token(TokenType::COLON, ":", startLine, startCol);
            case '.': return Token(TokenType::DOT, ".", startLine, startCol);
            case '<':
                if (peek() == '-') {
                    advance();
                    return Token(TokenType::ARROW, "<-", startLine, startCol);
                }
                return Token(TokenType::LT, "<", startLine, startCol);
            case '>':
                if (peek() == '=') {
                    advance();
                    return Token(TokenType::GTE, ">=", startLine, startCol);
                }
                return Token(TokenType::GT, ">", startLine, startCol);
            case '=':
                if (peek() == '=') {
                    advance();
                    return Token(TokenType::EQ, "==", startLine, startCol);
                }
                return Token(TokenType::EQ, "=", startLine, startCol);
            case '!':
                if (peek() == '=') {
                    advance();
                    return Token(TokenType::NEQ, "!=", startLine, startCol);
                }
                throw std::runtime_error("Expected '=' after '!'");
        }
        
        throw std::runtime_error("Unexpected character: " + std::string(1, c));
    }
    
    Token scanNumber(int startLine, int startCol) {
        std::string num;
        num += source[pos - 1];
        
        while (!isAtEnd() && (std::isdigit(peek()) || peek() == '.')) {
            num += advance();
        }
        
        return Token(TokenType::NUMBER, num, startLine, startCol);
    }
    
    Token scanString(int startLine, int startCol) {
        std::string str;
        
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\\' && peekNext() != '\0') {
                advance();
                char escape = advance();
                switch (escape) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case '"': str += '"'; break;
                    case '\\': str += '\\'; break;
                    default: str += escape; break;
                }
            } else {
                str += advance();
            }
        }
        
        if (isAtEnd()) {
            throw std::runtime_error("Unterminated string");
        }
        
        advance(); // closing "
        return Token(TokenType::STRING, str, startLine, startCol);
    }
    
    Token scanIdentifier(int startLine, int startCol) {
        std::string ident;
        ident += source[pos - 1];
        
        while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
            ident += advance();
        }
        
        // Check for keywords
        TokenType type = keywordType(ident);
        return Token(type, ident, startLine, startCol);
    }
    
    TokenType keywordType(const std::string& ident) {
        static const std::map<std::string, TokenType> keywords = {
            {"E", TokenType::E},
            {"PI", TokenType::PI},
            {"I", TokenType::I},
            {"rev", TokenType::REV},
            {"T", TokenType::T},
            {"pw", TokenType::PW},
            {"asc", TokenType::ASC},
            {"desc", TokenType::DESC},
            {"cmp", TokenType::CMP},
            {"echo", TokenType::ECHO},
            {"dict", TokenType::DICT},
            // fn removed — AC syntax, not SAPL
            {"loopu", TokenType::IDENTIFIER} // loopu is treated as identifier
        };
        
        auto it = keywords.find(ident);
        if (it != keywords.end()) {
            return it->second;
        }
        
        return TokenType::IDENTIFIER;
    }
};

// ============================================================================
// AST NODES
// ============================================================================

enum class ASTNodeType {
    PROGRAM,
    FUNCTION_DEF,
    FUNCTION_CALL,
    ASSIGNMENT,
    OUTPUT,
    NUMBER_LITERAL,
    STRING_LITERAL,
    ARRAY_LITERAL,
    DICTIONARY_LITERAL,
    IDENTIFIER,
    BINARY_OP,
    UNARY_OP,
    BLOCK,
    CONDITIONAL,
    LOOP,
    DICTIONARY_DEF,
    ECHO_STMT
};

struct ASTNode {
    ASTNodeType type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;
    int line;
    int column;
    
    ASTNode(ASTNodeType t, const std::string& v = "", int ln = 0, int col = 0)
        : type(t), value(v), line(ln), column(col) {}
    
    void addChild(std::shared_ptr<ASTNode> child) {
        children.push_back(child);
    }
};

// ============================================================================
// PARSER
// ============================================================================

class Parser {
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}
    
    std::shared_ptr<ASTNode> parse() {
        auto program = std::make_shared<ASTNode>(ASTNodeType::PROGRAM);
        
        while (!isAtEnd()) {
            skipNewlines();
            if (isAtEnd()) break;
            auto stmt = parseStatement();
            if (stmt) {
                program->addChild(stmt);
            }
            skipNewlines();
        }
        
        return program;
    }
    
private:
    std::vector<Token> tokens;
    size_t pos;
    
    bool isAtEnd() const { return pos >= tokens.size() || tokens[pos].type == TokenType::END_OF_FILE; }
    
    Token peek() const {
        if (isAtEnd()) return Token(TokenType::END_OF_FILE, "", 0, 0);
        return tokens[pos];
    }
    
    Token peekNext() const {
        if (pos + 1 >= tokens.size()) return Token(TokenType::END_OF_FILE, "", 0, 0);
        return tokens[pos + 1];
    }
    
    Token advance() {
        if (!isAtEnd()) return tokens[pos++];
        return Token(TokenType::END_OF_FILE, "", 0, 0);
    }
    
    bool check(TokenType type) const {
        return !isAtEnd() && tokens[pos].type == type;
    }
    
    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    void skipNewlines() {
        while (match(TokenType::NEWLINE)) {
        }
    }
    
    std::shared_ptr<ASTNode> parseStatement() {
        if (isAtEnd()) return nullptr;
        
        int line = peek().line;
        int col = peek().column;
        
        // Dictionary definition
        if (check(TokenType::DICT)) {
            return parseDictionaryDef();
        }
        
        // Function definition: name(args) = expr or name(args) = { ... }
        if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::LPAREN && isFunctionDefinitionStart()) {
            return parseFunctionDef();
        }
        
        // Echo statement
        if (check(TokenType::ECHO)) {
            return parseEchoStatement();
        }
        
        // Output statement: 0<- expr
        if (check(TokenType::NUMBER) && peek().lexeme == "0" && peekNext().type == TokenType::ARROW) {
            return parseOutputStatement();
        }
        
        // Assignment: identifier = expr
        if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::EQ) {
            return parseAssignment();
        }
        
        // Expression statement
        return parseExpression();
    }

    bool isFunctionDefinitionStart() const {
        if (!(check(TokenType::IDENTIFIER) && peekNext().type == TokenType::LPAREN)) {
            return false;
        }

        int paren_depth = 0;
        size_t i = pos;
        while (i < tokens.size() && tokens[i].type != TokenType::END_OF_FILE) {
            TokenType t = tokens[i].type;
            if (t == TokenType::LPAREN) paren_depth++;
            if (t == TokenType::RPAREN) {
                paren_depth--;
                if (paren_depth == 0) {
                    i++;
                    while (i < tokens.size() && tokens[i].type == TokenType::NEWLINE) i++;
                    return i < tokens.size() && tokens[i].type == TokenType::EQ;
                }
            }
            i++;
        }

        return false;
    }
    
    std::shared_ptr<ASTNode> parseFunctionDef() {
        int line = peek().line;
        int col = peek().column;
        
        std::string name = advance().lexeme;
        advance(); // (
        
        std::vector<std::string> params;
        if (!check(TokenType::RPAREN)) {
            do {
                if (check(TokenType::IDENTIFIER)) {
                    params.push_back(advance().lexeme);
                }
            } while (match(TokenType::COMMA));
        }
        advance(); // )
        
        if (!match(TokenType::EQ)) {
            throw std::runtime_error("Expected '=' in function definition");
        }
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::FUNCTION_DEF, name, line, col);
        
        // Parameters
        auto paramsNode = std::make_shared<ASTNode>(ASTNodeType::ARRAY_LITERAL);
        for (const auto& param : params) {
            paramsNode->addChild(std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, param));
        }
        node->addChild(paramsNode);
        
        // Check if it's a block or expression
        if (check(TokenType::LBRACE)) {
            node->addChild(parseBlock());
        } else {
            node->addChild(parseExpression());
        }
        
        return node;
    }
    
    std::shared_ptr<ASTNode> parseDictionaryDef() {
        int line = advance().line; // consume 'dict'
        int col = peek().column;
        
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected dictionary name");
        }
        std::string name = advance().lexeme;
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::DICTIONARY_DEF, name, line, col);
        
        if (!match(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' in dictionary definition");
        }
        
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (check(TokenType::IDENTIFIER) || check(TokenType::STRING) || check(TokenType::NUMBER)) {
                std::string key;
                if (check(TokenType::NUMBER)) {
                    key = advance().lexeme;
                } else if (check(TokenType::STRING)) {
                    key = advance().lexeme;
                } else {
                    key = advance().lexeme;
                }
                
                if (!match(TokenType::COLON)) {
                    throw std::runtime_error("Expected ':' in dictionary entry");
                }
                
                auto value = parseExpression();
                
                auto entry = std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, key);
                entry->addChild(value);
                node->addChild(entry);
                
                match(TokenType::COMMA);
            } else {
                advance();
            }
        }
        
        advance(); // }
        return node;
    }
    
    std::shared_ptr<ASTNode> parseEchoStatement() {
        int line = advance().line; // consume 'echo'
        int col = peek().column;
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::ECHO_STMT, "echo", line, col);
        
        // echo can have: identifier=expr or just expr
        if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::EQ) {
            std::string name = advance().lexeme;
            advance(); // =
            node->value = name;
            node->addChild(parseExpression());
        } else {
            node->addChild(parseExpression());
        }
        
        return node;
    }
    
    std::shared_ptr<ASTNode> parseOutputStatement() {
        int line = peek().line;
        int col = peek().column;
        
        advance(); // 0
        advance(); // <-
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::OUTPUT, "<-", line, col);
        node->addChild(parseExpression());
        
        return node;
    }
    
    std::shared_ptr<ASTNode> parseAssignment() {
        int line = peek().line;
        int col = peek().column;
        
        std::string name = advance().lexeme;
        advance(); // =
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::ASSIGNMENT, name, line, col);
        node->addChild(parseExpression());
        
        return node;
    }
    
    std::shared_ptr<ASTNode> parseBlock() {
        int line = peek().line;
        int col = peek().column;
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::BLOCK, "", line, col);
        
        advance(); // {
        
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            skipNewlines();
            if (check(TokenType::RBRACE) || isAtEnd()) break;
            auto stmt = parseStatement();
            if (stmt) {
                node->addChild(stmt);
            }
            skipNewlines();
        }
        
        advance(); // }
        return node;
    }
    
    std::shared_ptr<ASTNode> parseExpression() {
        return parseConditional();
    }
    
    std::shared_ptr<ASTNode> parseConditional() {
        // Check for not: as a standalone else branch
        // This is only valid inside a conditional block context
        // handled at the block level — see parseBlock for chaining

        auto left = parseComparison();

        if (check(TokenType::COLON)) {
            advance(); // :
            skipNewlines();
            auto trueBranch = parseExpression();

            auto condNode = std::make_shared<ASTNode>(ASTNodeType::CONDITIONAL, "?:", left->line, left->column);
            condNode->addChild(left);
            condNode->addChild(trueBranch);

            // Optional comma-separated false branch OR not: keyword
            if (match(TokenType::COMMA)) {
                skipNewlines();
                // check for not: 
                if (check(TokenType::IDENTIFIER) && peek().lexeme == "not" ) {
                    advance(); // not
                    if (match(TokenType::COLON)) {
                        skipNewlines();
                        auto falseBranch = parseExpression();
                        condNode->addChild(falseBranch);
                    }
                } else {
                    auto falseBranch = parseExpression();
                    condNode->addChild(falseBranch);
                }
            }

            return condNode;
        }

        return left;
    }
    
    std::shared_ptr<ASTNode> parseComparison() {
        auto left = parseAdditive();
        
        while (check(TokenType::LT) || check(TokenType::GT) || 
               check(TokenType::LTE) || check(TokenType::GTE) ||
               check(TokenType::CMP)) {
            TokenType op = advance().type;
            auto right = parseAdditive();
            
            std::string opStr;
            switch (op) {
                case TokenType::LT: opStr = "<"; break;
                case TokenType::GT: opStr = ">"; break;
                case TokenType::LTE: opStr = "<="; break;
                case TokenType::GTE: opStr = ">="; break;
                case TokenType::CMP: opStr = "cmp"; break;
                default: opStr = "?";
            }
            
            auto node = std::make_shared<ASTNode>(ASTNodeType::BINARY_OP, opStr, left->line, left->column);
            node->addChild(left);
            node->addChild(right);
            left = node;
        }
        
        return left;
    }
    
    std::shared_ptr<ASTNode> parseAdditive() {
        auto left = parseMultiplicative();
        
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            TokenType op = advance().type;
            auto right = parseMultiplicative();
            
            auto node = std::make_shared<ASTNode>(ASTNodeType::BINARY_OP, 
                op == TokenType::PLUS ? "+" : "-", 
                left->line, left->column);
            node->addChild(left);
            node->addChild(right);
            left = node;
        }
        
        return left;
    }
    
    std::shared_ptr<ASTNode> parseMultiplicative() {
        auto left = parseUnary();
        
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
            TokenType op = advance().type;
            auto right = parseUnary();
            
            std::string opStr;
            switch (op) {
                case TokenType::STAR: opStr = "*"; break;
                case TokenType::SLASH: opStr = "/"; break;
                case TokenType::PERCENT: opStr = "%"; break;
                default: opStr = "?";
            }
            
            auto node = std::make_shared<ASTNode>(ASTNodeType::BINARY_OP, opStr, left->line, left->column);
            node->addChild(left);
            node->addChild(right);
            left = node;
        }
        
        return left;
    }
    
    std::shared_ptr<ASTNode> parseUnary() {
        if (check(TokenType::MINUS)) {
            advance();
            auto operand = parseUnary();
            auto node = std::make_shared<ASTNode>(ASTNodeType::UNARY_OP, "-", operand->line, operand->column);
            node->addChild(operand);
            return node;
        }
        
        return parsePower();
    }
    
    std::shared_ptr<ASTNode> parsePower() {
        auto left = parsePrimary();
        
        if (check(TokenType::CARET)) {
            advance();
            auto right = parseUnary(); // Right associative
            
            auto node = std::make_shared<ASTNode>(ASTNodeType::BINARY_OP, "^", left->line, left->column);
            node->addChild(left);
            node->addChild(right);
            return node;
        }
        
        return left;
    }
    
    std::shared_ptr<ASTNode> parsePrimary() {
        if (check(TokenType::NUMBER)) {
            auto node = std::make_shared<ASTNode>(ASTNodeType::NUMBER_LITERAL, advance().lexeme);
            return node;
        }
        
        if (check(TokenType::STRING)) {
            auto node = std::make_shared<ASTNode>(ASTNodeType::STRING_LITERAL, advance().lexeme);
            return node;
        }
        
        // Array literal: [ ... ]
        if (check(TokenType::LBRACKET)) {
            return parseArrayLiteral();
        }
        
        // Function call or identifier
        if (check(TokenType::IDENTIFIER)) {
            return parseCallOrIdentifier();
        }
        
        // SAPL operators
        if (check(TokenType::E) || check(TokenType::PI) || check(TokenType::I) ||
            check(TokenType::REV) || check(TokenType::T) || check(TokenType::PW) ||
            check(TokenType::ASC) || check(TokenType::DESC)) {
            return parseSAPOperator();
        }
        
        // Grouped expression
        if (check(TokenType::LPAREN)) {
            advance();
            auto expr = parseExpression();
            if (!match(TokenType::RPAREN)) {
                throw std::runtime_error("Expected ')'");
            }
            return expr;
        }
        
        throw std::runtime_error("Unexpected token: " + peek().lexeme);
    }
    
    std::shared_ptr<ASTNode> parseArrayLiteral() {
        int line = peek().line;
        int col = peek().column;
        
        advance(); // [
        
        auto node = std::make_shared<ASTNode>(ASTNodeType::ARRAY_LITERAL, "", line, col);
        
        while (!check(TokenType::RBRACKET) && !isAtEnd()) {
            auto elem = parseExpression();
            node->addChild(elem);
            
            if (!match(TokenType::COMMA)) {
                // Space-separated is also ok
                if (check(TokenType::RBRACKET)) break;
            }
        }
        
        advance(); // ]
        return node;
    }
    
    std::shared_ptr<ASTNode> parseCallOrIdentifier() {
        int line = peek().line;
        int col = peek().column;
        std::string name = advance().lexeme;
        
        // Check for function call
        if (check(TokenType::LPAREN)) {
            advance(); // (
            
            auto node = std::make_shared<ASTNode>(ASTNodeType::FUNCTION_CALL, name, line, col);
            
            if (!check(TokenType::RPAREN)) {
                do {
                    auto arg = parseExpression();
                    node->addChild(arg);
                } while (match(TokenType::COMMA));
            }
            
            advance(); // )
            return node;
        }
        
        // Check for loopu/loopf (loop constructs)
        if (name == "loopu") {
            auto node = std::make_shared<ASTNode>(ASTNodeType::LOOP, "loopu", line, col);
            // Parse condition (everything up to the {)
            auto cond = parseExpression();
            node->addChild(cond);
            // Parse body block
            if (check(TokenType::LBRACE)) {
                node->addChild(parseBlock());
            }
            return node;
        }
        if (name == "loopf") {
            auto node = std::make_shared<ASTNode>(ASTNodeType::LOOP, "loopf", line, col);
            // loopf takes only a body block, no condition
            if (check(TokenType::LBRACE)) {
                node->addChild(parseBlock());
            }
            return node;
        }
        
        // Plain identifier
        return std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, name, line, col);
    }
    
    std::shared_ptr<ASTNode> parseSAPOperator() {
        int line = peek().line;
        int col = peek().column;
        
        std::string op = advance().lexeme;
        auto node = std::make_shared<ASTNode>(ASTNodeType::FUNCTION_CALL, op, line, col);
        
        // These operators take arguments
        // E, PI take one argument
        // I takes two arguments
        // rev, T, pw, asc, desc take one argument
        
        if (!isAtEnd() && !check(TokenType::NEWLINE) && !check(TokenType::RBRACE) && 
            !check(TokenType::RBRACKET) && !check(TokenType::COMMA)) {
            
            if (op == "I") {
                // I takes two arguments, possibly separated by comma
                auto arg1 = parsePrimary();
                node->addChild(arg1);
                
                if (match(TokenType::COMMA) || (!check(TokenType::RBRACE) && !check(TokenType::RBRACKET) && !check(TokenType::NEWLINE))) {
                    if (!isAtEnd() && !check(TokenType::RBRACE) && !check(TokenType::RBRACKET) && !check(TokenType::COMMA)) {
                        auto arg2 = parsePrimary();
                        node->addChild(arg2);
                    }
                }
            } else {
                // Single argument operators
                auto arg = parsePrimary();
                node->addChild(arg);
            }
        }
        
        return node;
    }
};

// ============================================================================
// ENVIRONMENT
// ============================================================================

class Environment {
public:
    std::map<std::string, Value> variables;
    std::map<std::string, std::pair<std::vector<std::string>, std::shared_ptr<ASTNode>>> functions;
    Environment* parent;
    
    Environment(Environment* p = nullptr) : parent(p) {}
    
    void define(const std::string& name, const Value& value) {
        variables[name] = value;
    }
    
    Value get(const std::string& name) {
        if (variables.find(name) != variables.end()) {
            return variables[name];
        }
        if (parent) {
            return parent->get(name);
        }
        throw std::runtime_error("Preposterous: UndefinedVariable — " + name);
    }
    
    bool exists(const std::string& name) {
        if (variables.find(name) != variables.end()) {
            return true;
        }
        if (parent) {
            return parent->exists(name);
        }
        return false;
    }
    
    void assign(const std::string& name, const Value& value) {
        if (variables.find(name) != variables.end()) {
            variables[name] = value;
            return;
        }
        if (parent) {
            parent->assign(name, value);
            return;
        }
        // Not found anywhere — define locally
        variables[name] = value;
    }

    void defineFunction(const std::string& name, const std::vector<std::string>& params, std::shared_ptr<ASTNode> body) {
        functions[name] = std::make_pair(params, body);
    }
    
    bool getFunction(const std::string& name, std::vector<std::string>& params, std::shared_ptr<ASTNode>& body) {
        auto it = functions.find(name);
        if (it != functions.end()) {
            params = it->second.first;
            body = it->second.second;
            return true;
        }
        if (parent) {
            return parent->getFunction(name, params, body);
        }
        return false;
    }
};

// ============================================================================
// INTERPRETER
// ============================================================================

class Interpreter {
public:
    Interpreter() : globalEnv(nullptr) {
        // Initialize built-in functions
        setupBuiltins();
    }
    
    Value execute(std::shared_ptr<ASTNode> program) {
        return executeNode(program, &globalEnv);
    }
    
private:
    Environment globalEnv;
    Value lastOutput;
    
    void setupBuiltins() {
        // Built-in functions are handled specially in evaluation
    }
    
    Value executeNode(std::shared_ptr<ASTNode> node, Environment* env) {
        if (!node) return Value();
        
        switch (node->type) {
            case ASTNodeType::PROGRAM: {
                Value result;
                for (const auto& child : node->children) {
                    result = executeNode(child, env);
                }
                return result;
            }
            
            case ASTNodeType::FUNCTION_DEF: {
                std::vector<std::string> params;
                if (node->children.size() > 0 && node->children[0]->type == ASTNodeType::ARRAY_LITERAL) {
                    for (const auto& param : node->children[0]->children) {
                        params.push_back(param->value);
                    }
                }
                auto body = node->children[1];
                env->defineFunction(node->value, params, body);
                return Value();
            }
            
            case ASTNodeType::DICTIONARY_DEF: {
                std::map<std::string, Value> dict;
                for (const auto& entry : node->children) {
                    std::string key = entry->value;
                    Value val = executeNode(entry->children[0], env);
                    dict[key] = val;
                }
                env->define(node->value, Value(dict));
                return Value(dict);
            }
            
            case ASTNodeType::ECHO_STMT: {
                if (node->children.size() > 0) {
                    Value val = executeNode(node->children[0], env);
                    if (node->value != "echo") {
                        // Named echo: echo name=expr
                        env->define(node->value, val);
                        std::cout << val.toString() << std::endl;
                    } else {
                        std::cout << val.toString() << std::endl;
                    }
                }
                return Value();
            }
            
            case ASTNodeType::OUTPUT: {
                if (node->children.size() > 0) {
                    Value val = executeNode(node->children[0], env);
                    std::cout << val.toString() << std::endl;
                    lastOutput = val;
                }
                return Value();
            }
            
            case ASTNodeType::ASSIGNMENT: {
                if (node->children.size() > 0) {
                    Value val = executeNode(node->children[0], env);
                    env->assign(node->value, val);
                    return val;
                }
                return Value();
            }
            
            case ASTNodeType::BLOCK: {
                Environment blockEnv(env);
                Value result;
                for (const auto& child : node->children) {
                    result = executeNode(child, &blockEnv);
                }
                return result;
            }
            
            case ASTNodeType::LOOP: {
                // loopu condition { body }
                // children[0] = condition, children[1] = body block (optional)
                if (node->value == "loopu") {
                    int safety = 0;
                    while (true) {
                        if (node->children.empty()) break;
                        Value condVal = executeNode(node->children[0], env);
                        bool cond = condVal.isNumber() ? condVal.number != 0.0 :
                                    condVal.isBoolean() ? condVal.boolean : false;
                        if (!cond) break;
                        if (node->children.size() > 1) {
                            executeNode(node->children[1], env);
                        }
                        if (++safety > 1000000) {
                            throw std::runtime_error("Preposterous: InfiniteLoop — loopu exceeded 1M iterations");
                        }
                    }
                } else if (node->value == "loopf") {
                    // loopf { body } — loop forever (must break via condition inside)
                    int safety = 0;
                    while (true) {
                        if (node->children.size() > 0) {
                            executeNode(node->children[0], env);
                        }
                        if (++safety > 1000000) {
                            throw std::runtime_error("Preposterous: InfiniteLoop — loopf exceeded 1M iterations");
                        }
                    }
                }
                return Value();
            }
            
            case ASTNodeType::CONDITIONAL: {
                if (node->children.size() >= 2) {
                    Value condVal = executeNode(node->children[0], env);
                    bool cond = condVal.isBoolean() ? condVal.boolean :
                                condVal.isNumber()  ? condVal.number != 0.0 : false;
                    if (cond) {
                        return executeNode(node->children[1], env);
                    } else if (node->children.size() > 2) {
                        return executeNode(node->children[2], env);
                    }
                }
                return Value();
            }
            
            case ASTNodeType::FUNCTION_CALL: {
                return evaluateFunctionCall(node, env);
            }
            
            case ASTNodeType::BINARY_OP: {
                return evaluateBinaryOp(node, env);
            }
            
            case ASTNodeType::UNARY_OP: {
                return evaluateUnaryOp(node, env);
            }
            
            case ASTNodeType::NUMBER_LITERAL: {
                return Value(std::stod(node->value));
            }
            
            case ASTNodeType::STRING_LITERAL: {
                return Value(node->value);
            }
            
            case ASTNodeType::ARRAY_LITERAL: {
                std::vector<Value> elements;
                for (const auto& child : node->children) {
                    elements.push_back(executeNode(child, env));
                }
                return Value(elements);
            }
            
            case ASTNodeType::IDENTIFIER: {
                return env->get(node->value);
            }
            
            default:
                throw std::runtime_error("Unknown AST node type");
        }
    }
    
    Value evaluateFunctionCall(std::shared_ptr<ASTNode> node, Environment* env) {
        const std::string& name = node->value;
        
        // Evaluate arguments
        std::vector<Value> args;
        for (size_t i = 0; i < node->children.size(); ++i) {
            args.push_back(executeNode(node->children[i], env));
        }
        
        // Check for built-in SAPL operators
        if (name == "E") return builtinSumReduce(args);
        if (name == "PI") return builtinProductReduce(args);
        if (name == "I") return builtinInterval(args);
        if (name == "rev") return builtinReverse(args);
        if (name == "T") return builtinTranspose(args);
        if (name == "pw") return builtinShape(args);
        if (name == "asc") return builtinAscendingGrade(args);
        if (name == "desc") return builtinDescendingGrade(args);
        if (name == "cmp") return builtinCompare(args);
        if (name == "rep") return builtinReplicate(args);
        if (name == "sort") return builtinSort(args, env);
        
        // Check for user-defined function
        std::vector<std::string> params;
        std::shared_ptr<ASTNode> body;
        if (env->getFunction(name, params, body)) {
            Environment callEnv(env);
            for (size_t i = 0; i < params.size() && i < args.size(); ++i) {
                callEnv.define(params[i], args[i]);
            }
            return executeNode(body, &callEnv);
        }
        
        throw std::runtime_error("Preposterous: UndefinedFunction — " + name);
    }
    
    Value evaluateBinaryOp(std::shared_ptr<ASTNode> node, Environment* env) {
        Value left = executeNode(node->children[0], env);
        Value right = executeNode(node->children[1], env);
        
        const std::string& op = node->value;
        
        // Handle array broadcasting
        if (left.isArray() || right.isArray()) {
            return applyBinaryOpArray(left, right, op);
        }
        
        double l = left.number;
        double r = right.number;
        
        if (op == "+") return Value(l + r);
        if (op == "-") return Value(l - r);
        if (op == "*") return Value(l * r);
        if (op == "/") return Value(l / r);
        if (op == "%") return Value(std::fmod(l, r));
        if (op == "^") return Value(std::pow(l, r));
        if (op == "<") return Value(l < r ? 1.0 : 0.0);
        if (op == ">") return Value(l > r ? 1.0 : 0.0);
        if (op == "<=") return Value(l <= r ? 1.0 : 0.0);
        if (op == ">=") return Value(l >= r ? 1.0 : 0.0);
        if (op == "cmp") {
            // cmp for equality check: returns 1.0 (true) if equal, 0.0 (false) if not
            // used as: x cmp y in conditionals
            return Value(l == r ? 1.0 : 0.0);
        }
        if (op == "==") return Value(l == r ? 1.0 : 0.0);
        if (op == "!=") return Value(l != r ? 1.0 : 0.0);
        
        throw std::runtime_error("Unknown binary operator: " + op);
    }
    
    Value applyBinaryOpArray(const Value& left, const Value& right, const std::string& op) {
        std::vector<Value> result;
        
        if (left.isArray() && right.isArray()) {
            // Element-wise operation
            size_t n = std::max(left.arr.size(), right.arr.size());
            for (size_t i = 0; i < n; ++i) {
                Value l = i < left.arr.size() ? left.arr[i] : left;
                Value r = i < right.arr.size() ? right.arr[i] : right;
                result.push_back(applyBinaryOpArray(l, r, op));
            }
        } else if (left.isArray()) {
            // Broadcast right across left
            for (const auto& l : left.arr) {
                result.push_back(applyBinaryOpArray(l, right, op));
            }
        } else {
            // Broadcast left across right
            for (const auto& r : right.arr) {
                result.push_back(applyBinaryOpArray(left, r, op));
            }
        }
        
        return Value(result);
    }
    
    Value evaluateUnaryOp(std::shared_ptr<ASTNode> node, Environment* env) {
        Value operand = executeNode(node->children[0], env);
        
        if (node->value == "-") {
            if (operand.isArray()) {
                std::vector<Value> result;
                for (const auto& v : operand.arr) {
                    result.push_back(Value(-v.number));
                }
                return Value(result);
            }
            return Value(-operand.number);
        }
        
        throw std::runtime_error("Unknown unary operator: " + node->value);
    }
    
    // Built-in SAPL functions
    
    Value builtinSumReduce(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        Value arr = args[0];
        if (!arr.isArray()) return arr;
        
        double sum = 0;
        for (const auto& v : arr.arr) {
            sum += v.isNumber() ? v.number : 0;
        }
        return Value(sum);
    }
    
    Value builtinProductReduce(const std::vector<Value>& args) {
        if (args.empty()) return Value(1.0);
        Value arr = args[0];
        if (!arr.isArray()) return arr;
        
        double product = 1;
        for (const auto& v : arr.arr) {
            product *= v.isNumber() ? v.number : 1;
        }
        return Value(product);
    }
    
    Value builtinInterval(const std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("Preposterous: ArgumentError — I requires 2 arguments (I start end)");
        
        double start = args[0].isNumber() ? args[0].number : 0;
        double end = args[1].isNumber() ? args[1].number : 0;
        
        std::vector<Value> result;
        if (start <= end) {
            for (double i = start; i <= end; ++i) {
                result.push_back(Value(i));
            }
        } else {
            for (double i = start; i >= end; --i) {
                result.push_back(Value(i));
            }
        }
        return Value(result);
    }
    
    Value builtinReverse(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        Value arr = args[0];
        if (!arr.isArray()) return arr;
        
        std::vector<Value> reversed(arr.arr.rbegin(), arr.arr.rend());
        return Value(reversed);
    }
    
    Value builtinTranspose(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        Value arr = args[0];
        if (!arr.isArray()) return arr;
        
        // Simple 2D transpose
        if (arr.arr.empty()) return arr;
        
        // Assume all sub-arrays have same length
        size_t rows = arr.arr.size();
        size_t cols = arr.arr[0].isArray() ? arr.arr[0].arr.size() : 1;
        
        std::vector<Value> result;
        for (size_t j = 0; j < cols; ++j) {
            std::vector<Value> row;
            for (size_t i = 0; i < rows; ++i) {
                if (arr.arr[i].isArray() && j < arr.arr[i].arr.size()) {
                    row.push_back(arr.arr[i].arr[j]);
                } else if (!arr.arr[i].isArray() && j == 0) {
                    row.push_back(arr.arr[i]);
                }
            }
            result.push_back(Value(row));
        }
        
        return Value(result);
    }
    
    Value builtinShape(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        Value arr = args[0];
        if (!arr.isArray()) return Value(1.0);
        return Value(static_cast<double>(arr.arr.size()));
    }
    
    Value builtinAscendingGrade(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        Value arr = args[0];
        if (!arr.isArray()) {
            std::vector<Value> single;
            single.push_back(Value(0.0));
            return Value(single);
        }
        
        // Return indices that would sort the array
        std::vector<std::pair<double, size_t>> indexed;
        for (size_t i = 0; i < arr.arr.size(); ++i) {
            indexed.push_back({arr.arr[i].isNumber() ? arr.arr[i].number : 0.0, i});
        }
        
        std::sort(indexed.begin(), indexed.end());
        
        std::vector<Value> result;
        for (const auto& p : indexed) {
            result.push_back(Value(static_cast<double>(p.second)));
        }
        return Value(result);
    }
    
    Value builtinDescendingGrade(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        Value arr = args[0];
        if (!arr.isArray()) {
            std::vector<Value> single;
            single.push_back(Value(0.0));
            return Value(single);
        }
        
        std::vector<std::pair<double, size_t>> indexed;
        for (size_t i = 0; i < arr.arr.size(); ++i) {
            indexed.push_back({arr.arr[i].isNumber() ? arr.arr[i].number : 0, i});
        }
        
        std::sort(indexed.rbegin(), indexed.rend());
        
        std::vector<Value> result;
        for (const auto& p : indexed) {
            result.push_back(Value(static_cast<double>(p.second)));
        }
        return Value(result);
    }
    
    Value builtinCompare(const std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        
        Value left = args[0];
        Value right = args[1];
        
        if (left.isNumber() && right.isNumber()) {
            if (left.number < right.number) return Value(-1.0);
            if (left.number > right.number) return Value(1.0);
            return Value(0.0);
        }
        
        return Value(0.0);
    }
    
    Value builtinReplicate(const std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        
        Value value = args[0];
        Value count = args[1];
        
        if (!count.isNumber()) return Value();
        
        int n = static_cast<int>(count.number);
        std::vector<Value> result;
        for (int i = 0; i < n; ++i) {
            result.push_back(value);
        }
        return Value(result);
    }
    
    Value builtinSort(const std::vector<Value>& args, Environment* env) {
        if (args.empty()) return Value();
        
        Value arr = args[0];
        if (!arr.isArray()) return arr;
        
        // Check for 'desc' modifier
        bool descending = false;
        if (args.size() > 1 && args[1].isString() && args[1].str == "desc") {
            descending = true;
        }
        
        std::vector<Value> sorted = arr.arr;
        std::sort(sorted.begin(), sorted.end(), [descending](const Value& a, const Value& b) {
            double av = a.isNumber() ? a.number : 0;
            double bv = b.isNumber() ? b.number : 0;
            return descending ? av > bv : av < bv;
        });
        
        return Value(sorted);
    }
};

// ============================================================================
// MAIN SAPL CLASS
// ============================================================================

class SAPLInterpreter {
public:
    SAPLInterpreter() {}
    
    Value run(const std::string& source) {
        try {
            // Lexical analysis
            Lexer lexer(source);
            std::vector<Token> tokens = lexer.tokenize();
            
            // Parsing
            Parser parser(tokens);
            std::shared_ptr<ASTNode> ast = parser.parse();
            
            // Interpretation
            Interpreter interpreter;
            return interpreter.execute(ast);
        } catch (const std::exception& e) {
            std::cerr << "SAPL Error: " << e.what() << std::endl;
            return Value();
        }
    }
    
    void runFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << filename << std::endl;
            return;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        
        run(source);
    }
    
    void runREPL() {
        std::cout << "SAPL Interpreter v0.1.0 - Abu Shariff, 2026 - Type 'exit' to quit" << std::endl;
        std::cout << "> ";
        
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "exit" || line == "quit") {
                break;
            }
            
            if (line.empty()) {
                std::cout << "> ";
                continue;
            }
            
            Value result = run(line);
            if (!result.isNil()) {
                std::cout << result.toString() << std::endl;
            }
            
            std::cout << "> ";
        }
    }
};

} // namespace SAPL

#endif // SAPL_H
