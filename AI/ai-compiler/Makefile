CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra
INCLUDES = -I include -I ../ai-vm/include
LDFLAGS  = -ldl

# Compiler sources + ai-vm runtime sources (linked into the 'ai' binary)
VM_SRC  = ../ai-vm/src/dec.cpp ../ai-vm/src/dec_heap.cpp ../ai-vm/src/interpreter.cpp
SRC     = src/main.cpp src/ai_codegen.cpp src/lexer.cpp src/parser.cpp $(VM_SRC)
BIN     = ai

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN) src/*.o
