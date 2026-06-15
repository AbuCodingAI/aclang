CXX      = g++
MINGW    = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall
SRC      = os_c.cpp

.PHONY: all linux windows clean

all: linux

linux: libacoos.so

libacoos.so: $(SRC) os_c.h
	$(CXX) $(CXXFLAGS) -shared -fPIC -o $@ $(SRC)
	@echo "Built: libacoos.so"

windows: acoos.dll

acoos.dll: $(SRC) os_c.h
	$(MINGW) $(CXXFLAGS) -shared -static-libgcc -static-libstdc++ -o $@ $(SRC)
	@echo "Built: acoos.dll"

clean:
	rm -f libacoos.so acoos.dll
