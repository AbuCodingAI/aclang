CXX      = g++
MINGW    = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall
SRC      = string_cheese_c.cpp

.PHONY: all linux windows clean

all: linux

linux: libacstringcheese.so

libacstringcheese.so: $(SRC) string_cheese_c.h
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $(SRC)
	@echo "Built: libacstringcheese.so"

windows: acstringcheese.dll

acstringcheese.dll: $(SRC) string_cheese_c.h
	$(MINGW) $(CXXFLAGS) -shared -static-libgcc -static-libstdc++ -o $@ $(SRC)
	@echo "Built: acstringcheese.dll"

clean:
	rm -f libacstringcheese.so acstringcheese.dll
