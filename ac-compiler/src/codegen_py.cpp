#include "../include/ast.hpp"
#include "../include/type.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <functional>
#include <map>

class PythonCodeGen {
    std::ostringstream out;
    int indentLevel = 0;
    std::map<std::string, TypePtr> varTypes;

    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    void emitRaw(const std::string& line) { out << line << "\n"; }

    // $...$ marker -> "..."
    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return "\"" + v.substr(1, v.size() - 2) + "\"";
        return v;
    }

    // Unwrap any $...$ inside an expression to "..."
    std::string unwrapDollars(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    result += "\"" + s.substr(i + 1, end - i - 1) + "\"";
                    i = end;
                    continue;
                }
            }
            result += s[i];
        }
        return result;
    }

    // Translate AC condition to Python
    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        // is -> == (AC equality keyword; Python's `is` is identity, so we use ==)
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.substr(0, 3) == "is ") r.replace(0, 3, "");
        // not of Obj type
        for (size_t p = 0; (p = r.find("not of Obj type", p)) != std::string::npos;)
            r.replace(p, 15, "not isinstance(arg, object)"), p += 26;
        return r;
    }

    // Translate fn expr*(expr)fn -> expr * (expr)
    // fn...fn is just a multiply wrapper, strip it and pass expression through
    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;) {
            r.replace(p, 2, "!="); p += 2;
        }
        // 1-based index: list[n] -> list[n-1]
        for (size_t p = 0; (p = r.find('[', p)) != std::string::npos;) {
            size_t end = r.find(']', p);
            if (end != std::string::npos) {
                std::string idx = r.substr(p + 1, end - p - 1);
                bool isNum = !idx.empty();
                for (char c : idx) if (!std::isdigit(c)) { isNum = false; break; }
                if (isNum) {
                    int n = std::stoi(idx);
                    r.replace(p, end - p + 1, "[" + std::to_string(n - 1) + "]");
                }
            }
            p++;
        }
        return r;
    }

    // Parse "$item1, item2$" or "item1, item2" into Python list/tuple items
    std::string parseCollectionItems(const std::string& raw) {
        std::string s = raw;
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
            s = s.substr(1, s.size() - 2);
        std::string result, item;
        for (size_t i = 0; i <= s.size(); i++) {
            if (i == s.size() || s[i] == ',') {
                size_t a = item.find_first_not_of(' ');
                size_t b = item.find_last_not_of(' ');
                if (a != std::string::npos) {
                    std::string t = item.substr(a, b - a + 1);
                    if (!result.empty()) result += ", ";
                    // only keep as bare identifier if it's a number
                    bool isNum = !t.empty() && (std::isdigit(t[0]) || t[0] == '-');
                    result += isNum ? t : "\"" + t + "\"";
                }
                item.clear();
            } else item += s[i];
        }
        return result;
    }

    bool isPythonBuiltin(const std::string& name) {
        static const std::vector<std::string> b = {
            "print","input","len","range","int","str","float","list","dict",
            "set","type","isinstance","open","exit","abs","max","min","sum"
        };
        for (auto& x : b) if (x == name) return true;
        return false;
    }

    void genBlock(const ASTNode& node) {
        if (node.children.empty()) { emit("pass"); return; }
        bool hasContent = false;
        for (auto& c : node.children) {
            if (c->type != NodeType::Block || !c->children.empty()) {
                hasContent = true; break;
            }
        }
        if (!hasContent) { emit("pass"); return; }
        for (auto& c : node.children) genNode(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            emitRaw("# Generated by AC Compiler (AC->PY)");
            emitRaw("import sys");
            emitRaw("");
            // Split pre-mainloop declarations from mainloop body
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty())
                    emitRaw(c->value + " = " + quoteIfString(c->attrs[0]));
            }
            emitRaw("");
            // Collect functions
            if (mainloopBlock) {
                std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                    if (n.type == NodeType::FuncDef) genNode(n);
                    for (auto& c : n.children) collectFuncs(*c);
                };
                collectFuncs(*mainloopBlock);
            }
            emitRaw("def main():");
            indentLevel++;
            if (mainloopBlock) {
                emit("while True:");
                indentLevel++;
                for (auto& c : mainloopBlock->children) genNode(*c);
                indentLevel--;
            }
            indentLevel--;
            emitRaw("");
            emitRaw("if __name__ == \"__main__\":");
            emitRaw("    main()");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt:  break;
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            // display $string$ -> screen output, for PY falls back to print
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$')
                val = "\"" + val.substr(1, val.size() - 2) + "\"";
            emit("print(" + val + ")  # display (screen output)");
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
            emit("sys.exit(0)");
            break;

        case NodeType::RaiseStmt:
            emit("raise Exception(\"Preposterous: " + node.value + "\")");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value));
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                if (val.substr(0, 8) == "__list__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeList());
                    emit(node.value + " = [" + parseCollectionItems(val.substr(8)) + "]");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeTuple());
                    emit(node.value + " = (" + parseCollectionItems(val.substr(9)) + ")  # tuple: immutable");
                } else if (val.substr(0, 6) == "__fn__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                    emit(node.value + " = " + translateExpr(val.substr(6)));
                } else {
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    if (isNum) {
                        auto t = Type::inferNumeral(val);
                        varTypes[node.value] = std::make_shared<Type>(t);
                        // Python is dynamically typed — just emit the value, add type comment
                        emit(node.value + " = " + val + "  # " + t.toString());
                    } else {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                        emit(node.value + " = " + quoteIfString(val));
                    }
                }
            }
            break;

        case NodeType::PlusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " += " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::MinusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " -= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::MultiplyEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::DivideEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " /= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::AtEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::PropAssign:
            if (!node.attrs.empty())
                emit(node.value + " = " + quoteIfString(node.attrs[0]));
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            if (args.size() >= 2 && args.front() == '$' && args.back() == '$')
                args = "\"" + args.substr(1, args.size() - 2) + "\"";
            else
                args = translateExpr(args);
            std::string name = node.value;
            if (name == "Term.display") name = "print";
            emit(name + "(" + args + ")");
            break;
        }

        case NodeType::FuncDef: {
            std::string arg = node.attrs.empty() ? "" : node.attrs[0];
            emit("def " + node.value + "(" + arg + "):");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::IfStmt: {
            if (node.value == "OTHER") {
                emit("else:");
            } else {
                std::string cond = translateCondition(node.value);
                emit("if " + cond + ":");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = translateCondition(node.value);
            emit("elif " + cond + ":");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "[]" : translateExpr(node.attrs[0]);
            emit("for " + node.value + " in " + lst + ":");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond = translateCondition(node.value);
            emit("while " + cond + ":");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::ListLiteral:
            emit("[" + parseCollectionItems(node.value) + "]");
            break;

        case NodeType::TupleLiteral:
            emit("(" + parseCollectionItems(node.value) + ")");
            break;

        case NodeType::BinaryExpr:
            emit(translateExpr(node.value));
            break;

        case NodeType::TagBlock:
            genTagBlock(node);
            break;

        case NodeType::Block:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::CustomTagDef:
            emit("def spawn_" + node.value + "():");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;

        case NodeType::SpawnStmt: {
            std::string name = node.value;
            if (isPythonBuiltin(name)) {
                std::string args;
                for (size_t i = 0; i < node.attrs.size(); i++) {
                    if (i > 0) args += ", ";
                    args += quoteIfString(node.attrs[i]);
                }
                emit(name + "(" + args + ")");
            } else {
                std::string lower = name;
                for (auto& c : lower) c = std::tolower(c);
                emit("spawn_" + lower + "()");
            }
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
            emit("def main():");
            indentLevel++;

            bool hasStartHere = false;
            for (auto& c : node.children)
                if (c->type == NodeType::TagBlock && c->value == "StartHere")
                    { hasStartHere = true; break; }

            if (hasStartHere) {
                // emit setup code before StartHere once
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") break;
                    genNode(*c);
                }
                // StartHere body loops forever
                emit("while True:");
                indentLevel++;
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") {
                        for (auto& inner : c->children) genNode(*inner);
                        break;
                    }
                }
                indentLevel--;
            } else {
                // no StartHere — whole mainloop body loops forever
                emit("while True:");
                indentLevel++;
                for (auto& c : node.children) genNode(*c);
                indentLevel--;
            }

            indentLevel--;
            emitRaw("");
            emit("if __name__ == \"__main__\":");
            indentLevel++;
            emit("main()");
            indentLevel--;
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else if (name == "LOGIC") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "Local") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "StartHere") {
            emit("while True:");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
        } else {
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("spawn_" + lower + "()");
            for (auto& c : node.children) genNode(*c);
        }
    }

public:
    std::string generate(const ASTNode& node) {
        genNode(node);
        return out.str();
    }
};

std::string generatePython(const ASTNode& ast) {
    PythonCodeGen gen;
    return gen.generate(ast);
}
