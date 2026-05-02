const { execSync } = require('child_process');
const os = require('os');

console.log("=== AC Language Compiler Setup ===");

try {
    if (os.platform() === 'win32') {
        console.log("Windows detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ not found! Please install MinGW-w64: https://www.mingw-w64.org/");
            process.exit(1);
        }
    } else {
        console.log("Linux/macOS detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ missing. Attempting install...");
            if (os.platform() === 'linux') {
                execSync('sudo apt update && sudo apt install -y g++', { stdio: 'inherit' });
            } else {
                console.log("Please install GCC via Homebrew: brew install gcc");
                process.exit(1);
            }
        }
    }
    
    console.log("Building AC compiler...");
    execSync('make clean && make', { stdio: 'inherit' });
    
    console.log("=== Setup Complete ===");
} catch (err) {
    console.error("Setup failed:", err.message);
    process.exit(1);
}
