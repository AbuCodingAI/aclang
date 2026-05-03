const { execSync } = require('child_process');
const os = require('os');
const fs = require('fs');
const path = require('path');

console.log("=== AC Language Compiler Setup ===");

// Find the project root (where ac-compiler directory is located)
function findProjectRoot() {
    let currentDir = process.cwd();
    
    // Try to find ac-compiler directory
    while (currentDir !== path.parse(currentDir).root) {
        const acCompilerPath = path.join(currentDir, 'ac-compiler');
        if (fs.existsSync(acCompilerPath)) {
            return currentDir;
        }
        currentDir = path.dirname(currentDir);
    }
    
    // If not found, return current directory (might be project root)
    return process.cwd();
}

const projectRoot = findProjectRoot();
const acCompilerPath = path.join(projectRoot, 'ac-compiler');

console.log(`Project root: ${projectRoot}`);
console.log(`AC compiler path: ${acCompilerPath}`);

if (!fs.existsSync(acCompilerPath)) {
    console.error("Error: ac-compiler directory not found!");
    console.error("Please run this script from the project root directory.");
    console.error("Current directory:", process.cwd());
    process.exit(1);
}

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
    execSync('make clean', { cwd: acCompilerPath, stdio: 'inherit' });
    execSync('make', { cwd: acCompilerPath, stdio: 'inherit' });
    
    console.log("=== Setup Complete ===");
} catch (err) {
    console.error("Setup failed:", err.message);
    process.exit(1);
}
