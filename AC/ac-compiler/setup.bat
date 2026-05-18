@echo off
REM AC Language Compiler Setup Script for Windows
REM Bundles g++ with the compiler for easy installation

echo === AC Language Compiler Setup ===
echo.

REM Check if g++ is installed
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo g++ not found. Please install MinGW-w64 first:
    echo.
    echo Option 1 - MSYS2 (Recommended):
    echo   1. Download from: https://www.msys2.org/
    echo   2. Install and open MSYS2 terminal
    echo   3. Run: pacman -S mingw-w64-x86_64-gcc
    echo.
    echo Option 2 - MinGW-w64:
    echo   Download from: https://www.mingw-w64.org/
    echo   Add bin folder to PATH
    echo.
    echo After installing g++, run this script again.
    pause
    exit /b 1
)

echo g++ found. Building AC compiler...
echo.

cd /d "%~dp0"
make clean
make

if %errorlevel% neq 0 (
    echo.
    echo Build failed. Please check the errors above.
    pause
    exit /b 1
)

echo.
echo === Setup Complete ===
echo.
echo Usage:
echo   ac mycode.ac          ^# Compile and run
echo   ac mycode.ac PY       ^# Compile to Python
echo   ac mycode.ac BNY      ^# Compile to native binary
echo.
echo Or use the REPL:
echo   python ac_repl.py
echo.
pause
