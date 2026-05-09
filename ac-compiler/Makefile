CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

SRC = src/main.cpp src/lexer.cpp src/parser.cpp src/backend_registry.cpp src/ir.cpp src/ir_codegen.cpp src/exp_bny.cpp
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
