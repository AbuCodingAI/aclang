CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

SRC = src/main.cpp src/lexer.cpp src/parser.cpp src/codegen_py.cpp src/codegen_js.cpp src/codegen_html.cpp src/codegen_java.cpp src/codegen_cpp.cpp src/codegen_c.cpp src/codegen_asm.cpp
OUT = ac

all: $(OUT)

$(OUT):
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

install: all
	sudo cp $(OUT) /usr/local/bin/ac

clean:
	rm -f $(OUT)
