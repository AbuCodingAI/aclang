#pragma once
#include <string>
#include <vector>
#include <map>
#include <thread>

namespace aczip {

// ACZip v2 Format
// Magic: "ACZ2" (4 bytes)
// File count: uint32 (4 bytes)
// For each file:
//   Tag: 4-byte ASCII tag (e.g., "0x01", "1x02")
//   Original size: uint32
//   Compressed size: uint32
//   Compressed data: [comp_size] bytes
//
// Each file is individually gzip-compressed for parallel compression + streaming decompression

struct FileEntry {
    std::string path;
    std::string tag;
    std::vector<uint8_t> data;
};

struct Archive {
    std::vector<FileEntry> files;
};

// Core compression - OPTIMIZED
// Strategy: Compress each file individually in parallel, then package
class ACZip {
public:
    // Compress file/directory to .aczip (parallel by default)
    static std::vector<uint8_t> compress(const std::string& path, bool parallel = true);

    // Decompress .aczip to directory
    static void decompress(const std::vector<uint8_t>& data, const std::string& output_path);

    // Compress optimized for HDD (sequential, less random access)
    static std::vector<uint8_t> compress_hdd(const std::string& path);

    // Compress optimized for SATA (balanced, threaded)
    static std::vector<uint8_t> compress_sata(const std::string& path);

    // Get compression ratio
    static double get_ratio(size_t original, size_t compressed);

private:
    static Archive build_archive(const std::string& path);
    static std::string generate_tag(int index);
};

// TAR and GZIP wrappers
class ACTar {
public:
    static std::vector<uint8_t> create(const std::string& path);
    static void extract(const std::vector<uint8_t>& data, const std::string& output_path);
};

class ACGzip {
public:
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int level = 6);
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
};

class ACXz {
public:
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int preset = 6);
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
    static void compress_file(const std::string& input, const std::string& output, int preset = 6);
    static void decompress_file(const std::string& input, const std::string& output);
};

class ACZstd {
public:
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int level = 3);
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
    static void compress_file(const std::string& input, const std::string& output, int level = 3);
    static void decompress_file(const std::string& input, const std::string& output);
};

}  // namespace aczip
