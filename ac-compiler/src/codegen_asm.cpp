#include "../include/ast.hpp"
#include "../include/type.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <map>
#include <functional>

class AsmCodeGen {
    std::ostringstream out;
    std::ostringstream functions;
    int labelCounter = 0;
    int stackOffset = 0;
    bool collectingFunctions = false;
    std::map<std::string, int> varOffsets;  // variable name -> stack offset
    std::map<std::string, TypePtr> varTypes;  // variable name -> type
    int currentOffset = 0;

    std::string newLabel() {
        return ".L" + std::to_string(labelCounter++);
    }

    void emit(const std::string& line) {
        out << "    " << line << "\n";
    }
    void emitRaw(const std::string& line) { out << line << "\n"; }
    
    void emitFunc(const std::string& line) {
        functions << "    " << line << "\n";
    }
    void emitFuncRaw(const std::string& line) { functions << line << "\n"; }

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
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;) {
            r.replace(p, 2, "!="); p += 2;
        }
        return r;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        return r;
    }

    void genBlock(const ASTNode& node) {
        for (auto& c : node.children) genNode(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            emitRaw(".section .rodata");
            emitRaw("    fmt_int: .asciz \"%d\\n\"");
            emitRaw("    fmt_str: .asciz \"%s\\n\"");
            emitRaw("");
            emitRaw(".section .text");
            emitRaw(".globl main");
            
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
            
            // Emit collected functions
            emitRaw(functions.str());
            
            // Main function
            emitRaw("main:");
            emit("push %rbp");
            emit("mov %rsp, %rbp");
            emit("sub $256, %rsp");  // Allocate stack space
            currentOffset = 0;
            varOffsets.clear();
            varTypes.clear();
            
            // Second pass: emit main body
            for (auto& c : node.children) genNode(*c);
            
            emit("xor %eax, %eax");
            emit("add $256, %rsp");
            emit("pop %rbp");
            emit("ret");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                // String literal
                std::string str = val.substr(1, val.size() - 2);
                std::string strLabel = ".str_" + std::to_string(labelCounter);
                emitRaw("    lea " + strLabel + "(%rip), %rdi");
                emitRaw("    xor %eax, %eax");
                emitRaw("    call printf");
                // Add string to data section
                out << ".section .rodata\n";
                out << strLabel << ": .asciz \"" << str << "\"\n";
                out << ".section .text\n";
                labelCounter++;
            } else {
                // Assume numeric or variable
                emitRaw("    lea fmt_int(%rip), %rdi");
                emitRaw("    mov $" + val + ", %esi");
                emitRaw("    xor %eax, %eax");
                emitRaw("    call printf");
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
            if (!val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1))) {
                emit("mov $" + val + ", %eax");
            } else if (!val.empty()) {
                auto it = varOffsets.find(val);
                if (it != varOffsets.end()) {
                    emit("mov " + std::to_string(it->second) + "(%rbp), %eax");
                }
            }
            break;
        }

        case NodeType::AssignStmt: {
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                TypePtr varType = nullptr;
                
                bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                bool isFuncCall = val.find('(') != std::string::npos && val.find(')') != std::string::npos;
                
                if (isNum) {
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                    emit("mov $" + val + ", %eax");
                } else if (isFuncCall) {
                    // Handle function call: extract function name and argument
                    varType = std::make_shared<Type>(TypeKind::Numeral);
                    size_t parenPos = val.find('(');
                    std::string funcName = val.substr(0, parenPos);
                    std::string arg = val.substr(parenPos + 1, val.find(')') - parenPos - 1);
                    
                    // Move argument to %edi (first parameter register)
                    if (std::isdigit(arg[0]) || (arg[0] == '-' && arg.size() > 1)) {
                        emit("mov $" + arg + ", %edi");
                    } else {
                        // It's a variable
                        auto it = varOffsets.find(arg);
                        if (it != varOffsets.end()) {
                            emit("mov " + std::to_string(it->second) + "(%rbp), %edi");
                        }
                    }
                    emit("call " + funcName);
                    emit("# result in %eax");
                } else {
                    varType = std::make_shared<Type>(TypeKind::String);
                    emit("# assign string: " + val);
                }
                
                // Allocate stack space for variable
                currentOffset -= 8;
                varOffsets[node.value] = currentOffset;
                varTypes[node.value] = varType;
                emit("mov %eax, " + std::to_string(currentOffset) + "(%rbp)  # " + node.value);
            }
            break;
        }

        case NodeType::PropAssign:
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;
            if (name == "Term.display") {
                // Check if args is a string literal
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    // String literal
                    std::string str = args.substr(1, args.size() - 2);
                    std::string strLabel = ".str_" + std::to_string(labelCounter);
                    emitRaw("    lea " + strLabel + "(%rip), %rdi");
                    emitRaw("    xor %eax, %eax");
                    emitRaw("    call printf");
                    // Add string to data section
                    out << ".section .rodata\n";
                    out << strLabel << ": .asciz \"" << str << "\"\n";
                    out << ".section .text\n";
                    labelCounter++;
                } else {
                    // Check if it's a known variable
                    auto it = varOffsets.find(args);
                    if (it != varOffsets.end()) {
                        auto typeIt = varTypes.find(args);
                        if (typeIt != varTypes.end() && typeIt->second->isNumeral()) {
                            emitRaw("    lea fmt_int(%rip), %rdi");
                            emit("mov " + std::to_string(it->second) + "(%rbp), %esi");
                        } else {
                            emitRaw("    lea fmt_str(%rip), %rdi");
                            emit("mov " + std::to_string(it->second) + "(%rbp), %rsi");
                        }
                    } else {
                        // Assume numeric literal
                        emitRaw("    lea fmt_int(%rip), %rdi");
                        emit("mov $" + args + ", %esi");
                    }
                    emitRaw("    xor %eax, %eax");
                    emitRaw("    call printf");
                }
            }
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            std::string arg = node.attrs.empty() ? "" : node.attrs[0];
            emitFuncRaw(node.value + ":");
            emitFunc("push %rbp");
            emitFunc("mov %rsp, %rbp");
            emitFunc("sub $256, %rsp");
            
            // Save current state
            auto savedOffsets = varOffsets;
            auto savedTypes = varTypes;
            int savedOffset = currentOffset;
            
            varOffsets.clear();
            varTypes.clear();
            currentOffset = 0;
            
            // Parameter at 16(%rbp)
            varOffsets[arg] = 16;
            varTypes[arg] = std::make_shared<Type>(TypeKind::Numeral);
            
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) {
                    // Emit function body
                    std::ostringstream tempOut = std::move(out);
                    out.str("");
                    out.clear();
                    genNode(*c);
                    functions << out.str();
                    out = std::move(tempOut);
                }
            }
            
            emitFunc("pop %rbp");
            emitFunc("ret");
            
            // Restore state
            varOffsets = savedOffsets;
            varTypes = savedTypes;
            currentOffset = savedOffset;
            break;
        }

        case NodeType::IfStmt: {
            std::string label_else = newLabel();
            std::string cond = node.value;
            emit("# if " + cond);
            emit("cmp $0, %eax");
            emit("je " + label_else);
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emitRaw(label_else + ":");
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string label_else = newLabel();
            emit("cmp $0, %eax");
            emit("je " + label_else);
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emitRaw(label_else + ":");
            break;
        }

        case NodeType::ForLoop: {
            std::string label_loop = newLabel();
            std::string label_end = newLabel();
            emit("# for loop");
            emitRaw(label_loop + ":");
            emit("cmp $10, %eax");
            emit("jge " + label_end);
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emit("inc %eax");
            emit("jmp " + label_loop);
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
            emit("# list literal");
            break;

        case NodeType::TupleLiteral:
            emit("# tuple literal");
            break;

        case NodeType::BinaryExpr:
            emit("# binary expr: " + node.value);
            break;

        case NodeType::TagBlock:
            genTagBlock(node);
            break;

        case NodeType::Block:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::CustomTagDef:
            emitFuncRaw("spawn_" + node.value + ":");
            emitFunc("push %rbp");
            emitFunc("mov %rsp, %rbp");
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            emitFunc("pop %rbp");
            emitFunc("ret");
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
        if (name == "mainloop") {
            std::string label_loop = newLabel();
            std::string label_end = newLabel();
            emitRaw(label_loop + ":");
            for (auto& c : node.children) genNode(*c);
            emit("jmp " + label_loop);
            emitRaw(label_end + ":");
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else if (name == "LOGIC") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "Local") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "StartHere") {
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
        return out.str();
    }
};

std::string generateAsm(const ASTNode& ast) {
    AsmCodeGen gen;
    return gen.generate(ast);
}
