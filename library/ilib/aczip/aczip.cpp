#include "aczip.hpp"
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

namespace fs = std::filesystem;
using namespace aczip;

// ============================================================================
// SUBPROCESS HELPERS (no shell) — replaces popen()/system() string-building.
//
// The old code built commands like `"tar -xf - -C \"" + output_path + "\""`
// and handed them to popen()/system(), which runs them through /bin/sh -c.
// Any path or argument containing shell metacharacters (`;`, `$(...)`,
// backticks, quotes, etc.) could inject arbitrary commands. These helpers
// fork()+execvp() the target binary directly with an argv array, so
// arguments are never re-parsed by a shell — matching the pattern already
// used by web.cpp's open_url().
// ============================================================================

namespace {

std::vector<char*> to_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    return argv;
}

void check_status(int status, const std::string& prog) {
    if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0))
        throw std::runtime_error(prog + " failed");
}

// Run argv, feed `input` (may be nullptr/0) to its stdin, capture all stdout.
std::vector<uint8_t> run_capture(const std::vector<std::string>& args,
                                  const uint8_t* input, size_t input_len) {
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) != 0) throw std::runtime_error("pipe() failed");
    if (pipe(out_pipe) != 0) throw std::runtime_error("pipe() failed");

    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork() failed");

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        auto argv = to_argv(args);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    // Feed stdin from a writer thread: with large input this avoids a
    // deadlock where the child blocks writing stdout (full pipe) while we're
    // still blocked writing its stdin.
    std::thread writer([&]{
        size_t written = 0;
        while (input && written < input_len) {
            ssize_t n = write(in_pipe[1], input + written, input_len - written);
            if (n <= 0) break;
            written += (size_t)n;
        }
        close(in_pipe[1]);
    });

    std::vector<uint8_t> output;
    char buf[4096];
    ssize_t n;
    while ((n = read(out_pipe[0], buf, sizeof(buf))) > 0)
        output.insert(output.end(), buf, buf + n);
    close(out_pipe[0]);
    writer.join();

    int status = 0;
    waitpid(pid, &status, 0);
    check_status(status, args.empty() ? "subprocess" : args[0]);
    return output;
}

// Run argv, feeding `input` to stdin, but with no shell redirection —
// used where the old code piped a command's stdin from data already in memory.
void run_argv_with_stdin(const std::vector<std::string>& args,
                          const uint8_t* input, size_t input_len) {
    run_capture(args, input, input_len);
}

// Run argv with stdout redirected to output_path (replaces `cmd > "file"`).
void run_argv_to_file(const std::vector<std::string>& args, const std::string& output_path) {
    int fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) throw std::runtime_error("Failed to open output file: " + output_path);

    pid_t pid = fork();
    if (pid < 0) { close(fd); throw std::runtime_error("fork() failed"); }
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        close(fd);
        auto argv = to_argv(args);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fd);
    int status = 0;
    waitpid(pid, &status, 0);
    check_status(status, args.empty() ? "subprocess" : args[0]);
}

} // namespace

// ============================================================================
// ACZIP IMPLEMENTATION - OPTIMIZED FOR SPEED
// ============================================================================

// Archive format v2 (self-consistent, filenames preserved, single zlib codec):
//   magic 'A','C','Z','2' | file_count:u32 |
//   per file: path_len:u32 | path bytes | orig_size:u32 | comp_size:u32 | zlib(data)
// (The old version compressed with zstd but decompressed with zlib, wrote an 8-byte
//  header but read a 12-byte one, lost all filenames, raced, and could div-by-zero.)
static void put32(std::vector<uint8_t>& v, uint32_t x) {
    uint8_t b[4]; std::memcpy(b, &x, 4); v.insert(v.end(), b, b + 4);
}

std::vector<uint8_t> ACZip::compress(const std::string& path, bool parallel) {
    Archive archive = build_archive(path);

    // Compress each file's data (zlib) into a per-index slot. Parallel writes go to
    // distinct indices (no shared push_back), so there is no data race.
    std::vector<std::vector<uint8_t>> comp(archive.files.size());
    if (parallel && archive.files.size() > 1) {
        unsigned hw = std::thread::hardware_concurrency();
        unsigned nt = hw ? std::min<unsigned>(hw, (unsigned)archive.files.size()) : 1;  // guard 0
        std::atomic<size_t> next{0};
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < nt; t++)
            pool.emplace_back([&]{
                size_t i;
                while ((i = next.fetch_add(1)) < archive.files.size())
                    comp[i] = ACGzip::compress(archive.files[i].data, 6);
            });
        for (auto& t : pool) t.join();
    } else {
        for (size_t i = 0; i < archive.files.size(); i++)
            comp[i] = ACGzip::compress(archive.files[i].data, 6);
    }

    std::vector<uint8_t> result{'A', 'C', 'Z', '2'};
    put32(result, (uint32_t)archive.files.size());
    for (size_t i = 0; i < archive.files.size(); i++) {
        const std::string& p = archive.files[i].path;
        put32(result, (uint32_t)p.size());
        result.insert(result.end(), p.begin(), p.end());
        put32(result, (uint32_t)archive.files[i].data.size());   // orig size
        put32(result, (uint32_t)comp[i].size());                 // comp size
        result.insert(result.end(), comp[i].begin(), comp[i].end());
    }
    return result;
}

void ACZip::decompress(const std::vector<uint8_t>& data, const std::string& output_path) {
    if (data.size() < 8 || data[0] != 'A' || data[1] != 'C' || data[2] != 'Z' || data[3] != '2')
        throw std::runtime_error("Invalid ACZip file");

    size_t pos = 4;
    auto need = [&](size_t n) { if (pos + n > data.size()) throw std::runtime_error("Corrupt archive: truncated"); };
    auto rd32 = [&]() -> uint32_t { need(4); uint32_t v; std::memcpy(&v, data.data() + pos, 4); pos += 4; return v; };

    uint32_t file_count = rd32();
    for (uint32_t i = 0; i < file_count; i++) {
        uint32_t path_len = rd32();
        need(path_len);
        std::string rel((const char*)data.data() + pos, path_len); pos += path_len;
        uint32_t orig_size = rd32();
        uint32_t comp_size = rd32();
        need(comp_size);
        std::vector<uint8_t> compressed(data.begin() + pos, data.begin() + pos + comp_size); pos += comp_size;

        auto decompressed = ACGzip::decompress(compressed);
        if (orig_size && decompressed.size() != orig_size)
            throw std::runtime_error("Corrupt archive: size mismatch for " + rel);

        // Sanitize path: strip absolute prefix and any ".." so extraction can never
        // escape output_path (zip-slip protection).
        std::string safe;
        for (const auto& part : fs::path(rel).lexically_normal()) {
            std::string s = part.string();
            if (s == ".." || s == "/" || s == "\\" || s.empty()) continue;
            safe += (safe.empty() ? "" : "/") + s;
        }
        if (safe.empty()) safe = "file_" + std::to_string(i);

        fs::path file_path = fs::path(output_path) / safe;
        fs::create_directories(file_path.parent_path());
        std::ofstream out(file_path, std::ios::binary);
        out.write((const char*)decompressed.data(), decompressed.size());
    }
}

std::vector<uint8_t> ACZip::compress_hdd(const std::string& path) {
    // Sequential compression, less random access
    return compress(path, false);
}

std::vector<uint8_t> ACZip::compress_sata(const std::string& path) {
    // Balanced compression
    return compress(path, true);
}

double ACZip::get_ratio(size_t original, size_t compressed) {
    if (original == 0) return 0.0;
    return (compressed * 100.0) / original;
}

Archive ACZip::build_archive(const std::string& path) {
    Archive archive;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());

            FileEntry fe;
            fe.path = entry.path().relative_path().string();
            fe.data = std::move(data);          // move the (possibly large) file buffer, don't copy

            archive.files.push_back(std::move(fe));
        }
    }

    return archive;
}

// (generate_tag removed — the v2 format preserves real filenames, so tags are gone;
//  the old buf[10] also truncated the folder-tag format.)

// ============================================================================
// TAR IMPLEMENTATION
// ============================================================================

std::vector<uint8_t> ACTar::create(const std::string& path) {
    return run_capture({"tar", "-cf", "-", path}, nullptr, 0);
}

void ACTar::extract(const std::vector<uint8_t>& data, const std::string& output_path) {
    run_argv_with_stdin({"tar", "-xf", "-", "-C", output_path}, data.data(), data.size());
}

// ============================================================================
// GZIP IMPLEMENTATION
// ============================================================================

std::vector<uint8_t> ACGzip::compress(const std::vector<uint8_t>& data, int level) {
    std::vector<uint8_t> compressed;
    compressed.resize(compressBound(data.size()));

    uLongf compressed_size = compressed.size();
    int result = compress2(compressed.data(), &compressed_size,
                          data.data(), data.size(), level);

    if (result != Z_OK) {
        throw std::runtime_error("Gzip compression failed");
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::vector<uint8_t> ACGzip::decompress(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};
    // Grow the output buffer until it fits — robust for any compression ratio
    // (the old version only doubled once, so anything better than ~8:1 failed).
    size_t cap = data.size() * 4 + 64;
    for (int attempt = 0; attempt < 32; attempt++) {
        std::vector<uint8_t> out(cap);
        uLongf out_size = (uLongf)cap;
        int r = uncompress(out.data(), &out_size, data.data(), data.size());
        if (r == Z_OK) { out.resize(out_size); return out; }
        if (r == Z_BUF_ERROR) { cap *= 2; continue; }
        throw std::runtime_error("Gzip decompression failed");
    }
    throw std::runtime_error("Gzip decompression: output exceeds limit");
}

// ============================================================================
// XZ COMPRESSION (LZMA2)
// ============================================================================

std::vector<uint8_t> ACXz::compress(const std::vector<uint8_t>& data, int preset) {
    return run_capture({"xz", "-" + std::to_string(preset), "-c"}, data.data(), data.size());
}

std::vector<uint8_t> ACXz::decompress(const std::vector<uint8_t>& data) {
    return run_capture({"xz", "-d", "-c"}, data.data(), data.size());
}

void ACXz::compress_file(const std::string& input, const std::string& output, int preset) {
    run_argv_to_file({"xz", "-" + std::to_string(preset), "-c", input}, output);
}

void ACXz::decompress_file(const std::string& input, const std::string& output) {
    run_argv_to_file({"xz", "-d", "-c", input}, output);
}

// ============================================================================
// ZSTD COMPRESSION
// ============================================================================

std::vector<uint8_t> ACZstd::compress(const std::vector<uint8_t>& data, int level) {
    return run_capture({"zstd", "-" + std::to_string(level), "-c"}, data.data(), data.size());
}

std::vector<uint8_t> ACZstd::decompress(const std::vector<uint8_t>& data) {
    return run_capture({"zstd", "-d", "-c"}, data.data(), data.size());
}

void ACZstd::compress_file(const std::string& input, const std::string& output, int level) {
    run_argv_to_file({"zstd", "-" + std::to_string(level), "-c", input}, output);
}

void ACZstd::decompress_file(const std::string& input, const std::string& output) {
    run_argv_to_file({"zstd", "-d", "-c", input}, output);
}
