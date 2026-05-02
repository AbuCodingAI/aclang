#include "../include/ast.hpp"
#include "../include/type.hpp"
#include "base_codegen.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>

class HtmlCodeGen : public BaseCodeGen {
    std::ostringstream head;
    std::ostringstream body;
    int indentLevel = 0;
    std::string pageTitle = "AC Generated Page";

    void emit(const std::string& line) {
        body << std::string(indentLevel * 4, ' ') << line << "\n";
    }

    void emitHead(const std::string& line) {
        head << "    " << line << "\n";
    }

    std::string stripDollars(const std::string& val) {
        std::string v = val;
        while (!v.empty() && v.back() == ' ') v.pop_back();
        if (v.size() >= 2 && v.front() == '$' && v.back() == '$')
            return v.substr(1, v.size() - 2);
        return v;
    }

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
            // Escape special characters for HTML/JavaScript
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

        // Convert AC operators to HTML
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
        
        // Translate AC booleans and null to JavaScript (case-insensitive)
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

    // First pass: scan for title.display to extract page title
    void collectTitle(const ASTNode& node) {
        if (node.type == NodeType::MethodCall && node.value == "title.display") {
            if (!node.attrs.empty()) pageTitle = stripDollars(node.attrs[0]);
            return;
        }
        for (auto& c : node.children) collectTitle(*c);
    }

    void genNode(const ASTNode& node) {
        switch (node.type) {

        case NodeType::Program: {
            collectTitle(node);
            
            // Check if widgets library is used
            bool usesWidgets = false;
            for (auto& c : node.children) {
                if (c->type == NodeType::UseLibStmt && c->value == "widgets") {
                    usesWidgets = true;
                    break;
                }
            }
            
            body << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
            body << "    <meta charset=\"UTF-8\">\n";
            body << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
            body << "    <title>" << pageTitle << "</title>\n";
            
            // Include widgets library if needed
            if (usesWidgets) {
                body << "    <script src=\"library/widgets/widgets.js\"></script>\n";
            }
            
            body << "    <script>\n";
            body << "        // Event listener implementation\n";
            body << "        class EventListener {\n";
            body << "            constructor() { this.bindings = {}; }\n";
            body << "            bind(key, callback) { this.bindings[key] = callback; }\n";
            body << "            trigger(key) {\n";
            body << "                // Automatically trigger if binding exists, otherwise do nothing\n";
            body << "                if (this.bindings[key]) this.bindings[key]();\n";
            body << "            }\n";
            body << "            check_and_trigger(key) {\n";
            body << "                // Check and trigger - if no binding, do absolutely nothing\n";
            body << "                this.trigger(key);\n";
            body << "            }\n";
            body << "        }\n";
            body << "        const acEventListener = new EventListener();\n";
            body << "\n";
            body << "        // Keybinds\n";
            body << "        const KEY_1 = \"1\";\n";
            body << "        const KEY_2 = \"2\";\n";
            body << "        const KEY_3 = \"3\";\n";
            body << "        const KEY_4 = \"4\";\n";
            body << "        const KEY_5 = \"5\";\n";
            body << "        const KEY_6 = \"6\";\n";
            body << "        const KEY_7 = \"7\";\n";
            body << "        const KEY_8 = \"8\";\n";
            body << "        const KEY_9 = \"9\";\n";
            body << "        const KEY_0 = \"0\";\n";
            body << "        const KEY_MINUS = \"-\";\n";
            body << "        const KEY_EQUAL = \"=\";\n";
            body << "        const KEY_BACKSPACE = \"Backspace\";\n";
            body << "        const KEY_BACKSLASH = \"\\\\\";\n";
            body << "        const KEY_TAB = \"Tab\";\n";
            body << "        const KEY_Q = \"q\";\n";
            body << "        const KEY_W = \"w\";\n";
            body << "        const KEY_E = \"e\";\n";
            body << "        const KEY_R = \"r\";\n";
            body << "        const KEY_T = \"t\";\n";
            body << "        const KEY_Y = \"y\";\n";
            body << "        const KEY_U = \"u\";\n";
            body << "        const KEY_I = \"i\";\n";
            body << "        const KEY_O = \"o\";\n";
            body << "        const KEY_P = \"p\";\n";
            body << "        const KEY_LBRACKET = \"[\";\n";
            body << "        const KEY_RBRACKET = \"]\";\n";
            body << "        const KEY_CAPS = \"CapsLock\";\n";
            body << "        const KEY_A = \"a\";\n";
            body << "        const KEY_S = \"s\";\n";
            body << "        const KEY_D = \"d\";\n";
            body << "        const KEY_F = \"f\";\n";
            body << "        const KEY_G = \"g\";\n";
            body << "        const KEY_H = \"h\";\n";
            body << "        const KEY_J = \"j\";\n";
            body << "        const KEY_K = \"k\";\n";
            body << "        const KEY_L = \"l\";\n";
            body << "        const KEY_SEMICOLON = \";\";\n";
            body << "        const KEY_APOSTROPHE = \"'\";\n";
            body << "        const KEY_ENTER = \"Enter\";\n";
            body << "        const KEY_SHIFT = \"Shift\";\n";
            body << "        const KEY_Z = \"z\";\n";
            body << "        const KEY_X = \"x\";\n";
            body << "        const KEY_C = \"c\";\n";
            body << "        const KEY_V = \"v\";\n";
            body << "        const KEY_B = \"b\";\n";
            body << "        const KEY_N = \"n\";\n";
            body << "        const KEY_M = \"m\";\n";
            body << "        const KEY_COMMA = \",\";\n";
            body << "        const KEY_PERIOD = \".\";\n";
            body << "        const KEY_SLASH = \"/\";\n";
            body << "        const KEY_CTRL = \"Control\";\n";
            body << "        const KEY_ALT = \"Alt\";\n";
            body << "        const KEY_SPACE = \" \";\n";
            body << "        const KEY_FN = \"Meta\";\n";
            body << "        const KEY_SUPER = \"Meta\";\n";
            body << "        const KEY_WINDOWS = \"Meta\";\n";
            body << "        const KEY_UP = \"ArrowUp\";\n";
            body << "        const KEY_DOWN = \"ArrowDown\";\n";
            body << "        const KEY_LEFT = \"ArrowLeft\";\n";
            body << "        const KEY_RIGHT = \"ArrowRight\";\n";
            body << "    </script>\n";
            body << head.str();
            body << "</head>\n<body>\n";
            indentLevel++;
            
            // Find mainloop tag block
            const ASTNode* mainloopBlock = nullptr;
            for (auto& c : node.children) {
                if (c->type == NodeType::TagBlock && c->value == "mainloop") {
                    mainloopBlock = c.get();
                    break;
                }
            }
            
            // Process only mainloop
            if (mainloopBlock) {
                for (auto& c : mainloopBlock->children) genNode(*c);
            }
            
            indentLevel--;
            body << "</body>\n</html>\n";
            break;
        }

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
        case NodeType::UseLibStmt: {
            // use ilib camera -> include camera library
            std::string libName = node.value;
            if (libName == "camera") {
                emitRaw("<!-- Camera library not supported in HTML -->");
            }
            break;
        }
        case NodeType::SaveStmt:    break;
        case NodeType::ConfigCall:  break;
        case NodeType::ObjDecl:     break;
        case NodeType::KillStmt:    break;
        case NodeType::ForeignBlock:
            body << "\n<!-- Preposterous: Foreign code is too Foreign (if this breaks, that's on you) -->\n";
            body << node.value << "\n";
            break;
        case NodeType::ReturnStmt:  break;

        case NodeType::DisplayStmt: {
            emit("<p>" + stripDollars(node.value) + "</p>");
            break;
        }

        case NodeType::MethodCall: {
            std::string name = node.value;
            std::string args = node.attrs.empty() ? "" : stripDollars(node.attrs[0]);

            if (name == "title.display") {
                // already handled in head, skip body output
            } else if (name == "Term.display") {
                // terminal only, no HTML output
            } else if (name == "Term.ask") {
                // terminal only, no HTML output
            } else if (name == "display") {
                emit("<p>" + args + "</p>");
            } else if (name == "bold.display") {
                emit("<b>" + args + "</b>");
            } else if (name == "italic.display") {
                emit("<i>" + args + "</i>");
            } else if (name == "header.display") {
                std::string level = "1";
                if (node.attrs.size() > 1)
                    for (char c : node.attrs[1])
                        if (std::isdigit(c)) { level = std::string(1, c); break; }
                emit("<h" + level + ">" + args + "</h" + level + ">");
            } else if (name == "link.display") {
                emit("<a href=\"#\">" + args + "</a>");
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

        case NodeType::AssignStmt:
            if (!node.attrs.empty()) {
                std::string val = node.attrs[0];
                if (val.substr(0, 8) == "__dict__") {
                    // HTML: JavaScript object in script tag
                    emit("<script>let " + node.value + " = {" + parseDictItems(val.substr(8)) + "};</script>");
                } else
                    emit("<p id=\"" + node.value + "\">" + translateExpr(stripDollars(node.attrs[0])) + "</p>");
            }
            break;

        case NodeType::TagBlock:
            genTagBlock(node);
            break;

        case NodeType::Block:
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::IfStmt:
        case NodeType::ElseIfStmt:
        case NodeType::ForLoop:
        case NodeType::WhilstLoop:
            // logic constructs — just emit children content
            for (auto& c : node.children) genNode(*c);
            break;

        case NodeType::ListLiteral: {
            std::string items = parseCollectionItems(node.value);
            emit("<ul><li>" + items + "</li></ul>");
            break;
        }

        case NodeType::TupleLiteral: {
            std::string items = parseCollectionItems(node.value);
            emit("<div class=\"tuple\">(" + items + ")</div>");
            break;
        }

        case NodeType::RangeExpr: {
            std::string n = node.value;
            while (!n.empty() && n.back() == ' ') n.pop_back();
            emit("<span class=\"range\">[0.." + n + "]</span>");
            break;
        }

        case NodeType::SequenceExpr: {
            size_t comma = node.value.find(',');
            std::string x = node.value.substr(0, comma);
            std::string y = node.value.substr(comma + 1);
            emit("<span class=\"sequence\">[" + x + ".." + y + "]</span>");
            break;
        }

        case NodeType::RaiseStmt:
            emit("<p class=\"error\">Preposterous: " + node.value + "</p>");
            break;

        case NodeType::EventListener: {
            // Generate event listener setup for HTML/JS
            body << "    <script>\n";
            for (auto& child : node.children) {
                if (child->type == NodeType::KeyBinding) {
                    std::string key = child->value;
                    std::string callback = child->attrs.empty() ? "" : child->attrs[0];
                    if (!callback.empty()) {
                        body << "        acEventListener.bind('" + key + "', () => " + callback + ");\n";
                    }
                }
            }
            body << "    </script>\n";
            break;
        }

        case NodeType::KeyBinding:
            // Handled by EventListener
            break;

        case NodeType::InputStmt: {
            // Send ghost/simulated keyboard input in HTML/JS
            std::string key = node.value;
            body << "    <script>\n";
            body << "        // Simulate key press: " << key << "\n";
            body << "        acEventListener.trigger('" << key << "');\n";
            body << "    </script>\n";
            break;
        }

        default: break;
        }
    }

    void genTagBlock(const ASTNode& node) {
        std::string name = node.value;
        if (name == "mainloop") {
            emit("<main>");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            emit("</main>");
        } else if (name == "LOGIC" || name == "Local") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "StartHere") {
            for (auto& c : node.children) genNode(*c);
        } else if (name == "gui" || name == "OBJECT" || name == "SCREEN") {
            // GUI — skip
        } else {
            emit("<section id=\"" + name + "\">");
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            emit("</section>");
        }
    }

public:
    std::string generate(const ASTNode& node) {
        genNode(node);
        return body.str();
    }
};

std::string generateHTML(const ASTNode& ast) {
    HtmlCodeGen gen;
    return gen.generate(ast);
}
