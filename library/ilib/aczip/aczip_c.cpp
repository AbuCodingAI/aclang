// extern "C" shim over the C++ aczip:: classes (ACZip et al.). This is the
// layer the FFI bindings (aczip_ffi.go / .rs / .v / .asm / ACZip.java) actually
// link against — aczip.cpp only ever exported C++-mangled symbols
// (_ZN5aczip5ACZip8compressE...), so none of those 5 backends could resolve
// ac_zip_compress / ac_zip_decompress / ac_free_bytes etc. before this file
// existed. Follows the same pattern as camera_c.cpp: a thin extern "C" layer
// that calls into the real C++ implementation and owns the malloc/free
// convention for handing binary (non-null-terminated) buffers to callers
// across the C ABI boundary.
#include "aczip_c.h"
#include "aczip.hpp"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>

using namespace aczip;

namespace {

// Copy a std::vector<uint8_t> into a malloc'd buffer for return-by-value
// across the C ABI. Binary data isn't null-terminated, so (pointer, size)
// plus an explicit ac_free_bytes() is the only safe convention here —
// unlike web.cpp's static-buffer-of-chars pattern, which only works for
// caller-immediately-consumed C strings.
ACZipByteArray to_bytearray(const std::vector<uint8_t>& v) {
    ACZipByteArray out{nullptr, 0};
    if (v.empty()) return out;
    out.data = static_cast<char*>(std::malloc(v.size()));
    if (!out.data) return ACZipByteArray{nullptr, 0};
    std::memcpy(out.data, v.data(), v.size());
    out.size = v.size();
    return out;
}

} // namespace

extern "C" {

ACZipByteArray ac_zip_compress(const char* path, int parallel) {
    if (!path) return ACZipByteArray{nullptr, 0};
    try {
        return to_bytearray(ACZip::compress(path, parallel != 0));
    } catch (...) {
        return ACZipByteArray{nullptr, 0};
    }
}

int ac_zip_decompress(const unsigned char* data, size_t size, const char* output_path) {
    if (!data || !output_path) return -1;
    try {
        std::vector<uint8_t> buf(data, data + size);
        ACZip::decompress(buf, output_path);
        return 0;
    } catch (...) {
        return -1;
    }
}

ACZipByteArray ac_zip_compress_hdd(const char* path) {
    if (!path) return ACZipByteArray{nullptr, 0};
    try {
        return to_bytearray(ACZip::compress_hdd(path));
    } catch (...) {
        return ACZipByteArray{nullptr, 0};
    }
}

ACZipByteArray ac_zip_compress_sata(const char* path) {
    if (!path) return ACZipByteArray{nullptr, 0};
    try {
        return to_bytearray(ACZip::compress_sata(path));
    } catch (...) {
        return ACZipByteArray{nullptr, 0};
    }
}

double ac_get_compression_ratio(size_t original, size_t compressed) {
    return ACZip::get_ratio(original, compressed);
}

void ac_free_bytes(ACZipByteArray arr) {
    std::free(arr.data);
}

// File-to-file convenience wrappers: AC has no raw byte-buffer type at the language
// level, so every FFI binding (PY/JS/C/CPP/Rust/Go/V/Java/ASM) calls through these
// instead of individually marshaling ACZipByteArray across each language's own FFI —
// one C++ implementation of "write compressed bytes to a file" / "read a file, hand its
// bytes to decompress" instead of N near-duplicate ones.
long long ac_zip_compress_to_file(const char* path, int parallel, const char* output_path) {
    if (!path || !output_path) return -1;
    try {
        std::vector<uint8_t> data = ACZip::compress(path, parallel != 0);
        std::ofstream f(output_path, std::ios::binary);
        if (!f) return -1;
        f.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        return f ? (long long)data.size() : -1;
    } catch (...) {
        return -1;
    }
}

int ac_zip_decompress_from_file(const char* archive_path, const char* output_path) {
    if (!archive_path || !output_path) return -1;
    try {
        std::ifstream f(archive_path, std::ios::binary);
        if (!f) return -1;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        ACZip::decompress(data, output_path);
        return 0;
    } catch (...) {
        return -1;
    }
}


// ── packaging: an ISO image or a .tar.gz from a directory ────────────────────
// Both run an external tool with execvp (argv arrays, no shell). Return 0 on success, -1 on failure.
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
static int _run_tool(const std::vector<std::string>& argv) {
    std::vector<char*> av;
    for (auto& a : argv) av.push_back(const_cast<char*>(a.c_str()));
    av.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(av[0], av.data());
        _exit(127);                     // the tool is not installed
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0 || !WIFEXITED(st)) return -1;
    return WEXITSTATUS(st) == 0 ? 0 : -1;
}
static bool _tool_exists(const char* name) {
    std::vector<std::string> probe = {name, "--version"};
    std::vector<char*> av;
    for (auto& a : probe) av.push_back(const_cast<char*>(a.c_str()));
    av.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); }
        execvp(av[0], av.data());
        _exit(127);
    }
    int st = 0;
    return waitpid(pid, &st, 0) >= 0 && WIFEXITED(st) && WEXITSTATUS(st) != 127;
}
#endif

// Writes an ISO 9660 image of srcdir to out_iso (Rock Ridge + Joliet, so long names survive).
// Uses xorriso, then genisoimage, then mkisofs — whichever is installed first.
int ac_zip_iso(const char* srcdir, const char* out_iso, const char* label) {
    if (!srcdir || !out_iso) return -1;
#ifdef _WIN32
    return -1;
#else
    std::string vol = (label && *label) ? label : "ACZIP";
    if (_tool_exists("xorriso"))
        return _run_tool({"xorriso", "-as", "mkisofs", "-quiet", "-r", "-J", "-V", vol, "-o", out_iso, srcdir});
    if (_tool_exists("genisoimage"))
        return _run_tool({"genisoimage", "-quiet", "-r", "-J", "-V", vol, "-o", out_iso, srcdir});
    if (_tool_exists("mkisofs"))
        return _run_tool({"mkisofs", "-quiet", "-r", "-J", "-V", vol, "-o", out_iso, srcdir});
    return -1;
#endif
}

// Writes a gzip-compressed tar of srcdir to out_path (.tar.gz).
int ac_zip_package(const char* srcdir, const char* out_path) {
    if (!srcdir || !out_path) return -1;
#ifdef _WIN32
    return -1;
#else
    // -C changes into the parent, so the archive holds the directory's own name, not the whole path
    std::string s(srcdir);
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    size_t cut = s.find_last_of('/');
    std::string parent = cut == std::string::npos ? "." : (cut == 0 ? "/" : s.substr(0, cut));
    std::string name = cut == std::string::npos ? s : s.substr(cut + 1);
    return _run_tool({"tar", "-czf", out_path, "-C", parent, name});
#endif
}

} // extern "C"
