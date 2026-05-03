@echo off
:: AC Compiler wrapper script for global npm installation (Windows)

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

:: The ac binary is in the parent directory (npm flattens the structure)
for %%I in ("%SCRIPT_DIR%..") do set "AC_COMPILER_DIR=%%~fI"
"%AC_COMPILER_DIR%\ac-compiler\ac.exe" %*
