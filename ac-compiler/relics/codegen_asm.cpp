#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include "../include/tags.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <cctype>
#include <map>
#include <functional>
#include <regex>

// Expression element for parsing
struct ExprElement {
    std::string value;  // operand or operator
    bool isOperator;
};

class AsmCodeGen : public BaseCodeGen {
    // === OUTPUT SECTIONS ===
    std::ostringstream rodata;      // .rodata strings
    std::ostringstream text_main;   // main() code
    std::ostringstream text_funcs;  // function definitions
    std::ostringstream* currentOut; // Active output stream

    // === SYMBOL TABLES ===
    std::map<std::string, int> varOffsets;  // var name -> stack frame offset
    std::map<std::string, TypePtr> varTypes;// var name -> type (for printf format)
    std::map<std::string, int> stringTable; // string content -> label ID

    // === STATE ===
    int labelCounter = 0;
    int stackOffset = 0;       // Stack frame offset tracking (-8, -16, -24...)
    int nextStringId = 0;      // Next unique string label ID
    bool collectingFunctions = false;
    int currentFuncStackDepth = 0;

    // === STRING TABLE MANAGEMENT ===
    std::string allocateString(const std::string& content) {
        if (stringTable.find(content) == stringTable.end()) {
            stringTable[content] = nextStringId++;
        }
        return ".str_" + std::to_string(stringTable[content]);
    }

    void emitStringSection() {
        if (stringTable.empty()) return;
        rodata << ".section .rodata\n";
        for (auto& [content, id] : stringTable) {
            rodata << ".str_" << id << ": .asciz \"" << content << "\"\n";
        }
        rodata << ".section .text\n";
    }

    // === SYMBOL TABLE MANAGEMENT ===
    int allocateStackVariable(const std::string& name, TypePtr type) {
        stackOffset -= 8;
        varOffsets[name] = stackOffset;
        varTypes[name] = type;
        return stackOffset;
    }

    bool isVariable(const std::string& name) {
        return varOffsets.find(name) != varOffsets.end();
    }

    bool isNumericType(const std::string& varName) {
        auto it = varTypes.find(varName);
        if (it != varTypes.end() && it->second) {
            return it->second->isNumeral();
        }
        return false;
    }

    void clearLocalScope() {
        varOffsets.clear();
        varTypes.clear();
        stackOffset = 0;
    }

    // === LABEL GENERATION ===
    std::string newLabel() {
        return ".L" + std::to_string(labelCounter++);
    }

    void emit(const std::string& line) {
        *currentOut << "    " << line << "\n";
    }
    void emitRaw(const std::string& line) { 
        *currentOut << line << "\n"; 
    }

    // Common function/return epilogue for function bodies and main
    void emitEpilogue() {
        emit("add $256, %rsp");
        emit("pop %rbp");
        emit("ret");
    }

    // Parse expression into tokens: "2 + 3 * (4 - 1)" -> [2, +, 3, *, (, 4, -, 1, )]
    std::vector<ExprElement> tokenizeExpr(const std::string& expr) {
        std::vector<ExprElement> tokens;
        std::string current;
        std::string input = expr;
        
        // Handle fn prefix (fn means multiply with higher precedence)
        if (input.substr(0, 3) == "fn ") {
            input = input.substr(3);  // Remove "fn "
            // fn x*y means x*y, fn x+(y*z) means x + (y*z) but with fn applied to multiply
            // For now, just strip fn and process normally
        }
        
        for (size_t i = 0; i < input.size(); i++) {
            char c = input[i];
            
            if (c == ' ') {
                if (!current.empty()) {
                    tokens.push_back({current, false});
                    current.clear();
                }
            } else if (c == '(' || c == ')') {
                if (!current.empty()) {
                    tokens.push_back({current, false});
                    current.clear();
                }
                tokens.push_back({std::string(1, c), true});
            } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
                // Handle negative numbers: if '-' starts a new token (current empty), treat it as part of a number
                if (c == '-' && current.empty()) {
                    current += c;  // Part of negative number
                } else {
                    if (!current.empty()) {
                        tokens.push_back({current, false});
                        current.clear();
                    }
                    tokens.push_back({std::string(1, c), true});
                }
            } else {
                current += c;
            }
        }
        
        if (!current.empty()) {
            tokens.push_back({current, false});
        }
        
        return tokens;
    }

    // Simple wrapper to evaluate an expression string
    void evalExprStr(const std::string& exprStr) {
        std::string expr = exprStr;
        // Parser sometimes prefixes fn expressions with __fn__
        if (expr.rfind("__fn__", 0) == 0) expr = expr.substr(6);
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = expr.find("@", p)) != std::string::npos;) {
            expr.replace(p, 1, "*"); p++;
        }
        auto tokens = tokenizeExpr(expr);


        size_t pos = 0;
        parseAddSub(tokens, pos);
    }

    // Load variable or number into %rax
    void loadOperand(const std::string& operand) {
        if (operand.empty()) return;
        
        // Check if it's a number
        if (std::isdigit(operand[0]) || (operand[0] == '-' && operand.size() > 1)) {
            emit("mov $" + operand + ", %rax");
        } else {
            // It's a variable
            auto it = varOffsets.find(operand);
            if (it != varOffsets.end()) {
                emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
            } else {
                emit("# Unknown operand: " + operand);
            }
        }
    }

    // Call a function with one argument in %edi, result in %eax
    void callFunc(const std::string& funcName, const std::string& arg) {
        if (std::isdigit(arg[0]) || (arg[0] == '-' && arg.size() > 1)) {
            emit("mov $" + arg + ", %edi");
        } else {
            auto it = varOffsets.find(arg);
            if (it != varOffsets.end()) {
                emit("mov " + std::to_string(it->second) + "(%rbp), %edi");
            }
        }
        emit("call " + funcName);
    }

    std::vector<std::string> splitListElements(const std::string& listContents) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : listContents) {
            if (c == ',') {
                // trim whitespace
                while (!cur.empty() && cur.back() == ' ') cur.pop_back();
                size_t i = 0;
                while (i < cur.size() && cur[i] == ' ') i++;
                if (i > 0) cur = cur.substr(i);
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
                continue;
            }
            cur.push_back(c);
        }
        while (!cur.empty() && cur.back() == ' ') cur.pop_back();
        size_t i = 0;
        while (i < cur.size() && cur[i] == ' ') i++;
        if (i > 0) cur = cur.substr(i);
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return v.substr(1, v.size() - 2);
        return v;
    }

    std::string unwrapDollars(const std::string& s) {
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for ASM (just return content, ASM handles strings differently)
            std::string escaped;
            for (char c : content) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            return escaped;
        }
        
        // Otherwise, find $...$ pairs within the expression
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    std::string content = s.substr(i + 1, end - i - 1);
                    // Escape special characters
                    std::string escaped;
                    for (char c : content) {
                        if (c == '"') escaped += "\\\"";
                        else if (c == '\\') escaped += "\\\\";
                        else if (c == '\n') escaped += "\\n";
                        else if (c == '\t') escaped += "\\t";
                        else escaped += c;
                    }
                    result += escaped;
                    i = end;
                    continue;
                }
            }
            result += s[i];
        }
        return result;
    }

    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && std::isspace(r.back())) r.pop_back();
        while (!r.empty() && std::isspace(r.front())) r.erase(r.begin());
        r = unwrapDollars(r);
        
        // Case-insensitive boolean handling
        std::regex truePattern(R"(\b(true|True|TRUE)\b)", std::regex::icase);
        std::regex falsePattern(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        r = std::regex_replace(r, truePattern, "1");
        r = std::regex_replace(r, falsePattern, "0");
        
        // Null handling: null -> 0 (ASM representation)
        std::regex nullPattern(R"(\bnull\b)");
        r = std::regex_replace(r, nullPattern, "0");
        
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        // is -> == (equality keyword)
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.substr(0, 3) == "is ") r.replace(0, 3, "");

        // Single '=' should be treated as equality too (but not if it's part of <=, >=, ==, !=)
        if (r.find("==") == std::string::npos && r.find("!=") == std::string::npos && 
            r.find("<=") == std::string::npos && r.find(">=") == std::string::npos) {
            size_t pos = r.find('=');
            if (pos != std::string::npos) {
                // Avoid replacing assignment operators in unsupported contexts
                r.replace(pos, 1, "==");
            }
        }
        return r;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && std::isspace(r.back())) r.pop_back();
        while (!r.empty() && std::isspace(r.front())) r.erase(r.begin());
        r = unwrapDollars(r);
        
        // Case-insensitive boolean handling
        std::regex truePattern(R"(\b(true|True|TRUE)\b)", std::regex::icase);
        std::regex falsePattern(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        r = std::regex_replace(r, truePattern, "1");
        r = std::regex_replace(r, falsePattern, "0");
        
        // Null handling: null -> 0 (ASM representation)
        std::regex nullPattern(R"(\bnull\b)");
        r = std::regex_replace(r, nullPattern, "0");
        
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
        
        return r;
    }

    // Check if the AST uses event listeners
    bool usesEventListener(const ASTNode& node) {
        if (node.type == NodeType::EventListener || node.type == NodeType::KeyBinding || node.type == NodeType::InputStmt) {
            return true;
        }
        for (auto& child : node.children) {
            if (usesEventListener(*child)) return true;
        }
        return false;
    }

    void genBlock(const ASTNode& node) {
        for (auto& c : node.children) genNode(*c);
    }

    // Precedence climbing expression evaluator
    // Returns true if successfully parsed; result in %eax
    void parseAddSub(const std::vector<ExprElement>& tokens, size_t& pos) {
        parseMulDiv(tokens, pos);
        
        while (pos < tokens.size() && tokens[pos].isOperator && 
               (tokens[pos].value == "+" || tokens[pos].value == "-")) {
            std::string op = tokens[pos].value;
            pos++;

            emit("push %rax");  // Save left operand
            parseMulDiv(tokens, pos);
            emit("mov %rax, %rdx");   // right operand in rdx
            emit("pop %rax");         // left operand in rax

            if (op == "+") {
                emit("add %rdx, %rax");
            } else {
                emit("sub %rdx, %rax");
            }
        }
    }

    void parseMulDiv(const std::vector<ExprElement>& tokens, size_t& pos) {
        parsePrimary(tokens, pos);
        
        while (pos < tokens.size() && tokens[pos].isOperator && 
               (tokens[pos].value == "*" || tokens[pos].value == "/" || tokens[pos].value == "%")) {
            std::string op = tokens[pos].value;
            pos++;

            emit("push %rax");  // Save left operand
            parsePrimary(tokens, pos);
            emit("mov %rax, %rdx");   // right operand in rdx
            emit("pop %rax");         // left operand in rax

            if (op == "*") {
                emit("imul %rdx");
            } else {
                // Division/modulo: left in rax, right in rdx
                emit("cqo");
                emit("idiv %rdx");
                if (op == "%") {
                    emit("mov %rdx, %rax");  // remainder
                }
            }
        }
    }

    void parsePrimary(const std::vector<ExprElement>& tokens, size_t& pos) {
        if (pos >= tokens.size()) return;

        // Parenthesized expression
        if (tokens[pos].value == "(") {
            pos++;  // skip (
            parseAddSub(tokens, pos);
            if (pos < tokens.size() && tokens[pos].value == ")") {
                pos++;  // skip )
            }
            return;
        }

        // Function call: name(expr)
        if (!tokens[pos].isOperator) {
            std::string name = tokens[pos].value;
            if (pos + 1 < tokens.size() && tokens[pos + 1].value == "(") {
                pos += 2; // skip name and '('
                parseAddSub(tokens, pos);
                if (pos < tokens.size() && tokens[pos].value == ")") pos++;
                emit("mov %rax, %rdi");
                emit("call " + name);
                return;
            }
        }

        // Default: load number/variable
        loadOperand(tokens[pos].value);
        pos++;
    }

    void evalExpr(const std::vector<ExprElement>& tokens) {
        size_t pos = 0;
        parseAddSub(tokens, pos);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            currentOut = &text_main;  // Output main body
            
            rodata << ".section .rodata\n";
            rodata << "    fmt_int: .asciz \"%d\\n\"\n";
            rodata << "    fmt_str: .asciz \"%s\\n\"\n";
            rodata << "    input_fmt: .asciz \"%255s\"\n";
            rodata << ".section .bss\n";
            rodata << "    input_buffer: .space 256\n";
            rodata << ".section .rodata\n";
            
            // Only emit keybinds if event listeners are used
            bool needsEventListener = usesEventListener(node);
            if (needsEventListener) {
                rodata << "    # Keybinds\n";
                rodata << "    .equ KEY_1, 0x31\n";
                rodata << "    .equ KEY_2, 0x32\n";
                rodata << "    .equ KEY_3, 0x33\n";
                rodata << "    .equ KEY_4, 0x34\n";
                rodata << "    .equ KEY_5, 0x35\n";
                rodata << "    .equ KEY_6, 0x36\n";
                rodata << "    .equ KEY_7, 0x37\n";
                rodata << "    .equ KEY_8, 0x38\n";
                rodata << "    .equ KEY_9, 0x39\n";
                rodata << "    .equ KEY_0, 0x30\n";
                rodata << "    .equ KEY_MINUS, 0x2D\n";
                rodata << "    .equ KEY_EQUAL, 0x3D\n";
                rodata << "    .equ KEY_BACKSPACE, 0x08\n";
                rodata << "    .equ KEY_BACKSLASH, 0x5C\n";
                rodata << "    .equ KEY_TAB, 0x09\n";
                rodata << "    .equ KEY_Q, 0x71\n";
                rodata << "    .equ KEY_W, 0x77\n";
                rodata << "    .equ KEY_E, 0x65\n";
                rodata << "    .equ KEY_R, 0x72\n";
                rodata << "    .equ KEY_T, 0x74\n";
                rodata << "    .equ KEY_Y, 0x79\n";
                rodata << "    .equ KEY_U, 0x75\n";
                rodata << "    .equ KEY_I, 0x69\n";
                rodata << "    .equ KEY_O, 0x6F\n";
                rodata << "    .equ KEY_P, 0x70\n";
                rodata << "    .equ KEY_LBRACKET, 0x5B\n";
                rodata << "    .equ KEY_RBRACKET, 0x5D\n";
                rodata << "    .equ KEY_CAPS, 0x14\n";
                rodata << "    .equ KEY_A, 0x61\n";
                rodata << "    .equ KEY_S, 0x73\n";
                rodata << "    .equ KEY_D, 0x64\n";
                rodata << "    .equ KEY_F, 0x66\n";
                rodata << "    .equ KEY_G, 0x67\n";
                rodata << "    .equ KEY_H, 0x68\n";
                rodata << "    .equ KEY_J, 0x6A\n";
                rodata << "    .equ KEY_K, 0x6B\n";
                rodata << "    .equ KEY_L, 0x6C\n";
                rodata << "    .equ KEY_SEMICOLON, 0x3B\n";
                rodata << "    .equ KEY_APOSTROPHE, 0x27\n";
                rodata << "    .equ KEY_ENTER, 0x0D\n";
                rodata << "    .equ KEY_SHIFT, 0x10\n";
                rodata << "    .equ KEY_Z, 0x7A\n";
                rodata << "    .equ KEY_X, 0x78\n";
                rodata << "    .equ KEY_C, 0x63\n";
                rodata << "    .equ KEY_V, 0x76\n";
                rodata << "    .equ KEY_B, 0x62\n";
                rodata << "    .equ KEY_N, 0x6E\n";
                rodata << "    .equ KEY_M, 0x6D\n";
                rodata << "    .equ KEY_COMMA, 0x2C\n";
                rodata << "    .equ KEY_PERIOD, 0x2E\n";
                rodata << "    .equ KEY_SLASH, 0x2F\n";
                rodata << "    .equ KEY_CTRL, 0x11\n";
                rodata << "    .equ KEY_ALT, 0x12\n";
                rodata << "    .equ KEY_SPACE, 0x20\n";
                rodata << "    .equ KEY_FN, 0x5B\n";
                rodata << "    .equ KEY_SUPER, 0x5B\n";
                rodata << "    .equ KEY_WINDOWS, 0x5B\n";
                rodata << "    .equ KEY_UP, 0x26\n";
                rodata << "    .equ KEY_DOWN, 0x28\n";
                rodata << "    .equ KEY_LEFT, 0x25\n";
                rodata << "    .equ KEY_RIGHT, 0x27\n";
            }
            
            text_main << ".section .text\n";
            text_main << ".globl main\n";
            
            // Find mainloop tag block
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop") {
                    mainloopBlock = c.get();
                    break;
                }
            }
            
            // First pass: collect ALL functions from entire program (not just mainloop)
            collectingFunctions = true;
            std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef) {
                    genNode(n);
                }
                for (auto& c : n.children) {
                    collectFuncs(*c);
                }
            };
            // Collect from entire program
            for (auto& c : node.children) {
                collectFuncs(*c);
            }
            collectingFunctions = false;
            
            // Main function
            emitRaw("main:");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            emit("sub $256, %rsp");  // Allocate stack space
            clearLocalScope();
            
            // Second pass: emit main body from mainloop
            if (mainloopBlock) {
                for (auto& c : mainloopBlock->children) genNode(*c);
            }
            
            emit("xor %rax, %rax");
            emitEpilogue();
            
            // Output is handled by generate() function
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib camera -> include camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("#include \"../library/camera/camera.hpp\"");
                emitRaw("using namespace AC;");
            }
            break;
        }
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                // String literal
                std::string str = val.substr(1, val.size() - 2);
                std::string strLabel = allocateString(str);
                emit("lea " + strLabel + "(%rip), %rdi");
                emit("xor %eax, %eax");
                emit("call printf");
            } else {
                // Variable or expression - assume numeric
                evalExprStr(val);
                emit("mov %rax, %rsi");
                emit("lea fmt_int(%rip), %rdi");
                emit("xor %eax, %eax");
                emit("call printf");
            }
            break;
        }

        case NodeType::ForeignBlock: {
            emit("# Preposterous: Foreign code is too Foreign (if this breaks, that's on you)");
            std::istringstream ss(node.value);
            std::string ln;
            while (std::getline(ss, ln)) emit(ln);
            break;
        }

        case NodeType::KillStmt:
            emit("mov $0, %edi");
            emit("call exit");
            break;

        case NodeType::RaiseStmt:
            emit("mov $1, %edi");
            emit("call exit");
            break;

        case NodeType::ReturnStmt: {
            std::string val = node.value;
            
            if (val.empty()) {
                emit("xor %rax, %rax");  // return 0
            } else {
                evalExprStr(val);  // Result in %rax
            }

            emitEpilogue();
            break;
        }

        case NodeType::AssignStmt: {
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                TypePtr varType = nullptr;
                
                if (val.substr(0, 8) == "__dict__") {
                    // ASM doesn't support dicts - comment it out
                    emit("# Dict not supported in ASM: " + node.value);
                    break;
                }
                
                // Handle __funcall__ prefix
                if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
                    if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
                        auto& funcCall = *node.children[0];
                        std::string funcName = funcCall.value;
                        
                        // Push arguments in reverse order (right to left for x86-64 calling convention)
                        // For simplicity, handle up to 6 args in registers: rdi, rsi, rdx, rcx, r8, r9
                        std::vector<std::string> argRegs = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
                        
                        for (size_t i = 0; i < funcCall.attrs.size() && i < argRegs.size(); i++) {
                            std::string arg = funcCall.attrs[i];
                            // Remove key= prefix if present
                            size_t eqPos = arg.find('=');
                            if (eqPos != std::string::npos) {
                                arg = arg.substr(eqPos + 1);
                            }
                            // Trim whitespace
                            while (!arg.empty() && arg.back() == ' ') arg.pop_back();
                            while (!arg.empty() && arg.front() == ' ') arg = arg.substr(1);
                            
                            evalExprStr(arg);
                            emit("mov %rax, " + argRegs[i]);
                        }
                        
                        emit("call " + funcName);
                        varType = std::make_shared<Type>(TypeKind::Numeral);
                        int offset = allocateStackVariable(node.value, varType);
                        emit("mov %rax, " + std::to_string(offset) + "(%rbp)  # " + node.value);
                        break;
                    }
                }

                // Check if it's a simple function call: identifier(args) without spaces before paren
                size_t parenPos = val.find('(');
                bool isFuncCall = false;
                if (parenPos != std::string::npos && val.find(')') != std::string::npos) {
                    std::string beforeParen = val.substr(0, parenPos);
                    bool validName = !beforeParen.empty() && beforeParen.find(' ') == std::string::npos &&
                                     beforeParen.find('+') == std::string::npos && 
                                     beforeParen.find('-') == std::string::npos &&
                                     beforeParen.find('*') == std::string::npos &&
                                     beforeParen.find('/') == std::string::npos;
                    isFuncCall = validName && std::isalpha(beforeParen[0]);
                }

                if (isFuncCall) {
                    std::string funcName = val.substr(0, parenPos);
                    std::string arg = val.substr(parenPos + 1, val.find(')') - parenPos - 1);
                    evalExprStr(arg);
                    emit("mov %rax, %rdi");
                    emit("call " + funcName);
                    varType = std::make_shared<Type>(TypeKind::Numeral);

                    int offset = allocateStackVariable(node.value, varType);
                    emit("mov %rax, " + std::to_string(offset) + "(%rbp)  # " + node.value);
                    break;
                }

                if (val.rfind("__list__", 0) == 0 || val.rfind("__tuple__", 0) == 0) {
                    bool isTuple = (val.rfind("__tuple__", 0) == 0);
                    std::string contents = isTuple ? val.substr(9) : val.substr(8);
                    auto elems = splitListElements(contents);
                    int len = (int)elems.size();
                    int bytes = 8 * (1 + len);

                    // allocate list on heap using malloc
                    emit("mov $" + std::to_string(bytes) + ", %edi");
                    emit("call malloc");
                    // returned pointer in %rax
                    int offset = allocateStackVariable(node.value, nullptr);
                    varType = std::make_shared<Type>(isTuple ? TypeKind::Tuple : TypeKind::List);
                    varTypes[node.value] = varType;
                    emit("mov %rax, " + std::to_string(offset) + "(%rbp)  # " + node.value);

                    // store length
                    emit("mov $" + std::to_string(len) + ", (%rax)");
                    for (int i = 0; i < len; i++) {
                        std::string elem = elems[i];
                        std::string strVal = quoteIfString(elem);

                        if (!strVal.empty() && strVal != elem) {
                            // string literal stored as pointer
                            std::string label = allocateString(strVal);
                            emit("mov " + std::to_string(offset) + "(%rbp), %rcx");
                            emit("lea " + label + "(%rip), %rdx");
                            emit("mov %rdx, " + std::to_string(8 * (i + 1)) + "(%rcx)");
                        } else {
                            // numeric or expression - apply translateExpr
                            std::string translatedElem = translateExpr(elem);
                            if (!translatedElem.empty() && (std::isdigit(translatedElem[0]) || translatedElem[0] == '-')) {
                                emit("mov $" + translatedElem + ", %rax");
                            } else {
                                // variable or expression
                                evalExprStr(translatedElem);
                            }
                            emit("mov " + std::to_string(offset) + "(%rbp), %rcx");
                            emit("mov %rax, " + std::to_string(8 * (i + 1)) + "(%rcx)");
                        }
                    }
                    break;
                }

                // Regular numeric/string assignment or expression
                std::string translatedVal = translateExpr(val);
                if (translatedVal.size() >= 2 && translatedVal.front() == '$' && translatedVal.back() == '$') {
                    std::string text = translatedVal.substr(1, translatedVal.size() - 2);
                    std::string label = allocateString(text);
                    emit("lea " + label + "(%rip), %rax");
                    varType = std::make_shared<Type>(TypeKind::String);
                } else {
                    evalExprStr(translatedVal);
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                }

                int offset = allocateStackVariable(node.value, varType);
                emit("mov %rax, " + std::to_string(offset) + "(%rbp)  # " + node.value);
            }
            break;
        }

        case NodeType::PropAssign:
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;
            if (name == "Term.display") {
                // String literal direct
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    std::string str = args.substr(1, args.size() - 2);
                    std::string strLabel = ".str_" + std::to_string(labelCounter);
                    emitRaw("    lea " + strLabel + "(%rip), %rdi");
                    emitRaw("    xor %eax, %eax");
                    emitRaw("    call printf");
                    rodata << ".section .rodata\n";
                    rodata << strLabel << ": .asciz \"" << str << "\"\n";
                    rodata << ".section .text\n";
                    labelCounter++;
                } else {
                    // Index access e.g. a[0]
                    size_t brOpen = args.find('[');
                    size_t brClose = args.rfind(']');
                    if (brOpen != std::string::npos && brClose != std::string::npos && brClose > brOpen) {
                        std::string listName = args.substr(0, brOpen);
                        // trim whitespace
                        while (!listName.empty() && std::isspace(listName.back())) listName.pop_back();
                        while (!listName.empty() && std::isspace(listName.front())) listName.erase(listName.begin());
                        std::string indexExpr = args.substr(brOpen + 1, brClose - brOpen - 1);
                        auto it = varOffsets.find(listName);
                        if (it != varOffsets.end()) {
                            evalExprStr(indexExpr);
                            emit("mov %rax, %rbx");
                            emit("mov " + std::to_string(it->second) + "(%rbp), %rcx");
                            emit("lea 8(%rcx), %rdx");
                            emit("mov (%rdx, %rbx, 8), %rax");
                            emitRaw("    lea fmt_int(%rip), %rdi");
                            emit("mov %rax, %rsi");
                            emitRaw("    xor %eax, %eax");
                            emitRaw("    call printf");
                        } else {
                            emit("# unknown variable: " + listName);
                        }
                    } else {
                        // Non-index variable or expression
                        auto it = varOffsets.find(args);
                        if (it != varOffsets.end()) {
                            auto typeIt = varTypes.find(args);
                            if (typeIt != varTypes.end() && typeIt->second->isNumeral()) {
                                emitRaw("    lea fmt_int(%rip), %rdi");
                                emit("mov " + std::to_string(it->second) + "(%rbp), %esi");
                                emitRaw("    xor %eax, %eax");
                                emitRaw("    call printf");
                            } else {
                                emitRaw("    lea fmt_str(%rip), %rdi");
                                emit("mov " + std::to_string(it->second) + "(%rbp), %rsi");
                                emitRaw("    xor %eax, %eax");
                                emitRaw("    call printf");
                            }
                        } else {
                            // Evaluate expression or numeric literal
                            evalExprStr(args);
                            emitRaw("    lea fmt_int(%rip), %rdi");
                            emit("mov %rax, %rsi");
                            emitRaw("    xor %eax, %eax");
                            emitRaw("    call printf");
                        }
                    }
                }
            } else if (name == "Term.ask") {
                // Read input from stdin into input_buffer
                emit("lea input_fmt(%rip), %rdi");
                emit("lea input_buffer(%rip), %rsi");
                emit("xor %eax, %eax");
                emit("call scanf");
                // Return pointer to input_buffer in %rax
                emit("lea input_buffer(%rip), %rax");
            }
            break;
        }

        case NodeType::MethodChain: {
            // Process chained method calls
            for (auto& child : node.children) {
                genNode(*child);
            }
            break;
        }

        case NodeType::RangeExpr: {
            // range N - create array [0, 1, 2, ..., N]
            std::string n = node.value;
            while (!n.empty() && n.back() == ' ') n.pop_back();
            
            // Evaluate N into %rax
            evalExprStr(n);
            
            // Allocate array on heap: size = (N+1)*8 + 8 for length
            emit("mov %rax, %rbx");             // Save N
            emit("add $1, %rax");               // N+1
            emit("imul $8, %rax");              // (N+1)*8
            emit("add $8, %rax");               // +8 for length field
            emit("mov %rax, %rdi");
            emit("call malloc");
            emit("mov %rbx, (%rax)");           // Store length (N) at offset 0
            
            // Fill array [0, N]
            emit("mov $0, %rcx");               // counter = 0
            emit(".loop_fill:");
            emit("cmp %rbx, %rcx");             // counter <= N?
            emit("jg .loop_end");
            emit("mov %rcx, 8(%rax, %rcx, 8)"); // array[counter] = counter
            emit("inc %rcx");
            emit("jmp .loop_fill");
            emit(".loop_end:");
            break;
        }

        case NodeType::SequenceExpr: {
            // sequence(x, y) - create array [x, x+1, ..., y]
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            
            // Evaluate x and y
            evalExprStr(x);
            emit("mov %rax, %r8");              // Save x to r8
            evalExprStr(y);
            emit("mov %rax, %r9");              // Save y to r9
            
            // Check x <= y
            emit("cmp %r9, %r8");
            emit("jg .seq_error");
            
            // Calculate size = (y - x + 1)
            emit("mov %r9, %rax");
            emit("sub %r8, %rax");              // y - x
            emit("add $1, %rax");               // +1
            emit("imul $8, %rax");              // *8 bytes per element
            emit("add $8, %rax");               // +8 for length
            emit("mov %rax, %rdi");
            emit("call malloc");
            
            // Store length
            emit("mov %r9, %rbx");
            emit("sub %r8, %rbx");
            emit("inc %rbx");
            emit("mov %rbx, (%rax)");
            
            // Fill array
            emit("mov %r8, %rcx");              // counter = x
            emit(".loop_seq:");
            emit("cmp %r9, %rcx");
            emit("jg .loop_seq_end");
            emit("mov %rcx, 8(%rax, %rcx, 8)"); // array[counter] = counter
            emit("inc %rcx");
            emit("jmp .loop_seq");
            emit(".loop_seq_end:");
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            
            // Switch to functions output
            currentOut = &text_funcs;
            
            emitRaw(node.value + ":");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            emit("sub $256, %rsp");
            
            // Save current function state
            auto savedOffsets = varOffsets;
            auto savedTypes = varTypes;
            int savedStackOffset = stackOffset;
            
            clearLocalScope();
            
            // Parameters passed in registers: %rdi, %rsi, %rdx, %rcx, %r8, %r9
            std::vector<std::string> paramRegs = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
            for (size_t i = 0; i < node.attrs.size() && i < paramRegs.size(); i++) {
                std::string param = node.attrs[i];
                int offset = -8 - (int)(i * 8);
                emit("mov " + paramRegs[i] + ", " + std::to_string(offset) + "(%rbp)  # parameter " + param);
                varOffsets[param] = offset;
                varTypes[param] = std::make_shared<Type>(TypeKind::Numeral);
            }
            // Set stackOffset to continue after parameters
            stackOffset = -8 - (int)(node.attrs.size() * 8);
            
            if (!node.children.empty() && !node.children[0]->children.empty()) {
                for (auto& c : node.children[0]->children) {
                    genNode(*c);
                }
            }
            
            // If no explicit return, return 0
            emit("mov $0, %rax");
            emitEpilogue();
            
            // Restore caller state
            varOffsets = savedOffsets;
            varTypes = savedTypes;
            stackOffset = savedStackOffset;
            currentOut = &text_main;  // Switch back to main output
            break;
        }

        case NodeType::IfStmt: {
            std::string label_end = newLabel();
            std::string cur_label = newLabel();

            auto emitCondition = [&](const std::string& cond, const std::string& nextLabel) {
                // Converts conditions to compares
                std::string parsed = translateCondition(cond);
                
                // Check for comparison operators
                std::string op;
                std::string jmpInstr;
                size_t opPos = std::string::npos;
                
                // Check for <=, >=, ==, !=, <, >
                if ((opPos = parsed.find("<=")) != std::string::npos) {
                    op = "<=";
                    jmpInstr = "jg";  // jump if greater (NOT <=)
                } else if ((opPos = parsed.find(">=")) != std::string::npos) {
                    op = ">=";
                    jmpInstr = "jl";  // jump if less (NOT >=)
                } else if ((opPos = parsed.find("==")) != std::string::npos) {
                    op = "==";
                    jmpInstr = "jne";  // jump if not equal
                } else if ((opPos = parsed.find("!=")) != std::string::npos) {
                    op = "!=";
                    jmpInstr = "je";  // jump if equal (NOT !=)
                } else if ((opPos = parsed.find("<")) != std::string::npos) {
                    op = "<";
                    jmpInstr = "jge";  // jump if greater or equal (NOT <)
                } else if ((opPos = parsed.find(">")) != std::string::npos) {
                    op = ">";
                    jmpInstr = "jle";  // jump if less or equal (NOT >)
                }
                
                if (opPos != std::string::npos) {
                    std::string lhs = parsed.substr(0, opPos);
                    std::string rhs = parsed.substr(opPos + op.length());
                    // Trim
                    while (!lhs.empty() && std::isspace(lhs.back())) lhs.pop_back();
                    while (!lhs.empty() && std::isspace(lhs.front())) lhs.erase(lhs.begin());
                    while (!rhs.empty() && std::isspace(rhs.back())) rhs.pop_back();
                    while (!rhs.empty() && std::isspace(rhs.front())) rhs.erase(rhs.begin());

                    // Evaluate LHS
                    auto it = varOffsets.find(lhs);
                    if (it != varOffsets.end()) {
                        emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
                    } else {
                        evalExprStr(lhs);
                    }
                    
                    // Compare with RHS
                    auto rhsIt = varOffsets.find(rhs);
                    if (rhsIt != varOffsets.end()) {
                        emit("cmp " + std::to_string(rhsIt->second) + "(%rbp), %rax");
                    } else {
                        emit("cmp $" + rhs + ", %rax");
                    }
                    
                    // Jump to nextLabel if condition is FALSE
                    emit(jmpInstr + " " + nextLabel);
                } else {
                    emit("# unsupported condition: " + cond);
                    emit("jmp " + nextLabel);
                }
            };

            // First IF block
            if (node.value != "OTHER") {
                emitCondition(node.value, cur_label);
                if (!node.children.empty()) {
                    for (auto& c : node.children[0]->children) genNode(*c);
                }
                emit("jmp " + label_end);
                emitRaw(cur_label + ":");
            } else {
                // direct other branch if NO condition
                if (!node.children.empty()) {
                    for (auto& c : node.children[0]->children) genNode(*c);
                }
                emit("jmp " + label_end);
            }

            // ElseIf/OTHER chain
            for (auto& child : node.children) {
                if (child->type == NodeType::ElseIfStmt) {
                    std::string next_label = newLabel();
                    emitCondition(child->value, next_label);
                    if (!child->children.empty()) {
                        for (auto& c : child->children[0]->children) genNode(*c);
                    }
                    emit("jmp " + label_end);
                    emitRaw(next_label + ":");
                } else if (child->type == NodeType::IfStmt && child->value == "OTHER") {
                    if (!child->children.empty()) {
                        for (auto& c : child->children[0]->children) genNode(*c);
                    }
                    emit("jmp " + label_end);
                }
            }

            emitRaw(label_end + ":");
            break;
        }

        case NodeType::ForLoop: {
            // FOR item in list
            std::string loopVar = node.value;
            std::string collectionName = node.attrs.empty() ? "" : node.attrs[0];

            // Allocate loop variable once
            int loopVarOffset = allocateStackVariable(loopVar, std::make_shared<Type>(TypeKind::Numeral));

            std::string label_loop = newLabel();
            std::string label_end = newLabel();

            // Handle list iteration
            auto it = varOffsets.find(collectionName);
            if (it != varOffsets.end()) {
                int listVarOffset = it->second;

                // Counter offset on stack - allocate once at loop entry
                int counterOffset = stackOffset;
                stackOffset -= 8;
                emit("mov $0, " + std::to_string(counterOffset) + "(%rbp)  # loop counter");

                emitRaw(label_loop + ":");
                // Load counter and list pointer
                emit("mov " + std::to_string(counterOffset) + "(%rbp), %rax");
                emit("mov " + std::to_string(listVarOffset) + "(%rbp), %rcx  # list pointer");
                emit("mov (%rcx), %rdx  # list length");
                emit("cmp %rdx, %rax");
                emit("jge " + label_end);

                // Load element from list (0-based index)
                emit("lea 8(%rcx), %rdx");
                emit("mov (%rdx, %rax, 8), %rdi");
                emit("mov %rdi, " + std::to_string(loopVarOffset) + "(%rbp)  # " + loopVar);

                // Execute loop body
                if (!node.children.empty()) {
                    for (auto& c : node.children[0]->children) genNode(*c);
                }

                // Increment counter
                emit("incq " + std::to_string(counterOffset) + "(%rbp)");
                emit("jmp " + label_loop);
            } else {
                emit("# unknown collection: " + collectionName);
            }

            emitRaw(label_end + ":");
            break;
        }

        case NodeType::WhilstLoop: {
            std::string label_loop = newLabel();
            std::string label_end = newLabel();
            emit("# while loop");
            emitRaw(label_loop + ":");
            
            // Evaluate condition
            std::string cond = node.value;
            std::string parsed = translateCondition(cond);
            
            // Check for comparison operators
            std::string op;
            std::string jmpInstr;
            size_t opPos = std::string::npos;
            
            if ((opPos = parsed.find("<=")) != std::string::npos) {
                op = "<=";
                jmpInstr = "jg";  // jump if greater (NOT <=)
            } else if ((opPos = parsed.find(">=")) != std::string::npos) {
                op = ">=";
                jmpInstr = "jl";  // jump if less (NOT >=)
            } else if ((opPos = parsed.find("==")) != std::string::npos) {
                op = "==";
                jmpInstr = "jne";  // jump if not equal
            } else if ((opPos = parsed.find("!=")) != std::string::npos) {
                op = "!=";
                jmpInstr = "je";  // jump if equal (NOT !=)
            } else if ((opPos = parsed.find("<")) != std::string::npos) {
                op = "<";
                jmpInstr = "jge";  // jump if greater or equal (NOT <)
            } else if ((opPos = parsed.find(">")) != std::string::npos) {
                op = ">";
                jmpInstr = "jle";  // jump if less or equal (NOT >)
            }
            
            if (opPos != std::string::npos) {
                std::string lhs = parsed.substr(0, opPos);
                std::string rhs = parsed.substr(opPos + op.length());
                while (!lhs.empty() && std::isspace(lhs.back())) lhs.pop_back();
                while (!lhs.empty() && std::isspace(lhs.front())) lhs.erase(lhs.begin());
                while (!rhs.empty() && std::isspace(rhs.back())) rhs.pop_back();
                while (!rhs.empty() && std::isspace(rhs.front())) rhs.erase(rhs.begin());

                auto it = varOffsets.find(lhs);
                if (it != varOffsets.end()) {
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
                } else {
                    evalExprStr(lhs);
                }
                
                auto rhsIt = varOffsets.find(rhs);
                if (rhsIt != varOffsets.end()) {
                    emit("cmp " + std::to_string(rhsIt->second) + "(%rbp), %rax");
                } else {
                    emit("cmp $" + rhs + ", %rax");
                }
                
                emit(jmpInstr + " " + label_end);
            }
            
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emit("jmp " + label_loop);
            emitRaw(label_end + ":");
            break;
        }

        case NodeType::ListLiteral:
            emit("# list literal - allocation needed");
            // TODO: Allocate list on heap and store pointer
            break;

        case NodeType::TupleLiteral:
            emit("# tuple literal - immutable");
            break;

        case NodeType::BinaryExpr: {
            // Evaluate binary expression with proper precedence
            evalExprStr(node.value);
            break;
        }

        case NodeType::IndexExpr: {
            // list[index] or list[index] = value
            std::string listName = node.value;
            std::string indexStr = node.attrs.empty() ? "0" : node.attrs[0];
            bool isAssignment = (node.attrs.size() > 1);
            std::string valueStr = isAssignment ? node.attrs[1] : "";

            auto it = varOffsets.find(listName);
            if (it != varOffsets.end()) {
                // Compute index
                evalExprStr(indexStr);
                emit("mov %rax, %rbx  # index");

                // Get list pointer
                emit("mov " + std::to_string(it->second) + "(%rbp), %rcx  # list pointer");

                if (isAssignment) {
                    // Evaluate new value into %rax - apply translateExpr
                    std::string translatedValue = translateExpr(valueStr);
                    if (translatedValue.size() >= 2 && translatedValue.front() == '$' && translatedValue.back() == '$') {
                        std::string text = translatedValue.substr(1, translatedValue.size() - 2);
                        std::string label = allocateString(text);
                        emit("lea " + label + "(%rip), %rax");
                    } else {
                        evalExprStr(translatedValue);
                    }

                    emit("lea 8(%rcx), %rdx");
                    emit("mov %rax, (%rdx, %rbx, 8)");
                } else {
                    // Read value
                    emit("lea 8(%rcx), %rdx");
                    emit("mov (%rdx, %rbx, 8), %rax");
                }
            } else {
                emit("# unknown list: " + listName);
                emit("xor %rax, %rax");
            }
            break;
        }

        case NodeType::TagBlock:
            genTagBlock(node);
            break;

        case NodeType::Block:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::CustomTagDef:
            emitRaw("spawn_" + node.value + ":");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emit("pop %rbp");
            emit("ret");
            break;

        case NodeType::SpawnStmt: {
            std::string name = node.value;
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("call spawn_" + lower);
            break;
        }

        case NodeType::EventListener:
            // Generate event listener support in ASM
            emit("# Event listener configuration");
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    std::string callback = child->attrs.empty() ? "" : child->attrs[0];
                    if (!callback.empty()) {
                        emit("# Bind key '" + key + "' to " + callback);
                        // In a real implementation, this would set up interrupt handlers
                        // or polling loops to check keyboard state
                    }
                }
            }
            break;

        case NodeType::KeyBinding:
            // Handled by EventListener
            break;

        case NodeType::InputStmt: {
            // Send ghost/simulated keyboard input
            std::string key = node.value;
            emit("# Simulate key press: " + key);
            emit("# In real implementation, this would trigger interrupt or system call");
            break;
        }

        case NodeType::WhenBlock:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::PlusEqualStmt: {
            if (!node.attrs.empty()) {
                std::string varName = node.value;
                std::string val = node.attrs[0];
                auto it = varOffsets.find(varName);
                if (it != varOffsets.end()) {
                    evalExprStr(val);
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rbx");
                    emit("add %rax, %rbx");
                    emit("mov %rbx, " + std::to_string(it->second) + "(%rbp)");
                }
            }
            break;
        }

        case NodeType::MinusEqualStmt: {
            if (!node.attrs.empty()) {
                std::string varName = node.value;
                std::string val = node.attrs[0];
                auto it = varOffsets.find(varName);
                if (it != varOffsets.end()) {
                    evalExprStr(val);
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rbx");
                    emit("sub %rax, %rbx");
                    emit("mov %rbx, " + std::to_string(it->second) + "(%rbp)");
                }
            }
            break;
        }

        case NodeType::MultiplyEqualStmt: {
            if (!node.attrs.empty()) {
                std::string varName = node.value;
                std::string val = node.attrs[0];
                auto it = varOffsets.find(varName);
                if (it != varOffsets.end()) {
                    evalExprStr(val);
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rbx");
                    emit("imul %rax, %rbx");
                    emit("mov %rbx, " + std::to_string(it->second) + "(%rbp)");
                }
            }
            break;
        }

        case NodeType::DivideEqualStmt: {
            if (!node.attrs.empty()) {
                std::string varName = node.value;
                std::string val = node.attrs[0];
                auto it = varOffsets.find(varName);
                if (it != varOffsets.end()) {
                    evalExprStr(val);
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
                    emit("cqo");
                    emit("idiv %rbx");
                    emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
                }
            }
            break;
        }

        case NodeType::AtEqualStmt: {
            if (!node.attrs.empty()) {
                std::string varName = node.value;
                std::string val = node.attrs[0];
                auto it = varOffsets.find(varName);
                if (it != varOffsets.end()) {
                    evalExprStr(val);
                    emit("mov %rax, %rdx");  // Move right operand to rdx
                    emit("mov " + std::to_string(it->second) + "(%rbp), %rax");  // Load left operand
                    emit("imul %rdx");  // Multiply: rax = rax * rdx
                    emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");  // Store result
                }
            }
            break;
        }

        default: break;
        }
    }

    void genTagBlock(const ASTNode& node) {
        std::string name = node.value;
        if (Tags::isMainLoop(name)) {
            std::string label_loop = newLabel();
            std::string label_end = newLabel();
            emitRaw(label_loop + ":");
            for (auto& c : node.children) genNode(*c);
            emit("jmp " + label_loop);
            emitRaw(label_end + ":");
        } else if (Tags::isGUIBox(name)) {
            // GUI — skip
        } else if (Tags::isLogicScope(name)) {
            for (auto& c : node.children) genNode(*c);
        } else if (Tags::isStartHere(name)) {
            std::string label_loop = newLabel();
            emitRaw(label_loop + ":");
            for (auto& c : node.children) genNode(*c);
            emit("jmp " + label_loop);
        } else {
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("call spawn_" + lower);
            for (auto& c : node.children) genNode(*c);
        }
    }

public:
    std::string generate(const ASTNode& node) {
        genNode(node);
        std::ostringstream output;
        output << rodata.str();
        // Add .section .text before functions
        if (!text_funcs.str().empty()) {
            output << ".section .text\n";
            output << text_funcs.str();
        }
        output << text_main.str();
        // Emit string section
        if (!stringTable.empty()) {
            output << ".section .rodata\n";
            for (auto& [content, id] : stringTable) {
                output << ".str_" << id << ": .asciz \"" << content << "\"\n";
            }
            output << ".section .text\n";
        }
        // Add GNU stack note to avoid executable stack warning
        output << ".section .note.GNU-stack,\"\",@progbits\n";
        return output.str();
    }
};

std::string generateAsm(const ASTNode& ast) {
    AsmCodeGen gen;
    return gen.generate(ast);
}
