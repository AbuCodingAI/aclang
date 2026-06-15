#include "../include/ast.hpp"
#include "../include/token.hpp"
#include "../include/error.hpp"
#include "../include/aibc.hpp"
#include "interpreter.hpp"   // from ai-vm/include (via -I ../ai-vm/include)
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

// Forward declarations from shared lexer/parser
std::vector<Token> lex(const std::string& source);
NodePtr            parse(const std::vector<Token>& tokens);

// Forward declaration from ai_codegen.cpp
AiBcModule compileToAibc(const ASTNode& ast);

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "Preposterous: cannot open file: " << path << "\n"; exit(1); }
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static void writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Preposterous: cannot write file: " << path << "\n"; exit(1); }
    f.write(data.data(), (std::streamsize)data.size());
}

static std::string makeOutputPath(const std::string& input) {
    size_t dot = input.rfind('.');
    std::string base = (dot == std::string::npos) ? input : input.substr(0, dot);
    return base + ".aibc";
}

// Inject FuncDef/BundleDef from flib .ac/.ai files into the AST before codegen.
static void injectFlibModules(ASTNode& root, const std::string& srcDir) {
    NodeList toAppend;
    for (auto& child : root.children) {
        if (!child || child->type != NodeType::UseLibStmt) continue;
        const std::string& val = child->value;
        if (val.rfind("flib:", 0) != 0) continue;
        if (val.rfind("flib:__inlined__:", 0) == 0) continue;
        std::string libpath = val.substr(5);
        auto dot = libpath.rfind('.');
        if (dot == std::string::npos) continue;
        std::string ext = libpath.substr(dot);
        if (ext != ".ac" && ext != ".ai") continue;

        std::string fullPath = (!libpath.empty() && libpath[0] == '/')
            ? libpath : (srcDir + "/" + libpath);

        std::ifstream ff(fullPath);
        if (!ff) {
            std::cerr << "Preposterous: FlibError: cannot open flib file: " << fullPath << "\n";
            continue;
        }
        std::ostringstream buf;
        buf << ff.rdbuf();

        auto flibTokens = lex(buf.str());
        auto flibAst    = parse(flibTokens);
        if (!flibAst) continue;

        std::string flibDir = fullPath;
        auto slash = flibDir.find_last_of("/\\");
        flibDir = (slash == std::string::npos) ? "." : flibDir.substr(0, slash);
        injectFlibModules(*flibAst, flibDir);

        for (auto& node : flibAst->children) {
            if (!node) continue;
            if (node->type == NodeType::FuncDef  ||
                node->type == NodeType::BundleDef ||
                node->type == NodeType::UseLibStmt)
                toAppend.push_back(std::move(node));
        }
        child->value = "flib:__inlined__:" + libpath;
    }
    for (auto& e : toAppend)
        root.children.push_back(std::move(e));
}

// ── .datac column-oriented injection for AI ──────────────────────────────────
// Parses a .datac file and generates one list variable per field (column-oriented).
// E.g. students_name = [STR:Alex, STR:Sam, ...], students_age = [INT:16, INT:17, ...]
// plus students_size = <row count>.

struct AiDatacRow { std::vector<std::pair<std::string,std::string>> fields; }; // key, raw-value

static std::string ai_datac_fmtval(const std::string& v) {
    // Returns "INT:N" or "DEC:N.N" or "STR:text"
    if (v.empty()) return "STR:";
    // strip surrounding quotes
    std::string inner = v;
    if ((inner.front() == '"' && inner.back() == '"') ||
        (inner.front() == '\'' && inner.back() == '\''))
        inner = inner.substr(1, inner.size() - 2);
    // is it a number?
    bool hasDigit = false, hasDot = false, isNum = true;
    for (size_t i = 0; i < inner.size(); i++) {
        char c = inner[i];
        if (c == '-' && i == 0) continue;
        if (c == '.') { if (hasDot) { isNum = false; break; } hasDot = true; continue; }
        if (std::isdigit((unsigned char)c)) { hasDigit = true; continue; }
        isNum = false; break;
    }
    if (isNum && hasDigit) {
        if (hasDot) return "DEC:" + inner;
        return "INT:" + inner;
    }
    return "STR:" + inner;
}

static std::vector<AiDatacRow> parseAiDatacRows(const std::string& content) {
    std::vector<AiDatacRow> rows;
    std::istringstream ss(content);
    std::string line;
    bool inBlock = false;
    std::string rowBuf;

    auto flushRow = [&]() {
        if (rowBuf.empty()) return;
        AiDatacRow row;
        std::string cur;
        bool inQ = false;
        for (char c : rowBuf) {
            if ((c == '"' || c == '\'') && !inQ) { inQ = true; cur += c; }
            else if ((c == '"' || c == '\'') && inQ)  { inQ = false; cur += c; }
            else if (c == ',' && !inQ) {
                auto colon = cur.find(':');
                if (colon != std::string::npos) {
                    std::string k = cur.substr(0, colon);
                    std::string v = cur.substr(colon + 1);
                    auto trim = [](std::string& s) {
                        auto b = s.find_first_not_of(" \t\r\n");
                        auto e = s.find_last_not_of(" \t\r\n");
                        s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
                    };
                    trim(k); trim(v);
                    if (!k.empty()) row.fields.push_back({k, ai_datac_fmtval(v)});
                }
                cur.clear();
            } else cur += c;
        }
        // flush last field
        if (!cur.empty()) {
            auto colon = cur.find(':');
            if (colon != std::string::npos) {
                std::string k = cur.substr(0, colon);
                std::string v = cur.substr(colon + 1);
                auto trim = [](std::string& s) {
                    auto b = s.find_first_not_of(" \t\r\n");
                    auto e = s.find_last_not_of(" \t\r\n");
                    s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
                };
                trim(k); trim(v);
                if (!k.empty()) row.fields.push_back({k, ai_datac_fmtval(v)});
            }
        }
        if (!row.fields.empty()) rows.push_back(row);
        rowBuf.clear();
    };

    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\t'))
            line.pop_back();
        if (!inBlock) {
            auto lb = line.find('{');
            if (lb != std::string::npos) { inBlock = true; }
        } else {
            if (line == "}") { flushRow(); inBlock = false; continue; }
            if (line.empty()) { flushRow(); continue; }
            if (!rowBuf.empty()) {
                auto fc = line.find(':');
                if (fc != std::string::npos) {
                    std::string fk = line.substr(0, fc);
                    while (!fk.empty() && (fk.front()==' '||fk.front()=='\t')) fk.erase(fk.begin());
                    if (!rowBuf.empty() && rowBuf.find(fk + ":") != std::string::npos)
                        flushRow();
                }
                if (!rowBuf.empty()) rowBuf += ",";
            }
            rowBuf += line;
        }
    }
    flushRow();
    return rows;
}

static void injectDatacImports(ASTNode& root, const std::string& srcDir) {
    for (size_t i = 0; i < root.children.size(); i++) {
        auto& child = root.children[i];
        if (!child || child->type != NodeType::UseLibStmt) continue;
        const std::string& val = child->value;
        if (val.rfind("datac:", 0) != 0) continue;

        std::string rest = val.substr(6);
        auto sep = rest.rfind(':');
        if (sep == std::string::npos) continue;
        std::string filepath = rest.substr(0, sep);
        std::string alias    = rest.substr(sep + 1);

        std::string fullPath = (!filepath.empty() && filepath[0] == '/')
            ? filepath : (srcDir + "/" + filepath);

        std::ifstream ff(fullPath);
        if (!ff) {
            std::cerr << "Preposterous: DatacError: cannot open datac file: " << fullPath << "\n";
            continue;
        }
        std::ostringstream buf;
        buf << ff.rdbuf();

        auto rows = parseAiDatacRows(buf.str());
        if (rows.empty()) continue;

        // Collect field names in insertion order
        std::vector<std::string> fieldNames;
        for (auto& f : rows[0].fields) {
            bool found = false;
            for (auto& n : fieldNames) if (n == f.first) { found = true; break; }
            if (!found) fieldNames.push_back(f.first);
        }

        NodeList replacements;

        // One list variable per field
        for (auto& fname : fieldNames) {
            std::string listContent;
            for (size_t r = 0; r < rows.size(); r++) {
                std::string fval;
                for (auto& kv : rows[r].fields)
                    if (kv.first == fname) { fval = kv.second; break; }
                if (fval.empty()) fval = "INT:0";
                if (r) listContent += ",";
                listContent += fval;
            }
            auto node = std::make_unique<ASTNode>(NodeType::AssignStmt, alias + "_" + fname);
            node->attrs.push_back("__ailist__" + listContent);
            replacements.push_back(std::move(node));
        }

        // Size variable
        auto szNode = std::make_unique<ASTNode>(NodeType::AssignStmt, alias + "_size");
        szNode->attrs.push_back(std::to_string(rows.size()));
        replacements.push_back(std::move(szNode));

        root.children.erase(root.children.begin() + (long)i);
        for (size_t k = 0; k < replacements.size(); k++)
            root.children.insert(root.children.begin() + (long)(i + k), std::move(replacements[k]));
        i += replacements.size() - 1;
    }
}

static void runAibc(const std::string& path) {
    std::string data = readFile(path);
    AiBcModule  mod  = AiBcModule::deserialize(data);
    aivm_run(mod);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "AI Compiler — Abu Development Kit\n"
                  << "Usage: ai <source.ai> [--no-run]\n"
                  << "       ai <source.aibc>  (run pre-compiled bytecode)\n";
        return 1;
    }

    std::string inputPath = argv[1];
    bool noRun = false;
    for (int i = 2; i < argc; i++)
        if (std::strcmp(argv[i], "--no-run") == 0) noRun = true;

    // Run a pre-compiled .aibc directly
    if (inputPath.size() >= 5 && inputPath.substr(inputPath.size() - 5) == ".aibc") {
        runAibc(inputPath);
        return 0;
    }

    // Compile .ai → .aibc
    std::string source = readFile(inputPath);
    auto tokens = lex(source);
    NodePtr ast = parse(tokens);

    std::string srcDir;
    {
        size_t lastSlash = inputPath.find_last_of("/\\");
        srcDir = (lastSlash == std::string::npos) ? "." : inputPath.substr(0, lastSlash);
    }
    injectFlibModules(*ast, srcDir);
    injectDatacImports(*ast, srcDir);

    AiBcModule  mod        = compileToAibc(*ast);
    std::string aibcPath   = makeOutputPath(inputPath);
    std::string serialized = mod.serialize();
    writeFile(aibcPath, serialized);
    std::cout << "Compiled → " << aibcPath << "\n";

    if (!noRun) runAibc(aibcPath);
    return 0;
}
