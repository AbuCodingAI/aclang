CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11

.PHONY: all clean

all: libdatac.a datac_example

datac.o: datac.c datac.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Static library — link with: gcc yourfile.c -L. -ldatac
libdatac.a: datac.o
	ar rcs $@ $^

# Shared library — link with: gcc yourfile.c -L. -ldatac -Wl,-rpath,.
libdatac.so: datac.o
	$(CC) -shared -fPIC -o $@ $^

datac_example: datac_example.c datac.o datac.h
	$(CC) $(CFLAGS) -o $@ datac_example.c datac.o

clean:
	rm -f *.o *.a *.so datac_example
