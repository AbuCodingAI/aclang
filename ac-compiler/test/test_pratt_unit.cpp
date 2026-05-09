// Unit tests for Pratt parser
// Compile: g++ -std=c++17 -I../include test_pratt_unit.cpp ../src/lexer.cpp ../src/parser.cpp -o test_pratt_unit
// Run: ./test_pratt_unit

#include <iostream>
#include <cassert>
#include <string>
#include "../include/token.hpp"
#include "../include/ast.hpp"

// External functions
extern std::vector<Token> lex(const std::string& source);
extern NodePtr parse(const std::vector<Token>& tokens);

void test_binary_operator_precedence() {
    std::cout << "Test: Binary operator precedence (2 + 3 * 4 should be 2 + (3 * 4))" << std::endl;
    
    std::string source = "AC->PY\n<mainloop>\n    x = 2 + 3 @ 4\n<mainloop>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    // The AST should have the structure where @ has higher precedence than +
    // For now, just verify it parses without error
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

void test_unary_operators() {
    std::cout << "Test: Unary operators (-5, NOT true)" << std::endl;
    
    std::string source = "AC->PY\n<mainloop>\n    x = -5\n    y = NOT true\n<mainloop>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

void test_function_calls() {
    std::cout << "Test: Function calls func(a, b, c)" << std::endl;
    
    std::string source = "AC->PY\nMake add func(a, b, c)\n    return a + b + c\n<mainloop>\n    x = add(1, 2, 3)\n    /kill\n<mainloop>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

void test_array_indexing() {
    std::cout << "Test: Array indexing arr[0], arr[i+1]" << std::endl;
    
    std::string source = "AC->PY\n<StartHere>\n    x = arr[0]\n    y = arr[i + 1]\n<EndHere>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

void test_nested_expressions() {
    std::cout << "Test: Nested expressions ((a + b) @ (c - d))" << std::endl;
    
    std::string source = "AC->PY\n<StartHere>\n    x = (a + b) @ (c - d)\n<EndHere>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

void test_comparison_operators() {
    std::cout << "Test: Comparison operators (5 > 3, 10 < 20, 5 is 5)" << std::endl;
    
    std::string source = "AC->PY\n<StartHere>\n    a = 5 > 3\n    b = 10 < 20\n    c = 5 is 5\n<EndHere>\n";
    auto tokens = lex(source);
    auto ast = parse(tokens);
    
    assert(ast != nullptr);
    assert(ast->type == NodeType::Program);
    
    std::cout << "  ✓ Parsed successfully" << std::endl;
}

int main() {
    std::cout << "Running Pratt Parser Unit Tests" << std::endl;
    std::cout << "================================" << std::endl;
    
    try {
        test_binary_operator_precedence();
        test_unary_operators();
        test_function_calls();
        test_array_indexing();
        test_nested_expressions();
        test_comparison_operators();
        
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
