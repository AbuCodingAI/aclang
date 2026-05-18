#!/usr/bin/env python3
"""
AC Language Compiler - Automated Installer
This script handles installation with bundled g++ for cross-platform compatibility
"""

import os
import sys
import subprocess
import shutil
import tempfile
import urllib.request
import tarfile
import zipfile
from pathlib import Path


def check_gcc():
    """Check if g++ is available"""
    try:
        result = subprocess.run(['g++', '--version'], capture_output=True, text=True)
        return result.returncode == 0
    except FileNotFoundError:
        return False


def install_gcc_linux():
    """Install g++ on Linux"""
    print("Installing g++ on Linux...")
    
    # Try different package managers
    package_managers = [
        ('apt', ['sudo', 'apt', 'update']),
        ('apt', ['sudo', 'apt', 'install', '-y', 'g++']),
        ('yum', ['sudo', 'yum', 'install', '-y', 'gcc-c++']),
        ('dnf', ['sudo', 'dnf', 'install', '-y', 'gcc-c++']),
        ('pacman', ['sudo', 'pacman', '-S', '--noconfirm', 'gcc']),
    ]
    
    for pm, cmd in package_managers:
        try:
            print(f"Trying {pm}...")
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if result.returncode == 0:
                print(f"Successfully installed g++ using {pm}")
                return True
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            continue
    
    print("Could not install g++ automatically.")
    print("Please install g++ manually:")
    print("  Ubuntu/Debian: sudo apt install g++")
    print("  CentOS/RHEL: sudo yum install gcc-c++")
    print("  Fedora: sudo dnf install gcc-c++")
    print("  Arch: sudo pacman -S gcc")
    return False


def install_gcc_macos():
    """Install g++ on macOS"""
    print("Installing g++ on macOS...")
    
    # Check for Homebrew
    try:
        result = subprocess.run(['brew', '--version'], capture_output=True, text=True)
        if result.returncode == 0:
            print("Installing GCC via Homebrew...")
            subprocess.run(['brew', 'install', 'gcc'], check=True)
            return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    
    print("Homebrew not found or installation failed.")
    print("Please install GCC manually:")
    print("  - Install Xcode Command Line Tools: xcode-select --install")
    print("  - Or use Homebrew: brew install gcc")
    return False


def install_gcc_windows():
    """Install g++ on Windows"""
    print("Windows requires manual g++ installation.")
    print("")
    print("Option 1 - MSYS2 (Recommended):")
    print("  1. Download: https://www.msys2.org/")
    print("  2. Install and open MSYS2 terminal")
    print("  3. Run: pacman -S mingw-w64-x86_64-gcc")
    print("")
    print("Option 2 - MinGW-w64:")
    print("  Download from: https://www.mingw-w64.org/")
    print("  Add bin folder to PATH")
    print("")
    print("After installing, run: make")
    return False


def build_ac():
    """Build the AC compiler"""
    print("Building AC compiler...")
    
    # Change to ac-compiler directory
    script_dir = Path(__file__).parent.resolve()
    os.chdir(script_dir)
    
    # Run make
    result = subprocess.run(['make', 'clean'], capture_output=True)
    result = subprocess.run(['make'], capture_output=True, text=True)
    
    if result.returncode != 0:
        print("Build failed!")
        print(result.stderr)
        return False
    
    print("Build successful!")
    return True


def main():
    print("=" * 50)
    print("AC Language Compiler - Automated Installer")
    print("=" * 50)
    print()
    
    # Check if g++ is available
    if not check_gcc():
        print("g++ not found. Installing...")
        
        if sys.platform.startswith('linux'):
            success = install_gcc_linux()
        elif sys.platform == 'darwin':
            success = install_gcc_macos()
        elif sys.platform == 'win32':
            success = install_gcc_windows()
        else:
            print(f"Unsupported platform: {sys.platform}")
            success = False
        
        if not success:
            print("\nInstallation failed. Please install g++ manually and run this script again.")
            sys.exit(1)
    
    # Build the compiler
    if not build_ac():
        print("\nBuild failed. Please check the errors above.")
        sys.exit(1)
    
    print()
    print("=" * 50)
    print("Installation Complete!")
    print("=" * 50)
    print()
    print("Usage:")
    print("  ./ac mycode.ac          # Compile and run")
    print("  ./ac mycode.ac PY       # Compile to Python")
    print("  ./ac mycode.ac BNY      # Compile to native binary")
    print()
    print("Or use the REPL:")
    print("  python3 ac_repl.py")
    print()


if __name__ == "__main__":
    main()
