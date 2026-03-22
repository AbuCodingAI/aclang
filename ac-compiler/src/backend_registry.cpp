#include "../include/backend_registry.hpp"

// Forward declarations for generator functions
std::string generatePython(const ASTNode& ast);
std::string generateJS(const ASTNode& ast);
std::string generateHTML(const ASTNode& ast);
std::string generateJava(const ASTNode& ast);
std::string generateCpp(const ASTNode& ast);
std::string generateC(const ASTNode& ast);
std::string generateAsm(const ASTNode& ast);
std::string generateRs(const ASTNode& ast);
std::string generateGo(const ASTNode& ast);
std::string generateV(const ASTNode& ast);

// Static member definition
std::unordered_map<std::string, BackendRegistry::BackendInfo> BackendRegistry::backends;

void BackendRegistry::initializeStandardBackends() {
    if (!backends.empty()) return; // Already initialized
    
    // Register all standard backends
    registerBackend("PY", ".py", generatePython, 
        [](const std::string& outFile) { return "python3 " + outFile; });
    
    registerBackend("JS", ".js", generateJS, 
        [](const std::string& outFile) { return "node " + outFile; });
    
    registerBackend("HTML", ".html", generateHTML, 
        [](const std::string& outFile) { return "xdg-open " + outFile; });
    
    registerBackend("Java", ".java", generateJava, 
        [](const std::string& outFile) { return "javac " + outFile + " && java Main"; });
    
    registerBackend("C++", ".cpp", generateCpp, 
        [](const std::string& outFile) { return "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("C", ".c", generateC, 
        [](const std::string& outFile) { return "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("ASM", ".s", generateAsm, 
        [](const std::string& outFile) { return "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("RS", ".rs", generateRs, 
        [](const std::string& outFile) { return "rustc " + outFile + " -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("GO", ".go", generateGo, 
        [](const std::string& outFile) { return "go run " + outFile; });
    
    registerBackend("V", ".v", generateV, 
        [](const std::string& outFile) { return "cd /tmp/v && /usr/local/bin/v run " + outFile; });
}
