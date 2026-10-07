// Path of the running compiler binary. Used to find `library/` next to it (<exe dir>/../library).
#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef __COSMOPOLITAN__
// Declared by hand: <cosmo.h> pulls in a global `SymbolTable` that collides with AC_IR::SymbolTable.
extern "C" char* GetProgramExecutableName(void);
#endif
#ifndef _WIN32
#include <unistd.h>
#endif

// Writes the real path of this executable into buf (at most cap bytes, not NUL-terminated by the caller's
// contract — same shape as readlink). Returns the length, or -1 if it can't be determined.
inline long acSelfExe(char* buf, size_t cap) {
#ifdef __COSMOPOLITAN__
    // A Cosmopolitan (APE) binary is started by a loader, so /proc/self/exe is the LOADER (~/.ape-1.x), not
    // ac.com — and the file may be launched through a symlink. Ask cosmopolitan for the program path and
    // resolve it.
    if (const char* p = GetProgramExecutableName()) {
        char full[4096];
        const char* r = realpath(p, full) ? full : p;
        size_t n = std::strlen(r);
        if (n > 0 && n <= cap) { std::memcpy(buf, r, n); return (long)n; }
    }
#endif
#ifndef _WIN32
    return (long)readlink("/proc/self/exe", buf, cap);
#else
    return -1;
#endif
}

// Modification time of the running compiler binary (0 if unknown). A cache written by an older build
// of the compiler is stale: the same source can lower or parse differently after a rebuild.
#include <ctime>
#include <sys/stat.h>
inline time_t acCompilerMtime() {
    char buf[4096] = {};
    long n = acSelfExe(buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    struct stat st{};
    if (stat(std::string(buf, (size_t)n).c_str(), &st) != 0) return 0;
    return st.st_mtime;
}
