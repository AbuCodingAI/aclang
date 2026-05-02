const { execSync } = require('child_process');
const os = require('os');

console.log("=== AC Language Compiler Setup ===");

try {
    if (os.platform() === 'win32') {
        console.log("Windows detected. Checking for g++...");
        try {
            execSync('g++ --version');
        } catch (e) {
            console.log("g++ not found! Installing via winget...");
            // Try winget first (built into Windows 10/11)
            try {
                execSync('winget install mingw.mingw-w64 -e', { stdio: 'inherit' });
            } catch (e) {
                console.log("winget failed. Trying choco...");
                try {
                    execSync('choco install mingw -y', { stdio: 'inherit' });
                } catch (e2) {
                    console.log("Please install MinGW-w64 manually:");
                    console.log("  - Download from: https://www.mingw-w64.org/");
                    console.log("  - Or use winget: winget install mingw.mingw-w64");
                    process.exit(1);
                }
            }
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
