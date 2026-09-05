# AC ACZip Library Makefile
# Builds libaczip.so / libaczip.a from aczip.cpp (C++ implementation) +
# aczip_c.cpp (extern "C" shim the FFI bindings actually link against).

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
ZLIB_FLAGS = $(shell pkg-config --cflags --libs zlib)

INCLUDES = -I.
LIBS = $(ZLIB_FLAGS) -lpthread

# Targets
TARGET_SO = libaczip.so
TARGET_A  = libaczip.a

all: $(TARGET_SO) $(TARGET_A)

# The shared/static libs AC's compiled backends actually link against —
# aczip.cpp owns the real C++ implementation (ACZip/ACTar/ACGzip/ACXz/ACZstd),
# and aczip_c.cpp's extern "C" shim wraps it as ac_zip_compress / ac_zip_decompress
# / ac_zip_compress_hdd / ac_zip_compress_sata / ac_get_compression_ratio /
# ac_free_bytes — the plain C symbols the Go/Rust/V/ASM/Java FFI files expect.
$(TARGET_SO): aczip.o aczip_c.o
	$(CXX) -shared -o $@ $^ $(LIBS)

$(TARGET_A): aczip.o aczip_c.o
	ar rcs $@ $^

aczip.o: aczip.cpp aczip.hpp
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) -c aczip.cpp -o $@ $(LIBS)

aczip_c.o: aczip_c.cpp aczip_c.h aczip.hpp
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) -c aczip_c.cpp -o $@ $(LIBS)

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)

install:
	@echo "ACZip library needs zlib. Install with:"
	@echo "  Ubuntu/Debian: sudo apt-get install zlib1g-dev"
	@echo "  macOS: brew install zlib"
	@echo "The xz/zstd/tar backends (ACXz/ACZstd/ACTar) shell out to the"
	@echo "xz/zstd/tar command-line tools at runtime — make sure they're on PATH."

.PHONY: all clean install
