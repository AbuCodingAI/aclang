#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include "../include/tags.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <functional>
#include <map>

class CppCodeGen : public BaseCodeGen {
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

    void emitFunc(const std::string& line) {
        functions << std::string(indentLevel * 4, ' ') << line << "\n";
    }

    void emitFuncRaw(const std::string& line) { 
        functions << line << "\n"; 
    }

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        // $...$ is always a string - convert to "..."
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return "\"" + v.substr(1, v.size() - 2) + "\"";
        // "..." only works with fn - leave as is (parser should validate)
        return v;
    }

    std::string unwrapDollars(const std::string& s) {
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for C++
            std::string escaped;
            for (char c : content) {
                if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            return "\"" + escaped + "\"";
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
                    result += "\"" + escaped + "\"";
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

        // Translate AC booleans to C++ (case-insensitive)
        std::string result;
        for (size_t i = 0; i < r.size(); ) {
            if ((i == 0 || !std::isalnum(r[i-1])) && i + 4 <= r.size()) {
                std::string word = r.substr(i, 4);
                std::string lower = word;
                for (auto& c : lower) c = std::tolower(c);
                if (lower == "true" && (i + 4 >= r.size() || !std::isalnum(r[i+4]))) {
                    result += "true";
                    i += 4;
                    continue;
                }
            }
            if ((i == 0 || !std::isalnum(r[i-1])) && i + 5 <= r.size()) {
                std::string word = r.substr(i, 5);
                std::string lower = word;
                for (auto& c : lower) c = std::tolower(c);
                if (lower == "false" && (i + 5 >= r.size() || !std::isalnum(r[i+5]))) {
                    result += "false";
                    i += 5;
                    continue;
                }
            }
            if ((i == 0 || !std::isalnum(r[i-1])) && i + 4 <= r.size()) {
                std::string word = r.substr(i, 4);
                std::string lower = word;
                for (auto& c : lower) c = std::tolower(c);
                if (lower == "null" && (i + 4 >= r.size() || !std::isalnum(r[i+4]))) {
                    result += "nullptr";
                    i += 4;
                    continue;
                }
            }
            result += r[i];
            i++;
        }
        r = result;

        // Convert AC operators to C++
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;

        // Convert single = to == (but not ==, !=, <=, >=)
        for (size_t p = 0; p < r.size(); p++) {
            if (r[p] == '=' && (p == 0 || (r[p-1] != '#' && r[p-1] != '!' && r[p-1] != '<' && r[p-1] != '>')) &&
                (p+1 >= r.size() || r[p+1] != '=')) {
                r.insert(p, "=");
                p++;
            }
        }

        // Convert 'is' to '=='
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
    
    // Parse "key:value,key2:value2" into C++ map items
    std::string parseDictItems(const std::string& raw) {
        std::string s = raw;
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
            s = s.substr(1, s.size() - 2);
        std::string result;
        size_t i = 0;
        while (i < s.size()) {
            std::string key;
            while (i < s.size() && s[i] != ':') key += s[i++];
            size_t a = key.find_first_not_of(' ');
            size_t b = key.find_last_not_of(' ');
            if (a != std::string::npos) key = key.substr(a, b - a + 1);
            if (i < s.size() && s[i] == ':') i++;
            std::string value;
            while (i < s.size() && s[i] != ',') value += s[i++];
            a = value.find_first_not_of(' ');
            b = value.find_last_not_of(' ');
            if (a != std::string::npos) value = value.substr(a, b - a + 1);
            if (i < s.size() && s[i] == ',') i++;
            if (!key.empty()) {
                if (!result.empty()) result += ", ";
                bool keyIsNum = !key.empty() && (std::isdigit(key[0]) || key[0] == '-');
                bool valIsNum = !value.empty() && (std::isdigit(value[0]) || value[0] == '-');
                result += "{" + (keyIsNum ? key : "\"" + key + "\"") + ", " + (valIsNum ? value : "\"" + value + "\"") + "}";
            }
        }
        return result;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
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
            currentOut = &out;
            emitRaw("// Generated by AC Compiler (AC->C++)");
            emitRaw("#include <iostream>");
            emitRaw("#include <vector>");
            emitRaw("#include <string>");
            emitRaw("#include <map>");
            emitRaw("#include <functional>");
            emitRaw("#include <unordered_map>");
            
            // Only emit event listener includes if actually used
            bool needsEventListener = usesEventListener(node);
            if (needsEventListener) {
                emitRaw("#include \"../library/keybinds/keybinds.hpp\"");
                emitRaw("#include \"../library/event_listener/event_listener.hpp\"");
            }
            
            emitRaw("using namespace std;");
            if (needsEventListener) {
                emitRaw("using namespace ACKeybinds;");
            }
            emitRaw("");
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty()) {
                    std::string val = c->attrs[0];
                    bool isStr = (!val.empty() && val.front() == '$');
                    if (isStr) emitRaw("string " + c->value + " = " + quoteIfString(val) + ";");
                    else       emitRaw("int " + c->value + " = " + translateExpr(val) + ";");
                }
            }
            emitRaw("");
            collectingFunctions = true;
            {
                std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                    if (n.type == NodeType::FuncDef) genNode(n);
                    for (auto& c : n.children) collectFuncs(*c);
                };
                for (auto& c : node.children) {
                    collectFuncs(*c);
                }
            }
            collectingFunctions = false;
            emitRaw(functions.str());
            emitRaw("");
            emitRaw("int main() {");
            indentLevel++;
            if (mainloopBlock) {
                emit("while (true) {");
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
        case NodeType::UseLibStmt: {
            // use ilib <libname> -> include library/<libname>/<libname>.hpp
            std::string libName = node.value;
            
            // Hardcoded camera library path for all backends
            if (libName == "camera") {
                emitRaw("#include \"../library/camera/camera.hpp\"");
                emitRaw("using namespace AC;");
            } else {
                emitRaw("#include \"../library/" + libName + "/" + libName + ".hpp\"");
                emitRaw("using namespace AC;");
            }
            break;
        }
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$')
                val = "\"" + val.substr(1, val.size() - 2) + "\"";
            std::string line = "cout << " + val + " << endl;";
            emit(line);
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

        case NodeType::RaiseStmt: {
            std::string msg = "Preposterous: " + node.value;
            std::string line1 = "cerr << \"" + msg + "\" << endl;";
            emit(line1);
            emit("exit(1);");
            break;
        }

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value) + ";");
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                TypePtr varType = nullptr;

                if (val.substr(0, 8) == "__list__") {
                    varType = std::make_shared<Type>(Type::makeList());
                    emit("vector<string> " + node.value + " = {" + parseCollectionItems(val.substr(8)) + "};");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varType = std::make_shared<Type>(Type::makeTuple());
                    emit("const vector<string> " + node.value + " = {" + parseCollectionItems(val.substr(9)) + "};");
                } else if (val.substr(0, 8) == "__dict__") {
                    varType = std::make_shared<Type>(Type::makeUnknown());
                    emit("map<string, string> " + node.value + " = {" + parseDictItems(val.substr(8)) + "};  // dict");
                } else if (val.substr(0, 6) == "__fn__") {
                    varType = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                    emit("int " + node.value + " = " + translateExpr(val.substr(6)) + ";");
                } else if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
                    // Function call: name = func(arg1, arg2)
                    if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
                        auto& funcCall = *node.children[0];
                        std::string funcName = funcCall.value;
                        std::string args;
                        for (size_t i = 0; i < funcCall.attrs.size(); i++) {
                            if (i > 0) args += ", ";
                            std::string attr = funcCall.attrs[i];
                            // Check if it's a keyword argument
                            size_t eqPos = attr.find('=');
                            if (eqPos != std::string::npos && eqPos > 0) {
                                // Extract value part
                                std::string v = attr.substr(eqPos + 1);
                                while (!v.empty() && v.back() == ' ') v.pop_back();
                                while (!v.empty() && v.front() == ' ') v = v.substr(1);
                                args += v;
                            } else {
                                // Positional argument
                                args += attr;
                            }
                        }
                        emit("int " + node.value + " = " + funcName + "(" + args + ");");
                    }
                } else {
                    // Check if it's a keyword (null, true, false)
                    std::string valLower = val;
                    for (auto& c : valLower) c = std::tolower(c);
                    bool isKeyword = (valLower == "null" || valLower == "true" || valLower == "false");
                    
                    bool isFuncCall = val.find('(') != std::string::npos;
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool hasAt = val.find('@') != std::string::npos;
                    if (isNum || isFuncCall || hasAt || isKeyword) {
                        auto t = Type::inferNumeral(val);
                        varType = std::make_shared<Type>(t);
                        emit(t.toCpp() + " " + node.value + " = " + translateExpr(val) + ";");
                    } else {
                        varType = std::make_shared<Type>(Type::makeString());
                        emit("string " + node.value + " = " + quoteIfString(val) + ";");
                    }
                }
                if (varType) varTypes[node.value] = varType;
            }
            break;

        case NodeType::PlusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " += " + quoteIfString(node.attrs[0]) + ";");
            }
            break;

        case NodeType::MinusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " -= " + quoteIfString(node.attrs[0]) + ";");
            }
            break;

        case NodeType::MultiplyEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + quoteIfString(node.attrs[0]) + ";");
            }
            break;

        case NodeType::DivideEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " /= " + quoteIfString(node.attrs[0]) + ";");
            }
            break;

        case NodeType::AtEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + quoteIfString(node.attrs[0]) + ";");
            }
            break;

        case NodeType::PropAssign:
            if (!node.attrs.empty())
                emit(node.value + " = " + quoteIfString(node.attrs[0]) + ";");
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            // Handle $...$ strings (always strings)
            if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                args = "\"" + args.substr(1, args.size() - 2) + "\"";
            } else {
                args = translateExpr(args);
            }
            std::string name = node.value;
            if (name == "Term.display") {
                bool isString = args.find('"') != std::string::npos;
                bool isIntFunc = args.find('(') != std::string::npos;
                auto it = varTypes.find(args);
                if (it != varTypes.end() && it->second->isNumeral()) {
                    emit("cout << " + args + " << endl;");
                } else if (isString) {
                    emit("cout << " + args + " << endl;");
                } else if (isIntFunc) {
                    emit("cout << " + args + " << endl;");
                } else {
                    emit("cout << " + args + " << endl;");
                }
            } else if (name == "Term.ask") {
                emit("string input;");
                emit("getline(cin, input);");
            } else {
                emit(name + "(" + args + ");");
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
            if (!collectingFunctions) break;
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", int ";
                else args = "int ";
                args += node.attrs[i];
            }
            
            currentOut = &functions;
            emitRaw("int " + node.value + "(" + args + ") {");
            indentLevel++;
            
            if (!node.children.empty()) {
                genBlock(*node.children[0]);
            }
            
            indentLevel--;
            emit("}");
            
            currentOut = &out;
            break;
        }

        case NodeType::IfStmt: {
            if (node.value == "OTHER") {
                emit("} else {");
            } else {
                std::string cond = translateCondition(node.value);
                emit("if (" + cond + ") {");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            
            // Process ELSEIF and OTHER children
            for (size_t i = 1; i < node.children.size(); i++) {
                genNode(*node.children[i]);
            }
            
            // Emit closing brace only if this is not the parent of an if/else-if/else chain
            // (i.e., if the last child is not an IfStmt with value "OTHER")
            bool hasOtherChild = false;
            if (!node.children.empty() && node.children.size() > 1) {
                auto& lastChild = node.children.back();
                if (lastChild->type == NodeType::IfStmt && lastChild->value == "OTHER") {
                    hasOtherChild = true;
                }
            }
            
            if (!hasOtherChild) {
                emit("}");
            }
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = translateCondition(node.value);
            emit("} else if (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
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
            emit("vector<string> list = {" + parseCollectionItems(node.value) + "};");
            break;

        case NodeType::TupleLiteral:
            emit("const vector<string> tuple = {" + parseCollectionItems(node.value) + "};");
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
            // Generate event listener setup
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    std::string callback = child->attrs.empty() ? "" : child->attrs[0];
                    if (!callback.empty()) {
                        // Extract function name without parentheses
                        size_t parenPos = callback.find('(');
                        std::string funcName = (parenPos != std::string::npos) ? callback.substr(0, parenPos) : callback;
                        emit("ac::global_listener.bind(\"" + key + "\", " + funcName + ");");
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
            emit("// Simulate key press: " + key);
            emit("ac::global_listener.trigger(\"" + key + "\");");
            break;
        }

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
                if (c->type == NodeType::TagBlock && Tags::isStartHere(c->value))
                    { hasStartHere = true; break; }

            if (hasStartHere) {
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") break;
                    genNode(*c);
                }
                emit("while (true) {");
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
                emit("while (true) {");
                indentLevel++;
                for (auto& c : node.children) genNode(*c);
                indentLevel--;
                emit("}");
            }
        } else if (Tags::isGUIBox(name)) {
            emit("gui.createBox(/*args*/)");
        } else if (Tags::isLogicScope(name)) {
            emit("logic.pushScope(/*args*/)");
        } else if (Tags::isStartHere(name)) {
            emit("logic.setStartHere(/*args*/)");
        } else {
            emit("gui.createTag(\"" + name + "\", /*args*/)");
        }
    }

public:
    std::string generate(const ASTNode& node) {
        genNode(node);
        return out.str();
    }
};

std::string generateCpp(const ASTNode& ast) {
    CppCodeGen gen;
    return gen.generate(ast);
}
