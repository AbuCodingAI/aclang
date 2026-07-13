CXX = g++
CXXFLAGS = -std=c++17 -fPIC -O2 -Wall

TARGET = libacweb.so
SRC = web.cpp
OBJ = web.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -shared -o $@ $^

$(OBJ): $(SRC) web.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
