#include "../include/ast.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstring>

class HTMLCodeGen {
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

        case NodeType::Program:
            collectTitle(node);
            body << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
            body << "    <meta charset=\"UTF-8\">\n";
            body << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
            body << "    <title>" << pageTitle << "</title>\n";
            body << head.str();
            body << "</head>\n<body>\n";
            indentLevel++;
            for (auto& c : node.children) genNode(*c);
            indentLevel--;
            body << "</body>\n</html>\n";
            break;

        case NodeType::BackendDecl: break;
        case NodeType::UseStmt:     break;
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

        case NodeType::AssignStmt:
            if (!node.attrs.empty())
                emit("<p id=\"" + node.value + "\">" + stripDollars(node.attrs[0]) + "</p>");
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
    HTMLCodeGen gen;
    return gen.generate(ast);
}
