# Freestanding integer-math pilot for BNY Option-2 static linking.
#
# Produces a relocatable object (fsmath.o) with ZERO undefined external symbols, built
# WITHOUT any hosted headers or runtime. BNY splices this object at AC-compile time; it
# never runs gcc/ld itself. This Makefile is run once, ahead of time, when the ilib is built.
#
#   make            -> fsmath.o  (+ verifies zero undefined externals)
#   make verify     -> print the object's undefined symbols (should be empty)
#   make clean

CC     = gcc
# -ffreestanding: no hosted-environment assumptions (no libc semantics for main, etc.)
# -nostdlib is a LINK flag; for a .o we just avoid all libc/libm/libstdc++ headers + symbols.
# -fno-stack-protector / -fno-asynchronous-unwind-tables: drop __stack_chk_fail and .eh_frame
#   pulls so the object stays self-contained. -fcf-protection=none drops __CET markers.
CFLAGS = -ffreestanding -O2 -fno-stack-protector -fno-asynchronous-unwind-tables \
         -fcf-protection=none -fno-pic -fno-plt -Wall

.PHONY: all verify clean

all: fsmath.o verify

fsmath.o: fsmath.c
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "Built: fsmath.o"

# The whole point of Option 2: the object must have NO undefined external symbols.
# 'U' lines from nm = undefined = would need a .so/libc at link time. There must be none.
verify: fsmath.o
	@u=$$(nm fsmath.o 2>/dev/null | grep ' U ' || true); \
	if [ -n "$$u" ]; then \
	  echo "FAIL: fsmath.o has undefined externals (not freestanding):"; echo "$$u"; exit 1; \
	else \
	  echo "OK: fsmath.o is freestanding (zero undefined external symbols)"; \
	fi

clean:
	rm -f fsmath.o
