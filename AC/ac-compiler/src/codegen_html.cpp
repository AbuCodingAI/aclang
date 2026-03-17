#include "../include/ast.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <cctype>

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
