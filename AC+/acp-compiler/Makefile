CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCLUDES = -Iinclude
SRC      = src/lexer.cpp src/parser.cpp src/codegen.cpp src/main.cpp
TARGET   = acp

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/acp

clean:
	rm -f $(TARGET)
