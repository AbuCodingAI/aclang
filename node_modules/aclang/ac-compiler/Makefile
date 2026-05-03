CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

SRC = src/main.cpp src/lexer.cpp src/parser.cpp src/backend_registry.cpp src/ir.cpp src/codegen_py.cpp src/codegen_js.cpp src/codegen_html.cpp src/codegen_java.cpp src/codegen_cpp.cpp src/codegen_c.cpp src/codegen_asm.cpp src/codegen_rs.cpp src/codegen_go.cpp src/codegen_v.cpp src/codegen_bny.cpp
OUT = ac

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

install: all
ifeq ($(OS),Windows_NT)
	cp $(OUT) /usr/local/bin/ac
else
	sudo cp $(OUT) /usr/local/bin/ac
endif

clean:
ifeq ($(OS),Windows_NT)
	rm -f $(OUT).exe
else
	rm -f $(OUT)
endif
