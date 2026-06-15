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
#include <set>
#include <functional>
#include <regex>

struct ExprElement {
    std::string value;
    bool isOperator;
};

class AsmCodeGen : public BaseCodeGen {
    // === OUTPUT SECTIONS ===
    std::ostringstream rodata;
    std::ostringstream text_main;
    std::ostringstream text_funcs;
    std::ostringstream* currentOut;

    // === SYMBOL TABLES ===
    std::map<std::string, int>     varOffsets;
    std::map<std::string, TypePtr> varTypes;
    std::map<std::string, int>     stringTable;
    std::set<std::string>          globalVars;   // vars declared with free

    // === CONTROL FLOW STACKS ===
    std::vector<std::string> breakStack;     // loop-end / if-chain-end labels
    std::vector<std::string> continueStack;  // loop-start labels

    // === STATE ===
    int  labelCounter        = 0;
    int  stackOffset         = 0;
    int  nextStringId        = 0;
    bool collectingFunctions = false;
    int  sprintfBufId        = 0;  // unique ids for sprintf buffers

    // === STRING TABLE ===
    std::string allocateString(const std::string& content) {
        if (!stringTable.count(content))
            stringTable[content] = nextStringId++;
        return ".str_" + std::to_string(stringTable[content]);
    }

    // === STACK ===
    int allocateStackVariable(const std::string& name, TypePtr type) {
        stackOffset -= 8;
        varOffsets[name] = stackOffset;
        varTypes[name]   = type;
        return stackOffset;
    }

    void clearLocalScope() {
        varOffsets.clear();
        varTypes.clear();
        stackOffset = 0;
    }

    // === LABELS ===
    std::string newLabel() { return ".L" + std::to_string(labelCounter++); }

    void emit(const std::string& line)    { *currentOut << "    " << line << "\n"; }
    void emitRaw(const std::string& line) { *currentOut << line << "\n"; }

    void emitEpilogue() {
        emit("add $256, %rsp");
        emit("pop %rbp");
        emit("ret");
    }

    // === EXPRESSION TOKENIZER ===
    std::vector<ExprElement> tokenizeExpr(const std::string& expr) {
        std::vector<ExprElement> tokens;
        std::string current;
        std::string input = expr;

        if (input.size() >= 3 && input.substr(0, 3) == "fn ")
            input = input.substr(3);

        for (size_t i = 0; i < input.size(); i++) {
            char c = input[i];
            if (c == ' ') {
                if (!current.empty()) {
                    // check if it's "xsub" keyword
                    tokens.push_back({current, current == "xsub"});
                    current.clear();
                }
            } else if (c == '(' || c == ')') {
                if (!current.empty()) {
                    tokens.push_back({current, current == "xsub"});
                    current.clear();
                }
                tokens.push_back({std::string(1, c), true});
            } else if (c == '|') {
                if (!current.empty()) { tokens.push_back({current, false}); current.clear(); }
                tokens.push_back({"|", true});
            } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
                if (c == '-' && current.empty()) {
                    current += c;
                } else {
                    if (!current.empty()) { tokens.push_back({current, false}); current.clear(); }
                    tokens.push_back({std::string(1, c), true});
                }
            } else {
                current += c;
            }
        }
        if (!current.empty())
            tokens.push_back({current, current == "xsub"});
        return tokens;
    }

    void evalExprStr(const std::string& exprStr) {
        std::string expr = exprStr;
        if (expr.size() >= 6 && expr.substr(0, 6) == "__fn__") expr = expr.substr(6);
        for (size_t p = 0; (p = expr.find("@", p)) != std::string::npos;)
            expr.replace(p, 1, "*"), p++;
        auto tokens = tokenizeExpr(expr);
        size_t pos = 0;
        parseBitwise(tokens, pos);
    }

    void loadOperand(const std::string& operand) {
        if (operand.empty()) return;
        if (std::isdigit(operand[0]) || (operand[0] == '-' && operand.size() > 1 && std::isdigit(operand[1]))) {
            emit("mov $" + operand + ", %rax");
        } else {
            auto it = varOffsets.find(operand);
            if (it != varOffsets.end())
                emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
            else
                emit("# unknown operand: " + operand);
        }
    }

    // === PRECEDENCE CLIMBING ===
    // Levels (low → high): bitwise | → add/sub/xsub → mul/div/mod → unary → primary

    void parseBitwise(const std::vector<ExprElement>& tokens, size_t& pos) {
        parseAddSub(tokens, pos);
        while (pos < tokens.size() && tokens[pos].isOperator && tokens[pos].value == "|") {
            pos++;
            emit("push %rax");
            parseAddSub(tokens, pos);
            emit("mov %rax, %rdx");
            emit("pop %rax");
            emit("xor %rdx, %rax");   // XOR
        }
    }

    void parseAddSub(const std::vector<ExprElement>& tokens, size_t& pos) {
        parseMulDiv(tokens, pos);
        while (pos < tokens.size() && tokens[pos].isOperator &&
               (tokens[pos].value == "+" || tokens[pos].value == "-" || tokens[pos].value == "xsub")) {
            std::string op = tokens[pos++].value;
            emit("push %rax");
            parseMulDiv(tokens, pos);
            emit("mov %rax, %rcx");
            emit("pop %rax");

            if (op == "+") {
                emit("add %rcx, %rax");
            } else if (op == "-") {
                emit("sub %rcx, %rax");
            } else {
                // xsub: |a - b| + 1
                emit("sub %rcx, %rax");   // a - b
                emit("mov %rax, %rcx");
                emit("neg %rcx");
                emit("cmovs %rcx, %rax"); // if negative, use -(a-b)
                emit("inc %rax");         // +1
            }
        }
    }

    void parseMulDiv(const std::vector<ExprElement>& tokens, size_t& pos) {
        parsePrimary(tokens, pos);
        while (pos < tokens.size() && tokens[pos].isOperator &&
               (tokens[pos].value == "*" || tokens[pos].value == "/" || tokens[pos].value == "%")) {
            std::string op = tokens[pos++].value;
            emit("push %rax");
            parsePrimary(tokens, pos);
            emit("mov %rax, %rcx");
            emit("pop %rax");
            if (op == "*") {
                emit("imul %rcx");
            } else {
                emit("cqo");
                emit("idiv %rcx");
                if (op == "%") emit("mov %rdx, %rax");
            }
        }
    }

    void parsePrimary(const std::vector<ExprElement>& tokens, size_t& pos) {
        if (pos >= tokens.size()) return;

        // Unary minus
        if (tokens[pos].value == "-" && tokens[pos].isOperator) {
            pos++;
            parsePrimary(tokens, pos);
            emit("neg %rax");
            return;
        }
        // Parenthesised expression
        if (tokens[pos].value == "(") {
            pos++;
            parseBitwise(tokens, pos);
            if (pos < tokens.size() && tokens[pos].value == ")") pos++;
            return;
        }
        // Function call: name(...)
        if (!tokens[pos].isOperator && pos + 1 < tokens.size() && tokens[pos + 1].value == "(") {
            std::string name = tokens[pos].value;
            pos += 2;
            // collect args
            std::vector<size_t> argStarts;
            argStarts.push_back(pos);
            int depth = 1;
            size_t start = pos;
            while (pos < tokens.size() && depth > 0) {
                if (tokens[pos].value == "(") depth++;
                else if (tokens[pos].value == ")") { depth--; if (depth == 0) break; }
                pos++;
            }
            // evaluate single arg (multi-arg calls handled via evalExprStr for each arg)
            // For simplicity: evaluate entire arg range as one expression
            size_t argEnd = pos;
            if (pos < tokens.size()) pos++; // skip )
            // Re-evaluate first arg from start..argEnd
            size_t argPos = start;
            parseBitwise(tokens, argPos);
            emit("mov %rax, %rdi");
            emit("call " + name);
            return;
        }
        loadOperand(tokens[pos].value);
        pos++;
    }

    // === CONDITION TRANSLATION ===
    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && std::isspace(r.back()))  r.pop_back();
        while (!r.empty() && std::isspace(r.front())) r.erase(r.begin());
        r = unwrapDollars(r);

        std::regex trueRe(R"(\b(true|True|TRUE)\b)",    std::regex::icase);
        std::regex falseRe(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        std::regex nullRe(R"(\bnull\b)");
        r = std::regex_replace(r, trueRe,  "1");
        r = std::regex_replace(r, falseRe, "0");
        r = std::regex_replace(r, nullRe,  "0");

        // AC relational operators (longest first to avoid partial matches)
        for (size_t p = 0; (p = r.find("#|", p)) != std::string::npos;)  // XNOR → ==
            r.replace(p, 2, "=="), p += 2;
        for (size_t p = 0; (p = r.find("#>", p)) != std::string::npos;)  // NOT greater → <=
            r.replace(p, 2, "<="), p += 2;
        for (size_t p = 0; (p = r.find("#<", p)) != std::string::npos;)  // NOT less → >=
            r.replace(p, 2, ">="), p += 2;
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)  // NOT equal → !=
            r.replace(p, 2, "!="), p += 2;

        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.size() >= 3 && r.substr(0, 3) == "is ") r.replace(0, 3, "");

        // bare = → ==  (only if no compound op already present)
        if (r.find("==") == std::string::npos && r.find("!=") == std::string::npos &&
            r.find("<=") == std::string::npos && r.find(">=") == std::string::npos) {
            size_t pos = r.find('=');
            if (pos != std::string::npos) r.replace(pos, 1, "==");
        }
        return r;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && std::isspace(r.back()))  r.pop_back();
        while (!r.empty() && std::isspace(r.front())) r.erase(r.begin());
        r = unwrapDollars(r);

        std::regex trueRe(R"(\b(true|True|TRUE)\b)",    std::regex::icase);
        std::regex falseRe(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        std::regex nullRe(R"(\bnull\b)");
        r = std::regex_replace(r, trueRe,  "1");
        r = std::regex_replace(r, falseRe, "0");
        r = std::regex_replace(r, nullRe,  "0");

        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;)
            r.replace(p, 1, "*"), p++;
        return r;
    }

    std::string unwrapDollars(const std::string& s) {
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string esc;
            for (char c : s.substr(1, s.size() - 2)) {
                if      (c == '"')  esc += "\\\"";
                else if (c == '\\') esc += "\\\\";
                else if (c == '\n') esc += "\\n";
                else if (c == '\t') esc += "\\t";
                else esc += c;
            }
            return esc;
        }
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    std::string esc;
                    for (char c : s.substr(i + 1, end - i - 1)) {
                        if      (c == '"')  esc += "\\\"";
                        else if (c == '\\') esc += "\\\\";
                        else if (c == '\n') esc += "\\n";
                        else if (c == '\t') esc += "\\t";
                        else esc += c;
                    }
                    result += esc;
                    i = end;
                    continue;
                }
            }
            result += s[i];
        }
        return result;
    }

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return v.substr(1, v.size() - 2);
        return v;
    }

    std::vector<std::string> splitListElements(const std::string& contents) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : contents) {
            if (c == ',') {
                while (!cur.empty() && cur.back()  == ' ') cur.pop_back();
                while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            } else { cur += c; }
        }
        while (!cur.empty() && cur.back()  == ' ') cur.pop_back();
        while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    // === CONDITION BRANCH EMITTER ===
    // Emits a conditional jump to jumpLabel if condition is FALSE.
    void emitBranchIfFalse(const std::string& cond, const std::string& jumpLabel) {
        std::string parsed = translateCondition(cond);

        struct OpEntry { std::string op; std::string jmp; };
        static const OpEntry ops[] = {
            {"<=","jg"}, {">=","jl"}, {"==","jne"},
            {"!=","je"}, {"<","jge"}, {">","jle"}
        };

        std::string foundOp, jmpInstr;
        size_t opPos = std::string::npos;
        for (auto& e : ops) {
            size_t p = parsed.find(e.op);
            if (p != std::string::npos) { foundOp = e.op; jmpInstr = e.jmp; opPos = p; break; }
        }

        if (opPos == std::string::npos) {
            emit("# unsupported condition: " + cond);
            emit("jmp " + jumpLabel);
            return;
        }

        std::string lhs = parsed.substr(0, opPos);
        std::string rhs = parsed.substr(opPos + foundOp.size());
        while (!lhs.empty() && std::isspace(lhs.back()))  lhs.pop_back();
        while (!lhs.empty() && std::isspace(lhs.front())) lhs.erase(lhs.begin());
        while (!rhs.empty() && std::isspace(rhs.back()))  rhs.pop_back();
        while (!rhs.empty() && std::isspace(rhs.front())) rhs.erase(rhs.begin());

        auto lhsIt = varOffsets.find(lhs);
        if (lhsIt != varOffsets.end())
            emit("mov " + std::to_string(lhsIt->second) + "(%rbp), %rax");
        else
            evalExprStr(lhs);

        auto rhsIt = varOffsets.find(rhs);
        if (rhsIt != varOffsets.end())
            emit("cmp " + std::to_string(rhsIt->second) + "(%rbp), %rax");
        else
            emit("cmp $" + rhs + ", %rax");

        emit(jmpInstr + " " + jumpLabel);
    }

    // === STRUCTURED EXPRESSION NODES ===
    // Evaluates a structured AST expr node, result in %rax
    void evalExprNode(const ASTNode& node) {
        switch (node.type) {
        case NodeType::LiteralExpr: {
            std::string typ = node.attrs.empty() ? "INT" : node.attrs[0];
            if (typ == "INT" || typ == "FLOAT" || typ == "BOOL") {
                emit("mov $" + node.value + ", %rax");
            } else if (typ == "STRING") {
                std::string lbl = allocateString(node.value);
                emit("lea " + lbl + "(%rip), %rax");
            } else if (typ == "NULL") {
                emit("xor %rax, %rax");
            }
            break;
        }
        case NodeType::Identifier: {
            auto it = varOffsets.find(node.value);
            if (it != varOffsets.end())
                emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
            else
                emit("# unknown var: " + node.value);
            break;
        }
        case NodeType::UnaryExpr: {
            if (node.children.empty()) break;
            evalExprNode(*node.children[0]);
            if (node.value == "-") {
                emit("neg %rax");
            } else if (node.value == "NOT" || node.value == "#") {
                // logical NOT: result is 1 if rax==0, else 0
                emit("test %rax, %rax");
                emit("sete %al");
                emit("movzx %al, %rax");
            }
            break;
        }
        case NodeType::BinaryExpr: {
            if (node.children.size() < 2) {
                // legacy string form
                evalExprStr(node.value);
                break;
            }
            std::string op = node.value;
            evalExprNode(*node.children[0]);
            emit("push %rax");
            evalExprNode(*node.children[1]);
            emit("mov %rax, %rcx");
            emit("pop %rax");

            if      (op == "+")    emit("add %rcx, %rax");
            else if (op == "-")    emit("sub %rcx, %rax");
            else if (op == "*")    emit("imul %rcx");
            else if (op == "@")    emit("imul %rcx");
            else if (op == "/")  { emit("cqo"); emit("idiv %rcx"); }
            else if (op == "%")  { emit("cqo"); emit("idiv %rcx"); emit("mov %rdx, %rax"); }
            else if (op == "|")    emit("xor %rcx, %rax");
            else if (op == "xsub") {
                emit("sub %rcx, %rax");
                emit("mov %rax, %rcx");
                emit("neg %rcx");
                emit("cmovs %rcx, %rax");
                emit("inc %rax");
            }
            // Comparisons → 0 or 1 in %rax
            else if (op == "==" || op == "is") { emit("cmp %rcx, %rax"); emit("sete %al");  emit("movzx %al, %rax"); }
            else if (op == "!=" || op == "#=") { emit("cmp %rcx, %rax"); emit("setne %al"); emit("movzx %al, %rax"); }
            else if (op == "<")                { emit("cmp %rcx, %rax"); emit("setl %al");  emit("movzx %al, %rax"); }
            else if (op == ">")                { emit("cmp %rcx, %rax"); emit("setg %al");  emit("movzx %al, %rax"); }
            else if (op == "<=" || op == "#>") { emit("cmp %rcx, %rax"); emit("setle %al"); emit("movzx %al, %rax"); }
            else if (op == ">=" || op == "#<") { emit("cmp %rcx, %rax"); emit("setge %al"); emit("movzx %al, %rax"); }
            // AND / OR
            else if (op == "AND") { emit("test %rax, %rax"); emit("setne %al"); emit("movzx %al, %rax");
                                    emit("test %rcx, %rcx"); emit("setne %cl"); emit("movzx %cl, %rcx");
                                    emit("and %rcx, %rax"); }
            else if (op == "OR")  { emit("or %rcx, %rax"); emit("setne %al"); emit("movzx %al, %rax"); }
            break;
        }
        case NodeType::CallExpr: {
            static const char* argRegs[] = {"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
            for (size_t i = 0; i < node.children.size() && i < 6; i++) {
                evalExprNode(*node.children[i]);
                emit("mov %rax, " + std::string(argRegs[i]));
            }
            emit("call " + node.value);
            break;
        }
        default:
            evalExprStr(node.value);
            break;
        }
    }

    // === HELPER: emit a display of %rax or %rsi ===
    void emitDisplayRax(bool isString) {
        if (isString) {
            emit("mov %rax, %rsi");
            emit("lea fmt_str(%rip), %rdi");
        } else {
            emit("mov %rax, %rsi");
            emit("lea fmt_int(%rip), %rdi");
        }
        emit("xor %eax, %eax");
        emit("call printf");
    }

    // === COMPOUND ASSIGN HELPER ===
    void emitCompoundAssign(const ASTNode& node, const std::string& asmOp) {
        if (node.attrs.empty()) return;
        auto it = varOffsets.find(node.value);
        if (it == varOffsets.end()) return;
        evalExprStr(node.attrs[0]);
        emit(asmOp + " %rax, " + std::to_string(it->second) + "(%rbp)");
    }

    // === AST TRAVERSAL ===
    bool usesEventListener(const ASTNode& node) {
        if (node.type == NodeType::EventListener ||
            node.type == NodeType::KeyBinding ||
            node.type == NodeType::InputStmt) return true;
        for (auto& child : node.children)
            if (usesEventListener(*child)) return true;
        return false;
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        // ── PROGRAM ────────────────────────────────────────────────────────
        case NodeType::Program: {
            currentOut = &text_main;

            rodata << ".section .rodata\n";
            rodata << "    fmt_int:   .asciz \"%ld\\n\"\n";
            rodata << "    fmt_str:   .asciz \"%s\\n\"\n";
            rodata << "    fmt_dec:   .asciz \"%f\\n\"\n";
            rodata << "    fmt_bool0: .asciz \"false\\n\"\n";
            rodata << "    fmt_bool1: .asciz \"true\\n\"\n";
            rodata << "    fmt_sprint:.asciz \"%ld\"\n";
            rodata << "    input_fmt: .asciz \"%255s\"\n";
            rodata << ".section .bss\n";
            rodata << "    input_buffer: .space 256\n";
            for (int i = 0; i < 8; i++)
                rodata << "    sprint_buf" << i << ": .space 64\n";
            rodata << ".section .rodata\n";

            if (usesEventListener(node)) {
                rodata << "    .equ KEY_1,0x31\n" << "    .equ KEY_2,0x32\n"
                       << "    .equ KEY_3,0x33\n" << "    .equ KEY_4,0x34\n"
                       << "    .equ KEY_5,0x35\n" << "    .equ KEY_6,0x36\n"
                       << "    .equ KEY_7,0x37\n" << "    .equ KEY_8,0x38\n"
                       << "    .equ KEY_9,0x39\n" << "    .equ KEY_0,0x30\n"
                       << "    .equ KEY_MINUS,0x2D\n" << "    .equ KEY_EQUAL,0x3D\n"
                       << "    .equ KEY_BACKSPACE,0x08\n" << "    .equ KEY_TAB,0x09\n"
                       << "    .equ KEY_ENTER,0x0D\n"     << "    .equ KEY_SPACE,0x20\n"
                       << "    .equ KEY_UP,0x26\n"   << "    .equ KEY_DOWN,0x28\n"
                       << "    .equ KEY_LEFT,0x25\n" << "    .equ KEY_RIGHT,0x27\n"
                       << "    .equ KEY_SHIFT,0x10\n" << "    .equ KEY_CTRL,0x11\n"
                       << "    .equ KEY_ALT,0x12\n"   << "    .equ KEY_CAPS,0x14\n"
                       << "    .equ KEY_A,0x61\n" << "    .equ KEY_B,0x62\n"
                       << "    .equ KEY_C,0x63\n" << "    .equ KEY_D,0x64\n"
                       << "    .equ KEY_E,0x65\n" << "    .equ KEY_F,0x66\n"
                       << "    .equ KEY_G,0x67\n" << "    .equ KEY_H,0x68\n"
                       << "    .equ KEY_I,0x69\n" << "    .equ KEY_J,0x6A\n"
                       << "    .equ KEY_K,0x6B\n" << "    .equ KEY_L,0x6C\n"
                       << "    .equ KEY_M,0x6D\n" << "    .equ KEY_N,0x6E\n"
                       << "    .equ KEY_O,0x6F\n" << "    .equ KEY_P,0x70\n"
                       << "    .equ KEY_Q,0x71\n" << "    .equ KEY_R,0x72\n"
                       << "    .equ KEY_S,0x73\n" << "    .equ KEY_T,0x74\n"
                       << "    .equ KEY_U,0x75\n" << "    .equ KEY_V,0x76\n"
                       << "    .equ KEY_W,0x77\n" << "    .equ KEY_X,0x78\n"
                       << "    .equ KEY_Y,0x79\n" << "    .equ KEY_Z,0x7A\n";
            }

            text_main << ".section .text\n";
            text_main << ".globl main\n";

            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children)
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    { mainloopBlock = c.get(); break; }

            collectingFunctions = true;
            std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef || n.type == NodeType::BundleDef) genNode(n);
                for (auto& c : n.children) collectFuncs(*c);
            };
            for (auto& c : node.children) collectFuncs(*c);
            collectingFunctions = false;

            emitRaw("main:");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            emit("sub $256, %rsp");
            clearLocalScope();

            if (mainloopBlock)
                for (auto& c : mainloopBlock->children) genNode(*c);

            emit("xor %rax, %rax");
            emitEpilogue();
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt:  break;
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;
        case NodeType::PassStmt:    emit("nop"); break;

        // ── DISPLAY ────────────────────────────────────────────────────────
        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                std::string lbl = allocateString(val.substr(1, val.size() - 2));
                emit("lea " + lbl + "(%rip), %rsi");
                emit("lea fmt_str(%rip), %rdi");
                emit("xor %eax, %eax");
                emit("call printf");
            } else {
                evalExprStr(val);
                emitDisplayRax(false);
            }
            break;
        }

        // ── FOREIGN ────────────────────────────────────────────────────────
        case NodeType::ForeignBlock: {
            std::istringstream ss(node.value);
            std::string ln;
            while (std::getline(ss, ln)) emit(ln);
            break;
        }

        // ── HALT / RAISE ───────────────────────────────────────────────────
        case NodeType::KillStmt:
            emit("xor %edi, %edi");
            emit("call exit");
            break;

        case NodeType::RaiseStmt:
            emit("mov $1, %edi");
            emit("call exit");
            break;

        // ── RETURN ─────────────────────────────────────────────────────────
        case NodeType::ReturnStmt:
            if (!node.children.empty())       evalExprNode(*node.children[0]);
            else if (!node.value.empty())      evalExprStr(node.value);
            else                               emit("xor %rax, %rax");
            emitEpilogue();
            break;

        // ── ASSIGN ─────────────────────────────────────────────────────────
        case NodeType::AssignStmt: {
            TypePtr varType = nullptr;

            // Structured expression as child
            if (!node.children.empty() && node.children[0]->type != NodeType::FunctionCall) {
                evalExprNode(*node.children[0]);
                varType = std::make_shared<Type>(TypeKind::Numeral);
                int off = allocateStackVariable(node.value, varType);
                emit("mov %rax, " + std::to_string(off) + "(%rbp)");
                break;
            }

            if (node.attrs.empty()) break;
            std::string val = node.attrs[0];

            if (val.substr(0, 8) == "__dict__") {
                emit("# dict not supported in ASM: " + node.value);
                break;
            }

            if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
                if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
                    auto& fc = *node.children[0];
                    static const char* argRegs[] = {"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
                    for (size_t i = 0; i < fc.attrs.size() && i < 6; i++) {
                        std::string arg = fc.attrs[i];
                        size_t eq = arg.find('=');
                        if (eq != std::string::npos) arg = arg.substr(eq + 1);
                        while (!arg.empty() && arg.back()  == ' ') arg.pop_back();
                        while (!arg.empty() && arg.front() == ' ') arg.erase(arg.begin());
                        evalExprStr(arg);
                        emit("mov %rax, " + std::string(argRegs[i]));
                    }
                    emit("call " + fc.value);
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                    int off = allocateStackVariable(node.value, varType);
                    emit("mov %rax, " + std::to_string(off) + "(%rbp)");
                }
                break;
            }

            // Simple function call: ident(args)
            size_t pp = val.find('(');
            if (pp != std::string::npos && val.find(')') != std::string::npos) {
                std::string before = val.substr(0, pp);
                bool valid = !before.empty() && before.find(' ') == std::string::npos &&
                             before.find_first_of("+-*/") == std::string::npos &&
                             std::isalpha((unsigned char)before[0]);
                if (valid) {
                    std::string arg = val.substr(pp + 1, val.find(')') - pp - 1);
                    evalExprStr(arg);
                    emit("mov %rax, %rdi");
                    emit("call " + before);
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                    int off = allocateStackVariable(node.value, varType);
                    emit("mov %rax, " + std::to_string(off) + "(%rbp)");
                    break;
                }
            }

            if (val.rfind("__list__", 0) == 0 || val.rfind("__tuple__", 0) == 0) {
                bool isTuple  = val.rfind("__tuple__", 0) == 0;
                std::string contents = isTuple ? val.substr(9) : val.substr(8);
                auto elems   = splitListElements(contents);
                int len      = (int)elems.size();
                emit("mov $" + std::to_string(8 * (1 + len)) + ", %edi");
                emit("call malloc");
                int off = allocateStackVariable(node.value,
                    std::make_shared<Type>(isTuple ? TypeKind::Tuple : TypeKind::List));
                varTypes[node.value] = std::make_shared<Type>(isTuple ? TypeKind::Tuple : TypeKind::List);
                emit("mov %rax, " + std::to_string(off) + "(%rbp)");
                emit("movq $" + std::to_string(len) + ", (%rax)");
                for (int i = 0; i < len; i++) {
                    std::string stripped = quoteIfString(elems[i]);
                    if (stripped != elems[i]) {
                        std::string lbl = allocateString(stripped);
                        emit("mov " + std::to_string(off) + "(%rbp), %rcx");
                        emit("lea " + lbl + "(%rip), %rdx");
                        emit("mov %rdx, " + std::to_string(8*(i+1)) + "(%rcx)");
                    } else {
                        evalExprStr(translateExpr(elems[i]));
                        emit("mov " + std::to_string(off) + "(%rbp), %rcx");
                        emit("mov %rax, " + std::to_string(8*(i+1)) + "(%rcx)");
                    }
                }
                break;
            }

            std::string tv = translateExpr(val);
            if (tv.size() >= 2 && tv.front() == '$' && tv.back() == '$') {
                std::string lbl = allocateString(tv.substr(1, tv.size() - 2));
                emit("lea " + lbl + "(%rip), %rax");
                varType = std::make_shared<Type>(TypeKind::String);
            } else {
                evalExprStr(tv);
                varType = std::make_shared<Type>(TypeKind::Numeral);
            }
            { int off = allocateStackVariable(node.value, varType);
              emit("mov %rax, " + std::to_string(off) + "(%rbp)"); }
            break;
        }

        case NodeType::PropAssign: break;

        // ── COMPOUND ASSIGNMENT ────────────────────────────────────────────
        case NodeType::PlusEqualStmt:     emitCompoundAssign(node, "add"); break;
        case NodeType::MinusEqualStmt:    emitCompoundAssign(node, "sub"); break;
        case NodeType::MultiplyEqualStmt: {
            if (node.attrs.empty()) break;
            auto it = varOffsets.find(node.value);
            if (it == varOffsets.end()) break;
            evalExprStr(node.attrs[0]);
            emit("imul %rax, " + std::to_string(it->second) + "(%rbp)");
            break;
        }
        case NodeType::DivideEqualStmt: {
            if (node.attrs.empty()) break;
            auto it = varOffsets.find(node.value);
            if (it == varOffsets.end()) break;
            evalExprStr(node.attrs[0]);             // divisor → %rax
            emit("mov %rax, %rcx");                 // save divisor
            emit("mov " + std::to_string(it->second) + "(%rbp), %rax"); // dividend
            emit("cqo");
            emit("idiv %rcx");
            emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
            break;
        }
        case NodeType::AtEqualStmt: {
            if (node.attrs.empty()) break;
            auto it = varOffsets.find(node.value);
            if (it == varOffsets.end()) break;
            evalExprStr(node.attrs[0]);
            emit("mov %rax, %rcx");
            emit("mov " + std::to_string(it->second) + "(%rbp), %rax");
            emit("imul %rcx");
            emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
            break;
        }
        case NodeType::XorEqualStmt: {
            // x |= y  → XOR assign
            if (!node.children.empty()) {
                auto it = varOffsets.find(node.value);
                if (it != varOffsets.end()) {
                    evalExprNode(*node.children[0]);
                    emit("xor %rax, " + std::to_string(it->second) + "(%rbp)");
                }
            } else if (!node.attrs.empty()) {
                auto it = varOffsets.find(node.value);
                if (it != varOffsets.end()) {
                    evalExprStr(node.attrs[0]);
                    emit("xor %rax, " + std::to_string(it->second) + "(%rbp)");
                }
            }
            break;
        }

        // ── METHOD CALL ────────────────────────────────────────────────────
        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;

            if (name == "Term.display") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    std::string lbl = allocateString(args.substr(1, args.size() - 2));
                    emit("lea " + lbl + "(%rip), %rsi");
                    emit("lea fmt_str(%rip), %rdi");
                    emit("xor %eax, %eax");
                    emit("call printf");
                } else {
                    size_t brOpen  = args.find('[');
                    size_t brClose = args.rfind(']');
                    if (brOpen != std::string::npos && brClose != std::string::npos && brClose > brOpen) {
                        std::string listName  = args.substr(0, brOpen);
                        std::string indexExpr = args.substr(brOpen + 1, brClose - brOpen - 1);
                        while (!listName.empty() && std::isspace(listName.back()))  listName.pop_back();
                        while (!listName.empty() && std::isspace(listName.front())) listName.erase(listName.begin());
                        auto it = varOffsets.find(listName);
                        if (it != varOffsets.end()) {
                            evalExprStr(indexExpr);
                            emit("mov %rax, %rbx");
                            emit("mov " + std::to_string(it->second) + "(%rbp), %rcx");
                            emit("lea 8(%rcx), %rdx");
                            emit("mov (%rdx,%rbx,8), %rsi");
                            emit("lea fmt_int(%rip), %rdi");
                            emit("xor %eax, %eax");
                            emit("call printf");
                        }
                    } else {
                        auto it = varOffsets.find(args);
                        if (it != varOffsets.end()) {
                            auto ti    = varTypes.find(args);
                            bool isStr = ti != varTypes.end() && ti->second && !ti->second->isNumeral();
                            emit("mov " + std::to_string(it->second) + "(%rbp), %rsi");
                            emit("lea " + std::string(isStr ? "fmt_str" : "fmt_int") + "(%rip), %rdi");
                            emit("xor %eax, %eax");
                            emit("call printf");
                        } else {
                            evalExprStr(args);
                            emitDisplayRax(false);
                        }
                    }
                }
            } else if (name == "Term.ask") {
                emit("lea input_fmt(%rip), %rdi");
                emit("lea input_buffer(%rip), %rsi");
                emit("xor %eax, %eax");
                emit("call scanf");
                emit("lea input_buffer(%rip), %rax");
                // store to last assigned variable if possible
            } else if (name == "Term.halt") {
                emit("xor %edi, %edi");
                emit("call exit");
            } else {
                // generic method call: treat as function call with args
                evalExprStr(args);
                emit("mov %rax, %rdi");
                emit("call " + name);
            }
            break;
        }

        case NodeType::MethodChain:
            for (auto& child : node.children) genNode(*child);
            break;

        // ── TYPE COERCION ──────────────────────────────────────────────────
        case NodeType::TypeCoerceStmt: {
            // dec/int/string/bool x [= expr]
            std::string varName = node.value;
            std::string kind    = node.attrs.empty() ? "INT" : node.attrs[0];
            auto it = varOffsets.find(varName);
            if (it == varOffsets.end()) break;

            // Load current value (or expression if child present)
            if (!node.children.empty()) evalExprNode(*node.children[0]);
            else                        emit("mov " + std::to_string(it->second) + "(%rbp), %rax");

            if (kind == "INT") {
                // already an integer in %rax — store back
                emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
            } else if (kind == "BOOL") {
                // normalize to 0 or 1
                emit("test %rax, %rax");
                emit("setne %al");
                emit("movzx %al, %rax");
                emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
            } else if (kind == "STRING") {
                // sprint integer to a buffer, store pointer
                int bufId = sprintfBufId++ % 8;
                emit("mov %rax, %rsi");
                emit("lea sprint_buf" + std::to_string(bufId) + "(%rip), %rdi");
                emit("lea fmt_sprint(%rip), %rdx");
                // sprintf(buf, "%ld", val) — arg order: rdi=buf, rsi=fmt, rdx=val
                emit("mov %rsi, %rdx");
                emit("mov %rdx, %rsi");  // swap: rsi=fmt_sprint, rdx=val... actually:
                // correct order: rdi=dest, rsi=format, rdx=value
                emit("lea sprint_buf" + std::to_string(bufId) + "(%rip), %rdi");
                emit("lea fmt_sprint(%rip), %rsi");
                emit("xor %eax, %eax");
                emit("call sprintf");
                emit("lea sprint_buf" + std::to_string(bufId) + "(%rip), %rax");
                emit("mov %rax, " + std::to_string(it->second) + "(%rbp)");
                varTypes[varName] = std::make_shared<Type>(TypeKind::String);
            } else if (kind == "DEC") {
                // integer to float via SSE2 (result in xmm0, stored as double)
                emit("cvtsi2sdq %rax, %xmm0");
                // store as 8-byte double on stack (reuse the same slot)
                emit("movsd %xmm0, " + std::to_string(it->second) + "(%rbp)");
            }
            break;
        }

        // ── DESTROY / FREE ─────────────────────────────────────────────────
        case NodeType::DestroyStmt: {
            auto it = varOffsets.find(node.value);
            if (it != varOffsets.end()) varOffsets.erase(it);
            break;
        }

        case NodeType::FreeDecl:
            // free x, y — mark as globally visible (no-op for stack-based ASM)
            for (const auto& v : node.attrs) globalVars.insert(v);
            break;

        // ── CONTROL FLOW ───────────────────────────────────────────────────
        case NodeType::BreakStmt:
        case NodeType::SkipStmt:
            if (!breakStack.empty()) emit("jmp " + breakStack.back());
            break;

        case NodeType::ContinueStmt:
            if (!continueStack.empty()) emit("jmp " + continueStack.back());
            break;

        // ── EVAL ───────────────────────────────────────────────────────────
        case NodeType::EvalExpr:
            evalExprStr(node.value);
            break;

        // ── FUNCTION DEF ───────────────────────────────────────────────────
        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            currentOut = &text_funcs;

            emitRaw(node.value + ":");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            emit("sub $256, %rsp");

            auto savedOffsets     = varOffsets;
            auto savedTypes       = varTypes;
            int  savedStack       = stackOffset;
            auto savedBreak       = breakStack;
            auto savedContinue    = continueStack;
            clearLocalScope();
            breakStack.clear();
            continueStack.clear();

            static const char* pRegs[] = {"%rdi","%rsi","%rdx","%rcx","%r8","%r9"};
            for (size_t i = 0; i < node.attrs.size() && i < 6; i++) {
                int offset = -8 - (int)(i * 8);
                emit("mov " + std::string(pRegs[i]) + ", " + std::to_string(offset) + "(%rbp)");
                varOffsets[node.attrs[i]] = offset;
                varTypes[node.attrs[i]]   = std::make_shared<Type>(TypeKind::Numeral);
            }
            stackOffset = -8 - (int)(node.attrs.size() * 8);

            if (!node.children.empty() && !node.children[0]->children.empty())
                for (auto& c : node.children[0]->children) genNode(*c);

            emit("xor %rax, %rax");
            emitEpilogue();

            varOffsets    = savedOffsets;
            varTypes      = savedTypes;
            stackOffset   = savedStack;
            breakStack    = savedBreak;
            continueStack = savedContinue;
            currentOut    = &text_main;
            break;
        }

        // ── BUNDLE (class-like) ────────────────────────────────────────────
        case NodeType::BundleDef: {
            if (!collectingFunctions) break;
            currentOut = &text_funcs;
            // Emit each method as bundlename_methodname
            std::string bundleName = node.value;
            for (auto& child : node.children) {
                if (child->type == NodeType::FuncDef) {
                    // temporarily rename to bundleName_funcName
                    std::string origName = child->value;
                    const_cast<ASTNode*>(child.get())->value = bundleName + "_" + origName;
                    genNode(*child);
                    const_cast<ASTNode*>(child.get())->value = origName;
                }
            }
            currentOut = &text_main;
            break;
        }

        // ── IF / ELSEIF / OTHER ────────────────────────────────────────────
        case NodeType::IfStmt: {
            std::string label_end = newLabel();
            std::string next      = newLabel();

            breakStack.push_back(label_end);

            if (node.value != "OTHER") {
                emitBranchIfFalse(node.value, next);
                if (!node.children.empty())
                    for (auto& c : node.children[0]->children) genNode(*c);
                emit("jmp " + label_end);
                emitRaw(next + ":");
            } else {
                if (!node.children.empty())
                    for (auto& c : node.children[0]->children) genNode(*c);
                emit("jmp " + label_end);
            }

            for (auto& child : node.children) {
                if (child->type == NodeType::ElseIfStmt) {
                    std::string nxt = newLabel();
                    emitBranchIfFalse(child->value, nxt);
                    if (!child->children.empty())
                        for (auto& c : child->children[0]->children) genNode(*c);
                    emit("jmp " + label_end);
                    emitRaw(nxt + ":");
                } else if (child->type == NodeType::IfStmt && child->value == "OTHER") {
                    if (!child->children.empty())
                        for (auto& c : child->children[0]->children) genNode(*c);
                    emit("jmp " + label_end);
                }
            }
            emitRaw(label_end + ":");
            breakStack.pop_back();
            break;
        }

        // ── WHILST LOOP ────────────────────────────────────────────────────
        case NodeType::WhilstLoop: {
            std::string lLoop = newLabel();
            std::string lEnd  = newLabel();
            breakStack.push_back(lEnd);
            continueStack.push_back(lLoop);

            emitRaw(lLoop + ":");
            emitBranchIfFalse(node.value, lEnd);
            if (!node.children.empty())
                for (auto& c : node.children[0]->children) genNode(*c);
            emit("jmp " + lLoop);
            emitRaw(lEnd + ":");

            breakStack.pop_back();
            continueStack.pop_back();
            break;
        }

        // ── FOR LOOP ───────────────────────────────────────────────────────
        case NodeType::ForLoop: {
            std::string loopVar        = node.value;
            std::string collectionName = node.attrs.empty() ? "" : node.attrs[0];
            int loopVarOff = allocateStackVariable(loopVar, std::make_shared<Type>(TypeKind::Numeral));

            std::string lLoop = newLabel();
            std::string lEnd  = newLabel();
            breakStack.push_back(lEnd);
            continueStack.push_back(lLoop);

            auto it = varOffsets.find(collectionName);
            if (it != varOffsets.end()) {
                int listOff    = it->second;
                int counterOff = stackOffset; stackOffset -= 8;
                emit("movq $0, " + std::to_string(counterOff) + "(%rbp)");

                emitRaw(lLoop + ":");
                emit("mov " + std::to_string(counterOff) + "(%rbp), %rax");
                emit("mov " + std::to_string(listOff)    + "(%rbp), %rcx");
                emit("mov (%rcx), %rdx");
                emit("cmp %rdx, %rax");
                emit("jge " + lEnd);
                emit("lea 8(%rcx), %rdx");
                emit("mov (%rdx,%rax,8), %rdi");
                emit("mov %rdi, " + std::to_string(loopVarOff) + "(%rbp)");

                if (!node.children.empty())
                    for (auto& c : node.children[0]->children) genNode(*c);

                emit("incq " + std::to_string(counterOff) + "(%rbp)");
                emit("jmp " + lLoop);
            } else {
                emit("# unknown collection: " + collectionName);
            }
            emitRaw(lEnd + ":");
            breakStack.pop_back();
            continueStack.pop_back();
            break;
        }

        // ── TRY / CATCH / AFTER ────────────────────────────────────────────
        case NodeType::TryCatchStmt: {
            // Minimal: run try body; if it exits abnormally, catch body runs.
            // Full signal-based exception handling is platform-specific.
            // For now emit try body; catch is a labelled fallthrough.
            std::string lCatch = newLabel();
            std::string lAfter = newLabel();

            // Try body (first child)
            if (node.children.size() > 0)
                for (auto& c : node.children[0]->children) genNode(*c);
            emit("jmp " + lAfter);

            // Catch body (second child, if present)
            emitRaw(lCatch + ":");
            if (node.children.size() > 1)
                for (auto& c : node.children[1]->children) genNode(*c);

            // After/finally body (third child, if present)
            emitRaw(lAfter + ":");
            if (node.children.size() > 2)
                for (auto& c : node.children[2]->children) genNode(*c);
            break;
        }

        // ── RANGE / SEQUENCE ───────────────────────────────────────────────
        case NodeType::RangeExpr: {
            evalExprStr(node.value);
            emit("mov %rax, %rbx");
            emit("lea 1(%rax), %rdi");
            emit("imul $8, %rdi");
            emit("add $8, %rdi");
            emit("call malloc");
            emit("mov %rbx, (%rax)");

            std::string lLoop = newLabel(), lEnd = newLabel();
            emit("xor %rcx, %rcx");
            emitRaw(lLoop + ":");
            emit("cmp %rbx, %rcx");
            emit("jg " + lEnd);
            emit("mov %rcx, 8(%rax,%rcx,8)");
            emit("inc %rcx");
            emit("jmp " + lLoop);
            emitRaw(lEnd + ":");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);

            evalExprStr(x); emit("mov %rax, %r8");
            evalExprStr(y); emit("mov %rax, %r9");

            emit("mov %r9, %rax");
            emit("sub %r8, %rax");
            emit("inc %rax");
            emit("mov %rax, %r10");
            emit("imul $8, %rax");
            emit("add $8, %rax");
            emit("mov %rax, %rdi");
            emit("call malloc");
            emit("mov %r10, (%rax)");

            std::string lLoop = newLabel(), lEnd = newLabel();
            emit("xor %rcx, %rcx");
            emitRaw(lLoop + ":");
            emit("cmp %r10, %rcx");
            emit("jge " + lEnd);
            emit("mov %r8, %rdx");
            emit("add %rcx, %rdx");
            emit("mov %rdx, 8(%rax,%rcx,8)");
            emit("inc %rcx");
            emit("jmp " + lLoop);
            emitRaw(lEnd + ":");
            break;
        }

        // ── INDEX EXPR ─────────────────────────────────────────────────────
        case NodeType::IndexExpr: {
            std::string listName = node.value;
            std::string indexStr = node.attrs.empty() ? "0" : node.attrs[0];
            bool isAssign        = node.attrs.size() > 1;
            auto it = varOffsets.find(listName);
            if (it != varOffsets.end()) {
                evalExprStr(indexStr);
                emit("mov %rax, %rbx");
                emit("mov " + std::to_string(it->second) + "(%rbp), %rcx");
                if (isAssign) {
                    std::string tv = translateExpr(node.attrs[1]);
                    if (tv.size() >= 2 && tv.front() == '$' && tv.back() == '$')
                        emit("lea " + allocateString(tv.substr(1, tv.size()-2)) + "(%rip), %rax");
                    else
                        evalExprStr(tv);
                    emit("lea 8(%rcx), %rdx");
                    emit("mov %rax, (%rdx,%rbx,8)");
                } else {
                    emit("lea 8(%rcx), %rdx");
                    emit("mov (%rdx,%rbx,8), %rax");
                }
            } else { emit("xor %rax, %rax"); }
            break;
        }

        // ── STRUCTURED EXPR NODES ──────────────────────────────────────────
        case NodeType::BinaryExpr:
            if (node.children.size() >= 2) evalExprNode(node);
            else evalExprStr(node.value);
            break;

        case NodeType::UnaryExpr:
        case NodeType::CallExpr:
        case NodeType::LiteralExpr:
            evalExprNode(node);
            break;

        // ── TAG BLOCKS ─────────────────────────────────────────────────────
        case NodeType::TagBlock:   genTagBlock(node); break;
        case NodeType::Block:      for (auto& c : node.children) genNode(*c); break;

        case NodeType::CustomTagDef:
            emitRaw("spawn_" + node.value + ":");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            if (!node.children.empty())
                for (auto& c : node.children[0]->children) genNode(*c);
            emit("pop %rbp");
            emit("ret");
            break;

        case NodeType::SpawnStmt: {
            std::string lower = node.value;
            for (auto& c : lower) c = std::tolower(c);
            emit("call spawn_" + lower);
            break;
        }

        case NodeType::EventListener:
        case NodeType::KeyBinding:
        case NodeType::InputStmt:
        case NodeType::WhenBlock:
            for (auto& c : node.children) genNode(*c);
            break;

        default: break;
        }
    }

    void genTagBlock(const ASTNode& node) {
        std::string name = node.value;
        if (Tags::isMainLoop(name)) {
            std::string lLoop = newLabel(), lEnd = newLabel();
            breakStack.push_back(lEnd);
            continueStack.push_back(lLoop);
            emitRaw(lLoop + ":");
            for (auto& c : node.children) genNode(*c);
            emit("jmp " + lLoop);
            emitRaw(lEnd + ":");
            breakStack.pop_back();
            continueStack.pop_back();
        } else if (Tags::isGUIBox(name)) {
            // skip
        } else if (Tags::isLogicScope(name)) {
            for (auto& c : node.children) genNode(*c);
        } else if (Tags::isStartHere(name)) {
            std::string lLoop = newLabel();
            emitRaw(lLoop + ":");
            for (auto& c : node.children) genNode(*c);
            emit("jmp " + lLoop);
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
        if (!text_funcs.str().empty()) {
            output << ".section .text\n";
            output << text_funcs.str();
        }
        output << text_main.str();
        if (!stringTable.empty()) {
            output << ".section .rodata\n";
            for (auto& [content, id] : stringTable)
                output << ".str_" << id << ": .asciz \"" << content << "\"\n";
            output << ".section .text\n";
        }
        output << ".section .note.GNU-stack,\"\",@progbits\n";
        return output.str();
    }
};

std::string generateAsm(const ASTNode& ast) {
    AsmCodeGen gen;
    return gen.generate(ast);
}
