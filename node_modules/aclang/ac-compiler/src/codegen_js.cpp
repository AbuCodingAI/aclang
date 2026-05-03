#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <map>
#include <regex>

class JSCodeGen : public BaseCodeGen {
    std::ostringstream out;
    int indentLevel = 0;
    std::map<std::string, TypePtr> varTypes;

    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    void emitRaw(const std::string& line) { out << line << "\n"; }

    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return "`" + v.substr(1, v.size() - 2) + "`";
        return v;
    }

    std::string unwrapDollars(const std::string& s) {
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for JavaScript
            std::string escaped;
            for (char c : content) {
                if (c == '`') escaped += "\\`";
                else if (c == '\\') escaped += "\\\\";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            return "`" + escaped + "`";
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
                        if (c == '`') escaped += "\\`";
                        else if (c == '\\') escaped += "\\\\";
                        else if (c == '\n') escaped += "\\n";
                        else if (c == '\t') escaped += "\\t";
                        else escaped += c;
                    }
                    result += "`" + escaped + "`";
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

        // Convert AC operators to JavaScript
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;

        // Case-insensitive boolean handling
        std::regex truePattern(R"(\b(true|True|TRUE)\b)", std::regex::icase);
        std::regex falsePattern(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        r = std::regex_replace(r, truePattern, "true");
        r = std::regex_replace(r, falsePattern, "false");

        // Convert 'is' to '=='
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;

        if (r.rfind("is ", 0) == 0) r = r.substr(3);

        return r;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        
        // Null handling: null -> null (JavaScript native)
        std::regex nullPattern(R"(\bnull\b)");
        r = std::regex_replace(r, nullPattern, "null");
        
        // Case-insensitive boolean handling
        std::regex truePattern(R"(\b(true|True|TRUE)\b)", std::regex::icase);
        std::regex falsePattern(R"(\b(false|False|FALSE)\b)", std::regex::icase);
        r = std::regex_replace(r, truePattern, "true");
        r = std::regex_replace(r, falsePattern, "false");
        
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!=="), p += 3;
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
    
    // Parse "key:value,key2:value2" into JavaScript object items
    std::string parseDictItems(const std::string& raw) {
        std::string s = raw;
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$')
            s = s.substr(1, s.size() - 2);
        std::string result;
        size_t i = 0;
        while (i < s.size()) {
            // Parse key
            std::string key;
            while (i < s.size() && s[i] != ':') {
                key += s[i++];
            }
            // Trim key
            size_t a = key.find_first_not_of(' ');
            size_t b = key.find_last_not_of(' ');
            if (a != std::string::npos) key = key.substr(a, b - a + 1);
            
            if (i < s.size() && s[i] == ':') i++; // skip :
            
            // Parse value
            std::string value;
            while (i < s.size() && s[i] != ',') {
                value += s[i++];
            }
            // Trim value
            a = value.find_first_not_of(' ');
            b = value.find_last_not_of(' ');
            if (a != std::string::npos) value = value.substr(a, b - a + 1);
            
            if (i < s.size() && s[i] == ',') i++; // skip ,
            
            // Add to result
            if (!key.empty()) {
                if (!result.empty()) result += ", ";
                // Quote key if not a number
                bool keyIsNum = !key.empty() && (std::isdigit(key[0]) || key[0] == '-');
                bool valIsNum = !value.empty() && (std::isdigit(value[0]) || value[0] == '-');
                result += (keyIsNum ? key : "\"" + key + "\"") + ": " + (valIsNum ? value : "\"" + value + "\"");
            }
        }
        return result;
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

    // Remove the last emitted closing brace line so we can chain else/else-if
    void removeLastBrace() {
        std::string cur = out.str();
        // find last line that is just whitespace + }
        size_t end = cur.rfind("}\n");
        if (end == std::string::npos) return;
        // find start of that line
        size_t start = cur.rfind('\n', end - 1);
        start = (start == std::string::npos) ? 0 : start + 1;
        out.str(cur.substr(0, start));
        out.seekp(0, std::ios::end);
    }

    void genBlock(const ASTNode& node) {
        if (node.children.empty()) return;
        for (auto& c : node.children) genNode(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            emitRaw("// Generated by AC Compiler (AC->JS)");
            emitRaw("\"use strict\";");
            emitRaw("");
            
            // Only emit event listener code if actually used
            bool needsEventListener = usesEventListener(node);
            if (needsEventListener) {
                emitRaw("// Event listener implementation");
                emitRaw("class EventListener {");
                emitRaw("    constructor() { this.bindings = {}; }");
                emitRaw("    bind(key, callback) { this.bindings[key] = callback; }");
                emitRaw("    trigger(key) {");
                emitRaw("        // Automatically trigger if binding exists, otherwise do nothing");
                emitRaw("        if (this.bindings[key]) this.bindings[key]();");
                emitRaw("    }");
                emitRaw("    check_and_trigger(key) {");
                emitRaw("        // Check and trigger - if no binding, do absolutely nothing");
                emitRaw("        this.trigger(key);");
                emitRaw("    }");
                emitRaw("}");
                emitRaw("const ac_event_listener = new EventListener();");
                emitRaw("");
                emitRaw("// Import keybinds");
                emitRaw("const KEY_1 = \"1\";");
                emitRaw("const KEY_2 = \"2\";");
                emitRaw("const KEY_3 = \"3\";");
                emitRaw("const KEY_4 = \"4\";");
                emitRaw("const KEY_5 = \"5\";");
                emitRaw("const KEY_6 = \"6\";");
                emitRaw("const KEY_7 = \"7\";");
                emitRaw("const KEY_8 = \"8\";");
                emitRaw("const KEY_9 = \"9\";");
                emitRaw("const KEY_0 = \"0\";");
                emitRaw("const KEY_MINUS = \"-\";");
                emitRaw("const KEY_EQUAL = \"=\";");
                emitRaw("const KEY_BACKSPACE = \"Backspace\";");
                emitRaw("const KEY_BACKSLASH = \"\\\\\";");
                emitRaw("const KEY_TAB = \"Tab\";");
                emitRaw("const KEY_Q = \"q\";");
                emitRaw("const KEY_W = \"w\";");
                emitRaw("const KEY_E = \"e\";");
                emitRaw("const KEY_R = \"r\";");
                emitRaw("const KEY_T = \"t\";");
                emitRaw("const KEY_Y = \"y\";");
                emitRaw("const KEY_U = \"u\";");
                emitRaw("const KEY_I = \"i\";");
                emitRaw("const KEY_O = \"o\";");
                emitRaw("const KEY_P = \"p\";");
                emitRaw("const KEY_LBRACKET = \"[\";");
                emitRaw("const KEY_RBRACKET = \"]\";");
                emitRaw("const KEY_CAPS = \"CapsLock\";");
                emitRaw("const KEY_A = \"a\";");
                emitRaw("const KEY_S = \"s\";");
                emitRaw("const KEY_D = \"d\";");
                emitRaw("const KEY_F = \"f\";");
                emitRaw("const KEY_G = \"g\";");
                emitRaw("const KEY_H = \"h\";");
                emitRaw("const KEY_J = \"j\";");
                emitRaw("const KEY_K = \"k\";");
                emitRaw("const KEY_L = \"l\";");
                emitRaw("const KEY_SEMICOLON = \";\";");
                emitRaw("const KEY_APOSTROPHE = \"'\";");
                emitRaw("const KEY_ENTER = \"Enter\";");
                emitRaw("const KEY_SHIFT = \"Shift\";");
                emitRaw("const KEY_Z = \"z\";");
                emitRaw("const KEY_X = \"x\";");
                emitRaw("const KEY_C = \"c\";");
                emitRaw("const KEY_V = \"v\";");
                emitRaw("const KEY_B = \"b\";");
                emitRaw("const KEY_N = \"n\";");
                emitRaw("const KEY_M = \"m\";");
                emitRaw("const KEY_COMMA = \",\";");
                emitRaw("const KEY_PERIOD = \".\";");
                emitRaw("const KEY_SLASH = \"/\";");
                emitRaw("const KEY_CTRL = \"Control\";");
                emitRaw("const KEY_ALT = \"Alt\";");
                emitRaw("const KEY_SPACE = \" \";");
                emitRaw("const KEY_FN = \"Meta\";");
                emitRaw("const KEY_SUPER = \"Meta\";");
                emitRaw("const KEY_WINDOWS = \"Meta\";");
                emitRaw("const KEY_UP = \"ArrowUp\";");
                emitRaw("const KEY_DOWN = \"ArrowDown\";");
                emitRaw("const KEY_LEFT = \"ArrowLeft\";");
                emitRaw("const KEY_RIGHT = \"ArrowRight\";");
                emitRaw("");
            }
            
            // Recursively collect and generate ALL function definitions from entire program
            std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef) genNode(n);
                for (auto& c : n.children) collectFuncs(*c);
            };
            for (auto& c : node.children) {
                collectFuncs(*c);
            }
            
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop")
                    mainloopBlock = c.get();
                else if (c->type == NodeType::AssignStmt && !c->attrs.empty())
                    emitRaw("let " + c->value + " = " + quoteIfString(c->attrs[0]) + ";");
            }
            emitRaw("");
            if (mainloopBlock) {
                emitRaw("(function main() {");
                indentLevel++;
                emit("while (true) {");
                indentLevel++;
                for (auto& c : mainloopBlock->children) genNode(*c);
                indentLevel--;
                emit("}");
                indentLevel--;
                emitRaw("})();");
            }
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib camera -> include camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("const camera = require('./library/camera/camera.js');");
            }
            break;
        }
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;

        case NodeType::DisplayStmt: {
            std::string val = node.value;
            if (val.size() >= 2 && val.front() == '$' && val.back() == '$')
                val = "`" + val.substr(1, val.size() - 2) + "`";
            emit("document.body.innerHTML += `<p>${" + val + "}</p>`;");
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
            emit("if (typeof process !== 'undefined') process.exit(0); else throw new Error('__ac_exit__');");
            break;

        case NodeType::RaiseStmt:
            emit("throw new Error(\"Preposterous: " + node.value + "\");");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value) + ";");
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                if (val.substr(0, 8) == "__list__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeList());
                    emit("let " + node.value + " = [" + parseCollectionItems(val.substr(8)) + "];");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeTuple());
                    emit("const " + node.value + " = Object.freeze([" + parseCollectionItems(val.substr(9)) + "]);");
                } else if (val.substr(0, 8) == "__dict__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeUnknown());
                    emit("let " + node.value + " = {" + parseDictItems(val.substr(8)) + "};  // dict");
                } else if (val.substr(0, 6) == "__fn__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                    emit("let " + node.value + " = " + translateExpr(val.substr(6)) + ";");
                } else if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
                    // Function call: name = func(arg1, arg2) or func(key=val)
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
                                // Keyword argument - JavaScript doesn't support these directly
                                // Just use the value part
                                std::string val = attr.substr(eqPos + 1);
                                while (!val.empty() && val.back() == ' ') val.pop_back();
                                while (!val.empty() && val.front() == ' ') val = val.substr(1);
                                args += quoteIfString(val);
                            } else {
                                // Positional argument
                                args += quoteIfString(attr);
                            }
                        }
                        emit("let " + node.value + " = " + funcName + "(" + args + ");");
                    }
                } else {
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool hasAt = val.find('@') != std::string::npos;
                    if (isNum || hasAt) {
                        auto t = Type::inferNumeral(val);
                        varTypes[node.value] = std::make_shared<Type>(t);
                        emit("let " + node.value + " = " + translateExpr(val) + "; // " + t.toString());
                    } else {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                        emit("let " + node.value + " = " + quoteIfString(val) + ";");
                    }
                }
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
            if (args.size() >= 2 && args.front() == '$' && args.back() == '$')
                args = "`" + args.substr(1, args.size() - 2) + "`";
            else
                args = translateExpr(args);
            std::string name = node.value;
            if (name == "Term.display") name = "console.log";
            else if (name == "Term.ask") name = "prompt";

            // display variants -> DOM injection
            if (name == "display" || name == "bold.display" || name == "italic.display" ||
                name == "title.display" || name == "header.display" || name == "link.display") {
                std::string content = args;
                std::string tag = "p";
                if (name == "bold.display")   tag = "b";
                if (name == "italic.display") tag = "i";
                if (name == "title.display")  tag = "title";
                if (name == "link.display")   tag = "a";
                if (name == "header.display") {
                    std::string level = "1";
                    if (node.attrs.size() > 1)
                        for (char c : node.attrs[1]) if (std::isdigit(c)) { level = std::string(1,c); break; }
                    tag = "h" + level;
                }
                emit("document.body.innerHTML += `<" + tag + ">${" + content + "}</" + tag + ">`;");
                break;
            }

            emit(name + "(" + args + ");");
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
            emit("(()=>{ if((" + n + ") < 0) throw new Error('Preposterous: range requires positive integer'); "
                 "let _r = []; for(let _i=0;_i<=(" + n + ");_i++) _r.push(_i); return _r; })()");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("(()=>{ if((" + x + ")>(" + y + ")) throw new Error('Preposterous: sequence start > end'); "
                 "let _r = []; for(let _i=(" + x + ");_i<=(" + y + ");_i++) _r.push(_i); return _r; })()");
            break;
        }

        case NodeType::FuncDef: {
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += node.attrs[i];
            }
            emit("function " + node.value + "(" + args + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
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
            indentLevel--;
            emit("}");
            
            // Handle ELSEIF and OTHER blocks (they are additional children)
            for (size_t i = 1; i < node.children.size(); i++) {
                genNode(*node.children[i]);
            }
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond = translateCondition(node.value);
            emit("else if (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "[]" : translateExpr(node.attrs[0]);
            emit("for (let i = 0; i < " + lst + ".length; ++i) {");
            indentLevel++;
            emit("let " + node.value + " = " + lst + "[i];");
            if (!node.children.empty()) genBlock(*node.children[0]);
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond = translateCondition(node.value);
            emit("while (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
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
                emit("/* evaluate index */ " + node.value + "[" + idx + "]; ");
            }
            break;
        }

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
            emit("function spawn_" + node.value + "() {");
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
                        emit("ac_event_listener.bind('" + key + "', () => " + callback + ");");
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
            emit("ac_event_listener.trigger('" + key + "');");
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
            emit("function main() {");
            indentLevel++;

            bool hasStartHere = false;
            for (auto& c : node.children)
                if (c->type == NodeType::TagBlock && c->value == "StartHere")
                    { hasStartHere = true; break; }

            if (hasStartHere) {
                for (auto& c : node.children) {
                    if (c->type == NodeType::TagBlock && c->value == "StartHere") break;
                    genNode(*c);
                }
                emit("while (true) {");
                indentLevel++;
                for (auto& c : node.children)
                    if (c->type == NodeType::TagBlock && c->value == "StartHere")
                        { for (auto& inner : c->children) genNode(*inner); break; }
                indentLevel--;
                emit("}");
            } else {
                emit("while (true) {");
                indentLevel++;
                for (auto& c : node.children) genNode(*c);
                indentLevel--;
                emit("}");
            }

            indentLevel--;
            emit("}");
            emitRaw("");
            emit("main();");
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else if (name == "LOGIC" || name == "Local") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "StartHere") {
            emit("while (true) {");
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

std::string generateJS(const ASTNode& ast) {
    JSCodeGen gen;
    return gen.generate(ast);
}
