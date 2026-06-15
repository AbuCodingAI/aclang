CXX      = g++
CXXFLAGS = -std=c++20 -O2 -Wall
INCLUDES = -I include -I ../ai-compiler/include
LDFLAGS  = -ldl

SRC = src/main.cpp src/dec.cpp src/dec_heap.cpp src/interpreter.cpp
BIN = aivm

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN) src/*.o
