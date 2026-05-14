// AC Standard Runtime Header — v0.2
// Single include for all AC-generated C/C++ programs.
// Place this file on your include path (-I path/to/library) and use:
//   #include "ac_standard.hpp"
// instead of individual system headers.

#ifndef AC_STANDARD_HPP
#define AC_STANDARD_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>
#include <sstream>

// AC integer and string type aliases
typedef long long   ac_int;
typedef const char* ac_str;

// Print helpers (mirrors what the AC->C backend emits)
inline void ac_print(ac_int v)   { printf("%lld\n", v); }
inline void ac_print(double v)   { printf("%.16g\n", v); }
inline void ac_print(const char* v) { printf("%s\n", v); }
inline void ac_print(bool v)     { printf("%s\n", v ? "true" : "false"); }

#endif // AC_STANDARD_HPP
