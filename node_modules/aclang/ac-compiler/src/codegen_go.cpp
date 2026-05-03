#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <map>
#include <functional>

class GoCodeGen : public BaseCodeGen {
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
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for Go
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

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return "\"" + v.substr(1, v.size() - 2) + "\"";
        return v;
    }

    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);

        // Translate AC booleans to Go (case-insensitive)
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
                if (lower == "null" && (i + 4 >= r.size() || !std::isalnum(r[i+4]))) {
                    result += "nil";
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
            result += r[i];
            i++;
        }
        r = result;

        // Convert AC operators to Go
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;

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
    
    // Parse "key:value,key2:value2" into Go map items
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
                result += (keyIsNum ? key : "\"" + key + "\"") + ": " + (valIsNum ? value : "\"" + value + "\"");
            }
        }
        return result;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        
        // Translate AC booleans and null to Go (case-insensitive)
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
                if (lower == "null" && (i + 4 >= r.size() || !std::isalnum(r[i+4]))) {
                    result += "nil";
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
            result += r[i];
            i++;
        }
        r = result;
        
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        
        // In AC, single = in condition context means ==
        for (size_t p = 0; p < r.size(); p++) {
            if (r[p] == '=' && (p == 0 || (r[p-1] != '#' && r[p-1] != '!' && r[p-1] != '<' && r[p-1] != '>')) &&
                (p+1 >= r.size() || r[p+1] != '=')) {
                r.insert(p, "=");
                p++;
            }
        }
        
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
            emitRaw("// Generated by AC Compiler (AC->Go)");
            emitRaw("package main");
            emitRaw("");
            emitRaw("import (");
            emitRaw("    \"fmt\"");
            emitRaw("    \"os\"");
            emitRaw(")");
            emitRaw("");
            emitRaw("// Event listener implementation");
            emitRaw("type EventListener struct {");
            emitRaw("    bindings map[string]func()");
            emitRaw("}");
            emitRaw("func NewEventListener() *EventListener {");
            emitRaw("    return &EventListener{bindings: make(map[string]func())}");
            emitRaw("}");
            emitRaw("func (el *EventListener) Bind(key string, callback func()) {");
            emitRaw("    el.bindings[key] = callback");
            emitRaw("}");
            emitRaw("func (el *EventListener) Trigger(key string) {");
            emitRaw("    // Automatically trigger if binding exists, otherwise do nothing");
            emitRaw("    if cb, ok := el.bindings[key]; ok { cb() }");
            emitRaw("}");
            emitRaw("func (el *EventListener) CheckAndTrigger(key string) {");
            emitRaw("    // Check and trigger - if no binding, do absolutely nothing");
            emitRaw("    el.Trigger(key)");
            emitRaw("}");
            emitRaw("var acEventListener = NewEventListener()");
            emitRaw("");
            emitRaw("// Keybinds");
            emitRaw("const (");
            emitRaw("    KEY_1 = \"1\"");
            emitRaw("    KEY_2 = \"2\"");
            emitRaw("    KEY_3 = \"3\"");
            emitRaw("    KEY_4 = \"4\"");
            emitRaw("    KEY_5 = \"5\"");
            emitRaw("    KEY_6 = \"6\"");
            emitRaw("    KEY_7 = \"7\"");
            emitRaw("    KEY_8 = \"8\"");
            emitRaw("    KEY_9 = \"9\"");
            emitRaw("    KEY_0 = \"0\"");
            emitRaw("    KEY_MINUS = \"minus\"");
            emitRaw("    KEY_EQUAL = \"equal\"");
            emitRaw("    KEY_BACKSPACE = \"backspace\"");
            emitRaw("    KEY_BACKSLASH = \"backslash\"");
            emitRaw("    KEY_TAB = \"tab\"");
            emitRaw("    KEY_Q = \"q\"");
            emitRaw("    KEY_W = \"w\"");
            emitRaw("    KEY_E = \"e\"");
            emitRaw("    KEY_R = \"r\"");
            emitRaw("    KEY_T = \"t\"");
            emitRaw("    KEY_Y = \"y\"");
            emitRaw("    KEY_U = \"u\"");
            emitRaw("    KEY_I = \"i\"");
            emitRaw("    KEY_O = \"o\"");
            emitRaw("    KEY_P = \"p\"");
            emitRaw("    KEY_LBRACKET = \"[\"");
            emitRaw("    KEY_RBRACKET = \"]\"");
            emitRaw("    KEY_CAPS = \"caps\"");
            emitRaw("    KEY_A = \"a\"");
            emitRaw("    KEY_S = \"s\"");
            emitRaw("    KEY_D = \"d\"");
            emitRaw("    KEY_F = \"f\"");
            emitRaw("    KEY_G = \"g\"");
            emitRaw("    KEY_H = \"h\"");
            emitRaw("    KEY_J = \"j\"");
            emitRaw("    KEY_K = \"k\"");
            emitRaw("    KEY_L = \"l\"");
            emitRaw("    KEY_SEMICOLON = \";\"");
            emitRaw("    KEY_APOSTROPHE = \"'\"");
            emitRaw("    KEY_ENTER = \"enter\"");
            emitRaw("    KEY_SHIFT = \"shift\"");
            emitRaw("    KEY_Z = \"z\"");
            emitRaw("    KEY_X = \"x\"");
            emitRaw("    KEY_C = \"c\"");
            emitRaw("    KEY_V = \"v\"");
            emitRaw("    KEY_B = \"b\"");
            emitRaw("    KEY_N = \"n\"");
            emitRaw("    KEY_M = \"m\"");
            emitRaw("    KEY_COMMA = \",\"");
            emitRaw("    KEY_PERIOD = \".\"");
            emitRaw("    KEY_SLASH = \"/\"");
            emitRaw("    KEY_CTRL = \"ctrl\"");
            emitRaw("    KEY_ALT = \"alt\"");
            emitRaw("    KEY_SPACE = \"space\"");
            emitRaw("    KEY_FN = \"fn\"");
            emitRaw("    KEY_SUPER = \"super\"");
            emitRaw("    KEY_WINDOWS = \"windows\"");
            emitRaw("    KEY_UP = \"up\"");
            emitRaw("    KEY_DOWN = \"down\"");
            emitRaw("    KEY_LEFT = \"left\"");
            emitRaw("    KEY_RIGHT = \"right\"");
            emitRaw(")");
            emitRaw("");
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty()) {
                    std::string val = c->attrs[0];
                    emitRaw("var " + c->value + " = " + unwrapDollars(val));
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
            emitRaw("func main() {");
            indentLevel++;
            if (mainloopBlock) {
                emit("for {");
                indentLevel++;
                for (auto& c : mainloopBlock->children) genNode(*c);
                indentLevel--;
                emit("}");
            }
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib camera -> import camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("// Camera library not yet implemented for Go");
                emitRaw("// TODO: Add camera package");
            }
            break;
        }
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                val = "\"" + val.substr(1, val.size() - 2) + "\"";
            }
            emit("fmt.Println(" + val + ")");
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
            emit("os.Exit(0)");
            break;

        case NodeType::RaiseStmt:
            emit("fmt.Fprintf(os.Stderr, \"Preposterous: " + node.value + "\\n\")");
            emit("os.Exit(1)");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value));
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                
                if (val.substr(0, 8) == "__dict__") {
                    // Dict: map[string]interface{}
                    emit(node.value + " := map[string]interface{}{" + parseDictItems(val.substr(8)) + "}");
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
                        emit(node.value + " := " + funcName + "(" + args + ")");
                    }
                } else {
                    // Check if it's null (special handling for Go)
                    std::string valLower = val;
                    for (auto& c : valLower) c = std::tolower(c);
                    
                    if (valLower == "null") {
                        // In Go, nil needs a type - use interface{}
                        emit("var " + node.value + " interface{} = nil");
                    } else {
                        bool isKeyword = (valLower == "true" || valLower == "false");
                        bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                        bool isFuncCall = val.find('(') != std::string::npos;
                        bool hasAt = val.find('@') != std::string::npos;
                        if (isNum || isFuncCall || hasAt || isKeyword) {
                            auto t = Type::inferNumeral(val);
                            varTypes[node.value] = std::make_shared<Type>(t);
                            // Go uses := for type inference, but we add a comment for clarity
                            emit(node.value + " := " + translateExpr(val) + " // " + t.toString());
                        } else {
                            varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                            emit(node.value + " := " + unwrapDollars(val));
                        }
                    }
                }
            }
            break;

        case NodeType::PropAssign:
            break;

        case NodeType::PlusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " += " + translateExpr(node.attrs[0]));
            }
            break;

        case NodeType::MinusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " -= " + translateExpr(node.attrs[0]));
            }
            break;

        case NodeType::MultiplyEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + translateExpr(node.attrs[0]));
            }
            break;

        case NodeType::DivideEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " /= " + translateExpr(node.attrs[0]));
            }
            break;

        case NodeType::AtEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + translateExpr(node.attrs[0]));
            }
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;
            if (name == "Term.display") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "\"" + args.substr(1, args.size() - 2) + "\"";
                }
                emit("fmt.Println(" + args + ")");
            } else if (name == "Term.ask") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "\"" + args.substr(1, args.size() - 2) + "\"";
                }
                emit("var _input string");
                emit("fmt.Scanln(&_input)");
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
            emit("func() []int { _r := make([]int, (" + n + ") + 1); for _i := 0; _i <= " + n + "; _i++ { _r[_i] = _i }; return _r }()");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("func() []int { _r := make([]int, (" + y + ") - (" + x + ") + 1); for _i := (" + x + "); _i <= " + y + "; _i++ { _r[_i - (" + x + ")] = _i }; return _r }()");
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += node.attrs[i] + " int";
            }
            
            currentOut = &functions;
            emitRaw("func " + node.value + "(" + args + ") int {");
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
            std::string cond = node.value;
            if (cond == "OTHER") {
                emit("} else {");
            } else {
                emit("if " + translateExpr(cond) + " {");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            
            // Process ELSEIF and OTHER children
            for (size_t i = 1; i < node.children.size(); i++) {
                genNode(*node.children[i]);
            }
            
            // Only emit closing brace if there are no children
            if (node.children.size() <= 1) {
                emit("}");
            }
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = node.value;
            emit("} else if " + translateExpr(cond) + " {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            // Don't emit closing brace - let the parent IfStmt handle it
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "arr" : translateExpr(node.attrs[0]);
            emit("for _, " + node.value + " := range " + lst + " {");
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

        case NodeType::IndexExpr: {
            if (node.attrs.size() == 2) {
                std::string idx = translateExpr(node.attrs[0]);
                std::string val = translateExpr(node.attrs[1]);
                emit(node.value + "[" + idx + "] = " + val);
            } else if (node.attrs.size() == 1) {
                std::string idx = translateExpr(node.attrs[0]);
                emit("_ = " + node.value + "[" + idx + "]");
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
            emit("func spawn_" + node.value + "() {");
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
            // Generate event listener setup
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    std::string callback = child->attrs.empty() ? "" : child->attrs[0];
                    if (!callback.empty()) {
                        emit("acEventListener.Bind(\"" + key + "\", func() { " + callback + " })");
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
            emit("acEventListener.Trigger(\"" + key + "\")");
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

std::string generateGo(const ASTNode& ast) {
    GoCodeGen gen;
    return gen.generate(ast);
}
