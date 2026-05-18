const { execSync } = require('child_process');
const os = require('os');
const fs = require('fs');
const path = require('path');

console.log("=== AC Language Compiler Setup ===");

const packageRoot = path.join(__dirname, '..');
const acCompilerPath = path.join(packageRoot, 'ac-compiler');

const isWindows = os.platform() === 'win32';
const binaryName = isWindows ? 'ac.exe' : 'ac';
const binaryPath = path.join(acCompilerPath, binaryName);

// If pre-built binary exists for this platform, nothing to do
if (fs.existsSync(binaryPath)) {
    console.log(`Pre-built binary found: ${binaryPath}`);
    if (!isWindows) {
        fs.chmodSync(binaryPath, 0o755);
    }
    console.log("=== Setup Complete ===");
    process.exit(0);
}

// No pre-built binary — fall back to building from source
console.log(`No pre-built binary found for ${os.platform()}. Building from source...`);

if (!fs.existsSync(acCompilerPath)) {
    console.error("Error: ac-compiler directory not found!");
    process.exit(1);
}

try {
    if (isWindows) {
        console.log("Windows detected. Checking for g++...");
        try {
            execSync('g++ --version', { stdio: 'pipe' });
        } catch (e) {
            console.log("g++ not found. Trying to install MinGW via winget...");
            try {
                execSync('winget install mingw.mingw-w64 -e', { stdio: 'inherit' });
            } catch (e2) {
                try {
                    execSync('choco install mingw -y', { stdio: 'inherit' });
                } catch (e3) {
                    console.error("Please install MinGW-w64 manually: https://www.mingw-w64.org/");
                    process.exit(1);
                }
            }
        }
        execSync('make', { cwd: acCompilerPath, stdio: 'inherit', shell: 'cmd.exe' });
    } else if (os.platform() === 'darwin') {
        console.log("macOS detected. Checking for g++...");
        try {
            execSync('g++ --version', { stdio: 'pipe' });
        } catch (e) {
            console.log("g++ not found. Installing via Homebrew...");
            execSync('brew install gcc', { stdio: 'inherit' });
        }
        execSync('make', { cwd: acCompilerPath, stdio: 'inherit' });
    } else {
        console.log("Linux detected. Checking for g++...");
        try {
            execSync('g++ --version', { stdio: 'pipe' });
        } catch (e) {
            console.log("g++ not found. Attempting install...");
            execSync('sudo apt-get install -y g++', { stdio: 'inherit' });
        }
        execSync('make', { cwd: acCompilerPath, stdio: 'inherit' });
    }

    console.log("=== Setup Complete ===");
} catch (err) {
    console.error("Setup failed:", err.message);
    process.exit(1);
}
