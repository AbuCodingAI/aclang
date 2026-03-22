#include "../include/ast.hpp"
#include "../include/type.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <map>
#include <functional>

class VCodeGen {
    std::ostringstream out;
    std::ostringstream functions;
    std::ostringstream* currentOut;
    int indentLevel = 0;
    bool collectingFunctions = false;
    std::map<std::string, TypePtr> varTypes;

    void emit(const std::string& line) {
        *currentOut << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    void emitRaw(const std::string& line) { 
        *currentOut << line << "\n"; 
    }

    std::string unwrapDollars(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '$') {
                size_t end = s.find('$', i + 1);
                if (end != std::string::npos) {
                    result += "'" + s.substr(i + 1, end - i - 1) + "'";
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
            return "'" + v.substr(1, v.size() - 2) + "'";
        return v;
    }

    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);

        // Normalize special AC condition patterns:
        // <obj>.Hitbox.Coords Overlap <other> -> obj.overlaps(other)
        size_t overlapPos;
        if ((overlapPos = r.find("Hitbox.Coords Overlap")) != std::string::npos) {
            std::string left = r.substr(0, overlapPos);
            if (!left.empty() && left.back() == ' ') left.pop_back();
            std::string right = r.substr(overlapPos + strlen("Hitbox.Coords Overlap"));
            while (!right.empty() && right.front() == ' ') right.erase(0, 1);
            r = left + ".overlaps(" + right + ")";
        }

        // CircleFall check pattern -> convert to function call
        if (r.find("CircleFell") != std::string::npos) {
            size_t atPos = r.find("CircleFell");
            std::string prefix = r.substr(0, atPos);
            if (prefix.find("Cactus") != std::string::npos) {
                r = "cactus_physics.circle_fell()";
            }
        }

        // hitbox overlap in AC pseudo-language
        if (r.find("hitbox overlap") != std::string::npos) {
            r = "true";
        }

        // not found fallback
        if (r.find("not found") != std::string::npos) {
            if (r.find("AC.Search") != std::string::npos) {
                r = r.substr(0, r.find(" not found"));
            } else {
                r = "true";
            }
        }

        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;

        // is True/False / is -> ==
        for (size_t p = 0; (p = r.find(" is True", p)) != std::string::npos;) {
            r.replace(p, 8, " == true"); p += 8;
        }
        for (size_t p = 0; (p = r.find(" is False", p)) != std::string::npos;) {
            r.replace(p, 9, " == false"); p += 9;
        }
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;

        if (r.rfind("is ", 0) == 0) r = r.substr(3);

        return r;
    }

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
                    bool isNum = !t.empty() && (std::isdigit(t[0]) || t[0] == '-');
                    result += isNum ? t : "\"" + t + "\"";
                }
                item.clear();
            } else item += s[i];
        }
        return result;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        // is -> == (equality keyword)
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.substr(0, 3) == "is ") r.replace(0, 3, "");
        // Remove 'fn' keywords
        for (size_t p = 0; (p = r.find("fn", p)) != std::string::npos;) {
            if (p + 2 < r.size() && r[p + 2] == ' ') {
                r.erase(p, 3);
            } else if (p + 2 < r.size()) {
                r.erase(p, 2);
            } else {
                r.erase(p, 2);
            }
        }
        return r;
    }

    void genBlock(const ASTNode& node) {
        for (auto& c : node.children) genNode(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            currentOut = &out;
            emitRaw("// Generated by AC Compiler (AC->V)");
            emitRaw("import os");
            emitRaw("");
            
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
            
            // Emit functions
            emitRaw(functions.str());
            
            emitRaw("fn main() {");
            indentLevel++;
            
            // Second pass: emit main body
            for (auto& c : node.children) genNode(*c);
            
            indentLevel--;
            emit("}");
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
                val = "'" + val.substr(1, val.size() - 2) + "'";
            }
            emit("println(" + val + ")");
            break;
        }

        case NodeType::ForeignBlock: {
            emit("// Preposterous: Foreign code is too Foreign (if this breaks, that's on you)");
            std::istringstream ss(node.value);
            std::string ln;
            while (std::getline(ss, ln)) emit(ln);
            break;
        }

        case NodeType::KillStmt:
            emit("exit(0)");
            break;

        case NodeType::RaiseStmt:
            emit("eprintln('Preposterous: " + node.value + "')");
            emit("exit(1)");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value));
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                bool isFuncCall = val.find('(') != std::string::npos;
                if (isNum || isFuncCall) {
                    auto t = Type::inferNumeral(val);
                    varTypes[node.value] = std::make_shared<Type>(t);
                    // V type names: int, f64
                    std::string vtype = t.isDec() ? "f64" : "int";
                    emit("mut " + node.value + " := " + vtype + "(" + val + ")");
                } else {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                    emit("mut " + node.value + " := " + unwrapDollars(val));
                }
            }
            break;

        case NodeType::PropAssign:
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;
            if (name == "Term.display") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "'" + args.substr(1, args.size() - 2) + "'";
                }
                emit("println(" + args + ")");
            }
            break;
        }

        case NodeType::RangeExpr: {
            std::string n = node.value;
            while (!n.empty() && n.back() == ' ') n.pop_back();
            emit("(0..(" + n + ")+1).array()");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("((" + x + ")..(" + y + ")+1).array()");
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            std::string arg = node.attrs.empty() ? "" : node.attrs[0];
            
            currentOut = &functions;
            emitRaw("fn " + node.value + "(" + arg + " int) int {");
            indentLevel++;
            
            if (!node.children.empty()) {
                for (auto& c : node.children[0]->children) genNode(*c);
            }
            
            emit("return 0");
            indentLevel--;
            emit("}");
            
            currentOut = &out;
            break;
        }

        case NodeType::IfStmt: {
            std::string cond = node.value;
            if (cond == "OTHER") {
                std::string cur = out.str();
                size_t end = cur.rfind("}\n");
                if (end != std::string::npos) {
                    size_t start = cur.rfind('\n', end - 1);
                    start = (start == std::string::npos) ? 0 : start + 1;
                    out.str(cur.substr(0, start));
                    out.seekp(0, std::ios::end);
                }
                emit("} else {");
            } else {
                emit("if " + translateExpr(cond) + " {");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = node.value;
            std::string cur = out.str();
            size_t end = cur.rfind("}\n");
            if (end != std::string::npos) {
                size_t start = cur.rfind('\n', end - 1);
                start = (start == std::string::npos) ? 0 : start + 1;
                out.str(cur.substr(0, start));
                out.seekp(0, std::ios::end);
            }
            emit("} else if " + translateExpr(cond) + " {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "arr" : translateExpr(node.attrs[0]);
            emit("for " + node.value + " in " + lst + " {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond = translateExpr(node.value);
            emit("for " + cond + " {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ListLiteral:
            emit("// list literal");
            break;

        case NodeType::TupleLiteral:
            emit("// tuple literal");
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
            emit("fn spawn_" + node.value + "() {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;

        case NodeType::SpawnStmt: {
            std::string name = node.value;
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("spawn_" + lower + "()");
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
            emit("for {");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            emit("}");
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else if (name == "LOGIC") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "Local") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "StartHere") {
            emit("for {");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            emit("}");
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

std::string generateV(const ASTNode& ast) {
    VCodeGen gen;
    return gen.generate(ast);
}
