#!/bin/bash
# Cross-builds the ilib shared libraries for AArch64 into <ilib>/arm64/ (x86-64 builds are untouched).
# Used by the ARM backend's dynamic-link path. Requires aarch64-linux-gnu-g++ and its sysroot.
#
# Skipped, because the AArch64 development libraries they link against are not installed here:
#   aczip (zlib), machine-audio (libmpg123 + ALSA), widgets (GTK 3), camera (OpenCV).
set -u
cd "$(dirname "$0")"
CXX=aarch64-linux-gnu-g++
CF="-std=c++17 -O2 -fPIC -w"
fail=0
build() { # build <dir> <outname> <command...>
    local dir=$1 out=$2; shift 2
    mkdir -p "$dir/arm64"
    if "$@" -o "$dir/arm64/$out"; then echo "built $dir/arm64/$out"
    else echo "FAILED $dir/$out"; fail=1; fi
}
build math libacmath.so $CXX $CF -shared math/math.cpp
build os libacoos.so $CXX $CF -shared os/os_c.cpp
build regex libacregex.so $CXX $CF -shared regex/regex.cpp
build string-cheese libacstringcheese.so $CXX $CF -shared string-cheese/string_cheese_c.cpp
build native-cpu libacncpu.so $CXX $CF -shared native-cpu/native-cpu.cpp
build ml libacml.so $CXX $CF -shared ml/ml.cpp
build web libacweb.so $CXX $CF -shared web/web.cpp
build web libacserver.so $CXX $CF "-DAC_WEBSERVER_DIR=\"$PWD/web/server\"" -shared web/server/web-server.cpp
exit $fail
