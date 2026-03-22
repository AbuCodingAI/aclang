#include "../include/ast.hpp"
#include "../include/type.hpp"
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

class AsmCodeGen {
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
        auto tokens = tokenizeExpr(expr);


        size_t pos = 0;
        parseAddSub(tokens, pos);
    }

    // Load variable or number into %eax
    void loadOperand(const std::string& operand) {
        if (operand.empty()) return;
        
        // Check if it's a number
        if (std::isdigit(operand[0]) || (operand[0] == '-' && operand.size() > 1)) {
            emit("mov $" + operand + ", %eax");
        } else {
            // It's a variable
            auto it = varOffsets.find(operand);
            if (it != varOffsets.end()) {
                emit("mov " + std::to_string(it->second) + "(%rbp), %eax");
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
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    result += s.substr(i + 1, end - i - 1);
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
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        // is -> == (equality keyword)
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.substr(0, 3) == "is ") r.replace(0, 3, "");

        // Single '=' should be treated as equality too
        if (r.find("==") == std::string::npos && r.find("!=") == std::string::npos) {
            size_t pos = r.find('=');
            if (pos != std::string::npos) {
                // Avoid replacing assignment operators in unsupported contexts
                r.replace(pos, 1, "==");
            }
        }
        return r;
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
            
            text_main << ".section .text\n";
            text_main << ".globl main\n";
            
            // First pass: collect functions
            collectingFunctions = true;
            std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef) {
                    genNode(n);
                }
                for (auto& c : n.children) {
                    collectFuncs(*c);
                }
            };
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
            
            // Second pass: emit main body
            for (auto& c : node.children) genNode(*c);
            
            emit("xor %eax, %eax");
            emitEpilogue();
            
            // Final assembly output
            std::cout << rodata.str();
            std::cout << text_funcs.str();
            std::cout << text_main.str();
            emitStringSection();
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt:  break;
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
                emit("xor %eax, %eax");  // return 0
            } else {
                evalExprStr(val);  // Result in %eax
            }

            emitEpilogue();
            break;
        }

        case NodeType::AssignStmt: {
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                TypePtr varType = nullptr;

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
                            // numeric or expression
                            if (elem == "true" || elem == "false") {
                                emit("mov $" + std::string(elem == "true" ? "1" : "0") + ", %rax");
                            } else if (!elem.empty() && (std::isdigit(elem[0]) || elem[0] == '-')) {
                                emit("mov $" + elem + ", %rax");
                            } else {
                                // variable or expression
                                evalExprStr(elem);
                            }
                            emit("mov " + std::to_string(offset) + "(%rbp), %rcx");
                            emit("mov %rax, " + std::to_string(8 * (i + 1)) + "(%rcx)");
                        }
                    }
                    break;
                }

                // Regular numeric/string assignment or expression
                if (val == "true" || val == "false") {
                    emit("mov $" + std::string(val == "true" ? "1" : "0") + ", %rax");
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                } else if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                    std::string text = val.substr(1, val.size() - 2);
                    std::string label = allocateString(text);
                    emit("lea " + label + "(%rip), %rax");
                    varType = std::make_shared<Type>(TypeKind::String);
                } else {
                    evalExprStr(val);
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
            std::string arg = node.attrs.empty() ? "" : node.attrs[0];
            
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
            
            // Parameter passed in %edi, move to stack
            emit("mov %edi, -8(%rbp)  # parameter " + arg);
            varOffsets[arg] = -8;
            varTypes[arg] = std::make_shared<Type>(TypeKind::Numeral);
            stackOffset = -8;
            
            if (!node.children.empty() && !node.children[0]->children.empty()) {
                for (auto& c : node.children[0]->children) {
                    genNode(*c);
                }
            }
            
            // If no explicit return, return 0
            emit("mov $0, %eax");
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
                // Converts `x = y` and `x #= y` to compares
                std::string parsed = translateCondition(cond);
                size_t eqPos = parsed.find("==");
                bool isNot = false;
                if (eqPos == std::string::npos) {
                    eqPos = parsed.find("!=");
                    isNot = true;
                }
                if (eqPos != std::string::npos) {
                    std::string lhs = parsed.substr(0, eqPos);
                    std::string rhs = parsed.substr(eqPos + (isNot ? 2 : 2));
                    // Trim
                    while (!lhs.empty() && std::isspace(lhs.back())) lhs.pop_back();
                    while (!lhs.empty() && std::isspace(lhs.front())) lhs.erase(lhs.begin());
                    while (!rhs.empty() && std::isspace(rhs.back())) rhs.pop_back();
                    while (!rhs.empty() && std::isspace(rhs.front())) rhs.erase(rhs.begin());

                    auto it = varOffsets.find(lhs);
                    if (it != varOffsets.end()) {
                        emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
                        emit("cmp $" + rhs + ", %rax");
                        if (isNot) emit("je " + nextLabel);
                        else emit("jne " + nextLabel);
                    } else {
                        emit("# condition: " + cond);
                        emit("jmp " + nextLabel);
                    }
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
            emit("cmp $0, %eax");
            emit("je " + label_end);
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
                    // Evaluate new value into %rax
                    if (valueStr == "true" || valueStr == "false") {
                        emit("mov $" + std::string(valueStr == "true" ? "1" : "0") + ", %rax");
                    } else if (valueStr.size() >= 2 && valueStr.front() == '$' && valueStr.back() == '$') {
                        std::string text = valueStr.substr(1, valueStr.size() - 2);
                        std::string label = allocateString(text);
                        emit("lea " + label + "(%rip), %rax");
                    } else {
                        evalExprStr(valueStr);
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
                emit("xor %eax, %eax");
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
        case NodeType::WhenBlock:
            for (auto& c : node.children) genNode(*c);
            break;

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
        emitStringSection();
        return text_main.str() + text_funcs.str() + rodata.str();
    }
};

std::string generateAsm(const ASTNode& ast) {
    AsmCodeGen gen;
    return gen.generate(ast);
}
