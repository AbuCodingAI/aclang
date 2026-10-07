CXX = g++
CXXFLAGS = -std=c++17 -fPIC -O2 -Wall

TARGET = libacweb.so
SRC = web.cpp
OBJ = web.o

# web is one ilib with two halves: the client (libacweb, this directory) and the
# server (server/, libacserver — `from ilib web use web-server`). Both build from here.
all: $(TARGET)
	$(MAKE) -C server

$(TARGET): $(OBJ)
	$(CXX) -shared -o $@ $^

$(OBJ): $(SRC) web.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
	$(MAKE) -C server clean

.PHONY: all clean
