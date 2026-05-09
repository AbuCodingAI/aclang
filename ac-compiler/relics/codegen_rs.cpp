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

class RsCodeGen : public BaseCodeGen {
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
            // Escape special characters for Rust
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

        // Convert AC operators to Rust
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;

        // Convert 'is' to '=='
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
    
    // Parse "key:value,key2:value2" into Rust HashMap items
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
                result += "(" + (keyIsNum ? key : "\"" + key + "\"") + ", " + (valIsNum ? value : "\"" + value + "\"") + ")";
            }
        }
        return result;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        
        // Translate AC booleans and null to Rust (case-insensitive)
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
                    result += "None";
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
            emitRaw("// Generated by AC Compiler (AC->Rust)");
            emitRaw("use std::collections::HashMap;");
            emitRaw("use std::sync::{Mutex, OnceLock};");
            emitRaw("");
            emitRaw("// Event listener implementation");
            emitRaw("struct EventListener {");
            emitRaw("    bindings: HashMap<String, Box<dyn Fn() + Send>>,");
            emitRaw("}");
            emitRaw("impl EventListener {");
            emitRaw("    fn new() -> Self { EventListener { bindings: HashMap::new() } }");
            emitRaw("    fn bind(&mut self, key: &str, callback: Box<dyn Fn() + Send>) {");
            emitRaw("        self.bindings.insert(key.to_string(), callback);");
            emitRaw("    }");
            emitRaw("    fn trigger(&self, key: &str) {");
            emitRaw("        // Automatically trigger if binding exists, otherwise do nothing");
            emitRaw("        if let Some(cb) = self.bindings.get(key) { cb(); }");
            emitRaw("    }");
            emitRaw("    fn check_and_trigger(&self, key: &str) {");
            emitRaw("        // Check and trigger - if no binding, do absolutely nothing");
            emitRaw("        self.trigger(key);");
            emitRaw("    }");
            emitRaw("}");
            emitRaw("static AC_EVENT_LISTENER: OnceLock<Mutex<EventListener>> = OnceLock::new();");
            emitRaw("fn get_event_listener() -> &'static Mutex<EventListener> {");
            emitRaw("    AC_EVENT_LISTENER.get_or_init(|| Mutex::new(EventListener::new()))");
            emitRaw("}");
            emitRaw("");
            emitRaw("// Keybinds");
            emitRaw("const KEY_1: &str = \"1\";");
            emitRaw("const KEY_2: &str = \"2\";");
            emitRaw("const KEY_3: &str = \"3\";");
            emitRaw("const KEY_4: &str = \"4\";");
            emitRaw("const KEY_5: &str = \"5\";");
            emitRaw("const KEY_6: &str = \"6\";");
            emitRaw("const KEY_7: &str = \"7\";");
            emitRaw("const KEY_8: &str = \"8\";");
            emitRaw("const KEY_9: &str = \"9\";");
            emitRaw("const KEY_0: &str = \"0\";");
            emitRaw("const KEY_MINUS: &str = \"minus\";");
            emitRaw("const KEY_EQUAL: &str = \"equal\";");
            emitRaw("const KEY_BACKSPACE: &str = \"backspace\";");
            emitRaw("const KEY_BACKSLASH: &str = \"backslash\";");
            emitRaw("const KEY_TAB: &str = \"tab\";");
            emitRaw("const KEY_Q: &str = \"q\";");
            emitRaw("const KEY_W: &str = \"w\";");
            emitRaw("const KEY_E: &str = \"e\";");
            emitRaw("const KEY_R: &str = \"r\";");
            emitRaw("const KEY_T: &str = \"t\";");
            emitRaw("const KEY_Y: &str = \"y\";");
            emitRaw("const KEY_U: &str = \"u\";");
            emitRaw("const KEY_I: &str = \"i\";");
            emitRaw("const KEY_O: &str = \"o\";");
            emitRaw("const KEY_P: &str = \"p\";");
            emitRaw("const KEY_LBRACKET: &str = \"[\";");
            emitRaw("const KEY_RBRACKET: &str = \"]\";");
            emitRaw("const KEY_CAPS: &str = \"caps\";");
            emitRaw("const KEY_A: &str = \"a\";");
            emitRaw("const KEY_S: &str = \"s\";");
            emitRaw("const KEY_D: &str = \"d\";");
            emitRaw("const KEY_F: &str = \"f\";");
            emitRaw("const KEY_G: &str = \"g\";");
            emitRaw("const KEY_H: &str = \"h\";");
            emitRaw("const KEY_J: &str = \"j\";");
            emitRaw("const KEY_K: &str = \"k\";");
            emitRaw("const KEY_L: &str = \"l\";");
            emitRaw("const KEY_SEMICOLON: &str = \";\";");
            emitRaw("const KEY_APOSTROPHE: &str = \"'\";");
            emitRaw("const KEY_ENTER: &str = \"enter\";");
            emitRaw("const KEY_SHIFT: &str = \"shift\";");
            emitRaw("const KEY_Z: &str = \"z\";");
            emitRaw("const KEY_X: &str = \"x\";");
            emitRaw("const KEY_C: &str = \"c\";");
            emitRaw("const KEY_V: &str = \"v\";");
            emitRaw("const KEY_B: &str = \"b\";");
            emitRaw("const KEY_N: &str = \"n\";");
            emitRaw("const KEY_M: &str = \"m\";");
            emitRaw("const KEY_COMMA: &str = \",\";");
            emitRaw("const KEY_PERIOD: &str = \".\";");
            emitRaw("const KEY_SLASH: &str = \"/\";");
            emitRaw("const KEY_CTRL: &str = \"ctrl\";");
            emitRaw("const KEY_ALT: &str = \"alt\";");
            emitRaw("const KEY_SPACE: &str = \"space\";");
            emitRaw("const KEY_FN: &str = \"fn\";");
            emitRaw("const KEY_SUPER: &str = \"super\";");
            emitRaw("const KEY_WINDOWS: &str = \"windows\";");
            emitRaw("const KEY_UP: &str = \"up\";");
            emitRaw("const KEY_DOWN: &str = \"down\";");
            emitRaw("const KEY_LEFT: &str = \"left\";");
            emitRaw("const KEY_RIGHT: &str = \"right\";");
            emitRaw("");
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty()) {
                    std::string val = c->attrs[0];
                    bool isStr = (!val.empty() && val.front() == '$');
                    if (isStr) {
                        emitRaw("static " + c->value + ": &str = " + unwrapDollars(val) + ";");
                    } else {
                        bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                        if (isNum) {
                            auto t = Type::inferNumeral(val);
                            emitRaw("static " + c->value + ": " + t.toRs() + " = " + val + ";");
                        } else {
                            emitRaw("static " + c->value + ": &str = \"" + val + "\";");
                        }
                    }
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
            emitRaw("fn main() {");
            indentLevel++;
            if (mainloopBlock) {
                emit("loop {");
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
            // use ilib camera -> include camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("// Camera library not yet implemented for Rust");
                emitRaw("// TODO: Add camera module");
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
            emit("println!(\"{}\", " + val + ");");
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
            emit("std::process::exit(0);");
            break;

        case NodeType::RaiseStmt:
            emit("eprintln!(\"Preposterous: " + node.value + "\");");
            emit("std::process::exit(1);");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value) + ";");
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                
                if (val.substr(0, 8) == "__dict__") {
                    // Dict: HashMap
                    emit("use std::collections::HashMap;");
                    emit("let mut " + node.value + " = HashMap::from([" + parseDictItems(val.substr(8)) + "]);");
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
                        emit("let mut " + node.value + " = " + funcName + "(" + args + ");");
                    }
                } else {
                    // Check if it's a keyword (null, true, false)
                    std::string valLower = val;
                    for (auto& c : valLower) c = std::tolower(c);
                    bool isKeyword = (valLower == "null" || valLower == "true" || valLower == "false");
                    
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool isFuncCall = val.find('(') != std::string::npos;
                    bool hasAt = val.find('@') != std::string::npos;
                    if (isNum || hasAt) {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                        emit("let mut " + node.value + ": i32 = " + translateExpr(val) + ";");
                    } else if (isFuncCall || isKeyword) {
                        // For function calls and keywords, let Rust infer the type
                        emit("let mut " + node.value + " = " + translateExpr(val) + ";");
                    } else {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                        emit("let mut " + node.value + " = " + unwrapDollars(val) + ";");
                    }
                }
            }
            break;

        case NodeType::PropAssign:
            break;

        case NodeType::PlusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " += " + translateExpr(node.attrs[0]) + ";");
            }
            break;

        case NodeType::MinusEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " -= " + translateExpr(node.attrs[0]) + ";");
            }
            break;

        case NodeType::MultiplyEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + translateExpr(node.attrs[0]) + ";");
            }
            break;

        case NodeType::DivideEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " /= " + translateExpr(node.attrs[0]) + ";");
            }
            break;

        case NodeType::AtEqualStmt:
            if (!node.attrs.empty()) {
                emit(node.value + " *= " + translateExpr(node.attrs[0]) + ";");
            }
            break;

        case NodeType::MethodCall: {
            std::string args = node.attrs.empty() ? "" : node.attrs[0];
            std::string name = node.value;
            if (name == "Term.display") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "\"" + args.substr(1, args.size() - 2) + "\"";
                }
                emit("println!(\"{}\", " + args + ");");
            } else if (name == "Term.ask") {
                if (args.size() >= 2 && args.front() == '$' && args.back() == '$') {
                    args = "\"" + args.substr(1, args.size() - 2) + "\"";
                }
                emit("let mut input = String::new();");
                emit("std::io::stdin().read_line(&mut input).unwrap();");
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
            emit("(0..=(" + n + ")).collect::<Vec<i32>>()");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("((" + x + ")..=(" + y + ")).collect::<Vec<i32>>()");
            break;
        }

        case NodeType::FuncDef: {
            if (!collectingFunctions) break;
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += node.attrs[i] + ": i32";
            }
            
            currentOut = &functions;
            emitRaw("fn " + node.value + "(" + args + ") -> i32 {");
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
            std::string lst = node.attrs.empty() ? "vec" : translateExpr(node.attrs[0]);
            emit("for " + node.value + " in " + lst + " {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond = translateExpr(node.value);
            emit("while " + cond + " {");
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
            emit(translateExpr(node.value) + ";");
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
                        emit("AC_EVENT_LISTENER.lock().unwrap().bind(\"" + key + "\", Box::new(|| " + callback + "));");
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
            emit("AC_EVENT_LISTENER.lock().unwrap().trigger(\"" + key + "\");");
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
            emit("loop {");
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
            emit("loop {");
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

std::string generateRs(const ASTNode& ast) {
    RsCodeGen gen;
    return gen.generate(ast);
}
