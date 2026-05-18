@echo off
:: AC Compiler wrapper script for global npm installation (Windows)

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

:: npm installs to %APPDATA%\npm\node_modules\aclang\
:: The ac binary is at %APPDATA%\npm\node_modules\aclang\ac-compiler\ac.exe
set "AC_COMPILER_DIR=%APPDATA%\npm\node_modules\aclang"
"%AC_COMPILER_DIR%\ac-compiler\ac.exe" %*
