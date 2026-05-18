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

class VCodeGen : public BaseCodeGen {
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
            // Escape special characters for V
            std::string escaped;
            for (char c : content) {
                if (c == '\'') escaped += "\\'";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            return "'" + escaped + "'";
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
                        if (c == '\'') escaped += "\\'";
                        else if (c == '\\') escaped += "\\\\";
                        else if (c == '\n') escaped += "\\n";
                        else if (c == '\t') escaped += "\\t";
                        else escaped += c;
                    }
                    result += "'" + escaped + "'";
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

        // Translate AC booleans to V (case-insensitive)
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
                    result += "none";
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

        // Convert AC operators to V
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
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
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

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            currentOut = &out;
            emitRaw("// Generated by AC Compiler (AC->V)");
            emitRaw("import os");
            emitRaw("");
            
            // Only emit event listener code if actually used
            bool needsEventListener = usesEventListener(node);
            if (needsEventListener) {
                emitRaw("// Event listener implementation");
                emitRaw("struct EventListener {");
                emitRaw("mut:");
                emitRaw("    bindings map[string]fn()");
                emitRaw("}");
                emitRaw("fn new_event_listener() EventListener {");
                emitRaw("    return EventListener{ bindings: map[string]fn(){} }");
                emitRaw("}");
                emitRaw("fn (mut el EventListener) bind(key string, callback fn()) {");
                emitRaw("    el.bindings[key] = callback");
                emitRaw("}");
                emitRaw("fn (el EventListener) trigger(key string) {");
                emitRaw("    // Automatically trigger if binding exists, otherwise do nothing");
                emitRaw("    if key in el.bindings { el.bindings[key]() }");
                emitRaw("}");
                emitRaw("fn (el EventListener) check_and_trigger(key string) {");
                emitRaw("    // Check and trigger - if no binding, do absolutely nothing");
                emitRaw("    el.trigger(key)");
                emitRaw("}");
                emitRaw("__global ac_event_listener = new_event_listener()");
                emitRaw("");
                emitRaw("// Keybinds");
                emitRaw("const (");
                emitRaw("    key_1 = \"1\"");
                emitRaw("    key_2 = \"2\"");
                emitRaw("    key_3 = \"3\"");
                emitRaw("    key_4 = \"4\"");
                emitRaw("    key_5 = \"5\"");
                emitRaw("    key_6 = \"6\"");
                emitRaw("    key_7 = \"7\"");
                emitRaw("    key_8 = \"8\"");
                emitRaw("    key_9 = \"9\"");
                emitRaw("    key_0 = \"0\"");
                emitRaw("    key_minus = \"minus\"");
                emitRaw("    key_equal = \"equal\"");
                emitRaw("    key_backspace = \"backspace\"");
                emitRaw("    key_backslash = \"backslash\"");
                emitRaw("    key_tab = \"tab\"");
                emitRaw("    key_q = \"q\"");
                emitRaw("    key_w = \"w\"");
                emitRaw("    key_e = \"e\"");
                emitRaw("    key_r = \"r\"");
                emitRaw("    key_t = \"t\"");
                emitRaw("    key_y = \"y\"");
                emitRaw("    key_u = \"u\"");
                emitRaw("    key_i = \"i\"");
                emitRaw("    key_o = \"o\"");
                emitRaw("    key_p = \"p\"");
                emitRaw("    key_lbracket = \"[\"");
                emitRaw("    key_rbracket = \"]\"");
                emitRaw("    key_caps = \"caps\"");
                emitRaw("    key_a = \"a\"");
                emitRaw("    key_s = \"s\"");
                emitRaw("    key_d = \"d\"");
                emitRaw("    key_f = \"f\"");
                emitRaw("    key_g = \"g\"");
                emitRaw("    key_h = \"h\"");
                emitRaw("    key_j = \"j\"");
                emitRaw("    key_k = \"k\"");
                emitRaw("    key_l = \"l\"");
                emitRaw("    key_semicolon = \";\"");
                emitRaw("    key_apostrophe = \"'\"");
                emitRaw("    key_enter = \"enter\"");
                emitRaw("    key_shift = \"shift\"");
                emitRaw("    key_z = \"z\"");
                emitRaw("    key_x = \"x\"");
                emitRaw("    key_c = \"c\"");
                emitRaw("    key_v = \"v\"");
                emitRaw("    key_b = \"b\"");
                emitRaw("    key_n = \"n\"");
                emitRaw("    key_m = \"m\"");
                emitRaw("    key_comma = \",\"");
                emitRaw("    key_period = \".\"");
                emitRaw("    key_slash = \"/\"");
                emitRaw("    key_ctrl = \"ctrl\"");
                emitRaw("    key_alt = \"alt\"");
                emitRaw("    key_space = \"space\"");
                emitRaw("    key_fn = \"fn\"");
                emitRaw("    key_super = \"super\"");
                emitRaw("    key_windows = \"windows\"");
                emitRaw("    key_up = \"up\"");
                emitRaw("    key_down = \"down\"");
                emitRaw("    key_left = \"left\"");
                emitRaw("    key_right = \"right\"");
                emitRaw(")");
                emitRaw("");
            }
            
            // Find mainloop tag block
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop") {
                    mainloopBlock = c.get();
                    break;
                }
            }
            
            // First pass: collect functions from entire program
            collectingFunctions = true;
            {
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
            }
            collectingFunctions = false;
            
            // Emit functions
            emitRaw(functions.str());
            
            emitRaw("fn main() {");
            indentLevel++;
            
            // Second pass: emit main body from mainloop
            if (mainloopBlock) {
                for (auto& c : mainloopBlock->children) genNode(*c);
            }
            
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib <libname> -> import library.<libname>
            std::string libName = node.value;
            emitRaw("// Library: " + libName);
            emitRaw("// Note: V library support requires manual setup");
            break;
        }
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
                
                if (val.substr(0, 8) == "__dict__") {
                    // V: map[string]string
                    emit("mut " + node.value + " := map[string]string{" + parseDictItems(val.substr(8)) + "}");
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
                        emit("mut " + node.value + " := " + funcName + "(" + args + ")");
                    }
                } else {
                    // Check if it's a keyword (null, true, false)
                    std::string valLower = val;
                    for (auto& c : valLower) c = std::tolower(c);
                    bool isKeyword = (valLower == "null" || valLower == "true" || valLower == "false");
                    
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool isFuncCall = val.find('(') != std::string::npos;
                    bool hasAt = val.find('@') != std::string::npos;
                    if (isNum || isFuncCall || hasAt || isKeyword) {
                        auto t = Type::inferNumeral(val);
                        varTypes[node.value] = std::make_shared<Type>(t);
                        // V type names: int, f64
                        std::string vtype = t.isDec() ? "f64" : "int";
                        emit("mut " + node.value + " := " + vtype + "(" + translateExpr(val) + ")");
                    } else {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                        emit("mut " + node.value + " := " + unwrapDollars(val));
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
                    args = "'" + args.substr(1, args.size() - 2) + "'";
                }
                emit("println(" + args + ")");
            } else if (name == "Term.ask") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "'" + args.substr(1, args.size() - 2) + "'";
                }
                emit("input := os.input(" + args + ")");
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
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += node.attrs[i] + " int";
            }
            
            currentOut = &functions;
            emitRaw("fn " + node.value + "(" + args + ") int {");
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
            // Generate event listener setup
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    std::string callback = child->attrs.empty() ? "" : child->attrs[0];
                    if (!callback.empty()) {
                        emit("ac_event_listener.bind('" + key + "', fn() { " + callback + " })");
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
            emit("ac_event_listener.trigger('" + key + "')");
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

std::string generateV(const ASTNode& ast) {
    VCodeGen gen;
    return gen.generate(ast);
}
