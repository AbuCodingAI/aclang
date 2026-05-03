#!/usr/bin/env node
/**
 * AC Language Backend Installer
 * Installs all backend dependencies for cross-platform compilation
 */

const { execSync } = require('child_process');
const os = require('os');
const fs = require('fs');
const path = require('path');

console.log("=== AC Language Backend Installer ===\n");

// Check if we're running from the ac-compiler directory
const acCompilerPath = path.join(__dirname, '..', 'ac-compiler');
if (!fs.existsSync(acCompilerPath)) {
    console.error("Error: ac-compiler directory not found!");
    console.error("Please run this script from the project root directory.");
    process.exit(1);
}

// Backend dependencies
const backends = {
    python: {
        name: "Python",
        check: "python3 --version",
        install: {
            linux: "sudo apt install -y python3",
            macos: "brew install python",
            windows: "winget install Python.Python.3.13"
        }
    },
    node: {
        name: "Node.js/JavaScript",
        check: "node --version",
        install: {
            linux: "sudo apt install -y nodejs npm",
            macos: "brew install node",
            windows: "winget install OpenJS.NodeJS"
        }
    },
    gcc: {
        name: "GCC (C/C++)",
        check: "gcc --version",
        install: {
            linux: "sudo apt install -y gcc g++",
            macos: "brew install gcc",
            windows: "winget install mingw.mingw-w64"
        }
    },
    rust: {
        name: "Rust",
        check: "rustc --version",
        install: {
            linux: "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh",
            macos: "brew install rust",
            windows: "winget install Rustlang.Rustup"
        }
    },
    go: {
        name: "Go",
        check: "go version",
        install: {
            linux: "sudo apt install -y golang-go",
            macos: "brew install go",
            windows: "winget install GoLang.Go"
        }
    },
    java: {
        name: "Java",
        check: "java --version",
        install: {
            linux: "sudo apt install -y default-jdk",
            macos: "brew install openjdk",
            windows: "winget install OpenJDK.OpenJDK.17"
        }
    },
    vlang: {
        name: "V",
        check: "v --version",
        install: {
            linux: "sudo apt install -y vlang",
            macos: "brew install v",
            windows: "winget install Vlang.V"
        }
    }
};

// Check if a backend is installed
function isBackendInstalled(checkCmd) {
    try {
        execSync(checkCmd, { stdio: 'pipe' });
        return true;
    } catch (e) {
        return false;
    }
}

// Install a backend
function installBackend(backend) {
    const platform = os.platform();
    const installCmd = backend.install[platform];

    if (!installCmd) {
        console.log(`  ${backend.name}: No automated install for ${platform}`);
        return false;
    }

    console.log(`  Installing ${backend.name}...`);
    try {
        execSync(installCmd, { stdio: 'inherit' });
        console.log(`  ${backend.name}: Installed successfully`);
        return true;
    } catch (e) {
        console.log(`  ${backend.name}: Installation failed. Please install manually.`);
        return false;
    }
}

// Main installation process
function installBackends() {
    const installed = [];
    const failed = [];

    console.log("Checking and installing backends...\n");

    for (const [key, backend] of Object.entries(backends)) {
        if (isBackendInstalled(backend.check)) {
            console.log(`✓ ${backend.name} is already installed`);
            installed.push(backend.name);
        } else {
            console.log(`✗ ${backend.name} not found`);
            if (installBackend(backend)) {
                installed.push(backend.name);
            } else {
                failed.push(backend.name);
            }
        }
    }

    console.log("\n=== Installation Summary ===");
    console.log(`Installed: ${installed.length}`);
    console.log(`Failed: ${failed.length}`);

    if (installed.length > 0) {
        console.log("\nInstalled backends:");
        installed.forEach(b => console.log(`  - ${b}`));
    }

    if (failed.length > 0) {
        console.log("\nFailed backends (install manually):");
        failed.forEach(b => console.log(`  - ${b}`));
    }

    console.log("\n=== Next Steps ===");
    console.log("1. Run: cd ac-compiler && make");
    console.log("2. Test: ./ac ../examples/hello-world.ac");
}

// Run installation
installBackends();
