const { execSync } = require('child_process');
const os = require('os');

console.log("=== AC Language Compiler Setup ===");

try {
    if (os.platform() === 'win32') {
        console.log("Windows detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ not found! Installing MinGW-w64 via MSYS2...");
            console.log("Installing MSYS2 and MinGW-w64...");
            execSync('choco install mingw -y', { stdio: 'inherit' });
        }
    } else if (os.platform() === 'darwin') {
        console.log("macOS detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ missing. Installing via Homebrew...");
            execSync('brew install gcc', { stdio: 'inherit' });
        }
    } else {
        console.log("Linux detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ missing. Attempting install...");
            execSync('sudo apt update && sudo apt install -y g++', { stdio: 'inherit' });
        }
    }
    
    console.log("Building AC compiler...");
    execSync('make clean && make', { stdio: 'inherit' });
    
    console.log("=== Setup Complete ===");
} catch (err) {
    console.error("Setup failed:", err.message);
    process.exit(1);
}
