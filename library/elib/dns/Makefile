CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -fPIC
.PHONY: all linux clean
all: linux
linux: libacdns.so libacdns.a
libacdns.so: dns.cpp dns_c.h
	$(CXX) $(CXXFLAGS) -shared -o $@ dns.cpp
libacdns.a: dns.cpp dns_c.h
	$(CXX) $(CXXFLAGS) -c dns.cpp -o dns.o
	ar rcs $@ dns.o
clean:
	rm -f dns.o libacdns.so libacdns.a
