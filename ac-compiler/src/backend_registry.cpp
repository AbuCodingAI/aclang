#include "../include/backend_registry.hpp"
#include "../include/ast.hpp"
#include <string>
#include <algorithm>

// Dummy generator function - not used anymore (unified IR codegen handles all backends)
std::string dummyGenerator(const ASTNode& ast) {
    return "";
}

// Static member definition
std::unordered_map<std::string, BackendRegistry::BackendInfo> BackendRegistry::backends;

// Helper to resolve library paths
std::string resolveLibraryPath(const std::string& libName) {
    // Hardcoded camera library path for all backends
    if (libName == "camera") {
        return "../library/camera/camera.hpp";
    }
    
    // Default library paths
    std::string path = "../library/" + libName + "/" + libName;
    
    // Determine extension based on backend
    // For C/C++ backends: .hpp or .h
    // For Python: .py
    // For JS: .js
    // etc.
    
    return path;
}

void BackendRegistry::initializeStandardBackends() {
    if (!backends.empty()) return; // Already initialized
    
    // Register all standard backends (generator function not used - unified IR codegen handles all)
    registerBackend("PY", ".py", dummyGenerator, 
        [](const std::string& outFile) { return "python3 " + outFile; });
    
    registerBackend("JS", ".js", dummyGenerator, 
        [](const std::string& outFile) { return "node " + outFile; });
    
    registerBackend("HTML", ".html", dummyGenerator, 
        [](const std::string& outFile) { return "xdg-open " + outFile; });
    
    registerBackend("Java", ".java", dummyGenerator, 
        [](const std::string& outFile) { return "javac " + outFile + " && java Main"; });
    
    registerBackend("C++", ".cpp", dummyGenerator, 
        [](const std::string& outFile) { return "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("CPP", ".cpp", dummyGenerator, 
        [](const std::string& outFile) { return "g++ " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("C", ".c", dummyGenerator, 
        [](const std::string& outFile) { return "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("ASM", ".s", dummyGenerator, 
        [](const std::string& outFile) { return "gcc " + outFile + " -I.. -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("RS", ".rs", dummyGenerator, 
        [](const std::string& outFile) { return "rustc " + outFile + " -o /tmp/ac_out && /tmp/ac_out"; });
    
    registerBackend("GO", ".go", dummyGenerator, 
        [](const std::string& outFile) { return "go run " + outFile; });
    
    registerBackend("V", ".v", dummyGenerator, 
        [](const std::string& outFile) { return "cd /tmp/v && /usr/local/bin/v run " + outFile; });
    
    registerBackend("BNY", ".acb", dummyGenerator, 
        [](const std::string& outFile) { return outFile; });  // Direct execution of binary
}
