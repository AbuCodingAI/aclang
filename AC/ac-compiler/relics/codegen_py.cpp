#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <set>
#include <functional>
#include <map>

class PythonCodeGen : public BaseCodeGen {
    std::ostringstream out;
    int indentLevel = 0;
    std::map<std::string, TypePtr> varTypes;
    bool inEventListener = false;

    void emit(const std::string& line) {
        out << std::string(indentLevel * 4, ' ') << line << "\n";
    }
    void emitRaw(const std::string& line) { out << line << "\n"; }

    // $...$ marker -> "..."
    std::string quoteIfString(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$') {
            // Escape special characters for Python
            std::string content = v.substr(1, v.size() - 2);
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
        return v;
    }

    // Unwrap any $...$ inside an expression to "..."
    std::string unwrapDollars(const std::string& s) {
        // If the entire string is $...$, unwrap it completely
        if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
            std::string content = s.substr(1, s.size() - 2);
            // Escape special characters for Python
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

    // Translate AC condition to Python
    std::string translateCondition(const std::string& cond) {
        std::string r = cond;
        while (!r.empty() && r.back() == ' ') r.pop_back();
        r = unwrapDollars(r);
        
        // Translate AC booleans to Python (case-insensitive) - replace all occurrences
        std::string result;
        for (size_t i = 0; i < r.size(); ) {
            // Check for word boundaries and boolean keywords
            if ((i == 0 || !std::isalnum(r[i-1])) && i + 4 <= r.size()) {
                std::string word = r.substr(i, 4);
                std::string lower = word;
                for (auto& c : lower) c = std::tolower(c);
                if (lower == "true" && (i + 4 >= r.size() || !std::isalnum(r[i+4]))) {
                    result += "True";
                    i += 4;
                    continue;
                }
            }
            if ((i == 0 || !std::isalnum(r[i-1])) && i + 5 <= r.size()) {
                std::string word = r.substr(i, 5);
                std::string lower = word;
                for (auto& c : lower) c = std::tolower(c);
                if (lower == "false" && (i + 5 >= r.size() || !std::isalnum(r[i+5]))) {
                    result += "False";
                    i += 5;
                    continue;
                }
            }
            result += r[i];
            i++;
        }
        r = result;
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
        // Translate AC booleans to Python
        if (r == "true") r = "True";
        if (r == "false") r = "False";
        // @ -> * (default multiplication operator)
        for (size_t p = 0; (p = r.find("@", p)) != std::string::npos;) {
            r.replace(p, 1, "*"); p++;
        }
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
    
    // Parse "key:value,key2:value2" into Python dict items
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

    bool isPythonBuiltin(const std::string& name) {
        static const std::vector<std::string> b = {
            "print","input","len","range","int","str","float","list","dict",
            "set","type","isinstance","open","exit","abs","max","min","sum"
        };
        for (auto& x : b) if (x == name) return true;
        return false;
    }
    
    // Generate Python code from structured expression AST nodes
    std::string generateExpressionCode(const ASTNode& expr) {
        switch (expr.type) {
            case NodeType::LiteralExpr: {
                // Literal values: int, float, string, bool
                if (!expr.attrs.empty()) {
                    const std::string& typeStr = expr.attrs[0];
                    if (typeStr == "INT" || typeStr == "FLOAT") {
                        return expr.value;
                    } else if (typeStr == "STRING") {
                        // Escape and quote the string
                        std::string escaped;
                        for (char c : expr.value) {
                            if (c == '"') escaped += "\\\"";
                            else if (c == '\\') escaped += "\\\\";
                            else if (c == '\n') escaped += "\\n";
                            else if (c == '\t') escaped += "\\t";
                            else escaped += c;
                        }
                        return "\"" + escaped + "\"";
                    } else if (typeStr == "BOOL") {
                        return (expr.value == "true") ? "True" : "False";
                    }
                }
                return expr.value;
            }
            
            case NodeType::Identifier: {
                // Variable reference
                return expr.value;
            }
            
            case NodeType::BinaryExpr: {
                // Binary operation: left op right
                if (expr.children.size() >= 2) {
                    std::string left = generateExpressionCode(*expr.children[0]);
                    std::string right = generateExpressionCode(*expr.children[1]);
                    std::string op = expr.value;
                    
                    // Translate AC operators to Python
                    if (op == "@") op = "*";  // @ is multiplication in AC
                    else if (op == "#=") op = "!=";  // #= is not-equal in AC
                    else if (op == "is") op = "==";  // is is equality in AC (not identity)
                    else if (op == "AND" || op == "and") op = "and";
                    else if (op == "OR" || op == "or") op = "or";
                    
                    return left + " " + op + " " + right;
                }
                // Fallback: treat as string expression (legacy compatibility)
                return translateExpr(expr.value);
            }
            
            case NodeType::UnaryExpr: {
                // Unary operation: op operand
                if (!expr.children.empty()) {
                    std::string operand = generateExpressionCode(*expr.children[0]);
                    if (expr.value == "-") {
                        return "-" + operand;
                    } else if (expr.value == "NOT") {
                        return "not " + operand;
                    }
                }
                return "";
            }
            
            case NodeType::CallExpr: {
                // Function call: func(args)
                std::string funcName = expr.value;
                std::string args;
                for (size_t i = 0; i < expr.children.size(); i++) {
                    if (i > 0) args += ", ";
                    args += generateExpressionCode(*expr.children[i]);
                }
                return funcName + "(" + args + ")";
            }
            
            case NodeType::IndexExpr: {
                // Array indexing: array[index]
                if (expr.children.size() >= 2) {
                    std::string arr = generateExpressionCode(*expr.children[0]);
                    std::string idx = generateExpressionCode(*expr.children[1]);
                    // AC uses 1-based indexing, Python uses 0-based
                    // If index is a constant integer, subtract 1
                    if (expr.children[1]->type == NodeType::LiteralExpr && 
                        !expr.children[1]->attrs.empty() && 
                        expr.children[1]->attrs[0] == "INT") {
                        try {
                            int idxVal = std::stoi(idx);
                            return arr + "[" + std::to_string(idxVal - 1) + "]";
                        } catch (...) {
                            // Not a constant, use runtime conversion
                            return arr + "[" + idx + " - 1]";
                        }
                    }
                    return arr + "[" + idx + " - 1]";
                }
                // Fallback: use variable name
                return expr.value;
            }
            
            default:
                // Fallback: treat as variable or constant
                if (!expr.value.empty()) {
                    return expr.value;
                }
                return "";
        }
    }

    void genBlock(const ASTNode& node) {
        if (node.children.empty()) { return; }
        for (auto& c : node.children) genNode(*c);
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

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            emitRaw("# Generated by AC Compiler (AC->PY)");
            emitRaw("import sys");
            emitRaw("");
            
            // Only emit event listener code if actually used
            bool needsEventListener = usesEventListener(node);
            if (needsEventListener) {
                emitRaw("sys.path.insert(0, 'library/keybinds')");
                emitRaw("from keybinds import *");
                emitRaw("");
                emitRaw("# Event listener implementation");
                emitRaw("class EventListener:");
                emitRaw("    def __init__(self):");
                emitRaw("        self.bindings = {}");
                emitRaw("    def bind(self, key, callback):");
                emitRaw("        self.bindings[key] = callback");
                emitRaw("    def trigger(self, key):");
                emitRaw("        if key in self.bindings:");
                emitRaw("            self.bindings[key]()");
                emitRaw("");
                emitRaw("ac_event_listener = EventListener()");
            }
            
            // Check if libraries are used and import them
            for (auto& c : node.children) {
                if (c->type == NodeType::UseLibStmt) {
                    std::string libName = c->value;
                    // Extract libname from "ilib:libname" format
                    size_t colonPos = libName.find(':');
                    if (colonPos != std::string::npos) {
                        libName = libName.substr(colonPos + 1);
                    }
                    emitRaw("sys.path.insert(0, 'library/" + libName + "')");
                    emitRaw("from " + libName + " import *");
                }
            }
            
            // Check if widgets is used for special handling
            bool usesWidgets = false;
            for (auto& c : node.children) {
                if (c->type == NodeType::UseLibStmt && c->value == "widgets") {
                    usesWidgets = true;
                }
            }
            emitRaw("");
            
            // Collect all variables that functions modify (need to be global)
            std::set<std::string> globalVars;
            std::function<void(const ASTNode&)> collectGlobals = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef) {
                    // Check what variables this function modifies
                    if (!n.children.empty() && n.children[0]->type == NodeType::Block) {
                        for (auto& stmt : n.children[0]->children) {
                            if (stmt->type == NodeType::PlusEqualStmt || stmt->type == NodeType::MinusEqualStmt ||
                                stmt->type == NodeType::MultiplyEqualStmt || stmt->type == NodeType::AtEqualStmt ||
                                stmt->type == NodeType::DivideEqualStmt || stmt->type == NodeType::AssignStmt) {
                                // Check if this variable is NOT a parameter
                                bool isParam = false;
                                for (auto& param : n.attrs) {
                                    if (param == stmt->value) {
                                        isParam = true;
                                        break;
                                    }
                                }
                                if (!isParam) {
                                    globalVars.insert(stmt->value);
                                }
                            }
                        }
                    }
                }
                for (auto& c : n.children) collectGlobals(*c);
            };
            for (auto& c : node.children) collectGlobals(*c);
            
            // Generate global variable declarations
            for (auto& varName : globalVars) {
                emit(varName + " = None");
            }
            if (!globalVars.empty()) emitRaw("");
            
            // Recursively collect and generate ALL function definitions from entire program
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
            
            // Find mainloop tag block
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop") {
                    mainloopBlock = c.get();
                    break;
                }
            }
            
            // If no mainloop, just generate empty main
            if (!mainloopBlock) {
                emitRaw("def main():");
                indentLevel++;
                emit("pass");
                indentLevel--;
                emitRaw("");
                emitRaw("if __name__ == \"__main__\":");
                emitRaw("    main()");
                break;
            }
            
            emitRaw("def main():");
            indentLevel++;
            
            // First, emit ALL function definitions from entire program
            std::function<void(const ASTNode&)> emitFuncs = [&](const ASTNode& n) {
                if (n.type == NodeType::FuncDef) {
                    genNode(n);
                }
                for (auto& c : n.children) {
                    emitFuncs(*c);
                }
            };
            
            for (auto& c : node.children) {
                emitFuncs(*c);
            }
            
            // Then emit mainloop body
            for (auto& c : mainloopBlock->children) {
                // If widgets is used and we hit /kill, add root.mainloop before it
                if (usesWidgets && c->type == NodeType::KillStmt) {
                    emit("root.mainloop()");
                }
                genNode(*c);
            }
            indentLevel--;
            emitRaw("");
            emitRaw("if __name__ == \"__main__\":");
            emitRaw("    main()");
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib <libname> -> import from library.<libname>
            // Extract libname from "ilib:libname" format
            std::string libName = node.value;
            size_t colonPos = libName.find(':');
            if (colonPos != std::string::npos) {
                libName = libName.substr(colonPos + 1);
            }
            emitRaw("import sys");
            emitRaw("sys.path.insert(0, 'library/" + libName + "')");
            emitRaw("from " + libName + " import *");
            break;
        }
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

        case NodeType::AssignStmt: {
            // Check if we have a structured expression as child (new Pratt parser)
            if (!node.children.empty()) {
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " = " + rhsCode);
                break;
            }
            
            // Legacy string-based handling (fallback)
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                if (val.substr(0, 8) == "__list__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeList());
                    emit(node.value + " = [" + parseCollectionItems(val.substr(8)) + "]");
                } else if (val.substr(0, 9) == "__tuple__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeTuple());
                    emit(node.value + " = (" + parseCollectionItems(val.substr(9)) + ")  # tuple: immutable");
                } else if (val.substr(0, 8) == "__dict__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeUnknown());
                    emit(node.value + " = {" + parseDictItems(val.substr(8)) + "}  # dict");
                } else if (val.substr(0, 6) == "__fn__") {
                    varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                    emit(node.value + " = " + translateExpr(val.substr(6)));
                } else if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
                    // Function call: name = func(key=val, key=val)
                    if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
                        auto& funcCall = *node.children[0];
                        std::string funcName = funcCall.value;
                        std::string args;
                        for (size_t i = 0; i < funcCall.attrs.size(); i++) {
                            if (i > 0) args += ", ";
                            // Parse key=value format
                            std::string kwarg = funcCall.attrs[i];
                            size_t eqPos = kwarg.find('=');
                            if (eqPos != std::string::npos) {
                                std::string key = kwarg.substr(0, eqPos);
                                std::string val = kwarg.substr(eqPos + 1);
                                // Trim whitespace
                                while (!val.empty() && val.back() == ' ') val.pop_back();
                                while (!val.empty() && val.front() == ' ') val = val.substr(1);
                                // Translate null to None
                                if (val == "null") val = "None";
                                // Don't quote if it's a variable name (no $ markers)
                                if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                                    // It's a string literal
                                    args += key + "=" + quoteIfString(val);
                                } else {
                                    // It's a variable or expression
                                    args += key + "=" + val;
                                }
                            } else {
                                args += quoteIfString(kwarg);
                            }
                        }
                        emit(node.value + " = " + funcName + "(" + args + ")");
                    } else {
                        // Fallback: just output the function name
                        std::string funcName = val.substr(11);
                        emit(node.value + " = " + funcName + "()");
                    }
                } else {
                    bool isNum = !val.empty() && (std::isdigit(val[0]) || (val[0] == '-' && val.size() > 1));
                    bool hasAt = val.find('@') != std::string::npos;
                    
                    if (isNum && !hasAt) {
                        auto t = Type::inferNumeral(val);
                        varTypes[node.value] = std::make_shared<Type>(t);
                        // Python is dynamically typed — just emit the value, add type comment
                        emit(node.value + " = " + val + "  # " + t.toString());
                    } else if (hasAt || val.find('*') != std::string::npos || val.find('+') != std::string::npos || val.find('-') != std::string::npos) {
                        // It's an expression with operators - translate it
                        varTypes[node.value] = std::make_shared<Type>(Type::makeNumeral(NumeralSubtype::PosInt));
                        emit(node.value + " = " + translateExpr(val) + "  # Numeral(Pos.Int)");
                    } else {
                        // Check if boolean (case-insensitive)
                        std::string lower = val;
                        for (auto& c : lower) c = std::tolower(c);
                        if (lower == "true" || lower == "false") {
                            varTypes[node.value] = std::make_shared<Type>(Type::makeBoolean());
                            std::string boolVal = (lower == "true") ? "True" : "False";
                            emit(node.value + " = " + boolVal);
                        } else if (lower == "null") {
                            // Null handling
                            emit(node.value + " = None");
                        } else {
                            varTypes[node.value] = std::make_shared<Type>(Type::makeString());
                            // Check what val looks like
                            if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                                // Has $ markers - use quoteIfString
                                emit(node.value + " = " + quoteIfString(val));
                            } else {
                                // No $ markers - just use as-is (might be a variable)
                                emit(node.value + " = " + val);
                            }
                        }
                    }
                }
            }
            break;
        }

        case NodeType::PlusEqualStmt:
            if (!node.children.empty()) {
                // Structured expression as child
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " += " + rhsCode);
            } else if (!node.attrs.empty()) {
                // Legacy string-based (fallback)
                emit(node.value + " += " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::MinusEqualStmt:
            if (!node.children.empty()) {
                // Structured expression as child
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " -= " + rhsCode);
            } else if (!node.attrs.empty()) {
                // Legacy string-based (fallback)
                emit(node.value + " -= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::MultiplyEqualStmt:
            if (!node.children.empty()) {
                // Structured expression as child
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " *= " + rhsCode);
            } else if (!node.attrs.empty()) {
                // Legacy string-based (fallback)
                emit(node.value + " *= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::DivideEqualStmt:
            if (!node.children.empty()) {
                // Structured expression as child
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " /= " + rhsCode);
            } else if (!node.attrs.empty()) {
                // Legacy string-based (fallback)
                emit(node.value + " /= " + quoteIfString(node.attrs[0]));
            }
            break;

        case NodeType::AtEqualStmt:
            if (!node.children.empty()) {
                // Structured expression as child
                std::string rhsCode = generateExpressionCode(*node.children[0]);
                emit(node.value + " *= " + rhsCode);  // @ is multiplication
            } else if (!node.attrs.empty()) {
                // Legacy string-based (fallback)
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
            else {
                // Check if it contains keyword arguments (key=value format)
                if (args.find('=') != std::string::npos) {
                    // Parse keyword arguments
                    std::string result;
                    size_t pos = 0;
                    bool first = true;
                    
                    while (pos < args.size()) {
                        // Find next key=value pair
                        size_t eqPos = args.find('=', pos);
                        if (eqPos == std::string::npos) break;
                        
                        // Extract key
                        size_t keyStart = pos;
                        while (keyStart < eqPos && args[keyStart] == ' ') keyStart++;
                        size_t keyEnd = eqPos;
                        while (keyEnd > keyStart && args[keyEnd-1] == ' ') keyEnd--;
                        std::string key = args.substr(keyStart, keyEnd - keyStart);
                        
                        // Extract value (until comma or end)
                        size_t valStart = eqPos + 1;
                        while (valStart < args.size() && args[valStart] == ' ') valStart++;
                        size_t valEnd = valStart;
                        int parenDepth = 0;
                        while (valEnd < args.size()) {
                            if (args[valEnd] == '(') parenDepth++;
                            else if (args[valEnd] == ')') parenDepth--;
                            else if (args[valEnd] == ',' && parenDepth == 0) break;
                            valEnd++;
                        }
                        while (valEnd > valStart && args[valEnd-1] == ' ') valEnd--;
                        std::string val = args.substr(valStart, valEnd - valStart);
                        
                        // Add to result
                        if (!first) result += ", ";
                        first = false;
                        
                        // Translate null to None
                        if (val == "null") val = "None";
                        
                        // Don't quote if it's a variable name (no $ markers)
                        if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                            // It's a string literal
                            result += key + "=" + quoteIfString(val);
                        } else {
                            // It's a variable or expression
                            result += key + "=" + val;
                        }
                        
                        pos = valEnd;
                        if (pos < args.size() && args[pos] == ',') pos++;
                    }
                    args = result;
                } else {
                    args = translateExpr(args);
                }
            }
            std::string name = node.value;
            if (name == "Term.display") name = "print";
            else if (name == "Term.ask") name = "input";
            emit(name + "(" + args + ")");
            break;
        }

        case NodeType::MethodChain: {
            // Process chained method calls
            for (auto& child : node.children) {
                genNode(*child);
            }
            break;
        }

        case NodeType::FunctionCall: {
            // Function call: func(key=val, key=val) or func(arg1, arg2, arg3)
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                std::string attr = node.attrs[i];
                // Check if it's a keyword argument (key=value format)
                size_t eqPos = attr.find('=');
                if (eqPos != std::string::npos && eqPos > 0) {
                    // Keyword argument
                    std::string key = attr.substr(0, eqPos);
                    std::string val = attr.substr(eqPos + 1);
                    // Trim whitespace
                    while (!val.empty() && val.back() == ' ') val.pop_back();
                    while (!val.empty() && val.front() == ' ') val = val.substr(1);
                    // Translate null to None
                    if (val == "null") val = "None";
                    // Don't quote if it's a variable name (no $ markers)
                    if (val.size() >= 2 && val.front() == '$' && val.back() == '$') {
                        // It's a string literal
                        args += key + "=" + quoteIfString(val);
                    } else {
                        // It's a variable or expression
                        args += key + "=" + val;
                    }
                } else {
                    // Positional argument
                    args += quoteIfString(attr);
                }
            }
            emit(node.value + "(" + args + ")");
            break;
        }

        case NodeType::FuncDef: {
            std::string args;
            for (size_t i = 0; i < node.attrs.size(); i++) {
                if (i > 0) args += ", ";
                args += node.attrs[i];
            }
            emit("def " + node.value + "(" + args + "):");
            indentLevel++;
            // Add global declarations for variables modified in function (but NOT parameters)
            if (!node.children.empty() && node.children[0]->type == NodeType::Block) {
                for (auto& stmt : node.children[0]->children) {
                    if (stmt->type == NodeType::PlusEqualStmt || stmt->type == NodeType::MinusEqualStmt ||
                        stmt->type == NodeType::MultiplyEqualStmt || stmt->type == NodeType::AtEqualStmt ||
                        stmt->type == NodeType::DivideEqualStmt || stmt->type == NodeType::AssignStmt) {
                        // Check if this variable is NOT a parameter
                        bool isParam = false;
                        for (auto& param : node.attrs) {
                            if (param == stmt->value) {
                                isParam = true;
                                break;
                            }
                        }
                        if (!isParam) {
                            emit("global " + stmt->value);
                        }
                    }
                }
            }
            if (!node.children.empty()) genBlock(*node.children[0]);
            else emit("pass");
            indentLevel--;
            break;
        }

        case NodeType::IfStmt: {
            if (node.value == "OTHER") {
                emit("else:");
                indentLevel++;
                for (auto& c : node.children) genNode(*c);
                indentLevel--;
            } else {
                std::string cond;
                size_t bodyIndex = 0;
                
                // Check if first child is a structured expression (new Pratt parser)
                if (!node.children.empty() && node.children[0]->type != NodeType::Block) {
                    cond = generateExpressionCode(*node.children[0]);
                    bodyIndex = 1;  // Body starts at index 1
                } else {
                    // Legacy string-based condition (fallback)
                    cond = translateCondition(node.value);
                    bodyIndex = 0;  // Body starts at index 0
                }
                
                emit("if " + cond + ":");
                indentLevel++;
                // Only process the body (first block child)
                if (bodyIndex < node.children.size()) {
                    genNode(*node.children[bodyIndex]);
                }
                indentLevel--;
                // Process remaining children (ELSEIF/OTHER) at the same indent level
                for (size_t i = bodyIndex + 1; i < node.children.size(); i++) {
                    genNode(*node.children[i]);
                }
            }
            break;
        }

        case NodeType::ElseIfStmt: {
            std::string cond;
            size_t bodyIndex = 0;
            
            // Check if first child is a structured expression (new Pratt parser)
            if (!node.children.empty() && node.children[0]->type != NodeType::Block) {
                cond = generateExpressionCode(*node.children[0]);
                bodyIndex = 1;  // Body starts at index 1
            } else {
                // Legacy string-based condition (fallback)
                cond = translateCondition(node.value);
                bodyIndex = 0;  // Body starts at index 0
            }
            
            emit("elif " + cond + ":");
            indentLevel++;
            for (size_t i = bodyIndex; i < node.children.size(); i++) {
                genNode(*node.children[i]);
            }
            indentLevel--;
            break;
        }

        case NodeType::ForLoop: {
            std::string lst;
            size_t bodyIndex = 0;
            
            // Check if first child is a structured expression (new Pratt parser)
            if (!node.children.empty() && node.children[0]->type != NodeType::Block) {
                lst = generateExpressionCode(*node.children[0]);
                bodyIndex = 1;  // Body starts at index 1
            } else {
                // Legacy string-based list (fallback)
                lst = node.attrs.empty() ? "[]" : translateExpr(node.attrs[0]);
                bodyIndex = 0;  // Body starts at index 0
            }
            
            emit("for " + node.value + " in " + lst + ":");
            indentLevel++;
            if (bodyIndex < node.children.size()) {
                genBlock(*node.children[bodyIndex]);
            } else {
                emit("pass");
            }
            indentLevel--;
            break;
        }

        case NodeType::WhilstLoop: {
            std::string cond;
            size_t bodyIndex = 0;
            
            // Check if first child is a structured expression (new Pratt parser)
            if (!node.children.empty() && node.children[0]->type != NodeType::Block) {
                cond = generateExpressionCode(*node.children[0]);
                bodyIndex = 1;  // Body starts at index 1
            } else {
                // Legacy string-based condition (fallback)
                cond = translateCondition(node.value);
                bodyIndex = 0;  // Body starts at index 0
            }
            
            // Simplify "x == True" to just "x" and "x == False" to "not x"
            if (cond.find(" == True") != std::string::npos) {
                cond = cond.substr(0, cond.find(" == True"));
            } else if (cond.find(" == False") != std::string::npos) {
                std::string var = cond.substr(0, cond.find(" == False"));
                cond = "not " + var;
            }
            
            emit("while " + cond + ":");
            indentLevel++;
            for (size_t i = bodyIndex; i < node.children.size(); i++) {
                genNode(*node.children[i]);
            }
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
            // Generate event listener setup with native keyboard binding
            emit("# Setup keyboard event bindings");
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    // Collect variables that are assigned in the callback (need nonlocal)
                    std::set<std::string> assignedVars;
                    std::function<void(const ASTNode&)> collectAssignedVars = [&](const ASTNode& n) {
                        if (n.type == NodeType::AssignStmt || n.type == NodeType::PlusEqualStmt ||
                            n.type == NodeType::MinusEqualStmt || n.type == NodeType::MultiplyEqualStmt ||
                            n.type == NodeType::AtEqualStmt || n.type == NodeType::DivideEqualStmt) {
                            assignedVars.insert(n.value);
                        }
                        for (auto& c : n.children) collectAssignedVars(*c);
                    };
                    for (auto& stmt : child->children) {
                        collectAssignedVars(*stmt);
                    }
                    
                    // Generate proper function definition for the key binding
                    emit("def _callback_" + key + "():");
                    indentLevel++;
                    // Add nonlocal declarations for assigned variables
                    for (const auto& var : assignedVars) {
                        emit("nonlocal " + var);
                    }
                    // Generate all nested statements from the KeyBinding
                    for (auto& stmt : child->children) {
                        genNode(*stmt);
                    }
                    indentLevel--;
                    emit("ac_event_listener.bind('" + key + "', _callback_" + key + ")");
                    // Map AC key names to tkinter key events - use proper function instead of lambda
                    std::string tk_key;
                    if (key == "up") tk_key = "<Up>";
                    else if (key == "down") tk_key = "<Down>";
                    else if (key == "left") tk_key = "<Left>";
                    else if (key == "right") tk_key = "<Right>";
                    else if (key == "space") tk_key = "<space>";
                    else tk_key = "<Key-" + key + ">";
                    // Generate tkinter binding function
                    emit("def _tk_" + key + "(e):");
                    indentLevel++;
                    emit("ac_event_listener.trigger('" + key + "')");
                    indentLevel--;
                    emit("root.root.bind('" + tk_key + "', _tk_" + key + ")");
                }
            }
            break;

        case NodeType::KeyBinding:
            // Handled by EventListener
            break;

        case NodeType::InputStmt: {
            // Send ghost/simulated keyboard input
            std::string key = node.value;
            emit("# Simulate key press: " + key);
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
            emit("def main():");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
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
