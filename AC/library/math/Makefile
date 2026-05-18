CXX      = g++
MINGW    = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall
SRC      = math.cpp

.PHONY: all linux windows clean

all: linux

linux: libacmath.so

libacmath.so: $(SRC) math.hpp statistics.hpp calculus.hpp math_c.h
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $(SRC)
	@echo "Built: libacmath.so"

windows: acmath.dll

acmath.dll: $(SRC) math.hpp statistics.hpp calculus.hpp math_c.h
	$(MINGW) $(CXXFLAGS) -shared -static-libgcc -static-libstdc++ -o $@ $(SRC)
	@echo "Built: acmath.dll"

clean:
	rm -f libacmath.so acmath.dll
