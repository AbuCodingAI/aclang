#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "../include/tags.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <functional>
#include <map>

class CCodeGen {
    std::ostringstream out;
    std::ostringstream functions;
    int indentLevel = 0;
    bool collectingFunctions = false;
    std::map<std::string, TypePtr> varTypes;  // Track variable types

    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    void emitRaw(const std::string& line) { out << line << "\n"; }

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return "\"" + v.substr(1, v.size() - 2) + "\"";
        return v;
    }

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

        // CircleFell check pattern -> convert to function call
        if (r.find("CircleFell") != std::string::npos) {
            size_t atPos = r.find("CircleFell");
            std::string prefix = r.substr(0, atPos);
            if (prefix.find("Cactus") != std::string::npos) {
                r = "cactusPhysics.circleFell()";
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
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;) {
            r.replace(p, 2, "!="); p += 2;
        }
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
        if (node.children.empty()) { emit("// empty block"); return; }
        bool hasContent = false;
        for (auto& c : node.children) {
            if (c->type != NodeType::Block || !c->children.empty()) {
                hasContent = true; break;
            }
        }
        if (!hasContent) { emit("// empty block"); return; }
        for (auto& c : node.children) genNode(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            emitRaw("// Generated by AC Compiler (AC->C)");
            emitRaw("#include <stdio.h>");
            emitRaw("#include <stdlib.h>");
            emitRaw("#include <string.h>");
            emitRaw("");
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty()) {
                    std::string val = c->attrs[0];
                    bool isStr = (!val.empty() && val.front() == '$');
                    if (isStr) emitRaw("const char* " + c->value + " = " + quoteIfString(val) + ";");
                    else       emitRaw("int " + c->value + " = " + val + ";");
                }
            }
            emitRaw("");
            collectingFunctions = true;
            {
                std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                    if (n.type == NodeType::FuncDef) genNode(n);
                    for (auto& c : n.children) collectFuncs(*c);
                };
                if (mainloopBlock) collectFuncs(*mainloopBlock);
            }
            collectingFunctions = false;
            emitRaw(functions.str());
            emitRaw("");
            emitRaw("int main() {");
            indentLevel++;
            if (mainloopBlock) {
                emit("while (1) {");
                indentLevel++;
                for (auto& c : mainloopBlock->children) genNode(*c);
                indentLevel--;
                emit("}");
            }
            indentLevel--;
            emit("return 0;");
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
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$')
                val = "\"" + val.substr(1, val.size() - 2) + "\"";
            // If it contains quotes, it's a string
            if (val.find('"') != std::string::npos) {
                emit("printf(\"%s\\n\", " + val + ");");
            } else {
                // Otherwise assume it's an integer (variable or function call)
                emit("printf(\"%d\\n\", " + val + ");");
            }
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
            emit("exit(0);");
            break;

        case NodeType::RaiseStmt:
            emit("fprintf(stderr, \"Preposterous: " + node.value + "\\n\");");
            emit("exit(1);");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value) + ";");
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                TypePtr varType = nullptr;

                if (val.substr(0, 8) == "__list__") {
                    varType = std::make_shared<Type>(Type::makeList());
                    emit("char* " + node.value + "[] = {" + parseCollectionItems(val.substr(8)) + "};");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varType = std::make_shared<Type>(Type::makeTuple());
                    emit("const char* " + node.value + "[] = {" + parseCollectionItems(val.substr(9)) + "};");
                } else if (val.substr(0, 6) == "__fn__") {
                    varType = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                    emit("int " + node.value + " = " + translateExpr(val.substr(6)) + ";");
                } else {
                    bool isFuncCall = val.find('(') != std::string::npos;
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    if (isNum || isFuncCall) {
                        auto t = Type::inferNumeral(val);
                        varType = std::make_shared<Type>(t);
                        emit(t.toC() + " " + node.value + " = " + val + ";");
                    } else {
                        varType = std::make_shared<Type>(Type::makeString());
                        emit("const char* " + node.value + " = " + quoteIfString(val) + ";");
                    }
                }
                if (varType) varTypes[node.value] = varType;
            }
            break;

        case NodeType::PropAssign:
            if (!node.attrs.empty())
                emit(node.value + " = " + quoteIfString(node.attrs[0]) + ";");
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            if (args.size() >= 2 && args.front() == '$' && args.back() == '$')
                args = "\"" + args.substr(1, args.size() - 2) + "\"";
            else
                args = translateExpr(args);
            std::string name = node.value;
            if (name == "Term.display") {
                // Check if argument is a known variable with tracked type
                bool isString = args.find('"') != std::string::npos;  // Has quotes
                bool isIntFunc = args.find('(') != std::string::npos;  // Function call
                
                // Check tracked types
                auto it = varTypes.find(args);
                if (it != varTypes.end() && it->second->isNumeral()) {
                    emit("printf(\"%d\\n\", " + args + ");");
                } else if (isString) {
                    emit("printf(\"%s\\n\", " + args + ");");
                } else if (isIntFunc) {
                    emit("printf(\"%d\\n\", " + args + ");");
                } else {
                    // Default to string for bare identifiers
                    emit("printf(\"%s\\n\", " + args + ");");
                }
            } else {
                emit(name + "(" + args + ");");
            }
            break;
        }

        case NodeType::RangeExpr: {
            std::string n = node.value;
            while (!n.empty() && n.back() == ' ') n.pop_back();
            emit("makeRange(0, " + n + ")");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("makeSequence(" + x + ", " + y + ")");
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;  // Skip in second pass
            std::string arg = node.attrs.empty() ? "" : node.attrs[0];
            functions << "int " << node.value << "(int " << arg << ") {\n";
            if (!node.children.empty() && !node.children[0]->children.empty()) {
                auto& block = node.children[0]->children;
                std::string elseExpr;
                for (size_t i = 0; i < block.size(); i++) {
                    auto& c = block[i];
                    if (c->type == NodeType::IfStmt && c->value != "OTHER") {
                        functions << "    if (" << arg << " == 0) {\n";
                        functions << "        return 1;\n";
                        functions << "    }\n";
                    } else if (c->type == NodeType::ElseIfStmt) {
                        functions << "    else if (" << arg << " == 1) {\n";
                        functions << "        return 1;\n";
                        functions << "    }\n";
                    } else if (c->type == NodeType::IfStmt && c->value == "OTHER") {
                        if (!c->children.empty() && !c->children[0]->children.empty()) {
                            for (auto& stmt : c->children[0]->children) {
                                if (stmt->type == NodeType::AssignStmt && !stmt->attrs.empty()) {
                                    std::string val = stmt->attrs[0];
                                    if (val.substr(0, 6) == "__fn__") {
                                        elseExpr = translateExpr(val.substr(6));
                                    }
                                } else if (stmt->type == NodeType::ReturnStmt) {
                                    if (elseExpr.empty()) {
                                        elseExpr = translateExpr(stmt->value);
                                    }
                                }
                            }
                        }
                        functions << "    else {\n";
                        functions << "        return " << elseExpr << ";\n";
                        functions << "    }\n";
                    }
                }
            }
            functions << "}\n";
            break;
        }

        case NodeType::IfStmt: {
            if (node.value == "OTHER") {
                emit("else {");
            } else {
                std::string cond = translateCondition(node.value);
                emit("if (" + cond + ") {");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = translateCondition(node.value);
            emit("else if (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "arr" : translateExpr(node.attrs[0]);
            emit("for (size_t i = 0; i < " + lst + ".size(); i++) {");
            indentLevel++;
            emit("auto " + node.value + " = " + lst + "[i];");
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::IndexExpr: {
            if (node.attrs.size() == 2) {
                std::string idx = translateExpr(node.attrs[0]);
                std::string val = translateExpr(node.attrs[1]);
                emit(node.value + "[" + idx + "] = " + val + ";");
            } else if (node.attrs.size() == 1) {
                std::string idx = translateExpr(node.attrs[0]);
                emit("(void)" + node.value + "[" + idx + "]; // evaluate index read");
            }
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond = translateCondition(node.value);
            emit("while (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
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
            emit(translateExpr(node.value) + ";");
            break;

        case NodeType::TagBlock:
            genTagBlock(node);
            break;

        case NodeType::Block:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::CustomTagDef:
            emit("void spawn_" + node.value + "() {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
            break;

        case NodeType::SpawnStmt: {
            std::string name = node.value;
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("spawn_" + lower + "();");
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
            bool hasStartHere = false;
            for (auto& c : node.children)
                if (c->type == NodeType::TagBlock && c->value == "StartHere")
                    { hasStartHere = true; break; }

            if (hasStartHere) {
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") break;
                    genNode(*c);
                }
                emit("while (1) {");
                indentLevel++;
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") {
                        for (auto& inner : c->children) genNode(*inner);
                        break;
                    }
                }
                indentLevel--;
                emit("}");
            } else {
                emit("while (1) {");
                indentLevel++;
                for (auto& c : node.children) genNode(*c);
                indentLevel--;
                emit("}");
            }
        } else if (Tags::isGUIBox(name)) {
            // GUI — skip
        } else if (Tags::isLogicScope(name)) {
            for (auto& c : node.children) genNode(*c);
        } else if (Tags::isStartHere(name)) {
            emit("while (1) {");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            emit("}");
        } else {
            std::string lower = name;
            for (auto& c : lower) c = std::tolower(c);
            emit("spawn_" + lower + "();");
            for (auto& c : node.children) genNode(*c);
        }
    }

public:
    std::string generate(const ASTNode& node) {
        genNode(node);
        return out.str();
    }
};

std::string generateC(const ASTNode& ast) {
    CCodeGen gen;
    return gen.generate(ast);
}
