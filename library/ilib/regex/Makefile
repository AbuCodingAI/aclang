CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -fPIC -Wall -Wextra
SRC      := regex.cpp
OBJ      := regex.o

# Detect platform
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    TARGET  := libacregex.so
    LDFLAGS := -shared -Wl,-soname,$(TARGET)
else ifeq ($(UNAME),Darwin)
    TARGET  := libacregex.dylib
    LDFLAGS := -shared -dynamiclib
else
    TARGET  := acregex.dll
    CXX     := x86_64-w64-mingw32-g++
    LDFLAGS := -shared -static-libgcc -static-libstdc++
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ): $(SRC) regex.hpp regex_c.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) libacregex.so libacregex.dylib acregex.dll
