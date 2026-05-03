@echo off
:: AC Compiler wrapper script for global npm installation (Windows)

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

:: Find the ac-compiler directory (go up from bin/ to project root)
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI"
set "AC_COMPILER_DIR=%PROJECT_ROOT%\ac-compiler"

:: Run the AC compiler
"%AC_COMPILER_DIR%\ac.exe" %*
