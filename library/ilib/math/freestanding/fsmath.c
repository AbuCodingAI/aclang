/* Freestanding integer-math pilot for BNY Option-2 static linking.
 *
 * This core is compiled with -ffreestanding -nostdlib -fno-stack-protector and uses
 * NOTHING from libc / libm / libstdc++ — no <stdlib.h>, no <string.h>, no exceptions.
 * The result is a relocatable object with ZERO undefined external symbols, so the BNY
 * backend's in-process object splicer can drop its .text straight into the binary it
 * writes, resolve the call sites internally, and emit ONE standalone ELF that needs no
 * dynamic loader and no .so — without BNY ever invoking gcc or ld.
 *
 * Only integer operations live here (they need no transcendental libm helpers, which is
 * why this is the pilot). Float/transcendental and string ilibs follow the same recipe
 * once the splicer is proven — see HANDOFF.md "Option 2 rollout".
 *
 * Exported names match the BNY ext-symbol convention (ac_* ) so existing em.call("math.X")
 * sites resolve to the spliced address instead of a PLT/GOT stub.
 */

/* --- integer helpers, all self-contained --------------------------------- */

long long ac_abs_int(long long x) {
    return x < 0 ? -x : x;
}

long long ac_max_int(long long a, long long b) {
    return a > b ? a : b;
}

long long ac_min_int(long long a, long long b) {
    return a < b ? a : b;
}

/* floor modulo — sign of divisor, matches AC's math.mod (guards divide-by-zero) */
long long ac_mod_int(long long a, long long b) {
    if (b == 0) return 0;
    long long r = a % b;
    if ((r != 0) && ((r < 0) != (b < 0))) r += b;
    return r;
}

/* floor integer division — matches AC's `//` */
long long ac_idiv_int(long long a, long long b) {
    if (b == 0) return 0;
    long long q = a / b;
    long long r = a % b;
    if ((r != 0) && ((r < 0) != (b < 0))) q -= 1;
    return q;
}

long long ac_ipow_int(long long base, long long exp) {
    long long r = 1;
    while (exp > 0) { r *= base; exp -= 1; }
    return r;
}

long long ac_gcd(long long a, long long b) {
    a = ac_abs_int(a);
    b = ac_abs_int(b);
    while (b != 0) { long long t = b; b = a % b; a = t; }
    return a;
}

long long ac_lcm(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    long long g = ac_gcd(a, b);
    return ac_abs_int(a / g * b);
}

long long ac_is_prime(long long n) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if ((n % 2) == 0) return 0;
    for (long long i = 3; i * i <= n; i += 2)
        if ((n % i) == 0) return 0;
    return 1;
}
