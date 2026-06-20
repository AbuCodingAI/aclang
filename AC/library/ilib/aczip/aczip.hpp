#pragma once
#include <string>
#include <vector>
#include <map>
#include <thread>

namespace aczip {

// 4-bit protocol tagging
// File tag: 0x01-0x0F
// Folder tag: 0x1x (1x01, 1x02, etc)
// Example: assets/enemy.png = 1x01.0x01

struct FileEntry {
    std::string path;
    std::string tag;
    std::vector<uint8_t> data;
};

struct Archive {
    std::vector<FileEntry> files;
    std::map<std::string, std::string> folder_tags;
};

// Core compression
class ACZip {
public:
    // Compress file/directory to .aczip
    static std::vector<uint8_t> compress(const std::string& path, bool parallel = true);

    // Decompress .aczip to directory
    static void decompress(const std::vector<uint8_t>& data, const std::string& output_path);

    // Compress optimized for HDD (sequential, less random access)
    static std::vector<uint8_t> compress_hdd(const std::string& path);

    // Compress optimized for SATA (balanced)
    static std::vector<uint8_t> compress_sata(const std::string& path);

    // Get compression ratio
    static double get_ratio(size_t original, size_t compressed);

private:
    static Archive build_archive(const std::string& path);
    static std::string generate_tag(int index);
    static std::string get_folder_tag(const std::string& folder_path);
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

}  // namespace aczip
