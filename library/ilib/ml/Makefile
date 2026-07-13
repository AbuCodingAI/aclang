CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -fPIC -Wall -Wextra
SRC      := ml.cpp
OBJ      := ml.o

UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    TARGET  := libacml.so
    LDFLAGS := -shared -Wl,-soname,$(TARGET)
else ifeq ($(UNAME),Darwin)
    TARGET  := libacml.dylib
    LDFLAGS := -shared -dynamiclib
else
    TARGET  := acml.dll
    CXX     := x86_64-w64-mingw32-g++
    LDFLAGS := -shared -static-libgcc -static-libstdc++
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ): $(SRC) libacml.h ml_c.h ml.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) libacml.so libacml.dylib acml.dll
