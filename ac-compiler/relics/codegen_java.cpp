#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>
#include <map>

class JavaCodeGen : public BaseCodeGen {
    std::ostringstream out;
    std::ostringstream functions;
    int indentLevel = 0;
    bool collectingFuncs = false;
    std::map<std::string, TypePtr> varTypes;

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
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for Java
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
                result += (keyIsNum ? key : "\"" + key + "\"") + ", " + (valIsNum ? value : "\"" + value + "\"");
            }
        }
        return result;
    }

    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);
        // #= -> !=
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;)
            r.replace(p, 2, "!="), p += 2;
        // is -> ==
        for (size_t p = 0; (p = r.find(" is ", p)) != std::string::npos;)
            r.replace(p, 4, " == "), p += 4;
        if (r.substr(0, 3) == "is ") r.replace(0, 3, "");
        return r;
    }

    std::string translateExpr(const std::string& expr) {
        std::string r = expr;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        while (!r.empty() && r.front() == ' ') r = r.substr(1);
        r = unwrapDollars(r);
        
        // Translate AC booleans and null to Java (case-insensitive)
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
                    result += "null";
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
        for (size_t p = 0; (p = r.find("#=", p)) != std::string::npos;) {
            r.replace(p, 2, "!="); p += 2;
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
            emitRaw("// Generated by AC Compiler (AC->Java)");
            emitRaw("import java.util.*;");
            emitRaw("import java.util.function.*;");
            emitRaw("");
            emitRaw("// Event listener implementation");
            emitRaw("class EventListener {");
            emitRaw("    private Map<String, Runnable> bindings = new HashMap<>();");
            emitRaw("    public void bind(String key, Runnable callback) { bindings.put(key, callback); }");
            emitRaw("    public void trigger(String key) {");
            emitRaw("        // Automatically trigger if binding exists, otherwise do nothing");
            emitRaw("        if (bindings.containsKey(key)) bindings.get(key).run();");
            emitRaw("    }");
            emitRaw("    public void checkAndTrigger(String key) {");
            emitRaw("        // Check and trigger - if no binding, do absolutely nothing");
            emitRaw("        trigger(key);");
            emitRaw("    }");
            emitRaw("}");
            emitRaw("");
            emitRaw("// Import keybinds");
            emitRaw("class ACKeybinds {");
            emitRaw("    static final int KEY_1 = java.awt.event.KeyEvent.VK_1;");
            emitRaw("    static final int KEY_2 = java.awt.event.KeyEvent.VK_2;");
            emitRaw("    static final int KEY_3 = java.awt.event.KeyEvent.VK_3;");
            emitRaw("    static final int KEY_4 = java.awt.event.KeyEvent.VK_4;");
            emitRaw("    static final int KEY_5 = java.awt.event.KeyEvent.VK_5;");
            emitRaw("    static final int KEY_6 = java.awt.event.KeyEvent.VK_6;");
            emitRaw("    static final int KEY_7 = java.awt.event.KeyEvent.VK_7;");
            emitRaw("    static final int KEY_8 = java.awt.event.KeyEvent.VK_8;");
            emitRaw("    static final int KEY_9 = java.awt.event.KeyEvent.VK_9;");
            emitRaw("    static final int KEY_0 = java.awt.event.KeyEvent.VK_0;");
            emitRaw("    static final int KEY_MINUS = java.awt.event.KeyEvent.VK_MINUS;");
            emitRaw("    static final int KEY_EQUAL = java.awt.event.KeyEvent.VK_EQUALS;");
            emitRaw("    static final int KEY_BACKSPACE = java.awt.event.KeyEvent.VK_BACK_SPACE;");
            emitRaw("    static final int KEY_BACKSLASH = java.awt.event.KeyEvent.VK_BACK_SLASH;");
            emitRaw("    static final int KEY_TAB = java.awt.event.KeyEvent.VK_TAB;");
            emitRaw("    static final int KEY_Q = java.awt.event.KeyEvent.VK_Q;");
            emitRaw("    static final int KEY_W = java.awt.event.KeyEvent.VK_W;");
            emitRaw("    static final int KEY_E = java.awt.event.KeyEvent.VK_E;");
            emitRaw("    static final int KEY_R = java.awt.event.KeyEvent.VK_R;");
            emitRaw("    static final int KEY_T = java.awt.event.KeyEvent.VK_T;");
            emitRaw("    static final int KEY_Y = java.awt.event.KeyEvent.VK_Y;");
            emitRaw("    static final int KEY_U = java.awt.event.KeyEvent.VK_U;");
            emitRaw("    static final int KEY_I = java.awt.event.KeyEvent.VK_I;");
            emitRaw("    static final int KEY_O = java.awt.event.KeyEvent.VK_O;");
            emitRaw("    static final int KEY_P = java.awt.event.KeyEvent.VK_P;");
            emitRaw("    static final int KEY_LBRACKET = java.awt.event.KeyEvent.VK_OPEN_BRACKET;");
            emitRaw("    static final int KEY_RBRACKET = java.awt.event.KeyEvent.VK_CLOSE_BRACKET;");
            emitRaw("    static final int KEY_CAPS = java.awt.event.KeyEvent.VK_CAPS_LOCK;");
            emitRaw("    static final int KEY_A = java.awt.event.KeyEvent.VK_A;");
            emitRaw("    static final int KEY_S = java.awt.event.KeyEvent.VK_S;");
            emitRaw("    static final int KEY_D = java.awt.event.KeyEvent.VK_D;");
            emitRaw("    static final int KEY_F = java.awt.event.KeyEvent.VK_F;");
            emitRaw("    static final int KEY_G = java.awt.event.KeyEvent.VK_G;");
            emitRaw("    static final int KEY_H = java.awt.event.KeyEvent.VK_H;");
            emitRaw("    static final int KEY_J = java.awt.event.KeyEvent.VK_J;");
            emitRaw("    static final int KEY_K = java.awt.event.KeyEvent.VK_K;");
            emitRaw("    static final int KEY_L = java.awt.event.KeyEvent.VK_L;");
            emitRaw("    static final int KEY_SEMICOLON = java.awt.event.KeyEvent.VK_SEMICOLON;");
            emitRaw("    static final int KEY_APOSTROPHE = java.awt.event.KeyEvent.VK_QUOTE;");
            emitRaw("    static final int KEY_ENTER = java.awt.event.KeyEvent.VK_ENTER;");
            emitRaw("    static final int KEY_SHIFT = java.awt.event.KeyEvent.VK_SHIFT;");
            emitRaw("    static final int KEY_Z = java.awt.event.KeyEvent.VK_Z;");
            emitRaw("    static final int KEY_X = java.awt.event.KeyEvent.VK_X;");
            emitRaw("    static final int KEY_C = java.awt.event.KeyEvent.VK_C;");
            emitRaw("    static final int KEY_V = java.awt.event.KeyEvent.VK_V;");
            emitRaw("    static final int KEY_B = java.awt.event.KeyEvent.VK_B;");
            emitRaw("    static final int KEY_N = java.awt.event.KeyEvent.VK_N;");
            emitRaw("    static final int KEY_M = java.awt.event.KeyEvent.VK_M;");
            emitRaw("    static final int KEY_COMMA = java.awt.event.KeyEvent.VK_COMMA;");
            emitRaw("    static final int KEY_PERIOD = java.awt.event.KeyEvent.VK_PERIOD;");
            emitRaw("    static final int KEY_SLASH = java.awt.event.KeyEvent.VK_SLASH;");
            emitRaw("    static final int KEY_CTRL = java.awt.event.KeyEvent.VK_CONTROL;");
            emitRaw("    static final int KEY_ALT = java.awt.event.KeyEvent.VK_ALT;");
            emitRaw("    static final int KEY_SPACE = java.awt.event.KeyEvent.VK_SPACE;");
            emitRaw("    static final int KEY_FN = java.awt.event.KeyEvent.VK_META;");
            emitRaw("    static final int KEY_SUPER = java.awt.event.KeyEvent.VK_META;");
            emitRaw("    static final int KEY_WINDOWS = java.awt.event.KeyEvent.VK_META;");
            emitRaw("    static final int KEY_UP = java.awt.event.KeyEvent.VK_UP;");
            emitRaw("    static final int KEY_DOWN = java.awt.event.KeyEvent.VK_DOWN;");
            emitRaw("    static final int KEY_LEFT = java.awt.event.KeyEvent.VK_LEFT;");
            emitRaw("    static final int KEY_RIGHT = java.awt.event.KeyEvent.VK_RIGHT;");
            emitRaw("}");
            emitRaw("");
            
            // Find mainloop tag block
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop") {
                    mainloopBlock = c.get();
                    break;
                }
            }
            
            // Collect functions from entire program
            {
                std::function<void(const ASTNode&)> collectFuncs = [&](const ASTNode& n) {
                    if (n.type == NodeType::FuncDef) genNode(n);
                    for (auto& c : n.children) collectFuncs(*c);
                };
                for (auto& c : node.children) {
                    collectFuncs(*c);
                }
            }
            
            emitRaw("public class Main {");
            indentLevel++;
            emitRaw("    static EventListener acEventListener = new EventListener();");
            emitRaw("    public static void main(String[] args) {");
            indentLevel++;
            if (mainloopBlock) {
                for (auto& c : mainloopBlock->children) genNode(*c);
            }
            indentLevel--;
            emit("}");
            indentLevel--;
            emitRaw(functions.str());
            emit("}");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib camera -> import camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("// Camera library not yet implemented for Java");
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
            emit("System.out.println(" + val + ");");
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
            emit("System.exit(0);");
            break;

        case NodeType::RaiseStmt:
            emit("throw new RuntimeException(\"Preposterous: " + node.value + "\");");
            break;

        case NodeType::ReturnStmt:
            emit("return " + translateExpr(node.value) + ";");
            break;

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                if (val.substr(0, 8) == "__list__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeList());
                    emit("List<Object> " + node.value + " = new ArrayList<>(Arrays.asList(" + parseCollectionItems(val.substr(8)) + "));");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeTuple());
                    emit("final Object[] " + node.value + " = {" + parseCollectionItems(val.substr(9)) + "};");
                } else if (val.substr(0, 8) == "__dict__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeUnknown());
                    emit("Map<Object, Object> " + node.value + " = Map.of(" + parseDictItems(val.substr(8)) + ");  // dict");
                } else if (val.substr(0, 6) == "__fn__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
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
                        emit("Object " + node.value + " = " + funcName + "(" + args + ");");
                    }
                } else {
                    // Check if it's a keyword (null, true, false)
                    std::string valLower = val;
                    for (auto& c : valLower) c = std::tolower(c);
                    bool isKeyword = (valLower == "null" || valLower == "true" || valLower == "false");
                    
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool hasAt = val.find('@') != std::string::npos;
                    bool isFuncCall = val.find('(') != std::string::npos;
                    if (isNum || isFuncCall || hasAt || isKeyword) {
                        auto t = Type::inferNumeral(val);
                        varTypes[node.value] = std::make_shared<Type>(t);
                        emit(t.toJava() + " " + node.value + " = " + translateExpr(val) + ";");
                    } else {
                        varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                        emit("String " + node.value + " = " + quoteIfString(val) + ";");
                    }
                }
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
            if (name == "Term.display") name = "System.out.println";
            else if (name == "Term.ask") {
                emit("java.util.Scanner scanner = new java.util.Scanner(System.in);");
                emit("String input = scanner.nextLine();");
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

        case NodeType::FuncDef: {
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += "Object " + node.attrs[i];
            }
            functions << "    public static Object " << node.value << "(" << args << ") {\n";
            
            if (!node.children.empty() && !node.children[0]->children.empty()) {
                auto& block = node.children[0]->children;
                for (auto& c : block) {
                    if (c->type == NodeType::ReturnStmt) {
                        functions << "        return " << translateExpr(c->value) << ";\n";
                    } else if (c->type == NodeType::AssignStmt && !c->attrs.empty()) {
                        std::string val = c->attrs[0];
                        if (val.substr(0, 6) == "__fn__") {
                            functions << "        return " << translateExpr(val.substr(6)) << ";\n";
                        }
                    }
                }
            }
            functions << "    }\n";
            break;
        }

        case NodeType::IfStmt: {
            if (node.value == "OTHER") {
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
                std::string cond = translateCondition(node.value);
                emit("if (" + cond + ") {");
            }
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
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
            std::string cur = out.str();
            size_t end = cur.rfind("}\n");
            if (end != std::string::npos) {
                size_t start = cur.rfind('\n', end - 1);
                start = (start == std::string::npos) ? 0 : start + 1;
                out.str(cur.substr(0, start));
                out.seekp(0, std::ios::end);
            }
            emit("} else if (" + cond + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
            break;
        }

        case NodeType::ForLoop: {
            std::string lst = node.attrs.empty() ? "new ArrayList<>()" : translateExpr(node.attrs[0]);
            emit("for (Object " + node.value + " : " + lst + ") {");
            indentLevel++;
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("// empty");
            indentLevel--;
            emit("}");
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
            emit("Arrays.asList(" + parseCollectionItems(node.value) + ")");
            break;

        case NodeType::TupleLiteral:
            emit("new Object[]{" + parseCollectionItems(node.value) + "}");
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
            emit("public static void spawn_" + node.value + "() {");
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
                        emit("acEventListener.bind(\"" + key + "\", () -> " + callback + ");");
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
            emit("acEventListener.trigger(\"" + key + "\");");
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
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else if (name == "LOGIC") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "Local") {
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

std::string generateJava(const ASTNode& ast) {
    JavaCodeGen gen;
    return gen.generate(ast);
}
