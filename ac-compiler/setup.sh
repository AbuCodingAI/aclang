#!/bin/bash
# AC Language Compiler Setup Script
# Bundles g++ with the compiler for easy installation

set -e

echo "=== AC Language Compiler Setup ==="
echo ""

# Detect OS
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    echo "Windows detected"
    echo "Please install MinGW-w64 first:"
    echo "  - Download from: https://www.mingw-w64.org/"
    echo "  - Or use MSYS2: https://www.msys2.org/"
    echo ""
    echo "Then run: make"
    exit 0
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "macOS detected"
    echo "Installing dependencies..."
    if command -v brew &> /dev/null; then
        brew install gcc
    else
        echo "Homebrew not found. Please install GCC manually:"
        echo "  - Xcode Command Line Tools: xcode-select --install"
        echo "  - Or use Homebrew: brew install gcc"
    fi
else
    echo "Linux detected"
    echo "Installing dependencies..."
    
    # Try to install g++ if not present
    if ! command -v g++ &> /dev/null; then
        if command -v apt &> /dev/null; then
            sudo apt update && sudo apt install -y g++
        elif command -v yum &> /dev/null; then
            sudo yum install -y gcc-c++
        elif command -v dnf &> /dev/null; then
            sudo dnf install -y gcc-c++
        elif command -v pacman &> /dev/null; then
            sudo pacman -S --noconfirm gcc
        else
            echo "Could not detect package manager. Please install g++ manually."
            exit 1
        fi
    fi
fi

echo ""
echo "Building AC compiler..."
cd "$(dirname "$0")"
make clean
make

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Usage:"
echo "  ./ac mycode.ac          # Compile and run"
echo "  ./ac mycode.ac PY       # Compile to Python"
echo "  ./ac mycode.ac BNY      # Compile to native binary"
echo ""
echo "Or use the REPL:"
echo "  python3 ac_repl.py"
echo ""
