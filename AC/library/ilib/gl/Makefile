CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -fPIC -Wall -Wextra
SRC      := gl.cpp
OBJ      := gl.o

UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    TARGET      := libacgl.so
    SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || echo "-I/usr/include/SDL2 -D_REENTRANT")
    SDL2_LIBS   := $(shell sdl2-config --libs   2>/dev/null || echo "-lSDL2")
    LDFLAGS     := -shared -Wl,-soname,$(TARGET)
else ifeq ($(UNAME),Darwin)
    TARGET      := libacgl.dylib
    SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || echo "-I/usr/local/include/SDL2")
    SDL2_LIBS   := $(shell sdl2-config --libs   2>/dev/null || echo "-lSDL2")
    LDFLAGS     := -shared -dynamiclib
else
    TARGET      := acgl.dll
    CXX         := x86_64-w64-mingw32-g++
    SDL2_CFLAGS := -I/usr/x86_64-w64-mingw32/include/SDL2
    SDL2_LIBS   := -lSDL2 -lSDL2main
    LDFLAGS     := -shared -static-libgcc -static-libstdc++
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SDL2_LIBS)

$(OBJ): $(SRC) gl.hpp gl_c.h
	$(CXX) $(CXXFLAGS) $(SDL2_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) libacgl.so libacgl.dylib acgl.dll
