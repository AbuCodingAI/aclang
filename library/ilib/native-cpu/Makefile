# AC native-cpu (ncpu) library Makefile
# Builds libacncpu.so/.a from native-cpu.cpp — pointer registry, allocator,
# arena allocator, hard abort, quickthread messaging.

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

TARGET_SO = libacncpu.so
TARGET_A  = libacncpu.a

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): native-cpu.o
	$(CXX) -shared -o $@ $^

$(TARGET_A): native-cpu.o
	ar rcs $@ $^

native-cpu.o: native-cpu.cpp native-cpu_c.h
	$(CXX) $(CXXFLAGS) -fPIC -c native-cpu.cpp -o $@

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)

.PHONY: all clean
