add cache like bytecode
add a way to compile into something like exe, .app, or .tar.gz
make GUI library
make the cli run like ac mycode.ac 
make run more cases
make way for users to make own libraries
something like pyaudio inside of AC
AC->BNY
app gui like tkinter1

## Library Import System
- ilib = internal library (ships with AC language)
- elib = external library (user-installed, e.g. apt/pip packages)
- clib = computer library (system-level, already on the machine)

## AC->BNY / .acb Binary Format
- AC->BNY compiles AC code all the way to native machine code saved as a .acb file
- Pipeline: file.ac -> AC->C++ codegen -> g++ -O2 -> native binary -> saved as file.acb
- .acb is a real native binary (not a VM like Java .class), runs directly on the machine
- Since all AC backends are implemented in C++, this is a natural fit
- To implement: add BNY case to main.cpp, run C++ codegen, call g++ with output as file.acb
