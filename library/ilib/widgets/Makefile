CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -fPIC -Wall -Wextra
SRC      := widgets.cpp
OBJ      := widgets.o

UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Linux)
    TARGET      := libacwidgets.so
    GTK_CFLAGS  := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "-I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/atk-1.0 -I/usr/include/gdk-pixbuf-2.0")
    GTK_LIBS    := $(shell pkg-config --libs gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lglib-2.0 -lgobject-2.0")
    LDFLAGS     := -shared -Wl,-soname,$(TARGET)
else ifeq ($(UNAME),Darwin)
    TARGET      := libacwidgets.dylib
    GTK_CFLAGS  := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
    GTK_LIBS    := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
    LDFLAGS     := -shared -dynamiclib
else
    TARGET      := acwidgets.dll
    CXX         := x86_64-w64-mingw32-g++
    GTK_CFLAGS  := -I/usr/x86_64-w64-mingw32/include/gtk-3.0
    GTK_LIBS    := -lgtk-3 -lgdk-3 -lglib-2.0 -lgobject-2.0
    LDFLAGS     := -shared -static-libgcc -static-libstdc++
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(GTK_LIBS)

$(OBJ): $(SRC) widgets_c.h
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) libacwidgets.so libacwidgets.dylib acwidgets.dll
